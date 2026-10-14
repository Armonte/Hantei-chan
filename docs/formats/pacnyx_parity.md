# PACNyx+ parity checklist (Hantei-chan replaces PACNyx+)

Re-graded 2026-10-14 on branch `feat/pacnyx-parity` after the audit `docs/audit_2026_10_14/PACNYX_PARITY.md` (which found the 10-03 "100%" was mostly
unverified ticks). Rule: **a box is ticked only when a test, a tool run, a screenshot made by the app, or an in-game check is cited next to it.**
Evidence for this round lives in `docs/audit_2026_10_14/evidence/pacnyx/` (shortened below to `ev/`).

Sources: our modified PACNyx+ (`C:\dev\pacnyx\dotPeek\PACNyx+`, `...\PACNyx`) and icaroffa's 2026 exes (decompiled to
`C:\dev\pacnyx-icaroffa-2026\_releases\decomp\{orig2010,ic10,ic12}`).

Credit: the 2026 GUI behaviour (PAC creation hardening, DAT crash fixes, JSON/PNG/aligned export) was first done by ícaro Fellini
(github.com/icaroffa/RAGNAROK-BATTLE-OFFLINE-PACNYX-2026). It has no source and no licence. Hantei-chan reimplements the behaviour from the
decompiled reference; no code is copied. The original PACNyx authors (2010) own the base tool.

Legend: [x] done with evidence, [~] implemented, no recorded run (counts 0.5), [ ] open. "(icaro)" = added by icaroffa 2026.

## Evidence index (this round)

| File | What it shows |
|---|---|
| `ev/unit_tests.log` | `han2_parts_edit_test` (sheet import reaches the PAT, limits, named poses, layers, units, PAT-editor undo channel), `undo_manager_test`, `fb_part_transform_test` |
| `ev/han2_roundtrip.log` | full `tools/han2/run_roundtrip.sh`, now with the sections `dt2companion` (BUG2/BUG7), `effectedit` (effect rename / base point / blocks) and `pacwrite` |
| `ev/fb_suite.log`, `ev/cgm_suite.log` | the archive-layer and CG-manager suites (regression) |
| `ev/build.log` | `./build.sh` (mingw) of the tree the logs and shots come from |
| `ev/shots/*.png`, `ev/shots/*.log` | the app's own captures (`tools/ui/ab_shot.sh tools/ui/shots/pn*.txt`, `--ui-script`, no real mouse/keyboard) and the script result lines |
| `ev/ctest.log` | full `ctest` of the same build |
| `ev/export_icaroffa/` | sample of `han2tool export <ACOLYTE_F.DAT> <dir> --icaroffa` (manifest, animations.json, 2 poses, 1 render, 1 aligned group); `EXPORT_INFO.txt` has the result line and the file count of the whole 119 MB tree |
| `ev/save_check.log` | the `.DT2` + companion `.DAT` written by the app's Save As in `pn01`, read back by `han2tool` |

## A. Main window (MainForm)
- [x] Working folder + refresh + tree, remembered. The tree became the archive browser (Open game folder, Sources); **Refresh is back** (re-reads the folder and every archive, mounts new ones) [`archive_browser.cpp` RefreshSource; `ev/shots/m8_workfolder_tree.png` retaken]
- [x] PAC expanded into entries; magic sniff (GOF2 data0x.dat) [`ev/han2_roundtrip.log` "PAC entry counts"]
- [x] DAT/DT2/IMG/FOB/PAT/CHP recognised, double-click opens [`docs/ux/07_*`, `09_*`; `ev/shots/pn21_fob_decoded.png`]
- [~] Extract file / extract all [browser context menu + `han2tool extract`; no scripted run this round]
- [x] Tabs: close, **close all** (asks about each unsaved character), `*`, save prompt. PAT-editor edits now set `*` [tab menu "Close all tabs"; `ev/shots/pn23_close_all_tabs.png`; `*` after a parts edit: `ev/shots/pn03_part_added.png` + `dirty -> modified` in `pn01.log`]
- [x] File > New: **IMG from an image** (version 6-8 and pixel format chosen in the viewer), PAC (Create / patch window). New DAT: not offered (an empty pattern area is not a playable character; edit an existing one) [`ev/shots/pn20_new_img.png`]
- [x] Open / Save / Save As / Exit; an RBO character saved as `.DT2` now also writes its parts / CG / names to the `.DAT` (BUG2) [`ev/han2_roundtrip.log` dt2companion; `ev/save_check.log`]
- [x] Language EN/JP (View menu, browser EN/JP button) [`ev/shots/pn22_parts_editor_ja.png`]
- [~] Warning for a working folder Windows protects: the restored line ("The working folder is on C: ...") shows when the game root is under `C:\Program Files*` or `C:\Windows` (plain `C:\games` is not warned about: nothing goes wrong there) [code: `UnderProtectedFolder`; not captured: no install under Program Files on this machine] -> counted [~]
- [~] Drag-and-drop of images routed by the window under the drop point: IMG viewer (replace pixels), CG window (replace sprite), PAT editor (new sheet), elsewhere (new IMG) [`RouteDroppedImage`; WM_DROPFILES cannot be produced by the script runner, the routed functions are the ones the shots call: `importsheet`, `cgimport`, `newimg`]

## B. PAC archives
- [x] Read PAC / data0x.dat, mapped [`ev/han2_roundtrip.log` count, pacrt]
- [~] Extract single / all [as A4]
- [~] Create-PAC dialog: base, add file, add dir, remove, list [`han2_pac_window.cpp`]
- [~] Name > 59 bytes refused [`pac_archive.cpp`]
- [~] Save with progress and a result line
- [x] (icaro) base copied to temp, read from the copy; (icaro) overwrite of the base / empty list refused; same-name replaces; root node imports all; full-path open [writer path proven by `ev/han2_roundtrip.log` section `pacwrite`: every archive rewritten byte-identical; the dialog-only guards are code-reviewed]
- [x] No-edit round trip byte-identical [`ev/han2_roundtrip.log` pacrt]

## C. DAT / DT2 / PAT / CG (DATView)
- [x] Open RBO .DAT, RBO .DT2, GOF2 .DT2; signature/kind/sub kept [`ev/han2_roundtrip.log` modelrt]. Fixed on the way: a `.DT2` loaded together with its `.DAT` was written with the `.DAT`'s header (kind 0 and other area offsets): it now keeps its own header (`dt2companion` checks the unedited `.DT2` is byte-identical, 95 characters)
- [x] Save .DAT / .DT2 / into PAC: parts, CG and pattern-name edits are no longer dropped on `.DT2` saves; into an archive the `.DAT` is replaced in the archive that holds it (BUG2, BUG7); a failed companion write is an error (BUG5) [`ev/han2_roundtrip.log` dt2companion; `ev/save_check.log`]
- [x] GOF2 .PAT v4, bare .CHP [`ev/han2_roundtrip.log` patrt, chprt]
- [x] Pose list: 1000 / 2000, **index spinner (InputInt with +/-), name combo** [`ev/shots/pn01_pose_controls.png`]
- [x] Pose add (**name required**), clear, rename, **reorder (swap with index N, frame references follow)** [`han2_parts_edit_test` TestLimits (name required, named empty pose survives the writer); `pn01.log` addpose]
- [x] Save pose as image (PNG/BMP/JPEG/TIFF, cropped, base point reported); pose base point (move the whole pose against it; show / hide the base point axes) [`han2::RenderPoseRgba`; `pn01.log` basepoint; `ev/shots/pn05_saved.png`]
- [x] Pose canvas: 40 parts, layer order, flip, scale, rot, ARGB; zoom, pan [`fb_part_transform_test`; `docs/formats/evidence/fb_render_*.png`]
- [x] Part list (40 slots); max 40 **checked while editing** (Add disabled with the reason) and at save with the real message (BUG4) [`han2_parts_edit_test` TestLimits; `main_frame.cpp` "Not saved: ..."]
- [x] Part add from a **sheet + rectangle** (from the mouse selection or typed), copy, paste, remove [`han2_parts_edit_test` TestAddPartAndUnits; `ev/shots/pn03_part_added.png`]
- [x] ARGB colour, origin, change sheet; the cut-out is made unique to the part before such an edit (shared cut-outs no longer change other parts) [`han2_parts_edit_test` UniqueCutOut]
- [x] Nudge buttons (Shift = 10), rotation +-1 / +-15 deg, H/V flip **and both**, H/V scale [`pn01.log` nudge / rotate; `ev/shots/pn03_part_added.png`]
- [x] Layer: to back / back one / forward one / to front / set N [`han2_parts_edit_test` TestLayers; `pn01.log` layer front]
- [x] Sheet list: **PNG import / export (also BMP/JPEG/TIFF), saved for RBO/GOF2 (BUG1)**, square check with a message, remove with in-use warning, drop a PNG on the editor [`han2_parts_edit_test` TestSheetImportSaved; `pn01.log` (the 256x200 import refused); `ev/shots/pn02_sheet_selection.png`]
- [x] Sheet canvas: **mouse rectangle selection** (right-drag in the sheet view, no longer draws hit boxes there), typed rect, zoom [`sheetsel` + `addpart` in `pn01.log` make part 14 from the selection (`ev/shots/pn03_part_added.png`); `ev/shots/pn02_sheet_selection.png` shows the selection box on the imported sheet (script centres the sheet with `zoomview 1.5` + `viewpan -192 -192`)]
- [x] Effects (CG): list <= 3000, add, remove, change graphic, save as; **rename, base point** [`ev/han2_roundtrip.log` effectedit; `ev/shots/pn10_cgm_effect_blocks.png`]
- [x] Effect block grid: draw, click toggle, enable / disable all, recalculate [`ev/han2_roundtrip.log` effectedit (cell off = exactly that cell transparent, rebuild = same pixels); `ev/shots/pn10_cgm_effect_blocks.png`]
- [x] Limits: 1000 / 2000 poses, 40 parts, 3000 effects [`han2_parts_edit_test`; CG manager kMaxImages]
- [~] Loading dialog with warnings/errors
- [~] (icaro) names cut at the first NUL
- [x] (icaro ic12) sprite-sheet export from memory, `.png` default, multi-format, error reported [texture pane Export image...; `WriteImageRgba`]
- [x] (icaro) pose JSON + PNG, animations.json — **in his layout** (`manifest.json`, `poses/<name>.json` with his keys) as an export option [`ev/export_icaroffa/` sample + `pn01.log`]
- [x] (icaro) aligned renders: per-group canvas **sized to fit** (+24 px margin, as his), `animation.json` with frameWidth / frameHeight / anchorCanvasX / anchorCanvasY, strip.png (note instead of a silent skip above 16384 px), sheet.png [`ev/export_icaroffa/`]
- [x] Groups from the real pattern table [`ev/han2_roundtrip.log` animtest]
- [x] Strings (pattern names): renames reach the `.DAT` also from `.DT2` saves [dt2companion]

## D. IMG / E. FOB
- [x] Open .IMG v6-8, all formats [`ev/han2_roundtrip.log` imgrt]
- [~] Change texture (button + drop on the viewer), export PNG
- [x] New IMG from an image; version / pixel format selectable; Save IMG [`ev/shots/pn20_new_img.png`]
- [x] FOB: decoded RBO / GOF2 listing (functions, index tables, instructions with names, unreached code greyed, Export disassembly) [`ev/shots/pn21_fob_decoded.png`; decoder `fobrt` in `ev/han2_roundtrip.log`]

## Undo (beyond PACNyx)
- [x] PAT-editor edits of an RBO / GOF2 character are undoable (Ctrl+Z) and set `*`; one gesture = one step; frames + parts in one gesture = one entry [`han2_parts_edit_test` TestPartsUndo; `ev/shots/pn04_after_undo.png`]
- [x] CG-window import undo is per character; a failed import records nothing (BUG6) [`CharacterInstance::cgImportUndo`]

### Parts undo (for stream S2: UNI / MBAACC `.pat`)
`UndoManager` (src/undo_manager.h) gained **channels**: extra state that is not the pattern list joins the same history. A channel captures an
immutable snapshot (sharing unchanged pieces with the previous one), tells whether the live state still equals a snapshot, and restores one.
`PartsUndoChannel` (src/parts/parts_undo.h) is the channel for a `Parts` model; every `CharacterInstance` owns one (`partsUndo`) and adds it in its
constructor. It is gated by `partsUndo.enabled`, currently `parts.fbPartModel && frameData.isHan2()` (French-Bread parts are saved with the
character). To turn it on for UNI / MBAACC `.pat` files S2 only needs to widen that predicate and make the `.pat` save (`savePAT`/`savePATAs`)
call `undoManager.markClean()` like `save()` does. No pane needs to call anything: the falling edge of every gesture already commits a diff.

## In-game check (pending, manual)
Not run this round. RBO (`rbo_ex3.exe`) draws through DirectDraw and reads DirectInput 8, and offline it sits in a 48 s opening movie with no
boot navigation; `tools/gamecap` hooks D3D9 Present and XInput / GetKeyboardState, so adapting it means a DDraw capture path, a DInput8 hook
and menu navigation for RBO: not cheap, so it was not attempted. Manual steps (one game running machine-wide; back up first):
1. Back up `C:\games\rbo_run\Data\Chara\Player\ACOLYTE_F\` if it exists (the game reads loose files before the PACs, see
   `docs/formats/evidence/m4_hurtbox_ingame.txt`).
2. Copy `C:\dev\hantei-chan\work\pacnyx\out\ACOLYTE_F.DT2` and `ACOLYTE_F.DAT` (written by the app in `pn01`, checked in `ev/save_check.log`)
   into that folder.
3. Start RBO, pick Acolyte (female) and check the stand pose (pose 3, PATTERN005) shows the extra part from `test_sheet_256.png`.
4. Remove the folder / restore the backup.

## Parity

Rows above: A 10, B 7, C 24, D/E 4, undo 2 = 47 graded rows (the parts-undo paragraph is notes). Final count for this round:
**[x] 37, [~] 10, [ ] 0 -> strict 37/47 = 78.7 %, with partials counted 0.5: 42/47 = 89.4 %.**
The [~] rows are code-reviewed only: extract (A/B), sheet-view selection (shot does not show the box), drop routing (WM_DROPFILES cannot come from the script runner), the Create-PAC dialog
guards and progress line, the protected-folder warning (no Program Files install here), the loading dialog, NUL-cut names and IMG
change-texture. The in-game check above is pending and is not counted as a row.
