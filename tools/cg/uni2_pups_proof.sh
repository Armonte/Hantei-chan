#!/bin/bash
# UNI2 in-game PUPS proof through gamecap (per-process: input only reaches the PID gamecap launched, capture = its backbuffer).
# UNI2 1.42 does NOT take loose files from ___St\data (S3 test: the archive copy wins, 0x4B9971). Run it with fbmod / UNI2-IM
# active and OVR = its override root relative to the game dir, "UNI2-IM\Mods" (what the CG manager's "Save to mod folder" uses). The override folder must not exist before the run; it is removed afterwards and checked.
[ -n "${OVR:-}" ] || { echo "pending: no UNI2 loader override root (set OVR=<dir relative to the game> once a loader is active)"; exit 2; }
set -u
D="$(cd "$(dirname "$0")/../.." && pwd)"
U="/mnt/c/Program Files (x86)/Steam/steamapps/common/UNDER NIGHT IN-BIRTH II Sys Celes"
G=/mnt/c/dev/hantei-chan/tools/gamecap; EV="$D/docs/audit_2026_10_14/evidence/cg"
SRC=/mnt/c/dev/hantei-chan/work/s45/uni2_pups_proof/___St/data/chr000
if tasklist.exe 2>/dev/null | grep -qiE "uni2\.exe|MBTL\.exe|MBAA\.exe|mbacPC\.exe|hantei4\.exe|gamecap\.exe"; then echo "a game or gamecap is running: not starting"; exit 3; fi
[ -e "$U/$OVR" ] && { echo "$U/$OVR already exists: refusing (it is not ours; it would be removed after the run)"; exit 3; }
[ -f "$SRC/chr000.ha6" ] && [ -f "$SRC/chr000_p1.pal" ] || python3 "$D/tools/cg/make_uni2_pups_proof.py" || exit 1
cleanup() { rm -rf "$U/$OVR"; [ -e "$U/$OVR" ] && echo "!! override still present" || echo "restored: override removed"; }
trap cleanup EXIT
run() { rm -f "$1"; "$G/build/gamecap.exe" -D "OUT=$(wslpath -w "$1")" "$(wslpath -w "$D/tools/cg/uni2_pups_proof.gcs")" </dev/null 2>&1 | tail -4; [ -s "$1" ]; }
echo "=== vanilla"; run "$EV/uni2_pups_before_vanilla.png" || { echo "vanilla run failed: the override is NOT placed"; exit 1; }
# never place the override while any game runs (another stream may have started one since the vanilla run)
if tasklist.exe 2>/dev/null | grep -qiE "uni2\.exe|MBTL\.exe|MBAA\.exe|mbacPC\.exe|hantei4\.exe|gamecap\.exe"; then echo "a game started meanwhile: override not placed"; exit 3; fi
mkdir -p "$U/$OVR/data/chr000" && cp "$SRC/chr000.ha6" "$SRC/chr000_p1.pal" "$U/$OVR/data/chr000/"
(cd "$U/$OVR" && find . -type f | xargs sha1sum)
echo "=== with the override (pattern 0 PUPS 1 + new chr000_p1.pal)"; run "$EV/uni2_pups_after_p1.png"
