# S4+S5 handoff: CG manager parity and palettes (feat/s45-cg-palettes)

Base: update/parity-1014 42f9e03. The branch is not pushed or merged. The design write-up is `docs/cg/cg_manager_design.md` section 9.

## Done (WIP commits 7b56547..HEAD; the full ./build.sh passes)
1. **Shared-bank safety** (`src/cgm/cgm_project.*`).
   - `FindBankUsers` scans the bank folder's `.txt` projects. On MBAACC akiha.cg it finds 5 projects and 9 HA6 files. On MBTL chr000.cg it finds 4 projects and 5 files, and marks `../BaseData` as shared.
   - It also picks up an unlisted `<bank>_*.ha6` (UNIB Hyd_s).
   - `RemapUserFiles` patches the ids in place. The loader records each id's offset (`g_ha6SpriteRefSink` in framedata.h/framedata_load.cpp), so the rewrite is byte-exact.
   - Usage is counted across all projects. `cgmtool usage --stack akiha_0.txt` reproduces the audit's 125 images that look unused but other projects draw.
   - Proof: `cgmtool project-refs-check` passes on akiha (12297 refs, inverse 8/8 byte-identical) and on MBTL chr000.
2. **One save.**
   - Images carry a `uid`, and `RemapSince(diskUids)` gives the renumbering still pending.
   - The window's button "Save bank, its projects and this character" writes the .cg, the other project files and the character together.
   - `MainFrame::saveCharacter` also saves the bank when ids are pending, and shows a notice.
   - `RemapSprites(..., skipMark)` does not bake inherited patterns into the own file.
3. **cgtool workflows** (`src/cgm/cgm_folder.*`, `src/cgm/cgmtool_ops.cpp`, and the window toolbar):
   - build a bank from a folder;
   - re-import a plain cgtool folder without a manifest;
   - sync a folder (add, delete, reorder with `12.5#name_ID_40.png` or `order.txt`);
   - `ReplaceImageResized` (re-cut, keep the anchor);
   - `export --pal f.pal[:N]`;
   - cgmtool commands `insert`, `delete`, `move`, `delete-unused`, `replace`, `sync`, `import-plain`, `build`, `users`, `usage` and `sprites`, each with `-o` or `--in-place [--refs]`.
   - Proof: `folder-check` passes on sion, MBTL chr000 and gakiha.
4. **GAKIHA "BMP Cutter2".**
   - Images have no blobs. Pixels are on pre-baked 64 KiB pages after the align table. The align count is a WORD and its high word is kept.
   - It is modelled in `cgm::Bank` (`cutter2`, `bakedPages`), and `CG` renders it the same way.
   - check, struct-check and roundtrip pass. cgmtool reads a `.DAT`'s embedded CG directly.
   - RE finding: MBAC never draws this bank. mbacPC `ProcessCGData` at 0x43E390 needs the dword at +0x10 to be 1, and GAKIHA's is 0. So there is no possible in-game proof for it.
5. **E19 palettes.**
   - `PalSet::paired/movePair/swapPair` keeps c and c+65 together, and the UI names the pair.
   - New `_pN.pal` (`CreatePupsBank`). Import of any palette index (`file.pal:37`). UNI-layout export and set conversion. In-place "Save .pal" with .bak.
   - A UI note says UNI2/MBTL saves only reach the game through override folders.
   - Proof: `pal-check-uni` passes.
6. **CC palettes** (`src/cgm/cgm_ccpal.*`).
   - Writes `<game>\palettes\<charID>_<palNum>.png` (PLTE, 1..252), and optionally a grown `data\X.pal` + `_csel` copy.
   - The layout hook is `CcPlaceOf` (tabs of 36). It follows `docs/cc_select_patch/DESIGN.md` section 4. The boss ids follow DESIGN.md.
   - Proof: `cc-check` passes.
7. **PAT textures** (`src/parts/pat_texconv.*`, `src/pattex.cpp`).
   - Type 22 = D3DFMT_X8R8G8B8, found in chr012_ed00.pat. It is drawn opaque. Fixed in PgLoad, Save and parts.cpp.
   - DXT1/5 and raw codecs, PNG import/export, `pattex list|export|import|check`.
   - `pat_rt_all.sh --fresh`: rc0=180 (that is every .pat now on disk).
8. **i18n:** Japanese rows are in `src/i18n_ja_cgm.inc`; `i18n_check` reports missing=0.
9. **Suite and tests.**
   - `tools/cg/run_cgm_suite.sh` now covers MBAACC, UNIB, UNI2, MBTL and MBAC (`--games`, `--quick`).
   - ctest entries: `cgm_project_refs_*`, `cgm_folder_mbaacc`, `cgm_cc_palettes`, `cgm_palette_uni_mbtl`, `cgm_cutter2_*`, `pat_textures_x8`.
10. **In-game proof for MBAACC insert + reorder on the moon-shared bank**, in `docs/audit_2026_10_14/evidence/cg/`:
    - `mbaacc_before_akiha_crescent.png`;
    - `mbaacc_after_insert_move_akiha_{c,f}.png`.
    - The edit: the stand sprites were striped, a sprite was inserted at 0 and image 1 was moved to 1424, all with `--in-place --refs`. 12297 refs were rewritten in 9 files.
    - The Full-moon shot draws correctly through the moved ids.
    - Tools: `tools/cg/ingame_shot.sh` (game_link_cli launch with an Authoring setup, so no input is sent) and `tools/cg/pw_capture.ps1` (PrintWindow of that PID only).

## Session 2 (2026-10-14 evening): suite, GUI smoke, in-game
11. **Suite on every bank** (`tools/cg/run_cgm_suite.sh --every`, 36 min at 12 workers; logs in `docs/audit_2026_10_14/evidence/cg/`):
    `suite_every_all.log` (1193 parallel jobs), `suite_every_seq_rerun.log` + `suite_every_mbac_rerun.log` (sequential sections with the
    fixed binary), `suite_rerun_crashed.log`, `suite_every_UNIB.log` (first UNIB-only run that found the refs bugs). Result, all pass:
    - bank check: MBAACC 72, UNIB 30, UNI2 84, MBTL 88 (every bank incl. _csel/_talk/_coloredit), MBAC 50 character DATs; differ 0.
    - round trip + struct on every one of those banks; project-refs + folder on every bank with a project: UNIB 30, UNI2 84, MBTL 81, MBAACC akiha/neco.
    - `.pal`: pal-check 44/30/86/106; pal-check-uni on every UNI2 (57) and MBTL (81) file.
    - 8 jobs (MBTL folder-check chr012/016/017/019/020/021/022, MBAC struct NERO) aborted with exit 3 at 12 workers next to other streams;
      all 8 pass at 4 workers, so `--jobs` now defaults to 4.
    Fixed on the way:
    - `project-refs-check` read `total[id]` past the bank for layers that name an id the bank does not have (UNIB Car/Elt/Hil/Lin/Mer/Set/Vat/Wal/Yuz).
      `RemapHa6File` leaves such ids alone; the check now does too (and prints the failing layer).
    - tiny banks (UNIB `_csel` portraits, 1-2 images): the move / folder-structure steps are skipped with a note instead of failing.
    - `PalSet::converted(toUni)` lost the alternate set (65..129) of a flat 130-palette file: UNI -> MBAACC -> UNI changed chr008/chr010.
    - MBAC: only `02/*.DAT` (character banks); the 68 `05/BG*.DAT` stage files hold another layout and were counted as parse failures.
12. **ctest.** The game-data tests (`cgm_project_refs_*`, `cgm_folder_mbaacc`, `cgm_cc_palettes`, `cgm_palette_uni_mbtl`, `pat_textures_x8`,
    `cgm_cutter2_*`) were never registered: cmake runs in WSL and tested `EXISTS "C:/..."`. `fb_data_exists()` also checks `/mnt/c/...`;
    `--tmp` is now relative to the build folder. 31 tests: 30 pass; `undo_manager` fails only its timing assert (`commitMs < 16`, 20-31 ms at
    load average 70); it is not touched by this branch. Log: `evidence/cg/ctest_full.log`.
13. **GUI smoke** through `--ui-script` only (`tools/ui/cgm_smoke.sh`, scripts in `tools/ui/shots_cgm/`, `AB_WORK=work/ab_s45`, fixtures copied
    to `work/ab_s45/fx`). New script hook: `cgm open|tab|press <English label>|path <next dialog answer>|select|set <field> <n>|status`
    (`Window::script`, `CgmButton`; a pending press opens the tree node that holds the button). Evidence `evidence/cg/gui/`:
    - 1: MBTL chr000 add at 0, move 2 -> 100, delete 50 (confirm dialog shot), "Save bank, its projects and this character": .cg + 3 other
      project files rewritten (+ .bak), BaseData untouched; `cgmtool refs-compare` (new) old vs saved: 2537 layers same picture, 3 cleared, 0 differ.
    - 2: palette pair move 3 -> 4 (file shows 3/4 and 68/69 swapped, header kept), new `chr000_p1.pal`, in-place Save .pal (+ .bak).
    - 3: MBAACC akiha "Export to <game>\palettes" -> `palettes/3_37.png`, `3_38.png`.
    - 4: plain cgtool folder import (1 replaced, 600 unchanged), new bank from folder (601 images, `check` pass), Export all.
14. **UNI2 PUPS in-game proof: prepared, PENDING A LOADER.** `tools/cg/make_uni2_pups_proof.py` (new `_p1.pal` via `pal-new-pups`,
    red/blue swapped; pattern 0 gets `PUPS 1`), `tools/cg/uni2_pups_proof.gcs` (gamecap, no menu input: the game's boot-into-training
    path, longer boot timeouts, PrintWindow capture) and `uni2_pups_proof.sh` (vanilla shot, then the override shot; refuses unless
    `OVR=<loader override root>` is given, refuses if that folder already exists, removes it afterwards).
    UNI2 1.42 does NOT take loose files from `___St\data` (S3 test: the archive copy wins, 0x4B9971), and no DLL may be added, so the
    run waits for the user's loader decision (nuuni / UNI2-Improvement-Mod). The UI note keeps naming the nuuni mod/ tree.

## In-game status
- MBAACC insert + reorder on the moon-shared bank: proven (session 1).
- UNI2 new `_pN.pal`: pending a loader (item 14). Attempts on 2026-10-14 used `___St` before S3's correction; none got to the
  override shot. One of them overlapped another stream's uni2.exe boot: `___St` existed for about a second while that process was
  starting (it was removed; the install was checked clean: no `___St`, only the pre-existing `winmm.dllx` / `winmm.dll_work`).
  The gamecap navigation itself works here: training Hyde vs Hyde was reached with no input (`tools/gamecap/logs/fail_20261014_175346.png`,
  taken when the presented-frame counter stopped after the battle drew; the script now captures by window after a wall-time wait).
- MBAC CG edit: not run (MBAC needs menu input to reach a battle, and no per-process MBAC tool exists). Bank: `02/AKIHA.DAT`
  (BMP Cutter3, dword +0x10 = 1, so `ProcessCGData` draws it; GAKIHA's is 0). Steps: copy `02.p` aside (sha1), extract AKIHA.DAT
  (PKFileInfo, byte-exact round trip per fb_format_matrix), open it in Hantei-chan, CG manager: stripe image 0 (Replace selected from PNG),
  save the character (`SyncHa4Cg` writes the blob back), repack `02.p`, start MBAC, Training, Akiha; screenshot; restore `02.p`, `sha1sum -c`.

## Game-file state
- Session 1: akiha files restored, `sha1sum -c work/s45/backup_mbaacc_akiha/SHA1SUMS` OK (10 files).
- Session 2: no game file changed; UNI2 install checked clean (no `___St`, no new DLL).

## Not run / remaining (priority order)
1. UNI2 in-game PUPS shot once a loader is active: `OVR=<override root> tools/cg/uni2_pups_proof.sh` (one game machine-wide).
   Alternative the coordinator offered: a gamecap memory-write proof (write the palette into the loaded palette memory), as S3 did.
2. Then name the working override folder in the UI note (`cgm_window.cpp` "UNI2 / MBTL read their data from archives...") and its
   `src/i18n_ja_cgm.inc` row.
3. MBAC in-game CG edit (steps above) when a person or an MBAC boot path is available.
4. `undo_manager` fails only its timing assert (17 ms vs 16 at load 15); this branch does not touch undo code. Re-run on an idle machine.
5. Possibly: a stage-DAT (`mbac/05/BG*.DAT`) CG reader, if stage sprites should be editable.

## Gotchas
- The scratchpad is shared with other streams. Use the `s45/` subdirectory.
- MBAACC `FrameData::save` is not byte-exact. That is why the remap patches ids in place.
- `Parts` destructors need GL. `pattex` leaks them and calls `_exit`.
- In folder names, a leading `000_` is an *identity*. The order prefix needs `#`.
