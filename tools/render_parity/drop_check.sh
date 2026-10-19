#!/bin/bash
# tools/render_parity/drop_check.sh [exe] [out-dir]
#
# Drag-drop check (PLAN.md 6 Gate P0 "drag-drop opens files"). Runs the editor
# VISIBLY (AB_VISIBLE=1, through tools/ui/ab_shot.sh) with nothing open, twice:
#
#   1. --post-drop C:\games\mbaacc\data\akiha.txt
#   2. the same without --post-drop (control)
#
# --post-drop posts the WM_DROPFILES message a shell drag-drop sends to the
# app's own main window (no desktop input). SDL's window procedure turns it into
# SDL_EVENT_DROP_FILE, the platform loop hands it to MainFrame::queueDroppedFile
# and ProcessDroppedFiles opens it the way File > Open does. Pass: run 1 has one
# view whose file is akiha.txt and the control has none, and the main window's
# in-app capture of run 1 is not one flat colour. Exit 0 = pass. Outputs stay
# in <out-dir> (default /mnt/c/dev/hantei-chan/render_parity/drop_check).
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$D/build/gonptechan.exe}"
OUT="${2:-/mnt/c/dev/hantei-chan/render_parity/drop_check}"
CHAR='C:\games\mbaacc\data\akiha.txt'
mkdir -p "$OUT"
OUTW="$(wslpath -w "$OUT")"
fail=0

run() {  # run <name> <extra args...>
	local name="$1"; shift
	rm -f "$OUT/${name}_main.png" "$OUT/${name}_stats.txt"
	printf 'wait 16\ncapture %s\n' "$(wslpath -m "$OUT")/${name}_main.png" > "$OUT/${name}_script.txt"
	AB_VISIBLE=1 AB_TIMEOUT=60 AB_EXE="$EXE" AB_WORK=/mnt/c/dev/hantei-chan/work/ab_drop_check \
	AB_ARGS="--stats $OUTW\\${name}_stats.txt $*" \
		bash "$D/tools/ui/ab_shot.sh" "$OUT/${name}_script.txt" > "$OUT/${name}_run.log" 2>&1
	echo "== $name: $(head -1 "$OUT/${name}_run.log")"
	cat "$OUT/${name}_stats.txt" 2>/dev/null | sed 's/^/   /'
}

stat_of() { awk -v k="$2" '$1 == k { $1 = ""; sub(/^ /, ""); print }' "$OUT/$1_stats.txt" 2>/dev/null; }

run drop --post-drop "$CHAR"
run control

[ "$(stat_of drop views)" = 1 ] || { echo "FAIL drop: views is '$(stat_of drop views)', expected 1"; fail=1; }
case "$(stat_of drop active_file)" in
	*akiha.txt) ;;
	*) echo "FAIL drop: active_file is '$(stat_of drop active_file)', expected ...akiha.txt"; fail=1 ;;
esac
[ "$(stat_of control views)" = 0 ] || { echo "FAIL control: views is '$(stat_of control views)', expected 0"; fail=1; }
if [ -f "$OUT/drop_main.png" ]; then
	python3 -I - "$OUT/drop_main.png" <<'PY' || fail=1
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
colours = im.getcolors(1 << 24)
print(f"   {sys.argv[1].rsplit('/', 1)[-1]}: {im.size[0]}x{im.size[1]}, {len(colours)} colours")
sys.exit(0 if len(colours) > 16 else 1)
PY
else
	echo "FAIL drop: no main window capture"; fail=1
fi
[ $fail = 0 ] && echo "drop_check: PASS" || echo "drop_check: FAIL"
exit $fail
