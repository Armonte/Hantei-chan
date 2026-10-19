param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('detached-scroll-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$script=Join-Path $out 'test.txt'
@('waitidle','pattern 333','viewframe 10','detachedsurfacefixture keepmain','wait 6','detached:panelscroll hover','wait 5','detached:panelscroll wheel','wait 12','detached:panelscroll inspect','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Detached scroll timeout' }
if($p.ExitCode -ne 0) { throw 'Detached scroll process failed' }
$trace=Get-Content (Join-Path $out 'pane_scroll_trace.log') -Raw
$rows=@([regex]::Matches($trace,'frame=(\d+) pane=Right Pane###host\d+_Right Pane restore=(\d) actual=([\d.]+) saved=([\d.]+) wheel=([-\d.]+)'))
$tail=@($rows | Select-Object -Last 8)
if($tail.Count -ne 8 -or [double]$tail[0].Groups[3].Value -le 0 -or ($tail | Where-Object { $_.Groups[2].Value -ne '0' -or $_.Groups[3].Value -ne $tail[0].Groups[3].Value })) { throw "Scroll oscillation or repeated restore: $out" }
Write-Host "DETACHED SCROLL LIVE PASS: $out"
