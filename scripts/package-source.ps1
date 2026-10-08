#requires -Version 7.0
param([string]$OutputDirectory='.')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/ReleaseVersion.ps1"
$version=Get-CassianVersion $projectRoot
$output=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $projectRoot $OutputDirectory}))
$revision=(& git -C $projectRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0){throw 'Source packaging requires a Git checkout.'}
if((& git -C $projectRoot status --porcelain --untracked-files=no)){throw 'Commit tracked changes before source packaging.'}
$untracked=@(& git -C $projectRoot ls-files --others --exclude-standard -- CMakeLists.txt CMakePresets.json LICENSE.txt COPYRIGHT.md Source ui scripts installer .github assets docs release README.md)
if($LASTEXITCODE -ne 0 -or $untracked.Count){throw 'Commit all source/build inputs before source packaging.'}
$allowed=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build/source-packages')).TrimEnd('\')+'\'
$staging=Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
$root=Join-Path $staging 'Cassian-source'
New-Item -ItemType Directory -Force -Path $root,$output | Out-Null
function ExportRepository([string]$source,[string]$destination,[string]$expected='') {
    $head=(& git -C $source rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or ($expected -and $head -ne $expected)){throw "Unexpected dependency checkout: $source"}
    if((& git -C $source status --porcelain --untracked-files=no)){throw "Modified dependency source: $source"}
    $zip=Join-Path $staging ([Guid]::NewGuid().ToString('N')+'.zip')
    & git -C $source archive --format=zip "--output=$zip" HEAD
    if($LASTEXITCODE -ne 0){throw "Cannot archive source: $source"}
    Expand-Archive -LiteralPath $zip -DestinationPath $destination -Force
    return $head
}
try {
    $null=ExportRepository $projectRoot $root $revision
    $dependencies=[ordered]@{}
    $dependencySources=@{}
    $cache=Get-Content -LiteralPath (Join-Path $projectRoot 'build/CMakeCache.txt')
    foreach($dependency in @(
        @('JUCE','juce','51a8a6d7aeae7326956d747737ccf1575e61e209'),
        @('nam','nam','1f42f88535884450104b8711d7595019afa0495b'),
        @('signalsmith-stretch','signalsmith-stretch','a670068d9aeb64913331d5cc29337b19a457a7df'),
        @('signalsmith-linear','signalsmith-linear','de55e6a50ffcf6f8f43f649692d94691c7025151')
    )) {
        $destination=Join-Path $root ".deps/$($dependency[0])"
        $key='FETCHCONTENT_SOURCE_DIR_'+$dependency[1].ToUpperInvariant()+':PATH='
        $configured=@($cache|Where-Object{$_.StartsWith($key)})|Select-Object -First 1
        $source=if($configured -and $configured.Substring($key.Length)){$configured.Substring($key.Length)}else{Join-Path $projectRoot "build/_deps/$($dependency[1])-src"}
        $dependencySources[$dependency[0]]=$source
        $dependencies[$dependency[0]]=ExportRepository $source $destination $dependency[2]
    }
    foreach($submodule in @('eigen','AudioDSPTools')) {
        $source=Join-Path $dependencySources['nam'] "Dependencies/$submodule"
        $expected=((& git -C $dependencySources['nam'] ls-tree HEAD "Dependencies/$submodule") -split '\s+')[2]
        $dependencies[$submodule]=ExportRepository $source (Join-Path $root ".deps/nam/Dependencies/$submodule") $expected
    }
    # The verified ASIO SDK uses its GPLv3 option. Include the SDK source and
    # its notices, not local installers, build products or driver binaries.
    $asio=Join-Path $projectRoot '.deps/asio/ASIOSDK'
    if(!(Test-Path (Join-Path $asio 'common/iasiodrv.h'))){throw 'ASIO source is missing.'}
    $sdkArchive=Join-Path $projectRoot '.deps/asiosdk.zip'
    if(!(Test-Path -LiteralPath $sdkArchive) -or (Get-FileHash -LiteralPath $sdkArchive -Algorithm SHA256).Hash -ne 'D5EBF0C20DD2C5F43771FD0C1418F4B361BF52434EE670097CFA6B3A335E2ECA'){throw 'Verified original ASIO SDK archive is needed for source packaging.'}
    $originalSdk=[IO.Compression.ZipFile]::OpenRead($sdkArchive)
    try {
    foreach($file in Get-ChildItem -LiteralPath $asio -File -Recurse) {
        if($file.Extension -in @('.exe','.dll','.lib','.pdb','.obj','.zip') -or $file.FullName.Contains('Steinberg ASIO Logo Artwork')){continue}
        $relative=[IO.Path]::GetRelativePath($asio,$file.FullName).Replace('\','/')
        $entry=$originalSdk.GetEntry('ASIOSDK/'+$relative)
        if(!$entry){throw 'Unexpected local ASIO SDK file.'}
        $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
        try{$hash=[Convert]::ToHexString($digest.ComputeHash($stream))}finally{$stream.Dispose();$digest.Dispose()}
        if((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne $hash){throw 'Modified ASIO source cannot be packaged as the verified SDK.'}
        $destination=Join-Path $root ('.deps/asio/ASIOSDK/'+[IO.Path]::GetRelativePath($asio,$file.FullName))
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
    } finally { $originalSdk.Dispose() }
    foreach($name in @('react','react-dom','scheduler')) { Copy-Item -LiteralPath (Join-Path $projectRoot "ui/node_modules/$name") -Destination (Join-Path $root ".deps/frontend-$name") -Recurse }
    $manifest=[ordered]@{schema=1;version=$version;checkout=$revision;license='AGPL-3.0-or-later';dependencies=$dependencies;asio='2.3.4, GPLv3 option';frontendLockSha256=(Get-FileHash (Join-Path $root 'ui/package-lock.json') -Algorithm SHA256).Hash.ToLowerInvariant()}
    [IO.File]::WriteAllText((Join-Path $root 'SOURCE-MANIFEST.json'),($manifest|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $root 'BUILD-SOURCE.txt'),"Cassian $version source, checkout $revision.`r`nRead README.md and docs/BUILD-FROM-SOURCE.md.`r`nPinned runtime C++ dependencies and NAM submodules are included in .deps.`r`nPowerShell 7, Node.js and Visual Studio 2022 C++ Build Tools are prerequisites.`r`nThe Microsoft WebView2 SDK/bootstrapper and npm build tools need internet during setup.`r`nRun ./scripts/build-windows.ps1 to use the bundled dependency source.`r`nNo owner captures, recordings, build outputs, credentials or Git metadata are included.`r`n",[Text.UTF8Encoding]::new($false))
    Compress-Archive -LiteralPath $root -DestinationPath (Join-Path $output "Cassian-$version-Source.zip") -Force
    Write-Host "Matching source ready: $(Join-Path $output "Cassian-$version-Source.zip")"
} finally {
    $resolved=[IO.Path]::GetFullPath($staging)
    if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe source-package cleanup path.'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
