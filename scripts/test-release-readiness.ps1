#requires -Version 7.0
param([string]$OutputDirectory='.',[switch]$RequireReady)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/ReleaseVersion.ps1"
. "$PSScriptRoot/WindowsSigning.ps1"
$version=Get-CassianVersion $projectRoot
if((& git -C $projectRoot status --porcelain --untracked-files=no)){throw 'Commit release evidence before checking readiness.'}
$output=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $projectRoot $OutputDirectory}))
$reasons=[Collections.Generic.List[string]]::new()
$revision=(& git -C $projectRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0){throw 'Readiness requires a source checkout.'}
$acceptance=Get-Content -LiteralPath (Join-Path $projectRoot 'release/acceptance.json') -Raw | ConvertFrom-Json
if($acceptance.schema -ne 1 -or $acceptance.version -ne $version){$reasons.Add('Acceptance records must match the project version.')}
foreach($key in @('freshWindowsInstall','realUpgrade','dawRecallAutomation','physicalMidi','clipchampSync','brandingRights','sellerTermsAndSupport','dependencyDistributionReview')) {
    $record=$acceptance.checks.$key
    if($record.passed -isnot [bool] -or !$record.passed -or [string]::IsNullOrWhiteSpace($record.evidence)){$reasons.Add("Missing release evidence: $key")}
}
$channel=Get-Content -LiteralPath (Join-Path $projectRoot 'ui/src/release.json') -Raw | ConvertFrom-Json
if($channel.channel -ne 'stable'){$reasons.Add('Release channel is a candidate/preview, not approved stable.')}
try {
    $metadata=Get-Content -LiteralPath (Join-Path $output 'Cassian-Build.json') -Raw | ConvertFrom-Json
    if($metadata.channel -ne $channel.channel -or $metadata.version -ne $version -or $metadata.checkout -ne $revision -or $metadata.trackedSourceModified -or !$metadata.releasePackaging -or $metadata.privateSoundBank){$reasons.Add('Package must match committed public Release source.')}
    foreach($file in $metadata.files) {
        if($file.file -ne [IO.Path]::GetFileName($file.file) -or (Get-FileHash -LiteralPath (Join-Path $output $file.file) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256){throw 'Package artifact hash mismatch.'}
    }
    if("Cassian-$version-Source.zip" -notin $metadata.files.file){$reasons.Add('Matching source archive must be included in package metadata.')}
    $archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $output "Cassian-$version-Source.zip"))
    try {
        $entry=$archive.GetEntry('Cassian-source/SOURCE-MANIFEST.json')
        if(!$entry){throw 'Missing matching-source manifest.'}
        $reader=[IO.StreamReader]::new($entry.Open())
        try{$source=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}
        if($source.checkout -ne $revision -or $source.version -ne $version -or $source.license -ne 'AGPL-3.0-or-later'){throw 'Source archive does not match the tested release.'}
    }finally{$archive.Dispose()}
    $setup=Join-Path $output "Cassian-$version-Setup.exe"
    $null=Assert-CassianSignature $setup
    $publisher=(Get-AuthenticodeSignature -LiteralPath $setup).SignerCertificate.Subject
    $portable=[IO.Compression.ZipFile]::OpenRead((Join-Path $output "Cassian-$version-Windows.zip"))
    $allowed=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build/signature-checks')).TrimEnd('\')+'\'
    $staging=Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    try {
        foreach($name in @('Cassian.exe','Cassian.vst3/Contents/x86_64-win/Cassian.vst3')) {
            $entry=$portable.GetEntry($name);if(!$entry){throw 'Portable app/plugin is missing.'}
            $file=Join-Path $staging ([IO.Path]::GetFileName($name));[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$file)
            $null=Assert-CassianSignature $file -Subject $publisher
        }
    }finally {
        $portable.Dispose();$resolved=[IO.Path]::GetFullPath($staging)
        if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe signature-check cleanup path.'}
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}catch{$reasons.Add($_.Exception.Message)}
$report=[ordered]@{schema=1;version=$version;checkout=$revision;ready=$reasons.Count -eq 0;scope=$acceptance.scope;ownerSoundReport=$acceptance.ownerSoundReport;remaining=$reasons.ToArray()}
New-Item -ItemType Directory -Path $output -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $output 'RELEASE-READINESS.json'),($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if($reasons.Count){Write-Host "Release not ready (channel: $($channel.channel)); $($reasons.Count) items remain:";foreach($reason in $reasons){Write-Host "- $reason"};if($RequireReady){throw 'Stable paid-release readiness checks failed. See RELEASE-READINESS.json.'}}
else{Write-Host 'Stable paid-release checks passed.'}
