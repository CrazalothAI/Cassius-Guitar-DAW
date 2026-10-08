param(
    [string]$Standalone = 'build/AmpSuite_artefacts/Release/Standalone/Cassian.exe',
    [string]$Vst3 = 'build/AmpSuite_artefacts/Release/VST3/Cassian.vst3',
    [string]$Compiler = '',
    [string]$AppVersion = '',
    [switch]$SmokeTest,
    [switch]$SkipRootCopy,
    [string]$OutputDirectory = '.',
    [string]$SoundBank = 'assets/sound-bank',
    [switch]$AllowDevelopmentSounds,
    [switch]$Release,
    [string]$CertificateThumbprint = '',
    [string]$TimestampUrl = '',
    [string]$SignTool = '',
    [string]$SigningDlib = '',
    [string]$SigningMetadata = '',
    [string]$PublisherSubject = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
function ProjectPath([string]$path) {
    if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
    return [IO.Path]::GetFullPath((Join-Path $projectRoot $path))
}
$exe = ProjectPath $Standalone
$plugin = ProjectPath $Vst3
$output = ProjectPath $OutputDirectory
. "$PSScriptRoot/ReleaseVersion.ps1"
. "$PSScriptRoot/WindowsSigning.ps1"
$channel = Get-Content -LiteralPath (Join-Path $projectRoot 'ui/src/release.json') -Raw | ConvertFrom-Json
$azureSigning = [bool]($SigningDlib -or $SigningMetadata)
$signing = [bool]($CertificateThumbprint -or $azureSigning)
if ($CertificateThumbprint -and $azureSigning) { throw 'Choose certificate-store signing or Artifact Signing, not both.' }
if ($azureSigning) {
    $SigningDlib = ProjectPath $SigningDlib; $SigningMetadata = ProjectPath $SigningMetadata
    if (!$TimestampUrl) { $TimestampUrl = 'http://timestamp.acs.microsoft.com' }
    Assert-CassianAzureSigningInputs $SigningDlib $SigningMetadata $PublisherSubject $TimestampUrl
    $SignTool = Get-CassianSigningTool $SignTool
} elseif ($CertificateThumbprint) { Assert-CassianSigningInputs $CertificateThumbprint $TimestampUrl; $SignTool = Get-CassianSigningTool $SignTool }
elseif ($TimestampUrl -or $SignTool -or $PublisherSubject) { throw 'Provide a certificate thumbprint or Artifact Signing configuration to enable signing.' }
$sourceVersion = Get-CassianVersion $projectRoot
if (!$AppVersion) { $AppVersion = $sourceVersion }
if ($AppVersion -notmatch '^\d+\.\d+\.\d+(\.\d+)?$') { throw 'Installer version must be a numeric Windows version.' }
if (!$SmokeTest -and $AppVersion -ne $sourceVersion) { throw 'Only isolated smoke installers can override the project version.' }
if (!(Test-Path -LiteralPath $exe -PathType Leaf) -or !(Test-Path -LiteralPath (Join-Path $plugin 'Contents/x86_64-win/Cassian.vst3'))) { throw 'Build both Release formats before packaging.' }
Assert-CassianBinaryVersion $exe $sourceVersion
Assert-CassianBinaryVersion (Join-Path $plugin 'Contents/x86_64-win/Cassian.vst3') $sourceVersion
$commit = & git -C $projectRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine the package source revision.' }
$sourceModified = [bool](& git -C $projectRoot status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) { throw 'Cannot check source changes.' }
$untrackedBuildInputs = @(& git -C $projectRoot ls-files --others --exclude-standard -- CMakeLists.txt CMakePresets.json LICENSE.txt COPYRIGHT.md Source ui scripts installer .github assets docs release README.md)
if ($LASTEXITCODE -ne 0) { throw 'Cannot check uncommitted build inputs.' }
if ($Release -and ($sourceModified -or $untrackedBuildInputs.Count -or $AllowDevelopmentSounds -or $SmokeTest)) { throw 'Release packaging needs committed source and cleared sounds, without smoke/private flags.' }
if (!$Compiler) {
    & "$PSScriptRoot/setup-installer.ps1"
    $Compiler = Join-Path $projectRoot '.deps/innosetup-6.7.3/ISCC.exe'
}
& "$PSScriptRoot/setup-runtime-bootstrapper.ps1"
if (!(Test-Path -LiteralPath $Compiler)) { throw 'Inno Setup compiler not found.' }
$packageRoot = Join-Path $projectRoot 'build/packages'
$staging = Join-Path $packageRoot ('windows-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $staging,$output | Out-Null
try {
    $bank = ProjectPath $SoundBank
    $withSounds = Test-Path -LiteralPath (Join-Path $bank 'manifest.json')
    if ($withSounds) {
        $manifest = Get-Content -LiteralPath (Join-Path $bank 'manifest.json') -Raw | ConvertFrom-Json
        if ($manifest.schema -ne 1 -or !$manifest.assets.Count -or $manifest.assets.Count -gt 1024) { throw 'Invalid packaged sound bank.' }
        if (!$manifest.distributionApproved -and !$AllowDevelopmentSounds) { throw 'Sound bank redistribution records are incomplete. Use private development packaging until the creator terms are recorded.' }
        foreach ($asset in $manifest.assets) {
            $kind = [string]$asset.kind; $id = [string]$asset.id
            if ($kind -notin @('amp','pedal','cab','ambience') -or $id -notmatch "^${kind}:[a-f0-9]{64}$") { throw 'Invalid packaged asset identity.' }
            $hash = $id.Split(':')[1]; $extension = if ($kind -in @('amp','pedal')) { '.nam' } else { '.wav' }
            $file = Join-Path $bank "assets/$kind/$hash$extension"
            if (!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant() -ne $hash) { throw "Missing or changed packaged sound: $($asset.name)" }
            if (!$AllowDevelopmentSounds) {
                if (!$asset.source -or !$asset.permission -or $asset.license -ne "licenses/$hash.txt" -or !(Test-Path -LiteralPath (Join-Path $bank $asset.license))) { throw "Missing packaged sound rights record: $($asset.name)" }
            }
        }
        $recipes = Get-Content -LiteralPath (Join-Path $projectRoot 'ui/src/startingRigs.json') -Raw | ConvertFrom-Json
        foreach ($recipe in $recipes.captureRigs) {
            foreach ($reference in $recipe.assets.PSObject.Properties.Value) {
                if ($reference.id -notin $manifest.assets.id) { throw "Packaged bank is missing recipe sound: $($reference.name)" }
            }
        }
        Copy-Item -LiteralPath $bank -Destination (Join-Path $staging 'Sounds') -Recurse
    }
    Copy-Item -LiteralPath $exe -Destination (Join-Path $staging 'Cassian.exe')
    Copy-Item -LiteralPath $plugin -Destination (Join-Path $staging 'Cassian.vst3') -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'installer/QUICK-START.txt'),(Join-Path $projectRoot 'THIRD_PARTY.md'),(Join-Path $projectRoot 'LICENSE.txt'),(Join-Path $projectRoot 'COPYRIGHT.md'),(Join-Path $projectRoot 'licenses') -Destination $staging -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/USER-GUIDE.md') -Destination (Join-Path $staging 'USER-GUIDE.md')
    if ($signing) {
        Sign-CassianFile (Join-Path $staging 'Cassian.exe') $SignTool $CertificateThumbprint $TimestampUrl $SigningDlib $SigningMetadata $PublisherSubject
        Sign-CassianFile (Join-Path $staging 'Cassian.vst3/Contents/x86_64-win/Cassian.vst3') $SignTool $CertificateThumbprint $TimestampUrl $SigningDlib $SigningMetadata $PublisherSubject
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'build/AmpSuite_artefacts/JuceLibraryCode/icon.ico') -Destination (Join-Path $staging 'Cassian.ico')
    Copy-Item -LiteralPath (Join-Path $projectRoot '.deps/runtime/MicrosoftEdgeWebview2Setup.exe') -Destination $staging
    $notices = @{
        'JUCE.txt' = 'build/_deps/juce-src/LICENSE.md'
        'NAM.txt' = 'build/_deps/nam-src/LICENSE'
        'Inno-Setup.txt' = '.deps/innosetup-6.7.3/license.txt'
        'WebView2.txt' = '.deps/Microsoft.Web.WebView2.1.0.2903.40/LICENSE.txt'
        'WebView2-NOTICE.txt' = '.deps/Microsoft.Web.WebView2.1.0.2903.40/NOTICE.txt'
        'ASIO.txt' = '.deps/asio/ASIOSDK/LICENSE.txt'
        'ASIO-common.txt' = '.deps/asio/ASIOSDK/common/LICENSE.txt'
        'React.txt' = 'ui/node_modules/react/LICENSE'
        'React-DOM.txt' = 'ui/node_modules/react-dom/LICENSE'
        'Scheduler.txt' = 'ui/node_modules/scheduler/LICENSE'
        'Eigen-MPL2.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.MPL2'
        'Eigen-BSD.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.BSD'
        'Eigen-Apache.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.APACHE'
        'Eigen-README.txt' = 'build/_deps/nam-src/Dependencies/eigen/COPYING.README'
        'AudioDSPTools.txt' = 'build/_deps/nam-src/Dependencies/AudioDSPTools/LICENSE'
        'VST3.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt'
        'VST3-public-sdk.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/public.sdk/LICENSE.txt'
        'VST3-pluginterfaces.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/pluginterfaces/LICENSE.txt'
        'VST3-base.txt' = 'build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/base/LICENSE.txt'
    }
    foreach ($notice in $notices.GetEnumerator()) {
        $source = Join-Path $projectRoot $notice.Value
        if (!(Test-Path -LiteralPath $source)) { throw "Missing third-party notice: $source" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $staging "licenses/$($notice.Key)")
    }
    $sourceInfo = "Cassian $sourceVersion ($($channel.channel)) - AGPL-3.0-or-later`r`nCopyright (c) 2026 Crazaloth.`r`nMatching source: Cassian-$sourceVersion-Source.zip, supplied alongside this download at no extra charge.`r`nSource/build instructions: https://github.com/CrazalothAI/Cassius-Guitar-DAW`r`nCheckout at packaging: $commit`r`nTracked source modified: $sourceModified`r`nPrivate third-party captures are excluded from public packages.`r`n"
    [IO.File]::WriteAllText((Join-Path $staging 'SOURCE.txt'), $sourceInfo, [Text.UTF8Encoding]::new($false))
    $options = @('/Qp', "/DPackageDir=$staging", "/DOutputDir=$output", "/DAppVersion=$AppVersion")
    if ($withSounds) { $options += '/DWithSoundBank=1' }
    if ($SmokeTest) { $options += '/DSmokeTest=1' }
    if ($signing) {
        if ($azureSigning) {
            $command = '$q' + $SignTool + '$q sign /fd SHA256 /tr $q' + $TimestampUrl + '$q /td SHA256 /dlib $q' + $SigningDlib + '$q /dmdf $q' + $SigningMetadata + '$q $f'
        } else {
            $command = '$q' + $SignTool + '$q sign /sha1 ' + $CertificateThumbprint + ' /tr $q' + $TimestampUrl + '$q /td SHA256 /fd SHA256 $f'
        }
        $options += '/DSignInstaller=1', ('/SCassianSign=' + $command)
    }
    & $Compiler @options (Join-Path $projectRoot 'installer/Cassian.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Windows installer compilation failed.' }
    if (!$SmokeTest) {
        if ($signing) { $null = Assert-CassianSignature (Join-Path $output 'Cassian-Setup.exe') $CertificateThumbprint $PublisherSubject }
        # Select top-level entries: no build or Standalone folder wrappers in the ZIP.
        $entries = @('Cassian.exe', 'Cassian.vst3', 'QUICK-START.txt', 'SOURCE.txt', 'THIRD_PARTY.md', 'LICENSE.txt', 'COPYRIGHT.md', 'USER-GUIDE.md', 'licenses') | ForEach-Object { Join-Path $staging $_ }
        if ($withSounds) { $entries += Join-Path $staging 'Sounds' }
        Compress-Archive -LiteralPath $entries -DestinationPath (Join-Path $output 'Cassian-Windows.zip') -Force
        & "$PSScriptRoot/package-source.ps1" -OutputDirectory $output
        $versioned = @("Cassian-$sourceVersion-Setup.exe", "Cassian-$sourceVersion-Windows.zip", "Cassian-$sourceVersion-Source.zip")
        Copy-Item -LiteralPath (Join-Path $output 'Cassian-Setup.exe') -Destination (Join-Path $output $versioned[0]) -Force
        Copy-Item -LiteralPath (Join-Path $output 'Cassian-Windows.zip') -Destination (Join-Path $output $versioned[1]) -Force
        $hashes = @($versioned | ForEach-Object { [ordered]@{file=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $output $_) -Algorithm SHA256).Hash.ToLowerInvariant()} })
        $checksumText = (($hashes | ForEach-Object { "$($_.sha256)  $($_.file)" }) -join "`n") + "`n"
        [IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'), $checksumText, [Text.UTF8Encoding]::new($false))
        $build = [ordered]@{schema=1;name='Cassian';version=$sourceVersion;channel=$channel.channel;candidate=$channel.candidate;signingConfigured=$signing;checkout=$commit;trackedSourceModified=$sourceModified;releasePackaging=[bool]$Release;privateSoundBank=([bool]$withSounds -and !$manifest.distributionApproved);soundAssets=if ($withSounds) {$manifest.assets.Count} else {0};files=$hashes}
        [IO.File]::WriteAllText((Join-Path $output 'Cassian-Build.json'), ($build | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        if (!$SkipRootCopy) { Copy-Item -LiteralPath (Join-Path $staging 'Cassian.exe') -Destination (Join-Path $projectRoot 'Cassian.exe') -Force }
        Write-Host "Windows download ready: $(Join-Path $output 'Cassian-Setup.exe')"
    }
} finally {
    $resolvedStaging = [IO.Path]::GetFullPath($staging)
    $resolvedRoot = [IO.Path]::GetFullPath($packageRoot).TrimEnd('\') + '\'
    if (!$resolvedStaging.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Refusing to remove a staging directory outside build/packages.' }
    Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
}
