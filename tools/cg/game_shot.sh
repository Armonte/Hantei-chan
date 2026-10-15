#!/bin/bash
# tools/cg/game_shot.sh <game-dir (WSL)> <name> [actions]: launch MBAA.exe windowed from a hard-linked proof dir, optional shoot.ps1 actions, screenshot, kill by PID.
G="$1"; NAME="$2"; ACT="$3"; W=/mnt/c/dev/hantei-chan/work/cgm; WG=$(wslpath -w "$G")
PID=$(powershell.exe -NoProfile -Command "\$p=Start-Process -FilePath '$WG\\MBAA.exe' -WorkingDirectory '$WG' -PassThru; Start-Sleep 9; (Get-CimInstance Win32_Process -Filter \"Name='MBAA.exe'\" | Where-Object { \$_.ExecutablePath -like '$WG\\*' } | Select-Object -First 1).ProcessId" | tr -d '\r')
echo "pid $PID"
cp "$(dirname "$0")/../i18n_shoot/i18n_shoot.ps1" $W/shoot.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\hantei-chan\work\cgm\shoot.ps1' -ProcId "$PID" -Out 'C:\dev\hantei-chan\work\cgm' -Actions "w 500; ${ACT:+$ACT; }s $NAME" 2>&1 | tr -d '\r'
powershell.exe -NoProfile -Command "Stop-Process -Id $PID -Force" 2>&1 | tr -d '\r'
