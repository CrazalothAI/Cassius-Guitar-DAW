#requires -Version 7.0
function Get-CassianNativeFailureAnnotations([string]$Report) {
    $file = Get-Item -LiteralPath $Report -ErrorAction Stop
    if ($file.Length -gt 8MB) { throw 'Native test report exceeds 8 MiB.' }
    $settings = [Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
    $settings.XmlResolver = $null
    $reader = [Xml.XmlReader]::Create($file.FullName, $settings)
    $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null
    try { $document.Load($reader) } finally { $reader.Dispose() }
    $failed = @($document.SelectNodes('//testcase[failure or error]')) | Select-Object -First 12
    foreach ($test in $failed) {
        $failure = $test.SelectSingleNode('failure | error')
        $reason = $failure.GetAttribute('message')
        $output = $test.SelectSingleNode('system-out')
        $lastLine = if ($output) { @($output.InnerText -split '[\r\n]+' | Where-Object { $_.Trim() }) | Select-Object -Last 1 } else { '' }
        $message = "$($test.GetAttribute('name')) failed."
        if ($reason) { $message += " $reason" }
        if ($lastLine) { $message += " Last output: $lastLine" }
        if ($message.Length -gt 1200) { $message = $message.Substring(0,1200) }
        # Report text is annotation data, never another workflow command.
        $message = $message.Replace('%','%25').Replace("`r",'%0D').Replace("`n",'%0A')
        "::error title=Native test failure::$message"
    }
}
