# PCMOD handoff: Hantei-chan "Export as pcmod package"

Branch `feat/pcmod-export` (worktree `wt/pcmod-export`), based on `update/parity-1014` `bfffbce`. Not pushed or merged.
The spec is `docs/modmanager/pcmod-manifest_schema1.md` in the main checkout (schema 1, pinned 2026-10-14). The user
page is `Hantei_Docs/pcmod_export.md`.

## Done
- `src/pcmod/`:
  - `pcmod_manifest.*`: the model, the strict reader, the deterministic writer, the §3 path rules and roots per class,
    and the package and session digests.
  - `pcmod_sha256.*`: a new FIPS 180-4 SHA-256 (public domain). The tree had no SHA-256 before.
  - `pcmod_zip.*`: a store-only zip with fixed timestamps.
  - `pcmod_package.*`: scan, pack, verify a folder or zip, read `mod.ini`, and session resolution with the §4 checks.
  - `pcmod_vanilla.*`: the `.p` and loose-file index of `C:\games\mbaacc`.
  - `pcmod_export_ui.*`: the window. `pcmodtool.cpp`: the CLI.
- The writer never emits `inject`, `palettes` or `exe_layers`. The reader refuses them, plus unknown and duplicate keys.
- `pcmodtool pack | verify | digest | session` (see the docs page).
- GUI: File > Export > "Export as pcmod package..." (also under Tools > MBAACC package).
  - It lists files with size and SHA-256, auto-detects the class, and warns `REPLACES vanilla ... ship the whole file`.
  - The exe hash defaults to `C:\games\mbaacc\MBAA.exe`.
  - Packing writes the folder and the zip, then reads both back and verifies them.
  - Japanese rows are in `src/i18n_ja_pcmod.inc`.
  - UI script: `pcmod open|add|addfolder|set|export|status`, with a shot script in `tools/ui/pcmod/export_window.txt`.
- Tests:
  - ctest `pcmod` (`tests/pcmod_test.cpp`) covers the fixture digests, the SHA-256 KATs, every §3 refusal, the roots
    per class, the reader refusals, a writer round trip, the pack -> verify round trip, the zip layout, determinism,
    zip and folder tamper, and `mod.ini`.
  - ctest `pcmod_cli` (`tools/pcmod/pcmod_cli_ctest.sh`) runs the same digests through the CLI.
  - The fixture was copied into `tests/fixtures/pcmod/` from PovertyCaster `343706aa4`. `ORIGIN.txt` records this, and
    `.gitattributes` sets `-text`.
- Full ctest after `build.sh`: 40/40 passed (`docs/audit_2026_10_14/evidence/pcmod/ctest.log`).
- `MMMONTOCASTER_TRIAGE.md` (main checkout, not git) has a new "Status" line.

## Digests produced (exe = 63 zeros + '1')
| | value |
|---|---|
| package | `eb3db61182ed3ee9875f68f6f668a1671ff3744340e073dffcbfc966998ddf27` |
| package sim | `16a83aa066f3a94620eed7219529cb3b6463d158e080c02190345a1d1ddc6041` |
| session (fixture enabled, priority 100) | `8c4fe7957fcc0be5912de92c093dcab0a47d812b6d080e6ab5517588df7341cc` |
| vanilla set | `f9956a1ae59ce26aa14e3c4d97597da9a665727f9b87539cd48432b10636c0db` |

## P3 dry run (ActCadenza moon 3)
- The input is `docs/actcadenza_parity/P3_PACKAGE_FILES.tsv` (S8b): 75 files, class sim.
- The 75 files were copied from `gamedata/mbac_to_cc/actcadenza_moon3/` into `gamedata/pcmod_out/_p3_src/`, and every
  SHA-256 matched the TSV. The pack command was:
  `pcmodtool pack ... --id actcadenza --version 1.0.3 --priority 100 --out gamedata/pcmod_out`.
- Output: `gamedata/pcmod_out/actcadenza/` and `actcadenza-1.0.3.zip` (15.2 MB, verifies OK). These are not in git.
  The log is `evidence/pcmod/p3_dry_run_pack.log`.
- **Package digest: `7721d18a3b3f8984582ca65a27ceea41992d6c9db48958a4ea6503750c4b2fe9`.** An independent Python run of
  §5 over the TSV gives the same value.
- **Package sim digest: `d6d72ad9295f0354dcabcfeffe06d06d2b031a07488460fd87c329e040026ef8`** (74 of 75 files; the one
  `data\*.pal` is cosmetic). ctest `pcmod` pins both P3 digests from `tests/fixtures/pcmod_p3/P3_PACKAGE_FILES.tsv`.
- Session digest on the local MBAA.exe (`6d1415ca...0310a`), folding the sim digest:
  `3e6b564cb0c7aa93d039a29337e2be8811fb6d83066a4ab9dc794d759f605901`.
- The pack warned only that `data\_type.txt` REPLACES a vanilla file, which is expected: P3 ships the whole file with
  `[Type_3]` appended.
- The local install has `0002.p` renamed to `0002.p.bak`, so the vanilla check found `_type.txt` as a loose file.
- The id and version `actcadenza 1.0.3` are the spec's example. Choose the real ones before publishing to the catalogue.
- `P3_PACKAGE_FILES_NEEDS_SCHEMA2_INJECT.tsv` (G_AKIHA_MBAC) is left out, because it needs schema-2 `inject`.

## Open / notes
- The zip reader accepts stored entries only: a deflated archive from another tool is refused with the reason. If the
  catalogue may hold zips that Hantei-chan did not write, deflate support is needed on the PovertyCaster side anyway.
- `tools/i18n_check.py` with no arguments (all sources) exits 1 at baseline. The 15 missing keys are all in
  `src/PatEditor/pat_partset_pane.cpp` and `pat_shape_pane.cpp`, from another stream. Every new key passes:
  `python3 tools/i18n_check.py src/pcmod/pcmod_export_ui.cpp src/ui/package_tools_impl.h src/ui/main_menu_impl.h`
  reports missing=0.
- The CLI writes its results to stdout, as cgmtool and mbaaccpackage do. The GUI logs through `HC_LOG_*` (quill).
- Character auto-gather uses `mbpackage::ValidateCharacterPackage` loose rows plus same-stem files. Packed-only rows
  (in `.p`) are not added; extract them first (Tools > MBAACC package > Validate > Extract).
- The shot is in `evidence/pcmod/pcmod_export_window.png` (via `tools/ui/ab_shot.sh`). The status colours adapt to the
  light and dark themes.

## Schema-1 widening (PovertyCaster mods/pcmod-schema 96889671e + 7cb8826d4)
- Classes are per path (`pcmod::ClassifyPath`): sim = `data\**` except `*.pal` / `data\_csel\**`, `story\**`,
  `charaselect.txt`; cosmetic = those two exceptions, `bg\ bgm\ dat\ grp\ se\ shader\ movie\ palettes\`,
  `hud_theme.json`, `font.fnt`, `_sysfont.ini`; everything else refused (`sound\` dropped, `PackDataVersion\` refused).
- A sim package needs a sim file; a cosmetic package may not list one. The session digest folds `package_sim_digest`.
- `pcmodtool digest` prints `package <hex>` and `sim <hex>`; the GUI has a Class column and a sim-file count.
