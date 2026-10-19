#!/bin/bash
# tools/render_parity/detach_check.sh [exe] [out-dir]
#
# Detached-window check (PLAN.md 6 P0 step 5 / Gate P0 "detached windows open,
# render, take shortcuts and capture"). The byte-identical corpus cannot cover
# this: --detach only opens an OS window when the app is visible. So this runs
# the editor VISIBLY (AB_VISIBLE=1, through tools/ui/ab_shot.sh) twice on
# MBAACC akiha:
#
#   1. --detach --capture-windows --post-key 0x4F ('O', Toggle onion skin)
#   2. the same without --post-key (control)
#
# and checks that run 1 wrote the detached window's frame (win_1.png, not one
# flat colour), that the view lives in a detached host which has ImGui focus,
# and that the key pushed at the detached window toggled onion skin on its view
# (view_onion_enabled 1) while the control run leaves it off. All input is the
# app's own (--post-key pushes an SDL event); the captures are in-app GL reads.
# Exit 0 = pass. Outputs stay in <out-dir> (default
# /mnt/c/dev/hantei-chan/render_parity/detach_check).
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$D/build/gonptechan.exe}"
OUT="${2:-/mnt/c/dev/hantei-chan/render_parity/detach_check}"
CHAR='C:\games\mbaacc\data\akiha.txt'
mkdir -p "$OUT"
OUTW="$(wslpath -w "$OUT")"
fail=0

run() {  # run <name> <extra args...>
	local name="$1"; shift
	rm -f "$OUT/${name}"_win_*.png "$OUT/${name}_main.png" "$OUT/${name}_stats.txt"
	printf 'wait 16\ncapture %s\n' "$(wslpath -m "$OUT")/${name}_main.png" > "$OUT/${name}_script.txt"
	AB_VISIBLE=1 AB_TIMEOUT=60 AB_EXE="$EXE" AB_WORK=/mnt/c/dev/hantei-chan/work/ab_detach_check \
	AB_ARGS="--open $CHAR --detach --capture-windows $OUTW\\${name}_win --stats $OUTW\\${name}_stats.txt $*" \
		bash "$D/tools/ui/ab_shot.sh" "$OUT/${name}_script.txt" > "$OUT/${name}_run.log" 2>&1
	echo "== $name: $(head -1 "$OUT/${name}_run.log")"
	cat "$OUT/${name}_stats.txt" 2>/dev/null | sed 's/^/   /'
}

stat_of() { awk -v k="$2" '$1 == k { print $2 }' "$OUT/$1_stats.txt" 2>/dev/null; }

check() {  # check <name> <expected onion>
	local name="$1" onion="$2"
	if [ ! -f "$OUT/${name}_win_1.png" ]; then echo "FAIL $name: no detached window capture"; fail=1; return; fi
	python3 -I - "$OUT/${name}_win_1.png" <<'PY' || fail=1
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
colours = im.getcolors(1 << 24)
print(f"   {sys.argv[1].rsplit('/', 1)[-1]}: {im.size[0]}x{im.size[1]}, {len(colours)} colours")
sys.exit(0 if len(colours) > 16 else 1)
PY
	[ "$(stat_of "$name" view_host)" != "" ] && [ "$(stat_of "$name" view_host)" != 0 ] \
		|| { echo "FAIL $name: the view is not in a detached host"; fail=1; }
	[ "$(stat_of "$name" focused_host)" = "$(stat_of "$name" view_host)" ] \
		|| { echo "FAIL $name: the detached host does not have focus"; fail=1; }
	[ "$(stat_of "$name" view_onion_enabled)" = "$onion" ] \
		|| { echo "FAIL $name: view_onion_enabled is $(stat_of "$name" view_onion_enabled), expected $onion"; fail=1; }
}

run key --post-key 0x4F
run control
check key 1
check control 0
[ $fail = 0 ] && echo "detach_check: PASS" || echo "detach_check: FAIL"
exit $fail
