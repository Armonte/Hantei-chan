#!/usr/bin/env bash
# Render every captured stage view with Hantei-chan (bg_render, state imported from the game dump) and diff
# it against the game capture. Also checks the runtime: the pool after N ticks (k1/k30/k120/k600) must equal
# the game's pool after the same exact step (bg_test --state c0 --step N --compare kN).
#   compare_all.sh <capture dir> <out dir> [id ...]      writes <out>/sNN_<view>_triple.png and results.jsonl
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$HERE/../../build"
CAP="$1"; OUT="$2"; shift 2
BG=/mnt/c/games/mbaacc_tag/bg   # read-only; the test copy links to the same files
mkdir -p "$OUT"
RES="$OUT/results.jsonl"
: > "$RES"
IDS="$*"
[ -n "$IDS" ] || IDS=$(ls "$CAP" | sed -n 's/^s\([0-9][0-9]\)$/\1/p')
for id in $IDS; do
    id=$(printf '%02d' $((10#$id)))
    d="$CAP/s$id"
    [ -f "$d/c0.png" ] || continue
    # the stage file the game loaded (BgList DataFile of this id)
    df=$(tr -d '\r' < "$BG/BgList.ini" | awk -v s="[Bg_0$id]" '$0==s{f=1;next} /^\[/{f=0} f&&/^DataFile/{print $3}')
    [ -n "$df" ] || { echo "s$id: no BgList entry (the game cannot load it), skipped"; continue; }
    dat="$(wslpath -w "$BG")\\$df.dat"
    for v in c0 cL cR cU z0 k1 k30 k120 k600; do
        [ -f "$d/$v.png" ] && [ -f "$d/$v.pool" ] || continue
        "$BUILD/bg_render.exe" "$dat" --out "$(wslpath -w "$OUT")\\s${id}_$v.png" --state "$(wslpath -w "$d")\\$v" > /dev/null 2>&1
        m=$(python3 "$HERE/stage_diff.py" "$d/$v.png" "$OUT/s${id}_$v.png" "$OUT/s${id}_$v" --label "s$id $v" 2>/dev/null)
        [ -n "$m" ] || continue
        pool=""
        case $v in k*) n=${v#k}
            r=$("$BUILD/bg_test.exe" "$dat" --state "$(wslpath -w "$d")\\c0" --step "$n" --compare "$(wslpath -w "$d")\\$v" | tail -1)
            pool=$(echo "$r" | grep -q "POOL MATCH" && echo match || echo "diff")
            ;; esac
        echo "{\"id\":$((10#$id)),\"file\":\"$df\",\"view\":\"$v\",\"pool\":\"$pool\",\"m\":$m}" >> "$RES"
        rm -f "$OUT/s${id}_$v.png"
    done
    echo "s$id done"
done
python3 - "$RES" <<'EOF'
import json, sys, collections
rows=[json.loads(l) for l in open(sys.argv[1])]
by=collections.defaultdict(list)
for r in rows: by[r['id']].append(r)
print("id  file   views  worst_mae  min_exact%  min_close%  pool")
for i in sorted(by):
    rs=by[i]
    pools=[r['pool'] for r in rs if r['pool']]
    print("%02d  %-5s  %5d  %9.3f  %10.2f  %10.2f  %s" % (i, rs[0]['file'], len(rs), max(r['m']['mae'] for r in rs),
          min(r['m']['exact'] for r in rs), min(r['m']['close'] for r in rs), ",".join(sorted(set(pools))) or "-"))
EOF
