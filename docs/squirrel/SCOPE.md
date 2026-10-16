# Squirrel scripting in Hantei-chan: scope (2026-10-15)

Can the games' Squirrel scripting run inside Hantei-chan, for the post-MBAACC French-Bread games?
**Yes, and it is simpler than feared.** Every French-Bread game we have embeds the **same VM: Squirrel 2.2.4
stable**, an ANSI 32-bit build. Scripts ship as **plain CP932 source** in `.txt` files. There is no bytecode,
no encryption and no obfuscation; the archive layer is the only container, and our extractors already open it.
FB made exactly one change to the VM: a Shift-JIS-aware string lexer, which we reproduce. So
"multi-version" means one vendored VM plus per-game **API profiles**, not several Squirrel builds. A
second VM (3.x) is needed only if a future game (Twi-Lumina) moves.

Prototype: worktree `wt/squirrel`, branch `feat/squirrel`. It vendors Squirrel 2.2.4 and adds `sqtool`,
which compiles every script of a game with that VM and runs the battle bootstrap against generated native
stubs. Results are in §3.

---

## 1. Per game

### 1.1 Evidence table

| | UNIB arcade 1.04.03 | DFCI Ignition arcade 2.32.00 | UNI2 Steam 1.42 | MBTL Steam 25646393 |
|---|---|---|---|---|
| exe | `D:\Games\unib\RingGame.exe` | `C:\dev\transmission\…\RingGame.exe` | `uni2.exe` (4ebed985) | `MBTL.exe` (be83d058) |
| `_version_` string | `Squirrel 2.2.4 stable` | `Squirrel 2.2.4 stable` | `Squirrel 2.2.4 stable` | `Squirrel 2.2.4 stable` |
| `_charsize_/_intsize_/_floatsize_` | 1 / 4 / 4 (x86) | same | 1 / 4 / 4 (seen in `sq_base_register`, UNI2 0x477580) | same |
| Binding | early Sqrat (`Sqrat::RootTable/Table/TableBase/ConstTable/Enumeration/Class<T,DefaultAllocator<T>>`, RTTI) | same | same | same |
| Script format | plain CP932 source `.txt`, CRLF | same | same (archive `d\`, no cipher) | same (archive `dataNNN.bin`, per-entry XOR is the *archive* layer only) |
| Bytecode (`.cnut`, 0xFAFA) | none | none | none | none |
| Script files | `script\` 11 + `data\<Chr>_0\<Chr>_0_{mv,sc,cmd}.txt` | `script\` 18 + `data\<Chr>[_s]_0\…` | `script/` 17 + `data/chrNNN/` | `script/` 19 + `data/chrNNN/`, `data/_talk/npcNNN/` |

The other titles (UNIEL, UNIST, UNICLR, DFC, PC DFCI) are not on this machine (`docs/fpan-im/INVENTORY.md` §4).
UNIB (2013) and MBTL (2026 build) carry the identical version string, so UNIEL/UNIST/UNICLR are almost
certainly 2.2.4 as well. That is a prediction: **verify with one `strings | grep "Squirrel "`** when a build
turns up.

### 1.2 Version identification (how we know it is 2.2.4, not 3.x)
- The literal `Squirrel 2.2.4 stable` is pushed as `_version_` by `sq_base_register`. In 3.x the string is
  `Squirrel 3.x …` and the base library is laid out differently.
- The base-library table (UNI2 0x9A3548) is exactly upstream 2.2.4: `seterrorhandler, setdebughook,
  enabledebuginfo, getstackinfos, getroottable, setroottable, getconsttable, setconsttable, assert, print,
  compilestring, newthread, suspend, array, type, dummy, collectgarbage`. 3.x adds `resurrectunreachable` and
  `callee`, which are absent.
- The 2.x-only default-delegate and stdlib names are present: `getclass`, `getstatus`, `wakeup`,
  `writeclosuretofile`, `vargc`/`vargv`. The 3.x-only `escape` and `subexp` are absent.
- Language level: the scripts use `<-`, `const`, `enum`, `foreach`, `delegate` and functions inside table
  literals, all of which 2.2.4 accepts. All 560 shipped Squirrel scripts of UNI2 and MBTL compile with the 2.2.4
  compiler plus FB's one lexer modification (§1.3, §3). That is the definitive test.
- Opcode set: no bytecode ships, so the bytecode format is irrelevant to the games. For reference, 2.2.4
  writes `0xFAFA` + `SQIR` + sizes + `TAIL`.

### 1.3 Custom VM modifications
**One modification, found and reproduced: the lexer is Shift-JIS-aware inside string literals.**
- In UNI2 1.42 `SQLexer_ReadString_FbSjis` 0x47B9F0 and MBTL 0x473430: when a byte in a string literal is a
  Shift-JIS lead byte (0x81–0x9F or ≥0xE0), the lexer copies it and the following byte verbatim, unless that
  byte is the closing quote.
- Upstream 2.2.4 does not do this. CP932 characters whose trail byte is 0x5C (`能`, `表`, `ソ`, …) are then read
  as `\` escapes. With unpatched 2.2.4, **7 MBTL and 18 UNI2 shipped scripts fail** with "unrecognised escaper
  char" or "newline in a constant", including `btl_MvFunc.txt` and `btl_StdMoveTbl.txt`.
- Reproduced as a marked patch in `third_party/squirrel-2.2.4/squirrel/sqlexer.cpp` (under `SQ_FB_COMPAT`).
  After it, every script compiles (§3).
- Comments need no such handling: a 0x5C byte inside a comment is harmless.
- UNIB and DFCI are very likely the same, but this is not yet verified in their IDBs (phase 1). The patch is
  harmless for them either way.

Everything else matches upstream:
- Every non-debug upstream string is present in UNI2 and MBTL. The missing ones are dead-code-stripped or
  unreachable:
  - `the class is locked`, `delagate cycle` and `invalid param type` are in unused `sq_*` APIs.
  - `internal compiler error: too many literals` is guarded by `_nliterals > 0x7FFFFFFF`, which is always false,
    and the check is still present in the older UNIB/DFCI builds.
  - `enumeration already exists` is commented out upstream.
  - the remaining `_OP_*`/dump strings exist only in `_DEBUG_DUMP` builds.
- The base-library names and arities match upstream.
- The load path is otherwise plain (UNI2 `Sq_CompileBufferAndRun` 0x8A67C0): the file bytes are read through the archive
  search paths, a NUL is appended, then `sq_compilebuffer(v, buf, size, "", raiseerror=1)`, `sq_push(roottable)`,
  `sq_call`. There is **no transcoding** (CP932 bytes go straight to the ANSI lexer) and no source preprocessing.
- Still to check (phase 1, cheap): diff the `SQVM::Execute` switch size and `SQCompiler` keyword table
  against upstream, to rule out a silently added opcode.

### 1.4 Global / VM setup (UNI2 `CSquirrel_ctor` 0x418640, MBTL `sub_4155C0`)
1. `sq_open(initial_stack)`, `sq_setprintfunc`, `sq_pushroottable`.
2. **stdlib, all registered:** `sqstd_register_bloblib`, `iolib`, `systemlib`, `mathlib`, `stringlib`, then
   `sqstd_seterrorhandlers`. Scripts can therefore reach `fopen`, `system`, `remove`, `dofile` and similar.
   This matters for the "run in the editor" sandbox (§2.4).
3. Root natives `SQ_LoadString(path)` (reads a file into a string) and `__dofile__(path)` (compile and run another
   script through the archive search paths; `./script/x.txt`).
4. Per-subsystem registration functions bind the native tables and classes (§1.5).
5. Load order: the game runs `script/Battle_Init.txt`, which `__dofile__`s `btl_Define_NoLocalDebug` or
   `LocalDebug`, then `btl_Define`, `btl_Define_Chara`, `btl_Define_CharaNew`, `btl_debugFunc`. It then runs
   `Battle_Std.txt` (`btl_MvFunc`, `btl_StdMoveTbl`, `btl_CmdCheck`, …). Per character it loads
   `%s/%s_mv_%d.txt`, `_sc_%d`, `_cmd_%d`, `_com_%d`, `_com_ranking_0`, `_se_list` (UNI2 builders
   `ChrScript_Build{Mv,Sc,Cmd,Com}Path` at 0x647390/0x647210/0x647530/0x6456E0). `%d` is the variant: 0, or
   6/7/8/9 for MBTL alternate projects, and 999 means no suffix (`npcNNN_mv.txt`).

### 1.5 Native API surface (generated from the IDBs)
Full tables, regenerable, with the native and wrapper address for each entry:
- `docs/squirrel/api/uni2_api.{json,md}`: **674 functions + 150 class methods, 525 constants, 60 class vars,
  65 tables/classes**.
- `docs/squirrel/api/mbtl_api.{json,md}`: **615 functions, 386 constants, 118 class vars, 61 tables/classes**.
  MBTL's compiler folded `Class::Func` into `TableBase::Func`, so methods show up as functions of their class
  table.

The main tables are `BMvTbl` (move/state: pattern, LP/SP/PP vars, move codes, input checks), `BMvEff`
(objects, camera, gauges, effects, throw), `BMvCore` (chara-data access, `CFindObject`), `BtlMvStd`, `BCMDTbl`
(command checks), `BCom` (CPU), `BSound` and `debuglog`. The constant tables are `BMvConst`, `BMvCoreConst`,
`BCMDConst`, `vConst`, `tDDC` and `tPAniConst`. The classes are `CCharaVector/Position/Rect/Point4`, `MvStatus`,
`MvHitStatus`, `MvBoundStatus`, `CmdInfo`, `CCharaData`, `CAnime`, and so on.

Signatures are inferred from the Sqrat wrapper template (its `sq_get*` parameter reads and `sq_push*`
return):
- UNI2: int 535, void 229, instance 41, value 9, string 5, float 5.
- MBTL: int 413, void 144, instance 39, value 9, string 5, float 5.

`string` parameters are read through `Sqrat::Object`, so they also accept ints (e.g.
`SetPattern("Name")` or `SetPattern(12)`).

**Cross-check against what the scripts use** (`tools/squirrel/sqrat_api_xcheck.py`, every `Table.Member` in all
shipped `.txt`):
- UNI2: 366 of 375 distinct members resolve.
- MBTL: 345 of 358 resolve.

The unresolved ones are:
- members whose name is a Japanese comment fragment;
- the `GRD_*` UNI leftovers in MBTL, and `Achievement_Increment`, `ClearHitInterrupt` and
  `SetGroundUkemi{Status,Finalize}`, which only appear in dead or `if(0)` code paths and are not registered
  by the exe. The validator will flag them as warnings.

**Cross-check against lumina:**
- `uni2_squirrelfuncs.md`: all 360 names are in our table, which has 714.
- `mbtl_squirrelfuncs_renaming_checklist.md`: all 151 natives are in our table (615). The 18 non-matches are
  lumina's helper and wrapper names.
- lumina's wrapper names are **wrong**. For example `Sqrat_Wrapper_IntParamVoidReturn` is really `int f(string)`.
  The IDB wrappers have been renamed `SqratWrap_<ret>__<args>` from the inferred signatures.

**How it is generated (for UNIB/DFCI and future builds):**
1. In IDA: `exec(open('tools/squirrel/ida_extract_sqrat_api.py').read())`, then
   `extract('GetPlayerSide', out)`, `classify_all(out, out)`, `attach_objects(out, out)`.
   - The binder is found from a seed name.
   - Registration functions are every caller of a binder.
   - String-literal calls are paired with their code call sites.
   - `this` is resolved from `ecx`, including Sqrat's chained `Func(..).Func(..)`.
2. `python3 tools/squirrel/sqrat_api_normalize.py <g>_raw.json <g>_api.json <g>_api.md`.
3. `classify_all` needs the `sq_*` names in the IDB. For a new build, align them from an already-named build
   with the wrapper call sequences (`uni2_callseq.json`), the same way MBTL was named from UNI2.

### 1.6 IDB names applied (both IDBs saved)
- **UNI2 1.42:**
  - `sq_*` API: getuserdata, getinteger, getfloat, getstring, tostring, getinstanceup, setinstanceup,
    createinstance, pushinteger, pushfloat, pushstring, pushnull, pushobject, pushroottable, push, pop, newclosure,
    newuserdata, createslot, throwerror, compilebuffer, call, open, getobjtypetag.
  - `sqstd_register_{blob,io,system,math,string}lib`, `sqstd_seterrorhandlers`, `sq_base_register`.
  - Sqrat binders: `Sqrat_TableBase_BindFunc` 0x418400, `Sqrat_Class_BindMethod`, `Sqrat_RootTable_BindTable`,
    `Sqrat_Enumeration_ConstInt`, `Sqrat_TableBase_SetValueInt`, `Sqrat_Class_Var{A..E}`,
    `Sqrat_ObjectFromStack`.
  - Loader: `CSquirrel_ctor`, `Sq_CompileBufferAndRun`, `Sq_DoFileViaSearchPaths`, `SqNative___dofile__`,
    `SqNative_SQ_LoadString`, `ChrScript_Build*Path`.
  - Registration: `BtlMv_RegisterSquirrelAPI_BMvTbl`, `BtlMvStd_RegisterSquirrelAPI`.
  - 68 `SqratWrap_*` wrappers.
- **MBTL 25646393:** the same `sq_*` and Sqrat set, aligned from UNI2. lumina's wrong guesses were corrected:
  `RegisterSquirrelFunction` became `Sqrat_TableBase_BindFunc`, `RegisterSquirrelConstant` became
  `Sqrat_TableBase_SetValueInt`, `RegisterSquirrelConstant2` became `Sqrat_Enumeration_ConstInt`,
  `Sqrat_RegisterClass` became `Sqrat_RootTable_BindTable`, `Sqrat_PushString` became `sq_pushstring`, and
  `Sqrat_PopStack`, `Sqrat_PushInteger` and `Sqrat_PushValue` became their `sq_*` names. 67 `SqratWrap_*`
  wrappers were renamed.
- **DFCI** (`Fighting Climax Ignition [SDCS]\RingGame.exe.i64`, saved 2026-10-15): an older MSVC kept one Sqrat template
  instance per bound signature (no shared `Sqrat_TableBase_BindFunc` + wrapper pointer). 80 binder instances named
  `<role>_<ADDR>` by call shape: `Sqrat_TableBase_BindFunc_*`, `Sqrat_Class_BindMethod_*` (the native is `__thiscall`),
  `Sqrat_Class_Var{A..D}_*`, `Sqrat_TableBase_SetValueInt_*`, plus `Sqrat_RootTable_BindTable`. Argument types come from
  each native's own prototype (the native is passed directly). Table: `api/dfci_api.json` (594 entries in 43 tables:
  BMvTbl 125, BMvEff 115, tDDC 62, BCMDTbl 26, CAnime 25, BMvCore 19, ...). Script: `tools/squirrel/ida_extract_sqrat_dfci.py`.
  `sqtool check <dfci> --api dfci_api.json --run`: Battle_Init + Battle_Std run clean (42 tables, 501 natives, 323 consts).
- **UNI[st]/[cl-r] arcade** (`C:\games\unib\RingGame.exe`, sha256 cf96fd7f..., IDB `C:\dev\ida\server\unib_arcade_RingGame.exe.i64`,
  saved 2026-10-15): the same older-compiler layout. `Sqrat_RootTable_BindTable` = 0x427DD0, 68 binder instances named by
  shape. Constant tables (`BMvConst`, `BMvCoreConst`, `BCMDConst`) use the (name, int) shape, so the normalizer turns a
  "var" on a `*Const` table into a constant. Table: `api/unib_api.json` (587 entries: 228 consts, 219 funcs, 81 vars,
  21 methods, 38 tables). The bootstrap loads its defines with the stdlib `dofile(".\\script\\...")`, which both VMs now
  resolve read-only inside the game root, like `__dofile__`. `sqtool --run`: Battle_Init + Battle_Std run clean
  (21 tables, 240 natives, 227 consts).

### 1.7 How scripts link to HA6
- **Move scripts (`_mv_`)** are tables `t.Mv_<Name> <- { DataPattern = N, Init_After, FrameUpdate_After, … }`.
  - `BMvTbl.SetPattern(name|id)` selects an HA6 pattern by its **PTCN code name** or number.
  - `GetUpdateFrameID()` reads the frame's **AFID**, which is how a move script reacts to specific frames
    (spawn objects, set flags).
  - `BMvEff.CreateObject({ DataPattern=…, … })` spawns effect patterns.
  - `BMvTbl.SetNextMoveTable`/`SetDeleteMoveTable` chain moves.
  - Hantei-chan's `mv_script.cpp` already parses this loosely, for the read-only spawn list.
- **Command tables (`_cmd_`)** are tables `cmd.Skill_236A <- { num, command="236A", CmdCheck={…},
  UpdateTable={name="Mv_…"} }`. They are the Squirrel equivalent of the MBAACC `_c.txt` and map an input
  to an `Mv_` table.
- **`btl_MvFunc` / `btl_StdMoveTbl` / `Battle_Std`** hold the shared move library (`Battle_Std.*`,
  `BtlMvStd.*`) that every character's `_mv_` calls.
- **`System/BtlCharaTbl.txt`** (Squirrel) is the roster and maps each character to its folder and variants.
- `_com_` is CPU AI; `_sc_` is the per-character setup; `_se_list` builds the SE tables (it `__dofile__`s
  `btl_SEDefine.txt`).

### 1.8 Text encoding and round trip
- The files are CP932 with CRLF. Some contain NEL (0x85) bytes inside Shift-JIS sequences; `file` reports
  "CRLF, NEL", but those bytes are SJIS trail bytes, not line breaks. `btl_Define_CharaNew.txt` has no line
  terminator at all.
- The VM is ANSI (`_charsize_ 1`), so string literals stay CP932 byte strings at run time. The editor must
  therefore:
  - load the raw bytes, decode CP932 → UTF-8 for display, and keep the original bytes;
  - for a **byte-exact save of untouched files**, write the original bytes back if the buffer is unmodified,
    or keep a per-line "dirty" map and re-encode only the edited lines;
  - re-encode edits UTF-8 → CP932 and refuse characters CP932 cannot represent, reporting the line;
  - preserve the line endings and the presence or absence of a final newline.
- The existing cmdfile editor already does CP932 round trips (`src/cmdfile/cmd_io.cpp`); reuse that codec.

---

## 2. Design for Hantei-chan

### 2.1 VMs
- **One vendored VM today: `third_party/squirrel-2.2.4`.** It is upstream with three marked portability
  patches and FB's Shift-JIS string-lexer modification (`PATCHES.md`). With `SQ_FB_COMPAT`, `SQInteger` stays 32-bit on x64 like the games (`1<<31`
  overflow, `%` and `/` match).
- Coexistence for a future 3.x game: build each VM as its own static library with symbols renamed
  (`-include sq2_prefix.h` mapping `sq_*` → `sq2_*`, plus a C++ `namespace sq2 {}` wrapper around the
  internals), behind a small `IScriptVM` interface (compile, run, call, stub registration, debug hooks).
  Squirrel 2 and 3 headers both define `SQInteger`/`HSQUIRRELVM`, so each VM's headers are included only from
  its own adapter `.cpp`. **Not needed until a 3.x game exists.**
- **Licences:**
  - Squirrel 2.2.4 is **zlib/libpng** (its `COPYRIGHT`), not MIT. MIT applies only from 3.0 on. Both are
    compatible; zlib requires altered sources to be marked, which `PATCHES.md` and the `HANTEI-CHAN PATCH`
    comments do.
  - Sqrat is zlib-licensed. We do **not** need Sqrat: the stubs are registered with the plain C API from
    data, and Sqrat's C++ templates would only add compile time.

### 2.2 Per-game engine profile
`docs/squirrel/api/<game>_api.json` → `res/squirrel/<game>.profile.json` (shipped), containing:
- the VM id (`sq2.2.4`), the stdlib set, the root natives (`__dofile__`, `SQ_LoadString`);
- the tables, classes and constants with values, and the functions with the inferred
  `ret(args)` and arity;
- the file layout: the path formats from §1.4, variants, `script/` load order and per-character file kinds.

Stubs are generated at VM start-up from the profile, as the prototype does with `InstallApi`. Three kinds:
1. **Pure stubs** return defaults and record calls (the prototype). Used for compile-plus-bootstrap checks.
2. **Mocks with state** for the preview simulator:
   - `GetLP`/`SetLP`/`AddLP`, `GetSP`, `SetPattern`, `GetUpdateFrameID` and `GetMuki` read and write a
     `PreviewCharacter` struct;
   - `CreateObject` spawns into the preview's effect list;
   - camera and quake calls are logged;
   - input checks (`CheckStickHold`, `CheckButtonHold`) read a scripted input timeline.
3. **Strict** mode: an unknown member is an error. This is the default for validation.

Class instances (`CCharaVector`, `MvStatus`, …) are mocked as Squirrel classes with the vars from the API
table; the Var offsets are in the JSON.

### 2.3 Bytecode tools
Not needed: no game ships bytecode. `writeclosuretofile`/`sq_readclosure` exist in the VM, so a disassembler is
about 300 lines on top of `SQFunctionProto` if it is ever wanted (debug view of a compiled function). It
stays out of the plan.

### 2.4 Script editor
- **Panel:** ImGui text editor (ImGuiColorTextEdit-style, or the existing cmdfile editor widget extended)
  with:
  - a Squirrel 2.2.4 grammar: the keyword list from `sqlexer.cpp`, including `vargc`, `vargv`, `delegate`,
    `resume` and `parent`;
  - CP932 load and save (§1.8);
  - fold by `t.Mv_*` / `cmd.*` blocks.
- **Autocomplete and hover** from the profile: `BMvTbl.` lists natives with signatures; constants show
  values.
- **Cross-links:**
  - `SetPattern("X")` / `DataPattern = N` jumps to the HA6 pattern (PTCN or index);
  - `GetUpdateFrameID()==N` jumps to the frame with AFID N;
  - from the HA6 frame panel, "scripts referencing this AFID / pattern" uses an index built on load from
    the same scanner as `mv_script.cpp`, upgraded to the real parser;
  - `_cmd_` `UpdateTable.name` jumps to `Mv_` tables.
- **Validation on save:**
  - compile with the game's exact VM, reporting errors with line and column (prototype `CompileBuffer`);
  - in strict mode, run the file's top level after the game's bootstrap (`Battle_Init`, `Battle_Std`),
    which catches unknown natives, typos in `Battle_Std.*` and missing `Mv_` tables;
  - static checks: AFIDs referenced but defined on no frame; patterns that do not exist.
- **"Run this move against the preview sim"**: `docs/HANTEI_PREVIEW_SIM.md` already ticks frames. Hook
  points:
  - on pattern enter, call the `Mv_*.Init_After`;
  - per tick, call `FrameUpdate_After`;
  - on AFID change, call `FrameID_Change`-style callbacks;
  - mock natives as in §2.2.

  The VM is deterministic and single-threaded, so it runs on the UI thread with a per-call instruction budget
  (`sq_setdebughook`) to stop runaway loops.
- **Sandbox:** the games register io and system (`system()`, `remove()`). Hantei-chan's VM registers
  **blob, math and string only**, and stubs `io`/`system` with read-only, game-root-confined versions.

### 2.5 Round trip
- Untouched files are written back from the original bytes, so they are byte-exact by construction.
- For edited files, CP932 is re-encoded only for changed lines. A test checks that every shipped script
  survives load → save byte-exact, and that a no-op edit (type a character, delete it) is byte-exact too.

---

### 2.6 Validation profiles (base game API + declared extensions)

Modders ship more than scripts. Exe patches (Cheat Engine tables such as `UNI2_Cha.CT`, see
`UNI2_CHA_CT_ANALYSIS.md`), fpan-im modules and loader DLLs can add natives, lift limits or change what a native
does. Validation therefore runs against a **profile stack**: one base game profile plus zero or more
**extension profiles**, each declared by the patch or module that provides it.

**Files (JSON, committed under `res/squirrel/`):**
- `res/squirrel/<game>.profile.json`: the base profile, generated from `docs/squirrel/api/<game>_api.json`
  (§2.2): `{ "kind": "base", "id": "uni2-1.42", "game": "uni2", "exe": { "sha1": "...", "version": "1.42" },
  "vm": "sq2.2.4-fb", "stdlib": [...], "tables": [...], "functions": [{ "name": "BMvTbl.SetPattern",
  "ret": "void", "args": ["string|int"] }, ...], "constants": [...], "vars": [...], "limits": { ... } }`.
  `limits` holds facts that scripts can trip over, for example `"csel.icons": 39` and `"chara.portraits": 39`.
- `res/squirrel/ext/<id>.ext.json`: an extension profile, with this shape:

```json
{ "kind": "extension", "id": "uni2cha-roster", "title": "UNI2_Cha roster extender",
  "base": ["uni2-1.42"],
  "source": { "type": "ce-table|fpan-im|exe-patch|dll", "ref": "UNI2_Cha.CT", "sha1": "..." },
  "adds":    { "functions": [...], "constants": [...], "tables": [...] },
  "changes": [{ "native": "BMvEff.X", "note": "returns 2 instead of 1 when ..." }],
  "removes": [],
  "limits":  { "csel.icons": 100, "chara.portraits": 256 },
  "behaviours": ["csel.roster>39"],
  "conflicts": ["other-ext-id"] }
```

  `behaviours` are free-form tags for effects that are not natives (for example `wallbounce.no-height-check`). A
  static check or the preview simulator can key on them. An fpan-im exe-patch module emits the same file next
  to its byte-patch records (fpan-im `DESIGN.md` §10), so one declaration feeds both the patcher and the
  validator.

**How a script or mod declares what it needs:**
- **Mod level (preferred).** The mod manifest (fpan-im `mod.json` / pcmod) gets
  `"squirrel": { "requires": ["uni2cha-roster"], "experimental": false }`. The editor's active stack
  is the base plus every extension the opened mod requires, and the mod packager refuses to build when a
  required extension is missing.
- **File level.** A pragma in a leading comment, which the game ignores:
  `// @hantei requires: uni2cha-roster, fpan-im-wallbounce` and `// @hantei experimental`. The parser reads only the
  first 20 lines, so the pragma survives the byte-exact round trip.
- The project or workspace can pin extensions for every file (UI toggle, saved in the session).

**Severity rules:**

| Finding | No extension active | Declared by an active extension | File marked `experimental` |
|---|---|---|---|
| Compile error (VM) | error, save needs "Save anyway" | error | error |
| Unknown native or table member (`Foo.Bar` not in the stack) | **error** | ok, shown as "ext: <id>" in hover | warning |
| Native used with an extension that `changes` it | n/a | info: "behaviour changed by <id>" | info |
| Exceeds a base limit (for example a 40th CSel icon) | error | ok when the extension raises the limit | warning |
| Requires an extension that is not installed or not found | n/a | error ("missing extension <id>") | warning |
| Two active extensions that conflict | n/a | error | error |
| Unknown member only in dead code (`if(0)`, as in the MBTL `GRD_*` leftovers) | warning | warning | warning |

Unknown natives are errors by default, because the game throws at run time, usually mid-match. `experimental`
downgrades the error to a warning for a single file. It never applies silently: the badge shows it.

**UI:**
- The Script panel toolbar shows `Profile: uni2-1.42 + [uni2cha-roster ×] [+ add]` chips.
- Each finding carries its source: base, ext id, or experimental.
- Hovering a native shows its signature and which profile provides it, for example `BMvTbl.SetPattern(string|int): void [uni2-1.42]`.
- Autocomplete lists base and extension members, and extension members carry a tag.
- Every save writes a status badge: "valid for uni2-1.42", "valid for uni2-1.42 + uni2cha-roster", or
  "experimental (2 warnings)".
- An extensions manager lists the installed ext files with their source (CE table, fpan-im module, DLL) and
  hash, and has a "derive from CE table" import, which only parses the table and never runs it.

**Implementation order:** this lands in P7.
- Base profiles come first, via `sqtool profile <api.json>`.
- Next, `ScriptVM::open` takes a profile stack. Natives from an extension are stubs that log
  `ext:<id>`.
- A static member pass, over the parser index from P4, then flags unknown `Table.Member`s before running.
- Today, P3 already gives part of this: an unknown native fails the top-level run after the bootstrap, and
  shows as a runtime warning at the line.

`UNI2_Cha.CT` is a useful calibration case. It adds no natives and changes no script behaviour. Its extension
file would only raise `limits`, so a script that relies on it validates the same way, and only the roster/CSel
data checks change.

## 3. Prototype (worktree `wt/squirrel`, branch `feat/squirrel`)

- `third_party/squirrel-2.2.4/`: upstream 2.2.4 stable (sha256 recorded) plus `PATCHES.md`.
- CMake `squirrel224` (static, `SQ_FB_COMPAT`) and `sqtool` (`src/squirrel/sqtool.cpp`, quill console log,
  static mingw exe).
- `sqtool check <gamedir> --api docs/squirrel/api/<g>_api.json --run [--report out.tsv]`:
  1. Compiles every `.txt` under `script/` and `data/` with the 2.2.4 compiler, from the raw bytes, exactly as
     the game does.
  2. Classifies files as Squirrel by the exe's loader path formats and reports pass/fail for each file.
  3. With `--run`, opens a game-shaped VM (the same stdlib set, `__dofile__`, `SQ_LoadString`), installs stubs
     from the API JSON and executes `Battle_Init.txt` + `Battle_Std.txt`.
- ctest:
  - `sqtool_fixture`: a hand-written FB-style script; no game data in git.
  - `sqtool_fixture_bad`: a syntax error must fail.
  - `sqtool_mbtl` / `sqtool_uni2`: the full game check. Skipped (77) when `gamedata/` is absent.

**Results (2026-10-15, offline, gamedata extracts):**

| | MBTL 25646393 | UNI2 1.42 |
|---|---|---|
| Squirrel scripts (script/ + per-character `_mv/_sc/_cmd/_com/_com_ranking/_se_list`) | **375 / 375 compile** (5.3 MB) | **185 / 185 compile** (5.3 MB) |
| …with upstream 2.2.4 and no FB lexer mod | 368 / 375 | 167 / 185 |
| other `.txt` (HA6 projects, BattleInfo, csel/edit, …; not loaded by the VM) | 164 (32 happen to parse) | 117 (32 happen to parse) |
| `--run` bootstrap `Battle_Init.txt` + `Battle_Std.txt` (all their `__dofile__`s) with generated stubs | ok (40 tables, 613 natives, 383 consts) | ok (52 tables, 822 natives, 523 consts) |

Also compile-only, with no API table yet:
- UNIB 1.04.03 (`D:\Games\unib`): **56 / 56** Squirrel scripts compile.
- DFCI Ignition 2.32.00: **171 / 171** compile.

Known prototype gaps:
- The extractor cannot yet tell a `Sqrat::ConstTable` (bare global constants, e.g. `_PosState_Air`, `_DevWindows`)
  from a named `Enumeration` (`WinMesTableConst.X`), because both bind under names like `BMvConst`. `sqtool`
  installs constants both ways. Phase 1 should identify the ConstTable constructor (UNI2 `sub_42FEF0`, MBTL
  `Sqrat_InitConstTable`) and record the container kind.
- Class vars (`MvStatus.*`) are not stubbed.
- MBTL class methods appear as functions of the class table, because MBTL folds `Class::Func` into
  `TableBase::Func`.
- Wrapper names inside `*_api.json` are the names at extraction time. The IDBs have since been renamed to
  `SqratWrap_*`; rerun the extractor to refresh them.

---

## 4. Plan

| Phase | What | Size |
|---|---|---|
| **P0** (done) | Version and format per game; vendored 2.2.4; IDB API extraction for UNI2 and MBTL; `sqtool` compile and bootstrap with stubs; ctest | – |
| **P1** | Run the extractor on the UNIB and DFCI IDBs (rename them first); `SQVM::Execute`/keyword-table diff to close "no VM mods"; MBTL Sqrat Var types; ship `res/squirrel/<game>.profile.json`; extend `sqtool --run` to every character's `_sc/_cmd/_mv/_com` after the bootstrap | S (1–2 days) |
| **P2** | CP932 document model shared with the cmdfile editor (byte-exact load and save, dirty-line re-encode) plus a round-trip test over every shipped script | S |
| **P2** status (2026-10-15, `feat/squirrel-editor`) | Done: `src/squirrel/script_document.*` (reuses `cmdfile::Cp932ToUtf8`/`Utf8ToCp932`). ctest `squirrel_doc_roundtrip`: 981 shipped .txt files (MBTL 539, UNI2 302, UNIB 140; 15.0 MB) round-trip byte-identical, also after an edit that is then undone | – |
| **P3** status | Minimal panel done: Tools > "Squirrel script editor", dockable "Script" window, game-folder file list, goossens ImGuiColorTextEdit with a Squirrel 2.2.4 language. Save: strict CP932 encode, then compile (and with an API profile, bootstrap and run the top level) in the sandboxed `ScriptVM`. A compile error blocks the save ("Save anyway"); a runtime error is a warning. Errors are marked at their line. Since 2026-10-15 also: line folding; hover shows each native's signature from the API profile (table, args, return type); Ctrl+Space opens a completion list (natives, Squirrel keywords, names the file declares) | – |
| **P3** | Script editor panel: syntax highlight, fold, compile-on-save errors, autocomplete and hover from the profile | M (3–5 days) |
| **P4** | Cross-links: a real-parser index of `Mv_*` tables, `DataPattern`, `SetPattern`, `GetUpdateFrameID` and `CreateObject`, linked both ways with HA6 patterns, frames and AFIDs. Replaces `mv_script.cpp`'s regex scan (closes parity F22's "read-only spawn list") | M |
| **P4** status (2026-10-15) | Hover and Ctrl+click on a pattern reference in any script: numbers after `pat=`, `start_pat=`, `DataPattern=`, `SetPattern(`; `def_PAT_*` constants (read from `script/*.txt`); PTCN code-name strings (`SetPattern("StdC_End")`, `pat="FB_NA"`); frame ids after `JumpFrameID(` or `case` (matched against AFID; the active tab's pattern wins, otherwise the first pattern holding it, with the others listed). A character folder resolves to its `<chr>_0.txt` (opened on Ctrl+click); `script/` files use the active tab. Still to do: the reverse direction (HA6 pattern → scripts) and replacing `mv_script.cpp`'s scan | – |
| **P5** status (2026-10-15) | Outline beside the editor: `_cmd_` entries with their `command = [...]` and the `Mv_` table they run, `X.Mv_* <-` move blocks, functions; filter, click to jump. Editing stays in the text (byte-exact) | – |
| **P5** | `_cmd_` command-table editor view (a structured table over the same document; parity F22 "no Squirrel `_cmd_` editor") | M |
| **P6** | Preview-sim hook: stateful mocks (LP/SP vars, pattern/frame, `CreateObject` into the preview effect list, input timeline); run `Init_After`/`FrameUpdate_After` per tick; instruction budget | L (1–2 weeks) |
| **P7** | Strict validation: bootstrap plus per-character run with unknown-native errors; AFID and pattern existence checks; "guaranteed to load" badge | S |
| **P8** | Multi-VM only if needed: a namespaced `sq3` library behind `IScriptVM` when a 3.x French-Bread game appears (Twi-Lumina day-1 check) | M, conditional |

## 5. Decisions for the user
1. **Single VM now?** Recommended: yes, 2.2.4 only, with the `IScriptVM` seam. Don't vendor 3.x speculatively.
2. **Sandbox:** decided: in-editor runs stub `file`/`dofile`/`loadfile`/`writeclosuretofile`/`system`/
   `remove`/`rename`/`getenv`, and log each call they block (`ScriptVM`). `sqtool` still mirrors the game.
3. **Validation:** proposed: validation profiles (§2.6). Unknown natives are errors unless an active
   extension declares them, and warnings in a file marked `experimental`.
4. **Editor widget:** decided 2026-10-15: goossens/ImGuiColorTextEdit (MIT), vendored in
   `third_party/ImGuiColorTextEdit` (`EDITOR_WIDGET_TRIAGE.md`).
5. **Where profiles live:** generated JSON committed under `res/squirrel/` (recommended; regenerate per game
   build), or generated at first run from user-supplied IDB exports.
6. **UNIEL/UNIST/UNICLR:** acquire a build to confirm 2.2.4. Only one `strings` check is needed; the extractor
   works unchanged once the IDB has the `sq_*` names.
