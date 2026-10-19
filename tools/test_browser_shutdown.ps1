param([string]$Executable = "$PSScriptRoot/../build/gonptechan.exe", [int]$Runs = 24)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('browser-shutdown-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
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
    Write-Host "PASS: exit-$i"
}
# The worker barrier is logged at debug level (hanteichan.log); a clean, timely exit is the contract here.
Write-Host "BROWSER SHUTDOWN PASS: $Runs runs; $artifacts"
