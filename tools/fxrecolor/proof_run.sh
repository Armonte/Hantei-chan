#!/usr/bin/env bash
# Launch MBAACC from the hard-linked proof dir (AI plays), take a shot after the battle starts, kill ONLY that dir's MBAA.exe by PID.
#   proof_run.sh <tag> <out.png> [wait-secs-after-InGame]
set -uo pipefail
DIR=/mnt/c/games/mbaacc_fxproof; WIN='C:\games\mbaacc_fxproof'
TAG="${1:?tag}"; OUT="${2:?out.png}"; WAIT="${3:-6}"
kill_ours() {
  timeout 20 powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='MBAA.exe'\" | Where-Object { \$_.ExecutablePath -like '$WIN\\*' } | ForEach-Object { Invoke-CimMethod -InputObject \$_ -MethodName Terminate | Out-Null }" >/dev/null 2>&1 || true
}
kill_ours; sleep 1
rm -f "$DIR/pchost_${TAG}.log"
( cd "$DIR" && cmd.exe /c "set PCHOST_GAME=mbaacc&& set PCHOST_LOG_TAG=${TAG}&& set PCHOST_AI_INPUT=1&& pc_inject.exe MBAA.exe pchost.dll" >/dev/null 2>&1 & )
for _ in $(seq 1 120); do grep -aq "SCENE -> InGame" "$DIR/pchost_${TAG}.log" 2>/dev/null && break; sleep 1; done
sleep "$WAIT"
bash /home/teo/dev/PovertyCaster/tools/pc_shot.sh "$DIR" "${OUT%.png}.bmp" 10 "$TAG"
python3 -c "from PIL import Image; Image.open('${OUT%.png}.bmp').save('$OUT')" && rm -f "${OUT%.png}.bmp"
kill_ours
