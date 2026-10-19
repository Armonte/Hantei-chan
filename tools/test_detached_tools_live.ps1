param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('detached-tools-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$script=Join-Path $out 'test.txt'
$hash=(Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash
@('waitidle','detachedtoolfixture','detachedsurfacefixture keepmain','wait 6','detachedtoolinput hover','wait 3','detachedtoolinspect',
 'detachedtoolinput down','wait 2','detachedtoolinput move','wait 2','detachedtoolinput up','wait 2','detachedtoolinspect',
 'detachedtoolinput undo','wait 3','detachedtoolinspect',
 'detachedtoolinput rdown','wait 2','detachedtoolinput rmove1','wait 2','detachedtoolinput rmove2','wait 2','detachedtoolinput rup','wait 2','detachedtoolinspect',
 'detachedtoolinput undo','wait 3','detachedtoolinspect','detachedtoolinput play','wait 8','detachedtoolinput stop','detachedtoolinspect','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Detached tools timeout' }
$log=Get-Content ($script+'.log') -Raw
$rows=@([regex]::Matches($log,'detachedtoolinspect: x=(-?\d+) y=(-?\d+) rotation=([-\d.]+) tick=(\d+) footprints=(\d+) active=(\d)'))
if($p.ExitCode -ne 0 -or $rows.Count -ne 6) { throw "Detached tools diagnostic failed: $log" }
if($rows[0].Groups[5].Value -ne '1' -or $rows[1].Groups[1].Value -eq '0' -or $rows[1].Groups[2].Value -eq '0' -or $rows[2].Groups[1].Value -ne '0' -or $rows[2].Groups[2].Value -ne '0' -or [math]::Abs([double]$rows[3].Groups[3].Value) -lt .1 -or [double]$rows[4].Groups[3].Value -ne 0 -or [int]$rows[5].Groups[4].Value -le 0) { throw "Detached move/rotate/undo/play contracts failed: $log" }
if($rows | Where-Object { $_.Groups[6].Value -ne '0' }) { throw 'Position transaction leaked' }
$ticks=@([regex]::Matches((Get-Content (Join-Path $out 'view_playback_trace.log') -Raw),'frame=(\d+) host=(\d+) view=(\d+) tick=(\d+)->(\d+)'))
if($ticks.Count -lt 8 -or ($ticks | Where-Object { [int]$_.Groups[5].Value - [int]$_.Groups[4].Value -ne 1 })) { throw 'Playback did not advance once per host frame' }
if((Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash -ne $hash) { throw 'HA6 changed' }
Write-Host "DETACHED TOOLS LIVE PASS: $out; real surface move/rotation/undo/playback; HA6 unchanged"
