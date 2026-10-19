param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('discover-paths-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$hash=(Get-FileHash "$root/iori_0.HA6").Hash
$script=Join-Path $out 'test.txt'; $project=Join-Path $out 'routes.hproj'
@('waitidle','pattern 333','discoverpathstest','wait 3',"opponentprojectsave $project",'quit') | Set-Content $script
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/iori_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Discovery diagnostic timeout' }
if($p.ExitCode -ne 0) { throw 'Discovery diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
if($log -notmatch 'discoverpathstest: discovery=1 selected=1 absolute=1 all=1 loop=1 stale=1') { throw "Route contracts failed: $log" }
$saved=Get-Content $project -Raw | ConvertFrom-Json
if($saved.views[0].route_start -ne 1) { throw 'Selected route not saved' }
$reload=Join-Path $out 'reload.txt'
@('waitidle','timelineinspect','quit') | Set-Content $reload
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$project+'" --ui-script "'+$reload+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Route restore timeout' }
if($p.ExitCode -ne 0 -or (Get-Content ($reload+'.log') -Raw) -notmatch 'timelineinspect: loop=1 authored=0 tick=0 route=1') { throw 'Selected route not restored' }
if((Get-FileHash "$root/iori_0.HA6").Hash -ne $hash) { throw 'HA6 input changed' }
Write-Host "DISCOVER PATHS LIVE PASS: $out; HA6 unchanged"
