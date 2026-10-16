#!/bin/bash
# tools/uni/s1_compare/v2_round11.sh [BWDIR]: render-match11. Control-registered metric: each control's registration is
# camera geometry (--exact-reg, no integer refine) plus a bounded sub-pixel offset fitted on the control's own lit
# residual (v2_model --subpix-fit); that offset goes unchanged to the control's cases (--subpix JSON). Offsets in
# docs/.../v2/round11/reg/*.json. Every case line is printed three ways: round-9 registration, --exact-reg, control-registered.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
R=$D/docs/audit_2026_10_14/evidence/compare/v2/round11/reg; mkdir -p "$R"
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
BK="--bokashi 0.0784"; L0=(--light "1,0;1,0;1,0")  # render-match13: an array; the old quoted L0 string passed "--light 1,0;..." as ONE argument, which v2_model ignored (stage light JSON)
GR="--grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5"
G19="--grad-ctrl ctrl_chr019_p0,ctrl_chr019_p0_r5"; G19Z="--grad-ctrl ctrl_chr019_p0_z1,ctrl_chr019_p0_r5"
G24="--grad-ctrl ctrl_chr024_p0,ctrl_chr024_p0_unlit"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw|^subpix" | cut -c1-110; }
fit() { local tag=$1; shift; [ -f "$R/$tag.json" ] && [ -z "${REFIT:-}" ] || run "$@" --subpix-fit "$R/$tag.json" >/dev/null & }
three() { local tag=$1; shift; echo "-- $1 (reg $tag)"; run "$@"; run "$@" --exact-reg; run "$@" --subpix "$R/$tag.json"; }
# controls: fit (cached in $R unless REFIT=1)
fit chr020_r3 ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $BK
fit chr027_r5 ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey $G27 $BK
fit chr019_r5 ctrl_chr019_p0 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19 $BK
fit chr019_r5_z1 ctrl_chr019_p0_z1 --game-name ctrl_chr019_p0_r5 "${L0[@]}" --grey $G19Z $BK
fit chr027_r5_z1 ctrl_chr027_p0_z1 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
fit chr024_unlit ctrl_chr024_p0 --game-name ctrl_chr024_p0_unlit --grey $G24 $BK
fit mbtl_chr001_nb mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nb "${L0[@]}" --grey
wait
for f in "$R"/*.json; do python3 -c "import json,sys;j=json.load(open(sys.argv[1]));print('reg %-16s %+.4f %+.4f  (%s)'%(sys.argv[1].split('/')[-1][:-5],*j['subpix'],j['control']))" "$f"; done
three chr020_r3 ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $BK
three chr020_r3 afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_r3 $GR --glow sq --grey $BK
three chr020_r3 afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 $GR --glow sq --grey $BK
three chr020_r3 pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --grey $BK --layers pras_chr020_p5f2_cg,pras_chr020_p5f2_pat
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
three chr020_r3 pras_chr020_p5f2_z1 --game-name pras_chr020_p5f2_r3 $GR --glow sq --grey $BK --layers pras_chr020_p5f2_z1_cg,pras_chr020_p5f2_z1_pat
three chr027_r5_z1 ctrl_chr027_p0_z1 --game-name ctrl_chr027_p0_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
three chr027_r5_z1 pups_chr027_p449f4_z1 --game-name pups_chr027_p449f4_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0_z1,ctrl_chr027_p0_r5 $BK
three chr024_unlit rect_chr024_p76_z1 --game-name rect_chr024_p76_unlit --grey
