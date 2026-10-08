#requires -Version 7.0
function Get-CassianPackageDirectory([string]$ProjectRoot, [string]$OutputDirectory, [string]$Version) {
    if (!$OutputDirectory) { $OutputDirectory = "build/releases/$Version" }
    return [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $ProjectRoot $OutputDirectory }))
}

# Only recognized, top-level generated files are eligible. Never recurse into
# a source, installed-app, user recording or previous-release directory.
function Remove-CassianRootArtifacts([string]$ProjectRoot) {
    $root = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $obsolete = @(Get-ChildItem -LiteralPath $root -File | Where-Object {
        $_.Name -match '^Cassian-\d+\.\d+\.\d+-(Setup\.exe|Windows\.zip|Source\.zip)$' -or
        $_.Name -in @('Cassian-Windows.zip','Cassian-Build.json','SHA256SUMS.txt','RELEASE-READINESS.json')
    })
    foreach ($file in $obsolete) {
        $resolved = [IO.Path]::GetFullPath($file.FullName)
        if ([IO.Path]::GetDirectoryName($resolved) -ne $root -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Refusing to remove a release artifact outside the project root or through a link.'
        }
        Remove-Item -LiteralPath $resolved -Force
    }
    return $obsolete.Count
}
