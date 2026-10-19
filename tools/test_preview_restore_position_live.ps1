param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path
$exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('preview-restore-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$source=Join-Path $root 'iori_0.HA6'
$hash=(Get-FileHash $source).Hash
function Run([string]$inputFile,[string]$scriptName) {
    $p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$inputFile+'" --ui-script "'+$scriptName+'"') -WindowStyle Hidden -PassThru
    if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Diagnostic timeout' }
    if($p.ExitCode -ne 0) { throw 'Diagnostic failed' }
    return Get-Content ($scriptName+'.log') -Raw
}
$project=Join-Path $out 'restore.hproj'
$commands=Join-Path $out 'save.txt'
@('waitidle','pattern 333','viewframe 10','wait 5','positionpixeltest','positiongesturetest layer','pattern 333','viewframe 0',
 'opponentpreview 0',"opponentselect $root/kyo_lo_0.txt",'wait 8','opponentreleasefixture','opponentrelease 1',
 'opponentonion 1','opponentonionconfig','opponenttick 13','wait 8','opponentinspect',"opponentprojectsave $project",'quit') | Set-Content $commands
$log=Run "$root/iori_0.txt" $commands
if($log -notmatch 'positiongesturetest: footprint=1 move=1 rotation=1') { throw "Gesture contract failed: $log" }
if($log -notmatch 'positionpixeltest: covered=([1-9][0-9]*) empty=([1-9][0-9]*) mismatch=0') { throw "Pixel coverage contract failed: $log" }
if($log -notmatch 'elapsed=12 y=-38') { throw "Release fixture failed: $log" }
$commands=Join-Path $out 'reload.txt'
@('waitidle','opponentreleasefixture','wait 8','opponentcontrolsinspect','opponentinspect',"transformtrace $out/restored.jsonl",'wait 4','quit') | Set-Content $commands
$log=Run $project $commands
if($log -notmatch 'tick=13 onion_spacing=7' -or $log -notmatch 'elapsed=12 y=-38') { throw "Immediate restore failed: $log" }
$j=Get-Content $project -Raw | ConvertFrom-Json
if($j.views[0].onion.enabled -or [Math]::Abs($j.views[0].opponent_onion.alpha-.27) -gt .0001 -or $j.views[0].grab_preview.target -notmatch 'kyo_lo_0.txt') { throw 'Independent onion or target persistence failed' }
if((Get-FileHash $source).Hash -ne $hash) { throw 'Source HA6 changed' }
$commands=Join-Path $out 'spawns.txt'
@('waitidle','pattern 149','viewframe 0','positionflipfixture 0','wait 8','positiongesturetest effect',
 'positionflipfixture 1','wait 8','positiongesturetest effect',
 'positionpersistentfixture normal','wait 8','positiongesturetest persistent',
 'positionpersistentfixture flip','wait 8','positiongesturetest persistent','quit') | Set-Content $commands
$log=Run "$root/kyo_lo_0.txt" $commands
if(([regex]::Matches($log,'positiongesturetest: footprint=1 move=1 rotation=1')).Count -ne 4) { throw "Facing/current/persistent spawn gesture fixture failed: $log" }
Write-Host "RESTORE / POSITION LIVE PASS: $out; source unchanged $hash"
