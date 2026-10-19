param([string]$Executable = "$PSScriptRoot/../build/gonptechan.exe", [int]$Runs = 24)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('browser-shutdown-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
# Shutdown records are async debug-log lines in hanteichan.log (working directory).
$env:HC_LOG_DEBUG = '1'
$busyExits = 0
for ($i = 0; $i -lt $Runs; $i++) {
    $root = Join-Path $artifacts "run-$i"
    New-Item -ItemType Directory -Path $root | Out-Null
    $script = Join-Path $root 'exit.txt'
    # Deliberately omit waitidle: production close must own the barrier.
    $lines = if ($i % 2) { @("browse $root", 'quit') } else { @('quit') }
    $lines | Set-Content -LiteralPath $script -Encoding utf8
    $process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--ui-script', ('"' + $script + '"')) -WorkingDirectory $root -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw "Shutdown timed out: $root" }
    if ($process.ExitCode -ne 0) { throw "Shutdown crashed ($($process.ExitCode)): $root" }
    $trace = Get-Content -LiteralPath (Join-Path $root 'hanteichan.log') -Raw
    if ($trace -notmatch 'browser worker joined threads=3 active=0 queued=0 discarded=\d+') { throw "Missing joined barrier: $root`n$trace" }
    if ($trace -match 'browser worker stopping threads=3 active=([1-9]\d*)' -or $trace -match 'browser worker stopping .*queued=([1-9]\d*)') { $busyExits++ }
    Write-Host "PASS: exit-$i"
}
Write-Host "BROWSER SHUTDOWN PASS: $Runs runs, $busyExits busy exits; $artifacts"
