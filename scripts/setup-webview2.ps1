$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $projectRoot '.deps'
$version = '1.0.2903.40'
$destination = Join-Path $dependencyRoot "Microsoft.Web.WebView2.$version"
if (Test-Path (Join-Path $destination 'build/native/include/WebView2.h')) { Write-Host 'WebView2 SDK already present.'; exit 0 }
New-Item -ItemType Directory -Force $dependencyRoot | Out-Null
$archive = Join-Path $dependencyRoot 'webview2.zip'
Invoke-WebRequest "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$version/microsoft.web.webview2.$version.nupkg" -OutFile $archive
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
Write-Host "WebView2 SDK ready: $destination"
