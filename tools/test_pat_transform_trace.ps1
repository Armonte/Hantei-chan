param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe",[int]$Pattern=175,[int]$Tick=8,[ValidateSet("right","left")][string]$Facing="right",[string]$RuntimeTrace="")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('pat-transform-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$inputs=@('C:/Games/MBAACC_Edit/data/effect.txt','C:/Games/MBAACC_Edit/data/effect.HA6','C:/Games/MBAACC_Edit/data/effect.PAT') | ForEach-Object { @{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash} }
foreach($mode in @('main','detached')) {
 $run=Join-Path $out $mode; New-Item -ItemType Directory $run | Out-Null
 $trace=Join-Path $out ($mode+'.jsonl'); $script=Join-Path $out ($mode+'.txt')
 $steps=@("diagnosticpattern $Pattern")
 if($Pattern -eq 173) { $steps+= "pattransformfixture $Facing" }
 $steps+=@("absolutescrub $Tick",'wait 5',"transformtrace $trace",'wait 3')
 if($mode -eq 'detached') { $steps=@('waitidle','detachedsurfacefixture','wait 6')+@($steps | ForEach-Object { if($_ -notlike 'wait *') {'detached:'+$_} else {$_} }) } else { $steps=@('waitidle')+$steps }
 if($Pattern -eq 173) { $prefix=if($mode -eq 'detached') {'detached:'} else {''}; $steps+= ($prefix+'positiongesturetest effect'); $steps+='wait 3' }
 $steps+='quit'; $steps | Set-Content $script
 $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $run -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/effect.txt" --ui-script "'+$script+'"') -PassThru
 if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'PAT transform trace timeout' }
 if($Pattern -eq 173 -and (Get-Content ($script+'.log') -Raw) -notmatch 'positiongesturetest: footprint=1 move=1 rotation=1') {throw 'PAT spawn position interaction failed'}
 if($p.ExitCode -ne 0) { throw "Editor exited $($p.ExitCode)" }
 $rows=@(& "$PSScriptRoot/read_transform_trace.ps1" -Log (Join-Path $run 'hanteichan.log') -Tag $trace | Where-Object {$_.kind -eq 'pat' -and $_.pattern -eq $Pattern -and $_.sprite -eq 84})
 if(!$rows.Count) { throw "No actual PAT84 geometry captured: $out" }
 if($RuntimeTrace) {
  $runtime=Get-Content $RuntimeTrace -Raw | ConvertFrom-Json
  if($Pattern -ne 173 -or $Tick -ne 6 -or $runtime.owner_pattern -ne 63 -or $runtime.pattern -ne 173 -or $runtime.keyframe -ne 1 -or $runtime.angle -ne 0 -or $runtime.facing_left -ne ($Facing -eq 'left') -or $runtime.scale[0] -le 0 -or $runtime.scale[0] -ne $runtime.scale[1]) {throw 'Unsynchronized runtime fixture'}
  if($rows.Count -ne 1) {throw 'Ambiguous PAT submission'}
  # Runtime TL/TR/BL/BR maps to actual GL triangle vertex slots 0/1/4/2.
  $order=@(0,1,4,2); $max=0.0
  for($i=0;$i -lt 4;$i++) {for($axis=0;$axis -lt 2;$axis++) {
   $expected=($runtime.final_screen_vertices_xyz[$i][$axis]-$runtime.ha6_actor_matrix_row_major[3][$axis])/$runtime.scale[$axis]
   $max=[Math]::Max($max,[Math]::Abs($expected-$rows[0].vertices[$order[$i]].normalized[$axis]))
  }}
  if($max -gt 0.001) {throw "Runtime PAT vertex mismatch: $max"}
  Write-Host "PAT runtime vertex comparison ($mode / $Facing): max error $max"
 }
 foreach($r in $rows) {
  if($r.submitted_matrix.Count -ne 16 -or $r.vertices.Count -lt 3) {throw 'Incomplete geometry trace'}
  foreach($v in $r.vertices) {if($v.local.Count -ne 3 -or $v.clip.Count -ne 4 -or $v.normalized.Count -ne 2) {throw 'Incomplete vertex'}}
 }
}
foreach($i in $inputs) {if((Get-FileHash -LiteralPath $i.path).Hash -ne $i.sha256) {throw 'Input changed'}}
@{inputs=$inputs;editor_sha256=(Get-FileHash $exe).Hash;scope='Actual editor PAT submission only; NOT runtime equivalence';pattern=$Pattern;tick=$Tick;pat=84} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'inputs.json')
Write-Host "PAT TRANSFORM TRACE PASS (not game equivalence): $out"
