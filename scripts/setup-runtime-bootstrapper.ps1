$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$folder = Join-Path $projectRoot '.deps/runtime'
$file = Join-Path $folder 'MicrosoftEdgeWebview2Setup.exe'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
if (!(Test-Path -LiteralPath $file)) {
    Invoke-WebRequest 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $file
}
$signature = Get-AuthenticodeSignature -LiteralPath $file
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') { throw 'WebView2 bootstrapper publisher signature could not be verified.' }
Write-Host 'Verified Microsoft WebView2 bootstrapper. It runs only when the runtime is missing.'
