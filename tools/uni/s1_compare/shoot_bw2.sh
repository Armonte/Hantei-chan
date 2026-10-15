#!/bin/bash
# tools/uni/s1_compare/shoot_bw.sh [new.exe]: new-build editor shots of every case on a black and a white clear
# colour (<name>_new_k.png / <name>_new_w.png). Per pixel and channel the view is then E = P + Q * backdrop
# (normal, additive, subtractive and multiply blends are all linear in the backdrop), so sheet.py can solve
# P = E_k, Q = (E_w - E_k) / 255 and predict the game pixel over the game's own backdrop (the gamecap plate).
set -u
# Hidden window + offscreen render by default (--headless); AB_VISIBLE=1 shows the window as before.
HL=(--headless); [ -n "${AB_VISIBLE:-}" ] && HL=()
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
NEW="${1:-$D/build/gonptechan.exe}"
W=$H/work/s1bw${WORK_SUFFIX:-}; OUT="${OUTDIR:-$D/docs/audit_2026_10_14/evidence/compare/editor}"; mkdir -p "$W" "$OUT"
cp "$NEW" "$W/gonptechan.exe"
G='C:\dev\hantei-chan\gamedata'
grep -v "^#" "$(dirname "$0")/${CASEFILE:-cases.txt}" | grep -E "${CASES:-.}" | while IFS='|' read -r name txt pat fr tick zoom note; do
	name=$(echo $name); txt=$(echo $txt | tr / '\\'); pat=$(echo $pat); fr=$(echo $fr); tick=$(echo $tick); zoom=$(echo $zoom)
	[ -z "$name" ] && continue
	for bg in k w; do
		c=$([ $bg = k ] && echo "0.000000,0.000000,0.000000" || echo "1.000000,1.000000,1.000000")
		printf "[Other settings][]\nposX=-2000\nposY=10\nsizeX=1280\nsizeY=900\nColor=%s\n" "$c" > "$W/hanteichan.ini"
		args=(--open "$G\\$txt" --pattern "$pat" --frame "$fr" --zoom "$zoom" --hide-boxes ${EXTRA_ARGS:-})
		[ "$tick" != "-1" ] && args+=(--tick "$tick")
		rm -f "$W/$name.png"
		(cd "$W" && timeout 120 ./gonptechan.exe "${HL[@]}" "${args[@]}" --capture-view "$(wslpath -w "$W")\\$name.png" </dev/null >/dev/null 2>&1)
		cp "$W/$name.png" "$OUT/${name}_new_$bg.png" 2>/dev/null || echo "no shot: $bg $name"
	done
	echo "$name done"
done
