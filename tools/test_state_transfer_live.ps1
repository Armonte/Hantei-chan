param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('state-transfer-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$hash=(Get-FileHash C:/Games/MBAACC_Edit/data/kyo_lo_0.HA6).Hash
foreach($mode in @('main','detached')) {
 $script=Join-Path $out ($mode+'.txt')
 $steps=@('transferprepare','wait 8','transferverify','wait 3')
 if($mode -eq 'detached') { $steps=@('waitidle','detachedsurfacefixture keepmain','wait 6')+@($steps | ForEach-Object {if($_ -notlike 'wait *') {'detached:'+$_} else {$_}}) } else {$steps=@('waitidle')+$steps}
 $steps+='quit';$steps | Set-Content $script
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/kyo_lo_0.txt" --ui-script "'+$script+'"') -PassThru
 if(-not $p.WaitForExit(30000)) {$p.Kill();throw 'State transfer timeout'}
 $log=Get-Content ($script+'.log') -Raw
 if($p.ExitCode -ne 0 -or $log -notmatch 'transferprepare: ready=1 changes=3' -or $log -notmatch 'transferverify: owner=1 apply=1 undo=1 redo=1 stale=1') {throw "Transfer failed ($mode): $log"}
}
if((Get-FileHash C:/Games/MBAACC_Edit/data/kyo_lo_0.HA6).Hash -ne $hash) {throw 'Live HA6 changed'}
Write-Host "STATE TRANSFER LIVE PASS: $out; main/detached ownership, actual preview/apply, one-step undo/redo, stale rejection; no file writes"
