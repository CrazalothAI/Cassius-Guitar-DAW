#requires -Version 7.0
param([string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/ReleaseVersion.ps1"
. "$PSScriptRoot/ReleaseArtifacts.ps1"
$version=Get-CassianVersion $projectRoot
$output=Get-CassianPackageDirectory $projectRoot $OutputDirectory $version
function Assert([bool]$ok,[string]$reason){if(!$ok){throw $reason}}
function Hash([string]$file){(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()}
function ReadJson($entry){Assert ($null -ne $entry) 'Archive manifest is missing.';$reader=[IO.StreamReader]::new($entry.Open());try{return $reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}}
function EntryHash($entry){$stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create();try{return [Convert]::ToHexString($digest.ComputeHash($stream)).ToLowerInvariant()}finally{$stream.Dispose();$digest.Dispose()}}
$meta=Get-Content -LiteralPath (Join-Path $output 'Cassian-Build.json') -Raw|ConvertFrom-Json
Assert ($meta.schema -eq 1 -and $meta.version -eq $version -and $meta.checkout -match '^[a-f0-9]{40}$') 'Invalid package identity.'
$expected=@('Cassian-Setup.exe',"Cassian-$version-Windows.zip","Cassian-$version-Source.zip")
Assert ($meta.files.Count -eq 3 -and @($meta.files.file|Sort-Object -Unique).Count -eq 3) 'Installer, portable and source artifacts must all be recorded exactly once.'
$checksums=Get-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt')
foreach($file in $meta.files){
    Assert ($file.file -in $expected -and $file.sha256 -match '^[a-f0-9]{64}$') 'Unsafe or unknown artifact metadata.'
    Assert ((Hash (Join-Path $output $file.file)) -eq $file.sha256) 'Artifact hash mismatch.'
    Assert ($checksums -contains "$($file.sha256)  $($file.file)") 'Checksum text differs from metadata.'
}
Assert (@(Get-ChildItem -LiteralPath $output -File -Filter '*Setup*.exe').Count -eq 1) 'Package must contain exactly one installer.'
if(!$OutputDirectory){Assert ((Hash (Join-Path $projectRoot 'Cassian-Setup.exe')) -eq (Hash (Join-Path $output 'Cassian-Setup.exe'))) 'Root installer differs from the current package.'}
$zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $output "Cassian-$version-Windows.zip"))
try {
    if($meta.releasePackaging) {
        Assert ($meta.pluginValidation.strictness -eq 10 -and $meta.pluginValidation.seeds.Count -ge 3 -and $meta.pluginValidation.compiledPluginSha256 -match '^[a-f0-9]{64}$') 'Release omits independent plugin validation evidence.'
        # Signing appends a publisher certificate; compare the compiled hash
        # directly when packaging is unsigned. Signed artifacts keep their own hashes.
        if(!$meta.signingConfigured){Assert ((EntryHash $zip.GetEntry('Cassian.vst3/Contents/x86_64-win/Cassian.vst3')) -eq $meta.pluginValidation.compiledPluginSha256) 'Packaged plugin differs from the independently validated compiled binary.'}
    }
    foreach($name in @('Cassian.exe','Cassian.vst3/Contents/x86_64-win/Cassian.vst3','LICENSE.txt','COPYRIGHT.md','USER-GUIDE.md','SOURCE.txt','THIRD_PARTY.md','licenses/JUCE.txt','licenses/ASIO.txt')){Assert ($null -ne $zip.GetEntry($name)) "Portable package omits $name"}
    $sounds=@($zip.Entries|Where-Object{$_.FullName -match '^Sounds/assets/.+\.(nam|wav)$'})
    Assert ($sounds.Count -eq $meta.soundAssets) 'Packaged sound count differs from metadata.'
    foreach($entry in $sounds){Assert ((EntryHash $entry) -eq [IO.Path]::GetFileNameWithoutExtension($entry.Name)) 'Packaged sound content differs from its stable ID.'}
    if($sounds.Count){$bank=ReadJson $zip.GetEntry('Sounds/manifest.json');Assert ($bank.assets.Count -eq $sounds.Count -and [bool]$meta.privateSoundBank -eq !$bank.distributionApproved) 'Sound permissions differ from package metadata.'}
    else{Assert (!$meta.privateSoundBank -and $null -eq $zip.GetEntry('Sounds/manifest.json')) 'An empty public package must exclude private bank metadata.'}
}finally{$zip.Dispose()}
$source=[IO.Compression.ZipFile]::OpenRead((Join-Path $output "Cassian-$version-Source.zip"))
try {
    $manifest=ReadJson $source.GetEntry('Cassian-source/SOURCE-MANIFEST.json')
    Assert ($manifest.version -eq $version -and $manifest.checkout -eq $meta.checkout -and $manifest.license -eq 'AGPL-3.0-or-later') 'Source bundle does not match the binary metadata.'
    foreach($name in @('LICENSE.txt','COPYRIGHT.md','CMakeLists.txt','Source/PluginProcessor.cpp','ui/package-lock.json','scripts/build-windows.ps1','.deps/JUCE/LICENSE.md','.deps/nam/NAM/dsp.h','.deps/nam/Dependencies/eigen/Eigen/Core','.deps/nam/Dependencies/AudioDSPTools/LICENSE','.deps/asio/ASIOSDK/common/iasiodrv.h','.deps/signalsmith-stretch/signalsmith-stretch.h','.deps/signalsmith-linear/linear.h','.deps/frontend-react/LICENSE')){Assert ($null -ne $source.GetEntry("Cassian-source/$name")) "Source bundle omits $name"}
    Assert ((EntryHash $source.GetEntry('Cassian-source/ui/package-lock.json')) -eq $manifest.frontendLockSha256) 'Source lockfile hash differs from its manifest.'
    foreach($entry in $source.Entries){Assert ($entry.FullName -notmatch '^Cassian-source/(\.local/|build/|ui/node_modules/|GPT6-MIGRATION-PLAN\.md)' -and $entry.FullName -notmatch '/\.git/') 'Source bundle includes private/local build material.'}
}finally{$source.Dispose()}
Write-Host 'Package integrity passed: one installer, checksums, matching source, runtime dependencies, notices and sound separation.'
