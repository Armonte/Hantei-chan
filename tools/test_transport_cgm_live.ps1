param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
$exe=(Resolve-Path $Executable).Path
$out=Join-Path $env:TEMP ('transport-cgm-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
foreach($char in @('iori_0','kyo_lo_0','angel_0','shermie_0')) {
    $script=Join-Path $out ($char+'.txt')
    @('waitidle','pattern 147','viewframe 1','transportcheck',
      'detachedsurfacefixture keepmain','wait 4','detached:transportcheck',
      'cgm open','wait 10','cgm select 112','wait 4',
      ('capture '+$out+'/'+$char+'.png'),'quit') | Set-Content $script
    $p=Start-Process $exe -WindowStyle Hidden -WorkingDirectory $out -ArgumentList ('--headless --size 1280x800 --game mbaacc --open "C:/Games/MBAACC_Edit/data/'+$char+'.txt" --ui-script "'+$script+'"') -PassThru
    if(!$p.WaitForExit(30000)) {$p.Kill();throw "Timeout: $char ($out)"}
    $log=Get-Content ($script+'.log') -Raw
    if($p.ExitCode -ne 0 -or $log -match 'FAIL|failed|unknown' -or ([regex]::Matches($log,'transportcheck: PASS')).Count -ne 2) {throw "Transport/CG failure: $char ($out)"}
}
if(Test-Path (Join-Path $out 'cgm_gl_trace.log')) {throw "GL error recorded: $out"}
Write-Host "TRANSPORT CG LIVE PASS: $out"
