#requires -Version 7.0
$ErrorActionPreference='Stop'
. "$PSScriptRoot/WindowsSigning.ps1"
$allowed=[IO.Path]::GetFullPath((Join-Path (Split-Path $PSScriptRoot -Parent) 'build/signing-tests')).TrimEnd('\')+'\'
$staging=Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $staging | Out-Null
function Assert([bool]$ok,[string]$message){if(!$ok){throw $message}}
function Reject([scriptblock]$action,[string]$message){$rejected=$false;try{& $action}catch{$rejected=$true};Assert $rejected $message}
try {
    $dlib=Join-Path $staging 'Azure.CodeSigning.Dlib.dll';Set-Content -LiteralPath $dlib -Value 'test placeholder; never loaded'
    $metadata=Join-Path $staging 'metadata.json'
    $profile=@{Endpoint='https://eus.codesigning.azure.net';CodeSigningAccountName='test';CertificateProfileName='test'}
    $profile|ConvertTo-Json|Set-Content -LiteralPath $metadata
    Assert-CassianAzureSigningInputs $dlib $metadata 'CN=Test publisher' 'http://timestamp.acs.microsoft.com'
    Reject {Assert-CassianAzureSigningInputs $dlib $metadata '' 'http://timestamp.acs.microsoft.com'} 'Missing expected publisher must fail.'
    Reject {Assert-CassianAzureSigningInputs $dlib $metadata 'CN=Test publisher' 'http://untrusted.example'} 'Unknown timestamp endpoint must fail.'
    $profile.Endpoint='https://eus.codesigning.azure.net.evil.example';$profile|ConvertTo-Json|Set-Content -LiteralPath $metadata
    Reject {Assert-CassianAzureSigningInputs $dlib $metadata 'CN=Test publisher' 'http://timestamp.acs.microsoft.com'} 'Lookalike cloud endpoint must fail.'
    $profile.Endpoint='https://eus.codesigning.azure.net';$profile.ClientSecret='test';$profile|ConvertTo-Json|Set-Content -LiteralPath $metadata
    Reject {Assert-CassianAzureSigningInputs $dlib $metadata 'CN=Test publisher' 'http://timestamp.acs.microsoft.com'} 'Secrets in metadata must fail.'
    Reject {Assert-CassianSigningInputs 'bad-thumbprint' 'https://timestamp.example'} 'Malformed thumbprint must fail.'
    # Stub only signature inspection/SignTool: no account access or certificate creation.
    function Get-AuthenticodeSignature {param($LiteralPath);return $script:signature}
    $script:signature=[pscustomobject]@{Status='Valid';TimeStamperCertificate=[pscustomobject]@{Subject='TSA'};SignerCertificate=[pscustomobject]@{Thumbprint=('a'*40);Subject='CN=Test publisher'}}
    $null=Assert-CassianSignature 'fixture.exe' ('a'*40) 'CN=Test publisher'
    $script:signature.SignerCertificate.Thumbprint='b'*40
    $null=Assert-CassianSignature 'fixture.exe' -Subject 'CN=Test publisher'
    Reject {Assert-CassianSignature 'fixture.exe' ('a'*40)} 'Traditional certificate mismatch must fail.'
    Reject {Assert-CassianSignature 'fixture.exe' -Subject 'CN=Other publisher'} 'Cloud publisher mismatch must fail.'
    $script:signature.Status='NotSigned'
    Reject {Assert-CassianSignature 'fixture.exe'} 'Unsigned binary must fail.'
    $script:signature.Status='Valid';$script:signature.TimeStamperCertificate=$null
    Reject {Assert-CassianSignature 'fixture.exe'} 'Missing timestamp must fail.'
    $script:signature.TimeStamperCertificate=[pscustomobject]@{Subject='TSA'}
    function TestSigningTool {param([Parameter(ValueFromRemainingArguments)]$Arguments);$script:arguments=@($Arguments);$global:LASTEXITCODE=0}
    Sign-CassianFile 'fixture.exe' 'TestSigningTool' '' 'http://timestamp.acs.microsoft.com' $dlib $metadata 'CN=Test publisher'
    Assert (($script:arguments -join '|') -eq "sign|/fd|SHA256|/tr|http://timestamp.acs.microsoft.com|/td|SHA256|/dlib|$dlib|/dmdf|$metadata|fixture.exe") 'Azure signing arguments differ.'
    Sign-CassianFile 'fixture.exe' 'TestSigningTool' ('b'*40) 'https://timestamp.example'
    Assert ($script:arguments -contains '/sha1' -and $script:arguments -notcontains '/dlib') 'Traditional provider arguments differ.'
    function FailedSigningTool {$global:LASTEXITCODE=1}
    Reject {Sign-CassianFile 'fixture.exe' 'FailedSigningTool' '' 'http://timestamp.acs.microsoft.com' $dlib $metadata 'CN=Test publisher'} 'SignTool failure must abort.'
    # GitHub's PowerShell runner forwards LASTEXITCODE; clear the expected
    # failure injected above after proving it was rejected.
    $global:LASTEXITCODE=0
    Write-Host 'Signing logic passed: configuration rejection, certificate rotation, publisher matching, timestamps, command arguments and failure propagation. Provider integration still needs a verified account.'
}finally{
    if(![IO.Path]::GetFullPath($staging).StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe signing fixture cleanup.'}
    Remove-Item -LiteralPath $staging -Recurse -Force
}
