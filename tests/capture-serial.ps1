param([string]$Port='COM12',[int]$Seconds=15,[string]$Output='docs/evidence/uart-serial.log',[switch]$Echo)
$ErrorActionPreference='Stop'
$serialLink=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serialLink.ReadTimeout=200
$serialLink.WriteTimeout=1000
$capture=[System.Text.StringBuilder]::new()
try {
    $serialLink.Open()
    $timer=[System.Diagnostics.Stopwatch]::StartNew()
    $sentEcho=$false
    while($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk=$serialLink.ReadExisting()
        if($chunk) { [void]$capture.Append($chunk) }
        if($Echo -and !$sentEcho -and $timer.ElapsedMilliseconds -ge 1500) {
            $serialLink.Write("UART7_ECHO_102021326858`r`n")
            $sentEcho=$true
        }
        Start-Sleep -Milliseconds 20
    }
} finally {
    if($serialLink.IsOpen) { $serialLink.Close() }
    $serialLink.Dispose()
    [System.IO.File]::WriteAllText((Join-Path (Get-Location) $Output),$capture.ToString())
}
$capture.ToString()
