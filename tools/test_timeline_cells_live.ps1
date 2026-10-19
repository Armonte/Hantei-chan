param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('timeline-cells-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$script=Join-Path $out 'test.txt'
@('waitidle','compactui wide','wait 6','compactui timelinewide','wait 4','pattern 333','timelinecellfixture','timelinezoom 8','wait 5',
  ('capture '+$out+'/duration-wide.png'),'timelinezoom 0.5','wait 5',('capture '+$out+'/duration-small.png'),
  'detachedsurfacefixture keepmain','wait 5','detached:timelinecellfixture','detached:timelinezoom 8','wait 5','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --size 1920x1080 --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(!$p.WaitForExit(30000)) {$p.Kill();throw "Timeline cells timeout: $out"}
if($p.ExitCode -ne 0) {throw "Timeline cells process failed: $out"}
$groups=@{}
foreach($line in (Get-Content (Join-Path $out 'timeline_cell_trace.log'))) {
 if($line -match 'frame=(\d+) window=(.+) row=(authored|duration|keyframes) state=(\d+) x0=([\d.-]+) x1=([\d.-]+) y=([\d.-]+) duration=(\d+)') {
  $key=$Matches[1]+' '+$Matches[2]+' '+$Matches[4]
  if(!$groups.ContainsKey($key)) {$groups[$key]=@{}}
  $groups[$key][$Matches[3]]=@([double]$Matches[5],[double]$Matches[6],[double]$Matches[7],[int]$Matches[8])
 }
}
$main=$false;$detached=$false
foreach($key in $groups.Keys) {
 $r=$groups[$key];if($r.Count -ne 3) {continue}
 foreach($row in @('duration','keyframes')) {
  if($r[$row][0] -ne $r.authored[0] -or $r[$row][1] -ne $r.authored[1] -or $r[$row][3] -ne $r.authored[3]) {throw "State boundaries disagree: $key ($out)"}
 }
 if($r.authored[2] -ge $r.duration[2] -or $r.duration[2] -ge $r.keyframes[2]) {throw 'Incorrect authored/duration/keyframes order'}
 if($key -match 'host\d+') {$detached=$true} else {$main=$true}
}
if(!$main -or !$detached) {throw "Missing main/detached geometry: $out"}
Write-Host "TIMELINE CELLS LIVE PASS: $out"
