param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'; $exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('detached-surface-'+[guid]::NewGuid().ToString('N')); New-Item -ItemType Directory $out | Out-Null
$script=Join-Path $out 'test.txt'
$hash=(Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash
@('waitidle','detachedsurfacefixture','wait 6','detachedsurfaceinput hover','wait 4','detachedsurfaceinspect','detachedsurfaceinput wheel','wait 4','detachedsurfaceinspect',
 'detachedsurfaceinput pandown','detachedsurfaceinput panmove','detachedsurfaceinput panup','wait 3','detachedsurfaceinspect',
 'detachedsurfaceinput boxdown','detachedsurfaceinput boxmove','detachedsurfaceinput boxup','wait 3','detachedsurfaceinspect','detachedsurfaceundo','wait 3','detachedsurfaceinspect','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Detached surface timeout' }
if($p.ExitCode -ne 0) { throw 'Detached surface diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
$rows=@([regex]::Matches($log,'zoom=([\d.]+) pan=([-\d.]+),([-\d.]+) dirty=(\d)'))
if($rows.Count -ne 5 -or [double]$rows[1].Groups[1].Value -le [double]$rows[0].Groups[1].Value -or [double]$rows[2].Groups[2].Value -ne 30 -or $rows[3].Groups[4].Value -ne '1' -or $rows[4].Groups[4].Value -ne '0') { throw "Detached surface contracts failed: $log" }
if((Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash -ne $hash) { throw 'HA6 changed' }
Write-Host "DETACHED SURFACE LIVE PASS: $out; zoom + pan + box drag/undo, HA6 unchanged"
