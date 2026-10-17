# Layout audit, 2026-10-16

Sweep of the editor's panes and default layout for wasted space, uneven spacing and poor panel layout.
Branch `ui/layout-sweep` (cut from `update/parity-1014`).

## How the shots were taken

- `tools/ui/shots_layout/L_all.txt` drives the in-app `--ui-script` runner (headless, the app's own GL frame:
  window only, no desktop capture, no global input). It covers the MBAACC character view (EN + JA), the CG manager,
  Preferences (EN + JA), the UNI2 character view, the Squirrel Script panel (UNI2), the MBTL character view, the
  archive browser (RBO), the RBO / GOF2 PAT editor (EN + JA) and the 3D Stage view (UNI2 bg001, EN + JA).
- `tools/ui/shots_layout/run_layout.sh <tag> [exe]` runs it at 1900x1000 (ab_shot default), 1280x720 and
  1920x1080 in parallel into `C:\dev\hantei-chan\work\ab_layout_<tag>_<size>\`.
- ActCadenza has no pane of its own outside the CG manager (moon-3 data / preset); it shares the MBAACC layout.
- Before/after sheets (shrunk): `docs/ui/evidence/layout_2026-10-16/`.

## Findings, ranked by visibility

| # | Pane | Finding | Kind | Status |
|---|------|---------|------|--------|
| 1 | Floating tool windows (3D Stage 900x640, CG manager 1200x820, Script 1000x700, cmd editor 1100x720, tag 760x720, prefs 620x720, ...) | First-use size is a fixed pixel size placed at the top left; at 1280x720 the 3D Stage, Script and CG manager windows run off the bottom of the screen (the 3D picture and the Script log are cut off) | fixed px, clipped | **Fixed**: `ui::FitNextWindow` clamps to 92% x 90% of the work area, scales with the font, and centres the window (33 windows) |
| 2 | 3D Stage | Parameter rows are value-then-label (`[0.000] cam x (px) [0.000] cam y (px) ...`) in fixed 110-160 px fields; inconsistent with the label-first source row; the row runs off the window ("Reload from Blender" clipped) and the JA labels push fields to the next line unevenly | label/value inconsistency, fixed px | **Fixed**: label-first `ui::FormGrid` (table, reflows 1-5 cells per row by width), source row label-first, buttons wrap with `SameLineFit` |
| 3 | 3D Stage | Picture left-aligned with a dead band on the right; stats line is one 160-character line that clips | empty region | **Fixed**: picture centred (both normal and MBAACC alignment views); stats wrapped, dimmed |
| 4 | 3D Stage | MBAACC alignment help sits in a hover on the checkbox only; Preferences > Blender help is a full-width line | long help text | **Fixed**: `(?)` markers |
| 5 | Main dock (character views) | Left pane takes 30% of the width for a pattern list and four collapsed trees, mostly empty; the Box pane (bottom, 25%) hides its Timeline section at 1280x720 and shows only its first rows at 1080p | wasted space, panel doesn't fit content | **Fixed**: split 24% / view / 44% right, Box pane 34% of the height (same for detached workspace hosts) |
| 6 | PAT editor (RBO / GOF2 / MBAACC .pat) | Tool pane (top right, 60% of the centre column) holds one help line; PartSet pane buttons clip ("Cle...", "Update fr...") in its 30% column; Shape pane is one button over a large empty area | wasted space, clipped | **Fixed** (dock): PAT layout keeps the left column at 30% and gives the Tool pane 36% instead of 60%. Shape pane emptiness remains (content is per-part) |
| 7 | Right pane (AT / AS / AF / IF / effects) | All field widths are fixed px (`constexpr float width = 75.f` etc.), so a larger font (Preferences font size, high DPI) truncates every number | fixed px | **Fixed**: widths follow the font (`ui::Px`) |
| 8 | Preferences (Saving and PAT editor) | Two full-width disabled help lines under Backup folder and one under Mouse buttons; fixed 120 / 260 px fields | long help, fixed px | **Fixed**: `(?)` tooltips, widths scale |
| 9 | Right pane AT | ImGui's value-then-label convention with mixed multi-field rows ("Weak (6f) Hitstop 0 Cust HS 0 Cust BS"), label column not aligned between rows; JA labels clip at the pane edge at 1280 ("ガードフラグ") | alignment | **Fixed (round 2)** for AT / AS / AF: `ui::BeginLabelForm / FormRow` (label column sized to the longest label, capped at 42% of the pane, wraps with a full-text tooltip on overflow), one label column per display, sub-headers (Hitstop, Damage, Proration, Vectors, Effects; Flags, State; Sprite, Animation, Transforms). Guard/Hit vector rows, UNI VectorTable names and JA hint kept. IF / EF smart UIs **fixed (round 3)**: frame_disp_if.h on one label column per condition (Type row label-first, Parameters sub-header, notes wrap, footer `ui::ButtonGrid`); effects/*.h through `effects/effect_form.h` (`EfRow` / `EfComboManual` / `EfSection`: label column = widest effect label, capped at 42%, `"--- X ---"` lines became sub-headers, X/Y pairs one row each); UNI2/MBTL label-table params (game_label_ui.h) as label rows |
| 10 | Left pane | Empty region below the collapsed trees (no fill) at every size | empty region | **Fixed (round 2)**: a Pattern list (clipper, follows the current pattern, click = same as the Pattern combo) fills the height left under the collapsed trees; nothing opens by default |
| 11 | CG manager | Fixed widths (Filter 160, Min/Max side 70, Thumbnail 120, Palette 90) do not follow the font; JA labels of "Min side / Max side" crowd the filter row | fixed px | **Fixed (round 3)**: all four tabs font-relative and label-first; toolbar buttons wrap; long UNI2/MBTL and effect-recolor help in `(?)`; Browser grid / inspector and Effect recolor list / preview are resizable splits filling the height; CG + .pat textures tab: CG list, .pat list (only the rows it needs) and Used-by on the left, actions grid + zoom + preview filling the right (resizable 46 / 54); palette tools and save rows as `ui::ButtonGrid` |
| 12 | Script panel (Squirrel) | Three columns fixed (file list, editor, outline) leave the editor at ~45% of the window at 1280 | proportion | **Fixed (round 2)**: resizable table 18 / 64 / 18, Files / Outline toggles fold the side columns, editor takes the rest |
| 13 | Box pane | Box-type colour strip and the "Copy all / Paste all / Copy params ..." row wrap unevenly in JA at 1280 (two buttons per line) | wrap | **Fixed (round 2)**: `ui::ButtonGrid` (src/ui/ui_layout2.h) equal-width cells, even rows; checkboxes on their own wrapping row. Colour strip unchanged |
| 14 | Archive browser | Fine at all three sizes; the preview column keeps its proportion | - | No change |
| 15 | han2 inspector / pac window / tool windows (tool_windows_impl.h) | Fixed `SetNextItemWidth(N)` in many places (cgm 29, tool windows 18, han2 15, fxr 15, png export 14, hmod 13) | fixed px | **Fixed (round 2)**: every positive `SetNextItemWidth/PushItemWidth(N)` in tool windows, png export, han2 pac / inspector, hmod, fxr (+ bg inspector, tag, authoring, pcmod export, cmd editor, game link, workspace hosts) goes through `ui::Px`. Remaining ImVec2 child sizes and the han2 frame-strip cards **fixed (round 3)** |

## Shared helper

`src/ui/ui_style.h`:
- `ui::Px(px)` design pixels at the current font size (the hand-tuned values were picked at 18 px), `ui::Em(n)`.
- Tokens: `FieldW`, `WideFieldW`, `PathFieldW`, `FormCellMinW`, `GroupGap`.
- `ui::FitNextWindow(w, h)`: first-use window size clamped to the screen and centred.
- `ui::HelpMarker(text)`: the `(?)` tooltip for long help.
- `ui::BeginFormGrid / FormCell / EndFormGrid`: label-first cells in a table that reflows by width.

## Round 2 (branch `ui/layout-sweep2`)

- PartSet pane (item 6): Clear pose / Update frame references / part buttons wrap with `ui::SameLineButton` instead of clipping (RBO ACOLYTE_F, `tools/ui/shots_layout/L_pat.txt`, since `C:\games\rbo_run` used by L_all.txt is gone).
- Showcase crop boxes (`docs/showcase_2026-10-16/scripts/ui/crop_features.py`) re-measured for the centred windows; feature shots re-run and re-cropped.
- Before / after sheets: `docs/ui/evidence/layout2_2026-10-16/` (`make_sheets.py <out> before2 after2`).

## Round 3 (branch `ui/layout-sweep3`)

- Item 9 (IF / EF) and item 11 (CG manager): see the table.
- Fixed pixel sizes left after round 2: han2 frame-strip cards (card, picture and label follow the font), tag window (its own
  SetNextWindowPos fought FitNextWindow), authoring crash log / Load in game / SE browser / win-quote file list / HUD layout
  split, bg inspector object and frame lists, bg browser thumbnails, cmd editor detail split, pac table, and the last literal
  `SetNextItemWidth(N)` in archive browser, box pane, menus, PAT editor, stage card, pattern picker popup.
- Left pane pattern list (item 10): the selection follows the current pattern both ways; it scrolls only when the row is out
  of view and never while a popup (the Pattern combo's keyboard navigation) or the list itself has keyboard nav; per-pane
  state instead of a function static shared by every open character.
- `ui::ButtonGrid` placed its cells with `SameLine(x)`, which adds a table column's offset twice: a grid inside a table cell ran
  off the pane. It now positions cells in screen space.
- Shots: `tools/ui/shots_layout/L_r3.txt` (IF+EF on akiha p73 f5, every CG manager tab on UNI2 chr020), `L_r3if.txt`
  (Conditions alone, UNI2 label-table forms), `run_layout.sh <tag> [exe] [script] [sizes]`,
  `make_sheets.py <out> <before> <after> [dir suffix]`. Sheets: `docs/ui/evidence/layout3_2026-10-16/`
  (L* = L_all / L_pat round 2 after vs round 3, R* = round 3 scripts before vs after; 1280x720 and 1920x1080, EN + JA).

## Round 4 (closed 2026-10-16)

- BOF / Extended helpers (`bof_extensions.h`: DrawEffect154, DrawVar6Presets, and IF 14 filters / IF 154-157) are
  label-first `effect_form.h` rows: `EfSection` sub-headers, `EfRow` fields, `EfComboManual` combos, the bit-mask
  checkbox groups as one row each (wrapping in the value column), queue / command names as `EfNote`, warnings wrapped.
  Every field, its validation and the presets are unchanged. **Closed.**
- UNI2 / MBTL label-table forms (`game_label_ui.h`): effect Number and parameter rows now use the effect form's label
  column (the Type row's), so Type, Number and p1..pN line up; condition parameter rows share the condition's Type
  column (`GameConditionLabelColW` folded into frame_disp_if.h `lw`). Long labels were cut at 160 bytes ("... 1024 stage
  cent"): buffers raised (and `ui::HiddenId` to 400). The spawned pattern / IF type name after a field drops under it
  when it does not fit (it clipped at 1280 in Japanese). Shot frames: UNI2 chr017 p110 f16 and MBTL chr015 p170 f6 (two
  conditions, two effects each; HA6 IFTP / EFTP scan). **Closed.**
- CG + .pat textures tab: "Used by" is sized to its rows (one row minimum, at most a quarter of the column / 12 em, then
  it scrolls); the CG list takes the rest. **Closed.**
- Game Link window: `ui::FitNextWindowToContent` (ui_style.h) auto-resizes it on its first appearance only (no imgui.ini
  entry, not created this session; 420..760 design px), centred like FitNextWindow; a size the user set is kept. Log
  child height font-relative. The UNI2 / MBTL hot-reload row (Squirrel panel) wraps before an item that no longer fits,
  and the panel status line moves to its own wrapped line instead of clipping. **Closed.**
- Shots: `tools/ui/shots_layout/L_r4.txt` (UNI2 / MBTL IF + EF, CG + .pat tab, hot-reload row), `L_r4if.txt`
  (Conditions alone), `L_r4gl.txt` (run with `--tool gamelink`). Sheets: `docs/ui/evidence/layout4_2026-10-16/`
  (1280x720 and 1920x1080, EN + JA). Showcase: `feat_cgm_uni2_textures`, `feat_gamelink_panel` re-cropped,
  `feat_hotreload_uni2` added (crop_features.py boxes updated).

## Remaining

- None from this sweep.
