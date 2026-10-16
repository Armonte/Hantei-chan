#!/bin/bash
# run_features.sh: feature screenshots through the in-app --ui-script runner (tools/ui/ab_shot.sh, headless; no real input).
P=/home/teo/dev/hantei-chan-wt/parity; D=$P/docs/showcase_2026-10-16/scripts/ui; export AB_WORK=/mnt/c/dev/hantei-chan/work/ab_showcase AB_TIMEOUT=240
for s in feat_cgm_uni2_palettes feat_squirrel feat_stage3d feat_pcmod; do echo "== $s"; $P/tools/ui/ab_shot.sh $D/$s.txt 2>&1 | tail -4; done
echo "== feat_gamelink (--tool gamelink)"
W=$AB_WORK; sed "s#C:/dev/hantei-chan/work/ab/#$(wslpath -m $W)/#g" $D/feat_gamelink.txt > $W/script_gl.txt
(cd $W && timeout 240 ./gonptechan.exe --headless --tool gamelink --ui-script "$(wslpath -w $W/script_gl.txt)"; echo "exit $?")
ls -la $W/*.png
