#!/usr/bin/env bash
# Capture ground truth for every MBAACC stage (or the ids given) from the test copy.
#   capture_all.sh <outdir> [id ...]
# Per stage id NN, into <outdir>/sNN/:
#   c0/cL/cR/cU  frozen stage-only shots at camera (0,0) (-208,0) (208,0) (0,-200), zoom 1, + state dumps
#   z0           camera (0,0) at zoom 0.839895 (the game's zoomed-out value)
#   k1/k30/k120/k600  camera (0,0) exactly 1/30/120/600 stage ticks after c0 (frozen + pcmem step)
#   ch           fighters drawn again (shadows / lights), HUD still off, camera (0,0)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SC="$HERE/stage_capture.sh"
OUT="$1"; shift
IDS="$*"
if [ -z "$IDS" ]; then
    IDS=$(tr -d '\r' < /mnt/c/games/mbaacc_winaspect/bg/BgList.ini | awk '/^\[Bg_/{s=substr($0,5,3)} /^DataFile/{print s+0}')
fi
for id in $IDS; do
    d=$(printf '%s/s%02d' "$OUT" "$id")
    if [ -s "$d/ch.png" ]; then echo "skip $id"; continue; fi
    mkdir -p "$d"
    "$SC" stop; sleep 2
    if ! "$SC" start "$id"; then echo "!! stage $id did not start"; continue; fi
    sleep 4
    "$SC" stageonly
    "$SC" freeze
    "$SC" cam 0 0 1.0; sleep 0.4
    "$SC" shot "$d" c0
    "$SC" cam -208 0 1.0; sleep 0.4; "$SC" shot "$d" cL
    "$SC" cam 208 0 1.0;  sleep 0.4; "$SC" shot "$d" cR
    "$SC" cam 0 -200 1.0; sleep 0.4; "$SC" shot "$d" cU
    "$SC" cam 0 0 0.839895; sleep 0.4; "$SC" shot "$d" z0
    "$SC" cam 0 0 1.0
    # exact animation steps from the c0 state: +1, +30, +120, +600 ticks
    "$SC" step 1;   sleep 0.4; "$SC" shot "$d" k1
    "$SC" step 29;  sleep 0.4; "$SC" shot "$d" k30
    "$SC" step 90;  sleep 0.4; "$SC" shot "$d" k120
    "$SC" step 480; sleep 0.4; "$SC" shot "$d" k600
    "$SC" chars; sleep 0.5
    "$SC" shot "$d" ch
    echo "done $id"
done
"$SC" stop
