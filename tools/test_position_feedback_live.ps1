param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('position-feedback-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$hash=(Get-FileHash "$root/kyo_lo_0.HA6").Hash
$script=Join-Path $out 'test.txt'
$project=Join-Path $out 'settings.hproj'
@('waitidle','pattern 149','positionoverlapfixture','wait 4','positionoverlaptest','positionpickmodifier off','positionfeedbackprobe off','wait 4','positiontoolinspect',
  'positionfeedbackprobe on','wait 3','positiontoolinspect','positionfeedbackprobe off','wait 3','positiontoolinspect',
  'positionpickmodifier ctrl','wait 3','positiontoolinspect',"capture $out/anchored.png",'positionpickmodifier off','wait 3',
  'positionsolo toggle','wait 3','positiontoolinspect','positionsolo edit','wait 3','positiontoolinspect','positionsolo cycle','wait 3','positiontoolinspect',
  'positionsolo toggle','wait 3','positiontoolinspect','positionsolo toggle','wait 3','positionsolo scope','wait 3','positiontoolinspect',
  'positionconfigopen','wait 3',"capture $out/config.png",'positionfeedbackprobe on',"opponentprojectsave $project",'quit') | Set-Content $script
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/kyo_lo_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Position feedback diagnostic timeout' }
if($p.ExitCode -ne 0) { throw 'Position feedback diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
$rows=@([regex]::Matches($log,'positiontoolinspect: feedback=(\d) anchor=(\d) solo=(\d) footprints=(\d)') | ForEach-Object { $_.Groups[1].Value+','+$_.Groups[2].Value+','+$_.Groups[3].Value+','+$_.Groups[4].Value })
if(($rows -join ';') -ne '0,1,0,2;1,1,0,2;0,1,0,2;1,1,0,2;0,1,1,1;0,1,1,1;0,1,1,1;0,1,0,2;0,1,0,2') { throw "Feedback/Solo/anchor contract failed: $log" }
if($log -notmatch 'positionsoloedit: retained=1') { throw "Solo edit failed: $log" }
$settings=(Get-Content $project -Raw | ConvertFrom-Json).views[0].position_tool_settings
if(-not $settings.handle_hover -or -not $settings.highlight) { throw 'Position settings not saved' }
$reload=Join-Path $out 'reload.txt'
@('waitidle','positiontoolinspect','quit') | Set-Content $reload
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$project+'" --ui-script "'+$reload+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Position settings restore timeout' }
if($p.ExitCode -ne 0 -or (Get-Content ($reload+'.log') -Raw) -notmatch 'handle_hover=1 highlight=1') { throw 'Position settings not restored' }
if((Get-FileHash "$root/kyo_lo_0.HA6").Hash -ne $hash) { throw 'HA6 input changed' }
Write-Host "POSITION FEEDBACK LIVE PASS: $out; HA6 unchanged"
