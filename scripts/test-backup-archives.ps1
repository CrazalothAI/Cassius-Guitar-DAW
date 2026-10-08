#requires -Version 7.0
param([string]$BuildDirectory='build', [string]$Ctest='', [string]$Python='', [switch]$Large, [switch]$KeepArchive)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot/ReleaseVersion.ps1"
$build=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $projectRoot $BuildDirectory}))
if(!$Ctest){
    $command=Get-Command ctest -ErrorAction SilentlyContinue
    if($command){$Ctest=$command.Source}
    elseif($IsWindows){
        $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if($vs){$Ctest=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'}
    }
}
if(!$Ctest -or !(Test-Path -LiteralPath $Ctest -PathType Leaf)){throw 'Build the native tests and provide -Ctest or install CMake.'}
$runDirectory=Join-Path $build ('backup-validation/'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory -Force|Out-Null
$small=Join-Path $runDirectory 'zip64-profile.zip'
$largeArchive=Join-Path $runDirectory 'large-personal.zip'
$reportPath=Join-Path $runDirectory 'BACKUP-VALIDATION.json'
$report=[ordered]@{schema=1;status='running';version=Get-CassianVersion $projectRoot;largeRequested=[bool]$Large;largeBytes=0;independentVerifier='not run';error='';limits=@('Synthetic trailing WAV padding tests archive transport, not long recording/RIFF support.','Second-PC and physical power-failure acceptance are not tested.')}
function SaveReport{[IO.File]::WriteAllText($reportPath,($report|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))}
function Run([string]$program,[string[]]$arguments,[bool]$native){
    $start=[Diagnostics.ProcessStartInfo]::new($program); $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
    $pathValue=$start.Environment['Path']; if(!$pathValue){$pathValue=$start.Environment['PATH']}
    $start.Environment.Remove('Path')|Out-Null; $start.Environment.Remove('PATH')|Out-Null; $start.Environment['Path']=$pathValue
    if($native){
        $start.Environment['CASSIAN_TEST_ZIP64_OUTPUT']=$small
        $start.Environment['CASSIAN_TEST_LARGE_BACKUP']=$(if($Large){'1'}else{'0'})
        if($Large){$start.Environment['CASSIAN_TEST_LARGE_BACKUP_OUTPUT']=$largeArchive}else{$start.Environment.Remove('CASSIAN_TEST_LARGE_BACKUP_OUTPUT')|Out-Null}
    }
    $process=[Diagnostics.Process]::Start($start); $out=$process.StandardOutput.ReadToEndAsync(); $err=$process.StandardError.ReadToEndAsync()
    if(!$process.WaitForExit(1200000)){$process.Kill($true); throw 'Archive validation exceeded 20 minutes.'}
    $log=$out.Result+"`n"+$err.Result
    [IO.File]::WriteAllText((Join-Path $runDirectory $(if($native){'native.log'}else{'independent.log'})),$log,[Text.UTF8Encoding]::new($false))
    Write-Host $log; if($process.ExitCode -ne 0){throw "Archive validation exited with code $($process.ExitCode)."}
}
SaveReport
try{
    Write-Host $(if($Large){'Validating above-4-GiB synthetic backup/recovery; at least 17 GiB free scratch space is required.'}else{'Validating classic/ZIP64 compatibility and malformed archives.'})
    Run $Ctest @('--test-dir',$build,'-C','Release','--output-on-failure') $true
    if(!(Test-Path -LiteralPath $small -PathType Leaf)){throw 'Native tests did not produce the required ZIP64 compatibility fixture. Rebuild them first.'}
    if($Large){$report.largeBytes=(Get-Item -LiteralPath $largeArchive).Length; if($report.largeBytes -le 4GB){throw 'Large fixture did not exceed 4 GiB.'}}
    if(!$Python){$command=Get-Command python -ErrorAction SilentlyContinue; if($command){$Python=$command.Source}}
    if($Python){Run $Python @('-m','zipfile','-t',$(if($Large){$largeArchive}else{$small})) $false; $report.independentVerifier='Python zipfile CRC verification passed'}
    $report.status='passed'
}catch{$report.status='failed'; $report.error=$_.Exception.Message; throw}
finally{
    SaveReport
    if(!$KeepArchive -and (Test-Path -LiteralPath $largeArchive -PathType Leaf)){Remove-Item -LiteralPath $largeArchive}
    Write-Host "Backup evidence: $reportPath"
}
