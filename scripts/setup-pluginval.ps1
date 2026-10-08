#requires -Version 7.0
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$version='1.0.4'
$root=Join-Path $projectRoot ".deps/pluginval-$version"
$archive=Join-Path $root 'pluginval_Windows.zip'
$tool=Join-Path $root 'pluginval.exe'
$expected='C08E61CE3B96DB41636F8EC7E76F4C7E2C13EBDAC7FA1B5A1F52B4F32EC715AB'
New-Item -ItemType Directory -Path $root -Force | Out-Null
if(!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/v$version/pluginval_Windows.zip" -OutFile $archive
}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected){throw 'pluginval archive changed; review the upstream release before updating the pin.'}
# Verify the cached executable too, so a modified extraction cannot bypass the archive pin.
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entry=$zip.GetEntry('pluginval.exe');if(!$entry){throw 'Pinned pluginval executable is missing.'}
    $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
    try{$toolHash=[Convert]::ToHexString($digest.ComputeHash($stream))}finally{$stream.Dispose();$digest.Dispose()}
}finally{$zip.Dispose()}
if(!(Test-Path -LiteralPath $tool)){Expand-Archive -LiteralPath $archive -DestinationPath $root -Force}
if((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne $toolHash){throw 'Cached pluginval executable differs from the pinned official archive.'}
Write-Host "Verified pluginval $version (development tool only)."
