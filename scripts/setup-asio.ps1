$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $projectRoot '.deps'
$destination = Join-Path $dependencyRoot 'asio'
if (Test-Path (Join-Path $destination 'ASIOSDK/common/iasiodrv.h')) { Write-Host 'ASIO SDK already present.'; exit 0 }
New-Item -ItemType Directory -Force $dependencyRoot | Out-Null
$archive = Join-Path $dependencyRoot 'asiosdk.zip'
Invoke-WebRequest 'https://www.steinberg.net/asiosdk' -OutFile $archive
$expected = 'D5EBF0C20DD2C5F43771FD0C1418F4B361BF52434EE670097CFA6B3A335E2ECA'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw 'ASIO download changed from verified SDK 2.3.4. Review the new archive before updating its checksum.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
Write-Host "ASIO SDK ready: $destination"
