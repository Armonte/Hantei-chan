# French-Bread part (pose) transform: RBO / GOF2 / GOF1

How the three engines place the parts of a PAT pose on screen, decoded from the executables, and what Hantei-chan's GL renderer does with it.
Everything here was read in IDA; the regression test is `tests/fb_part_transform_test.cpp`.

Binaries: RBO = `rbo_ex3.exe` (`C:\dev\frenchbread\rbo\rbo_ex3.exe`; the rbo.exe addresses in `ida/rbo_scripts_pat.md` are the same code at other addresses),
GOF2 = `GOF2.exe.i64`, GOF1 = `gof.exe.i64`. The engine is the same in all three (same 92-byte part record, same 1/10000-turn angles).

## 1. The model (per part, 92-byte record, fields in `ida/rbo_scripts_pat.md` section 3.3)

| Item | Decoded behaviour | RBO (ex3) | GOF2 | GOF1 |
|---|---|---|---|---|
| position `x,y` (+0x00/+0x04) | the quad's **top-left** corner, pixels, +x right, +y down, relative to the actor anchor | `PosePart_LerpPosition` 0x44DC70 | `PosePart_ComputeGeometry` 0x43FE50 | `Render_QueueFighterSprite` 0x42FFE0 |
| origin `ox,oy` (+0x40/+0x44) | the **pivot offset from the top-left**: scale and rotation are about `(x+ox, y+oy)` | `PosePart_LerpOrigin` 0x44DBA0 | same | same |
| quad size | `dest_w * sx`, `dest_h * sy` (truncated to int), `sx = scale_permille * 0.001` | `PosePart_LerpScale` 0x44DC00, `PoseQuad_FillDrawDesc` 0x44E150 | same | same (`* 0.001`) |
| scale | about the pivot: corner = pivot - ox*sx (so top-left = `x + ox - ox*sx`) | `PosePart_LerpPosition` | same | same |
| rotation `rot` (+0x1C) | 10000 = one turn, `deg = rot * 0.036` (0.035999998f); lerped by the shortest arc (wrap at 5000) | `Math_LerpAngle10000ToDeg` 0x44D990 | same constant | `sub_42FEF0` |
| rotation direction | **clockwise on screen** (y down): `x' = x cos a - y sin a`, `y' = x sin a + y cos a`, about the pivot | `Mat_RotateAboutPivotDeg` 0x413350 (row-vector matrix, `m0 = cos, m1 = sin, m4 = -sin, m5 = cos`) | `sub_43F850` | - |
| flip bits (+0x10) | UV only: `src_x += src_w - 1; src_w = -src_w`; the quad does not move | `PosePart_GetFlippedSrcRect` 0x440A90 | same | - |
| draw order | ascending `(layer << 8) + slot` (`layer` = byte +0x3C); the **last part is on top**; unused slots (`src_w == 0`) and clip parts (texture 0xFFFF) are skipped | `Pose_CollectSortedParts` 0x44E030 (selection sort, ascending) | `DrawSort_BubbleByPriority` 0x43FAA0 (same keys) | - |

The quad is submitted (`Pose_SubmitPartQuad` 0x44E5B0) as pivot-relative corners `(0.5 - sx*ox, 0.5 - sy*oy)` .. `+ (w, h)`, rotated by the part matrix
(`Mat_RotateAboutPivotDeg`: rotation with translation = pivot), then run through the frame matrix below.
The 0.5 is the D3D9 pixel-centre shift; sizes and the scaled origin are truncated to integers, so a scaled part can differ from a float renderer by up to ~1 px per term.

### Frame matrix (what is applied to the pose as a whole)

`Camera_BuildSpriteWorldMatrix` 0x441CD0 (RBO) / `sub_43F850` (GOF2) build, for a column vertex `v` already in actor space (matrices are row-vector,
`Mat4_MulRowVec` 0x412B40 = `out = a3 * a2`, so the LAST argument acts first):

```
world = Scale(zoom) * Rot(facing + frame rot X/Y/Z) * Translate(frame offset) * Rot(frame flip mode) * (part pose)   + actor position
```

* **frame flip mode** (`flipMode`, frame +0x08) is a rotation vector, table filled by `sub_4154C0` 0x4154C0 from the draw-mode keys `0x48D540`:
  FLIP_H = (0,180,0), FLIP_V = (180,0,0), ROT_90 = (0,0,90), ROT_180 = (0,0,180), ROT_270 = (0,0,270), FLIP_H_ROT_90 = (0,180,90), FLIP_H_ROT_270 = (0,180,270) degrees.
  The component order is Z, then X, then Y. It is applied **before the frame offset**, about the actor anchor, so a mirrored frame does not mirror its own `offsetX/Y`.
* **facing** (left) is the same kind of vector `(0,180,0)` but is applied **after** the offset (it mirrors the offset too). Hantei-chan only draws right-facing, or mirrors through the scale.
* **zoom** (frame `zoom` / 256, `FXF_USE_ZOOM`) scales everything including the frame offset, about the anchor.
* the actor position is added last; then `+320, +448` (640x480 screen, anchor = bottom centre).
* ROT_FREE modes (8, 9) take their angle from the actor (`actor+704`, set by scripts), not from the frame record, so a static frame shows them unrotated.

## 2. What was wrong in Hantei-chan

`han2::PatToParts` (src/han2_pat.cpp) stores `x,y` raw and the cut-out origin as `xy`; the GL part renderer (`Parts::Draw`) was written for MBAA/UNI where the part
position IS the pivot and the cut-out hangs at `-xy` from it. For French-Bread data that put every part `(ox, oy)` away from where the game draws it, and rotations swung about
the wrong point: heads floated off bodies, limbs spread apart ("spacing"), rotated parts (most parts of an RBO pose are rotated by a few degrees) landed beside their joints.
Three separate errors:

1. **Pivot**: position was used as the pivot. Fixed: `Parts::fbPartModel` -> pivot = `x + xy[0], y + xy[1]` (`partxf::Pivot`, `src/parts/part_transform.h`).
2. **Draw order**: Hantei draws the highest `priority` first (MBAA convention); French-Bread draws ascending, last on top, so layered parts (hair behind face, sword in front of arm) were inverted.
   Fixed: ascending for `fbPartModel` (`partxf::SortForDraw`).
3. **Flip vs offset order**: the layer matrix rotated the frame offset together with the flip (`scale * rot * translate(offset)` reversed), the game flips first and offsets afterwards.
   Fixed for PAT-drawn frames: `scale(zoom) * translate(offset) * Ry * Rx * Rz` when the layer's parts are French-Bread (`DrawPatLayerItem`, `src/render.cpp`).
   Frames drawn from a CG image (sprite id >= 10000: 231 RBO and 15 GOF2 frames use a flip mode) still go through the old MBAA order; their canvas position is not part of the model, so the
   flip-before-offset order cannot be applied without more work. Not changed, listed as a known gap.

The model, the writer (`han2_pat_write.cpp`) and the CPU compositor used by `han2tool export` (`han2_export.cpp`, which already used `pivot = x + ox` and ascending order) are untouched, so every byte
round trip is unaffected (see the suite results in the commit notes).

## 3. Verification

* `tests/fb_part_transform_test.cpp` (ctest `fb_part_transform`, Windows exe; `fb_part_transform_test.exe <DAT>...` also checks every part of every pose of the files given):
  re-implements the engine arithmetic from a raw 92-byte record (truncation included) and compares the 4 quad corners with the matrix `Parts::Draw` builds, plus the draw order against
  `Pose_CollectSortedParts`. Synthetic poses (origins, scales 60..150 %, rotations 15/270/355 deg, layer ties) always run; a negative control asserts the old reading is >= 4 px off.
  `ACOLYTE_F.DAT`: 486 poses, 6603 corner and order checks, 0 failures (tolerance 1.01 px unscaled, 3.01 px scaled).
* Screenshots (the app's own `--capture-view`, no desktop capture): `docs/formats/evidence/fb_render_*.png`, left to right: before, after, and the `han2tool export` compositor (engine formula) as reference.
  Silhouette overlap (alpha IoU) of ACOLYTE_F pattern 0 frame 0 against the reference compositor: **0.53 before, 0.98 after**.
  `fb_render_gof2_date_p0f0.png` adds a frame from the real game (`gof2_live_battle_story.png`, Futaba Date in the same guard stance).
* No in-game capture of the exact frames: the RBO install has no PovertyCaster seam and a game cannot be driven to a chosen pattern/frame without scripting it; the GOF2 comparison above is an
  in-battle frame in the same stance. The ground truth for the numbers is the decompiled engine code above.
* Cross-check with PACNyx (`C:\dev\pacnyx\dotPeek\PACNyx\PACNyx\DAT.cs` lines 711-718): `Translate(x, y); Translate(ox, oy); Rotate(rot/10000*360); Scale(sx, sy); Translate(-ox, -oy)`
  is the same pivot / scale / rotation model (GDI+ positive angles are clockwise on a y-down canvas).

Not covered: parts with texture 0xFFFF are clip rectangles the ex3 engine does apply (`sub_44E590`/`sub_44E5A0` scissor around the draw loop in `Actor_DrawPartsPose`); Hantei-chan skips them (it used to as well).
