# S2 handoff (feat/s2-pat-safety, merged with update/parity-1014 3d72074)

Worktree `/mnt/c/dev/hantei-chan/wt/s2-pat-safety`. Not pushed, not merged.

The design is documented in `Hantei_Docs/update_2026_09/HANTEI_UNDO_TOOLS.md` §10 (undo channel, dirty state, backups, shortcuts).

## Done

| Item | Commit | Where / evidence |
|---|---|---|
| Undo channel adopted unchanged from S6 (`undo_manager.*`, `parts/parts_undo.*` from pacnyx d74aa83) | 890ec53 | — |
| PAT document mode (`CharacterInstance::makePatDocument`) | 890ec53 | Own `PartsUndoChannel`. The undo manager is attached to an empty list, so the placeholder pattern is never "dirty". |
| C8 auto-backup | 890ec53, 14ec908 | `src/file_backup.*`; `<dir>/backup/<name>.<YYYYMMDD-HHMMSS>-NN.bak`, cap and folder in the ini. Called from `MainFrame::saveCharacter/saveCharacterAs/savePatOf`, which covers menu, Ctrl+S, the close prompt and Save all. Preferences under Edit > Preferences > Auto backup, and in the "Saving and PAT editor settings..." window. |
| C9 atomic .pat save | 890ec53 | `Parts::Save` → `WriteFileAtomic`. MBAACC-layout saves never add a VEST block (d54ecf5). |
| C10 bugs | d54ecf5, 4980bf0 | Animator arrows clamp before `.at()`; stale-name and selection guards in every pane, with name counts; `new int[]` leak removed; UV Left/Top labels fixed; double `ImGui::End()` in all 5 panes; add part set / prop / paste refresh and highlight by propId; tool combo uses live names; animation spriteIds clamped (`ClampPatState`, `ui/pat_editor_impl.h`). |
| C11 UV right-drag | 14ec908, b9980c8 | `BeginUvDrag/UvDragBy/EndUvDrag`: main and detached views, one undo step, Esc cancels. The sheet is drawn from world (0,0), not centred. |
| C12 dirty flag / undo / reload | 14ec908 | Dirty flag and undo via the channel. `savePatOf` reloads other tabs (`reloadPatInOtherTabs`). New PAT (UNI or MBAACC layout) in File > Import and the "+" tab menu, which now has HA6 / PAT sections. |
| G: keyboard and views | 4980bf0, 14ec908 | PAT navigation (Up/Down, Left/Right, Q/E, Num/ and Num*); Ctrl(+Shift)+arrows nudge, or move the UV rectangle in the UV view; F / L(PAT) / G(chr) / H / T / U; PNG export rows for PAT tabs. |
| G: drag blocking, #9 mouse remap, #61 Q/E | 4980bf0, 14ec908 | Shortcuts are blocked while dragging. Mouse remap: `main.cpp`, `MouseButtons=` in the ini. Q/E fire in numeric fields only. |
| G: selected-part outline; panel, opacity and colour persistence | 14ec908 | Outline drawn as an ImDrawList overlay. Panel visibility, highlight opacity and overlay colours are saved in the ini. |
| G: Tools > Layers | 14ec908 | Fill missing layers / Scale all layers and boxes. |
| G: hitgrab bit labels (UNI) and added-effect raw ID | b9980c8 | `frame_disp_at.h` |
| G: MBAACC DDS | d54ecf5 | Import is stored as PGTX (DXT1/DXT5/32-bit decoded). Export writes a real 32-bit DDS. Fixes the DDS w/h swap and the wrong-texture upload. |
| i18n | f6452f2 | `src/i18n_ja_patsafety.inc`. `tools/i18n_check.py`: missing=0, no new duplicates. |
| Coordinator task: undo perf | 2c8496a | `markModified(index)`, `setDirtyFeed` (FrameData::mark_modified → `undoDirtyFeed`), drift slice of 24 per commit. Full scan only for a generic `markModified()`, flush, markClean and undo/redo settle. Panes mark `m_state.pattern`. Test uses GetThreadTimes, best of 5×20, budget < 4 ms. |
| Crash fix: PAT tab with no palette | 2c8496a | `CG::palette` / `origPalette` were uninitialised; crashed in `getColorFromPal` (colour slot 251). Now null-initialised and reset in `free()`. |

**Undo perf numbers** (`docs/audit_2026_10_14/evidence/s2/undo_perf.log`):

| Commit kind | Cost per step |
|---|---|
| Before: full-document compare | ~18–20 ms CPU |
| After: incremental | 0.39–0.46 ms wall; CPU below one 15.6 ms GetThreadTimes tick |

**Session 2 (2026-10-14):**

| Item | Commit |
|---|---|
| Merged update/parity-1014 (S1 render, `app_log.h`, per-worktree `ab_shot.sh`). Conflicts: `pat_partset_pane.cpp` includes; `parts.cpp` empty-set check (kept the sheetView exception on top of S1's HC_LOG version). | 07b87b6 |
| `pat_safety_test` links `app_log.cpp` (parts/texture now log through quill) | CMakeLists |
| Detached views block shortcuts during a drag: `DrawDetachedViewSurface` flags an active surface / UV / box drag, and `DrawDetachedHosts` reports it each frame (`SetDetachedMouseDrag`). The router flag is main OR detached. | 065e143 |
| PNG export from a PAT tab is named `<pat>_ps<NNN>[_<part set name>]` (`PrepareExportSettings`). | 065e143 |
| printf: none left in `src/parts`, `render.cpp` or `main_ui_impl.h` after the merge. Removed the TEXTURE_VIEW button printfs (`pat_texture_pane.cpp`). `texture.cpp` LoadCompressed now logs errors only, through `HC_LOG_ERROR`. | 065e143 |

**Tests:** full ctest 25/25 on the merged build (`evidence/s2/ctest.log`). `tools/uni/uni_regress.sh` (`evidence/s2/uni_regress.log`):
- HA6: 262/262 bytes.
- PAT: 180/180 bytes, and MBAACC 19/19.
- All other sections pass.
- Two failures:
  - **PAT --fresh `mbtl/BattleRes/talk/st_pat/chr012/chr012_ed00.pat`** ("Unknown texture type in PGT2"). This is the known baseline failure (`evidence/baseline/uni_regress.log`).
  - **PAL `mbtl/data/_csel/chr020.pal` "save failed"** in the full run. It was transient (ctest was running in parallel): the PAL rerun alone gives 272 files, 0 differ.
- The local corpus has 272 .pal files, against 321 in the baseline. Gamedata content, not this branch.

**Evidence:** `docs/audit_2026_10_14/evidence/s2/`, recaptured on the merged build, cropped and quantized (20–50 KB each).
- `s2_01..06`. `s2_05_uv_dragged.png` shows the cyan UV rectangle.
- `s2_07_new_pat.png`, and `s2_08_prefs_backup.png` (Auto backup / PAT editor / mouse buttons).
- Logs: `s2_pat_editor_script.log`, `s2_newpat_script.log`.
- Scripts: `tools/ui/shots/s2_pat_editor.txt` (needs `work/ab_s2-pat-safety/s2/chr000.pat`; see its header) and `s2_pat_newpat.txt`. Run each with `tools/ui/ab_shot.sh <script>`.

## Remaining

- printfs outside parts/render remain: character_instance effect loading, the main_menu effect reload, and the `[Font]` startup lines in main.cpp.
- PAT shortcuts help page (P16). Only the key-binding window shows context tags.
- Priority as an int widget (audit P2 item 18). Not done.

## Gotchas

- **Shot folder:** `tools/ui/ab_shot.sh` uses `work/ab_<worktree>` (here `work/ab_s2-pat-safety`), so no manual copy is needed. One run failed with `Syntax error: "(" unexpected` because the freshly copied exe read back as zeros on DrvFs. Rerunning fixed it.
- **Script commands** added in `MainFrame::PatScriptCommand`: patopen, newpat, shortcut <actionId>, patsel, patmode, uvdrag, panview, patinfo, prefswindow, lang.
- **Headless Parts must never be destroyed:** `~Vao` calls GL with no context. Tests leak them and `_exit`.
- **Expected merge conflicts with S6:**
  - `character_instance.{h,cpp}`: constructor lines and members;
  - `ui/main_ui_impl.h` RightClick / HandleMouseDrag;
  - `PatEditor/pat_*` panes;
  - `undo_manager.*`: S2 modified S6's copy for the perf work;
  - CMake source list.
  
  S6's `partsUndo` (French-Bread) and S2's PAT-document channel are never active together.
- **Coordinator rules:** no desktop input (no `i18n_shoot` mouse driving); one game instance at a time; quill only for logging.
