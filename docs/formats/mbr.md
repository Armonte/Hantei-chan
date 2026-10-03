# Melty Blood Re-ACT (mbr.exe, 2004): character loader, data files and runtime character state

Scope: the 32-bit `mbr.exe` (MSVC6, base 0x400000, IDB `C:\dev\ida\server\mbr.exe.i64`) and the data in `C:\games\MB\R\*.p`.
Everything here was traced in the IDB and checked against the shipped files read with
`build/fbarctool.exe extract 'C:/games/MB/R/01.p' <NAME> <outdir>`. IDA types: `docs/formats/ida/mbr_types.h` (every struct and enum below,
exactly as applied in the IDB; struct sizes were machine-checked: MbrActor 0x2D0, MbrCharacterSlot 0x2994, MbrEffectObject 0x2D4,
MbrCharEntry 0xC4, MbrCommandMove 0x2C, MbrCtParams 0x5C, MbrCpfFile 193388, MbrWmtRecord 262, MbrFrame 216, MbrAt 88, MbrIf/MbrEf 52).

Evidence tags: **T** = access traced in the code (function names are the IDB names after this work, addresses are VAs), **D** = verified on the
shipped files, **U** = UNPROVEN guess, **X** = proved never read by the game.

Lineage: ReAct is the Melty Blood engine (`mb.exe`) with the Act Cadenza (MBAC) / HA4 character container already in place: 216-byte frames
(AF 44 + AS 56 + int16 idx[58]), box 8, AT 88, IF 52, EF 52. `src/framedata_ha4.cpp` parses the same bytes; the differences found are listed in section 4.4.
All offsets in this file are hexadecimal unless written decimal, little endian.

## 1. How a character gets loaded

There is no per-character file on disk: every name below is looked up by **basename, upper-cased, case-insensitive** in the 15-slot archive table
`g_PkArchiveTable` 0x9DD6B8 (`DataArchives_LoadEntry` 0x4360B0 -> `DataArchives_LoadEntryRange` 0x435FF0: slots 0..14 in order, the first slot whose
index contains the name and reports a non-zero size wins, then `PKArchive_ReadEntry` 0x42C3A0). `DataArchives_OpenAll` 0x43AD20 fills the table:
slots 0..6 = `.\00.p` .. `.\06.p` (ReAct), slots 10/11/12 = `data03.p / data02.p / data01.p` of the ORIGINAL Melty Blood (directory from
`_Path.dat`, registry or `.\`, `DataArchive_OpenOriginalMb` 0x4423A0), slots 7..9, 13, 14 stay empty; `10.p` and `data00.p` go to a second 5-slot
table `g_ExtraPkArchiveTable` 0x7C5208 that is not searched by the data loader. So a ReAct entry shadows an original-MB entry of the same name.
In the shipped install every character file is in `01.p` (23 `.DAT`, 23 `_C.CT`, 24 `.WMT`, 27 `.CPF`, `EFFECT.DAT`, `VECTOR.TXT`), CHARASELECT.CT is in `04.p`.

Battle load (`VsDemo_MatchLoaderThread` 0x413FC0, replay: `Replay_BeginPlaybackSetup` 0x457190 -> `Battle_LoadAllCharacterSlots` 0x406780):

| step | function | file / action |
|---|---|---|
| 1 | `CharDat_LoadAndSplitBlobs` 0x436940 (gfx slot 0..3) | `.\data\<stem>.dat` (stem = `MbrCharEntry.memberName0`); splits the file into blobs A / B / C (section 4.1) |
| 1b | same for slot 2/3 | partner: `.\data\<memberName1>.dat` when `memberName1` is not '0'/'1' and the entry exists (`DataArchives_EntryExists` 0x435FB0) |
| 2 | `Battle_SetupSlotCharacterFromTables` 0x406AB0 | `CharGfx_UploadTextures` 0x437650 (blob B/C to D3D textures, palette = colour index), `CharacterSlot_AttachDataBlobs` 0x437C20 (slot.actor.dataBlobA/B/C = blobs, slot.active = 1) |
| 3 | `Slot_LoadCommandCT` 0x430880 | `.\data\<stem>_c.txt` with the extension replaced by `.ct` = `<STEM>_C.CT` (section 5) |
| 4 | `Slot_InitPointCharacterForRound` 0x43F330 (partner: `Slot_InitReservePartnerForRound` 0x43F4F0) | clears the runtime parts of the slot, sets side / position / health 10000 / pointers |
| 5 | `CPU_LoadCpfForSlot` 0x406E50 -> `CPU_LoadCpfFile` 0x406DF0 | `.\data\<stem>.cpf` (stem of `g_CharTable[actor.characterId]`, +partner stem for slot+2) read whole into `g_CpfData[slot]` (section 7) |
| 6 | `Battle_LoadSlotFaceCutinAndVoice` 0x404FA0 | `grp\face\<shortName>_<col>_face.bmp`, `grp\cut\cut_<stem>NN_<col>.bmp`, `effect\<stem>NN.wav` x150 (not format-relevant) |

Other loaders: `WinQuote_LoadWmt` 0x4120E0 (`.\data\<shortName>.wmt`, win screen, section 6), `CharaSelect_LoadCharaSelectCT` 0x437E70
(`.\charaselect.ct`, section 8), `EffectDat_LoadIntoSlot4` 0x436770 (`.\data\effect.dat`, gfx slot 4 + `g_EffectDataOwnerSlot` 0x791770, the shared pattern data of
all effect objects), `HitVectorTable_LoadVectorTxt` 0x437F20 (`.\data\vector.txt`, section 9). `CharDat_LoadHeaderOverrideDof` 0x436800 (`.dof`, partner slot
only) reads an enciphered header file (first 0x444 bytes XOR `Crypto_XorWithKeyString` with the key at 0x4774A4, the rest with the key at 0x47747C); no `.dof` exists in
any shipped archive, so `DataArchives_LoadEntry` fails and the function does nothing.

## 2. `.p` archive (00.p .. 06.p, 10.p, data00..03.p) - `PKArchive_Open` 0x42C0D0

| off | type | meaning |
|---|---|---|
| 0x00 | u32 | cipher mode: 0 = payload head enciphered with the entry name, 1 = plain, 2 = XOR 0xCA (stored in `MbrPkArchive.cipherMode`) |
| 0x04 | u32 | `entryCount ^ 0xE3DF59AC` |
| 0x08 | 68 x count | `MbrPkEntry`: `name[60]` (bytes 0..58 stored as `name[j] ^ ((3*j*i + 61) & 0xFF)`, i = entry index), `u32 dataOffset` (absolute, plain), `u32 size ^ 0xE3DF59AC` |

The loader returns success when the file is at least as large as the sum of the decoded sizes (and rejects a zero size). Payload cipher
(`PKArchive_ReadEntry`): for cipher mode 0 only the first `min(size, 8563)` bytes of an entry are touched: `data[i] ^= (i + key[i % len] + 3) & 0xFF` with
`key = upper(basename)`; everything after 8563 is plain. Mode 2: first 8563 bytes `^= 0xCA`. (`tools`' `fbarctool` implements the same.) `MbrPkArchive` struct:
hFile, hMapping, mappedView, cipherMode, entryCount, `unused_14` (X: no function touches +0x14), entries.

## 3. Shared conventions

Pattern ids and frame indices are u8, positions are 1/256 pixel units (screen x = `(posX - cameraX + 0x8000) >> 8`), the screen is 640x480, facing 0 = right.
`CharacterSlot` (`g_CharacterSlots` 0x7C5AB8, 4 x 0x2994): slots 0 and 1 are the main members of side 0/1, slots 2 and 3 their partners (tag swap flips
`activeSlotIndex` by 2). The slot layout (see header): +0 active flag, +4 the Actor (0x2D0 bytes), +0x2D4 `MbrCtParams` (92 bytes from the CT file),
+0x330 `commandCount`, +0x334 200 x `MbrCommandMove`, +0x2594/+0x2694/+0x2794/+0x2894 four 256-byte input history rings. Effect objects
(`g_EffectObjectSlots` 0x7D0938, 1000 x 0x2D4) are the same Actor behind a 4-byte header.

## 4. Character `.DAT` container ('Hantei4')

### 4.1 Header, what the loader reads (`CharDat_LoadAndSplitBlobs` 0x436940, `MbrDatHeader`)

| off | type | meaning | evidence |
|---|---|---|---|
| 0x00 | char[8] | "Hantei4\0" | X: never compared |
| 0x08 | u32[2] | 0 | X |
| 0x10 | u32 | 1 | X: never read |
| 0x14 | u32 | `dataBlobSize`: blob A = file[0 .. this); if 0 the loader uses +0x1C instead | T D |
| 0x18 | u32 | `partsBlobSize`: blob B = file[dataBlobSize .. + this) copied into `GlobalAlloc(size + 0x4000)`; 0 = no blob B (AKIHA, LEN), 0x232638 GAKIHA | T D |
| 0x1C | u32 | `cgBlobOffset`: blob C start (= dataBlobSize + partsBlobSize in all files) | T D |
| 0x20 | u32 | `cgBlobSize`: blob C = file[cgBlobOffset .. + this) copied into `GlobalAlloc(size + 0x4000)` (the 'BMP Cutter3' CG, section 12) | T D |
| 0x24 | 32 B | zero | X |
| 0x44 | i32[256] | `patternOffset[p]`: offset from the START OF BLOB A (= file offset) of pattern p's header, -1 = absent. 183 of 256 used in AKIHA | T D (`Actor_ResolveFrameDataPointers`: `A + *(A + 4*pattern + 68)`) |

Then, after blob C, the file carries the 256 x 64-byte pattern name table (0x4000 bytes, `file size = cgBlobOffset + cgBlobSize + 0x4000`, verified); **the game never
reads it**. Blob A pointer = `actor.dataBlobA`, B = `actor.dataBlobB`, C = `actor.dataBlobC`; `g_CharGfxDataPtrsA/B/C[20]` hold them per gfx slot (0..3 players, 4 effect.dat).

### 4.2 Pattern header (`MbrPatternHeader`, 0x44 bytes, then `frameCount` frames of 216 bytes)

| off | meaning | evidence |
|---|---|---|
| 0x00 | frame count (max seen 69 AKIHA; the editor limit is 100) | T D (`Actor_ValidatePatternFrame`: frame index must be < it) |
| 0x04 | flags: low nibble = move type (editor 'psts'); bit 0x40 = linear texture filter (`Pattern_HasHeaderFlag40`), bit 0x80 (`Pattern_HasHeaderFlag80`) | T |
| 0x08 | move level | T (`Pattern_GetLevelField`) |
| 0x0C | box table offset (from the pattern header; 8-byte boxes) -> `actor.boxTable` | T D |
| 0x10 | AT table offset (88-byte attack records) | T (read by the hit code, not cached in the actor) |
| 0x14 | IF table offset (52-byte records) -> `actor.ifTable` | T D |
| 0x18 | EF table offset (52-byte records) -> `actor.efTable` | T D |
| 0x1C..0x43 | writer stack garbage (e.g. 0x12F7FC) | X D |

`Actor_ResolveFrameDataPointers` 0x42CFF0 computes, per pattern/frame change: `actor.patternHeader = A + patternOffset[pattern]`, `actor.frameRecord`
(+ `frameRecordAlias`) = header + 0x44 + 216*frame, `actor.asRecord` = frame + 44, `actor.frameBoxIndexArray` = frame + 148 (= idx[24]), `actor.boxTable/ifTable/efTable` from the header offsets (-1 -> NULL).

### 4.3 Frame record (`MbrFrame` = `MbrAF` 44 + `MbrAS` 56 + `short idx[58]`)

Field names and meanings are in `mbr_types.h`; the bytes the game reads and the actor field each one is copied to / drives:

| file field | consumer in the game |
|---|---|
| AF.spriteId (+0) | < 10000: blob B parts entry, >= 10000: blob C CG image id-10000, -1 nothing (`Sprite_DrawActorFrame`) |
| AF.offsetX/Y (+2/+4), zoomX/Y (+0x10/+0x12), flipMode (+8), blendMode (+9), alpha (+0xA), rotation (+0x24), interpolationType (+0x14) | draw only |
| AF.duration (+6) | `Character_UpdateMainLoop`: compared with `actor.frameTickCounter` |
| AF.aniType (+0xB), jump (+0xC), landJump (+0xD), loopEnd (+0x15), loopCount (+0x16) | frame stepping (`Character_UpdateMainLoop`, `UpdateCharacterAnimationFrame`): 0 stay, set `animationEnded`, request pattern `jump`; 1/3 `frameIndex++`; 2/4 `frameIndex = jump` (loop counter decrement if set); 5 loop count mode (counter, else `loopEnd`); landing: aniType 3/4 request pattern `landJump`, others `frameIndex = landJump` |
| AF.drawPriority (+0xE) | `SetCharacterDrawPriority` -> `actor.drawPriority` |
| AS.clearX/Y, addX/Y, speedX/Y, accelX/Y, maxSpeedX | `Actor_ApplyFrameMovementAndHitVector`: velX/velY/accelX/accelY/maxSpeedX (X mirrored by facing, Y not) |
| AS.stance (+0x18) | 0 ground, 1 air, 2 crouch; every state test; copied to `actor.prevStance` each tick |
| AS.cancelNormal/Special (+0x19/+0x1A) | cancel rules in `CommandMove_CheckRequirements` (0 never, 1 on contact, 2 always, 3 on hit) |
| AS.hitsNumber (+0x1C) | `actor.hitsRemaining`; resets the hit-group counters |
| AS.canMove (+0x1D) | 1 = free: ends hit-stun, clears chain state (`Character_UpdateMainLoop`) |
| AS.statusFlags0 (+0x24) | bit0 carry velocity into next pattern, bit1 clear carry, bit2 no walk, bit4/5 ground/air tech ok, bit31 skip this frame's movement |
| AS.statusFlags1 (+0x28) | bit1 throw escapable (`Throw_TryThrowEscape`), bits 16..19 invulnerability (inert: `Hitbox_InvincibilityCheckStub` 0x4479A0 is `mov al,1; ret`), bits 20..23 counter type -> `actor.counterHitState` |
| idx[0] | AT record index; idx[8..15] IF record indices, idx[16..23] EF record indices; idx[24..56] box slots (24 = pushbox, 25..32 hurtboxes, 35 = clash box, 49..56 attack boxes; hit detection scans 25..32 of the attacker as ATTACK boxes via `frame148[2*slot]` in the actor's own numbering of the box index array) | 

The unnamed AF/AS/AT/IF/EF bytes are zero in every frame of AKIHA / LEN / GAKIHA.DAT (D); no reader found for them.

### 4.4 Differences from `framedata_ha4.cpp` (MBAC)

* No structural difference: header words, pattern table, 0x44-byte pattern header, 216-byte frames, 88/52/52/8 tables, idx slot map are identical.
* The game ignores more than the editor models: header +0x00..+0x13 and +0x24..+0x43, pattern header +0x1C..+0x43, the name table, AT.hitStopOverride (+0x30), AT.breakTime (+0x50),
  AT.stunValue (+0x4E, only passed to a stub), AS.statusFlags1 bits 16..19.
* ReAct data only uses IF types {1,2,3,4,5,6,8,11,12,14,16,21,25,26,33,35,36} and EF types {1,2,3,4,5,6,8,9,11} (AKIHA/LEN/GAKIHA/EFFECT.DAT); the engine implements IF types
  1..0x26, 0x32, 0x64 and EF types 1,2,3,4,5,6,7,8,9,11,30,50 (sections 10, 11). Other types are no-ops.
* `Hitbox_InvincibilityCheckStub`: the invulnerability enum is inert in this build.
* EFFECT.DAT is the same container (36 patterns, header +0x14 = 0x1CCFC, CG 0xB02228) shared by all effect objects.

## 5. `<CHAR>_C.CT` command table (8896 bytes) - `Slot_LoadCommandCT` 0x430880

File = `u32 count` + 200 x 44-byte `MbrCommandMove` (8800 bytes) + 92-byte `MbrCtParams`. The loader `qmemcpy`s: `count -> slot+0x330`, records -> slot+0x334,
tail -> slot+0x2D4. All 23 shipped CTs are exactly 8896 bytes; `count` equals the number of non-0xFF entries (verified); entry `id` equals its index; indices are sparse (AKIHA uses 5..177).

`MbrCommandMove` (44 bytes), read by `Slot_FindAndExecuteCommandMove` 0x431370 (tries entries 0..199 in order), `CommandMove_CheckRequirements` 0x430F50, `CommandMove_MatchesInputHistory` 0x430BD0, `CommandMove_TryExecute` 0x4311E0:

| off | type | meaning |
|---|---|---|
| 0x00 | u8 | 0xFF = unused, else = own index |
| 0x01 | u8 | X D: never read, 0 |
| 0x02..0x11 | u8[16] | X D: zero in the files; `Slot_Init*` zeroes them every round for the first `count` entries (runtime scratch, no reader) |
| 0x12..0x21 | u8[16] | input sequence, 0xFF terminated, matched from the LAST symbol backwards: bytes 0..9 = numpad direction, 0x41..0x46 = buttons A..F, 0x2B '+' joins direction and buttons into one step, 0x5E '^' / 0x56 'V' / 0x3C '<' / 0x3E '>' = any up / down / left / right group (e.g. '^' = 7, 8 or 9), 0x54 'T' hold marker (U). Up to 64 history steps, at most 12 accumulated frames between steps (`CommandSeq_ParseFinalStep` 0x430930 / `CommandSeq_ParseEarlierStep` 0x430A80). AKIHA entry 5: `04 02 06 43 FF` = 4,2,6 then C. |
| 0x22 | u8 | pattern id requested with priority `4200 - index` |
| 0x23 | u8 | requirement mode: 0 = AS.cancelNormal rule, 1 = AS.cancelSpecial rule, 2 = special rule + throw-escape contact test |
| 0x24 | u16 | meter cost (`Actor_CanAffordMeter`: free in heat mode, else `actor.meter >= cost`); 0x2710 = 10000 = one bar; 99999 stored truncated (0x869F) = whole gauge (`Actor_SetPendingMeterCost`) |
| 0x26 | u8 | partner requirement: 1 = partner loaded and idle, 2 = partner loaded |
| 0x27 | u8 | use limit: < 100: `moveUseCounters[(v%100)/10] < v%10`; >= 100: `g_GlobalIfCounters[v/100] < v%100` |
| 0x28 | u8 | blocked while 0 < v <= `actor.commandLockLevel` |
| 0x29 | u8 | flags1: 0x01/0x02/0x04 usable in stance stand/air/crouch, 0x08 only from a free state, 0x10 clear input history after use, 0x20 never auto matched (CPU / IF commands only), 0x40 allowed in the cancel window after a hit |
| 0x2A | u8 | flags2: 0x01 match against ring B, 0x02 strict (newest step exactly 2 frames old), 0x04 allowed during the round intro, 0x08 re-request allowed, 0x40 only in the cancel context |
| 0x2B | u8 | X D: never read |

`MbrCtParams` (92 bytes at file +8804 -> slot+0x2D4); every `[reg+disp]` operand of the whole binary was scanned (ebp-based included) for displacements 0x2D4..0x333:

| off (slot+) | type | meaning |
|---|---|---|
| +0x00 (0x2D4) | u8 | `maxAirActions`: air jumps/dashes allowed (compared with `actor.airJumpState & 0x7F` in `Character_ProcessPlayerInput`): 1, AOKO/NECO/NEKO 0 |
| +0x01 | u8 | X D: never read (9 everywhere) |
| +0x02 (0x2D6) | u8 | `airTechMode` read by `Character_UpdateMainLoop`: 0 = tech window by combo table, 1 = juggle lock 0xF0 when `juggleLockState > 2`, 2 = lock after the second rise; 1 in every shipped file |
| +0x03..0x04 | u8[2] | X D: never read |
| +0x05 (0x2D9) | u8 | flags: 0x01 once-per-chain (`Character_TryNormalAttackFromInput`, `CommandMove_TryExecute`, `Actor_BeginRequestedPattern`), 0x10 air guard allowed, 0x20 jump cancel / ground jump (`Character_ProcessPlayerInput`), 0x40 KO launch override (`Actor_ResolveIncomingHits`, `CPU_UpdateAIForCharacter`), 0x80 read by `HUD_DrawHealthMeterBarsAndInfo`; shipped 0x39 (F_CIEL 0x79, SION 0xB9) |
| +0x06..0x07 | u8[2] | X D |
| +0x08 | f32 | X D: never read (1.0 mostly) |
| +0x0C (0x2E0) | f32 | `damageMultiplier` (`Damage_CalcScaledDamage`, `Damage_CalcModeScaledDamage`; 0 = ignored): 1.0, AKIHA 0.9, AKAAKIHA 1.1, GAKIHA / F_CIEL 0.6 |
| +0x10..0x1B | f32[3] | X D: never read |
| +0x1C..0x5B (0x2F0..) | 4 x 16 B | `MbrDoubleTapEntry` for directions 6, 2, 4, 8 (`Slot_PollGuardReversalCommand` 0x431490): +0 u16 `enabled` (**0 in every shipped CT, so the double-tap probe never matches**), +2 u16 patternId, +4 u16 hitStop (used by `Actor_ResolveIncomingHits` as pattern request priority 5000), +0xC u8 window frames, +0xE u8 lockout frames; other bytes U / X |

## 6. `<CHAR>.WMT` win quotes (4 + N x 262 bytes) - `WinQuote_LoadWmt` 0x4120E0

`u32 count` then `count` x `MbrWmtRecord` (262 bytes); the game reads `262*count` bytes into `g_WinQuoteTable` (room for 50). All 24 shipped WMT satisfy `size = 4 + 262*count` (344 records in total, LEN.WMT has 1).

| off | type | meaning |
|---|---|---|
| 0x00 | u8 | `opponentCharId`: quote candidate when the LOSER's character id equals it (`g_MatchLoserCharId`), 0xFF = any (`WinQuote_PickForOpponent` 0x4121A0: first all records with the id, else all 0xFF records) |
| 0x01 | u8 | X D: never read, 0 |
| 0x02 | u16 | `chanceDivisor`: 0 always accepted, n: accepted when `rand() % n == 0` (retry loop); shipped 0, 1, 3, 10 |
| 0x04 | u16 | `portraitVariant`: `grp\win\win_<shortName><NN>_<colour>.bmp` (falls back to NN = 00) |
| 0x06 | char[256] | Shift-JIS text, NUL terminated, `\r\n` line breaks, NUL padded (max 182 chars used); drawn from `262*index + 6` |

The selected index `g_WinQuoteSelected` also selects the voice `effect\win_voice\<shortName>_win<index+1:02>.wav` (falls back to `_win01`).

## 7. `<CHAR>.CPF` CPU script (193388 bytes) - `CPU_LoadCpfFile` 0x406DF0

All 27 CPF in `01.p` are exactly 193388 bytes (22 characters - NEKO has a CT but no CPF - plus 5 helper scripts `DEKU`, `JUMP`, `JUMPG`, `CROUCH`, `GUARD`). The whole file is read into `g_CpfData[slot]`
(4 slots: 0/1 main, 2/3 partner). Layout (`MbrCpfFile`, size checked = 193388):

| off | size | meaning |
|---|---|---|
| 0x0000 | i32 | `baseSkill` (0..100; 25 AKIHA, 30 most, 0 for the helper files): `CPU_SkillPercentFromBase` -> `CPU_SkillRollPasses` decides each frame whether the CPU acts |
| 0x0004 | 184 B | zero in every file, never read (X D) |
| 0x00BC (188) | 100 x 1892 | scripts 0..99 (`MbrCpfScript`): 40 steps x 44 B = 1760 B, then a 132-byte head. `CPU_ApplyCpfStepInput` addresses step k of script i at `188 + 1892*i + 44*k`; `CPU_ScoreAndSelectAction` reads the head at `188 + 1892*i + 1760` (the decompiler constant `1948` is 188 + 1760). Script 0 is the idle/neutral script (default), 1..99 are selected by score. |
| 0x2E3CC | 4000 B | never read (X); stale data in the shipped files |

`MbrCpfStep` (44 B): +0 u8 inputCode (0/0xF neutral, 1..6 = direction 6,4,2,8,9,7 held, 7..0xA = button 0x1000/0x2000/0x4000/0x8000, 0xB..0xE = down + that button, 0x10 = perform
CommandMove number `moveId`), +2 i16 duration, +4 i16 random extra duration, +6 i16 moveId, +0x18 u32 flags (bit0|1 end script, bit2 advance early on contact, bit3 only on hit,
bit4 OR button into the mask, bit31 holdLevel), +0x20 u8 dirCode (same coding as inputCode 1..8), +0x21 u8 holdLevel; the other bytes are zero in all files and unread. Step 0 with
`duration == 0 && inputCode == 0 && !(flags&1)` marks an empty script. The CPU's pseudo buttons 0x1000..0x8000 are bits 12..15 of `actor.inputButtons`.

`MbrCpfScriptHead` (132 B, 33 dwords) as used by `CPU_ScoreAndSelectAction` 0x407160: `rangeMin/rangeMax` (+0,+4, pixels to the nearest opponent; both 0 = 10000),
`useYRange/yMin/yMax` (+8..+0x10), `chancePercent` (+0x14, weight and final `rand()%100 <=` test), `selfStanceCond` (+0x20: 0 any, 1 not air, 2 air... see header), `targetStanceCond` (+0x24),
`targetActionCond` (+0x28), `needEnemyAttackNearby` (+0x2C), `requirePatternCond/requiredPattern/requiredFrame` (+0x30..0x38, 255 = any frame), `selfHealthCond` (+0x3C: 0 any, 1..4 health <= 5000/2500/1250/625, 5 = full), `targetHitCond` (+0x44),
`interruptClass` (+0x48: 1 interruptible, 2 not), `cooldownFrames` (+0x4C), `minDifficulty` (+0x50), `heatModeCond` (+0x54), `priority` (+0x64); +0x18/0x1C, +0x40, +0x58..0x60, +0x68..0x83 are never read (zero in the files).
Selection: every candidate that passes accumulates a weight; a random pick by weight among the highest `priority` wins, then `rand()%100 <= chancePercent` or the CPU repeat cooldown
(`CPU_RepeatCooldownFramesForDifficulty`: 60/40/15/8/0 frames by difficulty) is armed. Per-slot state `g_CpuSlotState[4]` (`MbrCpuSlotState`, 224 B): scriptIndex, stepIndex, framesLeft, repeatCooldown, targetDx/Dy, 100 x u16 `scriptCooldown`.
Actor fields written: `cpuStepFramesLeft` (+0x3E), `cpuScriptIndex` (+0x42), `cpuStepIndex` (+0x44), `cpuHeatRequest` (+0x46), `cpuCommandRequestLevel` (+0x47); the step's direction/buttons become `actor.inputDirection` / `inputButtons`.

## 8. `CHARASELECT.CT` and `MbrCharEntry` - `CharaSelect_LoadCharaSelectCT` 0x437E70

File (in `04.p`, 4548 bytes = 4 + 36 + 23*196): `u32 count` (23), 9-dword header copied to `g_CharaSelectCtHeader` 0x927D00 (only [0],[1] are read: default cursor characters of P1/P2 = 1 ARC, 2 CIEL), then `count` x 196 bytes
**enciphered**: `byte[k] ^= (k + key[k % 24] + 3) & 0xFF` over the whole `count*196` byte block, key = the 24-byte Shift-JIS string "ファイルが見つかりません" at 0x4774F8 (`Crypto_XorWithKeyString`).
`CharaSelect_BuildCharTableWithUnlocks` 0x437C90 then fills `g_CharEntryStorage` (100 x 196) and `g_CharTable[charId]` (ids are stable; locked/hidden entries become `valid=1, selectable=0` copies).
`MbrCharEntry` fields are in the header; shipped values (id = +0x78, stage = +0x7C, cursor): SION 0/1, ARC 1/10, CIEL 2/8, AKIHA 3/4, HISUI&KOHAKU 4/2 (member1 = KOHAKU), SHIKI 7/6, MIYAKO 8/15,
M_HISUI 14/5, KOHAKU 6/2, HISUI 5/2, NERO 10/9, WARAKIA 9/7, V_SION 11/11, WARC 12/13, AKAAKIHA 13/3, NANAYA 15/11, GAKIHA 16/14 (member1 = "1"), SATSUKI 17/13, LEN 18/10, NECO 20/10,
AOKO 22/10, WLEN 23/10, F_CIEL 24/10. `memberName1` first char: '0' = no partner, '1' = flag only (GAKIHA: `MbrTeamRecord.scriptedCharacterFlag`), else partner stem.

## 9. `VECTOR.TXT` and `EFFECT.DAT`

`VECTOR.TXT` (3816 bytes, Shift-JIS text with `//` comments) = hit vectors; `HitVectorTable_LoadVectorTxt` fills `g_HitVectorKnockbackRects[200]` (rows `id velX velY accelX accelY`, 16-byte `MbrKnockbackRect`),
`g_HitVectorTable[100]` (`MbrHitVector`, 152 B: name, stepCount, default untech time, priority, fallback id, kept-on-KO flag, 5 steps of 20 B: knockback rect index, hit pattern, guard pattern,
stun value, flag mask built from the decimal digits of the file value) and `g_HitStopFramesByLevel[7]` (last line, indexed by `AT.hitStopLevel`). `EFFECT.DAT` is a Hantei4 container (section 4) loaded into gfx slot 4;
effect objects spawned from EF types 1/11 inherit the spawner's blobs (`dataBlobA/B/C`), EF type 8 uses the shared effect blobs (`g_CommonEffectDataBlobA/B/C`).

## 10. IF records (`IF_ProcessCondition` 0x4318C0)

`Character_RunFrameIFs` 0x431830 walks `idx[8..15]`; a record runs when `g_IfTypePhaseTable[type] == phase`: phase 0 (before EFs; `Battle_RunFrameIFsPhase0All`, `EffectObjects_RunFrameIFsAll`) for every type except 14, 15, 19, 20 (box-overlap types), which run in phase 1 (hit detection).
"jump(f)" = `Actor_JumpToFrame` (set `frameIndex`, clear `frameTickCounter`, re-resolve); "request(p,prio,force)" = `Character_RequestPattern`. A target value `< 10000` is a frame, `>= 10000` a pattern (value - 10000). Direction groups for p0 (types 6/7/0xC): 10 = {4,6}, 11 = {4,6,7,8,9}, 12 = {7,8,9}, 13 = {1,2,3}, 255 = ignore; type 11 never fires in types 6/7 (the original compares the wrong variable).
Resource spec R (R >= 100: `g_GlobalIfCounters[R/100] -= R%100`; else owner counters `[R/10] -= R%10`, clamped at 0).

| type | meaning | params |
|---|---|---|
| 1 | direction held -> pattern/frame | p0 dir (10 = {3,6}), p1 target, p2 0 = act on match, 1 = act when not matching; only while `pendingPatternId == -1` |
| 2 | object lifetime / bounds | p0 bit0 x out of camera range (+61952 / -45568), bit1 y (+63488 / -70144) -> remove; p1 landed -> remove; p2 animationEnded -> remove; p3 resource spec when being removed |
| 3 | contact then goto, cost | p0 target, p1 contact code (1 `&7`, 2 `&2`, 3 `&5`, 5 `&1`, 6 `&4`, 7 `&5` against `hitResultFlags`; `p1/100` fails when bit 0x100), p2 stance 1 = victim bit 0x10, 2 = 0x20, p3 resource spec; needs `hitsRemaining == 0` |
| 4 | velocity direction goto | p0 frame, p1 0 any / 1 moving back / 2 forward (facing relative), p2 0 any / 1 down / 2 up |
| 5 | `healthLocked` set -> frame p0 | |
| 6 | button/direction input -> jump frame p2 | p0 direction group, p1 button mask (low byte), p3 bits: 1 invert, 2 use the pressed-this-frame byte (bits 12..19), 4 `(B&m)!=0` instead of `==m` (sets `commandFiredFlag`), 8/0x10 gate on hit-stun, 0x20 needs contact; blocked while knockback is active |
| 7 | same as 6 but request pattern p2 (priority 500) | |
| 8 | random | p1 chance /512 (fires when `rand()%512 < p1`), p2 0 jump(p0) else request(p0 + rand()%max(p3,1)) |
| 9 / 0xA | set / test scratch byte `loopCounter` | p0 value / jump(p0) when 0 |
| 0xB | command-move trigger | p0 CommandMove index, p1 contact code, p2 0 = request the move's pattern (priority 3000) else jump(p2), p3 bit0 victim stand, bit1 victim crouch; needs slot flags + input match (CPU: `cpuCommandRequestLevel == 1`) and meter |
| 0xC | proximity / height | p0 target, p1 distance (px) or height, p2 bit0 target on screen, bit1 request pattern, bit2 use own height, bit3 `Actor_HasAtMostOneUsedMoveFlag`, p3 direction group |
| 0xD | near screen edge | p0 target, p1 0 = frame / else pattern, p2 direction (255 any), p3 margin, p4 side mask |
| 0xE | own box 9 touches enemy box -> hit-stop both | p0 frame, p1 side filter (%1000..), p2 target box mode, p3 hit-stop; phase 1; decrements the target's `hitsRemaining` when p2 == 0 |
| 0xF | throw: box overlap then `Actor_AttachThrowVictim` | same params as 0xE |
| 0x10 | follow camera | p0 == 0: x += camera dx, p1 == 0: y += camera dy |
| 0x11 | hit-count goto | p0 target, p1 min `hitsLandedThisPattern`, p2 resource spec |
| 0x12 | owner state goto | p0 target, p1 owner pattern (256 = owner `skipAdvanceFlag`) |
| 0x13 | reflect enemy projectile | p0 speed factor (0 = invert); phase 1 |
| 0x14 | box overlap with self box p1 | p0 frame, p1 own box, p2 target box, p3 hit-stop / on-screen test; phase 1 |
| 0x15 / 0x16 / 0x17 | other character id present / stage == p1 / background mode == p1 | p0 jump target |
| 0x18 | owner counter `>=` | p1 = tens digit counter index, units = threshold |
| 0x19 | work-variable compare | p1 index, p2 value, p4 mode (0 `>`, 1 `<`, 2 `==`; tens digit selects self/owner), p0 target |
| 0x1A | directional speed add (air control) | p0 direction, p1 vx, p2 vy, p3 vx cap |
| 0x1B | owner is grabbed | p1 != 0 jump else request |
| 0x1C | round ended | jump(p0) |
| 0x1D | x position trigger | p1 target x (px, -30000 = workVars[0]), p2 direction; snaps x |
| 0x1E | facing == p1 | |
| 0x1F | command sets a work variable | p0 command, p1 index, p2 value |
| 0x20 | CPU and practice/replay test | |
| 0x21 | voice finished | p1 voice (255 = all) |
| 0x22 | aim speed at the opponent | p1 speed |
| 0x23 | multi command trigger | p0 contact bits, p1..p4 command indices tried in order |
| 0x24 | `heatMode == p1` | p2 invert |
| 0x25 | request `p0 + colour` | |
| 0x26 | contact then set/add work variable | |
| 0x32 | teammate box touch | jump |
| 0x64 | partner contact swap | both jump(p0), tag swap counter |

## 11. EF records (`Character_RunFrameEFs` 0x41A260)

Runs only while `actor.efRunPending != 0` (cleared afterwards), not paused, `objectKind != 31`, `grabbedState != 1`; walks `idx[16..23]`. EF record: s16 type, s16 number, p0..p7 s32 at +4, p8..p11 s16 at +0x24.
Types handled: 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 30, 50 (others ignored). Coordinates: frame pixels -> world units: AF.spriteId < 10000: origin (320,448) `<<7`, mirror `640-x`; >= 10000: origin (128,224) `<<8`, mirror `256-x`.

| type | handler | meaning |
|---|---|---|
| 1 | `EF01_SpawnEffectObject` | spawn effect object: number = pattern (inherits the spawner's data blobs); p0/p1 offset, p2 flags A (0x01 die with owner hit, 0x02 world fixed + no mirror, 0x04 follow owner, 0x08 freeze with owner hit-stop, 0x10 camera relative, 0x20 owner pattern, 0x40, 0x80 absolute y), p3 flags B (0x01 shadow, 0x02 slaved frame stepping, 0x04, 0x08 hit-stop to owner, 0x40, 0x80, 0x100 add enemy distance, 0x200 absolute), p4 homing mode, p7 user param |
| 8 | `EF08_SpawnEffectObject` | same, blobs from `g_CommonEffectDataBlobA/B/C` |
| 11 | `EF11_SpawnEffectObject` | burst of randomised spawns: p0/p1 base, p2/p3 jitter, p4 variants/count, p5 random count mode |
| 50 | `EF50_SpawnProjectileObject` | screen-crossing projectile: number pattern, p0 y, p1 vx, p2 vy, p3 accelY; x = spawner x +/- 81920 |
| 2 | `EF02_VariousEffects` | number: 0 id5, 1 id6, 2 super flash (60 frames, pays pending meter), 3 id18, 4 line effect (p2..p4), 50 hit spark (p4 damage effect, proration), 51 big hit burst, 55 colour flash, 200 marker, 256..260 particle emitters |
| 3 / 7 | `EF03_SpawnPresetEffect` / `EF07_SpawnPresetEffectExt` | SysEffect preset `number` at p0/p1 world |
| 4 | `EF04_PositionGrabbedTarget` | grab: number = victim pattern, p0/p1 offset, p2 %10000 frame (/1000 flip index), p3 bits 0x01 release, 0x04 y=0, 0x08, 0x10 skip, 0x20, 0x40, p4, p5 |
| 5 | `EF05_DamageGrabbedVictim` | number 0 added effect p0, 1 damage (p0 hp, p5 red, p1 combo counters, p2 hit-stop, p3 SE, p4 scaling), 2 flip victim, 3 meter, 4 heat gauge, 100/101 grab draw offsets |
| 6 | `EF06_VariousEffects2` | number: 0 trail colour, 1 screen shake + slowdown, 2/9/10 byte registers, 3 trail, 4 hp/meter/heat change, 5 facing control, 6/8/16 velocity set/aim/random, 7 ghost copy, 11 super freeze slot, 12 position set, 13 colour flash, 14 box hit with stat change, 15 BGM mute, 17/18 camera tracking, 100..105 counters / work variables, 150 call partner, 151 heat mode, 252..255 flags / `++g_RoundEndPhase` |
| 9 | `EF09_PlaySound` | number 0/1 SE (1 adds `150*slot+400` voice bank), 2/3 random pick, 4 stop bank |
| 30 | `EF30_SetFollowObjectParams` | edit the homing parameters of the owner-spawned object |

## 12. Actor layout (what the file fields become)

`MbrActor` (0x2D0 bytes, full member list with evidence in the header). Key groups: identity +0x00..+0x07 (slotIndex, characterId, controlType, objectKind, paletteIndex, patternId, frameIndex); frame stepping
+0x08..+0x16; effect object flags +0x18..+0x2A; CPU +0x3E..+0x47; resources: `meter` +0x48 (0..30000), `health` +0x50 / `redHealth` +0x54 (10000 full) with HUD copies +0x58/+0x5C, heat +0x60..+0x74; position `posX/posY` +0x88/+0x8C,
`velX/velY` +0xA0/+0xA4, accel +0xA8/+0xAA, knockback +0xB0..+0xBA, carry +0xBC..+0xCC, grab draw offsets +0xCE..+0xE0; hit result +0xE4; timers +0xE8..+0x10E; hit/stun state +0x118..+0x13E; ghost/trail +0x154..+0x161; command history +0x162..+0x17F;
hit queue +0x198..+0x257; links +0x258..+0x268; input +0x26C..+0x274; side/facing/pending requests +0x278..+0x290; frame data pointers +0x294..+0x2BC; own slot +0x2C0, teammate +0x2C4, frameAge +0x2C8.

Mapping file -> actor: Pattern/frame -> `patternId`/`frameIndex`; AF -> `frameTickCounter`, `animationEnded`, `loopCounter`, draw; AS -> velocities, `prevStance`, `hitsRemaining`, `counterHitState`; AT (via `hitQueueAt`) -> `hitStunTimer`, `untechTimeBudget`, `hitVectorId`, meter/combo; CT -> slot tail only (never copied into the actor); CPF -> `g_CpfData` + `cpu*` fields; CharEntry -> `characterId`, `paletteIndex`, team record.
Hit pipeline summary (`Battle_RunCharacterHitDetection` 0x446A70, `Actor_ResolveIncomingHits` 0x448BB0): attack boxes (idx 49..56) vs hurtboxes (25..32) with `Rect_Intersect`; queue <= 8 per victim; resolver picks guard class from held direction (`inputDirection`), `AT.guardFlags`, stance; vector word = `AT.hitVector[stance]` / `guardVector[stance]` (stand 0, air 1, crouch 2); damage = `AT.damage * Options_GetDamageMultiplier() * ct.damageMultiplier`, combo scaling `(32 - hits)/32` with low-health steps, proration `rec.prorationPercent`, air x0.92 / crouch x1.08, deep juggle x0.45, chip = scaled >> 3; meter gain `AT.meterGain * 1.7` (guard attacker x0.85, victim x0.30; hit attacker x1.0, victim x0.38); hit-stop `g_HitStopFramesByLevel[AT.hitStopLevel]` (counter x2 / x1.5).

## 13. CG container (blob C, 'BMP Cutter3') as the loader sees it (`CharGfx_UploadTextures` 0x437650)

Blob B (parts, only GAKIHA among the checked files): dword[10..1009] 1000 pattern offsets (relative to B, 0 = absent; pattern = 40 layer records of 104 B), dword[1010..2009] 1000 cutout offsets (84-byte cutouts, first dword texture index rebased by `slot*300+2`),
dword[2010..] 1000 x 32-byte names, dword[10010] texture region offset R: R+28 = 50 offsets to raw A8R8G8B8 squares, R+3428 = 50 square sizes (textures `slot*300 + 2 + i`, `Texture_UploadRawBgraSquare`).
Blob C: [0..2] "BMP Cutter3", [4] mode (1 = per-image embedded pixels, else pre-baked pages), [5 + 256*p ..] palette p (p = colour index; 256 x R,G,B,x; entry 0 forced to 0), [2053] page count, [2054] number of leading 8-bit pages,
[2065 + id] (id 0..2999) offset of the image record (or -1), [5065] offset of the 24-byte part table, [5066] offset of the baked pages. Image record: +0x00 name[32], +0x20 s32 type (0..5), +0x24 w, +0x28 h, +0x2C bpp, +0x30..+0x3C bounds, +0x40 first part index, +0x44 u16 part count,
+0x48 pixels. Part record (24 B): x, y, w, h (s32), src x / src y (s16 in the 256x256 page), u16 page (rebased by `slot*300 + #B textures + 2`), u16 copy flag (ignored by the game). Pixel formats by `type`: 0 w*h index bytes + global palette, 1 w*h BGRA dwords,
2 1024-byte palette + w*h index bytes, 3 RGB colour + w*h alpha bytes, 4 1024-byte palette + w*h index + w*h alpha, 5 256 skipped bytes + w*h index + w*h alpha with the global palette. Mode != 1: pages are read sequentially from C[5066]: first [2054] pages 256x256 8-bit (0x10000 B) with the global palette, remaining pages 256x256 BGRA (0x40000 B).
(Derived from the loader code and `cg.cpp`; not re-checked against a decoded ReAct CG.)

## 14. What the game ignores / unproven

* Ignored by the game (X): `.DAT` header +0x00..+0x13, +0x24..+0x43, pattern names, pattern header +0x1C..+0x43, AT.hitStopOverride / breakTime / stunValue effect, AS invulnerability bits; CT bytes +0x01, +0x03..0x04, +0x06..0x08, +0x10..0x1B, CommandMove +0x01/+0x02..0x11/+0x2B; WMT byte +1; CPF header bytes 4..187, the 4000-byte tail and the reserved step/head bytes; PKArchive +0x14;
  all four double-tap `enabled` words are 0 in the shipped CTs (feature inert).
* Unproven (U): the exact meaning of several actor bytes (`+0x4C`, `+0x70`, `+0x75`, `+0x7C/+0x80`, `+0x90`, `+0x94`, `+0xC4/+0xCC`, `+0x109`, `+0x120`, `+0x12A`, `+0x18E..+0x194`, `+0x28A/+0x28B/+0x28C`, `+0x27C`) and the `T` (0x54) command symbol; CharEntry +0x74, +0x88, +0x8C; team record +0x20/+0x34/+0x38;
  CPF step flag bit 3 vs bit 2 interplay; the blob B / CG sections were derived from the loader, not from decoded data.
* `unused_XX` members of `MbrActor` are proved by a whole-binary displacement scan plus the per-function review of the actor code ranges listed in the header; accesses at the same displacement in those ranges belong to other structs (AF/AS/AT records, combo records, draw records).

## 15. Named runtime globals (IDB names, all typed)

| address | name | type / meaning |
|---|---|---|
| 0x7C5AB8 | `g_CharacterSlots` | `MbrCharacterSlot[4]` |
| 0x7D0938 | `g_EffectObjectSlots` | `MbrEffectObject[1000]` |
| 0x791770 | `g_EffectDataOwnerSlot` | pseudo slot that owns the EFFECT.DAT blobs (gfx slot 4) |
| 0x7C4780 / 0x9DD084 / 0x927CAC | `g_CharGfxDataPtrsA/B/C` | blobs per gfx slot (20) |
| 0x9DE0E8 / 0x94E648 | `g_CharTable` / `g_CharEntryStorage` | `MbrCharEntry*[100]` / `MbrCharEntry[100]` |
| 0x927D00 | `g_CharaSelectCtHeader` | 9 dwords of CHARASELECT.CT |
| 0x9DD6B8 / 0x7C5208 | `g_PkArchiveTable` / `g_ExtraPkArchiveTable` | `MbrPkArchive[15]` / `[5]` |
| 0x499718 | `g_CpfData` | `MbrCpfFile[4]` |
| 0x5564C8 | `g_CpuSlotState` | `MbrCpuSlotState[4]`; candidates `g_CpuCandidateScores` 0x556848 (`MbrCpuCandidate[100]`), `g_CpuCandidateWeightSum` 0x556B70, `g_CpuSkillRollCount/Pass` 0x556B68/0x556B6C |
| 0x689940 / 0x68991C / 0x68CC6C | `g_WinQuoteTable` / `g_WinQuoteCount` / `g_WinQuoteSelected` | `MbrWmtRecord[50]` / count / chosen index |
| 0x69F8D8 | `g_TeamRecords` | `MbrTeamRecord[4]` (sides 0/1, records 2/3 touched only by slot-indexed stats) |
| 0x7C47D0 | `g_ComboCounterBlock` | `MbrComboRecord[4][8]`; current record index per side in `g_SlotComboHistoryIdx` 0x69F8D0 |
| 0x93E4A0 / 0x69EAF0 / 0x7C49D0 | `g_HitVectorTable` / `g_HitVectorKnockbackRects` / `g_HitStopFramesByLevel` | from VECTOR.TXT |
| 0x477EC0 / 0x4780A8 | `g_HitEffectPresets` / `g_DamageRankSteps` | `MbrHitEffectPreset[15]` / `MbrDamageRankStep[10]` |
| 0x476DA0 | `g_AirStunTechThresholdTable` | u8[9] = 80,50,40,36,32,28,24,20,16 (air-tech window by combo hits) |
| 0x47803C / 0x478050 / 0x478078 / 0x478090 | `g_CpuDamageDealtScaleByDifficulty` / `...TakenScale...` / `g_HandicapDamageDealtScale` / `g_HandicapDamageTakenScale` | float tables |
| 0x9D6628 | `g_AfterImageHistory` | `MbrAfterImageEntry[4][120]` |
| 0x9532E0 | `g_SysEffectPool` | `MbrSysEffect[5000]` (96 B, `SysEffect_Alloc`) |
| 0xA41500 | `g_SlotScreenFx` | `MbrSlotScreenFx[5]` (super-freeze timer per slot) |
| 0x791700 | `g_SpriteDrawRecord` | `MbrSpriteDrawRecord` (0x70) |
| 0x889968 / 0x89B2A8 / 0x8E17A8 | `g_ReplayTapeDirection` / `g_ReplayTapeButtons` / `g_ReplayTapeButtonsExtra` | per-slot input tapes, 4 x 18000 frames (`g_ReplayFrameIndex` 0x8898D4) |
| 0x7C51F8.. | `g_CameraTargetX/Y/AvgAux`, `g_CameraX/Y` 0x69F780/84, `g_CameraAux` 0x69F788 | camera |
| 0x88168C / 0xA45738 | `g_RoundEndPhase` / `g_RoundEndPhaseTimer` | round-end state machine (phases 0..5, 255 tally) |
| 0x69F9FA / 0x69F9F8 | `g_RoundIntroLockState` / `g_RoundTransitionTimer` | 2 locked, 1 intro, 0 fighting |

## 16. Naming sweep (whole binary)

Goal: no `sub_` / `dword_` / `byte_` / `word_` / `unk_` / `off_` / `flt_` / `dbl_` / `stru_` / `asc_` / `nullsub_` / `unknown_libname_` names left, and every referenced global typed.

| | before | after |
|---|---|---|
| functions with an auto name | 366 of 1123 | 0 |
| globals with an auto name that code references | 1375 | 0 |
| auto-named data items (all heads, .rdata/.data) | about 1450 | 0 |
| `loc_` labels | 9656 | 9656 (IDA-generated code labels inside functions, not names that were assigned; left as they are) |

What was done, in order of confidence:
* **Functions (366)**: every one was read in the decompiler and given a behaviour name (video layer `DDraw_*`, `D3D7_*`, `SurfaceSlot_*`, `Texture_*`, `PixelFormat_*`, `Render_Queue*`, `SysFx_*`; menus / title / story / ending / replay / practice `Menu_*`, `Title_*`, `Story_*`, `Replay_*`, `PracticeMenu_*`; audio `DirectSound_*`, `Se_*`, `Bgm_*`, `Mp3_*` (Layer I/II/III decoder, bit reader `Mp3Bits_*`), DirectInput `Joystick_*`, DirectShow `DShow_*`, system info `SysInfo_*`, `Cpu_*`, MSVC6 CRT `Crt_*`). Names for the MP3 decoder internals, the CRT internals and some menu screens are role guesses from callers, strings and call shape (UNPROVEN); the character / stage / CG / draw path is solid.
* **COM**: `struct IDirect3DDevice7 / IDirect3D7 / IDirectDraw7 / IDirectDrawSurface7 / IDirectDrawClipper` (+ `...Vtbl`) declared in `mbr_types.h` section 10 (method order from ddraw.h / d3d.h); `g_pD3D7Device`, `g_pD3D7`, `g_pDirectDraw7`, `g_SurfaceSlotDDSurface[2200]`, `g_SurfaceSlotAuxInterface[2200]` are typed with them, so the decompiler shows `g_pD3D7Device->lpVtbl->SetTexture(...)`.
* **Video structs**: `MbrDdPixelFormat`, `MbrDdSurfaceDesc2` (`g_SurfaceSlotDesc[2200]` 0x646710), `MbrTextureFormatInfo` (`g_TextureFormatTable[30]` 0x557CDC with the selected formats `g_TexFmtPalette8 / Argb4444 / Rgb565 / Rgb555 / Argb1555 / Argb8888`), `MbrD3dViewport`.
* **Constants and tables**: 72 float / double literals named by value (`kFloat_0p5`), DirectX GUIDs named by identity (`IID_IDirectDraw7`, `IID_IGraphBuilder`, `GUID_SysKeyboard`, ...), 26 compiler switch index maps named `<Function>_SwitchCaseMap_<addr>`, the PE import hint/name table named `ImportHintName_<API>` / `ImportLookupEntry_<API>`, short string literals `kStr_*`.
* **Mechanical names (1253 globals)**: every remaining referenced data item was renamed `g_<Subsystem>_<FirstReferencingFunction>_<Kind>_<ADDR>` (kind = Byte / Word / Dword / Float / Ptr / Bytes / Dwords) and typed as that scalar or byte array. These carry no meaning beyond the subsystem and one referencing function (the first function in name order, which is not always the main user); they exist so that nothing is auto-named and nothing is untyped. About 70 of them were then given real names (texture format selection config `g_Cfg_TextureFormat*`, `g_SysFxCurrentEffect`, `g_SysFxFadePercent`, `g_SysFxElapsedTicks`, `g_LastDDrawErrorText`, ...). The subsystems with most mechanical names: menus / story / title (380), video (283), MP3 decoder (189), CRT (176), input (98), sound (38), effects (78).
* Not done: array extents / struct layouts for the MP3 decoder state, the sound buffer tables, the menu / story state, the D3D device enumeration records (`dword_5570E8[97*n]` style, 388-byte records), the render command ring (`Render_ExecuteCommandList` 20 KB), the CRT heap tables. They are typed as scalars or byte arrays under mechanical names.
