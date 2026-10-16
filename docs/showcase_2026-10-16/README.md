# Showcase 2026-10-16: Hantei-chan vs sosfiro's fork vs gonp, against the game

Each sheet shows the same pattern and frame in four columns: the **game** (gamecap capture, HUD-free, stage held),
**ours** (update/parity-1014, build e758112), **sosfiro's fork** (v3.0.0_alpha10) and **gonp** (gonpgonp/Hantei-chan
master 7e97e19). The second row is |game - tool| x4. All per-case data is in `manifest.json`.

## How the numbers are made
- Each tool renders the case on black, white and 64-grey clear colours, with the same view (1264x861, origin at
  w/2, h/2+150, same zoom). Only the tool's own GL back buffer is read: no desktop capture and no input.
- `tools/uni/s1_compare/v2_model.py` (unchanged) composites each tool's render over the game's own fighters-hidden
  plate. It applies the same light model, bokashi/glow model and camera registration for every tool.
- `scripts/showcase_metric.py` scores every tool on the **union of all tools' footprints**. Using each tool's own
  footprint would reward a tool that draws less. Metric: mean |d| per channel (0-255), and the % of pixels whose
  |d| summed over RGB is > 24.

## Results (mean |d| / % px > 24, lower is better)

| case | ours | sosfiro | gonp |
|---|---|---|---|
| UNI2 chr020 p5 f2: CG + .pat set 991 (additive, PRAS, ring arcs) | **1.32 / 1.9%** | 2.44 / 7.3% | 39.37 / 50.1% |
| UNI2 chr020 p435 tick 27: rotated CG mid-tween (AFHK) | **3.98 / 6.9%** | 85.50 / 94.6% | 104.64 / 100% |
| UNI2 chr027 p449 f4: PUPS palette + PPPA .pat | **2.47 / 4.5%** | 129.25 / 87.5% | 73.75 / 100% |
| UNI2 chr019 p153: .pat-only, PRRV 3 flips | **0.38 / 0.3%** | 18.03 / 35.5% | 74.05 / 85.5% |
| MBTL chr001 p238 f10: translucent tray, zoom 1.5 | **0.39 / 0.4%** | 3.50 / 7.7% | 78.57 / 84.6% |
| MBTL chr001 p157 f2: additive arcs + puddle, zoom 1.5 | **0.26 / 0.2%** | 21.54 / 66.0% | 50.87 / 77.9% |
| MBTL chr001 p0 f0 at zoom 1.5, CG filter (control) | **0.67 / 0.0%** | 7.78 / 20.8% | 7.78 / 20.8% |
| MBAACC ActCadenza moon-3 Akiha p238 f4 (effect spawns) | qualitative | not shown | qualitative |

## What the other tools got wrong (all visible in the sheets)
- **sosfiro:** wrong palette on p435 (light-blue body) and on the PUPS case (white coat), and the p435 mid-tween
  rotation is not interpolated. On MBTL p157 the arcs are red/magenta and the puddle is missing; on p238 the
  translucent glow is missing; the PRRV 3 streaks land off the game geometry. On set 991 it is close to the game,
  and second best.
- **gonp:** no .pat support, so .pat-only frames come out empty. UNI2 palettes are not decoded: p435 is a solid green
  silhouette and the PUPS coat is orange. There is no playback/AFHK and no CG zoom filter. On plain MBAACC sprites it
  is correct.

## Caveats
- The cases were picked before measuring. It is a subset of the render-match cases; the ones not run are listed in
  `manifest.json` under `selection`.
- **Ours is not perfect.** p435 scores 3.98 on the shared footprint (0.63 on its own footprint): the game's drop
  shadow and point-sampled rotated texels. The set-991 ring arcs change between game runs.
- The camera registration was fitted earlier on our own control shots. It is a game-camera offset, and it is applied
  unchanged to every tool.
- **MBAACC:** there is no game column, because gamecap drives UNI2 and MBTL only. The sosfiro *capture build* crashed
  loading MBAACC akiha (exit 5, minidump). This was not checked in sosfiro's own UI, so it is not counted against
  sosfiro.
- **"Gonptechan":** this is the name of our own fork's binary and releases, and "Gonptechan EX" is bigorados' fork of
  ours. gonp's actual editor is upstream gonpgonp/Hantei-chan. To drive it, we added a capture-only patch
  (`scripts/gonp_capture_args.patch`): it loads the file, sets pattern, frame, zoom and clear colour, hides panels and
  axis lines, and reads back the frame. Its render code is unchanged.

## Features with no equivalent in sosfiro or gonp (`features/`)
- CG manager: an MBAACC bank insert/replace shown in game (`cgm_mbaacc_ingame_*.jpg`, from `docs/cg/evidence/`),
  UNI2 palette pairs (`feat_cgm_uni2_palettes.png`) and the unified "CG + .pat textures" tab on UNI2 chr020, with CG images
  and .pat textures side by side (`feat_cgm_uni2_textures.png`).
- The Squirrel script panel with the UNI2 API table loaded (536 natives): a hover tooltip on an engine native
  (`feat_squirrel_hover.png`) and the Problems tab after a check (`feat_squirrel_problems.png`).
- The 3D stage view with MBAACC camera alignment (`feat_stage3d_*_align.png`). The BgList stage name is decoded from cp932.
- The game link / hot-reload panel (`feat_gamelink_panel.png`, with no game attached; hitbox overlay off).
- PovertyCaster .pcmod export (`pcmod_export_*.png`).

The feature shots were retaken on 2026-10-16 with branch fix/showcase-polish. The scripts read the games through neutral
folders: `C:\games\UNI2` and `C:\games\MBTL` are junctions to the game data, and `C:\games\MyMod` holds a copy of the pcmod
fixture. `scripts/ui/crop_features.py` cuts each window out of the full frame.

## Reproduce
`scripts/shoot_others.sh {sosfiro|gonp} <cases>`, then `tools/uni/s1_compare/shoot_batch.sh` (with
`OUTDIR=work/bw_ours`), then `scripts/run_cases.sh`, `scripts/make_sheets.py`, `scripts/mbaacc_sheet.py` and
`scripts/ui/run_features.sh` and `scripts/ui/crop_features.py`. The raw shots in `work/` are not committed.
