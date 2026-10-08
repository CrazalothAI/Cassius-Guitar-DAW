#requires -Version 7.0
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/ReleaseArtifacts.ps1"
$projectRoot = Split-Path -Parent $PSScriptRoot
$allowed = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build/artifact-tests')).TrimEnd('\') + '\'
$root = Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
function Assert([bool]$ok, [string]$reason) { if (!$ok) { throw $reason } }
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $removed = @('Cassian-0.2.0-Setup.exe','Cassian-1.2.1-Setup.exe','Cassian-1.2.1-Windows.zip','Cassian-1.2.1-Source.zip','Cassian-Windows.zip','Cassian-Build.json','SHA256SUMS.txt','RELEASE-READINESS.json')
    $kept = @('Cassian.exe','Cassian-Setup.exe','other-app.exe','Cassian-custom-Setup.exe','README.md','recording.wav','Cassian-1.2.1-Setup.exe.notes')
    foreach ($name in $removed + $kept) { [IO.File]::WriteAllText((Join-Path $root $name), $name) }
    $nested = Join-Path $root 'takes'; New-Item -ItemType Directory -Path $nested | Out-Null
    [IO.File]::WriteAllText((Join-Path $nested 'Cassian-0.2.0-Setup.exe'), 'keep nested data')
    Assert ((Remove-CassianRootArtifacts $root) -eq $removed.Count) 'Cleanup must remove only the exact generated top-level files.'
    foreach ($name in $removed) { Assert (!(Test-Path -LiteralPath (Join-Path $root $name))) 'Obsolete generated file survived cleanup.' }
    foreach ($name in $kept) { Assert (([IO.File]::ReadAllText((Join-Path $root $name))) -eq $name) 'Cleanup changed an unrelated file or the current installer/app.' }
    Assert (Test-Path -LiteralPath (Join-Path $nested 'Cassian-0.2.0-Setup.exe')) 'Cleanup must not recurse.'
    Assert ((Remove-CassianRootArtifacts $root) -eq 0) 'Cleanup must be repeatable.'
    Assert ((Get-CassianPackageDirectory $root '' '1.2.1') -eq [IO.Path]::GetFullPath((Join-Path $root 'build/releases/1.2.1'))) 'Default output must be inside the versioned build directory.'
    Assert ((Get-CassianPackageDirectory $root 'custom' '1.2.1') -eq [IO.Path]::GetFullPath((Join-Path $root 'custom'))) 'Custom output must remain supported.'
    Write-Host 'Release layout tests passed: exact-file cleanup, preserved current/unrelated files, no recursion, repeatability and output paths.'
} finally {
    $resolved = [IO.Path]::GetFullPath($root)
    if (!$resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe artifact-test cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
