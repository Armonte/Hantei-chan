param([string]$DataRoot='C:/Games/MBAACC_Edit/data',[string]$Executable="$PSScriptRoot/../build/gonptechan.exe",[switch]$Detached)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $DataRoot).Path; $exe=(Resolve-Path $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('opponent-edit-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$ownerHash=(Get-FileHash "$root/iori_0.HA6").Hash
$targetHash=(Get-FileHash "$root/kyo_lo_0.HA6").Hash
$commands=Join-Path $out 'test.txt'
$steps=@('waitidle','pattern 333','viewframe 0','opponentpreview 0',"opponentselect $root/kyo_lo_0.txt",'wait 8')
foreach($fixture in @('normal','left','native4','extended','release14','release4','releaseleft','release162','releaselanded')) {
    if($fixture -eq 'extended' -or $fixture -eq 'release162') { $steps+='extensionprofile extended' }
    $steps+="opponenteditfixture $fixture"; $steps+='wait 8'
    $steps+='opponentinspect'
    $modes=if($fixture -eq 'native4' -or $fixture.StartsWith('release')) { @('move','undo') } else { @('move','rotate','undo') }
    foreach($mode in $modes) { $steps+="positiongesturetest opponent-$mode"; $steps+='wait 8' }
}
if($Detached) {
 $steps=@('waitidle','detachedsurfacefixture keepmain','wait 6')+@($steps | Where-Object { $_ -ne 'waitidle' } | ForEach-Object {
  if($_ -match '^(opponenteditfixture|opponentinspect|positiongesturetest|opponentselect|opponentpreview) ') { 'detached:'+$_ }
  elseif($_ -eq 'opponentinspect') { 'detached:'+$_ }
  else { $_ }
 })
 # The fixture pattern must belong to the detached owner, not the new main tab.
 $steps=$steps | ForEach-Object { if($_ -eq 'pattern 333') { 'detached:diagnosticpattern 333' } else { $_ } }
}
$steps+='quit'; $steps | Set-Content $commands
$p=Start-Process $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/iori_0.txt" --ui-script "'+$commands+'"') -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Hidden editor test timed out' }
if($p.ExitCode -ne 0) { throw 'Hidden editor failed' }
$log=Get-Content ($commands+'.log') -Raw
if(([regex]::Matches($log,'positiongesturetest: footprint=1 move=1 rotation=1[^\r\n]*kind=3')).Count -ne 21) { throw "Opponent pose editing failed: $log" }
if(([regex]::Matches($log,'opponentinspect: Release approximation:')).Count -ne 5) { throw "Release fixtures were not actually active (old executable?): $log" }
if((Get-FileHash "$root/iori_0.HA6").Hash -ne $ownerHash -or (Get-FileHash "$root/kyo_lo_0.HA6").Hash -ne $targetHash) { throw 'HA6 input changed' }
Write-Host "OPPONENT POSE EDIT PASS: $out; both HA6 inputs unchanged"
