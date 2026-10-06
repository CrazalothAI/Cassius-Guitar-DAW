#requires -Version 7.0
function Get-CassianVersion([string]$ProjectRoot) {
    $package = Get-Content -LiteralPath (Join-Path $ProjectRoot 'ui/package.json') -Raw | ConvertFrom-Json
    $lock = Get-Content -LiteralPath (Join-Path $ProjectRoot 'ui/package-lock.json') -Raw | ConvertFrom-Json -AsHashtable
    $version = [string]$package.version
    if ($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$' -or @($version.Split('.') | Where-Object { [double]$_ -gt 255 }).Count) {
        throw 'Cassian version must be major.minor.patch with components between 0 and 255.'
    }
    if ($lock.version -ne $version -or $lock.packages[''].version -ne $version) { throw 'Cassian package and lockfile versions differ.' }
    return $version
}
function Assert-CassianBinaryVersion([string]$File, [string]$Version) {
    $actual = (Get-Item -LiteralPath $File).VersionInfo.ProductVersion
    if ($actual -ne $Version -and $actual -ne "$Version.0") { throw "Rebuild before packaging: $File reports '$actual', expected '$Version'." }
}
