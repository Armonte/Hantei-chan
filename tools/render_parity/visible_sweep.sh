#!/bin/bash
# tools/render_parity/visible_sweep.sh [exe] [out-dir] [set...]
#
# Gate check (PLAN.md 6 P0 "run every tools/ui/shots*/ script visibly once with
# AB_VISIBLE=1 and check that it finishes", P2 "all ui-script shots complete").
# Runs each script of the given sets (default: shots shots_cgm shots_layout s8,
# the corpus area D sets) through tools/ui/ab_shot.sh with AB_VISIBLE=1, one
# process per script, one after another. All input is the in-app script runner;
# every capture is the app's own frame read. Nothing on the desktop is touched.
#
# A script completes when the exe exits 0 (not killed by the timeout) and every
# `capture <png>` line of the script left its PNG. The script runner's own log
# (`script.txt.log`: failed lines, savechar results) is kept per script and its
# "failed" lines are counted, but they are reported, not judged: some scripts
# exercise refusals on purpose.
#
# The scratch fixtures are rebuilt first (prepare_fixtures.py), so the
# CG-manager scripts start from the same bytes as in the corpus, and once more
# at the end, so the corpus never inherits this sweep's saves. The S2 PAT
# fixture is copied into the sweep's own work folder (ab_shot.sh points
# work/ab/ there); a capture outside work/ is moved to work/ab/other/. Exit 0 =
# every script completed. Results in <out-dir>/sweep.tsv (set, script, exit,
# captures, written, failed_lines).
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$D/build/gonptechan.exe}"
OUT="${2:-/mnt/c/dev/hantei-chan/render_parity/visible_sweep}"
shift 2 2>/dev/null || shift $#
SETS=("$@"); [ ${#SETS[@]} = 0 ] && SETS=(shots shots_cgm shots_layout s8)
WORK=/mnt/c/dev/hantei-chan/work/ab_visible_sweep
mkdir -p "$OUT" "$WORK/s2" "$WORK/shots"
python3 "$D/tools/render_parity/prepare_fixtures.py" || { echo "visible_sweep: fixture reset failed"; exit 2; }
cp /mnt/c/dev/hantei-chan/gamedata/uni2/data/chr000/chr000.pat "$WORK/s2/chr000.pat"
WW="$(wslpath -m "$WORK")"
printf 'set\tscript\texit\tcaptures\twritten\tfailed_lines\n' > "$OUT/sweep.tsv"
fail=0
for set in "${SETS[@]}"; do
	for f in "$D"/tools/ui/"$set"/*.txt; do
		[ -e "$f" ] || continue
		name="$(basename "$f" .txt)"
		log="$OUT/${set}__$name.log"
		# A capture outside C:/dev/hantei-chan/work/ (the s8 scripts name a
		# removed worktree and another session's scratch folder) goes to
		# work/ab/other/ instead: `capture` creates its folder, and the sweep
		# must not recreate stale trees. ab_shot.sh then points work/ab/ here.
		script="$OUT/scripts/${set}__$name.txt"; mkdir -p "$OUT/scripts"
		sed -E '/^[[:space:]]*capture[[:space:]]/{/C:\/dev\/hantei-chan\/work\//!s#^([[:space:]]*capture[[:space:]]+).*/#\1C:/dev/hantei-chan/work/ab/other/#}' "$f" > "$script"
		# the PNGs this script asks for, after ab_shot.sh's work/ab/ rewrite
		mapfile -t caps < <(sed -n 's#^[[:space:]]*capture[[:space:]]\+##p' "$script" | sed "s#C:/dev/hantei-chan/work/ab/#$WW/#g" | tr -d '\r')
		for c in "${caps[@]}"; do rm -f "$(wslpath -u "$c")"; done
		AB_VISIBLE=1 AB_TIMEOUT="${AB_TIMEOUT:-400}" AB_EXE="$EXE" AB_WORK="$WORK" \
			bash "$D/tools/ui/ab_shot.sh" "$script" > "$log" 2>&1
		rc="$(sed -n 's/^exit //p' "$log" | head -1)"
		written=0
		for c in "${caps[@]}"; do [ -s "$(wslpath -u "$c")" ] && written=$((written + 1)); done
		failed="$(grep -c "failed\|FAILED" "$log")"
		printf '%s\t%s\t%s\t%d\t%d\t%s\n' "$set" "$name" "$rc" "${#caps[@]}" "$written" "$failed" >> "$OUT/sweep.tsv"
		state=ok
		if [ "$rc" != 0 ] || [ "$written" != "${#caps[@]}" ]; then state=FAIL; fail=1; fi
		echo "== $set/$name: exit $rc, captures $written/${#caps[@]}, failed lines $failed: $state"
	done
done
python3 "$D/tools/render_parity/prepare_fixtures.py" > /dev/null || echo "visible_sweep: final fixture reset failed"
[ $fail = 0 ] && echo "visible_sweep: PASS" || echo "visible_sweep: FAIL"
exit $fail
