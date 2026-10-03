# Drill Milky Punch (DMP.EXE 2003-10-20) data formats -- PARTIAL (first pass)

Source: IDB `C:\dev\frenchbread\dmp_1020\DMP.EXE.i64`. Tool: `tools/fb/dmp_fob_probe.py`.
Status: FOB header + VM instruction lengths PROVEN from the interpreter; whole-file round trip NOT yet done (see 1.3).
Sections 2-4 (loaders, DEMO.DAT, IMG decode) NOT STARTED.

## 1. FOB

### 1.1 File layout (`Script_LoadBank` 0x412D40) -- differs from RBO: NO index tables
```
u32  nFuncs
{ char name[32]; u32 pc }[nFuncs]     36 bytes each; name is NUL-terminated but the bytes after the NUL are stale
                                      writer-stack junk (e.g. 68 00 34 04) -> keep the raw 32 bytes for byte-exact output
u32  codeSize
u8   code[codeSize]                   loader mallocs codeSize+2 and zeroes the last 2 bytes
```
Verified: CHARA01 (19 funcs: LoadTextureCallBack, InitPatternData, InitCharaFunc, ...) 4+19*36+4+76796 = 77488 = entry size.
INITCONFIG.FOB: 1 func `GameInit` pc 12, codeSize 54, total 98. All 58 files satisfy header+codeSize == file size.
Runtime bank (`DmpScriptBank` 276 B, 64 slots at g_ScriptBanks 0x94C1F8): name[260], codeImage, codeImageBytes, funcCount, funcTable.
Load fails/overflow: "RscriptBank is full" / "RloadScript Error" on screen, returns -1. Already-loaded name returns the slot (no reload).

### 1.2 VM dialect (`Script_RunThread` 0x415D80, ops 0x01..0xA0, 133 live opcodes)
Flat u16 opcode (RBO's class+sub collapsed into one word). Value stack of 512 dwords of (value, tag) pairs; tag 2 = reference
(deref). pc is a byte offset into codeImage. Instruction lengths (from handler pc advances):

| opcodes | length | operands |
|---|---|---|
| 01, 02 DATA | 6+4n | u32 n; n dwords (skipped) |
| 03 PUSH_IMM, 04 PUSH_CODE_ADDR (codeImage+off) | 6 | u32 |
| 05..08 (push &g_ScriptArgs-like globals 05/06/07, 08 POP) | 2 | |
| 09 STORE | 6 | u16 flags(bit0 deref rhs); u16 kind (0 '=', 0x64 += 0x65 -= 0x66 *= 0x67 /= 0x68 %= 0x69 &= 0x6A \|= 0x6B ^=) |
| 0A..17 binary/unary arithmetic (ADD..) | 4 | u16 flags (bit0 rhs deref, bit1 lhs deref) |
| 18 CMP | 6 | u16 flags; u16 kind (eq ne le ge lt gt = 0..5) |
| 19 JCC | 10 | u16 flags (==2 deref); u16 kind (==0 !=0 <=0 >=0 <0 >0); u32 target; pops cond |
| 1A JMP, 1B CALL (pushes ret pc,27) | 6 | u32 target |
| 1C CALLFUNC(file,label) | 2 | args on stack -> Script_CallFunc |
| 1E RET_FAR (marker 28: Script_UnloadBanksFrom(curBank)), 1F RET | 2 | pc restored from stack |
| 24 SWAP | 2 | |
| 25 SWITCH | 8 | u16 flags; u32 tablePc -> {u32 default; u32 n; {u32 key; u32 target}[n]} |
| 26 DEBUG_PRINT, 2F LINE (sets bank line var), 51 FORMAT_PRINT | 6 | u32 |
| 27 LOAD | 4 | u16 flags |
| 20 YIELD_FRAME, 21 END_THREAD (Script_FreeThread), 22 SPAWN_THREAD, 23 KILL_THREAD, 28 RAND (Rng_Next), 29 RAND2 (Rng2_Lcg956CEA5_Next), 2A SET_RNG, 2B GET_RNG, 2C WAIT_MS, 2E, 4B RET_VALUE, 54 END (halt, ends thread slice), 2D/55 spin-yield (no pc advance) | 2 | |
| every other opcode (0x30..0xA0 natives) | 2 | arguments from the value stack |
Unhandled opcode: "ScriptUnknown Code!!" and the thread slice ends. 0x41/0x42 do not advance pc (would spin).
Live opcode set: 01-1C 1E-2F(30..4B 4C..51) 54 55 57 58 59 5B 5C 61 62 63 67-6C 6E 6F 70 73-77 7E-8F 92-99 9D 9F A0 (list in the probe).

### 1.3 Unproven / failing
Linear sweep with the table above consumes only ~60% of each file (e.g. CHARA01 stops at 46994/76796, op 0xB75A): code blocks
contain raw data NOT wrapped in DATA ops (as in RBO: strings/tables addressed by PUSH_CODE_ADDR). A recursive-descent walk from
the function entries (like `src/han2/fob_file.cpp` Successors) is required; with data regions kept verbatim the file will round-trip.
Op histogram so far (linear sweep, therefore partial): LINE 2F is by far the most common, PUSH_IMM 03, PUSH_CODE_ADDR 04.
Native opcodes 0x30..0xA0: handler->engine function map not yet extracted (handler addresses: dispatch in `Script_RunThread`; each
native pops (value,tag) pairs and calls one engine function, e.g. 28 -> Rng_Next 0x430670, 2C -> Timer_FramesToMs, 5C -> sub_426760,
39/3A/3B/3F/40/43/44/4D/4E file/bank ops, 6x-7x -> sub_401xxx image/sprite tables, 7E-A0 -> sound 0x42F8xx..0x430220).

## 2. Loader / live reload seam -- NOT STARTED
Known addresses: Script_LoadBank 0x412D40 (reads via PackArchive 0x4128A0/0x4128F0 file cursor), Script_UnloadBanksFrom 0x4130A0,
Script_FindBankByName 0x412CB0, Script_CallFunc 0x4145F0, Script_RebindThreadImage 0x412B90, thread pool Script_AllocThread 0x412A00,
PackArchive_* 0x4121A0..0x4127F0, Tex_LoadImageIntoSlot 0x409430, DirectX_CreateTexture 0x408140, Replay_LoadFile 0x41BF10,
Replay_WriteFileAtMatchEnd 0x41BE90, Event_LoadResources 0x40DA80, Player_LoadAll 0x42A460. Threads hold `codeImage` pointer + pc;
LoadBank never frees an existing bank of the same name (returns the slot), so a reload must unload (Script_UnloadBanksFrom) and
rebind running threads (Script_RebindThreadImage) -- unverified.

## 3. DEMO.DAT -- NOT STARTED (2,592,104 bytes each; Replay_LoadFile 0x41BF10 is the loader)
## 4. IMG -- NOT STARTED (Tex_LoadImageIntoSlot 0x409430, ImageData_* 0x403370.., BitStream_* 0x403F20 suggest a bit-packed/compressed body)
