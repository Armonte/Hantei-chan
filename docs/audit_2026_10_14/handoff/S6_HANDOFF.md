# HANDOFF: stream S6, PACNyx+ / icaroffa-2026 parity (branch feat/pacnyx-parity, worktree wt/pacnyx)

Base fe01ac3 (update/parity-1014 before quill). 16 WIP commits d6922f3..b2b5267 (+ this one). Nothing pushed. update/parity-1014 now
has quill (0f4a5dc, src/app_log.h): **not merged here yet**. New shipped code adds no printf/logging. Pre-existing printfs remain
(parts.cpp "[COLOR DEBUG]", render.cpp); some were removed in the PAT panes.

## Done (code + where it is proven)
- BUG1: sprite-sheet PNG import reaches the saved PAT (`han2_parts_edit.cpp` ImportSheetRgba, owned pixels `Parts::AdoptPixels`;
  writer `han2_pat_write.cpp` drops removed sheets, refuses non-square / non-32-bit). Test: `build/han2_parts_edit_test.exe` (71 checks, 0 fail).
- BUG2/BUG7/BUG5: RBO saved as .DT2 writes `<stem>.DAT` with parts/CG/names (`framedata_han2.cpp` BuildDatCompanion/NamesChanged/
  RefreshNamesArea; `han2_character.cpp` SaveCompanions/MarkDatCompanionWritten; `archive_browser.cpp` SaveCharacterIntoArchive replaces the
  .DAT in the archive that holds it, ArchiveHoldingEntry). Also fixed: a .DT2 loaded with its .DAT was written with the .DAT header
  (container `hasDt2Header/dt2Header/dt2AreaOff`). Test: `han2tool dt2companion <RBO PACs>` 95/0 (section in run_roundtrip.sh).
  End-to-end via the app: `pn01` script Save As -> C:\dev\hantei-chan\work\pacnyx\out\ACOLYTE_F.DT2 + .DAT written.
- BUG4: `CharacterInstance::lastSaveError()` shown in Save Error popup ("Not saved: ...").
- BUG6: CG-window import undo per character (`CharacterInstance::cgImportUndo/Redo`), failed import records nothing.
- Undo channels in `UndoManager` (generic, `UndoChannel`), `parts/parts_undo.{h,cpp}` PartsUndoChannel; CharacterInstance owns
  `partsUndo`, gated `parts.fbPartModel && isHan2()` (S2: widen the gate + markClean in savePAT). Documented in docs/formats/pacnyx_parity.md.
- PAT editor (French-Bread): `PatEditor/pat_partset_fb.cpp` (pose spinner, named add, clear, swap+frame-ref remap, base point, save pose
  as image, 40-slot list, add part from sheet selection/typed rect, copy/paste/remove, nudge, rotate ±1/±15, flip both, layer cmds,
  per-part cut-out made unique); `pat_texture_pane.cpp` DrawFbSheets (PNG/BMP/JPG/TIF import/export, square check, remove w/ in-use).
  FB source rects are 1/256 sheet units (PxToUnits/UnitsToPx). Mouse sheet selection: right-drag in sheet view (`UpdateSheetSelection`),
  sheet drawn top-left at world origin, selection drawn as hitbox key 25 (`ui/view_render_impl.h` HAN2 pat-editor branch).
  Fixed: PAT editor no longer rewrites pattern 0 frame 0 / sequence 0 of HAN2 characters (panes guarded by framedata->isHan2()).
- Effects (CG manager): `cgm_struct.cpp` RenameImage/OffsetImage/CellMask/ReblockImage (bounds are ANCHOR-RELATIVE, may be negative);
  UI `cgm_window.cpp` drawEffect + block overlay. Test: `han2tool effectedit` (last full run 435/1; the 1 fail = limit, fixed in b2b5267, not rerun).
- Export: icaroffa layout (`han2_export.cpp` ExportIcaroffa, ExportOptions::icaroffaLayout, `han2tool export --icaroffa`, menu item);
  our layout now fits canvases per pattern/pose (canvasW=0), reports write errors, strip>16384 noted.
- New IMG from image + IMG version/format controls; image drops routed by window (`RouteDroppedImage`, DragQueryPoint in main.cpp);
  decoded RBO/GOF2 FOB viewer (`fob::Mnemonic`); Close all tabs (`ContinueCloseAll`); browser Refresh/Up/Down/Unmount + protected-folder
  warning (only Program Files / Windows); i18n rows in `src/i18n_ja_pacnyx.inc` (i18n_check missing=0).
- ui-script commands `ui/pacnyx_script_impl.h` (partseditor, pose, importsheet, sheetsel, addpart, layer, undo, dirty, savechar-as,
  cgm/cgmselect, cgimport, newimg, exporticaroffa, closealltabs, lang ...). Scripts: tools/ui/shots/pn01/pn10/pn20/pn22.

## Session 2 (2026-10-14 16:56-18:30) state
- Merged update/parity-1014 twice (2c2e063 quill/imgui/S1/CI; c660f62 per-worktree shot folders work/ab_pacnyx). Conflicts: CMakeLists
  (kept han2_parts_edit_test + part_render_math_test; han2_parts_edit_test now links src/app_log.cpp) and i18n.cpp (both .inc).
- Evidence retaken on that build (890fe56): build EXIT 0; han2_roundtrip all fail 0 (effectedit 436/0, dt2comp 95/0, pacwrite 18/0);
  fb_suite exit 0; unit_tests.log all pass; ctest 24/24; save_check.log OK; export_icaroffa/ sample; shots quantized (evidence 1.7 MB).
- cgm_suite.log: finished, suite exit 0 (bank 72/0, roundtrip 72 banks, refs pass, cgio 1/0, palette 44/0), committed.
- Parity: [x] 36, [~] 11 -> strict 76.6 %, with partials 88.3 % (docs/formats/pacnyx_parity.md + evidence README).

## Session 3 (2026-10-14 18:21-) merge with update/parity-1014 (S2, S3, then S4+S5)
- 86b1d8e merges S2+S3: undo_manager is S2's incremental commit + S6 channel API (channels diffed every commit); FB parts edits call
  `StateReference::markEdited()` (marks the view pattern in the undo manager, so drops / scripts commit too); pose swap marks every
  pattern whose frame refs it remaps. Panes keep S2 fixes + S6 FB UI. PAT tab right button: FB sheet view = sheet selection, other
  PAT tabs = S2 UV drag, never a hitbox. ui-script: PACNyx dispatcher runs before S3's editCmd and claims only its own names
  (`layer` in an FB PAT tab = part layer, else frame layer; `cgm on|off` only, other `cgm <sub>` go to S4's cgmScript).
- 1819081 merges S4+S5 (cgm_struct.cpp union). pattex now links app_log.cpp (link failure inherited from update/parity-1014).
- Evidence: ctest 36/36, han2_roundtrip.log, uni_regress.log, pn01 log + pn02 retaken.

## Remaining
1. (done) pn02 retaken with the selection box visible (`zoomview 1.5` + `viewpan -192 -192`; `viewpan` aliases S2's `panview`); sheet-canvas row ticked (37/47).
2. In-game check pending (manual steps in pacnyx_parity.md). gamecap does not fit RBO cheaply: rbo_ex3.exe is DirectDraw + DInput8, gamecap
   hooks D3D9/XInput/GetKeyboardState, and RBO has no offline boot navigation (48 s intro movie).
3. (done) cgm_suite.log committed: finished, suite exit 0.

## Gotchas
- Windows exes are run from WSL; a running suite locks build/*.exe -> link "Permission denied".
- ab_shot.sh copies build/gonptechan.exe to work/ab_pacnyx; one shot run at a time.
- FB PAT: uv = 1/256 of the sheet (PACNyx DAT.cs: ImageXPosHalf * width/256); dest wh = pixels; layer = priority floor; draw ascending.
- CG image bounds x1..y2 are relative to the anchor (GOLEM (-80,-64)-(79,47) on a 512 canvas); AddImage needs >=0 so ReblockImage shifts.
- Game data: C:\games\rbo\DATA (suites), C:\games\rbo_run (shots), C:\games\gof\Data, C:\games\gof2_run. Don't copy games.
