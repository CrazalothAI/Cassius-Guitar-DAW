#requires -Version 7.0
function Get-CassianSigningTool([string]$Requested) {
    if($Requested){$tool=[IO.Path]::GetFullPath($Requested)}
    else {
        $command=Get-Command signtool.exe -ErrorAction SilentlyContinue
        if($command){$tool=$command.Source}
        else {
            $sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
            $tool=@(Get-ChildItem -LiteralPath $sdk -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object {Join-Path $_.FullName 'x64/signtool.exe'} | Where-Object {Test-Path -LiteralPath $_}) | Select-Object -First 1
        }
    }
    if(!$tool -or !(Test-Path -LiteralPath $tool -PathType Leaf) -or $tool -match '["$%]'){throw 'Install the Windows SDK signing tools or provide a valid -SignTool path.'}
    return $tool
}
function Assert-CassianSigningInputs([string]$Thumbprint,[string]$TimestampUrl) {
    if($Thumbprint -notmatch '^[a-fA-F0-9]{40}$'){throw 'Use the 40-digit thumbprint of your current-user code-signing certificate.'}
    $uri=$null
    if(![Uri]::TryCreate($TimestampUrl,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https' -or $uri.UserInfo -or $TimestampUrl -match '["$%\s]'){throw 'Provide an HTTPS RFC3161 timestamp URL from your signing provider.'}
    $certificate=Get-Item -LiteralPath "Cert:\CurrentUser\My\$Thumbprint" -ErrorAction SilentlyContinue
    if(!$certificate -or !$certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date) -or '1.3.6.1.5.5.7.3.3' -notin $certificate.EnhancedKeyUsageList.ObjectId.Value){throw 'The selected certificate must be valid, have a private key and allow code signing.'}
}
function Assert-CassianSignature([string]$File,[string]$Thumbprint='',[string]$Subject='') {
    $signature=Get-AuthenticodeSignature -LiteralPath $File
    if($signature.Status -ne 'Valid' -or !$signature.TimeStamperCertificate -or ($Thumbprint -and $signature.SignerCertificate.Thumbprint -ne $Thumbprint) -or ($Subject -and $signature.SignerCertificate.Subject -cne $Subject)){throw "A trusted, timestamped publisher signature is required: $File"}
    return $signature.SignerCertificate.Thumbprint
}
function Sign-CassianFile([string]$File,[string]$Tool,[string]$Thumbprint,[string]$TimestampUrl,[string]$Dlib='',[string]$Metadata='',[string]$Subject='') {
    if($Dlib){ & $Tool sign /fd SHA256 /tr $TimestampUrl /td SHA256 /dlib $Dlib /dmdf $Metadata $File }
    else { & $Tool sign /sha1 $Thumbprint /tr $TimestampUrl /td SHA256 /fd SHA256 $File }
    if($LASTEXITCODE -ne 0){throw "Code signing failed: $File"}
    $null=Assert-CassianSignature $File $Thumbprint $Subject
}

function Assert-CassianAzureSigningInputs([string]$Dlib,[string]$Metadata,[string]$Subject,[string]$TimestampUrl) {
    foreach($path in @($Dlib,$Metadata)) {
        if(!$path -or !(Test-Path -LiteralPath $path -PathType Leaf) -or $path -match '["$%\r\n]'){throw 'Provide existing Artifact Signing dlib and metadata paths without command substitution characters.'}
    }
    if([string]::IsNullOrWhiteSpace($Subject)){throw 'Provide -PublisherSubject from your verified Public Trust certificate profile.'}
    if($TimestampUrl -notin @('http://timestamp.acs.microsoft.com','http://timestamp.acs.microsoft.com/','https://timestamp.acs.microsoft.com','https://timestamp.acs.microsoft.com/')){throw 'Use the Microsoft Artifact Signing RFC3161 timestamp endpoint.'}
    $metadataObject=Get-Content -LiteralPath $Metadata -Raw | ConvertFrom-Json
    $endpoint=$null
    if(![Uri]::TryCreate([string]$metadataObject.Endpoint,[UriKind]::Absolute,[ref]$endpoint) -or $endpoint.Scheme -ne 'https' -or $endpoint.Host -notmatch '^[a-z0-9]+\.codesigning\.azure\.net$' -or $endpoint.UserInfo -or $endpoint.Query -or $endpoint.Fragment){throw 'Use the HTTPS regional endpoint from your Artifact Signing account.'}
    foreach($name in @('CodeSigningAccountName','CertificateProfileName')) {
        if([string]::IsNullOrWhiteSpace($metadataObject.$name)){throw "Signing metadata omits $name."}
    }
    foreach($property in $metadataObject.PSObject.Properties.Name) {
        if($property -notin @('Endpoint','CodeSigningAccountName','CertificateProfileName','CorrelationId','ExcludeCredentials')){throw 'Signing metadata must contain profile identifiers only, not credentials.'}
    }
}
