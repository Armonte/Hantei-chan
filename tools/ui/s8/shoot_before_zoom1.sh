#!/bin/bash
# tools/ui/s8/shoot_before_zoom1.sh [old.exe] [outdir]: retake the S8 before_* shots at view zoom 1 (the after/control
# shots use `zoomview 1`). The pre-S1 exe (builds/gonptechan_2026-10-04.exe) has neither --ui-script nor zoomview, so this
# uses its startup flags: --open/--pattern/--frame/--zoom 1 and --capture (the app's own GL frame of the whole window,
# same layout as the after_* `capture` shots). No desktop input. Old ActCadenza roster (mbac_to_cc/roster).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
OLD="${1:-$H/builds/gonptechan_2026-10-04.exe}"; OUT="${2:-$D/docs/audit_2026_10_14/evidence/s8}"
W=$H/work/s8_before_rezoom; mkdir -p "$W" "$OUT"; cp "$OLD" "$W/gonptechan.exe"
# name | character folder | pattern | frame
while IFS='|' read -r name chr pat fr; do
	printf "[Other settings][]\nposX=10\nposY=10\nsizeX=1900\nsizeY=1000\n" > "$W/hanteichan.ini"
	(cd "$W" && timeout 120 ./gonptechan.exe --open "C:\\dev\\hantei-chan\\gamedata\\mbac_to_cc\\roster\\$chr\\$chr.txt" \
		--pattern "$pat" --frame "$fr" --zoom 1 --capture "$(wslpath -w "$W")\\$name.png" </dev/null >/dev/null 2>&1)
	python3 "$D/tools/ui/shrink_shot.py" "$W/$name.png" "$OUT/$name.png" || echo "no shot: $name"
done <<'CASES'
before_gchaos_p100_f10_arcdrive|g_chaos|100|10
before_gchaos_p0_f0_boxes|g_chaos|0|0
before_gchaos_p64_f2_parts_ef|g_chaos|64|2
before_sion_p65_f0_shield|sion|65|0
CASES
python3 "$D/tools/ui/s8/sheet.py" "$OUT"
