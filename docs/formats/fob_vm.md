# RBO FOB script VM: file format, opcode table, native table, AT-access proof

Source: `C:\games\rbo\rbo.exe` (IDB `rbo.exe.i64`), `Script_RunInterpreter` 0x428AA0 and the 24 class handlers 0x426A20..0x428A70.
Tool: `rbo-support/tools/rbo/fobdis.py` (disassembler + abstract interpreter, pure Python 3, reads the PACs directly).
Corpus: every `.FOB` entry of `DATA01 DATA02 Update01 Ex1Disc Ex2Disc Ex3Disc ETC` = **527 files**, **1,643,368 instructions decoded, 0 decode errors,
0 trailing bytes** (recursive descent from every named function and every index-table entry; every file is consumed to its last byte).

IDA artefacts created in rbo.exe: enums `RboScriptOpcode` (26 class words), `RboScriptStackOp`, `RboScriptFlowOp`, `RboScriptSysOp`,
`RboScriptCmpKind`, `RboScriptAssignKind`, `RboScriptOperandFlags`; all 24 class handlers renamed `ScriptClass_*`; ~130 native wrappers and ~60 engine
functions behind them renamed (see section 5); VM globals `g_ScriptPcPtr g_ScriptSpPtr g_ScriptStackPtr g_ScriptCodeBasePtr g_ScriptBankIdPtr g_ScriptCtxPtr0..4
g_ScriptScratchBuf g_ScriptErrno g_ScriptLinePtr`; enum member `RBO_AT76_RANGED_ATTACK` (was `UNREAD_100000`).

## 0. Headline result

* The VM has **26 class words** (0x00..0x19). Counting (class,sub) forms: 2 DATA + 28 STACK + 8 FLOW + 6 SYS + 132 native subs (classes 4..0x17) + END + HALT = **178 instruction forms**
  (117 of them occur in the shipped scripts).
* Exactly **one** script opcode reads an attack record: `ACTOR_OPS.Actor_Query` (class 0x11 sub 0x09, `ScriptNative_Actor_Query` 0x44A010), query id 6 with `inout[0]==63`,
  which calls `AtFlagIsRangedAttack(AT)` (0x45B590) = `(AT+0x4C & 0x100000) != 0`. Its only caller in the whole corpus is condition script `idx2[9]` of
  `ACOLYTE_F.FOB` and `ACOLYTE_M.FOB` (all 4 archives that contain them: DATA02, Update01, Ex1Disc, Ex2Disc). No other FOB instruction dereferences an AT pointer or passes one to a native.
* So of the AT bits/fields asked about, **only `RboAtFlags76` bit 0x100000 is read** (renamed `RBO_AT76_RANGED_ATTACK`); everything else is proved unread by native code AND by every shipped script (section 7).

## 1. File layout (`Script_LoadIntoBank` 0x4248B0, confirmed on 527 files)

```
u32 nFuncs
{ char name[32]; u32 entryPc }[nFuncs]            named entry points (LJumpSub / thread spawn look names up: Script_FindFunction)
u32 nIndexTypes
{ u32 count; i32 entryPc[count] }[nIndexTypes]    index tables, -1 = undefined (kind 0 FRAME_ENTER, 1 TRANSITION, 2 VARIABLE/condition, 3 SYSTEM_HOOK)
u32 codeSize
u8  code[codeSize]                                loader appends 2 zero bytes; all pcs are BYTE offsets into this block
```
`pc` is a byte offset into `code`; instructions are 2-byte aligned. The code block also holds the script's **data**: strings, tables and the event
records handed back to the engine (see `rbo_scripts_pat.md` 2.3). Data sits behind `THREAD.ExitSelf`/`RET` or inside `DATA` instructions and is addressed with `PUSH_CODE_ADDR`.
The code block is also the script's **mutable global memory** (scripts store into it through `PUSH_CODE_ADDR` pointers).

## 2. Runtime state

| Item | Global (IDA name) | Meaning |
|---|---|---|
| pc | `g_ScriptPcPtr` -> thread field | byte offset, `*(u16*)(code+pc)` is the class word |
| code base | `g_ScriptCodeBasePtr` | bank code pointer of the current thread/bank |
| value stack | `g_ScriptStackPtr`, `g_ScriptSpPtr` | per-thread array of dwords (512 slots max; overflow -> `Script_ReportStackOverflow`), `sp` = index of top |
| args | `g_ScriptArgs` 0xAB6A64 | global dword array; the engine fills it (`Script_SetArg`) before running; script results go back through `args[0]`.. |
| ctx pointers | `g_ScriptCtxPtr0..4` | engine object pointers published with `Script_SetContextPtr0(p)` (ctx0) and `Script_SetContextPtr(i,p)` (ctx1..4 for i=0..3), pushed by `STK.PUSH_CTX0..4` |
| scratch | `g_ScriptScratchBuf` | script-visible scratch buffer (`STK.PUSH_SCRATCH_ADDR`) |
| errno | `g_ScriptErrno` | set by file natives, `SYS.GET_ERRNO/SET_ERRNO` |
| line | `g_ScriptLinePtr` | updated by `SYS.LINE` (debug line numbers; every source statement starts with one) |
| threads | `Script_AllocThread` ... 50 slots | `THREAD.*`, `WAIT.*` |

Stack discipline (verified on the corpus): the stack is **empty at every `SYS.LINE`** (300,383 line markers, all height 0) and every native pops a fixed number of slots
(`ARITY` table in `fobdis.py`, identical value in every occurrence of all 85 used natives). Native arguments are pushed as **pairs `(value, tag)`**, last argument last;
the native pops the tag first. `tag == 2` marks a **reference**: the value is an address (out-parameter, or the native dereferences it for a value parameter);
other tags mean "immediate". String parameters are passed as code addresses with tag 2 but used directly as pointers.

## 3. Instruction encoding

Every instruction starts with a u16 **class word** (`RboScriptOpcode`); except END/HALT (class word only, 2 bytes) it is followed by a u16 **sub word**. All sizes in bytes:

| Form | Length | Operands after the sub word |
|---|---|---|
| `DATA.0/1` (class 0) | 8+4n | `u32 n; u32 data[n]` inline data, skipped by execution (strings, event records, tables) |
| `STK.PUSH_IMM` / `PUSH_CODE_ADDR` | 8 | `u32 imm` / `u32 codeOffset` (pushes `code+off`, an absolute address) |
| `STK` subs 2..9, 0x1A | 4 | none |
| `STK.STORE` (0x0A), `STK.CMP` (0x19) | 8 | `u16 flags; u16 kind` |
| `STK` subs 0x0B..0x18, 0x1B | 6 | `u16 flags` |
| `FLOW.JCC` | 12 | `u16 flags; u16 kind; u32 target` |
| `FLOW.SWITCH` | 10 | `u16 flags; u32 tablePc` (table at `tablePc`: `u32 default; u32 n; {u32 key; u32 target}[n]`) |
| `FLOW.JMP` / `FLOW.CALL` | 8 | `u32 target` (CALL pushes `(retpc,3)`) |
| `FLOW` subs 4..7 | 4 | none |
| `SYS.LINE` / `SYS.DEBUG_PRINT` / `SYS.FORMAT_PRINT` | 8 | `u32 n` (line number / slots to pop) |
| `SYS` subs 1,3,4 and every native (classes 4..0x17) | 4 | none; arguments come from the value stack |

Operand `flags` (`RboScriptOperandFlags`): bit 0 = dereference the **right** (top) operand (`*(i32*)v`), bit 1 = dereference the **left** operand. `STORE` only tests bit 0
(rhs deref); its left operand is always the target address and stays on the stack (hence the `POP` after every statement). Binary ops pop the right operand, the result replaces the left.
`CMP` kinds `eq ne le ge lt gt` = 0..5, `JCC` kinds `==0 !=0 <=0 >=0 <0 >0` = 0..5, `STORE` kinds `= +=(0x64) -=(0x65) *=(0x66) /=(0x67) %=(0x68) &=(0x69) |=(0x6A) ^=(0x6B)`.
Any failing handler turns the instruction into `END` (opcode 0x18) and stops the thread; an unknown class word prints "ScriptUnknown Code!!".

### 3.1 Class table (`RboScriptOpcode`)

| Class | Name | Handler | Role |
|---|---|---|---|
| 0x00 | DATA | 0x426A20 `ScriptClass_Data` | inline data block |
| 0x01 | STACK | 0x427B60 `ScriptClass_Stack` | push/pop/store/arithmetic/compare (28 subs) |
| 0x02 | FLOW | 0x428190 `ScriptClass_Flow` | jumps, switch, call/ret, LJumpSub (8 subs) |
| 0x03 | SYS | 0x428210 `ScriptClass_Sys` | line marker, bank-address resolve, errno, debug print (6 subs) |
| 0x04 | THREAD | 0x428270 `ScriptClass_Thread` | yield / exit / spawn / kill |
| 0x05 | WAIT | 0x4282D0 `ScriptClass_Wait` | frame wait, spin, VM state save/load |
| 0x06 | RAND | 0x428350 `ScriptClass_Rand` | RNG |
| 0x07 | FILE | 0x4283A0 `ScriptClass_File` | file I/O |
| 0x08 | BANK | 0x428420 `ScriptClass_Bank` | script-side memory slots |
| 0x09 | MEM | 0x428480 `ScriptClass_MemString` | memcpy/strcpy/format/ease |
| 0x0A | NOP | 0x428510 `ScriptClass_Nop` | |
| 0x0B | GFX | 0x428530 `ScriptClass_Gfx` | image/sprite/sequence tables |
| 0x0C | CAMERA2 | 0x4285C0 `ScriptClass_Camera_2` | camera init/limits/command |
| 0x0D | EFFECT_CHARSLOT | 0x428600 `ScriptClass_EffectAndCharSlot` | effect script slots, char data slots |
| 0x0E | SOUND_CHAN | 0x428680 `ScriptClass_Sound` | DirectSound channels |
| 0x0F | CAMERA | 0x428780 `ScriptClass_Camera` | camera target/mode |
| 0x10 | SYSFLOW | 0x4287F0 `ScriptClass_GameFlowSystem` | game flow / stage commands |
| 0x11 | ACTOR_OPS | 0x428850 `ScriptClass_ActorOps` | actor pool queries/commands, **Actor_Query** |
| 0x12 | SCREEN_FADE | 0x4288E0 `ScriptClass_ScreenFade` | |
| 0x13 | ACTOR_FIND | 0x428940 `ScriptClass_ActorQuery` | nearest/farthest actor search |
| 0x14 | PICTURE | 0x428960 `ScriptClass_PictureBanner` | picture tables, banner |
| 0x15 | AUDIO_CHAN | 0x428710 `ScriptClass_AudioChannel` | volume/stream channels |
| 0x16 | GAUGE_MISC | 0x4289E0 `ScriptClass_GaugeAndMisc` | meter, effect spawn, UI bars, game state |
| 0x17 | SOUND_LOAD | 0x428A70 `ScriptClass_SoundLoad` | load sound file |
| 0x18 | END | in interpreter | end of script (return 1, `*a1=1`) |
| 0x19 | HALT | in interpreter | stop thread slice |

### 3.2 STACK subs (`RboScriptStackOp`)

| Sub | Name | Behaviour |
|---|---|---|
| 0x00 | PUSH_IMM | push imm32 |
| 0x01 | PUSH_CODE_ADDR | push `code + off` |
| 0x02 | PUSH_SCRATCH_ADDR | push `&g_ScriptScratchBuf` |
| 0x03 | PUSH_ARGS_ADDR | push `&g_ScriptArgs[0]` (argument `i` is at `+4*i`) |
| 0x04..0x08 | PUSH_CTX0..4 | push the *value* of `g_ScriptCtxPtr0..4` (engine object pointers) |
| 0x09 | POP | drop top |
| 0x0A | STORE | `*lhs (op)= rhs` (flags bit0 derefs rhs); lhs stays |
| 0x0B..0x13,0x15,0x17,0x18 | ADD SUB MUL DIV MOD SHL SHR AND OR XOR LAND LOR | binary, flags bit0/bit1 deref rhs/lhs |
| 0x14 / 0x16 | NOT / LNOT | unary (flags bit1 derefs) |
| 0x19 | CMP | `lhs <kind> rhs` -> 0/1 |
| 0x1A | SWAP | swap top two |
| 0x1B | LOAD | deref top (flags bit1) |

### 3.3 FLOW subs (`RboScriptFlowOp`) and SYS subs (`RboScriptSysOp`)

FLOW: 0 `JCC` (pops cond, jumps if the kind test holds), 1 `SWITCH`, 2 `JMP`, 3 `CALL`, 4 `LJUMPSUB` (pops 4 slots = file-name and label-name strings, loads the bank, pushes a call), 5 `RET`, 6 `RET_FAR`
(also restores a bank, marker 4), 7 `RET_VALUE` (pops a value, leaves the interpreter with it).
SYS: 0 `LINE n`, 1 `RESOLVE_BANK_ADDR` (pops 3 pairs: `*dst = bankCodeBase(bank) + offset`, an address inside script data), 2 `DEBUG_PRINT n`, 3 `GET_ERRNO(*out)`, 4 `SET_ERRNO(v)`, 5 `FORMAT_PRINT n` (printf-style `%d %s %x`).

## 4. Engine -> script interface (where pointers enter a script)

`Script_SetArg(i,v)` call sites (98 functions; full list in `docs/formats/ida/rbo_scripts_pat.md` 2.3 and the IDB). Almost all pass **integers** (actor ids `a1+48`, bytes, flags).
Pointers handed to scripts:

| Channel | Pointer | Producers |
|---|---|---|
| arg0/arg1 | actor pointer (`Actor_RunFrameScriptList6/7`: arg1 = actor; `Actor_RunPerTickScript`: arg0 = actor; `Actor_RunAiThinkScript`: arg0 = actor; condition argset 0: arg0/arg1 = the two actors) | many |
| **arg1 of condition argset 4** | **`actor+1872` = the attacker's current AT record pointer** | `Actor_RunConditionScriptList` (0x41E810) switch case 4, called from the box-overlap tests `sub_41EAF0/41EBD0/41ECA0/41ED70/41EE10` for overlap rules (event record type 14) |
| ctx0..ctx4 | `actor+0x48C` (+1164) / AI work table `unk_25F944C+256*slot` / `*(actor+2052)+8` / `*(actor+2044)+84` / `actor+448` / second actor's `+2044+84` | `Script_SetContextPtr0/Script_SetContextPtr` callers (`Actor_RunPerTickScript`, `Actor_RunAiThinkScript`, `sub_420B30`, `sub_4567D0`, `sub_458xxx..45Bxxx` skill hooks) |
| native results | pointers returned through reference arguments (e.g. `Actor_Query` id 5 -> `actor+0x48C`; `Actors_FindFirstMatching`) | natives |

Condition lists live inside event records of type 14 in script data: record dwords `[14, ownerMask, flags, boxTestKind, ... , condList@dword 8]`; the list is
`{0, kind, id, argset}` (run script `kind/id` with that argset) or `{1, 10|56, 0}` (engine-side tests `sub_459070` / `sub_45B0A0`, which read actor fields, not the AT), terminated by -1.
Parsing all 357 type-14 records referenced by the corpus gives these (kind,id,argset) triples: `(2,1,0) (2,1,3) (2,2,0) (2,2,3) (2,3,3) (2,4,2) (2,4,3) (2,5,0) (2,6,2) (2,9,4) (2,18,1) (2,21,1) (2,100,3)`.
**Only `(2,9,4)` has argset 4** (files `ACOLYTE_F/M.FOB`). A raw byte-pattern search `[0, kind<=3, id, 4, next in {-1,0,1}]` over all code blocks agrees (the other hits are inside STAGE/ARENA data tables, not condition lists).

## 5. Native table (classes 0x04..0x17): 132 subs

Columns: class.sub, class, handler/wrapper address in rbo.exe, IDA name, number of arguments (value,tag pairs; blank = never used in the corpus), behaviour (args in push order; `*x` = reference/out), occurrences in the 527 files, touches AT.
Arity of natives the shipped scripts never call is not known (`?`/blank). All natives were checked: none except `Actor_Query` reads an AT or receives an AT pointer.

| c.s | class | addr | IDA name | args | behaviour | uses | AT |
|---|---|---|---|---|---|---|---|
| 04.00 | THREAD | 0x429100 | `ScriptNative_Thread_Yield` | ? | yield one frame (ends the thread slice, pc advances) | 0 | no |
| 04.01 | THREAD | 0x4253C0 | `ScriptNative_Thread_ExitSelf` | ? | deactivate the current script thread and yield | 14900 | no |
| 04.02 | THREAD | 0x4253E0 | `ScriptNative_Thread_Spawn` | ? | (file,label) allocate thread, LJumpSub label, run once; thread id -> args[0] | 0 | no |
| 04.03 | THREAD | 0x4246F0 | `ScriptNative_Thread_KillByNameOrId` | ? | (threadId/-1, name) find thread by name and deactivate it | 0 | no |
| 05.00 | WAIT | 0x4282D0 | `ScriptNative_Wait_Frames` | ? | (frames) set thread wait counter (60 Hz) and yield | 0 | no |
| 05.01 | WAIT | 0x4282D0 | `ScriptNative_Wait_SpinNoAdvance` | ? | yield without advancing pc (re-executes next frame) | 0 | no |
| 05.02 | WAIT | 0x429100 | `ScriptNative_Thread_Yield` | ? | same as 4.0 | 0 | no |
| 05.03 | WAIT | 0x4282D0 | `RgbFx_NoOpUpdate` | ? | no-op | 0 | no |
| 05.04 | WAIT | 0x4282D0 | `RgbFx_NoOpUpdate` | ? | no-op | 0 | no |
| 05.05 | WAIT | 0x425800 | `ScriptNative_Wait_SaveScriptState` | ? | write the whole VM state (threads, banks) to a file | 0 | no |
| 05.06 | WAIT | 0x425CD0 | `ScriptNative_Wait_LoadScriptState` | ? | read the VM state back from a file | 0 | no |
| 06.00 | RAND | 0x428350 | `ScriptNative_Rand_Range` | 2 | (*out,max) *out = Rng_Range(max) | 3149 | no |
| 06.01 | RAND | 0x428350 | `ScriptNative_Rand_Range` | ? | (*out,max) *out = Rng_Range(max) (identical to 6.0) | 0 | no |
| 06.02 | RAND | 0x45F5C0 | `Rng_SetStreamState` | ? | (value) set RNG stream state | 0 | no |
| 06.03 | RAND | 0x45F5B0 | `Rng_GetStreamState` | ? | (*out) read RNG stream state | 0 | no |
| 07.00 | FILE | 0x429A20 | `ScriptNative_File_WriteBufferToPath` | ? | (path,buf,count) create file and write | 0 | no |
| 07.01 | FILE | 0x429B40 | `ScriptNative_File_ReadVirtualToBuffer` | ? | (path,buf,seek,count) read from pack/virtual file | 0 | no |
| 07.02 | FILE | 0x4283A0 | `ScriptNative_File_GetVirtualSize` | ? | (path,*outSize) | 0 | no |
| 07.03 | FILE | 0x4283A0 | `ScriptNative_File_Open` | ? | (path,mode,*outHandle) | 0 | no |
| 07.04 | FILE | 0x4283A0 | `ScriptNative_File_Read` | ? | (*handle,buf,count) | 0 | no |
| 07.05 | FILE | 0x423550 | `File_Write` | ? | (*handle,buf,count) | 0 | no |
| 07.06 | FILE | 0x42A050 | `ScriptNative_File_Seek` | ? | (*handle,whence,offset) | 0 | no |
| 07.07 | FILE | 0x4283A0 | `ScriptNative_File_Close` | ? | (*handle) | 0 | no |
| 07.08 | FILE | 0x4283A0 | `ScriptNative_File_Exists` | ? | (path) errno=0/1 | 0 | no |
| 07.09 | FILE | 0x4261E0 | `ScriptNative_File_QueryVirtual` | ? | (path,mode,*outA,*outNotFound) | 0 | no |
| 08.00 | BANK | 0x424F10 | `ScriptNative_Mem_InitSlots` | ? | (slotCount) | 0 | no |
| 08.01 | BANK | 0x424F70 | `ScriptNative_Mem_AllocSlot` | ? | (slot,size,*outPtr) | 0 | no |
| 08.02 | BANK | 0x424EC0 | `ScriptNative_Mem_FreeSlot` | ? | (slot/-1) | 0 | no |
| 08.03 | BANK | 0x424FF0 | `ScriptNative_Mem_GetSlotPtr` | ? | (slot,*out) | 0 | no |
| 08.04 | BANK | 0x425000 | `ScriptNative_Mem_GetSlotSize` | ? | (slot,*out) | 0 | no |
| 09.00 | MEM | 0x428480 | `ScriptNative_Mem_Store16` | ? | (*dst16,val) | 0 | no |
| 09.01 | MEM | 0x428480 | `ScriptNative_Mem_Copy16` | ? | (*dst16,*src16) | 0 | no |
| 09.02 | MEM | 0x428480 | `ScriptNative_Str_Copy` | ? | (dst,src) strcpy | 0 | no |
| 09.03 | MEM | 0x428480 | `ScriptNative_Str_CopyN` | ? | (dst,src,n) strncpy | 0 | no |
| 09.04 | MEM | 0x428480 | `ScriptNative_Str_Length` | ? | (*out,str) | 0 | no |
| 09.05 | MEM | 0x4296E0 | `ScriptNative_String_Compare` | ? | (*out,s1,s2,n) strncmp | 0 | no |
| 09.06 | MEM | 0x428480 | `ScriptNative_Str_Find` | ? | (*out,hay,needle) strstr offset or -1 | 0 | no |
| 09.07 | MEM | 0x428480 | `ScriptNative_Mem_CopyDwords` | ? | (dst,src,count) | 0 | no |
| 09.08 | MEM | 0x428480 | `ScriptNative_Mem_FillDwords` | ? | (dst,value,count) | 0 | no |
| 09.09 | MEM | 0x42A220 | `ScriptNative_String_FormatNumber` | ? | (flags,value,digits,dest) decimal digit string | 0 | no |
| 09.0A | MEM | 0x426430 | `ScriptNative_Math_SineEase` | 2 | (*out,{start,end,t,duration}) | 18 | no |
| 0A.00 | NOP | 0x428510 | `RgbFx_NoOpUpdate` | ? | no-op | 0 | no |
| 0B.00 | GFX | 0x401730 | `ScriptNative_Gfx_FreeAll` | 0 | (mode) free sequence/sprite/image tables | 237 | no |
| 0B.01 | GFX | 0x401090 | `ScriptNative_Image_AllocTable` | 1 | (count) | 154 | no |
| 0B.02 | GFX | 0x4010F0 | `ScriptNative_Image_SetFileName` | 2 | (imageIdx,name) | 154 | no |
| 0B.03 | GFX | 0x42A5C0 | `ScriptNative_Image_SetRegion` | 7 | (imageIdx,a,b,c,d,texW,texH) | 154 | no |
| 0B.04 | GFX | 0x4011E0 | `ScriptNative_Sprite_AllocTable` | 1 | (count) | 178 | no |
| 0B.05 | GFX | 0x401310 | `ScriptNative_Sprite_Define` | 6 | (spriteIdx,imageIdx,x,y,w,h) | 178 | no |
| 0B.06 | GFX | 0x401440 | `ScriptNative_Seq_AllocFrames` | 2 | (seqIdx,frameCount) | 290 | no |
| 0B.07 | GFX | 0x42AAE0 | `ScriptNative_Seq_SetFrame` | 7 | (seqIdx,frameIdx,v0..v4) | 290 | no |
| 0B.08 | GFX | 0x401560 | `ScriptNative_Seq_AllocTable` | 1 | (count) | 290 | no |
| 0B.09 | GFX | 0x4015C0 | `ScriptNative_Seq_SetProps` | 3 | (seqIdx,mask,intArrayRef) | 353 | no |
| 0B.0A | GFX | 0x401750 | `ScriptNative_Image_LoadAllTextures` | 0 | () | 237 | no |
| 0B.0B | GFX | 0x4012E0 | `ScriptNative_Sprite_SetColor` | 3 | (spriteIdx,mask,intArrayRef) | 56 | no |
| 0C.00 | CAMERA2 | 0x433000 | `ScriptNative_Camera_Init` | 3 | (x,y,zoom) | 396 | no |
| 0C.01 | CAMERA2 | 0x433050 | `ScriptNative_Camera_SetLimits` | 3 | (flags,limitA,limitB) | 768 | no |
| 0C.02 | CAMERA2 | 0x433070 | `ScriptNative_Camera_Command` | 2 | (mode,intArrayRef) sub-command switch | 743 | no |
| 0D.00 | EFFECT_CHARSLOT | 0x4222B0 | `ScriptNative_Effect_SetSlotScript` | 2 | (slot,scriptPtr) | 1105 | no |
| 0D.01 | EFFECT_CHARSLOT | 0x4222E0 | `ScriptNative_Effect_SetSlotActive` | 2 | (slot,value) | 1487 | no |
| 0D.02 | EFFECT_CHARSLOT | 0x422300 | `ScriptNative_Effect_GetSlotActive` | 2 | (slot/-1,*out) | 408 | no |
| 0D.03 | EFFECT_CHARSLOT | 0x422930 | `ScriptNative_Effect_AnyPoolInUse` | ? | (a1,*out) | 0 | no |
| 0D.04 | EFFECT_CHARSLOT | 0x422900 | `ScriptNative_Effect_TickSlots` | 0 | () | 622 | no |
| 0D.05 | EFFECT_CHARSLOT | 0x421E00 | `ScriptNative_CharSlot_ClearMap` | ? | () | 0 | no |
| 0D.06 | EFFECT_CHARSLOT | 0x421E20 | `ScriptNative_CharSlot_GetId` | ? | (slot,*out) | 0 | no |
| 0D.07 | EFFECT_CHARSLOT | 0x43D6C0 | `ScriptNative_Char_LoadById` | 2 | (charId,*out) | 1033 | no |
| 0D.08 | EFFECT_CHARSLOT | 0x43D720 | `ScriptNative_Char_Unload` | 2 | (charId/-1,*out) | 458 | no |
| 0E.00 | SOUND_CHAN | 0x45BF00 | `ScriptNative_Sound_LoadSlot` | 3 | (channel,slot,filename,0,0) | 6640 | no |
| 0E.01 | SOUND_CHAN | 0x45BE70 | `ScriptNative_Sound_SetSlotPlayPointer` | 3 | (channel,slot,index) | 6493 | no |
| 0E.02 | SOUND_CHAN | 0x45BEA0 | `ScriptNative_Sound_InitChannelPlayback` | 2 | (channel,startSlot) | 3604 | no |
| 0E.03 | SOUND_CHAN | 0x42B8E0 | `ScriptNative_Sound_ChainSlots` | 4 | (channel,from,to,loop) | 3371 | no |
| 0E.04 | SOUND_CHAN | 0x45BFB0 | `ScriptNative_Sound_PrepareChannel` | 1 | (channel) | 380 | no |
| 0E.05 | SOUND_CHAN | 0x45BFD0 | `ScriptNative_Sound_PlayLooping` | 1 | (channel) | 380 | no |
| 0E.06 | SOUND_CHAN | 0x45C0B0 | `ScriptNative_Sound_StopStreamThread` | 1 | (channel/<0 all) | 275 | no |
| 0E.07 | SOUND_CHAN | 0x45C070 | `ScriptNative_Sound_Pause` | ? | (channel) | 0 | no |
| 0E.08 | SOUND_CHAN | 0x45C090 | `ScriptNative_Sound_Resume` | ? | (channel) | 0 | no |
| 0E.09 | SOUND_CHAN | 0x45BE20 | `ScriptNative_Sound_ReleaseChannel` | 1 | (channel/-1 all) | 303 | no |
| 0E.0A | SOUND_CHAN | 0x45C040 | `ScriptNative_Sound_IsPlaying` | 2 | (channel,*out) | 360 | no |
| 0F.00 | CAMERA | 0x42FD70 | `ScriptNative_Camera_SetTarget` | 1 | (structRef) | 68 | no |
| 0F.01 | CAMERA | 0x42FE00 | `ScriptNative_Camera_SetEnabled` | 1 | (flag) | 935 | no |
| 0F.02 | CAMERA | 0x42FE20 | `ScriptNative_Camera_GetEnabled` | 1 | (*out) | 205 | no |
| 0F.03 | CAMERA | 0x42FE30 | `ScriptNative_Camera_SetMode` | 1 | (mode) | 617 | no |
| 0F.04 | CAMERA | 0x42C2A0 | `ScriptNative_Camera_SetOffsetEntry` | 5 | (index,submode,a3,a4,a5) | 1095 | no |
| 0F.05 | CAMERA | 0x42FF20 | `ScriptNative_Camera_Reset` | 0 | () | 156 | no |
| 0F.06 | CAMERA | 0x42FD40 | `ScriptNative_Camera_SetParams` | 3 | (mask,a2,a3) | 549 | no |
| 10.00 | SYSFLOW | 0x432710 | `ScriptNative_System_Command` | 2 | (subcmd,argptr) misc game-flow switch | 1579 | no |
| 10.01 | SYSFLOW | 0x432870 | `ScriptNative_System_SetMode` | 3 | (mode,value,addr) | 460 | no |
| 10.02 | SYSFLOW | 0x432920 | `ScriptNative_System_Command2` | 2 | (subcmd,addr) stage/result flow | 1785 | no |
| 10.03 | SYSFLOW | 0x432A30 | `ScriptNative_System_Command3` | 2 | (subcmd,addr) | 47 | no |
| 10.04 | SYSFLOW | 0x4302B0 | `ScriptNative_System_GetCurrentStageIndex` | 1 | (*out) | 124 | no |
| 10.05 | SYSFLOW | 0x432AA0 | `ScriptNative_System_Command4` | 2 | (subcmd,addr) | 12 | no |
| 11.00 | ACTOR_OPS | 0x42C8C0 | `ScriptNative_Actors_ExistsMatching` | 5 | (poolId,sel44,sel48,excludeByte92,*outBool) | 1044 | no |
| 11.01 | ACTOR_OPS | 0x42CA40 | `ScriptNative_Actors_CountMatching` | 5 | (poolId,sel44,sel48,sel92,*outCount) | 70 | no |
| 11.02 | ACTOR_OPS | 0x42CBC0 | `ScriptNative_Actors_SetCommandAndTick` | 6 | (poolId,sel44,sel48,sel92,cmdWord,cmdByte) | 2050 | no |
| 11.03 | ACTOR_OPS | 0x42CDA0 | `ScriptNative_Actors_SetFieldByIndex` | 6 | (poolId,sel44,sel48,sel92,fieldIdx,value) | 2517 | no |
| 11.04 | ACTOR_OPS | 0x42CF80 | `ScriptNative_Actors_SetAutoMoveAndTurn` | 9 | (poolId,sel44,sel48,targetX,a5..a8,flags) | 947 | no |
| 11.05 | ACTOR_OPS | 0x42D270 | `ScriptNative_Actors_ApplyOp` | 6 | (poolId,sel44,sel48,sel92,mode,ptr) | 1023 | no |
| 11.06 | ACTOR_OPS | 0x43CDC0 | `ScriptNative_Pool_ResetObjectsByFilter` | 2 | (poolId,flagsWord) | 692 | no |
| 11.07 | ACTOR_OPS | 0x44A6D0 | `ScriptNative_Script_SetTargetFilter` | 2 | (op,&args) | 520 | no |
| 11.08 | ACTOR_OPS | 0x44A750 | `ScriptCmd_ActorsKillOrSetAi` | 4 | (poolId,sel44,sel48,&args) | 287 | no |
| 11.09 | ACTOR_OPS | 0x44A010 | `ScriptNative_Actor_Query` | 3 | (actor,queryId,&inOut)  query 6/arg 63 passes an AT pointer | 380 | **YES** (reads AT+0x4C & 0x100000 via AtFlagIsRangedAttack) |
| 11.0A | ACTOR_OPS | 0x44A870 | `ScriptCmd_OverlayControl` | 3 | (actor,op,&args) | 797 | no |
| 11.0B | ACTOR_OPS | 0x42D860 | `ScriptNative_Actors_FindFirstMatching` | 5 | (poolId,sel44,sel48,sel92,*outActor) | 14 | no |
| 12.00 | SCREEN_FADE | 0x4672E0 | `ScriptNative_ScreenFade_Reset` | 0 | () | 624 | no |
| 12.01 | SCREEN_FADE | 0x467300 | `ScriptNative_ScreenFade_SetEnabled` | 1 | (enable) | 777 | no |
| 12.02 | SCREEN_FADE | 0x467320 | `ScriptNative_ScreenFade_SetColor` | 3 | (r,g,b) | 624 | no |
| 12.03 | SCREEN_FADE | 0x467340 | `ScriptNative_ScreenFade_Start` | 3 | (startAlpha,endAlpha,frames) | 624 | no |
| 12.04 | SCREEN_FADE | 0x467370 | `ScriptNative_ScreenFade_SetLayer` | 1 | (layer) | 86 | no |
| 12.05 | SCREEN_FADE | 0x467380 | `ScriptNative_ScreenFade_IsDone` | 1 | (*out) | 282 | no |
| 13.00 | ACTOR_FIND | 0x42DCD0 | `ScriptNative_Actor_FindByDistance` | 5 | (mode,ownerList,mask,refActor,*outArr) nearest/farthest actor | 15 | no |
| 14.00 | PICTURE | 0x4675B0 | `ScriptNative_Picture_AllocImageTable` | 1 | (count) | 215 | no |
| 14.01 | PICTURE | 0x467610 | `ScriptNative_Picture_SetImagePath` | 2 | (idx,path) | 215 | no |
| 14.02 | PICTURE | 0x467690 | `ScriptNative_Picture_AllocPictureTable` | 1 | (count) | 215 | no |
| 14.03 | PICTURE | 0x4676F0 | `ScriptNative_Picture_DefinePicture` | 6 | (picIdx,imageIdx,x,y,w,h) | 215 | no |
| 14.04 | PICTURE | 0x4677E0 | `ScriptNative_Picture_SetMode` | 3 | (picIdx,mode) | 215 | no |
| 14.05 | PICTURE | 0x467930 | `ScriptNative_Picture_LoadAllImages` | 0 | () | 215 | no |
| 14.06 | PICTURE | 0x467910 | `ScriptNative_Picture_Free` | ? | (flag) | 0 | no |
| 14.07 | PICTURE | 0x467A40 | `ScriptNative_Picture_StopBanner` | 0 | () | 147 | no |
| 14.08 | PICTURE | 0x467B30 | `ScriptNative_Picture_StartBanner` | 1 | (picIdx) | 44 | no |
| 14.09 | PICTURE | 0x467BA0 | `ScriptNative_Picture_IsBannerActive` | ? | (*out) | 0 | no |
| 15.00 | AUDIO_CHAN | 0x42BD00 | `ScriptNative_Audio_AllocChannelTable` | ? | (count) | 0 | no |
| 15.01 | AUDIO_CHAN | 0x42BD60 | `ScriptNative_Audio_DefineChannel` | 3 | (idx,type,targetId) | 610 | no |
| 15.02 | AUDIO_CHAN | 0x42BE50 | `ScriptNative_Audio_SetChannelLevels` | 5 | (idx,a2,a3,a4,a5) | 454 | no |
| 15.03 | AUDIO_CHAN | 0x42BFE0 | `ScriptNative_Audio_SetChannelState` | 2 | (idx,state) | 454 | no |
| 15.04 | AUDIO_CHAN | 0x42C080 | `ScriptNative_Audio_GetChannelState` | ? | (idx,*out) | 0 | no |
| 16.00 | GAUGE_MISC | 0x42E370 | `ScriptNative_Meter_TrySpend` | 2 | (amount,*okOut) | 6 | no |
| 16.01 | GAUGE_MISC | 0x42E410 | `ScriptNative_Meter_HasAtLeast` | 2 | (amount,*out) | 6 | no |
| 16.02 | GAUGE_MISC | 0x42E4B0 | `ScriptNative_Actor_SpawnEffectFacingTarget` | 5 | (a1,a2,byte92,mode,ptr) | 35 | no |
| 16.03 | GAUGE_MISC | 0x42E630 | `ScriptNative_Ui_InitBarElement` | 5 | (slot,kind,valuePtr,a4,a5) | 8 | no |
| 16.04 | GAUGE_MISC | 0x42E7C0 | `ScriptNative_Game_QueryState` | 2 | (mode,&inOut) | 92 | no |
| 16.05 | GAUGE_MISC | 0x42E850 | `ScriptNative_Media_StopIfActive` | 0 | () | 122 | no |
| 16.06 | GAUGE_MISC | 0x42E870 | `ScriptNative_Stub_Pop4` | ? | pops 4 slots, no effect | 0 | no |
| 17.00 | SOUND_LOAD | 0x42E8D0 | `ScriptNative_Sound_LoadFile` | 2 | (slot,filename) | 780 | no |

`Actor_Query` (0x44A010) query ids (`inout` struct, first argument = actor pointer): 0 `out=actor+2108`; 1 `out=actor+52`; 2 HP permille (`1000*cur/max`, min 1, 0 if dead);
3 world X/Y/Z (`Actor_AddAncestorOffsets`); 4 `out=ActorRegistry_Get(*inout)`; 5 `out[0]=actor+44, out[1]=&actor+1164`; **6: if `inout[0]==60` `out=Actor_RunTargetConditionScript(actor,inout[1])` (0x45AF60);
if `inout[0]==63` `out=AtFlagIsRangedAttack(inout[1])` (0x45B590, `inout[1]` = AT pointer)**; 7 `out=actor+2072`; 8 `out=Script_NextUniqueId()`.

Pointer-taking natives besides `Actor_Query` receive pointers into script memory (strings, int arrays, out slots) or actor pointers whose fields they read (`+44 +48 +92 +40 +0xA0 ...`);
none of them loads `actor+0x750`/`actor+0x714`+0x3C, see section 7.2.

## 6. fobdis.py

```
python3 tools/rbo/fobdis.py --all                         # decode every .FOB in every RBO archive, statistics
python3 tools/rbo/fobdis.py --dis DATA02.PAC:ACOLYTE_F.FOB 'idx2[9]'   # disassemble one function ("idx<kind>[<id>]" or a named function)
python3 tools/rbo/fobdis.py --all --scan --json out.json   # abstract interpretation, every function of every file
python3 tools/rbo/fobdis.py --at                          # the argset-4 (AT-receiving) condition scripts and what they do with the AT
```
Set `RBO_DATA` to the `DATA` directory if it is not `/mnt/c/games/rbo/DATA`.

## 7. The AT-access scan: method and proof

### 7.1 Every way a script instruction can touch memory through a non-script pointer

1. `flags` bit 0/1 (deref) on `ADD..CMP/STORE`, `STK.LOAD`, `FLOW.JCC/SWITCH` flag 2: read `*(i32*)v`.
2. `STK.STORE`/`STORE op=`: writes (and compound forms read) `*v`.
3. Natives: tag-2 arguments are dereferenced (value params) or written (out params); `MEM.*` natives copy/fill/format between arbitrary pointers (`Mem_Store16`, `Mem_Copy16`, `Str_*`, `Mem_CopyDwords`, `Mem_FillDwords`, `File_*` buffers).
4. A native that is handed an actor/AT/other engine pointer **by value or inside a struct** and dereferences it internally (e.g. `Actor_Query`, `Actors_*`, `Actor_FindByDistance`).
5. `SYS.RESOLVE_BANK_ADDR` / `STK.PUSH_CODE_ADDR`: only produce addresses inside script data (cannot reach engine objects).
There is no other opcode that loads memory (`SWITCH` reads only its table in script code), so 1..4 is exhaustive.

### 7.2 Static proof (abstract interpreter in fobdis.py)

For all 527 files and every entry point (named functions and all index-table entries, 100% of reachable code; local `CALL`s are interpreted inline, joins widen to unknown):
the value stack, the args array and the code area are modelled; values are `Const`, `address-in-region`, `engine arg i`, `ctx pointer`, `native result`, `memory loaded through P`...
Every dereference that is not into script-local memory is recorded as a *pointer chain* (`ctx0+0x1C`, `[ctx1+0xA4]+8` ...), every native call is checked for pointer-valued arguments
(directly, as `tag 2` references, and as pointer-valued fields of script-local structs passed by reference).

Results (`--scan`): 1,643,368 instructions, **140 distinct dereference chains, 0 budget failures, 0 errors; 8 reads of a constant address (STAGE01 `StageMainCallback`, a pointer read out of a script table whose entries are bank addresses) and 8 reads whose address is lost by joins (same function, same table)**;
chain roots: `ctx0` (5,524 reads, offsets 0x00..0x50), `ctx1` (505, offsets up to 0x12C), `ctx2` (84, 0x08..0x14), `ctx3` (16, +8), results of `Actor_Query` id 5 (12, +0/+4: = `actor+0x48C`), script-global pointer variables
(92: SKILLEFFECT `idx0[51]`, STAGE01/STAGE07 tables, all rooted at those same ctx/actor pointers). **No chain is rooted at an engine argument pointer** (`arg0/arg1`): scripts use the args only as scalars
(`arg1` appears only in compares and arithmetic, e.g. `arg1 == arg5`, skill amounts `arg1*130/100`; in `idx2[9]` it is copied to a variable and passed to `Actor_Query`). ctx pointers reach only `actor+0x48C..0x4DC`, AI work slots (256 B), and the stats/script-variable blocks
(`actor+0x804/0x7F8` targets); the attack state starts at `actor+0x714` = `ctx0+0x288` and the AT pointer is stored at `actor+0x750`, both beyond every offset read.

Natives that receive pointer-valued arguments (`--scan` + manual review of each hit): `Rand_Range(*out)` writes into `ctx0+0x1C`; `Actors_*`, `Actor_Query`, `Actor_FindByDistance` get actor pointers (their bodies read actor
fields only, listed in section 5); STAGE/ARENA scripts pass script-table pointers to `Actors_ApplyOp/SetField..`. The only call that receives an AT pointer is `Actor_Query` in `idx2[9]` (see 7.4).

### 7.3 Native side: the AT is unreachable from every other native

Closure of everything callable from `Script_RunInterpreter` (direct calls + address-taken callbacks, 1,079 functions, computed in IDA): it contains **none** of the AT consumers
(`Combat_ResolveDamageHit 0x444530`, `Combat_ResolveGuardContacts 0x4433E0`, `Combat_ScanDamageHits_* 0x4448D0/0x444990`, `Combat_CountContactHit 0x4449D0`, `Combat_ScanContactHits_* 0x444B10/0x444BD0`, all `Hit_* 0x442000..0x445200`,
`Actor_RunConditionScriptList 0x41E810`, `HitSkill*`). The only functions in the closure that touch `actor+0x714/0x750` are `Actor_CollectAttackBoxes2 0x440F40` (computes `actor+0x750 = patternArea.at + 120*frame.atIndex`, reads frame fields only),
`Actor_UpdateAttackState 0x441060`, `Actor_ResetStateForInit 0x441850`, `Actor_ResetCombatState 0x441A00`, `Actor_ResetTransientStateOnFirstEnter 0x43EDC0`; each was decompiled: they read frame-record bits (`frame+56 & 1/2/0x10`, `+72`, `+76`)
or clear state words, never an AT field. Every AT pointer in the engine originates from `actor+0x750` (3 readers: 0x41E810, 0x440F40, 0x4433E0) or from the script argument; hence the only way a script native can see an AT is via arg1 of argset 4.

### 7.4 The one reader: `ACOLYTE_F/M.FOB idx2[9]` (condition script of an overlap rule)

```
LINE 1286  var15524 = arg0 ; LINE 1287 var15544 = arg1        (arg1 = attacker's AT pointer, argset 4)
args[0] = 63 ; args[1] = var15544                              (builds the Actor_Query in/out struct {63, AT})
Actor_Query(actor = var15524, query 6, &args)                  -> args[0] = AtFlagIsRangedAttack(AT) = (AT.target_fx_flags & 0x100000) != 0
Thread_ExitSelf                                                (condition result = args[0])
```
The overlap rule (type 14, condition `{0,2,9,4}`) therefore only fires for attacks whose AT has `RboAtFlags76` bit 0x100000. Data cross-check: the bit is set on 439 of 8612 AT records, all in
ranged/projectile attackers (DRACULA 117, DOPPELGANGER 57, DEVIOUS 23, PHENOMENA/PHARAOH/HYDRA 18, AMON-RA 12, ORC_ARC 10, OSIRIS 9, SANTAPORING 8, SKELETON_ARC 6, POISON_TOAD 6, ISIS 6, HATY 6, CRUISER/ANTIQUE_FIRELOCK 4, STORMKNIGHT/SASQUATCH 3).
Behaviour name: **RANGED_ATTACK**; the Acolyte rule is Pneuma-style (ranged-attack negation/guard).

### 7.5 Verdicts (task list)

| Item | Verdict |
|---|---|
| `RboAtFlags76` **0x100000** | **READ by script opcode `ACTOR_OPS.Actor_Query` (class 0x11 sub 9, query 6/63 -> `AtFlagIsRangedAttack` 0x45B590) in `ACOLYTE_F.FOB` and `ACOLYTE_M.FOB` `idx2[9]` (DATA02, Update01, Ex1Disc, Ex2Disc copies).** Behaviour: RANGED_ATTACK. |
| `RboAtFlags76` 0x2 | proved: no script reads it (also no engine code) |
| `RboAtFlags76` 0x100 | proved: no script reads it |
| `RboAtFlags76` 0x2000 | proved: no script reads it |
| `RboAtFlags76` 0x80 (not in the task list, same status) | proved: no script reads it |
| `RboAtFlags80` 0x1, 0x10, 0x20, 0x200, 0x2000, 0x20000000, 0x40000000 | proved: no script reads them |
| fields `unused_14` (+0x14), `reserved_flag_24` (+0x24), `unused_60/64/68/74` | proved: no script reads them |
| every other AT offset | no script reads it; the single AT access in the script corpus is the 4-byte read of `+0x4C` masked with 0x100000 |

"Proved" means: (a) the only AT pointer a script can obtain is arg1 of condition argset 4; (b) the only scripts called with argset 4 are `idx2[9]` of ACOLYTE_F/M (parsed from all 357 type-14 records, cross-checked by byte search);
(c) that script never dereferences the pointer itself (`fobdis.py --at`: no deref chain rooted at `AT`) and hands it to exactly one native, `Actor_Query` query 63;
(d) `Actor_Query` -> `AtFlagIsRangedAttack` reads only `*(AT+76) & 0x100000`; (e) all other script-callable native code (closure of 1,079 functions) contains no AT reader and no load of `actor+0x750` besides the pointer set-up in `Actor_CollectAttackBoxes2`;
(f) all 140 remaining deref chains in all 527 files are rooted at ctx pointers / native results / script variables that never point at an AT (offsets are far below `actor+0x714`).
Scope: the seven game archives under `C:\games\rbo\DATA` (the installers/English patch/widescreen patch are packed archives of executables and were not unpacked; `rbo_update\update_data\Data\Update01.PAC` is byte-identical to `DATA\Update01.PAC`).

### 7.6 Aggregation by AT offset and bit mask

| AT offset | Script reads | Mask tested | Where |
|---|---|---|---|
| +0x4C (`target_fx_flags`) | 1 (via native) | `& 0x100000` | ACOLYTE_F/M idx2[9] -> `Actor_Query(6,63)` |
| all other offsets 0x00..0x77 | 0 | - | - |
