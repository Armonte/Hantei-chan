# DFCI 3D stages in MBAACC (pc-stage3d): issue list

Date: 2026-10-15. Scope: how DFCI (Dengeki Bunko Fighting Climax Ignition, arcade SDCS build) renders its 14 FBX stages, compared with
how pc-stage3d currently draws them in MBAACC. No code was changed. The IDA work is in
`C:\dev\transmission\Fighting Climax Ignition [SDCS]\RingGame.exe.i64` (renames saved, listed at the end).

Sources:
- game data: `/mnt/c/dev/transmission/dfci+transmission/bg/` (BgList.txt cp932, `bgNN/bg.fbx`, `object.txt`, `*.pat`), `Shader/fbxshader.txt`
- pc-stage3d: `/home/teo/dev/pc-fm2k-native/pc-stage3d/src/CameraMap.cpp`, `include/pc/stage3d/StageParams.hpp`, `shaders/stage3d.hlsl`,
  `src/FbxAscii.cpp`, inis `tools/stage3d_stages/dfci/dfci_bgNN.ini`, design doc `docs/recon/2026-10-03/melty_3d_stages.md` §14-15,
  evidence `melty_3d_dfci_a.png` / `_b.png`
- Zanaylo's UNI2-Improvement-Mod `src/Game/Stages/BgGrade.h` (DFCI lift 0 / contrast 1.37)

## 1. DFCI's own stage camera (from RingGame.exe, confirmed in IDA)

DFCI uses the same camera as UNI2 1.42. The only difference found is the near plane: 0.2 here, 0.15 in UNI2.

| Piece | DFCI (address) | Value |
|---|---|---|
| Per-stage camera setup | `dfci_stage3d_camera_from_stagedef` 0x7ECF90 | camera+0x238 = FOV (deg), +0x23C = VanishingPoint, `dfci_camera_set_target_size` 0x432950(TargetW, TargetH) |
| Target size | `dfci_stage3d_create_targets` 0x7ED1A0 | `g_dfci_stage3d_target_w/h` (0x99AA58/5C) = BgList TargetW/H; DFCI's BgList leaves them unset, so the default is 1280x720 |
| Camera fields | 0x432950 | +0x240 W/2 = 640, +0x244 H/2 = 360, +0x248 2/W = 1/640, +0x24C 2/H, **+0x250 H/W = 0.5625** |
| Projection | `dfci_camera_update_view_proj` 0x81BD20 | `PerspectiveFovLH(FOV, aspect 1.0, zn 0.2, zf 1000) * T(0, -VP, 0)` |
| View | same | LookAtLH, eye = (camX/640, (280/zoom - camY)/360 - VP/zoom, (p4/128 - cot(FOV/2))/zoom), looking +Z, up +Y, no rotation. `cot(FOV/2)` is read back as proj._22 (camera+0x2E8) |
| Stage world | `dfci_stage3d_render` 0x7EE530 | `Scale(Sx * 0.5625, Sy, Sz) * T(Position)`. There is no ViewRotationX/Y in DFCI's BgList or in the code |
| Z mirror | `dfci_fbxscene_load_build_root` 0x741450 | root = FBX global matrix * Scale(1, 1, -1), the same as UNI2 |
| Mesh shader | `Shader/fbxshader.txt` Diffuse_PS_Easy | `tex * COLOR0`. The fog lerp is commented out, so fog never applies even with IsFog = 1 |
| Whole-image grade | none | DFCI has no `sh_bgspeculer` and no 0.10 / 0.6708 composite (UNI2 has both) |
| Bloom | FbxBloomShader_* techniques exist | `IsBloom = 0` on every stage, so bloom is never on |
| Stage effect sprites | `dfci_stage3d_draw` 0x7EEA70 -> `dfci_panimgr_draw_pass` | `bgNN/object.txt` + `.pat` (snow, sakura, confetti), prio 0 = behind the fighters, 700 = in front |

### What these formulas mean in practice

- Aspect 1.0 plus the 0.5625 X squeeze is the same as a normal 16:9 `PerspectiveFovLH` with **FOV as the VERTICAL angle**:
  x_ndc = cot·0.5625x/z in both cases.
- On the fighter plane, 1 unit = **360 px at 720p (240 px at 480 rows) at zoom 1, whatever the FOV**.
- The camera is **cot(FOV/2) ≈ 2.1 to 3.2 units** from the plane, not 10.
- World y = 0 at z = 0 lands on screen row 640/720 (426.7 at 480 rows).
- The horizon (vanishing row) is at 360·(1 + VP) at 720p, i.e. **240·(1 + VP) at 480**.
- The eye is 0.778 − VP units above the floor.
- Position.x is applied after the squeeze, so it is in squeezed units: one Position.x unit = 1/0.5625 = 1.778 world units (640 px at 720p).

## 2. Per-stage data

### 2.1 BgList.txt (all 14 stages, plus Bg_099 debug which has no data)

No stage sets ViewRotationX/Y, TargetW/H or IsBloom = 1. StageW = 4096 and MSAA = 2 everywhere.

| Stage | Name | Scale | Position | FOV | VP | IsFog / Start / End / Color (never applied) | Shadow scale / alpha |
|---|---|---|---|---|---|---|---|
| bg01 | Valkyria Chronicles | 1.7 | -1.5, 0, 17.5 | 35.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 0.8 / 0.35 |
| bg02 | Virtua Fighter 5 | 10 | -1.7, 0, 0 | 39.6 | 0.40 | 1 / 2 / 250 / (.17,.38,.59,1) | 0.8 / 0.35 |
| bg04 | NiGHTS | 1.0 | 0, 0, 0 | 35.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 1.0 / 0.4 |
| bg05 | PSO2 | 0.0087 | 0, -0.1, -1.0 | 35.6 | 0.58 | 1 / 2 / 700 / (2,0,0,1) | 1.0 / 0.4 |
| bg07 | Sonic | 1.1 | 0, 0, 0 | 39.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 0.8 / 0.35 |
| bg08 | 7th Dragon 2020 | 0.65 | 0, 0, -5.0 | 39.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 1.0 / 0.4 |
| bg09 | Shinobi | 0.5 | 0.5, 0, 0 | 35.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 0.8 / 0.35 |
| bg13 | Virtual-On | 1.0 | 0, 0, 0 | 35.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 1.0 / 0.2 |
| bg14 | Border Break | 0.53 | 0, -0.40, 26.2 | 39.6 | 0.55 | 1 / 2 / 700 / (.5,.6,2,1) | 0.4 / 0.8 |
| bg15 | Valkyria (night) | 1.7 | -1.5, 0, 17.5 | 35.6 | 0.55 | 1 / 2 / 700 / (2,0,0,1) | 0.8 / 0.35 |
| bg16 | Wonderland Wars | 0.15 | 0, 0, -4.2 | 35.0 | 0.60 | 1 / 0 / 2200 / (.5,.5,.5,1) | 0.8 / 0.35 |
| bg17 | Hi☆sCoool! SeHa Girls | 0.14 | 0, 0, 0 | 45.0 | 0.30 | 1 / 0 / 2200 / (.5,.5,.5,1) | (default) |
| bg18 | World End Eclipse | 0.3 | 0, 0.2, -4.0 | 37.0 | 0.30 | 1 / 0 / 1e6 / (.17,.41,.80,1) | (default) |
| bg29 | Training | 10 | 0, 0, 0 | 50.0 | 0.40 | 1 / 0 / 1000 / (.17,.41,.80,1) | (default) |

### 2.2 FBX facts

Every file has the same basics:
- ASCII FBX 6.1.0 (FBXVersion 6100), FBX SDK 2009.1.
- GlobalSettings UpAxis = 1 (Y up) and UnitScaleFactor = 1, so there is no unit or axis conversion to apply.
- One UV set per mesh.
- Only `.dds` texture references (no png or tga), and none are missing on disk.
- One take, "Take 001".

Animation length is the take's LocalTime end divided by 46186158000 ticks/s. Meshes with no colour layer render white, which is right because their lighting is baked into the textures.

| Stage | Mesh / Null / other nodes | Materials (lambert / phong) | Meshes with a colour layer | Anim curves (KeyCount blocks) | Take length | Negative-scale nodes | Extra files |
|---|---|---|---|---|---|---|---|
| bg01 | 27 / 17 | 15 / 15 | 2 | 18 | 40.0 s | 2 | object.txt, stage_ef.pat |
| bg02 | 71 / 17 | 21 / 3 | 7 | 0 (static) | (1152 s header, no keys) | 27 | object.txt, sakura.pat |
| bg04 | 68 / 45 / 1 Camera | 18 / 1 | 7 | 29 | 45.0 s | 1 | object.txt, yuki.pat |
| bg05 | 81 / 17 | 11 / 11 | 17 | 110 | 40.0 s | 5 | none |
| bg07 | 117 / 22 / 1 Camera / 4 Light | 22 / 9 | 0 | 100 | 4.0 s | 0 | object.txt, sonicbg.pat |
| bg08 | 34 / 7 | 13 / 2 | 0 | 25 | 75.0 s | 0 | object.txt, 7drbg.pat |
| bg09 | 35 / 9 / 1 Light | 7 / 2 | 0 | 27 | 2.67 s | 0 | object.txt, kamihubuki.pat |
| bg13 | 82 / 38 | 13 / 4 | 0 | 97 | 41.7 s | 1 | object.txt, yuki.pat (file is `bg.FBX`, upper case) |
| bg14 | 107 / 30 / 1 Camera | 19 / 1 | 0 | 105 | 41.7 s | 2 | object.txt, yuki.pat |
| bg15 | 23 / 15 | 14 / 15 | 2 | 4 | 40.0 s | 2 | object.txt, yuki.pat |
| bg16 | 266 / 60 / 1 Camera | 26 / 2 | 106 | 103 | 30.0 s | 0 | none |
| bg17 | 33 / 6 | 29 / 0 | 28 | 7 | 30.0 s | 0 | none |
| bg18 | 184 / 59 | 15 / 0 | 19 | 494 | 30.0 s | 4 | none |
| bg29 | 66 / 8 | 11 / 0 | 0 | 360 | 30.0 s | 0 | none |

The phong materials do not matter. DFCI draws textured meshes with DefaultShader_Easy, which reads no material, no normals and no lights.

### 2.3 Effect sprites DFCI draws on top of the stage (`object.txt`, not rendered by pc-stage3d)

| Stage | .pat | Entries | prio 0 (behind) / 700 (in front) |
|---|---|---|---|
| bg01 | stage_ef.pat | 12 | 12 / 0 |
| bg02 | sakura.pat | 9 | 0 / 3 (the rest have no prio set) |
| bg04 | yuki.pat | 7 | 0 / 7 |
| bg07 | sonicbg.pat | 6 | 0 / 3 |
| bg08 | 7drbg.pat | 6 | 0 / 3 |
| bg09 | kamihubuki.pat | 6 | 0 / 3 |
| bg13 | yuki.pat | 15 | 8 / 7 |
| bg14 | yuki.pat | 15 | 8 / 7 |
| bg15 | yuki.pat | 15 | 0 / 15 |

### 2.4 pc-stage3d inis (as generated by `gen_uni2_inis.py --dfci`)

Every DFCI ini contains only these keys: `scale = <BgList Scale>`, `pos = <BgList Position>`, `fov = ref_fov = <BgList FOV>`, `lift = 0`, `gain = 1`, `fog = 0`, `stage_w = 4096`.

Every other value comes from the StageParams defaults:

| Default | Value |
|---|---|
| Vertical FOV | `2·atan(tan(FOV/2)·9/16)` (FOV read as horizontal) |
| Camera distance | 10 "UNI2 units", via `f720Ref/10` |
| Fighter height | `fighter_px` 237, `uni_char_px` 355 |
| Screen rows | `floor_px` 432, `horizon_px` 255 (same for every stage) |
| Ground | `ground_auto = 1` |
| Depth parallax | `pan_boost = 0.5` up to `pan_depth = 20` |
| Model transform | `negate_z = 1`, `flip_v = 1`, fit mode `crop` |

VanishingPoint is neither generated nor supported: StageParams has no such key.

## 3. Side-by-side camera numbers

- `pc ppu720` is pc-stage3d's pixels per unit on the fighter plane at 720p: (360/tan(vfov_pc/2))/10, after the fighter scaling (237 px at 480 = 355 px at 720).
- DFCI's value is 360 for every stage.
- `r(z)` = size of the stage geometry at stage depth z in pc-stage3d ÷ size in DFCI, for the same fighter height. `r(z) = r0·10·(Dd+z)/(Dd·(10+z))`, where r0 = pc ppu720 / 360.
- The horizon rows are on MBAACC's 480-row picture. The DFCI horizon is shifted by +5.3 px so that its floor (426.7) lines up with pc's 432.

| Stage | FOV | VP | DFCI cam dist (cot FOV/2) | pc cam dist | DFCI eye h (units) | pc eye h | DFCI horizon @480 | pc horizon | pc vfov (DFCI vfov = FOV) | pc ppu720 (DFCI 360) | r(0) | r(5) | r(20) | r(∞) | Depth where pc = DFCI | Position.x DFCI-equivalent |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| bg01 | 35.6 | .55 | 3.12 | 10 | 0.228 | 1.33 | 377 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | -2.667 (ini -1.5) |
| bg02 | 39.6 | .40 | 2.78 | 10 | 0.378 | 1.49 | 341 | 255 | 22.9° | 178 | 0.49 | 0.92 | 1.35 | 1.78 | 6.5 | -3.022 (ini -1.7) |
| bg04 | 35.6 | .55 | 3.12 | 10 | 0.228 | 1.33 | 377 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | 0 |
| bg05 | 35.6 | .58 | 3.12 | 10 | 0.198 | 1.33 | 385 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | 0 |
| bg07 | 39.6 | .55 | 2.78 | 10 | 0.228 | 1.49 | 377 | 255 | 22.9° | 178 | 0.49 | 0.92 | 1.35 | 1.78 | 6.5 | 0 |
| bg08 | 39.6 | .55 | 2.78 | 10 | 0.228 | 1.49 | 377 | 255 | 22.9° | 178 | 0.49 | 0.92 | 1.35 | 1.78 | 6.5 | 0 |
| bg09 | 35.6 | .55 | 3.12 | 10 | 0.228 | 1.33 | 377 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | 0.889 (ini 0.5) |
| bg13 | 35.6 | .55 | 3.12 | 10 | 0.228 | 1.33 | 377 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | 0 |
| bg14 | 39.6 | .55 | 2.78 | 10 | 0.228 | 1.49 | 377 | 255 | 22.9° | 178 | 0.49 | 0.92 | 1.35 | 1.78 | 6.5 | 0 |
| bg15 | 35.6 | .55 | 3.12 | 10 | 0.228 | 1.33 | 377 | 255 | 20.5° | 199 | 0.55 | 0.96 | 1.37 | 1.78 | 5.7 | -2.667 (ini -1.5) |
| bg16 | 35.0 | .60 | 3.17 | 10 | 0.178 | 1.31 | 389 | 255 | 20.1° | 203 | 0.56 | 0.97 | 1.37 | 1.78 | 5.6 | 0 |
| bg17 | 45.0 | .30 | 2.41 | 10 | 0.478 | 1.72 | 317 | 255 | 26.2° | 155 | 0.43 | 0.88 | 1.33 | 1.78 | 7.3 | 0 |
| bg18 | 37.0 | .30 | 2.99 | 10 | 0.478 | 1.39 | 317 | 255 | 21.3° | 191 | 0.53 | 0.95 | 1.36 | 1.78 | 6.0 | 0 |
| bg29 | 50.0 | .40 | 2.14 | 10 | 0.378 | 1.93 | 341 | 255 | 29.4° | 137 | 0.38 | 0.85 | 1.31 | 1.78 | 8.0 | 0 |

Floor showing between the horizon and the feet at zoom 1:

| | pc-stage3d | DFCI |
|---|---|---|
| VP 0.55 | 177 px | 55 px |
| VP 0.40 | 177 px | 91 px |
| VP 0.30 | 177 px | 115 px |

## 4. Ranked issues (by visual impact)

### 1. The FOV is read as horizontal, but DFCI's is vertical. Focal length is 1.78x too long on every stage (high)

- **What pc-stage3d does:** CameraMap `verticalFovRad()` turns FOV 35.6 into a 20.5° vertical field. DFCI's vertical field is 35.6°: the aspect-1 projection plus the 0.5625 X squeeze is a 16:9 frame with the vertical FOV.
- **Effect:** focal length 1993 px instead of 1121 px at 720p. Everything at depth renders 16/9 = 1.78x too big relative to the fighter plane (`r(∞)` column), so perspective and parallax are much flatter than DFCI's.
- **Note:** the same correction probably applies to UNI2 1.42 (identical camera code). §14's "horizontal at 16:9" reading was fitted to a screenshot together with a fudge scale (`scale 2`).
- **Fix:** add a "uni142" camera mode with vfov = FOV.
- **How to verify:** render bg13 (Virtual-On grid floor) and bg01 at zoom 1, camX 0. Measure the screen spacing of two known grid lines or crates at different depths and compare with `cot(FOV/2)/(cot(FOV/2)+z)`. Or render the same frame with the workaround ini (§5) and compare with a native DFCI screenshot at the same character position.

### 2. Camera distance is 10 units; DFCI's is cot(FOV/2) ≈ 2.1 to 3.2 units. Near/far size ratio is off by 3.2x (high)

- **What pc-stage3d does:** pixels per unit come from the "10 units from the plane" assumption: `f720Ref/10` gives 137 to 203 px/unit at 720p. DFCI's is 360 px/unit at every FOV.
- **Combined with issue 1:**
  - geometry on the fighter plane is drawn at 0.38 to 0.56x DFCI size
  - geometry about 6 units deep comes out the same size
  - geometry 20 units deep is 1.31 to 1.37x too large
  - the far background is 1.78x too large
- **How it looks:** foreground props (bg01 rubble, bg02 cherry tree, bg13 grid) look small and the backdrop looms. Camera pans drag the far scenery almost as fast as the floor.
- **This is the root cause of the "stage looks wrong" look.** It also explains why the generated defaults need retuning on every stage.
- **How to verify:** with `debug_overlay = 1`, compare against a native DFCI capture: the fighter height against a fixed prop on the plane (e.g. bg13's floor tile width, bg07's loop diameter). Expected ratio: DFCI prop px / pc prop px = 1/r0 (1.8 to 2.6x).

### 3. VanishingPoint is ignored; horizon fixed at row 255. Camera 4 to 6x too high above the floor (high)

- **What DFCI does:** the horizon is at 240·(1+VP) on 480 rows: 317 for VP .3, 341 for .4, 377 for .55, 385 for .58, 389 for .6. The eye is only 0.18 to 0.48 units above the floor.
- **What pc-stage3d does:** horizon_px is 255 for all stages, giving an eye height of 1.31 to 1.93 units. pc therefore looks down onto the floor. At VP .55 it shows 177 px of floor between the horizon and the feet where DFCI shows 55 px (3.2x). The bg13 neon grid and the bg29 dome floor in `melty_3d_dfci_b.png` show this clearly.
- **Size of the error:** worst on VP 0.55 to 0.60 stages (bg01, 04, 05, 07, 08, 09, 13, 14, 15, 16: 122 to 134 px too high), least on bg17/bg18 (62 px).
- **Fix:** add a `vanishing_point` key (or emit `horizon_px` per stage from the generator).
- **How to verify:** with `debug_overlay = 1`, check that the horizon line of a level floor (bg13 grid convergence, bg07 checker) sits at 240·(1+VP) + 5.3.

### 4. Position.x is not converted out of squeezed units (medium, 3 stages)

- **The problem:** DFCI applies T(Position) after Scale(Sx·0.5625, ...), so Position.x is in squeezed units: 1 unit = 1.778 world units = 640 px at 720p. pc uses it raw.
- **Error:**

  | Stages | ini Position.x | Should be | Off by |
  |---|---|---|---|
  | bg01, bg15 | -1.5 | -2.667 | 1.17 units |
  | bg02 | -1.7 | -3.022 | 1.32 units |
  | bg09 | 0.5 | 0.889 | 0.39 units |

  At DFCI scale, 1.17 units is 420 px at 720p (280 at 480). The stage's centrepiece is visibly off-centre.
- **Fix:** generator writes `pos.x / 0.5625`.
- **How to verify:** bg01's central gate/door and bg02's tree trunk should sit where they do in a DFCI capture with the camera at x = 0.

### 5. ground_auto snaps the floor on top of Position.y (medium, mainly bg05, bg14, bg18; check all)

- **The difference:** DFCI has no floor snap. The model's floor ends at `floorModel·Sy + Position.y`. pc ends it at `Position.y` (it subtracts the measured floor), so pc is off by −(measured floor) on every stage.
- **Stages with a non-zero Position.y:**

  | Stage | Position.y | At 480 rows (DFCI scale) |
  |---|---|---|
  | bg05 | −0.1 | 24 px |
  | bg14 | −0.4 | 96 px |
  | bg18 | +0.2 | 48 px |

  The authors almost certainly chose these to cancel the model's own floor height, which pc then cancels a second time. On any stage where the area-mode picks a platform rather than the walkable floor, the error is the platform height.
- **Fix:** `ground_auto = 0`, `ground_y = 0` for DFCI.
- **How to verify:** log `resolveGroundY()` per stage (expected ≈ −Position.y where the floor was hand-placed). Toggle ground_auto in the F3 tab and check that the fighters' feet meet the floor seam.

### 6. Stage effect sprites (`object.txt` / `.pat`) are not drawn (medium, 9 stages)

- **What is missing:** snow (bg04, 13, 14, 15), sakura (bg02), confetti (bg09), sparks/embers (bg01), flowers (bg07), bg08 FX. Many have prio 700, i.e. in front of the fighters.
- **Impact:** a large part of the motion and atmosphere on those stages. pc-stage3d has no `object.txt` path.
- **How to verify:** compare a native DFCI capture of bg04/bg14 (falling snow) with pc. This needs a .pat (Hantei-style pattern) player, so it is out of scope for the camera fix.

### 7. `pan_boost = 0.5` adds parallax DFCI does not have (medium; goes away once issues 1 and 2 are fixed)

- **The difference:** DFCI uses pure perspective. pc shifts far geometry by an extra 0.5x of the plane's scroll, fully at z ≥ 20. This partly hides issue 1 today. With the correct DFCI camera (distance ≈ 3), the natural parallax is already 3.2x stronger, so the boost would over-scroll.
- **Fix:** `pan_boost = 0` for DFCI.
- **How to verify:** pan the camera to the stage edge. The far backdrop should move by `Dd/(Dd+z)` of the floor's shift.

### 8. 4:3 crop of a 16:9 framing (low, inherent)

- MBAACC's 640x480 picture covers 75% of DFCI's 1280-wide frame at equal height, so the stage edges visible in DFCI are cut.
- **How to verify:** none; this is a design choice (crop mode).

### 9. Grade, fog, bloom: correct as generated (low; keep)

- **Grade:** DFCI's image is `tex * vcol` with no composite grade, so `lift 0 / gain 1` is faithful.
  - Zanaylo's 1.37 is a correction applied inside UNI2's 0.10 + 0.6708 composite. Net result there: 0.919x with lift 0, i.e. 8% darker than native DFCI.
  - Do not use the UNI2 lift/gain on DFCI.
- **Fog:** DFCI's shader has the lerp commented out, so `fog = 0` is right. Enabling it from BgList would tint bg01/04/05/07/08/09/13/15 bright red (FogColor (2,0,0,1)).
- **Bloom:** off on every stage, same as pc.
- **How to verify:** sample the mean RGB of a flat textured wall in a DFCI capture against pc at the same texel. Expect equal values ±2/255.

### 10. Cull mode and the near plane (low, unverified)

- pc draws with `D3DCULL_NONE`. DFCI's per-frame state in `dfci_stage3d_render` sets only Z-enable, Z-write and Z-func; its cull mode is not confirmed.
- 2 to 27 nodes per stage have negative scale (winding flips). If DFCI culls back faces, pc shows faces DFCI hides (insides of buildings, backs of billboards).
- Near plane: DFCI's is 0.2 at a camera distance of about 3. pc's is D·0.02 = 0.2 at distance 10. Once issue 2 is fixed, set zn to about 0.2/3 of the distance so props close to the camera are not clipped.
- **How to verify:** read D3DRS_CULLMODE (render state 22) in a running DFCI frame (PIX/apitrace), or find the CullMode set in CFbxMesh_Draw.

### 11. Character-height reference is UNI2's (low to medium, unmeasured)

- `uni_char_px 355` is UNI2's bg_chara_height mean. DFCI's on-screen character height at 720p was not measured, so the fighter-relative scale in the §5 ini assumes DFCI fighters are also about 355 px.
- **How to verify:** measure two or three DFCI characters standing in a native capture.

## 5. Ini-only workaround (no code change)

These settings reproduce DFCI's camera with the current CameraMap. Derivation:
- `fov_ini = 2·atan(tan(FOV/2)·16/9)`, so pc's 9/16 conversion gives back vfov = FOV.
- `units_to_px = 240` gives DFCI's 360 px/unit at 720p, scaled to 480. The camera distance then becomes F/240 = cot(FOV/2).
- `horizon_px = 240·(1+VP) + 5.3`.

| FOV | `fov =` |
|---|---|
| 35 | 58.54 |
| 35.6 | 59.43 |
| 37 | 61.49 |
| 39.6 | 65.24 |
| 45 | 72.73 |
| 50 | 79.32 |

| VP | `horizon_px =` |
|---|---|
| 0.3 | 317.3 |
| 0.4 | 341.3 |
| 0.55 | 377.3 |
| 0.58 | 384.5 |
| 0.6 | 389.3 |

Also set:
- `units_to_px = 240`
- `ground_auto = 0`, `ground_y = 0`
- `pan_boost = 0`
- `pos.x = BgList.x / 0.5625`
- keep `lift 0`, `gain 1`, `fog 0`

With this, fighters keep MBAACC's size (about 195 px). DFCI's own are about 237 px at 480 if they match UNI2's 355 @720, so the stage will read about 20% "bigger" than in DFCI. That is the honest price of a 1:1 camera.

The proper fix is a `camera_model = uni142` path in CameraMap, using vfov = FOV, D = cot(FOV/2), 1 unit = H/2, horizon from VP, and Position.x / 0.5625, plus `vanishing_point` emitted by `gen_uni2_inis.py --dfci`.

## 6. Capture path

`/mnt/c/dev/hantei-chan/tools/gamecap` has **no DFCI game or case**: `games/` holds only `mbtl*.gcs`, and `cases/` holds uni2/mbtl cases plus `stage3d_uni2_bg001.gcs`. Native DFCI reference shots for the "how to verify" items therefore need a new gamecap game script (RingGame.exe, windowed via `standalone.cfg`) or manual screenshots. Nothing was run.

## 7. IDA renames (RingGame.exe.i64, saved)

| Address | Name |
|---|---|
| 0x81BD20 | `dfci_camera_update_view_proj` |
| 0x432950 | `dfci_camera_set_target_size` |
| 0x432930 | `dfci_camera_set_fov` |
| 0x432910 | `dfci_camera_set_vanishing_point` |
| 0x4329D0 | `dfci_camera_get_aspect_h_over_w` |
| 0x4329F0 | `dfci_camera_get_half_w` |
| 0x432A10 | `dfci_camera_get_half_h` |
| 0x406B00 | `dfci_camera_get_inv_half_w` |
| 0x406B40 | `dfci_camera_get_view` |
| 0x406B80 | `dfci_camera_get_proj` |
| 0x406BA0 | `dfci_camera_get_screen_mtx` |
| 0x42B800 | `dfci_camera_get_zoom` |
| 0x4323D0 | `dfci_camera_set_zoom` |
| 0x4323F0 | `dfci_camera_get_inv_zoom` |
| 0x81B2C0 | `dfci_get_battle_camera` |
| 0x432310 | `dfci_MatrixPerspectiveFovLH` |
| 0x432340 | `dfci_MatrixLookAtLH` |
| 0x762E90 | `dfci_d3dcam_set_perspective` |
| 0x765570 | `dfci_d3dcam_set_lookat` |
| 0x7E6550 | `dfci_draw_sprite3d_fov65` |
| 0x7ED1A0 | `dfci_stage3d_create_targets` |
| 0x7ECF90 | `dfci_stage3d_camera_from_stagedef` |
| 0x7ED400 | `dfci_stage3d_resolve_capture` |
| 0x73E860 | `dfci_fbx_xmatrix_to_d3d` |
| 0x741450 | `dfci_fbxscene_load_build_root` |
| 0x99AA58 / 0x99AA5C | `g_dfci_stage3d_target_w` / `g_dfci_stage3d_target_h` |

Function comments were added on 0x81BD20, 0x7EE530 and 0x741450.
