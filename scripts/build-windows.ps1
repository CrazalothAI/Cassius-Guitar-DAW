$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) { $cmake = $cmakeCommand.Source }
elseif (Test-Path -LiteralPath $vswhere) {
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
if (!$cmake -or !(Test-Path -LiteralPath $cmake)) { throw 'Install Visual Studio 2022 C++ Build Tools with CMake and a Windows SDK.' }
Push-Location $projectRoot
try {
    & "$PSScriptRoot/setup-webview2.ps1"
    & "$PSScriptRoot/setup-asio.ps1"
    $configureArgs = @('--preset', 'windows')
    if (Test-Path '.deps/JUCE/CMakeLists.txt') { $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$projectRoot/.deps/JUCE" }
    if (Test-Path '.deps/nam/NAM/dsp.h') { $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_NAM=$projectRoot/.deps/nam" }
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build --preset release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Processor tests failed.' }
} finally { Pop-Location }
