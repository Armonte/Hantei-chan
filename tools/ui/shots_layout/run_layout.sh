#!/bin/bash
# run_layout.sh <tag> [exe] : runs L_all.txt at three window sizes (headless, in-app script runner, no real input)
# into /mnt/c/dev/hantei-chan/work/ab_layout_<tag>_<size>/ ; captures are the app's own frame (window only).
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=${1:-before}; EXE=${2:-$D/build/gonptechan.exe}
for sz in 1900x1000 1280x720 1920x1080; do
  W=/mnt/c/dev/hantei-chan/work/ab_layout_${T}_$sz
  ( AB_WORK=$W AB_EXE=$EXE AB_W=${sz%x*} AB_H=${sz#*x} AB_TIMEOUT=400 "$D/tools/ui/ab_shot.sh" "$D/tools/ui/shots_layout/L_all.txt" > $W.log 2>&1 ) &
done
wait; tail -n 3 /mnt/c/dev/hantei-chan/work/ab_layout_${T}_*.log
