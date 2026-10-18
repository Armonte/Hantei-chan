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
while [ $# -gt 0 ]; do
	case "$1" in
		--renderer) RENDERER="$2"; shift 2 ;;
		--only) ONLY="$2"; shift 2 ;;
		--round) ROUND="$2"; shift 2 ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		--force) FORCE=1; shift ;;
		*) echo "unknown option $1" >&2; exit 2 ;;
	esac
done
D="$(cd "$(dirname "$0")/../.." && pwd)"
CORPUS="$D/tools/render_parity/corpus"
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
	[ -n "$ONLY" ] && case "$case_base" in $(echo "$ONLY" | tr ',' '|')) ;; *) return 0 ;; esac
	local st="$OUT/$case_base.status"
	if [ "$FORCE" = 0 ] && [ -f "$st" ] && grep -q '^done ' "$st"; then return 0; fi
	# rewrite OUT/ to this run's shot folder (never touches the committed corpus)
	sed "s#OUT/#$WSL_OUT/#g" "$src" > "$W/$base"
	local arg
	if [ "$kind" = batch ]; then arg=--batch; else arg=--ui-script; fi
	( cd "$W" && timeout "$TIMEOUT" ./gonptechan.exe "${RUN_ARGS[@]}" "$arg" "$(win "$W/$base")" </dev/null >"$W/$base.log" 2>&1 )
	local rc=$?
	printf 'done %d\n' "$rc" > "$st"
	echo "== $case_base exit $rc"
	[ "$rc" -ne 0 ] && fail=1
	return 0
}

for f in "$CORPUS"/*.batch; do [ -e "$f" ] && run_file "$f" batch; done
for f in "$CORPUS"/ui/*.txt; do [ -e "$f" ] && run_file "$f" ui; done

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
