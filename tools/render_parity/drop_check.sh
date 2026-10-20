#!/bin/bash
# tools/render_parity/drop_check.sh [exe] [out-dir]
#
# Drag-drop check (PLAN.md 6 Gate P0 "drag-drop opens files"). Runs the editor
# VISIBLY (AB_VISIBLE=1, through tools/ui/ab_shot.sh) with nothing open, three
# times, never with desktop input:
#
#   1. ole:     --post-drop-ole C:\games\mbaacc\data\akiha.txt --post-drop-at 300,200
#   2. wmdrop:  --post-drop     C:\games\mbaacc\data\akiha.txt (client centre)
#   3. control: neither
#
# --post-drop-ole is the route a file dragged from Explorer takes: SDL
# initialises OLE and registers an IDropTarget for the window, and the app calls
# that target's DragEnter / DragOver / Drop in process, at the given client
# point, with the shell's own data object for the file (CF_HDROP).
# --post-drop posts WM_DROPFILES to the main window, the route SDL keeps for a
# window whose OleInitialize failed (DragAcceptFiles). Either way SDL sends
# SDL_EVENT_DROP_POSITION + SDL_EVENT_DROP_FILE, the platform loop hands the
# point to MainFrame::setDropPoint and the file to queueDroppedFile, and
# ProcessDroppedFiles opens it the way File > Open does.
# Pass: each drop run has one view whose file is akiha.txt, the drop point
# (--stats drop_client) is the one asked for (300,200; the client centre for
# wmdrop), drop_screen is that point plus the client area's desktop origin, the
# OLE run's Drop returned S_OK with a CF_HDROP data object, the control has no
# view, and each drop run's in-app main-window capture is not one flat colour.
# Exit 0 = pass. Outputs stay in <out-dir> (default
# /mnt/c/dev/hantei-chan/render_parity/drop_check).
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

OLE_AT="300 200"
run ole --post-drop-ole "$CHAR" --post-drop-at 300,200
run wmdrop --post-drop "$CHAR"
run control

check_drop() {  # check_drop <name> <expected client point "x y">
	local name="$1" want="$2"
	[ "$(stat_of "$name" views)" = 1 ] || { echo "FAIL $name: views is '$(stat_of "$name" views)', expected 1"; fail=1; }
	case "$(stat_of "$name" active_file)" in
		*akiha.txt) ;;
		*) echo "FAIL $name: active_file is '$(stat_of "$name" active_file)', expected ...akiha.txt"; fail=1 ;;
	esac
	local dc ds org
	dc="$(stat_of "$name" drop_client)"; ds="$(stat_of "$name" drop_screen)"; org="$(stat_of "$name" client_origin)"
	[ "$dc" = "$want" ] || { echo "FAIL $name: drop_client is '$dc', expected '$want'"; fail=1; }
	if [ -n "$ds" ] && [ -n "$org" ]; then
		local sx sy ox oy cx cy
		read -r sx sy <<< "$ds"; read -r ox oy <<< "$org"; read -r cx cy <<< "$want"
		[ "$((sx - ox)) $((sy - oy))" = "$cx $cy" ] || { echo "FAIL $name: drop_screen $ds - client_origin $org != $want"; fail=1; }
	else
		echo "FAIL $name: no drop_screen / client_origin in the stats"; fail=1
	fi
	if [ -f "$OUT/${name}_main.png" ]; then
		python3 -I - "$OUT/${name}_main.png" <<'PY' || fail=1
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
colours = im.getcolors(1 << 24)
print(f"   {sys.argv[1].rsplit('/', 1)[-1]}: {im.size[0]}x{im.size[1]}, {len(colours)} colours")
sys.exit(0 if len(colours) > 16 else 1)
PY
	else
		echo "FAIL $name: no main window capture"; fail=1
	fi
}

check_drop ole "$OLE_AT"
case "$(stat_of ole ole_drop)" in
	"enter=00000000 over=00000000 drop=00000000 "*"hdrop=1"*) ;;
	*) echo "FAIL ole: ole_drop is '$(stat_of ole ole_drop)', expected S_OK x3 and a CF_HDROP data object"; fail=1 ;;
esac
# The wmdrop point is the client centre: half the main capture's size.
if [ -f "$OUT/wmdrop_main.png" ]; then
	CENTRE="$(python3 -I -c 'import sys; from PIL import Image; w, h = Image.open(sys.argv[1]).size; print(w // 2, h // 2)' "$OUT/wmdrop_main.png")"
	check_drop wmdrop "$CENTRE"
else
	echo "FAIL wmdrop: no main window capture"; fail=1
fi
[ "$(stat_of control views)" = 0 ] || { echo "FAIL control: views is '$(stat_of control views)', expected 0"; fail=1; }
[ $fail = 0 ] && echo "drop_check: PASS" || echo "drop_check: FAIL"
exit $fail
