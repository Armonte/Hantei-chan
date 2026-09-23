#!/bin/bash
# Stage regression: byte-identical save round trip of every stage file, a
# bounded runtime simulation of each, seed determinism, and the edit test.
#
#   tools/bg_regress.sh [ticks]
#
# Stage folders (override with env vars; read-only, outputs go to a temp dir):
#   BG_MBAACC_DIR  default /mnt/c/games/mbaacc/bg        (56 .dat + BgList.ini/Info.txt)
#   BG_MBAC_DIR    default /mnt/c/games/MB/AC/dump/05    (68 .DAT incl. _S variants)
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BGT="$ROOT/build/bg_test.exe"
TICKS="${1:-3600}"
MBAACC_DIR="${BG_MBAACC_DIR:-/mnt/c/games/mbaacc/bg}"
MBAC_DIR="${BG_MBAC_DIR:-/mnt/c/games/MB/AC/dump/05}"
[ -x "$BGT" ] || { echo "build first: $BGT missing"; exit 2; }
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
# bg_test.exe is a Windows binary; hand it Windows paths when running under WSL.
wp() { if command -v wslpath >/dev/null; then wslpath -w "$1"; else echo "$1"; fi; }

files=()
for d in "$MBAACC_DIR" "$MBAC_DIR"; do
	[ -d "$d" ] || { echo "missing stage dir: $d"; continue; }
	while IFS= read -r f; do files+=("$f"); done < <(find "$d" -maxdepth 1 -iname 'bg*.dat' | sort)
done

rt_ok=0; rt_bad=0; sim_ok=0; sim_bad=0
for f in "${files[@]}"; do
	out="$TMP/rt.dat"
	if "$BGT" "$(wp "$f")" "$(wp "$out")" >"$TMP/log" 2>&1 && grep -q "ROUNDTRIP OK" "$TMP/log"; then
		rt_ok=$((rt_ok+1))
	else
		rt_bad=$((rt_bad+1)); echo "ROUNDTRIP FAIL $f"; tail -2 "$TMP/log"
	fi
	if "$BGT" "$(wp "$f")" --sim "$TICKS" --seed 1234 >"$TMP/sim" 2>&1; then
		sim_ok=$((sim_ok+1))
		echo "  $(basename "$f"): $(grep '^SIM' "$TMP/sim")"
	else
		sim_bad=$((sim_bad+1)); echo "SIM FAIL (unbounded or crash) $f"; tail -3 "$TMP/sim"
	fi
done

# Same seed -> same state; different seed -> (for stages with random events) different state.
det_bad=0
for s in bg16 bg41 bg55; do
	f="$(find "$MBAACC_DIR" -maxdepth 1 -iname "$s.dat" | head -1)"
	[ -n "$f" ] || continue
	h1=$("$BGT" "$(wp "$f")" --sim 1200 --seed 77 2>&1 | grep '^SIM' | sed 's/.*hash=//')
	h2=$("$BGT" "$(wp "$f")" --sim 1200 --seed 77 2>&1 | grep '^SIM' | sed 's/.*hash=//')
	h3=$("$BGT" "$(wp "$f")" --sim 1200 --seed 78 2>&1 | grep '^SIM' | sed 's/.*hash=//')
	if [ -z "$h1" ] || [ "$h1" != "$h2" ] || [ "$h1" = "$h3" ]; then
		det_bad=$((det_bad+1)); echo "DETERMINISM FAIL $s: $h1 $h2 $h3"
	fi
done

# Editing: change fields + records, save, reload, verify.
edit_bad=0
for s in bg01 bg16; do
	f="$(find "$MBAACC_DIR" -maxdepth 1 -iname "$s.dat" | head -1)"
	[ -n "$f" ] || continue
	"$BGT" "$(wp "$f")" --edit-test "$(wp "$TMP/edit.dat")" 2>&1 | grep -q "EDIT-TEST OK" || { edit_bad=$((edit_bad+1)); echo "EDIT FAIL $s"; }
done

# RNG port matches Knuth's subtractive generator (= .NET Random(0)).
rng=$("$BGT" --rng 0 3 | tr -d '\r' | tr '\n' ' ')
[ "$rng" = "1559595546 1755192844 1649316166 " ] || { echo "RNG FAIL: $rng"; det_bad=$((det_bad+1)); }

echo "roundtrip: $rt_ok ok, $rt_bad failed | sim($TICKS ticks): $sim_ok ok, $sim_bad failed | determinism/rng failures: $det_bad | edit failures: $edit_bad"
[ $rt_bad -eq 0 ] && [ $sim_bad -eq 0 ] && [ $det_bad -eq 0 ] && [ $edit_bad -eq 0 ]
