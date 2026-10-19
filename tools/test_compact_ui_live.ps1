param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('compact-ui-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$script=Join-Path $out 'test.txt'
@('waitidle','compactui wide','wait 8','pattern 147','viewframe 6','opentrees 1','framenodes 7','wait 6','compactui 0','wait 4','compactui inspect',('capture '+$out+'/normal.png'),'compactui 1','wait 6','compactui inspect',('capture '+$out+'/compact.png'),('diagnosticsaveini '+$out+'/compact.ini'),'compactui panes','wait 6','compactui inspect',('capture '+$out+'/compact-wide.png'),'detachedsurfacefixture keepmain','wait 6','detached:compactui inspect','compactui 0','wait 6','detached:compactui inspect','compactui 1','wait 6','quit') | Set-Content $script
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --size 1920x1080 --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$script+'"') -PassThru
if(!$p.WaitForExit(45000)) {$p.Kill();throw "Compact UI timeout: $out"}
if($p.ExitCode -ne 0) {throw "Compact UI process failed: $out"}
$log=Get-Content ($script+'.log') -Raw
if($log -match 'failed|unknown' -or $log -notmatch 'compactui: enabled=1') {throw "Compact UI script failed: $out"}
$versions=@([regex]::Matches($log,'version=(\d+)') | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique)
if($versions.Count -ne 1) {throw "Layout toggle changed document data: $out"}
$heights=@([regex]::Matches($log,'compactui: enabled=(\d)[^\r\n]*?\[Left Pane/FrameInfo_[^ ]+ content=([\d.]+)'))
$normal=($heights | Where-Object {$_.Groups[1].Value -eq '0'} | ForEach-Object {[double]$_.Groups[2].Value} | Measure-Object -Maximum).Maximum
$compact=($heights | Where-Object {$_.Groups[1].Value -eq '1'} | ForEach-Object {[double]$_.Groups[2].Value} | Measure-Object -Minimum).Minimum
if(!$normal -or !$compact -or $compact -ge $normal*.95) {throw "No meaningful inspector height reduction: $out"}
if((Get-Content (Join-Path $out 'compact.ini') -Raw) -notmatch 'CompactUi=1') {throw 'Compact UI persistence failed'}
# Restart with the saved global preference in an isolated INI, never the user's.
Copy-Item (Join-Path $out 'compact.ini') (Join-Path $out 'hanteichan.ini')
$restart=Join-Path $out 'restart.txt'
@('waitidle','compactui inspect','quit') | Set-Content $restart
$p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --game mbaacc --open "C:/Games/MBAACC_Edit/data/iori_0.txt" --ui-script "'+$restart+'"') -PassThru
if(!$p.WaitForExit(30000)) {$p.Kill();throw 'Compact UI restart timeout'}
if($p.ExitCode -ne 0 -or (Get-Content ($restart+'.log') -Raw) -notmatch 'compactui: enabled=1') {throw 'Global preference did not survive restart'}
Write-Host "COMPACT UI LIVE PASS: $out"
