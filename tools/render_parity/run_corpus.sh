#!/bin/bash
# run_corpus.sh <exe-dir> <out-dir> [--renderer gl|d3d9] [--only <glob>] [--round N] [--force]
#
# Renders the committed corpus (tools/render_parity/corpus/) with one process per
# file, headless, and writes <out-dir>/manifest.tsv:
#     case  png  sha256  w  h  exit  round
# One row per captured PNG (= one corpus case). Each .batch is run with
# --headless --batch; each corpus/ui/*.txt with --headless --ui-script. The
# scratch folder on C: holds the exe, res/ and the rewritten scripts, the way
# tools/ui/ab_shot.sh does it. Runs resume: a file whose .status says done is
# skipped unless --force.
#
# The corpus names its output as OUT/<case>.png; OUT/ is rewritten to <out-dir>.
set -u
EXE_DIR="${1:?usage: run_corpus.sh <exe-dir> <out-dir> [--renderer gl|d3d9] [--only glob] [--round N] [--force]}"
OUT="${2:?usage: run_corpus.sh <exe-dir> <out-dir> [--renderer gl|d3d9] [--only glob] [--round N]}"
shift 2
RENDERER=gl; ONLY=""; ROUND=1; FORCE=0; TIMEOUT="${RP_TIMEOUT:-300}"
RESET=1
LIMIT=0
while [ $# -gt 0 ]; do
	case "$1" in
		--renderer) RENDERER="$2"; shift 2 ;;
		--only) ONLY="$2"; shift 2 ;;
		--round) ROUND="$2"; shift 2 ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		--force) FORCE=1; shift ;;
		--no-reset) RESET=0; shift ;;
		--limit) LIMIT="$2"; shift 2 ;;
		*) echo "unknown option $1" >&2; exit 2 ;;
	esac
done

D="$(cd "$(dirname "$0")/../.." && pwd)"
CORPUS="$D/tools/render_parity/corpus"

# Rebuild the mutable scratch fixtures the UI scripts read and write back, so a
# run never inherits the previous run's edits (PLAN.md 4.3). --no-reset skips it.
if [ "$RESET" = 1 ]; then
	python3 "$D/tools/render_parity/prepare_fixtures.py" || echo "run_corpus: fixture reset failed" >&2
fi
mkdir -p "$OUT"
SHOTS="$OUT/shots"; mkdir -p "$SHOTS"
W="/mnt/c/dev/hantei-chan/work/render_parity/run_$(basename "$OUT")"
mkdir -p "$W"
cp "$EXE_DIR/gonptechan.exe" "$W/gonptechan.exe"
cp -r "$EXE_DIR/res" "$W/" 2>/dev/null || true
mkdir -p "$W/res/squirrel" && cp "$D"/docs/squirrel/api/*_api.json "$W/res/squirrel/" 2>/dev/null || true

WSL_OUT="$(wslpath -m "$SHOTS")"
win() { wslpath -w "$1"; }

# --deterministic pins every wall-clock-driven effect (PLAN.md 4.3): the stage
# background animation steps exactly one 1/60 s tick per frame and ImGui's modal
# dim fade uses a fixed frame step, so the same shot is byte-identical on a fast
# and on a loaded machine.
RUN_ARGS=(--headless --deterministic)
[ "$RENDERER" != gl ] && RUN_ARGS+=(--renderer "$RENDERER")

fail=0
run_file() {   # <corpus-file> <kind>
	local src="$1" kind="$2"
	local base; base="$(basename "$src")"
	local case_base="${base%.*}"
	if [ -n "$ONLY" ]; then
		# comma-separated shell patterns, matched one at a time (a case pattern
		# list built with command substitution does not word-split reliably).
		local hit=0
		local OLDIFS="$IFS"; IFS=','
		for pat in $ONLY; do
			case "$case_base" in $pat) hit=1 ;; esac
		done
		IFS="$OLDIFS"
		[ "$hit" = 1 ] || return 0
	fi
	local st="$OUT/$case_base.status"
	if [ "$FORCE" = 0 ] && [ -f "$st" ] && grep -q '^done ' "$st"; then return 0; fi
	# rewrite OUT/ to this run's shot folder (never touches the committed corpus)
	sed "s#OUT/#$WSL_OUT/#g" "$src" > "$W/$base"
	# A "#mode: plain" file (areas B and E) carries its own exe arguments on a
	# "#args:" line instead of a script body: those options are startup-only.
	local arg=() extra=() line mode=script
	while IFS= read -r line; do
		case "$line" in
			'#mode:'*) mode="${line#*:}"; mode="${mode// /}" ;;
			'#args:'*) read -r -a extra <<< "${line#*:}" ;;
		esac
	done < "$W/$base"
	if [ "$mode" = plain ]; then
		# No script argument at all: the file only supplies startup arguments
		# such as --export-png / --onion / --compare.
		( cd "$W" && timeout "$TIMEOUT" ./gonptechan.exe "${RUN_ARGS[@]}" "${extra[@]}" </dev/null >"$W/$base.log" 2>&1 )
	else
		[ "$kind" = batch ] && arg=(--batch) || arg=(--ui-script)
		( cd "$W" && timeout "$TIMEOUT" ./gonptechan.exe "${RUN_ARGS[@]}" "${extra[@]}" "${arg[@]}" "$(win "$W/$base")" </dev/null >"$W/$base.log" 2>&1 )
	fi
	local rc=$?
	printf 'done %d\n' "$rc" > "$st"
	echo "== $case_base exit $rc"
	[ "$rc" -ne 0 ] && fail=1
	return 0
}

# --limit N: stop after N corpus files (the smoke subset: the same first files in
# the same order every time, so its manifest is a stable prefix of the corpus).
ran=0
for f in "$CORPUS"/*.batch; do
	[ -e "$f" ] || continue
	[ "$LIMIT" -gt 0 ] && [ "$ran" -ge "$LIMIT" ] && break
	run_file "$f" batch
	ran=$((ran + 1))
done
for f in "$CORPUS"/ui/*.txt; do
	[ -e "$f" ] || continue
	[ "$LIMIT" -gt 0 ] && [ "$ran" -ge "$LIMIT" ] && break
	run_file "$f" ui
	ran=$((ran + 1))
done

# manifest: every PNG under the shot folder, one case per file
python3 - "$OUT" "$ROUND" <<'PY'
import hashlib, os, struct, sys
out, rnd = sys.argv[1], sys.argv[2]
shots = os.path.join(out, "shots")
rows = []
for dirpath, _, names in os.walk(shots):
    for n in sorted(names):
        if not n.endswith(".png"):
            continue
        p = os.path.join(dirpath, n)
        rel = os.path.relpath(p, out).replace(os.sep, "/")
        case = rel[len("shots/"):-4] if rel.startswith("shots/") else rel[:-4]
        h = hashlib.sha256(open(p, "rb").read()).hexdigest()
        with open(p, "rb") as f:
            head = f.read(24)
        w, ht = struct.unpack(">II", head[16:24])
        rows.append((case, rel, h, w, ht, 0, rnd))
rows.sort()
path = os.path.join(out, "manifest.tsv")
with open(path, "w", encoding="utf-8") as f:
    f.write("case\tpng\tsha256\tw\th\texit\tround\n")
    for r in rows:
        f.write("%s\t%s\t%s\t%d\t%d\t%d\t%s\n" % r)
print("manifest: %d cases -> %s" % (len(rows), path))
PY

exit $fail
