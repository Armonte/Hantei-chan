param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('position-selection-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$hash=(Get-FileHash "$root/kyo_lo_0.HA6").Hash
$script=Join-Path $out 'test.txt'
@('waitidle','pattern 149','positionoverlapfixture','wait 4','positionoverlaptest','wait 3','positionwidgetinspect','positionpickmodifier ctrl','wait 3','positionwidgetinspect',"capture $out/selection.png",'positionpickmodifier alt','wait 3','positionwidgetinspect','positionpickmodifier off','wait 3','positionwidgetinspect',"capture $out/hidden.png",'quit') | Set-Content $script
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/kyo_lo_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Position selection diagnostic timeout' }
if($p.ExitCode -ne 0) { throw 'Position selection diagnostic failed' }
$log=Get-Content ($script+'.log') -Raw
if($log -notmatch 'positionoverlaptest: coverage=1 cycle=1 stable=1 source=1 drag=1 shortcut=1 rotation=1') { throw "Selection contracts failed: $log" }
$visibility=@([regex]::Matches($log,'positionwidgetinspect: visible=(\d) selected=1 candidates=2') | ForEach-Object { $_.Groups[1].Value }) -join ','
if($visibility -ne '0,1,1,0') { throw "Temporary widget visibility failed: $log" }
if((Get-FileHash "$root/kyo_lo_0.HA6").Hash -ne $hash) { throw 'HA6 input changed' }
Write-Host "POSITION SELECTION LIVE PASS: $out; HA6 unchanged"
