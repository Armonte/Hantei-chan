param(
    [string]$Executable = "$PSScriptRoot/../build/gonptechan.exe",
    [string]$DataRoot
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('pat-open-routes-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
function WriteTag([byte[]]$Bytes, [int]$At, [string]$Tag) { [Text.Encoding]::ASCII.GetBytes($Tag).CopyTo($Bytes, $At) }
function Write32([byte[]]$Bytes, [int]$At, [int]$Value) { [BitConverter]::GetBytes($Value).CopyTo($Bytes, $At) }
# Small complete native header/end and an empty GOF2 v4 parts block. No game
# files or artwork are required by the registered route regression test.
$native = Join-Path $artifacts 'native.PAT'
$bytes = [byte[]]::new(40)
WriteTag $bytes 0 'PAniDataFile'; WriteTag $bytes 32 '_STR'; WriteTag $bytes 36 '_END'
[IO.File]::WriteAllBytes($native, $bytes)
$accent = Join-Path $artifacts 'café.PAT'
[IO.File]::WriteAllBytes($accent, $bytes)
$gof = Join-Path $artifacts 'gof2.pat'
$imageOffset = 24 + 2000 * 36 + 4
$bytes = [byte[]]::new($imageOffset + 11824)
Write32 $bytes 0 4; Write32 $bytes 4 0x01234567; Write32 $bytes ($imageOffset - 4) $imageOffset
[IO.File]::WriteAllBytes($gof, $bytes)
$bad = Join-Path $artifacts 'truncated.pat'
[IO.File]::WriteAllBytes($bad, [Text.Encoding]::ASCII.GetBytes('PAniDataFile'))
$unknown = Join-Path $artifacts 'unknown.pat'
[IO.File]::WriteAllBytes($unknown, [byte[]](1, 2, 3, 4))

function RunRoute([string]$Name, [string]$Path, [string]$Route, [bool]$Native, [bool]$Invalid = $false, [bool]$Roundtrip = $false) {
    $script = Join-Path $artifacts "$Name.txt"
    $output = Join-Path $artifacts "$Name-saved.pat"
    $lines = @()
    $arguments = @('--headless', '--ui-script', ('"' + $script + '"'))
    switch ($Route) {
        'file' { $lines += "openany $Path" }
        'editor' { $lines += "patopen $Path" }
        'drop' { $lines += "dropfile $Path" }
        'startup' { $arguments += @('--open', ('"' + $Path + '"')) }
        'browser' { $lines += @("browse $(Split-Path -Parent $Path)", 'waitidle', "select $(Split-Path -Leaf $Path)", 'open') }
    }
    $lines += 'patinfo'
    if ($Roundtrip) { $lines += @("savechar-as $output", 'patinfo') }
    # The browser owns asynchronous install scans/previews. Drain them before
    # scripted process exit; shutdown during a scan has a separate upstream
    # static-lifetime race (RescanInstalls/Games), unrelated to PAT routing.
    $lines += @('waitidle', 'quit')
    $lines | Set-Content -LiteralPath $script -Encoding utf8
    $before = (Get-FileHash -LiteralPath $Path).Hash
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Resolve-Path "$PSScriptRoot/..").Path -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw "$Name timed out: $artifacts" }
    if ($process.ExitCode -ne 0) { throw "$Name crashed ($($process.ExitCode)): $artifacts" }
    $log = Get-Content -LiteralPath ($script + '.log') -Raw
    if ($Invalid) {
        if ($log -notmatch 'patinfo: no view') { throw "$Name adopted an invalid file: $log" }
    } else {
        $expected = if ($Native) { 'patinfo: doc=1 .*fb=0 editor=1 views=1' } else { 'patinfo: doc=0 .*fb=1 editor=1 views=2' }
        if ($log -notmatch $expected) { throw "$Name did not open the expected PAT view: $log" }
    }
    if ($Roundtrip) {
        if (!(Test-Path -LiteralPath $output) -or (Get-FileHash -LiteralPath $output).Hash -ne $before) { throw "$Name changed bytes on no-edit Save As" }
    }
    if ((Get-FileHash -LiteralPath $Path).Hash -ne $before) { throw "$Name modified its source" }
    Write-Host "PASS: $Name"
}
foreach ($route in @('file', 'editor', 'drop', 'startup', 'browser')) {
    RunRoute "native-$route" $native $route $true
    RunRoute "accent-$route" $accent $route $true
    RunRoute "gof-$route" $gof $route $false
}
foreach ($route in @('file', 'editor', 'drop', 'startup')) {
    RunRoute "truncated-$route" $bad $route $true $true
    RunRoute "unknown-$route" $unknown $route $true $true
}
if ($DataRoot) {
    $samples = @(Get-ChildItem -LiteralPath $DataRoot -File | Where-Object Extension -ieq '.pat')
    if (!$samples.Count) { throw 'No live PAT files found' }
    foreach ($sample in $samples) { RunRoute ("live-" + $sample.BaseName) $sample.FullName 'file' $true $false $true }
    foreach ($route in @('editor', 'drop', 'startup', 'browser')) { RunRoute "live-arc-$route" (Join-Path $DataRoot 'arc.pat') $route $true }
}
Write-Host "PAT OPEN ROUTES PASS: $artifacts"
