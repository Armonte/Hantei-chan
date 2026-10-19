param([string]$DataRoot, [string]$Executable = "$PSScriptRoot/../build/preview_sim_tool.exe")
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('opponent-scene-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$source = Join-Path $DataRoot 'iori_0.HA6'
$hash = (Get-FileHash -LiteralPath $source).Hash
foreach ($state in @(10,11)) {
    $output = & $exe --data $DataRoot --char iori --moon 0 --pattern 333 --opponent-state $state --extended
    if ($LASTEXITCODE -ne 0) { throw 'Authored inspection failed' }
    $output | Set-Content -LiteralPath (Join-Path $artifacts "authored-$state.log")
    if ($state -eq 10 -and ($output -join "`n") -notmatch 'target=26/10 release=0 .*entry=-1 elapsed=0') { throw 'Live state-10 fixture changed; inspect source' }
    if ($state -eq 11 -and ($output -join "`n") -notmatch 'release=1 vector=8 entry=-1 elapsed=0') { throw 'Live state-11 fixture changed; inspect source' }
}
$runtime = & $exe --data $DataRoot --char iori --moon 0 --pattern 333 --ticks 100 --every 10 --opponent-trace --extended
if ($LASTEXITCODE -ne 0) { throw 'Runtime-flow trace failed' }
$runtime | Set-Content -LiteralPath (Join-Path $artifacts 'runtime.log')
if (($runtime -join "`n") -match 'release=1') { throw 'Current default-policy trace changed; inspect source/IF options' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $hash) { throw 'Source HA6 changed' }
@{ source = $source; sha256 = $hash; scope = 'current Iori fixture, editor simulator only' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $artifacts 'source.json')
Write-Host "OPPONENT SCENE LIVE PASS: $artifacts; source $hash"
