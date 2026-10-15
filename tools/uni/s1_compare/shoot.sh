#!/bin/bash
# tools/uni/s1_compare/shoot.sh [old.exe] [new.exe]: editor shots of every case in cases.txt with the old build
# (default builds/gonptechan_2026-10-04.exe = origin/update/ex-mbac 395a56b) and the new build (build/gonptechan.exe),
# (OUT / W override the output and scratch folders), then sheet.py makes the side-by-side + diff sheets in docs/audit_2026_10_14/evidence/compare/.
set -u
# Hidden window + offscreen render by default (--headless); AB_VISIBLE=1 shows the window as before.
HL=(--headless); [ -n "${AB_VISIBLE:-}" ] && HL=()
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
OLD="${1:-$H/builds/gonptechan_2026-10-04.exe}"; NEW="${2:-$D/build/gonptechan.exe}"
W=${W:-$H/work/s1}; OUT="${OUT:-$D/docs/audit_2026_10_14/evidence/compare/editor}"; mkdir -p "$W/old" "$W/new" "$OUT"
cp "$OLD" "$W/old/gonptechan.exe"; cp "$NEW" "$W/new/gonptechan.exe"
G='C:\dev\hantei-chan\gamedata'
grep -v "^#" "$(dirname "$0")/cases.txt" | grep -E "${CASES:-.}" | while IFS='|' read -r name txt pat fr tick zoom note; do
	name=$(echo $name); txt=$(echo $txt | tr / '\\'); pat=$(echo $pat); fr=$(echo $fr); tick=$(echo $tick); zoom=$(echo $zoom)
	[ -z "$name" ] && continue
	for v in old new; do
		printf "[Other settings][]\nposX=-2000\nposY=10\nsizeX=1280\nsizeY=900\n" > "$W/$v/hanteichan.ini"
		args=(--open "$G\\$txt" --pattern "$pat" --frame "$fr" --zoom "$zoom")
		[ "$tick" != "-1" ] && args+=(--tick "$tick")
		(cd "$W/$v" && timeout 120 ./gonptechan.exe "${HL[@]}" "${args[@]}" --capture-view "$(wslpath -w "$W/$v")\\$name.png" </dev/null >/dev/null 2>&1)
		cp "$W/$v/$name.png" "$OUT/${name}_$v.png" 2>/dev/null || echo "no shot: $v $name"
	done
	echo "$name done"
done
