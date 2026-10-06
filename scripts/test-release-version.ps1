param([string]$Standalone = 'build/AmpSuite_artefacts/Release/Standalone/Cassian.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/ReleaseVersion.ps1"
$version = Get-CassianVersion $projectRoot
$exe = if ([IO.Path]::IsPathRooted($Standalone)) { $Standalone } else { Join-Path $projectRoot $Standalone }
Assert-CassianBinaryVersion $exe $version
function Reject([scriptblock]$Action, [string]$Expected) {
    try { & $Action; throw 'Invalid release input was accepted.' }
    catch { if (!$_.Exception.Message.StartsWith($Expected)) { throw } }
}
$allowed = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build/version-tests')).TrimEnd('\') + '\'
$fixture = Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'ui') -Force | Out-Null
function Fixture([string]$PackageVersion, [string]$LockVersion) {
    @{version=$PackageVersion} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'ui/package.json')
    @{version=$LockVersion;packages=@{''=@{version=$LockVersion}}} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'ui/package-lock.json')
}
try {
    Fixture '1.2.3' '1.2.3'
    if ((Get-CassianVersion $fixture) -ne '1.2.3') { throw 'Valid version rejected.' }
    Fixture '1.2.3' '1.2.2'
    Reject { Get-CassianVersion $fixture } 'Cassian package and lockfile versions differ.'
    foreach ($invalid in @('1.2.3-beta','01.2.3','1.256.3','1.2.3.4')) {
        Fixture $invalid $invalid
        Reject { Get-CassianVersion $fixture } 'Cassian version must be'
    }
    $otherVersion = if ($version -eq '0.0.1') { '0.0.2' } else { '0.0.1' }
    Reject { Assert-CassianBinaryVersion $exe $otherVersion } 'Rebuild before packaging:'
    Write-Host 'Release version checks passed: binary identity, lockfile mismatch and invalid versions.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if (!$resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe version-test cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
