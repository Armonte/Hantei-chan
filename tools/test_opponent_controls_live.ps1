param([Parameter(Mandatory=$true)][string]$DataRoot,[string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $DataRoot).Path
$exe=(Resolve-Path -LiteralPath $Executable).Path
$out=Join-Path ([IO.Path]::GetTempPath()) ('grab-controls-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$script=Join-Path $out 'save.txt'
$project=Join-Path $out 'test.hproj'
@('waitidle','pattern 333','viewframe 10','opponentpreview 0','opponentroster',
 "opponentselect $root/arc_0.txt",'wait 8','opponentinspect',"transformtrace $out/full.jsonl",'wait 5',
 'opponentopacity 0.35','opponentonion 1','wait 8',"transformtrace $out/onion.jsonl",'wait 5',
 'opponentpoolnone','opponentcycle 1',"opponentprojectsave $project",("diagnosticsaveini "+(Join-Path $out 'hanteichan.ini')),'quit') | Set-Content -LiteralPath $script
function RunHidden([string]$inputFile,[string]$commands) {
    $p=Start-Process -FilePath $exe -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "'+$inputFile+'" --ui-script "'+$commands+'"') -WindowStyle Hidden -PassThru
    if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Hidden diagnostic timed out' }
    if($p.ExitCode -ne 0) { throw 'Hidden diagnostic failed' }
}
RunHidden "$root/iori_0.txt" $script
$log=Get-Content -LiteralPath ($script+'.log') -Raw
if($log -notmatch 'opponentroster: (\d+)' -or [int]$Matches[1] -le 4) { throw 'Roster did not exceed open-tab list' }
if($log -notmatch 'Selected state: 26 / 10' -or $log -notmatch 'opponentcycle: empty pool or unavailable') { throw 'Selection/empty-pool contract failed' }
$trace=@(Get-Content -LiteralPath "$out/full.jsonl" | ForEach-Object { $_ | ConvertFrom-Json })
if(@($trace | Where-Object { $_.pattern -eq 26 -and $_.state -eq 10 -and $_.texture_available }).Count -ne 1) { throw 'Unopened target did not render' }
$onion=@(Get-Content -LiteralPath "$out/onion.jsonl" | ForEach-Object { $_ | ConvertFrom-Json })
if(@($onion | Where-Object { $_.pattern -eq 27 -and $_.texture_available }).Count -ne 1) { throw 'Current Iori future-onion fixture changed or opponent onion missing' }
$saved=Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
if($saved.characters.Count -ne 1) { throw 'Preview target unexpectedly created an editor character' }
$p=$saved.views[0].grab_preview
if(-not $p.onion -or [Math]::Abs($p.opacity-.35) -gt .001 -or $p.separate_pool -or -not $p.pool_initialized) { throw 'Persistence values incorrect' }
$reload=Join-Path $out 'reload.txt'
@('waitidle','opponentroster','opponentcontrolsinspect','opponentinspect','quit') | Set-Content -LiteralPath $reload
RunHidden $project $reload
$again=Get-Content -LiteralPath ($reload+'.log') -Raw
if($again -notmatch 'opacity=0.350000 onion=1 pool=0' -or $again -notmatch 'Selected state: 26 / 10') { throw 'Project reload lost settings or private target' }
Write-Host "GRAB CONTROLS LIVE PASS: $out (editor contracts; current Iori/Arc fixture)"
