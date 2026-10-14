#!/bin/bash
# tools/cg/ingame_shot.sh <out.png> <setup args...>
# MBAACC in-game proof with no desktop input: game_link_cli launch (pc_inject + pchost, Authoring Mode boots straight into the
# requested match, e.g. "1v1 3/c/0 0/c/0 scene training"), wait, capture ONLY that game window with PrintWindow
# (tools/cg/pw_capture.ps1), then kill exactly the PID that was launched. One game at a time: refuses if MBAA.exe already runs.
# Env: MBAA_DIR (default C:\games\mbaacc), SHOT_WAIT seconds after Ready (default 6).
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$1"; shift
DIR="${MBAA_DIR:-C:\\games\\mbaacc}"; WAIT="${SHOT_WAIT:-6}"
if tasklist.exe 2>/dev/null | grep -qi -E "^(MBAA|uni2|MBTL)"; then echo "a game is already running: refusing (one instance at a time)"; exit 3; fi
LOG=$(mktemp)
"$ROOT/build/game_link_cli.exe" launch "$DIR" "$@" > "$LOG" 2>&1
PID=$(grep -oE "game pid [0-9]+" "$LOG" | grep -oE "[0-9]+" | head -1)
tail -4 "$LOG"
[ -z "$PID" ] && { echo "no game pid"; cat "$LOG"; rm -f "$LOG"; exit 1; }
sleep "$WAIT"
cp "$HERE/pw_capture.ps1" /mnt/c/dev/hantei-chan/work/pw_capture.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\hantei-chan\work\pw_capture.ps1' -ProcId "$PID" -Out "$(wslpath -w "$OUT")" 2>&1 | tr -d '\r'
powershell.exe -NoProfile -Command "Stop-Process -Id $PID -Force" 2>&1 | tr -d '\r'
rm -f "$LOG"
