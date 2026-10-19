param([Parameter(Mandatory=$true)][string]$DataRoot,[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $DataRoot).Path
$exe=(Resolve-Path -LiteralPath $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('grab-release-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$source=Join-Path $root 'iori_0.HA6'
$before=(Get-FileHash -LiteralPath $source).Hash
$script=Join-Path $out 'smoke.txt'
@('waitidle','pattern 333','viewframe 0','opponentpreview 0',"opponentselect $root/kyo_lo_0.txt",'wait 8',
 'opponentsystemkeytest',"opponentselect $root/kyo_lo_0.txt",'opponentraisetest','wait 8','opponentkeys','wait 8',
 "capture $out/keys.png",'opponentreleasefixture','opponentrelease 1','opponenttick 1','wait 8','opponentinspect',
 'opponenttick 2','wait 8','opponentinspect','opponenttick 13','wait 8','opponentinspect',
 'opponenttick 26','wait 8','opponentinspect','opponenttick 90','wait 8','opponentinspect',
 'opponenttick 2','wait 8','opponentinspect','quit') | Set-Content -LiteralPath $script
$process=Start-Process -FilePath $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$root+'/iori_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Hidden test timed out' }
if($process.ExitCode -ne 0) { throw 'Hidden test failed' }
$log=Get-Content -LiteralPath ($script+'.log') -Raw
foreach($expected in @('opponentsystemkeytest: cycled','opponentkeys: visible registry rows=3','elapsed=0 y=0 landing=-1','elapsed=1 y=-6 landing=-1','elapsed=12 y=-38 landing=-1','elapsed=25 y=0 landing=24')) {
    if(-not $log.Contains($expected)) { throw "Missing $expected; $log" }
}
if(([regex]::Matches($log,'elapsed=1 y=-6 landing=-1')).Count -ne 2) { throw 'Rewind/replay did not reproduce release position' }
$window=Get-Content -LiteralPath (Join-Path $out 'opponent_window_trace.log') -Raw
if($window -notmatch 'collapsed=0 focused=1') { throw 'Collapsed settings did not restore/focus' }
if((Get-FileHash -LiteralPath $source).Hash -ne $before) { throw 'In-memory fixture changed source file' }
Write-Host "GRAB RELEASE/SYSTEM-KEY LIVE PASS: $out; source unchanged $before"
