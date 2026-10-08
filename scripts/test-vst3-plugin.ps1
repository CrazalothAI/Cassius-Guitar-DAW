#requires -Version 7.0
param(
    [string]$Vst3='build/AmpSuite_artefacts/Release/VST3/Cassian.vst3',
    [string]$OutputDirectory='build/plugin-validation',
    [ValidateRange(5,10)][int]$Strictness=10,
    [ValidateCount(1,8)][int[]]$Seeds=@(10002,10003,10004)
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot/ReleaseVersion.ps1"
function ProjectPath([string]$path){[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $projectRoot $path}))}
$plugin=ProjectPath $Vst3
$binary=Join-Path $plugin 'Contents/x86_64-win/Cassian.vst3'
$output=ProjectPath $OutputDirectory
if($Seeds|Where-Object{$_ -le 0}){throw 'Use positive nonzero reproducible random seeds.'}
if(@($Seeds|Sort-Object -Unique).Count -ne $Seeds.Count){throw 'Use distinct seeds.'}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$runDirectory=Join-Path $output ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$reportPath=Join-Path $output 'PLUGIN-VALIDATION.json'
$report=[ordered]@{schema=1;status='running';version=Get-CassianVersion $projectRoot;pluginSha256='';validator='Tracktion pluginval 1.0.4';strictness=$Strictness;sampleRates=@(44100,48000,96000);blockSizes=@(64,128,256,512,1024);guiTests=$false;steinbergValidator=$false;runs=@();limits=@('GUI/editor interaction is not tested.','Steinberg SDK validator is not supplied.','No actual DAW session, physical MIDI, interface timing or owner capture board is tested.')}
function SaveReport{[IO.File]::WriteAllText($reportPath,($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))}
SaveReport
try {
    Assert-CassianBinaryVersion $binary $report.version
    $report.pluginSha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    & "$PSScriptRoot/setup-pluginval.ps1"
    $program=Join-Path $projectRoot '.deps/pluginval-1.0.4/pluginval.exe'
    foreach($seed in $Seeds){
        $folder=Join-Path $runDirectory "seed-$seed";New-Item -ItemType Directory -Path $folder | Out-Null
        $start=[Diagnostics.ProcessStartInfo]::new()
        $start.FileName=$program;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
        $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
        # ArgumentList escapes paths independently; no shell is invoked.
        $arguments=@('--strictness-level',"$Strictness",'--skip-gui-tests','--random-seed',"$seed",'--timeout-ms','60000','--sample-rates','44100,48000,96000','--block-sizes','64,128,256,512,1024','--output-dir',$folder,'--output-filename','validation.txt','--validate',$plugin)
        foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
        $start.Environment['CASSIAN_VALIDATION_ROOT']=Join-Path $folder 'scratch-library'
        $process=[Diagnostics.Process]::Start($start)
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(!$process.WaitForExit(600000)){
            # Terminate only the validator process started by this run, never Cassian or a DAW.
            $process.Kill($true);$process.WaitForExit();throw "pluginval timed out; inspect $folder"
        }
        [IO.File]::WriteAllText((Join-Path $folder 'stdout.txt'),$stdout.Result)
        [IO.File]::WriteAllText((Join-Path $folder 'stderr.txt'),$stderr.Result)
        $log=Join-Path $folder 'validation.txt'
        $text=if(Test-Path -LiteralPath $log){Get-Content -LiteralPath $log -Raw}else{''}
        $completed=[regex]::Matches($text,'(?m)^Completed tests in pluginval / (.+)\r?$')|ForEach-Object{$_.Groups[1].Value.Trim()}
        $passed=$process.ExitCode -eq 0 -and $text -match '(?m)^SUCCESS\r?$' -and $text -notmatch '(?m)^FAILED|!!! Test'
        foreach($required in @('Open plugin (cold)','Open plugin (warm)','Audio processing','Plugin state','Automation','Restoring default layout')){if($required -notin $completed){$passed=$false}}
        if($Strictness -eq 10){foreach($required in @('Plugin state restoration','Parameter thread safety','Fuzz parameters')){if($required -notin $completed){$passed=$false}}}
        $report.runs+=@{seed=$seed;exitCode=$process.ExitCode;passed=$passed;log=[IO.Path]::GetRelativePath($output,$log);completedTests=@($completed)}
        SaveReport
        if(!$passed){throw "Plugin validation failed for seed $seed; inspect $log"}
        Write-Host "VST3 validation passed: level $Strictness, seed $seed."
    }
    if((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $report.pluginSha256){throw 'VST3 changed while it was being validated.'}
    $report.status='passed';SaveReport
    Write-Host "Independent VST3 checks passed; report: $reportPath"
} catch {
    $report.status='failed';$report.failure=$_.Exception.Message;SaveReport;throw
}
