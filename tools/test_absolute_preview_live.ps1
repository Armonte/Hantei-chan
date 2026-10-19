param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('absolute-preview-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$hash=(Get-FileHash "$root/kyo_lo_0.HA6").Hash
$script=Join-Path $out 'test.txt'
@('waitidle','pattern 149','opponentpreview 0',"opponentselect $root/kyo_lo_0.txt",'wait 8','absolutepreviewfixture',
 'absolutescrub 3','wait 8','opponentinspect',"transformtrace $out/later.jsonl",'wait 4','positiongesturetest persistent','wait 8','positiongesturetest opponent-move','wait 8',
 'absolutescrub 6','wait 8','opponentinspect',"transformtrace $out/release.jsonl",'wait 4',
 'absolutescrub 3','wait 8','opponentinspect',"transformtrace $out/again.jsonl",'wait 4','quit') | Set-Content $script
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/kyo_lo_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Absolute preview diagnostic timeout' }
if($p.ExitCode -ne 0) { throw 'Absolute preview diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
if($log -notmatch 'Release approximation:.*elapsed=1' -or ([regex]::Matches($log,'positiongesturetest: footprint=1 move=1 rotation=1')).Count -ne 2) { throw "Absolute gesture/release failure: $log" }
foreach($file in @('later','again')) {
    $rows=@(& "$PSScriptRoot/read_transform_trace.ps1" -Log "$out/hanteichan.log" -Tag "$out/$file.jsonl")
    foreach($pat in @(149,400,26)) { if(@($rows | Where-Object { $_.pattern -eq $pat -and $_.texture_available }).Count -lt 1) { throw "Missing pattern $pat from absolute $file scene" } }
    if(@($rows | Where-Object { $_.pattern -eq 149 -and $_.state -eq 1 }).Count -ne 1) { throw 'Root followed self-jump during absolute scrub' }
}
if((Get-FileHash "$root/kyo_lo_0.HA6").Hash -ne $hash) { throw 'HA6 input changed' }
Write-Host "ABSOLUTE PREVIEW LIVE PASS: $out; root + persistent spawn + opponent, A-B-A, editing, release; HA6 unchanged"
