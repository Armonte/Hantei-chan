param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('tab-reorder-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
foreach($mode in @('main','detached')) {
$script=Join-Path $out ($mode+'.txt')
@('waitidle',("tabfixture "+$mode),'wait 6','tabinspect','tabmoveprobe','wait 6','tabinspect',
 'tabinput hover','wait 2','tabinput down','wait 2','tabinput drag','wait 3','tabinput up','wait 6','tabinspect',
 'tabinput hover','wait 2','tabinput downlast','wait 2','tabinput drag','wait 3','tabinput up','wait 6','tabinspect','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(-not $p.WaitForExit(30000)) { $p.Kill(); throw 'Tab reorder timeout' }
$log=Get-Content ($script+'.log') -Raw
if($p.ExitCode -ne 0 -or $log -notmatch 'match=1 order=2,1,3,' -or $log -notmatch 'match=1 order=2,3,1,' -or ([regex]::Matches($log,'match=1 order=1,2,3,')).Count -ne 2) { throw "Tab order contract failed: $out; $log" }
}
Write-Host "TAB REORDER LIVE PASS: $out"
