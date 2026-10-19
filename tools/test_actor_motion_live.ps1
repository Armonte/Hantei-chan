param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('actor-motion-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$hash=(Get-FileHash C:/Games/MBAACC_Edit/data/kyo_lo_0.HA6).Hash
foreach($mode in @('main','detached')) {
 $commands=Join-Path $out ($mode+'.txt')
 $steps=@('diagnosticpattern 179','motiontoggle on','motionfacing right','absolutescrub 14','wait 4','motioninspect','absolutescrub 3','wait 3','absolutescrub 14','wait 4','motioninspect','motionfacing left','wait 4','motioninspect')
 if($mode -eq 'detached') { $steps=@('waitidle','detachedsurfacefixture keepmain','wait 6')+@($steps | ForEach-Object { if($_ -notlike 'wait *') { 'detached:'+$_ } else { $_ } }) }
 else { $steps=@('waitidle')+$steps }
 $steps+='quit'; $steps | Set-Content $commands
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/kyo_lo_0.txt" --ui-script "'+$commands+'"') -PassThru
 if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Actor motion timeout' }
 $log=Get-Content ($commands+'.log') -Raw
 if($p.ExitCode -ne 0 -or ([regex]::Matches($log,'motioninspect: root=44.531250 child=64.531250 found=1')).Count -ne 2 -or $log -notmatch 'root=-44.531250 child=-64.531250 found=1') { throw "Actor motion contract failed ($mode): $log" }
}
if((Get-FileHash C:/Games/MBAACC_Edit/data/kyo_lo_0.HA6).Hash -ne $hash) { throw 'HA6 changed' }
Write-Host "ACTOR MOTION LIVE PASS: $out; real Kyo179 AS vectors, parent/child origin, A-B-A, facing, both hosts; HA6 unchanged"
