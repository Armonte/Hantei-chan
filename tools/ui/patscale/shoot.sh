#!/bin/bash
# tools/ui/patscale/shoot.sh [old.exe] [new.exe] [outdir]: G_CHAOS Arc Drive (MBAC->CC .pat) at a fixed zoom with the
# pre-S1 build (builds/gonptechan_2026-10-04.exe) and this build, via --capture-view (the app's own GL frame, no desktop).
# Cases: the frame picked in the editor, and timeline ticks inside frames 9/10 (AFHK 1 on every frame of p100).
set -u
# Hidden window + offscreen render by default (--headless); AB_VISIBLE=1 shows the window as before.
HL=(--headless); [ -n "${AB_VISIBLE:-}" ] && HL=()
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
OLD="${1:-$H/builds/gonptechan_2026-10-04.exe}"; NEW="${2:-$D/build/gonptechan.exe}"
OUT="${3:-$D/docs/audit_2026_10_14/evidence/patscale}"; W=$H/work/patscale; mkdir -p "$W/old" "$W/new" "$OUT"
cp "$OLD" "$W/old/gonptechan.exe"; cp "$NEW" "$W/new/gonptechan.exe"
TXT='C:\dev\hantei-chan\gamedata\mbac_to_cc\roster\g_chaos\g_chaos.txt'
# name | pattern | frame | tick (-1 = frame only) | zoom
while IFS='|' read -r name pat fr tick zoom; do
	for v in old new; do
		printf "[Other settings][]\nposX=10\nposY=10\nsizeX=1280\nsizeY=900\n" > "$W/$v/hanteichan.ini"
		args=(--open "$TXT" --pattern "$pat" --frame "$fr" --zoom "$zoom")
		[ "$tick" != "-1" ] && args+=(--tick "$tick")
		(cd "$W/$v" && timeout 120 ./gonptechan.exe "${HL[@]}" "${args[@]}" --capture-view "$(wslpath -w "$W/$v")\\$name.png" </dev/null >/dev/null 2>&1)
		cp "$W/$v/$name.png" "$OUT/${name}_$v.png" 2>/dev/null || echo "no shot: $v $name"
	done
	echo "$name done"
done <<'CASES'
gchaos_p100_f10_z1|100|10|-1|1
gchaos_p100_f10_z3|100|10|-1|3
gchaos_p100_t29_z1|100|10|29|1
gchaos_p100_t30_z1|100|10|30|1
gchaos_p100_t28_z1|100|9|28|1
gchaos_p0_f0_z1|0|0|-1|1
CASES
