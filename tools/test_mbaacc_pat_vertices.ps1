param([string]$Executable="$PSScriptRoot/../build/gonptechan.exe")
$ErrorActionPreference='Stop'
& "$PSScriptRoot/test_pat_transform_trace.ps1" -Executable $Executable -Pattern 173 -Tick 6 -RuntimeTrace "$PSScriptRoot/../tests/fixtures/mbaacc_pat173_runtime.json"
& "$PSScriptRoot/test_pat_transform_trace.ps1" -Executable $Executable -Pattern 173 -Tick 6 -Facing left -RuntimeTrace "$PSScriptRoot/../tests/fixtures/mbaacc_pat173-left_runtime.json"
