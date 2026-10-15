#!/bin/bash
# tools/ui/headless_vs_visible.sh [exe] : for CASES (regex over tools/uni/s1_compare/cases.txt, default 3 cases) shoot
# each case with the visible --capture-view and with --headless --export-png, one --batch run of all cases too, and
# report whether the three PNGs are byte-identical per case.
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"; H=/mnt/c/dev/hantei-chan
EXE="${1:-$D/build/gonptechan.exe}"; W="${W:-$H/work/headless_cmp}"
rm -rf "$W"; mkdir -p "$W"; cp "$EXE" "$W/gonptechan.exe"
printf "[Other settings][]\nposX=10\nposY=10\nsizeX=1280\nsizeY=900\n" > "$W/hanteichan.ini"
G='C:\dev\hantei-chan\gamedata'; WW="$(wslpath -w "$W")"
CASES="${CASES:-^(pral2_chr025_p133f1|flipV_chr000_p383f1|afhk_cg_chr020_p435_t27) }"
: > "$W/batch.txt"
grep -v "^#" "$D/tools/uni/s1_compare/cases.txt" | grep -E "$CASES" > "$W/cases.sel"
while IFS='|' read -r name txt pat fr tick zoom note; do
	name=$(echo $name); txt=$(echo $txt | tr / '\\'); pat=$(echo $pat); fr=$(echo $fr); tick=$(echo $tick); zoom=$(echo $zoom)
	args=(--open "$G\\$txt" --pattern "$pat" --frame "$fr" --zoom "$zoom"); [ "$tick" != "-1" ] && args+=(--tick "$tick")
	t0=$(date +%s.%N)
	(cd "$W" && timeout 120 ./gonptechan.exe "${args[@]}" --capture-view "$WW\\${name}_visible.png" </dev/null >/dev/null 2>&1)
	t1=$(date +%s.%N)
	(cd "$W" && timeout 120 ./gonptechan.exe --headless "${args[@]}" --export-png "$WW\\${name}_headless.png" </dev/null >/dev/null 2>&1); rc=$?
	t2=$(date +%s.%N)
	echo "$name visible $(echo "$t1-$t0" | bc)s headless $(echo "$t2-$t1" | bc)s exit $rc"
	line="open $G\\$txt; pattern $pat; frame $fr; zoom $zoom"; [ "$tick" != "-1" ] && line="$line; tick $tick"
	echo "$line; export $WW\\${name}_batch.png" >> "$W/batch.txt"
done < "$W/cases.sel"
t0=$(date +%s.%N); (cd "$W" && timeout 300 ./gonptechan.exe --headless --batch "$WW\\batch.txt" </dev/null >/dev/null 2>&1); rc=$?; t1=$(date +%s.%N)
echo "batch exit $rc $(echo "$t1-$t0" | bc)s"
while IFS='|' read -r name rest; do
	name=$(echo $name)
	v=$(sha256sum < "$W/${name}_visible.png" 2>/dev/null | cut -c1-16); h=$(sha256sum < "$W/${name}_headless.png" 2>/dev/null | cut -c1-16); b=$(sha256sum < "$W/${name}_batch.png" 2>/dev/null | cut -c1-16)
	[ -n "$v" ] && [ "$v" = "$h" ] && [ "$v" = "$b" ] && r=IDENTICAL || r=DIFF
	echo "$name visible=$v headless=$h batch=$b $r"
done < "$W/cases.sel"
