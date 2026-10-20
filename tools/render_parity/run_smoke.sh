#!/bin/bash
# run_smoke.sh <repo-root> <gonptechan.exe>: the ctest "render_parity" entry
# (PLAN.md 4.4: a small smoke subset against golden_smoke/ for quick local
# checks; the full corpus is the phase gate, run by hand).
#
# Exits 77 (ctest SKIP_RETURN_CODE) when the smoke golden or the game data is
# absent, so CI without either stays green.
set -u
ROOT="${1:?usage: run_smoke.sh <repo-root> <exe>}"
EXE="${2:?usage: run_smoke.sh <repo-root> <exe>}"
GOLDEN=/mnt/c/dev/hantei-chan/render_parity/golden_smoke
RENDER_ROOT=/mnt/c/dev/hantei-chan/render_parity
if [ ! -f "$GOLDEN/manifest.tsv" ]; then
	echo "render_parity: no smoke golden at $GOLDEN (run make_smoke.py) -> skip"
	exit 77
fi
if [ ! -d /mnt/c/dev/hantei-chan/gamedata/uni2/data/chr000 ]; then
	echo "render_parity: no game data -> skip"
	exit 77
fi
EXE_DIR="$(dirname "$EXE")"
RUN="$RENDER_ROOT/runs/smoke_$(date +%Y%m%d_%H%M%S)"
# The smoke subset is corpus/smoke.batch, so every case is one shot of that file.
bash "$ROOT/tools/render_parity/run_corpus.sh" "$EXE_DIR" "$RUN" --force --only smoke >/dev/null 2>&1
python3 "$ROOT/tools/render_parity/diff.py" "$GOLDEN" "$RUN"
rc=$?
# A passing run has nothing left to look at: drop it (C: is nearly full).
if [ $rc -eq 0 ]; then rm -rf "$RUN"; else echo "render_parity: see $RUN" >&2; fi
exit $rc
