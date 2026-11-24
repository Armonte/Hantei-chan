#!/bin/bash
# MBAACC archive priority (MBAA.exe: 0008 > 0003 > 0005 > 0002 > 0007 > 0000 > 0004 > 0006 > 0001, first mounted wins) in
#  (a) the Archive Browser's merged "All archives (game view)", (b) unpack (writes the winning copy), (c) pack (skips paths a higher archive holds).
# Machine-specific (WSL): builds a THROWAWAY folder from the Steam 0006.p (41 KB) + MBAA.exe: 0000.p = 0006.p, 0001.p = 0006.p with
# Shader/sh_fade.txt replaced. Name order would let 0001 win; the game (and the browser) must take 0000.
# Usage: tests/ui_scripts/mbaacc_merged_priority.sh  (from the repo root, after ./build.sh)
set -u
REPO=$(cd "$(dirname "$0")/../.." && pwd)
W=${W:-/mnt/c/dev/Hantei-chan/work/p_prio_test}
STEAM=${STEAM:-"/mnt/c/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD Actress Again Current Code"}
T=$REPO/build/fbarctool.exe; w() { wslpath -w "$1"; }
rm -rf "$W"; mkdir -p "$W/game" "$W/mod/Shader"
cp "$STEAM/0006.p" "$W/game/0000.p"; cp "$STEAM/MBAA.exe" "$W/game/"
printf 'modified in 0001\r\n' > "$W/mod/Shader/sh_fade.txt"
"$T" pack "$(w "$W/game/0000.p")" "$(w "$W/mod")" "$(w "$W/game/0001.p")" | tr -d '\r'
G=$(w "$W/game")
printf 'browse %s\nwaitidle\nsource All archives\nwaitidle\nowner Shader/sh_fade.txt\nunpackgame %s\nquit\n' "$G" "$G" > "$W/s1.txt"
printf 'packgame %s\nrestorepacked %s\nquit\n' "$G" "$G" > "$W/s2.txt"
cd "$REPO/build"
./gonptechan.exe --headless --ui-script "$(w "$W/s1.txt")"; cat "$W/s1.txt.log"
fail=0
grep -q "owner Shader/sh_fade.txt = 0000.p" "$W/s1.txt.log" && echo "PASS merged view takes 0000.p" || { echo "FAIL merged view"; fail=1; }
grep -q "modified in 0001" "$W/game/Shader/sh_fade.txt" && { echo "FAIL unpack wrote the 0001 copy"; fail=1; } || echo "PASS unpack wrote the 0000 copy"
printf 'loose edit\r\n' >> "$W/game/Shader/sh_fade.txt"
./gonptechan.exe --headless --ui-script "$(w "$W/s2.txt")"; cat "$W/s2.txt.log"
grep -q "0000.p: 1 replaced" "$W/s2.txt.log" && grep -q "0001.p: 0 replaced.*9 skipped" "$W/s2.txt.log" && echo "PASS pack edits 0000.p, skips the shadowed 0001.p entries" || { echo "FAIL pack"; fail=1; }
rm -rf "$W"
exit $fail
