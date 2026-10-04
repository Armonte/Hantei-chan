#!/bin/bash
# tools/ui/shoot_all.sh : runs every tools/ui/shots/*.txt through the in-app script runner and copies the captures into docs/ux/.
D="$(cd "$(dirname "$0")/../.." && pwd)"; S=/mnt/c/dev/hantei-chan/work/ab/shots
mkdir -p "$S"
for f in "$D"/tools/ui/shots/*.txt; do echo "== $f"; AB_TIMEOUT=240 "$D/tools/ui/ab_shot.sh" "$f" 2>&1 | grep -E "^(exit|script line|savechar)"; done
cp "$S"/*.png "$D/docs/ux/"
