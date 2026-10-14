# S3 handoff: UNI2/MBTL/UNIB data loss (D13-D18)

Branch `feat/s3-uni-dataloss` (base `42f9e03`). Not pushed or merged. Evidence is in `docs/audit_2026_10_14/evidence/s3/`.

## Done (commits 6b7e913..70a3473)

- **D13: paste conversion and per-file format** (`src/framedata.{h,cpp}`, `src/paste_notice.h`).
  - The dialect is detected once at load (`DetectHa6Game`, stored in `FrameData::m_detected`). Pasting never re-detects.
  - `FrameData::conformPasted(Sequence|Frame)` and `conformPastedAT` convert pasted data to the file's dialect:
    - Layers past the game's count are dropped. For MBAACC, one AFGP layer is kept: layer 0, or the first layer with a sprite.
    - MBAACC-only AT fields are cleared in UNI files. UNI-only fields are cleared in MBAACC files.
    - An empty file adopts the pasted data's dialect instead.
  - The returned report becomes a notice (`FrameState::pasteNotice`, shown in the main pane with a Dismiss button).
  - Wired into: main pane Paste pattern / frame / frames, box pane Paste before/after, right pane Paste AT, and the
    pattern manager (`patrefs::PastePatterns(..., Ha6ConvertReport*)`).
  - The format override is per file (`setGameOverride`). The global `g_ha6GameOverride` is now only the `--game`
    default. View > Game format shows Auto / MBAACC / UNIB / UNIST / UNI2 / MBTL.
- **D14: AT fields.** IDA (uni2 `Han6_LoadFrameAT` 0x4C6CB0, MBTL_25646393 0x4A8CE0, both commented and saved):
  - ATBT is never read by either game.
  - ATUH is read into a local and dropped.
  - ATVV keeps only its high words.
  - So in UNI mode the four fields are hidden, with a note (`frame_disp_at.h`).
- **D15: 10 condition parameters.** `kMaxIfParams` = 10, `kMaxEfParams` = 12. Overflow keeps the first N, as the game
  does (`framedata_load.cpp`); the writers emit up to 10. The raw IF editor shows 10 in UNI mode.
- **D16: layer tools.**
  - `PasteLayerRange` gives the range tools a "Selected layer / All layers" choice; "Number sprites" uses the selected layer.
  - `DeleteLayerKeepIds` clears a middle layer in UNI files instead of erasing it.
  - Add Layer is capped at `gameLayerCount()` (UNI2/UNIST 5, MBTL/UNIB 3). MBAACC files are not capped.
  - Paste transforms uses the layer's AFRT in UNI files.
- **D17: templates.**
  - The loader keeps BaseData PFLG&1 blocks for slots the character defines (`fd_main_load(..., templates)` →
    `FrameData::m_templates`, `templateFor(n)`).
  - `MergeTemplatePattern()` follows `Han6_MergeTemplatePattern_PFLG` (MBTL is now at 0x4A6BE0; same logic with 3 layers).
  - Main pane banner, with template vs pattern frame/tick counts. AS, AF timing/transforms, AT, EF and IF are greyed out
    via `AfDisplay(..., templateLocked)` and `BeginDisabled` in the right pane.
- **D18: UNIB.**
  - New enums `Ha6Game::UNIB` / `UNIST`.
  - 3-layer files are classified by tags: PSTS/PLVL → UNIB; AFJH/ATHH/ATC0 → MBTL. Otherwise by naming: chrNNN/npc → MBTL;
    Xxx, Xxx_N, csel_Xxx → UNIB.
  - Name-only stubs (one frame or fewer) in a `Xxx_N` folder are UNIB.
  - `mv_script` finds `Hyd_0_mv.txt`.
  - `--game` accepts uni2/unist/unib.
- **Extra: VectorTable.txt names.** `src/uni_vector_table.h` parses `vectmpl.add({number=..,name=..})` and resolves
  `const` ids. UNI-mode vector fields show those names instead of the MBAACC list.
- **i18n.** About 25 rows were added to `src/i18n_ja_ui2.inc`. `python3 tools/i18n_check.py` reports missing=0.
- **Tests.** `tests/uni_dataloss_test.cpp`, ctest `uni_dataloss`, 187 checks pass (`uni_dataloss_test.log`). It covers:
  paste conversion, AT writer, 10/12 params plus overflow, layer ops, merge, UNIB/MBTL/UNI2 detection over all data
  (0 wrong), template counts (48 UNI2 / 39 MBTL), VectorTable, and the same-game paste byte-exact round trip.
- **IDA (saved).** uni2: `BuildSearchPathPrefixedName` 0x4B8D50, `ArchiveIndex_FindEntryByName` 0x4BA240,
  `InitPatAndEnableSearchPaths` 0x83BE00, plus comments on `OpenFileViaSearchPaths` and the loaders.
- **Scripted evidence** (in-app `--ui-script`, no desktop input): the new commands
  copypattern/pastepattern/gameformat/layer/switchview/opentrees/tint are in `startup_args.cpp`.
  - Shots: `s3_template_banner.png`, `s3_paste_notice_mbtl.png`, `s3_format_uni2.png`, `s3_format_unib.png`.
  - Scripts: `shot_*.txt`.

## Session 2 (2026-10-14 evening)

- **Merge.** update/parity-1014 (3d72074) was merged with no conflicts (3056186). It brings S1 render work and
  `app_log.h`, the regress fix (every file must pass; counts are minimums) and the per-worktree ab_shot folder.
- **Build:** ok.
- **ctest:** 24/24 pass, undo_manager included (`evidence/s3/ctest_merge.log`).
- **`uni_regress.sh`** (`evidence/s3/uni_regress_merge.log`, gamedata stable, no grpdat):
  - Passing parts:
    - HA6: rc0=262, every file byte-exact. `--fresh`: rc7=261 rc5=1 (chr016, as expected).
    - PAT `--bytes`: rc0=180, every file byte-exact.
    - MBAACC single 305, UNI single 61, HA6_REGRESS_PASS.
  - **It prints UNI_REGRESS_FAIL for one reason only:** PAT `--fresh` rc5 on
    `mbtl/BattleRes/talk/st_pat/chr012/chr012_ed00.pat` ("Unknown texture type in PGT2").
  - This was already there before the merge. update/parity-1014's own `pat_roundtrip.exe --fresh` gives the same
    result on that file, and S3 changes no .pat code. It belongs to the PAT/S1 owner: either decode that PGT2
    texture type, or list the file as an expected rc5 next to chr016.
- **In-game (gamecap, per-process only):** see `evidence/s3/INGAME_MANUAL_TEST.md`.
  - The red AFRG renders in game: `ingame/mem_tinted_p0_f0.png` vs `mem_vanilla_p0_f0.png`, with a p96 control.
  - The `___St\` loose override does **not** load on UNI2 1.42. `ArchiveIndex_OpenEntryByName` runs after the loose
    open (0x4B9971) and the archive wins. This was proven with the `override_*.png` run, then the install was
    restored and checked (`install_before/after.txt`).
  - IDA uni2 1.42 IDB, renamed, commented and saved: OpenFileViaSearchPaths, SearchPath_SetSlot,
    SearchPath_BuildPrefixedName, ArchiveIndex_OpenEntryByName, DFolderIndex_LookupName,
    SearchPath_ReloadLanguageSlots, Han6_LoadFrame, Han6_LoadFrameAF, g_SearchPathSlots, g_SearchPathsEnabled.

## Remaining, by priority

1. To load the edited *file* in game, add a gamecap hook that makes `ArchiveIndex_OpenEntryByName` miss
   `data/chr000/chr000.ha6`, so the loose `___St\` file is used. That stays per process. The alternative is the
   nuuni winmm proxy, which changes the install.
2. chr012_ed00.pat PGT2 `--fresh` (not S3; see above).
3. D17 follow-up (optional): a "preview merged" toggle that renders `MergeTemplatePattern()` output. Today the merge is
   computed (and tested) but only drives the banner/greying.
4. Leftover from the audit: the AFPL combo and layer selector are still hidden on 1-layer frames.

## Gotchas

- The scratchpad is shared with other agents. Use `scratchpad/s3_uni/` and unique names (a `build1.log` collision happened).
- `ab_shot.sh` uses the shared `work/ab`. This stream used `work/ab_s3` (copy the exe there and run `--ui-script`).
- The gamedata corpus changed during the session (UNI2 115-143 and MBTL 147-168 HA6 between runs). Data-dependent tests
  skip or adapt, but regression counts drift.
- `pkill -f <pattern>` also kills the calling shell if the pattern appears in its own command line (exit 144).
- Logging: none was added. `src/app_log.h` (quill) is now in this branch via the merge.
- `tools/gamecap/games/uni2_force_pattern.gcs` now needs `OUT_META` per capture (another stream added it).
- Another agent runs gamecap batches. Check tasklist before launching; gamecap refuses if a game is already running.
