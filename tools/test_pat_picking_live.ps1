param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('pat-picking-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$hash=(Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash
foreach($hostMode in @('main','detached')) {
 $commands=Join-Path $out ($hostMode+'.txt')
 $steps=@('waitidle')
 foreach($kind in @('normal','flip','compressed','compressedflip')) { $steps+="patpickfixture $kind"; $steps+='wait 5'; $steps+='patpicktest' }
 if($hostMode -eq 'detached') { $steps=@('waitidle','detachedsurfacefixture keepmain','wait 6')+@($steps | Where-Object { $_ -ne 'waitidle' } | ForEach-Object { if($_ -like 'patpick*') { 'detached:'+$_ } else { $_ } }) }
 $steps+='quit'; $steps | Set-Content $commands
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$commands+'"') -PassThru
 if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'PAT picking timeout' }
 $log=Get-Content ($commands+'.log') -Raw
 if($p.ExitCode -ne 0 -or ([regex]::Matches($log,'patpicktest: triangles=2 covered=1 empty=1 cycle=1 move=1 rotation=1')).Count -ne 4) { throw "PAT picking failed ($hostMode): $log" }
}
if((Get-FileHash C:/Games/MBAACC_Edit/data/iori_0.HA6).Hash -ne $hash) { throw 'HA6 changed' }
Write-Host "PAT PICKING LIVE PASS: $out; transparent parts, overlap, flipped UV, move/undo/rotation, both hosts"
