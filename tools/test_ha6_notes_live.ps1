param(
    [Parameter(Mandatory=$true)][string]$DataRoot,
    [string]$Executable = "$PSScriptRoot/../build/gonptechan.exe"
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$data = (Resolve-Path -LiteralPath $DataRoot).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('ha6-notes-ui-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$source = Join-Path $data 'shermie_0.HA6'
$sidecar = $source + '.notes'
$original = (Get-FileHash -LiteralPath $sidecar).Hash
$target = Join-Path $artifacts 'saved.HA6'
$script = Join-Path $artifacts 'smoke.txt'
@('dirty', 'opentrees 1', "savechar-as $target", 'dirty', "capture $artifacts/notes.png", 'quit') | Set-Content -LiteralPath $script -Encoding utf8
# --headless keeps the native window hidden. The UI script exercises the real
# CharacterInstance loader/save path and pane rendering with disposable output.
$process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--open', $source, '--tool', 'notes', '--pattern', '330', '--frame', '19', '--ui-script', $script) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(30000)) { $process.Kill(); throw "Headless notes smoke timed out; artifacts: $artifacts" }
if ($process.ExitCode -ne 0) { throw "Headless notes smoke failed ($($process.ExitCode)); artifacts: $artifacts" }
$log = Get-Content -LiteralPath ($script + '.log') -Raw
if ($log -notmatch 'savechar-as .* -> saved' -or $log -notmatch 'dirty .* -> clean') { throw "Unexpected UI script log: $log" }
$saved = Get-Content -LiteralPath ($target + '.notes.json') -Raw | ConvertFrom-Json
if ($saved.notes.'p330.f19' -ne 'RAW' -or @($saved.notes.PSObject.Properties).Count -ne 5) { throw 'State notes missing in real application Save As' }
if ((Get-FileHash -LiteralPath ($target + '.notes.json.legacy-backup')).Hash -ne $original) { throw 'Migration backup differs' }
if ((Get-FileHash -LiteralPath $sidecar).Hash -ne $original) { throw 'Source sidecar changed' }
# A malformed native sidecar must block Save As before writing the HA6.
$bad = Join-Path $artifacts 'bad.HA6'
Copy-Item -LiteralPath $source -Destination $bad
Set-Content -LiteralPath ($bad + '.notes.json') -Value 'malformed native notes' -Encoding utf8
$blocked = Join-Path $artifacts 'blocked.HA6'
$failureScript = Join-Path $artifacts 'save-failure.txt'
@("savechar-as $blocked", 'quit') | Set-Content -LiteralPath $failureScript -Encoding utf8
$process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--open', $bad, '--ui-script', $failureScript) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Malformed-sidecar smoke timed out' }
if ($process.ExitCode -ne 0 -or (Test-Path -LiteralPath $blocked)) { throw 'Malformed sidecar did not block Save As' }
if ((Get-Content -LiteralPath ($failureScript + '.log') -Raw) -notmatch 'savechar-as .* -> FAILED') { throw 'Missing save-failure diagnostic' }
Write-Host "HEADLESS NOTES SMOKE PASS: $artifacts"
