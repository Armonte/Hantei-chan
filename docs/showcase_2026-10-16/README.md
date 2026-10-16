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

## In-game recolour through the CG manager (`ingame/`)
This is a modder's round trip with no hand edits to files. `tools/cg/showcase_recolour.sh` drives the editor with --ui-script (no
real input) and `tools/cg/hue_shift.py` does the recolour. fpan-im boots [boot] training with P1 = chr003 and P2 = chr005, colour 1.
The files are installed in `<game>\<IM>\Mods\data\chr003\` only inside gamecap's game lock (`tools/cg/showcase_ingame_boot.sh`)
and restored afterwards. The Mods\data folders were empty before and after, and UNI2_IM.ini / MBTL_IM.ini are byte-identical (md5).
The shots are window-only gamecap captures shrunk to 1280x720: `<game>_{before,after}_idle.jpg` and `<game>_{before,after}_move.jpg`.
The move is 236+A: pad_down/pad_up DOWN, RIGHT, then pad_tap **X**, which is the game's A button on the default pad layout. Pad A is
the game's D button, so the first take on 2026-10-16 ran 236+D (Shield) by mistake, and that shield effect comes from the shared bank.
The move pair was retaken with the fix (`tools/cg/showcase_ingame_boot.sh <game> <out> X ...`). The idle pair is from the first
take, with the same palette.

- **Export.** "CG + .pat textures" tab > Export PNG of pattern 0's images (UNI2: 36, MBTL: 21) and of one .pat texture. Palettes tab >
  Export colours (.act) of chr003.pal palette 0. Every pattern-0 sprite is 8-bit (bank palette), so its colour lives in the .pal.
  The natural path is the palette, and the sprite pixels were not touched.
- **UNI2 Carmine, red to deep teal.** chr003.pal palette 0, entries 8-12, 31-40, 48-53, 114-120 and 251 (coat, hair and the red
  tint slots): hue **+185 deg**, with saturation and value kept. Entries 240-245 (blue effect tint slots): hue **-40 deg**. Skin
  (2-7), greys, black line art and the other 129 palettes are unchanged; 35 of 256 colours changed. .pat texture 0 `car_ef00.dds`
  (2048x2048 DXT5) is greyscale, because the game tints it from the palette's PPPA slots. Every grey pixel was colourised to hue
  **165 deg (sea green), saturation 0.6**, with value and alpha kept (902,305 px; `hue_shift.py png ... --colorize 165 0.6`).
- **MBTL Shiki, navy to dark crimson.** chr003.pal palette 0: every entry below 240 with hue 200-245 deg and saturation >= 0.2 (the
  uniform and hair shading, plus entry 15, the light-blue eye highlight): hue **+125 deg**, with S, V and alpha kept; 52 colours
  changed. The fx slots 241-249 are unchanged. .pat texture 1 `chr003_0.dds` (2048x2048 DXT5): pixels with hue 160-220 deg and
  saturation >= 0.2 (the blue slashes) get hue **+150 deg**, with V and alpha kept (224,060 px). The red parts are unchanged.
  The white slash-trail cutouts that 236A draws (cutout 56, texels 832,0 64x448, and cutout 21, texels 0,896 192x896) were then
  colourised to hue **348 deg (crimson), saturation 0.7**, with V and alpha kept (120,630 px; `--colorize 348 0.7 --rect ...`).
- **Import and save.** Palettes tab > Import colours (the shifted .act) > **Save .pal to mod folder** (new, see below). Then
  "CG + .pat textures" > Import PNG (the shifted texture) > Save to mod folder. The game folder is a scratch folder; the boot
  script installs what the GUI wrote.
- **Which frame draws the .pat texture (from the data).** `cgmtex users <ha6> <pat> <id>` prints the CG manager's "Used by" list
  (texture > parts > part sets > pattern layers), and `cgmtex effects <ha6>` prints every frame effect (to see what spawns a pattern).
  - UNI2 Carmine 236A: Mv_Skill_236A (chr003_mv_0.txt, maketmpl_SkillFireRollingCutter) creates an object at FrameID 100 and sets its
    pattern to `236TamaA` = pattern **136**. Frames 1-21 of 136 draw layer 0 from part sets 70-85, whose cutouts (56, 57, 59, 60, ...)
    sample texture 0 `car_ef00.dds` with PPPA colour slot 251. The game multiplies the texel by palette[251] (vanilla (255,0,0), red;
    the palette recolour makes it (0,234,255), cyan). So the blade's hue is texel x slot 251: cyan from the palette alone, and the
    sea-green cast in `uni2_after_move.jpg` comes from the texture edit. Shot: `move_30` (about 30 frames after the button).
  - MBTL Shiki 236A = pattern **200** (PTCN "236A"). Frame 5 has a type-1 effect that spawns pattern **210**, whose frames 0-3 draw
    part sets 648-651. Those place cutouts 56 and 21 from texture 1 `chr003_0.dds` with no colour slot (texel drawn as is), and
    cutout 22 (the same texels as 21) with fx slot 246 (blue, unchanged). The inner trail of the slash is the untinted texel, now
    crimson. The outer blue glow is the slot-246 copy, and the hit spark is the shared effect bank. Shots: before `move_06`, after
    `move_02`; the game's frame timing differed between the two boots, so the frames that match visually were used.
- **Result.** The move pair shows the .pat texture edit in game: the UNI2 236A blood blade is sea-green teal instead of red, and
  the MBTL 236A slash has a crimson inner trail instead of white. The first take's "no visible change" was the wrong button (236+D),
  not the .pat path.
- **Fix found along the way.** The Palettes tab could only save the .pal in place, and UNI2 / MBTL never read that (they read the
  archive), so a palette recolour could not reach the game from the browser. "Save .pal to mod folder" writes it to the same IM
  mod folder as the tab's .cg / .pat save, using that tab's game and game-folder settings (`Window::modFolderDst`).
