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
| 9 | Right pane AT | ImGui's value-then-label convention with mixed multi-field rows ("Weak (6f) Hitstop 0 Cust HS 0 Cust BS"), label column not aligned between rows; JA labels clip at the pane edge at 1280 ("ガードフラグ") | alignment | **Fixed (round 2)** for AT / AS / AF: `ui::BeginLabelForm / FormRow` (label column sized to the longest label, capped at 42% of the pane, wraps with a full-text tooltip on overflow), one label column per display, sub-headers (Hitstop, Damage, Proration, Vectors, Effects; Flags, State; Sprite, Animation, Transforms). Guard/Hit vector rows, UNI VectorTable names and JA hint kept. IF / EF smart UIs (effects/*.h, frame_disp_if.h, ~280 call sites) still value-then-label: open |
| 10 | Left pane | Empty region below the collapsed trees (no fill) at every size | empty region | **Fixed (round 2)**: a Pattern list (clipper, follows the current pattern, click = same as the Pattern combo) fills the height left under the collapsed trees; nothing opens by default |
| 11 | CG manager | Fixed widths (Filter 160, Min/Max side 70, Thumbnail 120, Palette 90) do not follow the font; JA labels of "Min side / Max side" crowd the filter row | fixed px | Open (cgm_window.cpp left for the CG manager follow-up) |
| 12 | Script panel (Squirrel) | Three columns fixed (file list, editor, outline) leave the editor at ~45% of the window at 1280 | proportion | **Fixed (round 2)**: resizable table 18 / 64 / 18, Files / Outline toggles fold the side columns, editor takes the rest |
| 13 | Box pane | Box-type colour strip and the "Copy all / Paste all / Copy params ..." row wrap unevenly in JA at 1280 (two buttons per line) | wrap | **Fixed (round 2)**: `ui::ButtonGrid` (src/ui/ui_layout2.h) equal-width cells, even rows; checkboxes on their own wrapping row. Colour strip unchanged |
| 14 | Archive browser | Fine at all three sizes; the preview column keeps its proportion | - | No change |
| 15 | han2 inspector / pac window / tool windows (tool_windows_impl.h) | Fixed `SetNextItemWidth(N)` in many places (cgm 29, tool windows 18, han2 15, fxr 15, png export 14, hmod 13) | fixed px | **Fixed (round 2)**: every positive `SetNextItemWidth/PushItemWidth(N)` in tool windows, png export, han2 pac / inspector, hmod, fxr (+ bg inspector, tag, authoring, pcmod export, cmd editor, game link, workspace hosts) goes through `ui::Px`. Some ImVec2 child sizes and the han2 frame-strip cards stay fixed |

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

## Remaining

- Item 9 for IF / EF (frame_disp_if.h and effects/*.h smart UIs).
- Item 11 (CG manager, after fix/pat-ingame-proof).
- ImVec2 child sizes in tag / authoring / bg inspector / cmd editor, han2 frame-strip cards.
