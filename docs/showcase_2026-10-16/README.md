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
- CG manager: an MBAACC bank insert/replace shown in game (`cgm_mbaacc_ingame_*.jpg`, from `docs/cg/evidence/`), and
  UNI2 palette pairs (`feat_cgm_uni2_palettes.png`).
- The Squirrel script panel (`feat_squirrel_hover.png`). The scripted hover does not draw its tooltip into the capture.
- The 3D stage view with MBAACC camera alignment (`feat_stage3d_*_align.png`).
- The game link / hot-reload panel (`feat_gamelink_panel.png`, with no game attached).
- PovertyCaster .pcmod export (`pcmod_export_*.png`).

## Reproduce
`scripts/shoot_others.sh {sosfiro|gonp} <cases>`, then `tools/uni/s1_compare/shoot_batch.sh` (with
`OUTDIR=work/bw_ours`), then `scripts/run_cases.sh`, `scripts/make_sheets.py`, `scripts/mbaacc_sheet.py` and
`scripts/ui/run_features.sh`. The raw shots in `work/` are not committed.

## In-game recolour through the CG manager (`ingame/`)
This is a modder's round trip with no hand edits to files. `tools/cg/showcase_recolour.sh` drives the editor with --ui-script (no
real input) and `tools/cg/hue_shift.py` does the recolour. fbmod boots [boot] training with P1 = chr003 and P2 = chr005, colour 1.
The files are installed in `<game>\<IM>\Mods\data\chr003\` only inside gamecap's game lock (`tools/cg/showcase_ingame_boot.sh`)
and restored afterwards. The Mods\data folders were empty before and after, and UNI2_IM.ini / MBTL_IM.ini are byte-identical (md5).
The shots are window-only gamecap captures shrunk to 1280x720: `<game>_{before,after}_idle.jpg` and `<game>_{before,after}_move.jpg`.
The move is 236+A (pad_down/pad_up DOWN, RIGHT, then pad_tap A), shown 6 frames after the button.

- **Export.** "CG + .pat textures" tab > Export PNG of pattern 0's images (UNI2: 36, MBTL: 21) and of one .pat texture. Palettes tab >
  Export colours (.act) of chr003.pal palette 0. Every pattern-0 sprite is 8-bit (bank palette), so its colour lives in the .pal.
  The natural path is the palette, and the sprite pixels were not touched.
- **UNI2 Carmine, red to deep teal.** chr003.pal palette 0, entries 8-12, 31-40, 48-53, 114-120 and 251 (coat, hair and the red
  tint slots): hue **+185 deg**, with saturation and value kept. Entries 240-245 (blue effect tint slots): hue **-40 deg**. Skin
  (2-7), greys, black line art and the other 129 palettes are unchanged; 35 of 256 colours changed. .pat texture 0 `car_ef00.dds`
  (2048x2048 DXT5) is greyscale, because the game tints it from the palette's PPPA slots. Its grey pixels were colourised to hue
  185 deg, saturation 0.35, with value and alpha kept.
- **MBTL Shiki, navy to dark crimson.** chr003.pal palette 0: every entry below 240 with hue 200-245 deg and saturation >= 0.2 (the
  uniform and hair shading, plus entry 15, the light-blue eye highlight): hue **+125 deg**, with S, V and alpha kept; 52 colours
  changed. The fx slots 241-249 are unchanged. .pat texture 1 `chr003_0.dds` (2048x2048 DXT5): pixels with hue 160-220 deg and
  saturation >= 0.2 (the blue slashes) get hue **+150 deg**, with V and alpha kept (224,060 px). The red parts are unchanged.
- **Import and save.** Palettes tab > Import colours (the shifted .act) > **Save .pal to mod folder** (new, see below). Then
  "CG + .pat textures" > Import PNG (the shifted texture) > Save to mod folder. The game folder is a scratch folder; the boot
  script installs what the GUI wrote.
- **Result.** The idle pair shows the full recolour in game. On the move pair, the character's colours change, but the main
  236A effect does not (UNI2: the blue swirl; MBTL: the blue/purple ring). That effect is drawn from the shared effect bank,
  not from chr003.pat, so neither the texture edit nor the palette slots reach it. The .pat edit is loaded: the game reads the
  2.4 MB / 39 MB mod .pat without error. It is not visible in this move, though, and a move that draws car_ef00 /
  chr003_0 parts is still to be picked.
- **Fix found along the way.** The Palettes tab could only save the .pal in place, and UNI2 / MBTL never read that (they read the
  archive), so a palette recolour could not reach the game from the browser. "Save .pal to mod folder" writes it to the same IM
  mod folder as the tab's .cg / .pat save, using that tab's game and game-folder settings (`Window::modFolderDst`).
