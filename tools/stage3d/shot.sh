#!/bin/bash
# tools/stage3d/shot.sh <script>: tools/ui/ab_shot.sh with the editor window placed off-screen (posX -2000) and never
# activated by us, so desktop input cannot reach it. `stage3d capture` reads the stage's own 1280x720 target, so the
# window size and any view zoom do not affect those shots.
D="$(cd "$(dirname "$0")/../.." && pwd)"
W="${AB_WORK:-/mnt/c/dev/hantei-chan/work/ab_$(basename "$D")}"
mkdir -p "$W"
rm -f "$W/hanteichan.ini"
printf "[Other settings][]\nposX=-2000\nposY=10\nsizeX=${AB_W:-1600}\nsizeY=${AB_H:-1000}\n" > "$W/hanteichan.ini"
AB_KEEP_INI=1 AB_WORK="$W" exec "$D/tools/ui/ab_shot.sh" "$1"
