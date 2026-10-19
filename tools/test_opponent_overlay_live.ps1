param([Parameter(Mandatory=$true)][string]$DataRoot,[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Executable).Path
$root=(Resolve-Path -LiteralPath $DataRoot).Path
$artifacts=Join-Path ([IO.Path]::GetTempPath()) ('opponent-overlay-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$script=Join-Path $artifacts 'smoke.txt'
$poseTrace=Join-Path $artifacts 'held.jsonl'
$releaseTrace=Join-Path $artifacts 'release.jsonl'
@('waitidle',"openany $root/kyo_lo_0.txt",'waitidle','switchview 0','pattern 333','viewframe 10',
 'opponentpreview 1','wait 12','opponentinspect',"transformtrace $poseTrace",'wait 6',
 "captureview $artifacts/held.png",'viewframe 11','wait 8','opponentinspect',
 "transformtrace $releaseTrace",'wait 6','quit') | Set-Content -LiteralPath $script
$process=Start-Process -FilePath $exe -ArgumentList ('--headless --game mbaacc --open "'+$root+'/iori_0.txt" --ui-script "'+$script+'"') -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Hidden diagnostic timed out' }
if($process.ExitCode -ne 0) { throw 'Overlay app failed' }
$log=Get-Content -LiteralPath ($script+'.log') -Raw
if($log -notmatch 'Selected state: 26 / 10' -or $log -notmatch 'Release vectors disabled') { throw "Unexpected pose/release resolution: $log" }
$held=@(Get-Content -LiteralPath $poseTrace | ForEach-Object { $_ | ConvertFrom-Json })
if(@($held | Where-Object { $_.pattern -eq 26 -and $_.state -eq 10 -and $_.texture_available }).Count -ne 1) { throw 'Held target not rendered once with real CG' }
$released=@(Get-Content -LiteralPath $releaseTrace | ForEach-Object { $_ | ConvertFrom-Json })
if(@($released | Where-Object { $_.pattern -eq 26 }).Count -ne 0) { throw 'Release retained stale opponent overlay' }
Write-Host "OPPONENT OVERLAY SMOKE PASS: $artifacts (editor contract, not runtime equivalence)"
