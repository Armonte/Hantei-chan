#!/bin/bash
# tools/cg/shot.sh <name> <app args...>: launch the built editor from C:\dev\hantei-chan\work\cgm, screenshot its window (tools/i18n_shoot driver), kill it by PID.
D="$(cd "$(dirname "$0")/../.." && pwd)"; W=/mnt/c/dev/hantei-chan/work/cgm
NAME="$1"; shift
cp "$D/build/gonptechan.exe" $W/gonptechan.exe
ARGS=""; for a in "$@"; do ARGS="$ARGS,'${a//\'/\'\'}'"; done; ARGS=${ARGS#,}
PID=$(powershell.exe -NoProfile -Command "\$p=Start-Process -FilePath C:\\dev\\hantei-chan\\work\\cgm\\gonptechan.exe -WorkingDirectory C:\\dev\\hantei-chan\\work\\cgm -ArgumentList @($ARGS) -PassThru; Start-Sleep 6; \$p.Id" | tr -d '\r')
echo "pid $PID"
cp "$D/tools/i18n_shoot/i18n_shoot.ps1" $W/shoot.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\hantei-chan\work\cgm\shoot.ps1' -ProcId "$PID" -Out 'C:\dev\hantei-chan\work\cgm' -Actions "win 0 0 1600 900; w 1500; ${ACT:+$ACT; }s $NAME" 2>&1 | tr -d '\r'
powershell.exe -NoProfile -Command "Stop-Process -Id $PID -Force" 2>&1 | tr -d '\r'
