param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('static-preview-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
foreach($mode in @('main','detached')) {
 $script=Join-Path $out ($mode+'.txt')
 $steps=@('diagnosticpattern 179','absolutescrub 14','wait 4','motioninspect','motiontoggle on','wait 4','motioninspect','motiontoggle off','wait 4','motioninspect')
 if($mode -eq 'detached') { $steps=@('waitidle','detachedsurfacefixture keepmain','wait 6')+@($steps | ForEach-Object { if($_ -notlike 'wait *') { 'detached:'+$_ } else { $_ } }) } else { $steps=@('waitidle')+$steps }
 $steps+='quit'; $steps | Set-Content $script
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/kyo_lo_0.txt" --ui-script "'+$script+'"') -PassThru
 if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Static preview timeout' }
 $log=Get-Content ($script+'.log') -Raw
 if($p.ExitCode -ne 0 -or ([regex]::Matches($log,'root=0.000000 child=20.000000 found=1')).Count -ne 2 -or $log -notmatch 'root=44.531250 child=64.531250 found=1') { throw "Movement toggle failed: $log" }
}
$script=Join-Path $out 'effect.txt'
@('waitidle','pattern 67','absolutescrub 7','wait 5','positiongesturetest persistentfx','wait 4','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Effect editing timeout' }
$log=Get-Content ($script+'.log') -Raw
if($p.ExitCode -ne 0 -or $log -notmatch 'positiongesturetest: footprint=1 move=1 rotation=1') { throw "Persistent effect spawn editing failed: $log" }
Write-Host "STATIC PREVIEW LIVE PASS: $out; off/on/off both hosts, Iori67 later-state effect source undo"
