param(
    [Parameter(Mandatory)][string]$LibraryRoot,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$RightsManifest = '',
    [switch]$Development
)
$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath($LibraryRoot)
$destination = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Use a new output directory; existing banks are never overwritten.' }
[xml]$catalog = Get-Content -LiteralPath (Join-Path $source 'library.xml') -Raw
$rights = if ($RightsManifest) { Get-Content -LiteralPath $RightsManifest -Raw | ConvertFrom-Json } else { $null }
$rows = @($catalog.LIBRARY.ASSET)
if (!$rows.Count -or $rows.Count -gt 1024) { throw 'Sound bank needs 1–1024 assets.' }
$verified = @()
foreach ($asset in $rows) {
    $kind = [string]$asset.kind; $id = [string]$asset.id
    if ($kind -notin @('amp','pedal','cab','ambience') -or $id -notmatch "^${kind}:[a-f0-9]{64}$") { throw "Invalid identity: $id" }
    $file = [string]$asset.path; $hash = $id.Split(':')[1]
    if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) { throw "Missing or changed file: $($asset.name)" }
    $grant = @($rights.assets | Where-Object id -eq $id)
    if (!$Development -and ($grant.Count -ne 1 -or !$grant[0].source -or !$grant[0].permission -or !$grant[0].licenseFile)) { throw "Redistribution record missing for $($asset.name). Supply source, permission and licenseFile per asset." }
    if (!$Development -and !(Test-Path -LiteralPath $grant[0].licenseFile -PathType Leaf)) { throw "License file missing for $($asset.name)" }
    $extension = if ($kind -in @('amp','pedal')) { '.nam' } else { '.wav' }
    $row = [ordered]@{id=$id;kind=$kind}
    foreach ($field in @('name','sourceName','creator','pack','notes','styles','gain','speaker','captureKind')) { if ($asset.HasAttribute($field)) { $row[$field]=[string]$asset.GetAttribute($field) } }
    if (!$Development) { $row.source=[string]$grant[0].source; $row.permission=[string]$grant[0].permission; $row.license="licenses/$hash.txt" }
    $verified += @{row=$row;file=$file;relative="assets/$kind/$hash$extension";grant=if ($grant.Count) {$grant[0]} else {$null}}
}
New-Item -ItemType Directory -Path $destination | Out-Null
foreach ($entry in $verified) {
    $target = Join-Path $destination $entry.relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $entry.file -Destination $target
    if (!$Development) {
        New-Item -ItemType Directory -Force -Path (Join-Path $destination 'licenses') | Out-Null
        Copy-Item -LiteralPath $entry.grant.licenseFile -Destination (Join-Path $destination $entry.row.license)
    }
}
$manifest = [ordered]@{schema=1;name='Cassian shared sounds';distributionApproved=(!$Development);assets=@($verified | ForEach-Object {$_.row})}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'), ($manifest | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
$note = if ($Development) { 'PRIVATE DEVELOPMENT BANK: creator redistribution terms have not yet been verified. Do not publish.' } else { 'Per-asset sources, redistribution records and license files are included in manifest.json and licenses/.' }
[IO.File]::WriteAllText((Join-Path $destination 'NOTICES.txt'), $note, [Text.UTF8Encoding]::new($false))
Write-Host "$($verified.Count) exact sounds prepared in $destination"
