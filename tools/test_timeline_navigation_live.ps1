param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('timeline-nav-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$hash=(Get-FileHash "$root/iori_0.HA6").Hash
$script=Join-Path $out 'test.txt'
$project=Join-Path $out 'navigation.hproj'
@('waitidle','pattern 333','timelinenavigationtest','wait 4',"capture $out/navigation.png", "opponentprojectsave $project",'timelinedurationtest','quit') | Set-Content $script
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/iori_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Timeline diagnostic timeout' }
if($p.ExitCode -ne 0) { throw 'Timeline diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
if($log -notmatch 'timelinenavigationtest: inspect=1 replay=1 policy=1 once=1 loop=1 absolute=1') { throw "Timeline contracts failed: $log" }
if($log -notmatch 'timelinedurationtest: preview=1 cancel=1 commit=1 undo=1 redo=1 stale=1 cancel_route=1') { throw "Duration contracts failed: $log" }
$saved=Get-Content $project -Raw | ConvertFrom-Json
if($saved.views[0].playback_loop -or -not $saved.views[0].authored_inspection -or $saved.views[0].authored_offset -ne 1) { throw 'Timeline project settings not saved' }
if((Get-FileHash "$root/iori_0.HA6").Hash -ne $hash) { throw 'HA6 input changed' }
Write-Host "TIMELINE NAVIGATION LIVE PASS: $out; HA6 unchanged"
