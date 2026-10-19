param(
    [Parameter(Mandatory=$true)][string]$DataRoot,
    [string]$Executable = "$PSScriptRoot/../build/gonptechan.exe",
    [string]$Character = 'kyo_lo',
    [int]$Pattern = 149,
    [int]$Tick = 0,
    [int]$TargetPattern = 400,
    [int]$TargetState = 0
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$root = (Resolve-Path -LiteralPath $DataRoot).Path
# Open the descriptor, not the HA6 alone: otherwise missing CGs leave placeholder
# geometry that can produce a plausible but meaningless transform comparison.
$descriptor = Join-Path $root ($Character + '_0.txt')
$ha6 = Join-Path $root ($Character + '_0.HA6')
$cg = Join-Path $root ($Character + '.cg')
$before = @($descriptor,$ha6,$cg) | ForEach-Object {
    $f = Get-Item -LiteralPath $_
    @{ path=$f.FullName; size=$f.Length; sha256=(Get-FileHash -LiteralPath $_).Hash }
}
$artifacts = Join-Path ([IO.Path]::GetTempPath()) ('cg-transform-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $artifacts | Out-Null
$trace = Join-Path $artifacts 'editor.jsonl'
$script = Join-Path $artifacts 'trace.txt'
# Generated input for the existing application script runner, not source code.
@('waitidle','wait 15',('transformtrace ' + $trace),'wait 5','quit') | Set-Content -LiteralPath $script
$args = '--headless --game mbaacc --open "' + $descriptor + '" --pattern ' + $Pattern + ' --tick ' + $Tick + ' --ui-script "' + $script + '"'
$process = Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) {
    # This is our isolated hidden diagnostic process, never the daily editor.
    $process.Kill()
    throw "Diagnostic exceeded 30 seconds; artifacts: $artifacts"
}
if ($process.ExitCode -ne 0) { throw "Diagnostic exited $($process.ExitCode)" }
$rows = @(Get-Content -LiteralPath $trace | ForEach-Object { $_ | ConvertFrom-Json })
$target = @($rows | Where-Object { $_.pattern -eq $TargetPattern -and $_.state -eq $TargetState })
if ($target.Count -ne 1) { throw "Expected one target, found $($target.Count): $trace" }
if (-not $target[0].texture_available) { throw "Missing real sprite texture: $trace" }
if ($target[0].normalized_vertices.Count -ne 4) { throw 'Incomplete final vertices' }
foreach ($traceInput in $before) {
    if ((Get-FileHash -LiteralPath $traceInput.path).Hash -ne $traceInput.sha256) { throw "Input changed: $($traceInput.path)" }
}
@{ inputs=$before; editor_sha256=(Get-FileHash -LiteralPath $exe).Hash; pattern=$Pattern; tick=$Tick;
   target_pattern=$TargetPattern; target_state=$TargetState; scope='Editor capture only; NOT game equivalence' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $artifacts 'inputs.json')
Write-Host "CG TRANSFORM CAPTURE PASS (not game equivalence): $artifacts"
