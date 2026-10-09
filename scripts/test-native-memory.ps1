#requires -Version 7.0
param([string]$BuildDirectory='build-memory', [ValidateSet('full','sound-pack','starting-rigs','startup')][string]$Check='full')
$ErrorActionPreference='Stop'
if(!$IsWindows){throw 'Native memory diagnostics currently require Windows MSVC.'}
$project=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath((Join-Path $project $BuildDirectory))
$allowed=[IO.Path]::GetFullPath($project).TrimEnd('\')+'\'
if(!$build.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or $build -eq (Join-Path $project 'build')){throw 'Use a separate diagnostic build directory inside the project.'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C++ build tools are required.'}
$cmake=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$tools=Get-ChildItem (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$runtime=Join-Path $tools.FullName 'bin/Hostx64/x64'
if(!(Test-Path (Join-Path $runtime 'clang_rt.asan_dynamic-x86_64.dll'))){throw 'Install the Visual Studio C++ AddressSanitizer component first.'}
$logs=Join-Path $build 'memory-validation'; New-Item -ItemType Directory -Path $logs -Force | Out-Null
function Run([string]$program,[string[]]$arguments,[string]$name,[bool]$native=$false){
    $info=[Diagnostics.ProcessStartInfo]::new($program); $info.WorkingDirectory=$project
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true; $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    foreach($arg in $arguments){$info.ArgumentList.Add($arg)}
    $path=$info.Environment['Path']; if(!$path){$path=$info.Environment['PATH']}
    $info.Environment.Remove('Path')|Out-Null; $info.Environment.Remove('PATH')|Out-Null; $info.Environment['Path']=$runtime+';'+$path
    if($native){
        # Standard Windows ASan checks. The opt-in allocator-family check can
        # misclassify the static-runtime new[] forwarding hook on startup.
        $info.Environment['ASAN_OPTIONS']='halt_on_error=1'
        $info.Environment.Remove('ASAN_SAVE_DUMPS')|Out-Null
        foreach($key in @('CASSIAN_TEST_SOUND_BANK','CASSIAN_SOUND_PACK_MANIFEST','CASSIAN_ACTUAL_SOUND_LIBRARY')){$info.Environment.Remove($key)|Out-Null}
    }
    $process=[Diagnostics.Process]::Start($info); $out=$process.StandardOutput.ReadToEndAsync(); $err=$process.StandardError.ReadToEndAsync()
    if(!$process.WaitForExit(1800000)){$process.Kill($true);throw 'Native memory diagnostics exceeded thirty minutes.'}
    [IO.File]::WriteAllText((Join-Path $logs ($name+'-stdout.txt')),$out.Result)
    [IO.File]::WriteAllText((Join-Path $logs ($name+'-stderr.txt')),$err.Result)
    ($out.Result+"`n"+$err.Result) -split '[\r\n]+' | Select-Object -Last 24 | Write-Host
    if($process.ExitCode -ne 0){
        if($native){
            $summary=@($err.Result -split '[\r\n]+' | Where-Object {$_ -match 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer'} | Select-Object -First 1)
            $message="Native memory check failed with exit code $($process.ExitCode)."
            if($summary.Count){$message+=' '+$summary[0]}
            if($message.Length -gt 1000){$message=$message.Substring(0,1000)}
            $message=$message.Replace('%','%25').Replace("`r",'%0D').Replace("`n",'%0A')
            Write-Host "::error title=Native memory failure::$message"
        }
        throw "$name failed with exit code $($process.ExitCode). See $logs."
    }
}
$arguments=@('-S',$project,'-B',$build,'-G','Visual Studio 17 2022','-A','x64','-DCASSIAN_MEMORY_DIAGNOSTICS=ON',('-DJUCE_WEBVIEW2_PACKAGE_LOCATION='+ (Join-Path $project '.deps')))
# Reuse pinned sources, not release object files; a failing clean CI does not
# need another network fetch before collecting instrumented evidence.
foreach($name in @('juce','nam','signalsmith-linear','signalsmith-stretch')){
    $source=Join-Path $project ('build/_deps/'+$name+'-src')
    if(!(Test-Path $source -PathType Container)){$source=Join-Path $project ('.deps/'+$name)}
    if(Test-Path $source -PathType Container){$arguments+=('-DFETCHCONTENT_SOURCE_DIR_'+$name.ToUpper()+'='+$source)}
}
Run $cmake $arguments 'configure'
Run $cmake @('--build',$build,'--config','Release','--parallel','4','--target','AmpSuiteTests','--','/nodeReuse:false') 'build'
$cache=Get-Content (Join-Path $build 'CMakeCache.txt')
$namEntry=($cache | Select-String '^FETCHCONTENT_SOURCE_DIR_NAM:' | Select-Object -First 1).Line
$nam=if($namEntry){($namEntry -split '=',2)[1]}else{''}
if(!$nam){$nam=Join-Path $build '_deps/nam-src'}
if(!(Test-Path (Join-Path $nam 'example_models/wavenet.nam') -PathType Leaf)){throw 'Cannot locate the pinned NAM fixture in the diagnostic build.'}
$nativeArguments=@((Join-Path $nam 'example_models/wavenet.nam'))
if($Check -ne 'full'){$nativeArguments+=switch($Check){'sound-pack'{'--sound-pack-stress'}'starting-rigs'{'--starting-rigs'}'startup'{'--startup'}}}
Run (Join-Path $build 'AmpSuiteTests_artefacts/Release/AmpSuiteTests.exe') $nativeArguments 'native' $true
Write-Host "Native memory check passed ($Check). Diagnostic executables are not release artifacts."
