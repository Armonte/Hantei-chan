#!/bin/bash
# tools/uni/s1_compare/v2_round14.sh [BWDIR]: render-match14. Round 13 unchanged (no editor change this round; offsets
# reused from docs/.../v2/round13/reg), plus the pras ring-prop mask (v2_mask_rings.py) and the MBTL p238 HUD split.
# Round 13 header: render-match13. As round 12 with the L0 identity light passed as two
# arguments (the round-10..12 scripts passed it as one, so v2_model used the stage light JSON / sidecar aura on the
# chr019/chr027/MBTL lines), offsets refitted into docs/.../v2/round13/reg, plus the pras set-991 solo shots, the MBTL
# zoom-1 rows (p238/p277/p157/solos, pups chr011) and the p435 split.
# Round 12 header: render-match12. As round 11 (control-registered metric), plus: the stage
# gradient fit leaves out game-clipped px (>= 254), the pras glow silhouette is the CG layer only (--glow-sil cg), and
# the MBTL chr001 offset is fitted on raw (--subpix-obj raw). Offsets in docs/.../v2/round13/reg/*.json.
# Round 11 header:
# render-match11. Control-registered metric: each control's registration is
# camera geometry (--exact-reg, no integer refine) plus a bounded sub-pixel offset fitted on the control's own lit
# residual (v2_model --subpix-fit); that offset goes unchanged to the control's cases (--subpix JSON). Offsets in
# docs/.../v2/round13/reg/*.json. Every case line is printed three ways: round-9 registration, --exact-reg, control-registered.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
R=$D/docs/audit_2026_10_14/evidence/compare/v2/round13/reg; mkdir -p "$R"
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
BK="--bokashi 0.0784"; L0=(--light "1,0;1,0;1,0")  # render-match13: an array; the old quoted L0 string passed "--light 1,0;..." as ONE argument, which v2_model ignored (stage light JSON)
GR="--grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5"
G19="--grad-ctrl ctrl_chr019_p0,ctrl_chr019_p0_r5"; G19Z="--grad-ctrl ctrl_chr019_p0_z1,ctrl_chr019_p0_r5"
G24="--grad-ctrl ctrl_chr024_p0,ctrl_chr024_p0_unlit"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw|^subpix" | cut -c1-110; }
fit() { local tag=$1; shift; [ -f "$R/$tag.json" ] && [ -z "${REFIT:-}" ] || run "$@" --subpix-fit "$R/$tag.json" >/dev/null & }
three() { local tag=$1; shift; echo "-- $1 (reg $tag)"; run "$@" --subpix "$R/$tag.json"; }  # round 12: the control-registered line only
# controls: fit (cached in $R unless REFIT=1)
fit chr020_r3 ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $BK
fit chr027_r5 ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey $G27 $BK
fit chr019_r5 ctrl_chr019_p0 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19 $BK
fit chr019_r5_z1 ctrl_chr019_p0_z1 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19Z $BK
fit chr027_r5_z1 ctrl_chr027_p0_z1 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
fit chr024_unlit ctrl_chr024_p0 --game-name ctrl_chr024_p0_unlit --grey $G24 $BK
fit mbtl_chr001_nb mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nb "${L0[@]}" --grey --subpix-obj raw
wait
for f in "$R"/*.json; do python3 -c "import json,sys;j=json.load(open(sys.argv[1]));print('reg %-16s %+.4f %+.4f  (%s)'%(sys.argv[1].split('/')[-1][:-5],*j['subpix'],j['control']))" "$f"; done
three chr020_r3 ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $BK
three chr020_r3 afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_r3 $GR --glow sq --grey $BK
three chr020_r3 afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 $GR --glow sq --grey $BK
three chr020_r3 pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_cg,pras_chr020_p5f2_pat
three chr027_r5 ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey $G27 $BK
three chr027_r5 pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 "${L0[@]}" --grey $G27 $BK
three chr019_r5 ctrl_chr019_p0 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19 $BK
three chr019_r5 flipHV_chr019_p153 --game-name flipHV_chr019_p153_r5 "${L0[@]}" --grey
three chr019_r5_z1 ctrl_chr019_p0_z1 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19Z $BK
three chr019_r5_z1 flipHV_chr019_p153_z1 --game-name flipHV_chr019_p153_r5 "${L0[@]}" --grey
three chr019_r5_z1 flipHV_chr019_p153_solo8_z1 --game-name flipHV_chr019_p153_solo8 "${L0[@]}" --grey
three chr024_unlit ctrl_chr024_p0 --game-name ctrl_chr024_p0_unlit --grey $G24 $BK
three chr024_unlit rect_chr024_p76 --game-name rect_chr024_p76_unlit --grey
three mbtl_chr001_nb mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nb "${L0[@]}" --grey
three mbtl_chr001_nb mbtl_patctrl_chr001_p277f0 --game-name mbtl_patctrl_chr001_p277f0_nb "${L0[@]}" --grey
three mbtl_chr001_nb mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb "${L0[@]}" --grey
for k in s8 s456; do
  three mbtl_chr001_nb mbtl_flipV_chr001_p157f2_solo_$k "${L0[@]}" --grey --plate $GD/mbtl_flipV_chr001_p157f2_solo_${k}_game_plate_bb.png
done
# no chr011 control was captured: the MBTL chr001 control's offset (same camera, zoom 1.5, stage)
three mbtl_chr001_nb mbtl_pups_chr011_p309 --game-name mbtl_pups_chr011_p309_nb "${L0[@]}" --grey
# zoom-1 shots (render-match11): no editor-shot resample at all; the zoom-1 controls fit to 0,0 (at sc = 1 the area
# resample is pixel-exact, so only whole-pixel offsets would register, and none does)
three chr020_r3 pras_chr020_p5f2_z1 --game-name pras_chr020_p5f2_r3 $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_z1_cg,pras_chr020_p5f2_z1_pat
three chr027_r5_z1 ctrl_chr027_p0_z1 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
three chr027_r5_z1 pups_chr027_p449f4_z1 --game-name pups_chr027_p449f4_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
three chr024_unlit rect_chr024_p76_z1 --game-name rect_chr024_p76_unlit --grey
# render-match13: pras set-991 solo shots at zoom 1 (game: gamecap_scripts/v2r13_chr020_solo_*.gcs, same chr020 r3 offset)
for k in s12 slin s1025; do
  three chr020_r3 pras_chr020_p5f2_solo_${k}_z1 --game-name pras_chr020_p5f2_solo_$k $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_solo_${k}_z1_cg,pras_chr020_p5f2_solo_${k}_z1_pat
done
# MBTL at zoom 1 (camera geometry, --subpix 0,0; raw = lit with the identity light)
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2 mbtl_pups_chr011_p309; do
  echo "-- ${c}_z1 (reg 0,0)"; run ${c}_z1 --game-name ${c}_nb "${L0[@]}" --grey --subpix 0,0
done
for k in s8 s456; do
  echo "-- mbtl_flipV_chr001_p157f2_solo_${k}_z1 (reg 0,0)"
  run mbtl_flipV_chr001_p157f2_solo_${k}_z1 --game-name mbtl_flipV_chr001_p157f2_solo_${k} "${L0[@]}" --grey --plate $GD/mbtl_flipV_chr001_p157f2_solo_${k}_game_plate_bb.png --subpix 0,0
done
# render-match13: pras recaptured this round (v2r13_chr020_full.gcs, pras_chr020_p5f2_r13) and both full captures split
# by where the solo .pat shots draw: the residual sits on an animated blue arc object that is not in the .pat layer
three chr020_r3 pras_chr020_p5f2_z1 --game-name pras_chr020_p5f2_r13 $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_z1_cg,pras_chr020_p5f2_z1_pat
for g in pras_chr020_p5f2_r3 pras_chr020_p5f2_r13; do
  echo "-- pras_chr020_p5f2_z1 vs $g split by the s12 / slin solo .pat shots"
  python3 "$T/v2_split_additive.py" pras_chr020_p5f2_solo_s12_z1_pat,pras_chr020_p5f2_solo_slin_z1_pat pras_chr020_p5f2_z1 --game-name $g \
    $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_z1_cg,pras_chr020_p5f2_z1_pat --subpix "$R/chr020_r3.json" --bw "$BW" 2>&1 | tail -4
done
# render-match14: pras without the set-991 ring props' runtime-varying thin game arcs (mask = editor srings solo shot |
# game r13 vs the norings capture), and the MBTL p238 split (tray prop / HUD rows)
echo "-- pras_chr020_p5f2_z1 ring mask (r13 capture)"
python3 "$T/v2_mask_rings.py" pras_chr020_p5f2_solo_srings_z1_pat $GD/pras_chr020_p5f2_norings_game.png pras_chr020_p5f2_z1 \
  --game-name pras_chr020_p5f2_r13 $GR --glow sq --glow-sil cg --grey $BK --layers pras_chr020_p5f2_z1_cg,pras_chr020_p5f2_z1_pat \
  --subpix "$R/chr020_r3.json" --bw "$BW" 2>&1 | tail -5
echo "-- mbtl_patctrl_chr001_p238f10_z1 tray / HUD split"
python3 "$T/v2_mbtl_hud_split.py" 2>&1 | tail -9
