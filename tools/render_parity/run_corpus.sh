#!/bin/bash
# run_corpus.sh <exe-dir> <out-dir> [--renderer gl|d3d9] [--only <glob>] [--round N]
#                [--force] [-j N] [--limit N]
#
# Renders the committed corpus (tools/render_parity/corpus/) with one process per
# file, headless, and writes <out-dir>/manifest.tsv:
#     case  png  sha256  w  h  exit  round
# One row per captured PNG (= one corpus case), plus one row per non-PNG file an
# export writes (w = h = 0; diff.py compares those by sha). `exit` is the exit
# code of the process that wrote the file (124 = killed by the timeout), taken
# from the output paths each corpus file names (<status>.outputs); a file no
# process claims gets -1, which diff.py fails. Each .batch is run with
# --headless --batch; each corpus/ui/*.txt with --headless --ui-script. The
# scratch folder on C: holds the exe, res/ and the rewritten scripts, the way
# tools/ui/ab_shot.sh does it. Runs resume: a file whose .status says done is
# skipped unless --force.
#
# -j N (default 8) shards the corpus across N workers, each with its own scratch
# folder and its own copy of the exe. The shard assignment is round robin over
# the sorted file list, so the case set does not depend on N; the golden and the
# new run still have to use the same -j, because the shots themselves are
# independent but the aggregate timing is not. -j 1 is the reference.
#
# The corpus names its output as OUT/<case>.png; OUT/ is rewritten to <out-dir>.
set -u
EXE_DIR="${1:?usage: run_corpus.sh <exe-dir> <out-dir> [options]}"
OUT="${2:?usage: run_corpus.sh <exe-dir> <out-dir> [--renderer gl|d3d9] [--only glob] [--round N]}"
shift 2
RENDERER=gl; ONLY=""; ROUND=1; FORCE=0; TIMEOUT="${RP_TIMEOUT:-300}"
RESET=1
LIMIT=0
JOBS="${RP_JOBS:-8}"
while [ $# -gt 0 ]; do
	case "$1" in
		--renderer) RENDERER="$2"; shift 2 ;;
		--only) ONLY="$2"; shift 2 ;;
		--round) ROUND="$2"; shift 2 ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		--force) FORCE=1; shift ;;
		--no-reset) RESET=0; shift ;;
		--limit) LIMIT="$2"; shift 2 ;;
		-j) JOBS="$2"; shift 2 ;;
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

# One scratch folder per shard, each with its own exe copy, so N processes never
# share a working folder (res/, hanteichan.ini, the dropped files).
ROOT="/mnt/c/dev/hantei-chan/work/render_parity/run_$(basename "$OUT")"
mkdir -p "$ROOT"
WIN_ROOT="$(wslpath -w "$ROOT")"
setup_shard() {   # <k>
	local W="$ROOT/shard$1"
	rm -rf "$W" 2>/dev/null || true
	mkdir -p "$W"
	# DrvFs occasionally fails a large copy while several shards copy at once;
	# retry, and fail loudly (a missing exe makes every file exit 127).
	local n=0
	while [ "$n" -lt 5 ]; do
		cp "$EXE_DIR/gonptechan.exe" "$W/gonptechan.exe" 2>/dev/null && [ -s "$W/gonptechan.exe" ] && break
		n=$((n + 1)); sleep 1
	done
	if [ ! -s "$W/gonptechan.exe" ]; then
		echo "run_corpus: could not stage gonptechan.exe into $W" >&2
		return 1
	fi
	cp -r "$EXE_DIR/res" "$W/" 2>/dev/null || true
	mkdir -p "$W/res/squirrel" && cp "$D"/docs/squirrel/api/*_api.json "$W/res/squirrel/" 2>/dev/null || true
	# The archive browser remembers the folders it last opened in
	# han2_settings.ini and the welcome screen draws them ("Recent game folders
	# and archives"). A run would otherwise show whatever the previous run left,
	# so the file starts from a fixed state (PLAN.md 4.3).
	printf '[browser]\nThumbs=1\n' > "$W/han2_settings.ini"
	# MBAACC DXT stages (bg01 ...) compress their bank to DXT5 at load with the
	# in-process D3DX9 encoder (d3dx9_36.dll + a D3D9 HAL device,
	# src/background/bg_gametex.cpp), and fall back to the builtin BC3 encoder
	# when that device cannot be created. Which one runs depends on the machine's
	# GPU state at that moment, not on the build: on 2026-10-19 the golden binary
	# itself rendered stage_mbaacc_bg01_{c,l,r} with the builtin encoder
	# (max |d| 78, 418319 px, 45.39 % vs the golden). The exe directory comes
	# first in the DLL search order, so this placeholder makes LoadLibrary fail
	# the same way every time and every binary uses the builtin encoder (PLAN.md
	# 4.3: pin the cause, not the golden).
	printf 'render parity: not a DLL; pins the builtin BC3 encoder (run_corpus.sh)\n' > "$W/d3dx9_36.dll"
}

win() { wslpath -w "$1"; }

# --deterministic pins every wall-clock-driven effect (PLAN.md 4.3): the stage
# background animation steps exactly one 1/60 s tick per frame and ImGui's modal
# dim fade uses a fixed frame step, so the same shot is byte-identical on a fast
# and on a loaded machine.
RUN_ARGS=(--headless --deterministic)
[ "$RENDERER" != gl ] && RUN_ARGS+=(--renderer "$RENDERER")

run_file() {   # <corpus-file> <kind> <shard> [<case-base>]
	local src="$1" kind="$2" shard="$3"
	local W="$ROOT/shard$shard"
	local base; base="$(basename "$src")"
	# A sharded batch part is called "<corpus-file>.part.batch", so the caller
	# passes the corpus file name for --only and for the status file.
	local case_base="${4:-$(basename "$src")}"
	case_base="${case_base%.*}"
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
	# Every shard runs its own part of the same corpus file, so the status file
	# carries the shard number; otherwise one shard resumes past another one's
	# work and the case set would depend on the order the shards finished.
	local st="$OUT/$case_base.status"
	[ "$kind" = batch ] && st="$OUT/$case_base.s$shard.status"
	if [ "$FORCE" = 0 ] && [ -f "$st" ] && grep -q '^done ' "$st"; then return 0; fi
	# Per-file reset: the app writes the folders it browsed back into
	# han2_settings.ini, and the welcome screen draws them ("Recent game folders
	# and archives"). Resetting once per shard is not enough, because files in a
	# shard run one after another and later ones would see the earlier ones'
	# folders. Resetting before every file makes each case independent of the
	# order and of -j (PLAN.md 4.3).
	printf '[browser]\nThumbs=1\n' > "$W/han2_settings.ini"
	# Rewrite OUT/ to this run's shot folder (never touches the committed corpus).
	# Sharded batch files are already inside the shard folder, so the rewritten
	# copy gets its own name: writing to "$W/$base" would read and truncate the
	# same file when the source is a split part.
	sed "s#OUT/#$SHOTS_WSL/#g" "$src" > "$W/$base.run"
	# A "#mode: plain" file (areas B and E) carries its own exe arguments on a
	# "#args:" line instead of a script body: those options are startup-only.
	local arg=() extra=() line mode=script
	while IFS= read -r line; do
		case "$line" in
			'#mode:'*) mode="${line#*:}"; mode="${mode// /}" ;;
			'#args:'*) read -r -a extra <<< "${line#*:}" ;;
		esac
	done < "$W/$base.run"
	if [ "$mode" = plain ]; then
		# No script argument at all: the file only supplies startup arguments
		# such as --export-png / --onion / --compare.
		( cd "$W" && timeout "$TIMEOUT" ./gonptechan.exe "${RUN_ARGS[@]}" "${extra[@]}" </dev/null >"$W/$base.run.log" 2>&1 )
	else
		[ "$kind" = batch ] && arg=(--batch) || arg=(--ui-script)
		( cd "$W" && timeout "$TIMEOUT" ./gonptechan.exe "${RUN_ARGS[@]}" "${extra[@]}" "${arg[@]}" "$(win "$W/$base.run")" </dev/null >"$W/$base.run.log" 2>&1 )
	fi
	local rc=$?
	# The outputs this process was asked to write (capture / export paths and
	# --export-png folders), relative to the shot folder: the manifest gives
	# every file under them this process's exit code.
	grep -o "$SHOTS_WSL/[^ ;\"]*" "$W/$base.run" | sed "s#^$SHOTS_WSL/##" | sort -u > "${st%.status}.outputs"
	printf 'done %d\n' "$rc" > "$st"
	echo "== $case_base exit $rc"
	return 0
}

# Work units, balanced by shot count: the .batch files hold most of the corpus
# (chars_uni2 alone is 832 lines), so they are split by line, round robin, into
# one sub-batch per shard. The ui/*.txt scripts are one process each and are
# assigned whole, round robin. The split is a pure function of the sorted input
# and -j, so golden and new runs see the same cases either way.
FILES=()
for f in "$CORPUS"/*.batch "$CORPUS"/ui/*.txt; do [ -e "$f" ] && FILES+=("$f"); done
if [ "$LIMIT" -gt 0 ]; then
	FILES=("${FILES[@]:0:$LIMIT}")
fi

# Order matters: setup_shard wipes the shard folder, so it has to run before the
# batch lines are split into it.
for k in $(seq 0 $((JOBS - 1))); do setup_shard "$k"; done

# Split the batch lines now; run_file consumes the per-shard files below.
python3 - "$ROOT" "$JOBS" "${FILES[@]}" <<'PY'
import os, sys
root, jobs = sys.argv[1], int(sys.argv[2])
files = sys.argv[3:]
ui_assign, bysrc, slot, n = [], {}, 0, 0
# The ui scripts that write into the shared scratch fixtures (work/ab_s45/fx,
# work/ab/s2, work/pacnyx/out) have to run one at a time and on shard 0. Every
# other ui script is independent and is spread over the shards.
# Scripts that own a scratch path another script also uses, or that write a
# whole output tree, run one at a time on shard 0. The rest parallelise.
SHARED = ("shots_cgm__cgm_1_", "shots_cgm__cgm_2_", "shots_cgm__cgm_3_",
          "shots_cgm__cgm_4_", "shots__s2_pat_editor",
          "export_")
for p in files:
    if not p.endswith(".batch"):
        name = os.path.basename(p)
        ui_assign.append((p, 0 if name.endswith(".txt") and name.startswith(SHARED) else slot % jobs))
        slot += 1
        continue
    base = os.path.basename(p)[:-6]
    for i, l in enumerate(l for l in open(p, encoding="utf-8") if l.strip()):
        bysrc.setdefault((i % jobs, base), []).append(l)
        n += 1
for (k, base), ls in sorted(bysrc.items()):
    d = os.path.join(root, "shard%d" % k)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "%s.part.batch" % base), "w", encoding="utf-8") as f:
        f.write("".join(ls))
with open(os.path.join(root, "ui_assign.tsv"), "w") as f:
    for p, k in ui_assign:
        f.write("%s\t%d\n" % (p, k))
print("split %d batch lines + %d ui scripts across %d shards" % (n, len(ui_assign), jobs))
PY

fail=0
SHOTS_WSL="$(wslpath -m "$SHOTS")"
for k in $(seq 0 $((JOBS - 1))); do
	(
	for part in "$ROOT/shard$k"/*.part.batch; do
		[ -e "$part" ] || continue
		# "chars_uni2.part.batch" -> case base "chars_uni2" for --only and status
		stem="${part##*/}"; stem="${stem%.part.batch}"
		run_file "$part" batch "$k" "$stem"
	done
		# The ui scripts go after this shard's batch work: every shard owns its
		# own assignments, so this reads the whole table and skips the rest.
		while IFS=$'\t' read -r p sk; do
			[ "$sk" = "$k" ] && run_file "$p" ui "$k"
		done < "$ROOT/ui_assign.tsv"
	) > "$ROOT/shard$k.log" 2>&1 &
done
wait

# The .status files are the authority on failures (a shard log also records them)
while IFS= read -r st; do grep -q '^done 0$' "$st" || fail=1; done < <(find "$OUT" -maxdepth 1 -name '*.status')

# manifest: every file under the shot folder, one case per file
python3 - "$OUT" "$ROUND" <<'PY'
import glob, hashlib, os, struct, sys
out, rnd = sys.argv[1], sys.argv[2]
shots = os.path.join(out, "shots")
# output path (relative to shots/) -> exit code of the process that wrote it
claims = {}
for st in glob.glob(os.path.join(out, "*.status")):
    words = open(st).read().split()
    rc = int(words[1]) if len(words) > 1 and words[0] == "done" else -1
    o = st[:-len(".status")] + ".outputs"
    if not os.path.exists(o):
        continue
    for rel in open(o).read().split():
        rel = rel.rstrip("/")
        claims[rel] = max(claims.get(rel, 0), rc) if rc >= 0 else rc
def exit_of(rel):
    # the exact file, else the longest claimed folder above it (--export-png)
    p = rel
    while p:
        if p in claims:
            return claims[p]
        p = p.rsplit("/", 1)[0] if "/" in p else ""
    return -1
rows = []
for dirpath, _, names in os.walk(shots):
    for n in sorted(names):
        p = os.path.join(dirpath, n)
        rel = os.path.relpath(p, out).replace(os.sep, "/")
        inner = rel[len("shots/"):]
        h = hashlib.sha256(open(p, "rb").read()).hexdigest()
        if n.endswith(".png"):
            case = inner[:-4]
            with open(p, "rb") as f:
                head = f.read(24)
            w, ht = struct.unpack(">II", head[16:24])
        else:
            case, w, ht = inner, 0, 0
        rows.append((case, rel, h, w, ht, exit_of(inner), rnd))
rows.sort()
path = os.path.join(out, "manifest.tsv")
with open(path, "w", encoding="utf-8") as f:
    f.write("case\tpng\tsha256\tw\th\texit\tround\n")
    for r in rows:
        f.write("%s\t%s\t%s\t%d\t%d\t%d\t%s\n" % r)
bad = sum(1 for r in rows if r[5] != 0)
print("manifest: %d cases (%d non-PNG, %d with a non-zero or unknown exit) -> %s" % (
    len(rows), sum(1 for r in rows if r[3] == 0), bad, path))
PY

# Keep the logs, drop the scratch (shards, exe copies, rewritten scripts): a
# full run's scratch is hundreds of MB on a nearly full C:.
mkdir -p "$OUT/logs"
for k in $(seq 0 $((JOBS - 1))); do
	cp "$ROOT/shard$k.log" "$OUT/logs/" 2>/dev/null || true
	mkdir -p "$OUT/logs/shard$k"
	cp "$ROOT/shard$k"/*.run.log "$OUT/logs/shard$k/" 2>/dev/null || true
done
rm -rf "$ROOT"

exit $fail
