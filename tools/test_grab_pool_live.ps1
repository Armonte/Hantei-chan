param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('grab-pool-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$project=Join-Path $out 'test.hproj'
$script=Join-Path $out 'save.txt'
@('waitidle','grabpooltest',"opponentprojectsave $project",("diagnosticsaveini "+(Join-Path $out 'hanteichan.ini')),'quit') | Set-Content $script
function RunHidden($inputFile,$commands) {
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$inputFile+'" --ui-script "'+$commands+'"') -PassThru
 if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Grab pool timeout' }
 if($p.ExitCode -ne 0) { throw 'Grab pool process failed' }
}
RunHidden 'C:/Games/MBAACC_Edit/data/iori_0.txt' $script
if((Get-Content ($script+'.log') -Raw) -notmatch 'grabpooltest: shared=1 separate=1 rejoin=1') { throw 'Sharing/override contract failed' }
$saved=(Get-Content $project -Raw | ConvertFrom-Json).views[0].grab_preview
if(-not $saved.separate_pool -or $saved.pool.Count -ne 1) { throw 'Tab override not saved' }
$reload=Join-Path $out 'reload.txt'
@('waitidle','grabpoolinspect','quit') | Set-Content $reload
RunHidden $project $reload
if((Get-Content ($reload+'.log') -Raw) -notmatch 'global=1 separate=1 effective=1') { throw 'Global preferences or project override did not survive restart' }
$fresh=Join-Path $out 'fresh.txt'
@('waitidle','grabpoolinspect','quit') | Set-Content $fresh
RunHidden 'C:/Games/MBAACC_Edit/data/iori_0.txt' $fresh
if((Get-Content ($fresh+'.log') -Raw) -notmatch 'global=1 separate=0 effective=1') { throw 'New tab did not use saved global list' }
Write-Host "GRAB POOL LIVE PASS: $out; shared default, opt-out/rejoin, preferences + project restart"
