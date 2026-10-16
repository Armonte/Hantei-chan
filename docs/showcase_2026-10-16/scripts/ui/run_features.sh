#!/bin/bash
# run_features.sh: feature screenshots through the in-app --ui-script runner (tools/ui/ab_shot.sh, headless; no real input).
# The scripts read the games through neutral folders: C:\games\UNI2 and C:\games\MBTL (junctions to gamedata\uni2, gamedata\mbtl)
# and C:\games\MyMod (a junction to a copy of the pcmod fixture). Then crop_features.py cuts each window out of the full frame.
P=${P:-/home/teo/dev/hantei-chan-wt/polish}; D=$P/docs/showcase_2026-10-16/scripts/ui; export AB_WORK=${AB_WORK:-/mnt/c/dev/hantei-chan/work/ab_polish} AB_TIMEOUT=240
for s in feat_cgm_uni2_palettes feat_cgm_uni2_textures feat_squirrel feat_stage3d feat_pcmod; do echo "== $s"; $P/tools/ui/ab_shot.sh $D/$s.txt 2>&1 | tail -4; done
echo "== feat_gamelink (--tool gamelink, hitbox overlay off so the sprite shows its own palette)"
W=$AB_WORK; sed "s#C:/dev/hantei-chan/work/ab/#$(wslpath -m $W)/#g" $D/feat_gamelink.txt > $W/script_gl.txt
(cd $W && timeout 240 ./gonptechan.exe --headless --hide-boxes --tool gamelink --ui-script "$(wslpath -w $W/script_gl.txt)"; echo "exit $?")
ls -la $W/*.png
