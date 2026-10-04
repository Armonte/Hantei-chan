# Rosa Chinensis Four hand (rosa_fh.exe 2002-12-28, "Rosa Chinensis -Four hand- / benibara rendan"): PAC, IMG, FOB, loaders

Source: `C:\dev\frenchbread\benibara_rendan\files\Rosa Chinensis Four hand\rosa_fh.exe` (IDB `rosa_fh.exe.i64` next to it; `Data\PAC.PAC` 14,434,112 B and the
sound PAC `Data\<oto>.PAC` 3,046,700 B) and the 2005 repack `C:\dev\frenchbread\yamayuri_rendan\files\omake\Rosa Chinensis Four hand\` (same layout, `Data\` v0 PAC, plain IMG).
Tool: `tools/fb/rosa_verify.py` (all numbers below come from it). Types: `docs/formats/ida/rosa_types.h` (all applied in the IDB). Tags: **T** traced in code, **D** verified on the files, **U** unproven.
The engine is the dMp engine (`docs/formats/dmp.md`) one year earlier: same VM, same PAC, same loose-file-first lookup, same actor pool; fewer natives, 20 script banks instead of 64.

## 0. Headline results (`python3 tools/fb/rosa_verify.py`, all green)
```
2002 PAC.PAC re-serialize byte-exact (flag=1)   1 / 1      2005 PAC.PAC re-serialize byte-exact (flag=0)   1 / 1
2002 sound PAC (3 x MP3) re-serialize           1 / 1      2005 sound PAC (3 x MP3) re-serialize           1 / 1
2002 IMG v4 encode(decode(x)) == x            100 / 100    2005 IMG v1 serialize(parse(x)) == x          100 / 100
2005 IMG converted to the 2002 v4 form == 2002 file: 99 / 100 (TITLE.IMG differs: the 2005 title has 8174 changed pixel bytes)
FOB (2002 + 2005) parse / decode / serialize byte-exact: 12 / 12  (6 per set; 2002 and 2005 FOB code differ)
```
* Rosa IMG = DMP.EXE's legacy "version 3/4" reader (`Img_ReadV3to4`): an **encrypted** 44-byte header block + RGB24 / 8-bit paletted pixels, **per-file keyed cipher** (file name + a 9-byte key in the exe).
  The cipher is fully reversible and is implemented as `img_stream` / `img4_decode` / `img4_encode`.
* Rosa FOB = same container as dMp (no index tables), **opcode numbers shifted by -1 from dMp 0x24 on** and only 20 natives (0x56..0x69). 12,056 instructions / 59,794 of 65,560 code bytes decode (91 %, the rest is data).
* The game defines **no sprites, no maps and no chips in scripts**: images are loaded by hard-coded C functions, sprite-rect tables are in the exe `.data`, the stage is `STAGE01.FOB` (BG layer commands + enemy spawn script).

## 1. Archives (T, D)
### 1.1 PAC format (`PackArchive_Open` rosa_fh 0x40B1F0, same as DMP.EXE 0x4121A0)
```
u32 plainFlag            1 = payload stored plain (2002 PAC.PAC and both sound PACs); 0 = first 9696 bytes of each entry enciphered (2005 PAC.PAC)
u32 count ^ 0xFA261EFB
{ char name[56]; u32 size ^ 0xFA261EFB; u32 offset }[count]     64 B; byte j of entry i stored as name[j] ^ (3*(j*i-28)) & 0xFF; offsets plain, payloads contiguous in index order
payloads                 entry 0 starts at 8 + 64*count (7368 for the 115-entry PAC.PAC)
```
Cipher v0 (`PackArchive_ReadEntry` 0x40B3A0): for k < min(size, 9696): `byte[k] ^= (k + UPPER(name)[k % strlen(name)]) & 0xFF` where name is the entry name (basename, upper-cased). Verified: 2005 payloads decipher to the same WAV/FOB/IMG bytes as the 2002 ones.
`rosa_verify.py` re-serializes all four PACs byte-exact from (names, payloads), including the raw 56-byte name fields.
Contents of `PAC.PAC` (115 entries): 100 IMG, 6 FOB (ENEMY01..04, PIANO, STAGE01), 9 WAV (`00 01 10..16`). The sound PAC (name = one kanji `oto`, cp932 0x89 0xB9, + `.PAC`): 3 MP3: `01 BGM01.MP3` (2,865,527), `02 OVER.MP3`, `03 CLEAR.MP3`.
### 1.2 Lookup order
`File_OpenLooseOrPack` (0x40B8B0): `CreateFileA(path)` relative to the working directory first, then `PackArchive_SizeFileAndGrowBuffer` / `PackArchive_ReadEntry` on slot `g_PackSearchArchive` by upper-cased basename. Paths the code uses:
`.\data\script\enemy01..04.fob`, `.\data\script\piano.fob`, `.\data\script\Stage%02d.fob` (the number is hard-coded 1: `sub_412570` returns 1), `.\data\cg\<name>.img` (+ `.\Data\cg\bom\bom_a|b|c|d%02d.img`), `.\Data\<oto>\NN.wav`.
`App_OpenArchives` (0x412140): slot 1 = `.\data\PAC.PAC` (`PackArchive_OpenSlot`), slot 0 = `.\data\<oto>.PAC` opened **and memory-mapped** (`SoundPack_Open` 0x4206D0 -> `PackArchive_MapView`: `CreateFileMapping` + `MapViewOfFile`; `PackArchive_EntryDataOffset(slot, off) = view + off`).
Archive record (24 B): `hFile, entryCount, entries, plainFlag, hMapping, mappedView` (DMP.EXE's `DmpPackArchive` +0x10/+0x14 are the same two fields: the dMp header calls them `unused_10` / `dataBase`; they are `hMapping` / `mappedView`, correct that).
### 1.3 WAV / MP3 (T)
WAV: `Wav_LoadIntoSlot(i, name)` -> `Wav_LoadSlotLooseOrPack` (0x40B830): loose `.\Data\<oto>\NN.wav` first, else the PAC entry, into a DirectSound buffer (`sub_406A40`). The 9 WAV are plain RIFF in both PACs. MP3: `Stage_Load` (0x4129C0) calls `Sound_LoadFromPack(0..2, "01 BGM01.MP3" ...)`: the entry is **not copied**,
the mapped bytes are handed to an in-exe MP3 stream decoder (`sub_404E80`, 40,040-byte per-stream objects, DirectSound streaming; no DirectShow). Both are loose-file overridable only for WAV; MP3 always comes from the mapped sound PAC.

## 2. IMG (T, D): Img_OpenAndReadHeader 0x402D90 -> Img_ReadV3to4 0x402B60 (DMP.EXE: 0x407CC0 -> 0x4079F0)
Dispatcher: reads u32 (discarded) then u32 `version`: 0,1,2 -> `Img_ReadV0to2` 0x402A40 (plain), 3,4 -> `Img_ReadV3to4` 0x402B60 (this section), else fail. All 100 files of the 2002 PAC.PAC are version 4.
### 2.1 Layout (v4, 44-byte header)
```
+0x00 u32 0                      (discarded)
+0x04 u32 4                      version (3 behaves the same)
+0x08 u8  nameSeed[16]           = (UPPER(file stem) repeated to 16 bytes) XOR g_ImgKey[j % 9]   (D: all 100 files)
+0x18 u32 flag     } five 32-bit fields, EACH enciphered by its own call of Img_DecryptStream (index restarts at 0 per field)
+0x1C u32 paletteCount (0 for 24 bpp, 256 for 8 bpp)
+0x20 u32 bitsPerPixel (24 or 8 in the shipped set; 4 and 16 are supported by Img_RowBytes)
+0x24 u32 width
+0x28 u32 height
+0x2C palette[paletteCount]      4 bytes each B,G,R,0; ONE cipher call over 4*paletteCount bytes
+...  pixels[height * rowBytes]  ONE cipher call; rows top-down (bottomUp = 0, confirmed by rendering TITLE.IMG/CH01.IMG), rowBytes = ((bpp*width+7)/8 rounded as below) & ~3:
      4 bpp: ((w+1)/2 + 3) & ~3,  8 bpp: (w+3) & ~3,  16 bpp: (2w+3) & ~3,  24 bpp: (3w+3) & ~3   (Img_RowBytes 0x4029C0; the `and al,0FCh` only clears bits 0-1)
      24 bpp = B,G,R per pixel; 8 bpp = index into the palette
```
File size = 44 + 4*paletteCount + height*rowBytes for all 100 files (D). Shipped mix: 24 bpp 128x128 x65 (BOM_*, ONP*), 256x256 x18, 512x512 x3 (BG01 BG02 BG_BASE), 640x480 (SYS01), 1024x1024 (TITLE); 8 bpp/256 colours: 32x32 (KAGE), 128x128 (1PLAYER), 256x256 x8 (CH01/02, ENE01..04, PIANO, TAMA2), 512x512 x2 (KURO, TAMA).
### 2.2 Cipher (`Img_DecryptStream` 0x41F4D0; DMP.EXE `sub_42E740`)
```
key  = g_ImgKey  = 8C 4B 92 4A 20 89 C2 97 F7 00      (rosa_fh.exe 0x432458, set into g_ImgKeyPtr by Img_InitKey 0x4029B0; length 9, cyclic)
stem = UPPER(basename without extension)[:16]          (Img_DeriveKeyStem 0x41F540: it also VERIFIES that the decoded nameSeed starts with the stem, else the load fails)
decrypt(buf):  for i in 0..len-1:   t = stem[i % len(stem)] ^ ((((i >> 3) & 0xFF) + (~(8*i) & 0xFF)) & 0xFF) ^ buf[i];   out[i] = (t - key[i % 9] - 109) & 0xFF
encrypt(buf):  t = (p + key[i % 9] + 109) & 0xFF;   out[i] = t ^ stem[i % len(stem)] ^ ((((i >> 3) & 0xFF) + (~(8*i) & 0xFF)) & 0xFF)
```
Every call restarts all three indices at 0: one call per header field, one for the palette, one for the pixels. There is no per-row cipher. The key bytes were first recovered from the data (`key[j] = nameSeed[j] ^ stem[j]`, 0 conflicts over 100 files) and then matched the 9 bytes in the exe.
The 16-byte nameSeed beyond the stem is not random: it is the stem repeated, so an editor can regenerate it (`img4_seed`); `rosa_verify.py` encodes with the regenerated seed and still gets byte-exact files.
### 2.3 What the engine does with the pixels (`Img_LoadToTexture` 0x4031A0, T)
`Img_LoadToTexture(path, splitAlpha, renderTarget, d3dFormat)`: reads the file (palette buffer 1024 B, so <= 256 entries), builds a `RosaImgBuffer` (bottomUp 0), creates a D3D8 texture of the image size and converts pixel by pixel (`Img_GetPixelBGRA` 0x403950) into the requested D3DFORMAT:
* format 25 (A1R5G5B5) is used by **every** loader except the BOM_C / BOM_D sheets (23, R5G6B5, no alpha); 20/21/22/24/26 are supported but unused.
* For alpha formats with no alpha plane the alpha is **colour-keyed**: transparent iff R=G=B=0 (black = transparent). `splitAlpha` (second argument) is 0 in every call: when non-zero the lower half of the image would be an 8-bit alpha plane (so 24 bpp sheets are plain colour + black key).
* Channels are reduced with `>>3` (5 bit) / `>>2` (6 bit) with a "non-zero stays >= 1" clamp.
### 2.4 2005 repack form (v1) and conversion
`Data\PAC.PAC` of the 2005 build stores the same 100 images as plain version-1 files: `u32 0; u32 1; u32 flag 0; u32 paletteCount; u32 bpp; u32 width; u32 height; palette; pixels` (28-byte header, pixels identical). `img1_encode(...)` / `img4_encode(...)` convert between the two:
2005 -> 2002 gives the original bytes for 99 of 100 files; `TITLE.IMG` differs because the 2005 title picture itself was edited (8,174 bytes), not because of the cipher.
### 2.5 Matching routines
rosa_fh.exe: `Img_OpenAndReadHeader` 0x402D90 (dispatcher), `Img_ReadV3to4` 0x402B60 (reader; stem + key check `Img_DeriveKeyStem` 0x41F540, cipher `Img_DecryptStream` 0x41F4D0, `Img_RowBytes` 0x4029C0, key init 0x4029B0), `Img_ReadV0to2` 0x402A40, `Img_LoadToTexture` 0x4031A0.
DMP.EXE: `Img_ReadV3to4` 0x4079F0 with `sub_42E740` / `sub_42E7B0` (= the same cipher / stem routines) and `Img_RowBytes` 0x407850. In DMP.EXE the key pointer `dword_1D80770` is **never initialised** (no writer in the image): the v3/v4 reader is dead code there; no DMP file uses it.

## 3. FOB (T, D)
### 3.1 Container (`Script_LoadBank` 0x40BD00): identical to dMp
`u32 nFuncs; {char name[32]; u32 pc}[n]; u32 codeSize; u8 code[codeSize]`, no index tables, raw 32-byte names (stale junk after the NUL, keep verbatim). All 6 files: header + codeSize == file size. Loader: 20 banks (`g_ScriptBanks[20]` 0x75EA70, 276 B), 50 threads (`g_ScriptThreads` 0x760218, 2148 B), value stack 512, `g_ScriptArgs[64]` 0x760014.
Strings: `\rLoadScript Error\r->`, `\rScriptBank is Full`, `\rScriptBank Error!!`, `\rNoFunction!!`, `LJumpSubError`, `\rStack Error!!`, `\r%d:ScriptUnknown Code!!`.
### 3.2 Opcode map vs dMp (the reason the dMp decoder stops after 85 instructions)
Rosa opcode N = dMp opcode N for **0x01..0x1F** (data, push, store, arithmetic, CMP/JCC/JMP/CALL, CALL_NAMED, RET/RET_FAR); from there dMp's YIELD (0x20) does not exist, so everything after shifts down by one:
| Rosa | dMp | op | | Rosa | dMp | op |
|---|---|---|---|---|---|---|
| 0x20 | 0x21 | EXIT_THREAD | | 0x2D | 0x2E | YIELD_OR_END |
| 0x21 | 0x22 | SPAWN_THREAD | | 0x2E | 0x2F | LINE (u32) |
| 0x22 | 0x23 | KILL_THREAD | | 0x2F..0x3F | 0x30..0x40 | MEM/STR (2F..37), FILE_* (38..3F) |
| 0x23 | 0x24 | SWAP | | 0x40, 0x41 | 0x41, 0x42 | NOP_NO_ADVANCE (shared stub) |
| 0x24 | 0x25 | SWITCH | | 0x42, 0x43 | 0x43, 0x44 | VM_SAVE_STATE / VM_LOAD_STATE |
| 0x25 | 0x26 | DEBUG_PRINT (u32) | | 0x44 | 0x45 | FORMAT_NUMBER |
| 0x26 | 0x27 | LOAD | | 0x45..0x49 | 0x46..0x4A | SLOT_INIT/ALLOC/FREE/GET_PTR/GET_SIZE |
| 0x27 | 0x28 | RAND_RANGE | | 0x4A | 0x4B | RET_VALUE |
| 0x28..0x2A | 0x29..0x2B | RAND_LCG2, RNG_SET, RNG_GET | | 0x4B..0x4F | 0x4C..0x50 | SINE_EASE, FILE_EXISTS, FILE_QUERY_VIRTUAL, GET/SET_ERRNO |
| 0x2B | 0x2C | WAIT_FRAMES_REALTIME | | 0x50 | 0x51 | FORMAT_PRINT (u32) |
| 0x2C | 0x2D / 0x55 | SPIN_NO_ADVANCE | | 0x53 / 0x54 | 0x54 / 0x55 | END / SPIN |
(dMp's 0x1E..0x1F RET_FAR/RET and 0x1C CALL_NAMED are unchanged.) Handlers in the IDB are renamed `ScriptOpXX_<Name>` with the Rosa number; `Script_RunThread` is 0x40ECF0.
### 3.3 Instruction lengths (verified: 0 decode errors on 12 files)
Same encodings as dMp 1.3: **01/02** DATA: 6+4n; **03/04** PUSH_IMM/PUSH_CODE_ADDR: 6; **05..08**: 2; **09** STORE: 6 (u16 flags, u16 kind 0/0x64..0x6B); **0A..17, 26**: 4 (u16 flags); **18** CMP: 6; **19** JCC: 10 (flags, kind, u32 target);
**1A/1B** JMP/CALL: 6; **24** SWITCH: 8 (flags, u32 tablePc -> {default, n, {key,target}[n]}); **25, 2E, 50**: 6 (u32); everything else (1C 1E 1F 20..23 27..2D 2F..4F 51..69): 2. Terminators for a decoder: JMP, RET, RET_FAR, EXIT_THREAD, SPIN (2C/54), RET_VALUE (4A), END (53).
No unknown opcodes occur; the highest is 0x64 in the shipped scripts. Every file starts with a run of `LINE` markers (pc 0) before the first named function.
### 3.4 Natives 0x56..0x69 (the only game-specific ops; (value,tag) pair arguments as in dMp 1.1)
| op | name | args | what it calls | uses |
|---|---|---|---|---|
| 56 | SYSTEM_COMMAND | 1 (argblock*): argblock[0] = cmd 1 = `Enemy01_Spawn` 0x407A30, 2 = `Enemy02Master_Spawn` 0x4089B0, 3 = `Enemy03_Spawn` 0x4091C0, 4 = `Enemy04Master_Spawn` 0x409D20, 0xA = `PianoMaster_Spawn` 0x41DE90 (block [1],[2] = x,y); each allocates an actor, sets its handlers/tables and runs the new enemy script's `StartInit`/`Ene0nMasterInit` | `Game_SystemCommand` 0x41CCC0 | 22 |
| 57 | SPAWN_ACTOR | 1 (desc*, up to 20 ints: [0] mode, [1..9] -> shot type/position/velocity, [10..19] extra when mode 1) | `Actor_SpawnShotFromDesc` 0x40A390 -> `Actor_SpawnShot` 0x40A290 | 35 |
| 58 | SPAWN_EFFECT | 6 (type, x, y, a, b, c) | `Actor_SpawnEffectOffset` 0x40A0D0 | 0 |
| 59 | GAME_GET_FIELD_77A99C | 1 (*out) | reads dword 0x77A99C (`sub_4125A0`) | 1 |
| 5A | MATH_ANGLE | 2 (*out, int[4] x1,y1,x2,y2) | `Math_AngleDeciDegrees` 0x41A120 (0..3599) | 1 |
| 5B | ENTITY_CALL_COLLIDE | 2 (&actor, arg) | `actor->commandHandler(actor, arg)`: slot +0x28 of `RosaActor` (dMp: +0x24, which the dMp header names `collide`; it is the script -> actor command handler, 1700 uses in dMp) | 92 |
| 5C | NEXT_UNIQUE_ID | 1 (*out) | `Script_NextUniqueId` 0x41CDA0 | 0 |
| 5D..62 | BG_SET_LAYER_PARAMS (cmd, int[]; cmd 1,2,3,4,other), BG_SCROLL_BY (dx,dy), BG_SET_SCROLL_LIMITS (mask, 4 values), BG_SET_VIEWPORT (8 values, -1 = keep), BG_GET_SCROLL (sel, out), BG_SET_ORIGIN (x,y) | | `Bg_*` 0x401400..0x401680 | 5/2/3/3/2/0 |
| 63 | PLAYER_SLOT_COMMAND | 2 (cmd, int[3]) | `Player_SlotCommand` 0x41B410 (cmd 0 -> `sub_41B3D0(x,y,z)`) | 1 |
| 64..67 | SOUND_LOAD_FROM_PACK (slot, name), SOUND_SET_VOLUME, SOUND_RELEASE_BUFFER, SOUND_SET_FADE (3) | | `Sound_*` 0x420510..0x4205C0 (the MP3 streams) | 2/0/0/0 |
| 68, 69 | TIMED_EVENT_CREATE (6), TIMED_EVENT_ADJUST (3) | | `TimedEvent_Create` 0x41CE40 / `TimedEvent_AdjustField` 0x41CF10 | 0 |
Usage over the 6 files (opcode:count): `03:2945 04:817 06:935 07:302 08:1153 09:1153 0A:1421 0B:47 0C:28 0D:11 0E:56 11:1 16:1 18:144 19:164 1A:101 1B:86 1C:10 1E:73 1F:1 20:36 23:206 24:22 27:34 2B:4 2E:2109 56:22 57:35 59:1 5A:1 5B:92 5D:5 5E:2 5F:3 60:3 61:2 63:1 64:2 01:6 02:21`.
### 3.5 Entry points per file (named functions; engine calls them by name)
STAGE01.FOB: `Init`, `Start`. ENEMY01: `Init`, `CreateEnemy01`, `StartInit`, `Start`, `DamageCallback`, `OnpuStartInit`, `OnpuStart`. ENEMY02 / ENEMY04 / PIANO: `Init StartInit Start EneNNMasterInit EneNNMasterMain InitReCreateEneNNMaster EneNNMasterDelete` (NN = 02, 04, 10).
ENEMY03: `Init StartInit Start DamageCallback`. Engine call sites: `Stage_Load` runs `Init` then `Start` of STAGE01; enemy spawn functions run `StartInit` (and `Ene0nMasterInit` for the masters); `Enemy_LoadAssets` (0x4070B0) runs `Init` of ENEMY01 once; `Piano_LoadAssets` (0x41D120) runs `Init` of PIANO.
Decode coverage: ENEMY01 32,760/34,706 B (6,790 insn), ENEMY02 4,666/5,654, ENEMY03 4,044/4,878, ENEMY04 4,922/5,736, PIANO 7,390/8,242, STAGE01 6,012/6,344; the rest is string/table data.
### 3.6 How scripts define sprites, stages, enemies and BG in this game (T)
* **Sprites / frames:** no sprite-definition natives exist (dMp's GFX_* ops 0x73..0x77 are absent). Sheets are plain IMG files laid out as regular grids (CH01 = a grid of numbered cells, TITLE = fixed UI rectangles); the **UV rectangles are 48-byte `RosaGfxSpriteRect` tables in the exe `.data`** (e.g. 0x4340F8 for the effect sheet, 0x432EE8, 0x4334C8, 0x4389B8 for the masters) selected by `RosaActor.frameIndex` (`Actor_ApplyFrameRect` 0x41A1A0). Scripts only choose frame numbers through `actor->commandHandler(actor, arg)` (opcode 0x5B, command number in `arg`), set fields through `PUSH_CONTEXT`/`PUSH_ARGS_ADDR` pointers into the actor/context block and call SPAWN_ACTOR. A sprite editor therefore edits (a) the IMG sheets and (b) the rect tables in the exe; (c) FOB data only for frame numbers.
* **Stage:** `STAGE01.FOB` `Init` (`PLAYER_SLOT_COMMAND`, `BG_SET_SCROLL_LIMITS`, `BG_SET_LAYER_PARAMS` for layers 0..3, scroll limits/viewport) and `Start` (a timed thread: `WAIT_FRAMES_REALTIME`, `GAME_GET_FIELD_77A99C` progress checks, `SYSTEM_COMMAND` spawns, `SOUND_LOAD_FROM_PACK` for the BGM). The stage number is fixed to 01 in the exe.
* **BG / "chips":** there are no tile maps or chip data. The background is three 512x512 24-bpp pictures (`BG_BASE`, `BG01`, `BG02`, loaded by `Bg_LoadTextures` 0x401740 with format 25) drawn as scrolling layers whose parameters are set by BG_* natives; `KURO` (black overlay), `KAGE` (shadow) and `TAMA/TAMA2` (bullets), `BOM_A..D` (explosion frames, 12/12/17/18), `ONP00..23` (note effects), `SYS01` (HUD), `TITLE`, `PIANO`, `1PLAYER`, `CH01/02`, `ENE01..04` complete the sheets.
* **Enemies:** `SYSTEM_COMMAND` 1..4, 0xA create the five enemy types (script file + image + update/handler functions are fixed per cmd in the exe); behaviour = the script's `Start`/`*MasterMain`/`DamageCallback`.
* **Other game data files:** none (no score/config reads found beyond the PACs; sound = the 9 WAV + 3 MP3).

## 4. Loader chain and reload seams (analysis, not run)
Boot `App_OpenArchives` -> per scene loaders. Battle: `Stage_Load` 0x4129C0 (resets via `sub_40A510 sub_412370 sub_41B220 sub_4016C0 sub_420F50 sub_407090 sub_4080B0 sub_408C10 sub_409420 sub_41D110`, `Script_UnloadBanksFrom(-1)`, `Script_FreeAllThreads`, loads `Stage01.fob`, runs `Init`/`Start`, starts the MP3s).
Textures (each = `Img_LoadToTexture(path, 0, 0, 25)` into a global texture pointer): `Bg_LoadTextures` 0x401740, `Enemy_LoadAssets` 0x4070B0 (ENE01..04 + ENEMY01.FOB), `Shot_LoadTexture` 0x409F80 (TAMA), `Fade_LoadTexture` 0x40A530 (KURO), `Piano_LoadAssets` 0x41D120, `Player_LoadTextures` 0x41E320 (CH01, CH02, TAMA2, 1PLAYER), `Bomb_LoadTextures` 0x420F90 (BOM_A..D), `Title_LoadTextures` 0x421DF0 (TITLE, KURO).
* **IMG reload for one sheet:** release the old `IDirect3DTexture8` held in the global and call `Img_LoadToTexture(path, 0, 0, fmt)` again (same fmt as above); a loose file `.\data\cg\<name>.img` (2002 v4 or v0..2 form) is picked up first. No other state caches the texture; UV tables are in the exe.
* **FOB reload:** `Script_UnloadBanksFrom(bank)` (kills the bank's threads, frees image + func table) then `Script_LoadBank(path)`, re-run `Init` (and for enemies the spawn). Scripts are looked up by path+label on every callback; script globals live in the image (reset on reload). Same hazards as `dmp.md` section 5.2; safe point = between frames of the single-threaded main loop.

## 5. IDA artefacts (rosa_fh.exe.i64)
Types from `rosa_types.h`: enums `RosaScriptOpcode RosaScriptStoreKind RosaScriptCmpKind RosaScriptOperandFlags RosaScriptFrameMarker RosaD3dFormat RosaActorKind`; structs `RosaScriptFuncEntry RosaScriptBank RosaScriptThread RosaPackEntry RosaPackArchive RosaImgFileV4 RosaImgFileV1 RosaImgBuffer RosaGfxSpriteRect RosaActor`.
Globals typed/renamed: `g_Vm* g_ScriptArgs[64] g_ScriptScratchBuf g_ScriptThreads[50] g_ScriptBanks[20] g_PackArchives[2] g_PackArchiveOpened g_OpenLooseFile g_PackBuf* g_ImgKey g_ImgKeyPtr g_D3DDevice`. ~100 functions renamed/prototyped (File_*, PackArchive_*, Script_*, Img_*, Actor_*, Bg_*, Sound_*, loaders) and 96 opcode handlers `ScriptOpXX_*`.
## 6. Unproven / not done
* Meaning of most `RosaActor` fields (named only where a loader or spawn native writes them; the rest `untyped_xx`). The exe `.data` sprite-rect tables were identified by their consumers, not catalogued one by one.
* Natives 0x58 / 0x5C / 0x64..0x69 argument layouts come from their callees (no shipped script uses 0x58, 0x5C, 0x65..0x69).
* Arg counts of unused ops are by analogy with dMp; the opcode shift mapping was derived by handler structure/size and confirmed by the 0-error decode of all 12 files.
* The MP3 decoder (`sub_404E80` and DirectSound streaming), WAV buffer creation and input/UI/game logic are untyped.
* The 2005 exe (`Rosa Chinensis Four hand.exe`) was not disassembled; its v0 PAC and v1 IMG forms are inferred from the data and from the shared dMp/Rosa reader code.
