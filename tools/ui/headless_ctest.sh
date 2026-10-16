#!/bin/bash
# tools/ui/headless_ctest.sh <gonptechan.exe> <character file> <out dir> : ctest `headless_export`.
# Renders one view shot with --headless (hidden window, FBO readback) twice and once more through --batch, then checks
# the exit codes, the PNG size (IHDR = --size) and that all three files are byte-identical (stable checksum).
set -u
EXE="$1"; CHAR="$2"; OUT="$3"
rm -rf "$OUT"; mkdir -p "$OUT"
cp "$EXE" "$OUT/gonptechan.exe"
W() { if command -v wslpath >/dev/null 2>&1; then wslpath -w "$1"; elif command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi; }
CW="$(W "$(cd "$(dirname "$CHAR")" && pwd)/$(basename "$CHAR")")"
run() { (cd "$OUT" && timeout 60 ./gonptechan.exe --headless "$@" </dev/null); }
fail() { echo "FAIL: $*"; exit 1; }
for k in 1 2; do
	out=$(run --open "$CW" --pattern 0 --frame 0 --zoom 2 --size 320x240 --bg 40,80,120 --export-png "$(W "$OUT")\\shot$k.png" 2>&1)
	rc=$?
	[ $rc -eq 0 ] || fail "headless run $k exit $rc"
	[ -z "$out" ] || fail "headless run $k printed: $out"
	[ -s "$OUT/shot$k.png" ] || fail "no shot$k.png"
done
printf 'open %s; pattern 0; frame 0; zoom 2; size 320x240; bg 40,80,120; export %s\\batch_a.png\n' "$CW" "$(W "$OUT")" > "$OUT/batch.txt"
printf 'pattern 0; frame 0; export %s\\batch_b.png\n' "$(W "$OUT")" >> "$OUT/batch.txt"
run --batch "$(W "$OUT/batch.txt")" >/dev/null 2>&1; rc=$?
[ $rc -eq 0 ] || fail "batch exit $rc"
dims=$(od -An -tu1 -j16 -N8 "$OUT/shot1.png" | awk '{printf "%dx%d", $1*16777216+$2*65536+$3*256+$4, $5*16777216+$6*65536+$7*256+$8}')
[ "$dims" = "320x240" ] || fail "shot size $dims, want 320x240"
s1=$(sha256sum < "$OUT/shot1.png"); s2=$(sha256sum < "$OUT/shot2.png"); sa=$(sha256sum < "$OUT/batch_a.png"); sb=$(sha256sum < "$OUT/batch_b.png")
[ "$s1" = "$s2" ] || fail "checksum differs between runs"
[ "$s1" = "$sa" ] || fail "batch shot differs from the single shot"
[ "$sa" = "$sb" ] || fail "repeated batch shot differs"
# missing batch file -> exit 2
run --batch "$(W "$OUT")\\missing.txt" >/dev/null 2>&1; rc=$?
[ $rc -eq 2 ] || fail "missing batch exit $rc, want 2"
echo "OK $dims ${s1%% *}"
