#requires -Version 7.0
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/NativeTestReport.ps1"
$projectRoot = Split-Path $PSScriptRoot -Parent
$allowed = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build/native-report-tests')).TrimEnd('\') + '\'
$fixture = Join-Path $allowed ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$report = Join-Path $fixture 'report.xml'
function Assert([bool]$ok, [string]$message) { if (!$ok) { throw $message } }
try {
    [IO.File]::WriteAllText($report,'<testsuite><testcase name="pass"><system-out>passed</system-out></testcase><testcase name="skipped"><skipped/></testcase></testsuite>')
    Assert (@(Get-CassianNativeFailureAnnotations $report).Count -eq 0) 'Passing/skipped tests must not create failure annotations.'
    [IO.File]::WriteAllText($report,"<testsuite><testcase name='processor'><failure message=''/><system-out>Earlier success`nReimport must clear approval`n</system-out></testcase></testsuite>")
    $messages = @(Get-CassianNativeFailureAnnotations $report)
    Assert ($messages.Count -eq 1 -and $messages[0].EndsWith('Last output: Reimport must clear approval')) 'Failed test must identify its final diagnostic.'
    [IO.File]::WriteAllText($report,"<testsuite><testcase name='bad&#10;::warning::fake'><error message='100% timeout&#13;&#10;::error::fake'/></testcase></testsuite>")
    $message = @(Get-CassianNativeFailureAnnotations $report)[0]
    Assert ($message.Contains('100%25 timeout%0D%0A') -and !$message.Contains("`n") -and $message.Contains('bad%0A')) 'Annotation data must escape command separators.'
    [IO.File]::WriteAllText($report,'<!DOCTYPE testsuite [<!ENTITY file SYSTEM "file:///invalid">]><testsuite>&file;</testsuite>')
    $rejected = $false; try { Get-CassianNativeFailureAnnotations $report | Out-Null } catch { $rejected = $true }
    Assert $rejected 'DTD report must reject without resolving external content.'
    [IO.File]::WriteAllText($report,'broken XML')
    $rejected = $false; try { Get-CassianNativeFailureAnnotations $report | Out-Null } catch { $rejected = $true }
    Assert $rejected 'Malformed test report must reject.'
    Write-Host 'Native report checks passed: passing/skipped cases, failure diagnostics, escaped annotations and invalid XML.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if (!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe native-report fixture cleanup.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
