# Drill Milky Punch (DMP.EXE 2003-10-20): FOB scripts, IMG, DEMO.DAT, loaders, live-reload seam

Source: IDB `C:\dev\frenchbread\dmp_1020\DMP.EXE.i64` (base 0x400000, md5 bb0bd329...). Data: `C:\dev\frenchbread\dmp_1020\DATA\GAMEDATA.PAC`
(132 entries). Tool: `tools/fb/dmp_fob_probe.py` (parse + recursive-descent decode + re-serialize + `--dis`). Types: `docs/formats/ida/dmp_types.h`
(all applied in the IDB). Evidence tags as in the header: **T** traced in code, **D** verified on the shipped files, **U** unproven.

Headline results
* FOB = **no index tables** (RBO has them), flat u16 opcode (RBO's class+sub collapsed). **58/58 FOB files** parse, decode (376,668 instructions, 0 errors,
  2,035,734 of 2,421,664 code bytes decoded = 84 %, the rest is string/table data reached only through `PUSH_CODE_ADDR`) and re-serialize byte-exact.
* IMG v6 = 20-byte header + **raw 16-bit pixels, NOT obfuscated**; the D3D format (A1R5G5B5 or A4R4G4B4) is chosen by the caller, not the file.
* Every script / image / demo is opened **loose file first** (`.\data\...` relative to the game directory), then from the PACs by basename.
  So the live-reload route for assets needs no PAC rewrite.
* The engine already has a "reload every texture of the scene" path (the device-lost recovery): `Scene_Release(0)` + `Scene_Init(0)`.
* DEMO.DAT = `DmpMatchSetup` (68 B) + replay blob (12 B header + 108001 x 24 B input rows) = 2,592,104 bytes, verified on all 6 files.

## 1. FOB file format (`Script_LoadBank` 0x412D40)

```
u32  nFuncs
{ char name[32]; u32 pc }[nFuncs]      36 B each. name is NUL-terminated, the bytes after the NUL are stale writer junk
                                       (e.g. 68 00 34 04 at +28): keep the raw 32 bytes for byte-exact output  [D, 58 files]
u32  codeSize
u8   code[codeSize]                    loader mallocs codeSize+2 and zeroes the last 2 bytes; pcs are BYTE offsets into this block
```
Size check: header + codeSize == entry size for all 58 files, no trailing bytes. INITCONFIG.FOB: 98 bytes = 4 + 36 + 4 + 54.
Not present vs RBO (`fob_vm.md`): the `nIndexTypes` + index tables block. The names are the only entry points (looked up by `Script_FindFuncPc`).
The code block also holds the scripts' data (strings, int tables, event/pattern rows) and is their **mutable global memory** (`PUSH_CODE_ADDR` pushes
`codeImage + off`).  A run of `LINE` markers (pc 0 .. first function) precedes the code in every file (a source-line preamble); the probe uses pc 0 as an
extra decode root.

Runtime bank (`DmpScriptBank`, 276 B, `g_ScriptBanks[64]` 0x94C1F8): `name[260]` (the UPPER-CASED path string passed to `Script_LoadBank`, e.g.
`.\DATA\SCRIPT\CHARA\CHARA01.FOB`), `codeImage`, `codeImageBytes` (= codeSize+2), `funcCount`, `funcTable` (malloc(36*n), verbatim copy of the entry table).
`Script_LoadBank` returns the existing slot if the upper-cased path is already loaded (no reload). Slot full: on-screen "RscriptBank is full" and -1.

### 1.1 VM state and calling convention (T)
Per-thread record `DmpScriptThread` (2148 B, `g_ScriptThreads[50]` 0x950910): `allocated, name[64], stack[512], sp, codeImage, pc, bank, line, waitFlag,
waitStartMs, waitDurMs`. `Script_BindThreadContext(t)` publishes pointers to the current thread's fields in globals: `g_VmStackBase` (0x96AEA0, `&stack[0]`),
`g_VmSpPtr` (0x950700, `&sp`), `g_VmCodeImagePtr` (0x94C1EC, `&codeImage`), `g_VmPcPtr` (0x95080C, `&pc`), `g_VmBankPtr` (0x96AE9C), `g_VmLinePtr`
(0x9506F8). Other VM globals: `g_ScriptArgs[64]` 0x95070C (engine <-> script argument/result array: `Script_SetArg/GetArg`), `g_ScriptContext`
(0x96AEA4, pushed by PUSH_CONTEXT, `Script_SetContext`), `g_ScriptScratchBuf[256]` 0x96AD9C, `g_ScriptErrno` 0x9506FC, memory slots
`g_ScriptMemSlots` 0x950708 + `g_ScriptMemSlotCount`.
Stack: dword array, element 0 unused, `stack[sp]` = top, 512 max (overflow: `Script_ReportStackOverflow` prints "Rstack error" and the op fails).
Natives take **(value, tag) pairs**: the script pushes value then tag; the native pops the tag first. tag 2 = reference (value is an address);
for "value" parameters tag 2 means dereference. Any failing handler rewrites the opcode at pc to `END` (0x54) and ends the slice.
`Script_RunThreads(t)` runs one thread (or all 50 when t == -1) until it yields/ends; `Script_RunThread` is the opcode switch (0x415D80).
A call frame is two dwords (return pc, marker 27 = `CALL`, 28 = `CALL_NAMED`/`Script_CallFunc` which also saves the bank; `RET_FAR` unloads the callee bank
(`Script_UnloadBanksFrom(current)`) and restores the caller's).

### 1.2 Engine-side use of a script (the thread idiom, T)
Every engine -> script call is the same: `cur = Script_GetCurThread(); t = Script_AllocThread(); Script_SelectThreadIfReady(t); Script_SetThreadName(t, label);
Script_SetArg(...); Script_CallFunc(bankPath, label); Script_RunThreads(t); Script_SelectThreadIfReady(cur); read Script_GetArg(...)`. The thread ends with
`EXIT_THREAD` (op 0x21). Callbacks (e.g. `Event_OnAttack` -> `KougekiCallback`) are looked up **by bank path and label every time**, so no persistent pointer
into the code image exists between frames except long-running threads spawned by `SPAWN_THREAD`/`WAIT` and the script memory itself.

### 1.3 Instruction encoding (T, D)
Every instruction starts with a u16 opcode. Lengths in bytes (verified by the 58/58 decode):

| opcodes | len | operands after the u16 opcode |
|---|---|---|
| 01 DATA_DWORDS, 02 DATA_STRING | 6+4n | u32 n; n dwords (identical handler `ScriptOp01_02_SkipData`; 02 is what the compiler uses for string literals) |
| 03 PUSH_IMM, 04 PUSH_CODE_ADDR | 6 | u32 |
| 05..08 (PUSH_SCRATCH_ADDR, PUSH_ARGS_ADDR, PUSH_CONTEXT, POP) | 2 | |
| 09 STORE | 6 | u16 flags; u16 kind `DmpScriptStoreKind` (0 `=`, 0x64 `+=` 0x65 `-=` 0x66 `*=` 0x67 `/=` 0x68 `%=` 0x69 `&=` 0x6A `\|=` 0x6B `^=`) |
| 0A..17 binary/unary arithmetic, 27 LOAD | 4 | u16 flags (bit0 deref right/top, bit1 deref left) |
| 18 CMP | 6 | u16 flags; u16 kind `DmpScriptCmpKind` (eq ne le ge lt gt = 0..5) -> pushes 0/1 |
| 19 JCC | 10 | u16 flags (value 2 = deref); u16 kind (compare the single operand with 0); u32 target; pops the operand |
| 1A JMP, 1B CALL | 6 | u32 target (CALL pushes (retpc, 27)) |
| 25 SWITCH | 8 | u16 flags; u32 tablePc -> `{u32 default; u32 n; {u32 key; u32 target}[n]}` |
| 26 DEBUG_PRINT, 2F LINE, 51 FORMAT_PRINT | 6 | u32 (LINE: source line -> `*g_VmLinePtr`) |
| everything else (1C 1E 1F 20..24 28..2E 30..50 54 55 57..A1) | 2 | none; native arguments on the stack |
Unhandled opcodes (no `case` in `Script_RunThread`): **0x90, 0x91** (occur in STAGE01/02 after PUSH_IMM, 2 bytes) and **0xA1** (sentinel after the last YIELD of
INITCONFIG/ZAKOTEXTURELOAD): the slice ends with the on-screen "ScriptUnknown Code!!". 0x41/0x42 and 0x2D/0x55 do not advance pc (spin).
Terminators for the decoder: JMP, RET, RET_FAR, EXIT_THREAD, RET_VALUE, END, SPIN, 0xA1.

### 1.4 Opcode table (name = `DMPOP_*` enum, handler = `ScriptOpXX_<Name>` renamed in the IDB)
"args" = (value,tag) pairs popped, exact for the opcodes marked * (checked against the stack height of every shipped call), tag-test count otherwise.
Used = occurrences in the 58 shipped files.

Core (handlers 0x413370..0x416B60):
`01/02 DATA 0x413370 (3319/82)` `03 PUSH_IMM 0x4133A0 (69733)` `04 PUSH_CODE_ADDR 0x413400 (21081)` `05 PUSH_SCRATCH_ADDR` `06 PUSH_ARGS_ADDR (31808)`
`07 PUSH_CONTEXT (2924)` `08 POP (34826)` `09 STORE 0x4135A0 (34805)` `0A ADD (35306) 0B SUB (115) 0C MUL (1965) 0D DIV (135) 0E MOD (32) 0F SHL 10 SHR 11 AND (274) 12 OR 13 NOT 14 XOR 15 LNOT 16 LAND (48) 17 LOR (58)`
`18 CMP 0x414170 (2907)` `19 JCC 0x414310 (2967)` `1A JMP (4455)` `1B CALL (1447)` `1C CALL_NAMED 0x414690` `1E RET_FAR (969)` `1F RET` `20 YIELD (65)` `21 EXIT_THREAD (627)`
`22 SPAWN_THREAD 0x414860 (file,label) -> thread id in g_ScriptArgs[0]` `23 KILL_THREAD 0x4148E0` `24 SWAP (3509)` `25 SWITCH (360)` `26 DEBUG_PRINT (32)` `27 LOAD`
`28 RAND_RANGE* (out*,max) (451)` `29 RAND_LCG2 (second generator 0x430700)` `2A RNG_SET_STATE` `2B RNG_GET_STATE` `2C WAIT_FRAMES_REALTIME* (1) (4)` `2D SPIN_NO_ADVANCE` `2E YIELD_OR_END` `2F LINE (119926)`
`54 END` `55 SPIN_NO_ADVANCE_55` (0x55 is also what `Script_CallFunc` writes at pc when a label is missing: "LJumpSubError").

Memory/string/file/slot natives (same behaviour as RBO 09.xx/07.xx/08.xx, names by analogy where unused): `30 MEM_STORE16 31 MEM_COPY16 32 STR_COPY 33 STR_COPY_N 34 STR_LENGTH
35 STR_COMPARE 36 STR_FIND 37 MEM_COPY_DWORDS 38 MEM_FILL_DWORDS`, `39 FILE_WRITE_BUFFER 3A FILE_READ_VIRTUAL 3B FILE_GET_SIZE 3C FILE_OPEN 3D FILE_READ 3E FILE_WRITE 3F FILE_SEEK 40 FILE_CLOSE`,
`41/42 NOP_NO_ADVANCE`, `43 VM_SAVE_STATE 44 VM_LOAD_STATE (U: whole VM to/from a file, Script_RebindThreadImage per thread)`, `45 FORMAT_NUMBER (String_FormatNumber 0x4304D0)`,
`46 SLOT_INIT 47 SLOT_ALLOC 48 SLOT_FREE 49 SLOT_GET_PTR 4A SLOT_GET_SIZE` (script memory slots, 8-byte records {ptr,size}), `4B RET_VALUE`, `4C SINE_EASE`, `4D FILE_EXISTS 4E FILE_QUERY_VIRTUAL 4F GET_ERRNO 50 SET_ERRNO 51 FORMAT_PRINT`.
File natives read through the same loose-then-pack layer (section 3). None of these is used by the shipped scripts.

Game/engine natives (the ones the shipped scripts use are exact):
| op | name | args | engine function | uses |
|---|---|---|---|---|
| 57 | STAGE_ENEMY_LIST | 2* (cmd, enemyId): -1 clear, 0 append | `Stage_EnemyListCommand` 0x41DAB0 (`g_StageEnemyIds[22]`, `g_StageEnemyCount`) | 11 |
| 58 | ENEMY_SPAWN | 1* (desc*) | `EnemyType_SpawnByDesc` 0x429E70 -> `DmpEnemyType.spawn` | 26 |
| 59 | STUB_POP_REF | 1 | none | 0 |
| 5B | GAME_GET_FIELD_BF7FDC | 1 (*out) | `Setup_GetField_BF7FDC` 0x41CCB0 | 0 |
| 5C | MATH_ANGLE | 2* (*out, int[4]) | `Math_AngleDeciDegrees` 0x426760 (atan2 -> 0..3599) | 42 |
| 61 | ENTITY_MOTION | 2* (entity ref, params*) | `Entity_ApplyMotionCommand` 0x427760 (switch on params[1], writes entity +860 or +1052 sub-block) | 257 |
| 62 | ENTITY_CALL_COLLIDE | 2* (&entity, arg) | `entity->collide(entity, arg)` (DmpEntity +0x24) | 1700 |
| 63 | NEXT_UNIQUE_ID | 1 (*out) | `Script_NextUniqueId` 0x429F00 | 0 |
| 67..70 | BG_* (SET_LAYER_PARAMS 2, SCROLL_BY 2, SET_SCROLL_LIMITS 5, SET_VIEWPORT 8, GET_SCROLL 2, SET_ORIGIN 2, 6E SET_CAMERA_MODE 2, 6F COPY_STATE 1, 70 SET_LAYER_IMAGE_NAME 3) | | `Bg_*` 0x401710..0x402570 | 0 |
| 73 | GFX_ALLOC_IMAGE_TABLE | 1* (count) | `Gfx_AllocImageTable` 0x401000 | 2 |
| 74 | GFX_ALLOC_SPRITE_TABLE | 1* (count) | `Gfx_AllocSpriteTable` 0x401110 | 2 |
| 75 | GFX_SET_IMAGE | 5* (idx, path ref, type 0/1, width, height) | `Gfx_SetImage` 0x401070 | 20 |
| 76 | GFX_DEFINE_SPRITE | 10* | `Gfx_DefineSprite` 0x4011A0 | 2 |
| 77 | GFX_DRAW_SPRITE | 8* (spriteIdx, layer, x, y, r, g, b, alpha) | `Gfx_DrawSprite` 0x4014E0 | 2 |
| 7E | PLAYER_SLOT_COMMAND | 2 | `Player_SlotCommand` 0x4288A0 | 0 |
| 7F | GAME_QUERY | 2* (mode, &inout) | `Game_QueryState` 0x41E750 (0: entryKind+gameMode, 1: time-attack sel, 2: frame+limit, 3: time left, 4: object search ...) | 68 |
| 80..83 | SOUND_LOAD_FROM_PACK 2, SOUND_SET_VOLUME 2, SOUND_RELEASE_BUFFER 1, SOUND_SET_FADE 3 | | `Sound_*` 0x42FA00.. | 0 |
| 84 | AUDIO_ALLOC_CHANNELS | 1 (count of 32-byte records) | `Audio_AllocChannelTable` 0x42FFB0 | 0 |
| 85 | AUDIO_DEFINE_CHANNEL | 3* | `Audio_DefineChannel` 0x430010 | 6 |
| 86 | AUDIO_SET_LEVELS | 5* | `Audio_SetChannelLevels` 0x430040 | 2 |
| 87 | AUDIO_SET_STATE | 2* | `Audio_SetChannelState` 0x430070 | 6 |
| 88 | AUDIO_GET_STATE | 2 | `Audio_GetChannelState` 0x430090 | 0 |
| 89..8F | SOUND_LOAD_SLOT 3, SET_PLAY_POINTER 3, INIT_PLAYBACK 2*, CHAIN_SLOTS 4, PREPARE_CHANNEL 1*, PLAY_LOOPING 1*, STOP_STREAM 1* | | `Sound_*`/`Bgm_*` 0x42F8B0..0x42F9B0 | 0/0/4/0/4/4/12 |
| 90, 91 | (no handler; RBO analogues Pause/Resume) | 0 | none, "ScriptUnknown Code!!" | 2 + 2 |
| 92..99 | SOUND_LOAD_FILE 2, STOP_SLOT 1, IS_BANK_PLAYING 2, AUDIO_PLAY 2, PLAY_BANK_ONESHOT 1, STORE_VOICE 3, CMD_ONE 1, RELEASE_SLOTS 1 | | `Sound_*` | 0 |
| 9D | SOUND_CHANNEL_SET_UNIQUE | 2* (idx, value) | `Sound_ChannelSetUnique` 0x430220 | 263 |
| 9F | TIMED_EVENT_CREATE | 6 | `TimedEvent_Create` 0x429FA0 | 0 |
| A0 | TIMED_EVENT_ADJUST | 3 | `TimedEvent_AdjustField` 0x42A080 | 0 |
| A1 | END_OF_CODE (no handler) | | | 1 |
Only 51 distinct opcodes occur in the shipped scripts (list in the probe output). `Gfx_DefineSprite` takes `u1 = (srcX+srcW)/texW`; the per-pattern rows of
`InitPatternData` store `u1px` directly (section 2.3).

Arg-count caveat: the unused natives' counts are tag-test counts of the handler text (U-grade); the 25 used ones are exact.

## 2. What each script file is for (T, D)

Paths (script strings): `.\data\script\InitConfig.fob`, `.\data\script\stage\StageNN.fob` (NN from `String_FormatNumber`, 2 digits),
`.\data\script\chara\chara01..12.fob` (`g_ScriptCharaPaths[12][260]` 0x455548), `.\data\script\enemy\Enemy000..039.fob`
(`g_ScriptEnemyPaths[40][260]` 0x44C40C) and `.\data\script\enemy\ZakoTextureLoad.fob`. Enemy ids 900/910/980 use their own loader functions
(`g_EnemyTypeTable` rows 40..42, `sub_410530`); ENEMY900/910 have 4 and 3 functions.

| file(s) | functions | role |
|---|---|---|
| INITCONFIG.FOB (98 B) | `GameInit` | run once by `GameMain_Init`; sets `g_ScriptArgs[0] = -1`. If it were != -1 it would be the stage script set (`Setup_SetStageScriptSet`). |
| STAGE01/02.FOB (12872 / 14178 B) | `Init(stage)`, `Start`, `BossDead`, `PrintBg`, `ChangeStageBgm PlayBossStageBgm FadeOutStageBgm PauseStageBgm RestoreStageBgm StopStageBgm` | one per stage script set. `Init` defines the background image table (`GFX_ALLOC_IMAGE_TABLE` + 10x `GFX_SET_IMAGE` of `.\Data\BG\bg00..09.img`, type 0, 1024x512), `Start` fills the enemy list (`STAGE_ENEMY_LIST`), `PrintBg` draws the layers (called each frame by `Bg_DrawAndRunPrintBg`). Banks stay loaded for the whole match (`g_StageScriptBank`). |
| ZAKOTEXTURELOAD.FOB (3660 B) | `LoadTextureCallBack(i)` | returns the IMG path for texture slot i in `g_ScriptArgs[0]` (a switch); 18 `.\Data\grp\*.img` names. `Event_LoadTextures` loops i = 0.. and calls `Tex_LoadImageIntoSlot(path, 25, &g_TexEnemy[i])` (D3DFMT_A1R5G5B5). |
| CHARA01..12.FOB (77..83 KB, 16..19 functions) | `LoadTextureCallBack(i)` (i = 0..15 -> player texture slot), `InitPatternData(i)` (i = 0..73 -> `DmpPatternData`), `InitCharaInfo`, `InitCharaFunc`, `InitCharaButtonInfo(0..2)`, `StartInit`, `SetNormalShotInfo`, `SetExShotInfo`, `CheckStun`, `YarareCallback`, `GameSetCallback`, `act_loop`, `normal_act_callback`, `kabe_hit_callback`, `init_yarare_obj_create`, `yarare_obj_anime`, `tama_*` (7 files) | per character (index `Setup_GetSlotChara(seat)` = chara number - 1). `Player_LoadAll` (0x42A460) runs InitPatternData x74 / InitCharaInfo / InitCharaFunc / InitCharaButtonInfo x3 per seat; `Player_LoadTextures` (0x42AAE0) runs `LoadTextureCallBack(0..15)` -> `g_TexPlayer[seat*16+i]`. The rest are runtime callbacks (move scripts, damage, hit, game-set). |
| ENEMY000..039.FOB (18..49 KB, 3..12 functions) | `InitFunction(frame, entryKind!=0)`, `InitPatternData(i)` (i = 0..47), `LevelCheck`, `StartInit`, `Start`, `YarareCallback`, `Yarare`, `YarareCheck`, `PatternChangeCallback`, `AnimeEndCallback`, `move_callback`, `KabeYarareCallback`, `KougekiCallback`, `EtcCallback`, `Erase`, `create_zako` | one per enemy type (`DmpEnemyType`: `loadScripts` = `Event_LoadResources` 0x40DA80 loads all 40 banks, `loadTextures` = `Event_LoadTextures`, `releaseAssets` = `Enemy_ReleaseAssets`, `spawn` = `EffectEnemy_Spawn` 0x40E090, `onLevelCheck` = `Event_CheckLevel` 0x40D9F0). |

### 2.1 Loader order (T)
`App_RunSceneLoop` -> `App_InitSubsystems` (0x41AF60: `PackArchive_InitTables`, `PackArchive_OpenGameArchives` 0x41A820: slot 0 = `.\Data\SoundData.pac`, slot 1 = `.\Data\GameData.pac`) ->
`Scene_Init(1)` (scene 1 = battle):
1. `GameMain_Init(1)` (0x41E050): `Script_LoadBank(InitConfig.fob)` -> `GameInit` -> `Script_UnloadBanksFrom(-1)` + `Script_FreeAllThreads` -> build `.\data\script\stage\StageNN.fob`
   (`g_StageScriptPath`, NN = stage script set) -> `Script_LoadBank` -> `Init(stage)` and `Start` threads (these fill the image table / enemy list) ->
   for each id in `g_StageEnemyIds`: `g_EnemyTypeTable[id].loadScripts(1)` -> `Player_LoadAll(1)` -> per seat `Event_Init`.
2. `GameMain_Load(1)` (0x41E470): `Gfx_LoadAllTextures(1)` (the script-defined image table: `Tex_LoadImageIntoSlot(path, slot.d3dFormat, &slot.texture)` for each `DmpGfxImageSlot`) ->
   enemy `loadTextures()` -> `Player_LoadTextures` -> `SysObj_LoadTextures` -> `SystemCg_LoadTextures` (sysfont.img, p_font00.img ...).
3. `GameMain_Release(phase)`: textures (`Gfx_ReleaseAllTextures`), `Player_ReleaseAssets`, enemy `releaseAssets`; phase 1 additionally `Script_UnloadBanksFrom(g_StageScriptBank)`.
`phase` = 1 full, 0 = textures only (device recovery).
Other scenes: title (2) `sub_438590/sub_438620`, character select (3) `CharaSel_Init/CharaSel_LoadAssets` (chr_sel.img, chr_sel_wall.img, `ta_bust\ta_<name>.img`).

### 2.2 Pack and loose lookup (T)
`File_OpenLooseOrPack(path)` (0x4128A0): `CreateFileA(path)` (relative to the working directory) -> `g_OpenLooseFile`; on failure `PackArchive_LoadFileToBuffer` (0x4127F0):
searches slot `g_PackSearchArchive` (-1 -> 2) down to 0 for the **basename, upper-cased** (`Path_BaseName` after the last `\`), reads the whole entry into `g_PackLoadBuffer` (grown to fit).
`File_ReadCursor/SkipCursor/GetOpenSize/CloseCursor` then serve either the handle or the buffer. Used by `Script_LoadBank`, `Img_OpenAndReadHeader`, `Replay_LoadFile`, the FILE natives.
PAC format (`PackArchive_Open` 0x4121A0): u32 plainFlag (1 in both shipped PACs = payload stored plain; 0 = first 9696 bytes of each entry XOR-enciphered by the entry name),
u32 count ^ 0xFA261EFB, then count x 64-byte entries: name[56] (byte j of entry i stored as `name[j] ^ (3*(j*i-28))`), u32 size ^ 0xFA261EFB, u32 offset. GAMEDATA.PAC: 132 entries, SOUNDDATA.PAC: 86.

### 2.3 InitPatternData contract (T)
`InitPatternData(i)` returns through `g_ScriptArgs`: [0] count n; [1] -> script array of n rows of 10 ints `{texW, texH, u0px, v0px, u1px, v1px, ofsX, ofsY, ofsZ, ofsW}`
(becomes `DmpGfxSpriteRect`: u0 = u0px/texW, v0 = v0px/texH, u1 = u1px/texW, v1 = v1px/texH, ofs floats, 0.1/texW, 0.1/texH, texW, texH); [2], [3] -> n-int tables;
[4] count + [5] -> 6-int records; [6] count + [7] -> 5-int records; [8..11] raw ints. Player_LoadAll/Event_LoadResources copy everything out of the image into mallocs
(`DmpPatternData`, 36 B; players `g_PlayerPatterns[6][74]` 0x1D66830, enemies `g_EnemyPatterns[40][48]` 0x93AC20), after `memset`-ing the record: **the previous mallocs are not freed (leak)**.

## 3. IMG (T, D)
```
u32 reserved0      read and discarded (0 in all 68 files)
u32 version        6 in all 68 files (switch in Img_OpenAndReadHeader 0x407CC0: 0..2 -> Img_ReadV0to2 0x4078D0, 3..4 -> Img_ReadV3to4 0x4079F0, 6 -> Img_ReadV6 0x407C20)
u32 paletteFlag    0 in all files (selects bit depth 16)
u32 width, height
u16 pixels[width*height]    raw, row-major; size = 20 + 2*w*h verified for all 68 files
```
There is **no per-file obfuscation or compression**: `Img_ReadV6` mallocs `2*w*h` and reads it. The pixels are copied unconverted into the locked D3D texture (`Tex_CopyRaw16Pixels` 0x4091E0,
`DmpImgBuffer.rowBytes == -1`). The meaning of the 16-bit words is decided by the texture format the **caller** creates: `Tex_LoadImageIntoSlot(path, d3dFormat, &tex)`:
25 = D3DFMT_A1R5G5B5 (every Player/Enemy `LoadTextureCallBack` load and `GFX_SET_IMAGE` type 0), 26 = D3DFMT_A4R4G4B4 (`GFX_SET_IMAGE` type 1); the system CG loads
pass their own constants (U). `Tex_ConvertAndFill` (0x408180) handles the generic converters for the legacy v0..v4 readers (formats 20..26) and is not used for v6.
Texture dimensions equal the file's (1024x1024, 1024x512, 512x512, 256x256, 16x16 in the shipped set). Reader versions 0..2/3..4 (palette / BMP-like) are not exercised by shipped files (U).
The existing `ParseImg` therefore reads dMp IMG correctly if "format 0 = ARGB1555" is only a default: dMp does not name the format in the file.

### 3.1 IMG versions 3 and 4 (legacy reader `Img_ReadV3to4` 0x4079F0) - fully decoded from the Rosa Chinensis Four hand data (see `docs/formats/rosa.md` section 2)
Not used by any dMp file (the key pointer `dword_1D80770` is never written in DMP.EXE, so the reader is dead code there) but the format is fully known:
```
+0x00 u32 0 | +0x04 u32 3 or 4 | +0x08 u8 nameSeed[16] = (UPPER(file stem) repeated to 16 bytes) XOR key[j % 9]
+0x18 five u32 fields, each enciphered separately: flag, paletteCount, bitsPerPixel (4/8/16/24), width, height
+0x2C palette[paletteCount] x 4 (B,G,R,0)   +...  pixels[height * rowBytes]   (one cipher call each; rows top-down, 4-byte aligned: Img_RowBytes 0x407850)
key = 8C 4B 92 4A 20 89 C2 97 F7 (from rosa_fh.exe 0x432458; DMP.EXE has no key)
decrypt: t = stem[i % len(stem)] ^ (((i >> 3) + ~(8*i)) & 0xFF) ^ c[i]; out[i] = (t - key[i % 9] - 109) & 0xFF   (index restarts at 0 per call; stem = upper-cased basename without extension, max 16)
```
Python proof (decode + re-encode byte-exact for all 100 Rosa IMG, 2005 plain v1 round trip): `tools/fb/rosa_verify.py`. Versions 0..2 (`Img_ReadV0to2` 0x4078D0) are the same layout without the cipher and with a 28-byte header `0, version, flag, paletteCount, bpp, width, height`.
Corrections found while decoding Rosa: `DmpPackArchive` +0x10 / +0x14 are `hMapping` / `mappedView` (not unused/dataBase); the script op called "ENTITY_CALL_COLLIDE" (0x62, slot +0x24 of the actor) is a generic actor command handler (`commandHandler`, +0x28 in Rosa), not necessarily collision.

## 4. DEMO.DAT / Replay (T, D)
`TitleMenu_AttractMode` (0x439600): after 600 idle frames sets `g_ReplayFilePath` to `.\Data\Demo01..06.dat` (`g_DemoPaths`) and calls `Replay_LoadFile` (0x41BF10: `File_OpenLooseOrPack`,
read 68 B into `g_MatchSetup`, read 2,592,036 B into `g_ReplayHeader`.., `g_ReplayPlayback = 1`, cursor = 0). Recording: `Replay_LatchFrameInputs` (0x41BD90) stores `g_FrameInput[0..5]` (= `Input_GetRawWord(seat)`)
into `g_ReplayInputFrames[g_ReplayFrameCursor++]` every sim tick (capacity 108000 = 30 min at 60 fps); `Replay_WriteFileAtMatchEnd` (0x41BE90) writes the same two blobs to
`.\Data\Replay*.dat` (names at 0x45075C: Replay.dat, Replay2M.dat ...).
```
+0x00  DmpMatchSetup (68 B)  reserved0=0 | seed | lives | scoreAttackTimeSel | configC | configD | stageScriptSet | entryKind | gameMode | stage | playerCount | slotChara[6] (-1 = empty)
+0x44  u32 header = 0
+0x48  u32 frameCount            (demos: 13133, 10199, 15743, 7615, 12689, 17008)
+0x4C  u32 frameCursor           (== frameCount in the files; reset to 0 on load)
+0x50  DmpReplayFrame[108001]    24 B each = 6 u32 raw input words (seat 0..5); frames >= frameCount are zero
total = 80 + 24*108001 = 2,592,104
```
Shipped demos (D): DEMO01 `3,2,3,0, set 1, kind 0, mode 1, stage 2, 6 players, chara 0..5`; DEMO02 `mode 0, stage 9, 1 player, chara 5`; DEMO03 stage 4, 6 players; DEMO04 `3,3,3` stage 3 1 player;
DEMO05 stage 8, 4 players (3,5,1,4); DEMO06 stage 7, 5 players. Seed = 0 in every demo (`Rng_Seed(Setup_GetSeed())` in `GameMain_Init`), so playback is RNG-exact only if the sim state matches.
Input word bits seen: 0x04/0x08 = menu up/down, 0x10 = confirm (`TitleMenu_AttractMode`); gameplay words 16/18/20/26... (individual bit meanings: U).

## 5. Live reload seam (analysis; NOT exercised in a running game)
Single-threaded main loop (`App_RunSceneLoop` 0x41B6F0): `Win_PumpMessages -> Scene_StepAndDispatch -> Sound_PumpQueue -> Input_PollDevicesToRawWords`. All script threads and entity updates run inside
`Scene_StepAndDispatch -> ... -> GameMain_SimTick` (0x41EB20: `Rng_Next`, input latch, `EnemyType_RunSpawners`, **`Script_RunThreads(-1)`**, `TimedEvent_RunList`, `Obj_UpdateAllLists`, `Obj_CollideAll`, draw).
**Safe point = on the main thread between two `Scene_StepAndDispatch` calls** (e.g. a hook at `Sound_PumpQueue` 0x430470's call site or `Win_PumpMessages`), never inside a tick. Sound stream threads
(`SndStream_ThreadProc`, `Bgm_StreamThreadProc`) do not touch VM/image state (T, by reading the thread procs' callees; U for completeness).

### 5.1 IMG sheet / texture (preferred: the engine's own path)
* **Whole scene, using the game's loaders**: `Scene_Release(0); Scene_Init(0);` with `g_SceneCur` unchanged. This is exactly what `sub_41B010` does after a lost device. Phase 0 = textures only:
  `GameMain_Release(0)` -> `Gfx_ReleaseAllTextures(0)` (Tex_ReleaseSlot on every `DmpGfxImageSlot.texture`, tables kept), `Player_ReleaseAssets(0)`, enemy release(0), `SystemCg`..; then
  `GameMain_Load(0)` -> `Gfx_LoadAllTextures(0)`, enemy `loadTextures`, `Player_LoadTextures`, `SysObj_LoadTextures`, `SystemCg_LoadTextures` -> each calls `Tex_LoadImageIntoSlot`, which re-reads the IMG
  (loose file first). Script banks, pattern tables, entities and RNG are untouched. Cost: re-reads ~every IMG of the scene (tens of MB).
* **One sheet**: find the slot holding the texture (`g_GfxImageTable[i].texture` for a script-table image; `g_TexPlayer[seat*16 + k]`, `g_TexEnemy[k]` 0x94BA20, CharaSel/title slots for system images),
  then `Tex_ReleaseSlot(&slot)` (0x408110: `IDirect3DTexture8::Release`, slot = 0) and `Tex_LoadImageIntoSlot(path, d3dFormat, &slot)` (0x409430) with the SAME `d3dFormat` the original load used
  (25 for players/enemies/type-0 images, 26 for type-1 images). The device keeps its own ref on a texture currently set (`Gfx_SetTextureCached` 0x409920 caches `g_GfxCurTexture`), so releasing is safe; the cache is
  refreshed by the next draw. Alternative without touching the slot pointer: lock the existing texture and run `Tex_CopyRaw16Pixels` (only valid if width/height/format are unchanged).
  Replaces: the D3D texture object only; nothing else caches UVs (the `DmpGfxSpriteRect` UVs derive from the width/height in `DmpGfxImageSlot` / script rows, so a **resized** sheet also needs its script rows changed).
* Frees: the old texture is released by `Tex_ReleaseSlot`; the 2*w*h pixel buffer is freed by `Tex_LoadImageIntoSlot`.
* Loose override: place the edited file at `<game dir>\data\grp\<name>.img` (or `data\system\`, `data\BG\`, paths in the table above); no PAC edit is needed. Basename match is case-insensitive.

### 5.2 FOB bank
There is **no engine reload path for scripts** (banks are loaded once per scene and `Script_LoadBank` returns the cached slot). The loader pieces:
`Script_UnloadBanksFrom(bank)` 0x4130A0 (frees every thread bound to the bank via `Script_FindThreadByBank`, frees `codeImage` and `funcTable`, zeroes the 276-byte slot; `-1` = all 64),
then `Script_LoadBank(path)` 0x412D40 (re-reads, loose first, takes the lowest free slot). Because every engine -> script call re-resolves the bank by path + label (section 1.2),
a bank that has **no live threads** can be swapped between ticks:
1. at the safe point, `Script_UnloadBanksFrom(idx)` (kills the threads bound to that bank, e.g. stage `PrintBg`/`Start` threads and any `SPAWN_THREAD` children - the stage bank holds the thread that draws the background, so re-run `Init`/`Start` for it);
2. `Script_LoadBank(path)`;
3. re-run the load-time entry points that copied data out of the old image, because those copies are independent mallocs:
   * CHARAnn: `Player_LoadAll(1)` (re-runs `InitPatternData` x74, `InitCharaInfo`, `InitCharaFunc`, `InitCharaButtonInfo` for every seat; it `memset`s `g_PlayerPatterns[seat]` / `g_PlayerCharaInfo[seat]` first and leaks the old mallocs, which keeps any stale pointers valid but unused);
   * ENEMYnnn: `Event_LoadResources(1)` (all 40 enemy banks, or the enemy-specific loader for 900/910); stage: re-run `GameInit/Init/Start` threads like `GameMain_Init`;
   * texture names come from `LoadTextureCallBack`: follow with 5.1 (`Player_LoadTextures` / `Event_LoadTextures`).
**Hazards**
* The code image is also the scripts' **mutable global memory**: any reload resets script globals (counters, flags). PovertyCaster treats "dwords != pristine file copy" as the saved state; an editor reload must either accept the reset
  (safe between rounds / at the character select) or re-apply the runtime delta for the dwords the edit did not touch.
* Absolute pointers into the old image: values pushed by `PUSH_CODE_ADDR` that are still on a live thread's stack, `g_ScriptArgs[1,2,3,5,7]` after an `InitPatternData` call (only valid during the load), `g_ScriptContext`
  is an entity pointer (not in the image). Killing threads with `Script_UnloadBanksFrom` removes the stack hazard; an in-place patch is possible only if `codeSize` and every function pc are unchanged (then `memcpy` over `codeImage`
  keeps all pointers and `Script_RebindThreadImage(thread)` 0x412B90 is a no-op).
* `Script_UnloadBanksFrom` with a bank index that does not match the path's slot, and the stage bank index `g_StageScriptBank` (0x450F14) cached by `GameMain_Init`: after unload + load the slot index can change; refresh `g_StageScriptBank`.
* Do not reload while `Script_RunThreads` is active (not re-entrant: it uses the global VM pointers), during the first frames after `GameMain_Init`, or while a replay/netplay savestate is being captured (the bank set is part of the state).
* DEMO files: `Replay_LoadFile` (0x41BF10) re-reads the file and resets the cursor, usable from the title only (`TitleMenu_AttractMode`); loose `.\Data\DemoNN.dat` overrides the PAC.

## 6. IDA artefacts created
Types (`docs/formats/ida/dmp_types.h`, all applied): enums `DmpScriptOpcode DmpScriptStoreKind DmpScriptCmpKind DmpScriptOperandFlags DmpScriptFrameMarker`; structs `DmpScriptFuncEntry DmpScriptBank DmpScriptThread DmpPackEntry DmpPackArchive
DmpImgFileHeader DmpImgBuffer DmpGfxImageSlot DmpGfxSpriteRect DmpEnemyType DmpMatchSetup DmpReplayFrame DmpReplayFile DmpPatternData DmpPlayerCharaInfo`.
Globals typed + renamed: `g_VmSpPtr g_VmCodeImagePtr g_VmPcPtr g_VmBankPtr g_VmLinePtr g_VmStackBase g_ScriptArgs[64] g_ScriptContext g_ScriptErrno g_ScriptScratchBuf g_ScriptMemSlots g_ScriptThreads[50] g_ScriptBanks[64]
g_PackArchives[3] g_PackArchiveOpened g_OpenLooseFile g_PackBuf* g_GfxImageTable g_GfxSpriteRects g_GfxSpriteObjects g_StageEnemyIds g_StageScriptPath g_StageScriptBank g_EnemyTypeTable[44] g_EnemyPatterns g_PlayerPatterns
g_PlayerCharaInfo g_MatchSetup g_ReplayFrameCount g_ReplayFrameCursor g_ReplayInputFrames[108001] g_FrameInput[6] g_ReplayFilePath g_ScriptCharaPaths g_ScriptEnemyPaths g_DemoPaths`.
Functions: all 125 opcode handlers renamed `ScriptOpXX_<Name>`, ~140 loader/VM/engine callees renamed + prototyped (`Script_*`, `File_*`, `PackArchive_*`, `Img_*`, `Tex_*`, `Gfx_*`, `Bg_*`, `Sound_*`, `Audio_*`, `DebugText_*`, `Game*`).

## 7. Unproven / not done
* Entity body (`DmpEntity.body[1468]`, the sprite-object fields written by `Gfx_DefineSprite`/`Entity_ApplyMotionCommand`) is not typed; only the header links/`update`/`collide` slots are named.
* Handler semantics of the unused natives (30..44, 4C..51, 59, 5B, 67..70 BG_*, 7E, 80..83, 88..99, 9D..A0) are by analogy / callee name; arg counts for them are approximate. 0x90/0x91 have no handler in this build.
* `DmpPatternData` and `DmpPlayerCharaInfo` fields are named by their source (`g_ScriptArgs[n]`), not by gameplay meaning.
* Legacy IMG readers v0..v4 (`Img_ReadV0to2`, `Img_ReadV3to4`) only skimmed; no shipped file uses them.
* The reload procedures in section 5 are derived from the code, not run (the game must not be run by this task).
