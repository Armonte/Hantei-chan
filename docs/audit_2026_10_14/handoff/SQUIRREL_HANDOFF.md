# Squirrel scripting scope + prototype: handoff (2026-10-15)

Worktree `wt/squirrel`, branch `feat/squirrel`, based on `update/parity-1014`. Not pushed or merged.
The full write-up is `docs/squirrel/SCOPE.md`.

## Findings
- **Every French-Bread game we have embeds Squirrel 2.2.4 stable**, an x86 ANSI build (char 1 / int 4 /
  float 4), bound with early Sqrat. Games checked: UNIB 1.04.03, DFCI Ignition 2.32.00, UNI2 1.42 and
  MBTL 25646393. There is no 3.x anywhere.
- Scripts are plain CP932 source `.txt`. There is no bytecode, encryption or obfuscation; the archive layer is
  the only container.
- **FB VM modification (one): a Shift-JIS-aware string lexer.** Found at UNI2 0x47B9F0 and MBTL 0x473430.
  Without it, 7 MBTL and 18 UNI2 scripts fail to compile. It is reproduced as a marked patch.
- The stdlibs are all registered: blob, io, system, math, string. The root natives are `__dofile__` and
  `SQ_LoadString`. The load path is `sq_compilebuffer` on the raw bytes, with no transcoding.
- The native API was extracted from the IDBs into `docs/squirrel/api/{uni2,mbtl}_api.{json,md}`:
  - UNI2: 674 functions, 150 methods, 525 constants, 60 vars.
  - MBTL: 615 functions, 386 constants, 118 vars.

  Cross-checks: 366/375 (UNI2) and 345/358 (MBTL) of the `Table.Member` uses in the scripts resolve, and the
  rest are comments or dead code. Every lumina name is covered.

## Prototype
- `third_party/squirrel-2.2.4`: upstream tarball, sha256 recorded. It carries three portability patches
  (`SQ_FB_COMPAT` keeps int32 on x64) and the FB lexer mod; see `PATCHES.md`.
- `squirrel224` static library, `sqtool` CLI (`src/squirrel/sqtool.cpp`, quill console sink).
- `sqtool check <gamedir> --api <api.json> --run [--report x.tsv]`:

  | game | Squirrel scripts compiled | bootstrap run |
  |---|---|---|
  | MBTL | 375/375 | ok |
  | UNI2 | 185/185 | ok |
  | UNIB | 56/56 | compile only |
  | DFCI | 171/171 | compile only |

- ctest:
  - `sqtool_fixture`: own FB-style script.
  - `sqtool_fixture_bad`: syntax error, WILL_FAIL.
  - `sqtool_mbtl`, `sqtool_uni2`: full check plus run. SKIP_RETURN_CODE 77 when `C:/dev/hantei-chan/gamedata` is
    missing; override with `-DSQTOOL_GAMEDATA_DIR=`.

## IDB work (saved)
- **UNI2 `uni2_142_4ebed985.exe.i64`:**
  - the `sq_*` API (about 25 functions), `sqstd_*`, `sq_base_register`;
  - the Sqrat binders, `CSquirrel_ctor`, `Sq_CompileBufferAndRun`, `SqNative___dofile__`/`SQ_LoadString`,
    `ChrScript_Build*Path`, `SQLexer_*`;
  - 68 `SqratWrap_<ret>__<args>` wrappers.
- **MBTL `MBTL_25646393.exe.i64`:** the same set, aligned from UNI2 by wrapper call sequences. lumina's wrong
  guesses were corrected (`RegisterSquirrelFunction` became `Sqrat_TableBase_BindFunc`, `Sqrat_PopStack`
  became `sq_pop`, …). 67 wrappers were renamed.

## Tools (re-runnable)
- `tools/squirrel/ida_extract_sqrat_api.py`: in IDA, run `extract(seed, out)`, then `classify_all(out, out)`,
  then `attach_objects(out, out)`.
- `tools/squirrel/sqrat_api_normalize.py`: converts raw JSON into the API JSON and Markdown.
- `tools/squirrel/sqrat_api_xcheck.py`: checks the API against the scripts.

## Open items / next steps (plan in SCOPE.md §4)
1. Run the extractor on the UNIB IDB (`D:\dev\unib\game\RingGame.exe.i64`) and the DFCI IDB. Confirm the
   lexer mod there.
2. Record ConstTable versus Enumeration in the profile. sqtool currently installs constants both ways.
3. Stub class vars. Extend `--run` to every character's `_sc/_cmd/_mv/_com` after the bootstrap.
4. Diff `SQVM::Execute` and the keyword table against upstream, to close the "no other VM mods" question.
5. Phases P2 to P7: CP932 document model, editor, cross-links, `_cmd_` editor, preview-sim mocks, strict
   validation.

## Decisions for the user
See SCOPE.md §5:
- single VM now;
- in-editor sandbox without io/system;
- strict validation by default;
- choice of editor widget;
- profiles committed under `res/squirrel/`;
- acquire UNIEL/UNIST/UNICLR for one `strings` check.
