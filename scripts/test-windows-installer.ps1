param(
    [string]$Standalone = 'build/AmpSuite_artefacts/Release/Standalone/Cassian.exe',
    [string]$SoundBank = 'assets/sound-bank',
    [switch]$AllowDevelopmentSounds
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build/installer-tests')).TrimEnd('\') + '\'
$testRoot = Join-Path $allowedRoot ([Guid]::NewGuid().ToString('N'))
$installDir = Join-Path $testRoot 'app'
$output = Join-Path $testRoot 'output'
$exe = if ([IO.Path]::IsPathRooted($Standalone)) { $Standalone } else { Join-Path $projectRoot $Standalone }
$uninstaller = Join-Path $installDir 'unins000.exe'
. "$PSScriptRoot/ReleaseVersion.ps1"
$version = Get-CassianVersion $projectRoot
$registration = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{239BAD5F-8A9D-4E14-BB81-CE61624D444E}_is1'
function RunInstaller([string]$program, [string[]]$arguments) {
    $process = Start-Process -FilePath $program -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(120000)) { throw "Installer did not finish; inspect $testRoot before retrying." }
    if ($process.ExitCode -ne 0) { throw "Installer exited with code $($process.ExitCode); inspect $testRoot." }
}
function Assert([bool]$condition, [string]$message) {
    if (!$condition) { throw $message }
}
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
try {
    & "$PSScriptRoot/package-windows.ps1" -Standalone $Standalone -SmokeTest -SkipRootCopy -OutputDirectory $output -AppVersion '0.0.1' -SoundBank $SoundBank -AllowDevelopmentSounds:$AllowDevelopmentSounds
    $setup = Join-Path $output 'Cassian-Setup-Smoke.exe'
    $installArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/TASKS=""', '/COMPONENTS="app,vst3"', "/DIR=`"$installDir`"")
    RunInstaller $setup ($installArgs + "/LOG=`"$(Join-Path $testRoot 'install.log')`"")
    $installedExe = Join-Path $installDir 'Cassian.exe'
    Assert (Test-Path -LiteralPath $installedExe) 'Installed app is missing.'
    foreach ($document in @('LICENSE.txt','COPYRIGHT.md','USER-GUIDE.md','SOURCE.txt')) {
        Assert (Test-Path -LiteralPath (Join-Path $installDir $document)) "Installed document is missing: $document"
    }
    Assert ((Get-FileHash -LiteralPath $installedExe).Hash -eq (Get-FileHash -LiteralPath $exe).Hash) 'Installed executable differs from the built app.'
    Assert ((Get-ItemProperty -LiteralPath $registration).DisplayVersion -eq '0.0.1') 'Initial installer registration has the wrong version.'
    Assert (Test-Path -LiteralPath (Join-Path $installDir 'VST3/Cassian.vst3/Contents/x86_64-win/Cassian.vst3')) 'Optional VST3 was not installed.'
    $shortcut = Join-Path $installDir 'Cassian Test.lnk'
    Assert (Test-Path -LiteralPath $shortcut) 'App shortcut is missing.'
    $bankPath = if ([IO.Path]::IsPathRooted($SoundBank)) { $SoundBank } else { Join-Path $projectRoot $SoundBank }
    if (Test-Path -LiteralPath (Join-Path $bankPath 'manifest.json')) {
        $manifest = Get-Content -LiteralPath (Join-Path $bankPath 'manifest.json') -Raw | ConvertFrom-Json
        Assert (Test-Path -LiteralPath (Join-Path $installDir 'Sounds/manifest.json')) 'Installer omitted the sound bank.'
        foreach ($asset in $manifest.assets) {
            $ext = if ($asset.kind -in @('amp','pedal')) { '.nam' } else { '.wav' }
            $relative = "Sounds/assets/$($asset.kind)/$($asset.id.Split(':')[1])$ext"
            Assert ((Get-FileHash -LiteralPath (Join-Path $installDir $relative)).Hash.ToLowerInvariant() -eq $asset.id.Split(':')[1]) 'Installed sound differs from the packaged sound.'
        }
    }
    $shell = New-Object -ComObject WScript.Shell
    Assert ($shell.CreateShortcut($shortcut).TargetPath -eq $installedExe) 'Shortcut does not point at the app.'
    $sentinel = Join-Path $installDir 'user-data.txt'
    [IO.File]::WriteAllText($sentinel, 'preserve user data')
    & "$PSScriptRoot/package-windows.ps1" -Standalone $Standalone -SmokeTest -SkipRootCopy -OutputDirectory $output -SoundBank $SoundBank -AllowDevelopmentSounds:$AllowDevelopmentSounds
    RunInstaller $setup ($installArgs + "/LOG=`"$(Join-Path $testRoot 'upgrade.log')`"")
    Assert ((Get-FileHash -LiteralPath $installedExe).Hash -eq (Get-FileHash -LiteralPath $exe).Hash) 'Upgrade did not retain the correct executable.'
    Assert ((Get-ItemProperty -LiteralPath $registration).DisplayVersion -eq $version) 'Upgrade did not register the current version.'
    Assert ([IO.File]::ReadAllText($sentinel) -eq 'preserve user data') 'Upgrade changed user data.'
    RunInstaller $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$(Join-Path $testRoot 'uninstall.log')`"")
    Assert (!(Test-Path -LiteralPath $installedExe)) 'Uninstall left the app executable.'
    Assert (!(Test-Path -LiteralPath $registration)) 'Uninstall left the isolated registration.'
    Assert (!(Test-Path -LiteralPath $shortcut)) 'Uninstall left the shortcut.'
    Assert (!(Test-Path -LiteralPath (Join-Path $installDir 'VST3'))) 'Uninstall left the optional VST3.'
    Assert ([IO.File]::ReadAllText($sentinel) -eq 'preserve user data') 'Uninstall deleted user data.'
    # Only our unique, verified workspace test directory is removed.
    $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
    if (!$resolvedTestRoot.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe installer test cleanup path.' }
    # Inno's uninstall child can briefly retain the log after its launcher exits.
    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        try { Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force; break } catch {
            if ($attempt -eq 19) { throw }
            Start-Sleep -Milliseconds 250
        }
    }
    Write-Host 'Installer checks passed: app, shortcut, optional VST3, upgrade, uninstall and preserved user data.'
} catch {
    # Keep logs for diagnosis. Remove only the isolated test registration/files
    # through its own uninstaller; the real Cassian installation is untouched.
    if (Test-Path -LiteralPath $uninstaller) {
        try { RunInstaller $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') } catch { Write-Warning $_ }
    }
    throw
}
