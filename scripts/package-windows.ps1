param(
    [string]$Standalone = 'build/AmpSuite_artefacts/Release/Standalone/Cassian.exe',
    [string]$Vst3 = 'build/AmpSuite_artefacts/Release/VST3/Cassian.vst3',
    [string]$Compiler = '',
    [string]$AppVersion = '',
    [switch]$SmokeTest,
    [switch]$SkipRootCopy,
    [string]$OutputDirectory = '.'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
function ProjectPath([string]$path) {
    if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
    return [IO.Path]::GetFullPath((Join-Path $projectRoot $path))
}
$exe = ProjectPath $Standalone
$plugin = ProjectPath $Vst3
$output = ProjectPath $OutputDirectory
if (!$AppVersion) {
    $versionMatch = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'CMakeLists.txt')), 'project\(Cassian VERSION ([0-9.]+)')
    if (!$versionMatch.Success) { throw 'Cannot determine the Cassian project version.' }
    $AppVersion = $versionMatch.Groups[1].Value
}
if ($AppVersion -notmatch '^\d+\.\d+\.\d+(\.\d+)?$') { throw 'Installer version must be a numeric Windows version.' }
if (!(Test-Path -LiteralPath $exe -PathType Leaf) -or !(Test-Path -LiteralPath (Join-Path $plugin 'Contents/x86_64-win/Cassian.vst3'))) { throw 'Build both Release formats before packaging.' }
if (!$Compiler) {
    & "$PSScriptRoot/setup-installer.ps1"
    $Compiler = Join-Path $projectRoot '.deps/innosetup-6.7.3/ISCC.exe'
}
& "$PSScriptRoot/setup-runtime-bootstrapper.ps1"
if (!(Test-Path -LiteralPath $Compiler)) { throw 'Inno Setup compiler not found.' }
$packageRoot = Join-Path $projectRoot 'build/packages'
$staging = Join-Path $packageRoot ('windows-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $staging,$output | Out-Null
try {
    Copy-Item -LiteralPath $exe -Destination (Join-Path $staging 'Cassian.exe')
    Copy-Item -LiteralPath $plugin -Destination (Join-Path $staging 'Cassian.vst3') -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'installer/QUICK-START.txt'),(Join-Path $projectRoot 'THIRD_PARTY.md'),(Join-Path $projectRoot 'licenses') -Destination $staging -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'build/AmpSuite_artefacts/JuceLibraryCode/icon.ico') -Destination (Join-Path $staging 'Cassian.ico')
    Copy-Item -LiteralPath (Join-Path $projectRoot '.deps/runtime/MicrosoftEdgeWebview2Setup.exe') -Destination $staging
    $notices = @{
        'JUCE.txt' = 'build/_deps/juce-src/LICENSE.md'
        'NAM.txt' = 'build/_deps/nam-src/LICENSE'
        'Inno-Setup.txt' = '.deps/innosetup-6.7.3/license.txt'
        'WebView2.txt' = '.deps/Microsoft.Web.WebView2.1.0.2903.40/LICENSE.txt'
        'WebView2-NOTICE.txt' = '.deps/Microsoft.Web.WebView2.1.0.2903.40/NOTICE.txt'
        'ASIO.txt' = '.deps/asio/ASIOSDK/LICENSE.txt'
        'ASIO-common.txt' = '.deps/asio/ASIOSDK/common/LICENSE.txt'
        'React.txt' = 'ui/node_modules/react/LICENSE'
        'React-DOM.txt' = 'ui/node_modules/react-dom/LICENSE'
        'Scheduler.txt' = 'ui/node_modules/scheduler/LICENSE'
        'Eigen-MPL2.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.MPL2'
        'Eigen-BSD.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.BSD'
        'Eigen-Apache.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.APACHE'
        'Eigen-README.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.README'
        'AudioDSPTools.txt' = 'build/_deps/nam-src/Dependencies/AudioDSPTools/LICENSE'
        'VST3.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt'
        'VST3-public-sdk.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/public.sdk/LICENSE.txt'
        'VST3-pluginterfaces.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/pluginterfaces/LICENSE.txt'
        'VST3-base.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/base/LICENSE.txt'
    }
    foreach ($notice in $notices.GetEnumerator()) {
        $source = Join-Path $projectRoot $notice.Value
        if (!(Test-Path -LiteralPath $source)) { throw "Missing third-party notice: $source" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $staging "licenses/$($notice.Key)")
    }
    $commit = & git -C $projectRoot rev-parse HEAD
    [IO.File]::WriteAllText((Join-Path $staging 'SOURCE.txt'), "Cassian source and build instructions:`r`nhttps://github.com/CrazalothAI/Cassius`r`nCheckout: $commit`r`nDevelopment packages may include uncommitted local changes.`r`n", [Text.UTF8Encoding]::new($false))
    $options = @('/Qp', "/DPackageDir=$staging", "/DOutputDir=$output", "/DAppVersion=$AppVersion")
    if ($SmokeTest) { $options += '/DSmokeTest=1' }
    & $Compiler @options (Join-Path $projectRoot 'installer/Cassian.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Windows installer compilation failed.' }
    if (!$SmokeTest) {
        # Select top-level entries: no build or Standalone folder wrappers in the ZIP.
        $entries = @('Cassian.exe', 'Cassian.vst3', 'QUICK-START.txt', 'SOURCE.txt', 'THIRD_PARTY.md', 'licenses') | ForEach-Object { Join-Path $staging $_ }
        Compress-Archive -LiteralPath $entries -DestinationPath (Join-Path $output 'Cassian-Windows.zip') -Force
        if (!$SkipRootCopy -and $exe -ne (Join-Path $projectRoot 'Cassian.exe')) { Copy-Item -LiteralPath $exe -Destination (Join-Path $projectRoot 'Cassian.exe') -Force }
        Write-Host "Windows download ready: $(Join-Path $output 'Cassian-Setup.exe')"
    }
} finally {
    $resolvedStaging = [IO.Path]::GetFullPath($staging)
    $resolvedRoot = [IO.Path]::GetFullPath($packageRoot).TrimEnd('\') + '\'
    if (!$resolvedStaging.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Refusing to remove a staging directory outside build/packages.' }
    Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
}
