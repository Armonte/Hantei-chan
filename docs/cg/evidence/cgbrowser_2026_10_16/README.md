# CG + .pat texture browser (2026-10-16), branch feat/cg-pat-browser

What this covers: CG_AUDIT.md (docs/audit_2026_10_16) gaps 1–3.

- Shared layer: `src/cgm/cgm_texsrc.{h,cpp}`. One `TexSource` over CG images (`CgTexSource` on any `BankIO`) and .pat textures
  (`PatTexSource` on `Parts`, pat_texconv codecs). It provides view, replace, PNG export and import, references and the mod-folder save.
- GUI: CG manager > tab "CG + .pat textures". CG images are listed on the left and .pat textures on the right. The same buttons work on either
  kind: Export PNG, Import PNG, Replace (any WIC image), Save to game / Save to mod folder, Restore mod folder. The tab also has a "Used by" list.
  The PAT editor texture pane gained Import PNG and Export PNG, which use the same layer.
- CLI and tests: `cgmtex` (selftest, list, check, refs, export, pattern-ids, stripe, modsave, modrestore). The ctest rows are `texsrc_*`.
  The suite has `texsrc-pat` and `texsrc-cg` sections in `tools/cg/run_cgm_suite.sh`.

In-game proofs. fbmod [boot] training with P1 = chr003. The edited .cg is placed in `<game>\<IM>\Mods\data\chr003\` only while the game runs,
inside gamecap's game lock (`tools/cg/ingame_modfolder_boot.sh`), and is restored afterwards:
- `gui_uni2/`, `gui_mbtl/`: the editor (--ui-script, no real input) imports striped PNGs into the 36 (UNI2) / 21 (MBTL) images of pattern 0 and presses
  "Save to mod folder" (scratch game folder). `tools/cg/gui_modfolder_edit.sh` does this.
- `uni2_boot_gui/`, `mbtl_boot_gui/`: that GUI-written file booted in the game.
- `uni2_chr003_striped_ingame_cli.png`, `mbtl_boot/`: the first runs, where the file came from `cgmtex stripe` (the same layer, patterns 0-40).

Results: ctest 80/80 (`ctest.log`, exit 0). The CG suite passed everywhere except UNIB texsrc-pat, where the first binary failed on DXT3
textures. With the fix, `cgm_suite_unib_rerun.log` exits 0 and `texsrc_mbtl_pat_rerun.log` passes.

## Left to do (HANDOFF)
- UNIB DXT3 .pat textures (6 in 14 files: hyd/lin/ori/yuz ef00). The PGT2 loader does not read DXT3, so the browser lists them as
  "unsupported kind" and does not edit them. This needs DXT3 load, GL upload, decode and encode in parts_texture.cpp and pat_texconv.
- RBO / GOF2: CG banks show in the tab through CgIO. Their part sheets are edited in the han2 PAT pane, which already has PNG; they are not
  routed through PatTexSource. ActCadenza has no .pat.
- The tab's .pat replace marks the character edited, but it has no undo step of its own (the CG replace goes through the CG manager history).
- Mod-folder save writes the .cg / .pat. A .pal edited in the Palettes tab still saves in place, not to the IM mod folder.
