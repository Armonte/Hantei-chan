param([string]$DataRoot, [string]$Executable = "$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('grab-authoring-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$source = Join-Path $DataRoot 'iori_0.HA6'
$commands = Join-Path $DataRoot 'iori_0_c.txt'
$before = (Get-FileHash -LiteralPath $source).Hash
$commandsBefore = (Get-FileHash -LiteralPath $commands).Hash
$script = Join-Path $artifacts 'smoke.txt'
@('extensionprofile extended', 'framenodes 1', 'bofinspect atng', "capture $artifacts/typed-hitgrab.png", "cmdopen $commands", 'cmdtab 5', "capture $artifacts/grab-profiles.png", 'quit') | Set-Content -LiteralPath $script -Encoding utf8
$process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--open', $source, '--ui-script', ('"' + $script + '"')) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Grab authoring UI timeout' }
if ($process.ExitCode -ne 0 -or (Get-Content -LiteralPath ($script + '.log') -Raw) -notmatch 'bofinspect: typed ATNG profile=1') { throw "Missing typed ATNG coverage: $artifacts" }
if ((Get-Content -LiteralPath ($script + '.log') -Raw) -notmatch 'cmdtab: available=1') { throw 'Command window was not opened' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $before -or (Get-FileHash -LiteralPath $commands).Hash -ne $commandsBefore) { throw 'Source file changed' }
Write-Host "GRAB AUTHORING UI PASS: $artifacts"
