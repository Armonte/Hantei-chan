param([string]$DataRoot, [string]$BaselineExecutable, [string]$Executable = "$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('bof-authoring-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$found = @{}
foreach ($name in @('angel_0', 'iori_0', 'kyo_lo_0', 'shermie_0', 'effect')) {
    $source = Join-Path $DataRoot ($name + '.HA6')
    $before = (Get-FileHash -LiteralPath $source).Hash
    $script = Join-Path $artifacts ($name + '.txt')
    $output = Join-Path $artifacts ($name + '.HA6')
    # Compare to the prior editor's serializer, not the original HA6 container:
    # upstream can normalize records during an untouched load/save.
    $baseline = Join-Path $artifacts ($name + '-baseline.HA6')
    $baseScript = Join-Path $artifacts ($name + '-baseline.txt')
    @("savechar-as $baseline", 'quit') | Set-Content -LiteralPath $baseScript -Encoding utf8
    $baseProcess = Start-Process -FilePath $BaselineExecutable -ArgumentList @('--headless', '--open', $source, '--ui-script', ('"' + $baseScript + '"')) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
    if (!$baseProcess.WaitForExit(30000)) { $baseProcess.Kill(); throw 'Baseline save timed out' }
    if ($baseProcess.ExitCode -ne 0) { throw 'Baseline save crashed' }
    $savedHash = (Get-FileHash -LiteralPath $baseline).Hash
    $lines = @('extensionprofile extended', 'framenodes 6')
    foreach ($type in 158..162) { $lines += @("bofinspect $type", "capture $artifacts/$name-$type.png") }
    $lines += @("savechar-as $output", 'extensionprofile vanilla', "savechar-as $artifacts/$name-vanilla.HA6", 'quit')
    $lines | Set-Content -LiteralPath $script -Encoding utf8
    $process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--open', $source, '--ui-script', ('"' + $script + '"')) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw "UI smoke timed out: $name" }
    if ($process.ExitCode -ne 0) { throw "UI smoke crashed: $name" }
    $log = Get-Content -LiteralPath ($script + '.log') -Raw
    foreach ($type in 158..162) { if ($log -match "bofinspect: type=$type pattern=\d+ state=\d+ valid=1") { $found[$type] = $true } }
    if ((Get-FileHash -LiteralPath $source).Hash -ne $before) { throw 'Source HA6 changed' }
    foreach ($file in @($output, "$artifacts/$name-vanilla.HA6")) {
        if ((Get-FileHash -LiteralPath $file).Hash -ne $savedHash) { throw "Profile/UI changed serialized bytes: $file ($artifacts)" }
    }
    Write-Host "PASS: $name UI render and both-profile byte-identical save"
}
foreach ($type in @(158,160,161,162)) { if (!$found.ContainsKey($type)) { throw "Missing valid live coverage: $type ($artifacts)" } }
# IF159 is not present in the active corpus. Exercise its actual panel with a
# disposable in-memory fixture, never a write to a live file.
$fixture = Join-Path $artifacts 'fixture.HA6'
Copy-Item -LiteralPath (Join-Path $DataRoot 'angel_0.HA6') -Destination $fixture
$script = Join-Path $artifacts 'if159.txt'
@('extensionprofile extended', 'framenodes 4', 'bofsample159', 'bofinspect 159', "capture $artifacts/if159.png", 'quit') | Set-Content -LiteralPath $script -Encoding utf8
$process = Start-Process -FilePath $exe -ArgumentList @('--headless', '--open', $fixture, '--ui-script', ('"' + $script + '"')) -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'IF159 fixture timeout' }
if ($process.ExitCode -ne 0 -or (Get-Content -LiteralPath ($script + '.log') -Raw) -notmatch 'bofinspect: type=159 pattern=\d+ state=\d+ valid=1') { throw 'IF159 fixture failed' }
Write-Host 'PASS: disposable IF159 panel render'
Write-Host "BOF AUTHORING LIVE PASS: $artifacts"
