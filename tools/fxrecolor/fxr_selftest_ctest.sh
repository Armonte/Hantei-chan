#!/bin/bash
# tools/fxrecolor/fxr_selftest_ctest.sh <gonptechan.exe> <character .HA6> <.cg or -> <out dir> : ctest `fxr_selftest` / `fxr_selftest_akiha`.
# Runs the Effect recolor tab self-test (--fxr-selftest, fxr_panel.cpp FxPanel::selfTest) headless and checks its report.
# With a CG-less fixture the test covers the ruleset paths (DGV parse, save/load round trip, new rule, assign, accent-follow variant,
# shader maths); with a real bank it also ticks real effect patterns. Exit 77 (ctest skip) when the character file is missing.
set -u
EXE="$1"; CHAR="$2"; CG="$3"; OUT="$4"
[ -f "$CHAR" ] || { echo "SKIP: no $CHAR"; exit 77; }
[ "$CG" = "-" ] || [ -f "$CG" ] || { echo "SKIP: no $CG"; exit 77; }
rm -rf "$OUT"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
cp "$EXE" "$OUT/gonptechan.exe"
W() { if command -v wslpath >/dev/null 2>&1; then wslpath -w "$1"; elif command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi; }
ABS() { echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }
CHARW="$(W "$(ABS "$CHAR")")"
CGARG=(); [ "$CG" = "-" ] || CGARG=(--cg "$(W "$(ABS "$CG")")")
(cd "$OUT" && timeout 90 ./gonptechan.exe --headless --open "$CHARW" "${CGARG[@]}" --fxr-selftest "$(W "$OUT")\\st" --capture "$(W "$OUT")\\fxr_selftest.png" </dev/null >/dev/null 2>&1)
rc=$?
R="$OUT/st/fxr_selftest.txt"
[ -f "$R" ] || { echo "FAIL: no report (exit $rc)"; exit 1; }
cat "$R"
grep -q "^FAIL" "$R" && { echo "FAIL: self-test checks failed"; exit 1; }
grep -q "SECTION fxr-selftest: pass" "$R" || { echo "FAIL: no pass line"; exit 1; }
[ $rc -eq 0 ] || { echo "FAIL: exit $rc"; exit 1; }
echo OK
