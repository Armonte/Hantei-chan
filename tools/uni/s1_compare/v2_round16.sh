#!/bin/bash
# tools/uni/s1_compare/v2_round16.sh [BWDIR]: render-match16. The MBTL rows against the HUD-free recaptures (*_nh: the
# battle HUD / gauges hidden by holding g_BattleHudFader, MBTL IDB 0xAFC3C8; gamecap cases v2r16_mbtl_chr00{1,11}_nohud.gcs)
# next to the round-5 captures (*_nb). Zoom 1: camera geometry, --subpix 0,0. Zoom 1.5: the round-12 chr001 offset
# (round13/reg/mbtl_chr001_nb.json, +0.25,0: same camera) on both; a refit on the _nh p238 alone (round16/reg) is printed
# for reference only (+0.25,+0.125: worse on p277/p157, the p238 fit alone is ambiguous in y).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
R13=$D/docs/audit_2026_10_14/evidence/compare/v2/round13/reg; R=$D/docs/audit_2026_10_14/evidence/compare/v2/round16/reg; mkdir -p "$R"
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw|^subpix" | cut -c1-110; }
[ -f "$R/mbtl_chr001_nh.json" ] && [ -z "${REFIT:-}" ] || run mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nh "${L0[@]}" --grey --subpix-obj raw --subpix-fit "$R/mbtl_chr001_nh.json" >/dev/null
for f in "$R13/mbtl_chr001_nb.json" "$R/mbtl_chr001_nh.json"; do python3 -c "import json,sys;j=json.load(open(sys.argv[1]));print('reg %-16s %+.4f %+.4f'%(sys.argv[1].split('/')[-1][:-5],*j['subpix']))" "$f"; done
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2 mbtl_pups_chr011_p309; do
  for s in nb nh; do
    echo "-- ${c}_z1 vs _$s (reg 0,0)"; run ${c}_z1 --game-name ${c}_$s "${L0[@]}" --grey --subpix 0,0
  done
  echo "-- $c (zoom 1.5) vs _nb (reg mbtl_chr001_nb)"; run $c --game-name ${c}_nb "${L0[@]}" --grey --subpix "$R13/mbtl_chr001_nb.json"
  echo "-- $c (zoom 1.5) vs _nh (reg mbtl_chr001_nb)"; run $c --game-name ${c}_nh "${L0[@]}" --grey --subpix "$R13/mbtl_chr001_nb.json"
done
