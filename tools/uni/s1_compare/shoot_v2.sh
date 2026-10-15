#!/bin/bash
# tools/uni/s1_compare/shoot_v2.sh [new.exe]: v2 full-frame editor shots at the game's camera scale (UNI2 training
# camera at rest = 1 game px per sprite px, so --zoom 1), boxes ON, for every case in cases.txt (CASES=regex).
# Writes $OUT/<case>_ours.png (default docs/audit_2026_10_14/evidence/compare/v2/ours in the main checkout).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
NEW="${1:-$D/build/gonptechan.exe}"
W=$H/work/s1v2${WORK_SUFFIX:-}; OUT="${OUT:-$H/docs/audit_2026_10_14/evidence/compare/v2/ours}"; mkdir -p "$W" "$OUT"
cp "$NEW" "$W/gonptechan.exe"
G='C:\dev\hantei-chan\gamedata'
grep -v "^#" "$(dirname "$0")/${CASEFILE:-cases.txt}" | grep -E "${CASES:-.}" | while IFS='|' read -r name txt pat fr tick zoom note; do
	name=$(echo $name); txt=$(echo $txt | tr / '\\'); pat=$(echo $pat); fr=$(echo $fr); tick=$(echo $tick)
	[ -z "$name" ] && continue
	printf "[Other settings][]\nposX=-2000\nposY=0\nsizeX=1700\nsizeY=1100\n" > "$W/hanteichan.ini"
	args=(--open "$G\\$txt" --pattern "$pat" --frame "$fr" --zoom 1 ${EXTRA_ARGS:-})
	[ "$tick" != "-1" ] && args+=(--tick "$tick")
	rm -f "$W/$name.png"
	(cd "$W" && timeout 120 ./gonptechan.exe "${args[@]}" --capture-view "$(wslpath -w "$W")\\$name.png" </dev/null >/dev/null 2>&1)
	cp "$W/$name.png" "$OUT/${name}_ours.png" 2>/dev/null || echo "no shot: $name"
	echo "$name done"
done
