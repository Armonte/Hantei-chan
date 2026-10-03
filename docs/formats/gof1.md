# Glove on Fight 1 (gof.exe, 2002): archive and character container formats

Scope: the French-Bread fighter *Glove on Fight* (GOF1), 32-bit `gof.exe` (462,848 bytes, IDB `C:\dev\GOF\extracted\Glove on Fight\gof.exe.i64`),
game files `C:\games\gof1\run\gof_00.p .. gof_03.p` and `gofop.mpg`. Everything below was traced in the IDB and then verified on the shipped data
with `tools/gof1/gof1_pack.py` and `tools/gof1/gof1_dat.py` (exit code 1 on any violated invariant). IDA types: `docs/formats/ida/gof1_types.h`
(19 structs, 18 enums, applied in the IDB; every struct offset in that file was machine-checked against IDA's own member offsets).

Evidence tags: **T** = reader traced in gof.exe, **I** = reader traced, meaning inferred from the code shape and the data, **E** = data-bearing but no reader in
gof.exe (editor-only), **V** = validated by the loader. Function names are the IDB names after this work (addresses are VAs).

Lineage: the character container is the MBAC/HA4 skeleton at an earlier stage (0x444 header, 256-entry pattern table at +0x44, 3 sections, trailing 256 x 64 B
pattern names). The frame record is 116 bytes = AF 40 + AS 40 + 36 bytes of index slots, against 216 bytes (AF 44 + AS 56 + 58 x i16) in HA4.
`docs/formats/ida/gof1_types.h` uses the same AF/AS naming as `Hantei_Docs/update_2026_09/HA4_SECTION.md`.

## 1. Tools

| tool | use |
|---|---|
| `tools/gof1/gof1_pack.py ls <archive>` | list a `.p` archive (also validates that the entries tile the file exactly) |
| `gof1_pack.py extract <archive> <name\|-all> <outdir>` | extract entries (stage-1 cipher undone). Duplicate names (gof_01.p) get a `NNN_` index prefix |
| `gof1_pack.py chars <archive> <outdir>` | extract every `*.DAT` and also undo the character cipher (stage 2) |
| `gof1_pack.py decdat <stage1.DAT> <out>` | stage-2 decrypt of one character file |
| `tools/gof1/gof1_dat.py <decrypted.DAT>...` | parse + verify every invariant in section 9 |

Extracted data (outside the repo): `C:\dev\hantei-chan\work\gof1\gof_0N\` (all entries, stage 1 done) and `...\work\gof1\chars\gof_0N\` (decrypted `.DAT`).

## 2. `.p` archive format (gof_00.p .. gof_03.p)

Loader: `LoadArchiveAndDecryptIndex` 0x423B50 (called from `MainGameLoop` 0x435CD0 for gof_03, gof_02, gof_00, gof_01 in that order); lookup
`Archive_Find_File_By_Name` 0x423C70 (case-insensitive `_strcmpi` over the decoded index, first match wins); read `Archive_XOR_Decrypt_With_Filename` 0x423D60;
dispatcher `Load_File_From_Archive` 0x42AE50 (tries the 10 slots of `g_DataArchiveSlots` 0x18001E8, first with a non-zero size wins).
This is **not** the MBAACC `FilePacHeaderA` format (no magic, no folders, a different cipher), but it is its ancestor: 64-byte index entries and a name-keyed
payload cipher.

All little endian. `KEY = 0xFA261EFB`.

| off | type | meaning | evidence |
|---|---|---|---|
| 0x00 | u32 | `plainFlag`: 0 = file data is enciphered, non-zero = plain. 0 in gof_00/01/03, 1 in gof_02 | T: stored at handle+12; `Archive_XOR_Decrypt_With_Filename` returns early when `*(a1+12) != 0` |
| 0x04 | u32 | `count ^ KEY` | T: `v7 = a2[4] ^ 0xFA261EFB`, `GlobalAlloc(count << 6)` |
| 0x08 | 64 x count | index entries (below) | T |
| 8 + 64*count | | file data; entries are contiguous in index order and their sizes sum exactly to the archive size | T: the loader returns `File_Get_Size >= sum(sizes)`; verified equal in all four archives |

Index entry (64 bytes), entry index `i`:

| off | type | meaning |
|---|---|---|
| +0x00 | char[56] | name, CP932, NUL padded. Every byte `j` is enciphered: `name[j] ^= (3*(j*i - 28)) & 0xFF` (the loader decodes in place; `j` in 0..55, `i` = entry index) |
| +0x38 | u32 | `size ^ KEY` (the loader decodes it in place; the running sum of the decoded sizes is the integrity test) |
| +0x3C | u32 | absolute file offset of the data (stored plain, `File_Seek(+60)`) |

**Payload cipher** (only when `plainFlag == 0`, in `Archive_XOR_Decrypt_With_Filename`): the first `min(size, 9696)` bytes of each entry (only when the read starts
below offset 9696) are XORed with the entry name, which the game derives as `CharUpperA(basename(requested path))`:

```
key = CharUpperA(name)                       # code page 932: ASCII a-z only, SJIS lead/trail pairs untouched
for i in range(min(size, 9696)):  data[i] ^= (i + key[i % len(key)]) & 0xFF
```

(`Extract_Filename_From_Path` strips everything up to the last backslash, `Filename_To_Uppercase` 0x4240D0 calls `CharUpperA`; the stored entry names of the real files
are upper case, the backup copies `コピー ～ X.DAT` contain SJIS trail bytes in the a-z range and only decode with the SJIS-aware upper.) Everything after byte 9696 is stored plain.

### 2.1 Measured contents

| archive | bytes | entries | plainFlag | contents |
|---|---|---|---|---|
| gof_00.p | 134,940,812 | 15 | 0 | 13 character `.DAT` (AKIKO, AYAKA, AYU, CIEL, DIGIKO, ECOCO, MORI, OBJECT and 5 `LAST*` copies), `PAC.H` (452 B plain text index), `PAC.PAC` (46 MB nested archive, 7 entries, same format; its entry 3 is a `PAC.PAC` with size 0xFFFFFFFF). Older builds of the characters: **not** loaded by the game (integrity decoys) |
| gof_01.p | 190,144,462 | 29 | 0 | 29 character `.DAT`: all 8 characters + OBJECT + `LAST*`/`コピー ～ *` backup copies. The 9 plain-named files equal gof_03's byte for byte |
| gof_02.p | 125,110,868 | 23 | 1 (plain) | 17 plain MP3 files (`01.MP3` ..), 2 nested archives `0083` and `933` (30,815,438 bytes each, 5 entries each, format as above, each entry a 6,163,022-byte AYU.DAT copy), 4 loose AYU.DAT copies `0200 0500 0600 9` (6,163,022 B). Opened and indexed by `MainGameLoop`; no entry is read through the archive API in the traced paths |
| gof_03.p | 97,603,060 | **426** | 0 | the game data: 9 `.DAT` (8 characters + OBJECT), 232 `.EX3` (compressed images, `Decompress_EX3_File` 0x423940), 89 `.WAV`, 36 `.B`, 30 `.TXT`, 9 `.CT`, 8 `.WMT`, 8 `.BMP`, 4 `.CPF` (`DEKU/JUMP/CROUCH/GUARD.CPF`, 56,048 B), 1 `.FNT` |

The only archive the game reads entries from is gof_03 (slot 9 of `g_DataArchiveSlots`, `unk_18002E4` = `g_DataArchiveSlots[9]`); `MainGameLoop` additionally
seeks fixed offsets in gof_00 (71,582,788 and 125,845,834) and gof_01 (184,549,376 and 0x948CD7), reads 4 plain bytes and `exit(0)`s on a
mismatch (tamper checks); gofop.mpg is probed at 0x2000000, 0x1000000 and 0x4000000 the same way.

Magic check after decryption (all entries of all archives): every `.p` index tiles its file exactly; gof_03: 89/89 `.WAV` start with `RIFF`, 232/232 `.EX3` with `LLIF`; gof_02: 17/17 MP3 start with an MPEG sync (`FF FB`); all 51 character `.DAT` decode to version 18 with the `備前長船` signature.

Retail characters are `gof_03.p`: AKIKO, AYAKA, AYU, CIEL, DIGIKO, ECOCO, MORI, SATSUKI plus OBJECT.DAT (effect/object patterns, same container).

## 3. Character `.DAT` container

Loader: `SpriteDataSlot_LoadCharacterDat` 0x42B330 (slot, name). Stage 1 (archive cipher) is undone by `DataArchives_LoadEntry`; stage 2 is a second, fixed-key cipher
applied per section with `Crypto_XorWithKeyString` 0x4238C0(buf, len, limit, key): `buf[p] ^= (p + key[p % strlen(key)]) & 0xFF`, `p` relative to the start of the section,
`min(len, limit)` bytes. The keys are NUL-terminated CP932 strings in `.data`:

| section | bytes | key (IDB name) |
|---|---|---|
| header | [0, 0x444) | `g_CharDatHeaderKey` 0x464568 `"Memoryえらーってことにシテオク"` (30 B) |
| pattern area | [0x444, patternAreaEnd) | `g_CharDatPatternKey` 0x464540 `"Meンどう--な事はYAりたくNaいんだけどね"` (38 B) |
| parts blob | [patternAreaEnd, +partsSize) | `g_CharDatBlobKey` 0x464588 `"hiまﾅnoカにょ？ ご苦労ナこToだにょ"` (34 B) |
| CG blob | [cgOffset, +cgSize) | `g_CharDatBlobKey` |
| pattern names (tail, editor only) | last 0x4000 bytes | `g_CharDatBlobKey` from the tail start (not decrypted by the game; verified here: the names decode to `立ち`, `立ち弱攻撃`, ...) |

The loader copies `[0, patternAreaEnd)` to `g_CharPatternData[slot]`, the parts blob to `g_CharPartsData[slot]` and the CG blob to `g_CharCgData[slot]` (each allocated +0x4000),
refuses the file unless `version == 18`, and `Slot_BindCharacterData` 0x42BC60 puts the pointers at actor+604/636/640 of the fighter slot (entity +600/+632/+636).

File layout:

```
0x000  Gof1FileHeader (0x44)
0x044  int32 patternOffset[256]            absolute offsets, -1 = absent; first = 0x444, ascending, packed
0x444  patterns in slot order (header 20 B, frames, box/AT/IF/EF tables), up to patternAreaEnd
patternAreaEnd  parts blob (old PAT v2), partsSize bytes
cgOffset        CG blob, cgSize bytes (0 in all shipped files)
file end-0x4000 256 x 64-byte pattern names (CP932, editor only)
```

### 3.1 File header (0x44) - `Gof1FileHeader`

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | char[8] | `signature` = `備前長船` (94 F5 91 4F 92 B7 91 44) | E: identical in all 51 files, never compared |
| 0x08 | 8 B | `unused_08` | zero in all files, not read |
| 0x10 | u32 | `version` = 18 | V: `SpriteDataSlot_LoadCharacterDat` `v10[4] != 18 -> return 0` |
| 0x14 | u32 | `patternAreaEnd` | T: sizes the pattern copy and starts the parts copy |
| 0x18 | u32 | `partsSize` | T |
| 0x1C | u32 | `cgOffset` (= patternAreaEnd + partsSize in all files) | T |
| 0x20 | u32 | `cgSize` (0 in all files) | T |
| 0x24 | 0x20 B | `unused_24` | zero in all files, not read |

File size = `cgOffset + cgSize + 0x4000` in all 51 files.

### 3.2 Pattern header (20 bytes) - `Gof1PatternHeader`

Reached through `Actor_ResolveFrameDataPointers` 0x424590 (`data + patternOffset[pattern]`). Frames follow at +0x14, then the optional tables in the order box, AT, IF, EF.
The offsets are relative to the pattern start, `-1` = table absent, and the tables tile the rest of the pattern exactly.

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u8 | `frameCount` (1..100; max 60 in the data) | T: Actor_ResolveFrameDataPointers `*v3 <= frame` error box |
| 0x01 | u8 bitmask | `moveInfo`: low nibble = move type (0, 6 seen; `Pattern_GetMoveType` 0x425D60, compared `< 6` in `ObjFighterStateAndHitReaction`), 0x40 = linear filter (`Pattern_GetLinearFilterBit` 0x425DA0 -> `sub_42FFE0` `Render_SetMinFilterFlag(2)`), 0x80 = double-resolution CG (`Pattern_IsCgDoubleRes` 0x425D80: halves the box and the draw rect in `ObjBoxToWorld` and `sub_42FFE0`) | T |
| 0x02 | u8 | `moveLevel` (0, 100, 255 seen) | T: `Pattern_GetMoveLevel` 0x425D40, compared `< 100` by `Actor_RequestLocomotionFromInput` 0x425FC0 |
| 0x03 | u8 | `frameSizeTag`: 0 in the 9 retail files, **116** in the 17 `LAST*.DAT` backups (an editor build wrote the frame size here) | E |
| 0x04 | i32 | `boxTableOffset` | T: actor+596 |
| 0x08 | i32 | `atTableOffset` | T: `*(pattern+8) + 26*frame.atIndex` in ObjCheckAttackVsFighters 0x43B070, ObjCheckAttackVsObjects 0x43B3F0, Actor_FindOverlappingTarget 0x43A740 |
| 0x0C | i32 | `ifTableOffset` | T: actor+620 |
| 0x10 | i32 | `efTableOffset` | T: actor+624 |

### 3.3 Frame record (116 bytes) - `Gof1FrameRecord`

`Actor_ResolveFrameDataPointers` sets actor+608 and +612 = frame, actor+616 = frame+40 (state half), actor+628 = frame+96 (box index array). Layout = AF 0x28 | AS 0x28 | 0x24 index bytes.

**AF (animation/draw), frame +0x00 - `Gof1AnimFrame`**

| off | type | name | meaning / proof |
|---|---|---|---|
| 0x00 | i16 | `spriteId` | parts-pattern index when < 10000, CG image `id-10000` otherwise. `DrawFighterSprite` 0x432150 passes it to `sub_42FFE0` which reads `parts + 24 + 4*id`; `ObjBoxToWorld` 0x42E4D0 tests `< 10000`. All 3,184 frames have id < 376 (no CG sprite is used) |
| 0x02 | i16 | `offsetX` | DrawFighterSprite `v3[1]` |
| 0x04 | i16 | `offsetY` | `v3[2]` |
| 0x06 | u16 | `duration` | `ObjRunActionScript` 0x424A20 `v40 < v43` ticks before advancing; interpolation divisor in `sub_42FFE0` (`*(a22+6)`) |
| 0x08 | u8 enum | `drawMode` | indexes `g_DrawModeTransformTable` 0x4640F8 (`2*mode + facing`); mode 8 shifts the origin by 100 (ObjBoxToWorld, InitChildObject, sub_41D8F0). 0 (370 frames) or 1 (2814) in the data |
| 0x09 | u8 | `blendMode` | non-zero = drawn alpha-blended with `alpha`; `sub_42FFE0` never reads the blend argument itself (61 frames use 1) |
| 0x0A | u8 | `alpha` | DrawFighterSprite `v37`; only used when blendMode != 0 |
| 0x0B | u8 enum | `aniFlag` | `ObjRunActionScript`/`ObjEnterCurrentAction`: 0 end -> pattern `jumpTarget` (sets actor+578), 1 next frame, 2 jump to frame `jumpTarget` (decrements actor+20 if set), 3 = 1 + landing goes to pattern `landJumpTarget`, 4 = 2 + landing goes to pattern, 5 counted loop |
| 0x0C | u8 | `jumpTarget` | pattern (aniFlag 0) or frame |
| 0x0D | u8 | `landJumpTarget` | frame (pattern for aniFlag 3/4) entered when actor+592 (landed) is set |
| 0x0E | u8 | `drawPriority` | `ObjEnterCurrentAction` -> `Actor_ApplyDrawPriority` 0x425B80 (1 = front, 2 = back among fighters, adjusts actor+207 by 5) |
| 0x0F | u8 | `unused_0F` | zero in all frames |
| 0x10 | u16 | `zoom` | 256 = 1.0, 0 = none (`v3[8]` in DrawFighterSprite); constant 0 in the data |
| 0x12 | u8 | `loopCount` | `ObjEnterCurrentAction` `v1[18]` -> actor+20 when non-zero |
| 0x13 | u8 | `loopEndFrame` | `ObjRunActionScript` case 5 (`v42[19]`) when the counter reaches 0 |
| 0x14 | 4 B | `unused_14` | zero in all frames |
| 0x18 | u8 | `interpolateToNext` | `sub_42FFE0` `*(a22 + 24)`: tween the parts toward the next frame's pattern (2761 of 3184 frames set) |
| 0x19 | 15 B | `unused_19` | zero in all frames |

**AS (state/movement), frame +0x28 - `Gof1StateFrame`** (actor+616; offsets below are relative to the AS start)

| off | type | name | meaning / proof |
|---|---|---|---|
| 0x00 | u8 | `clearVelX` | `Actor_ApplyFrameMotionFlags` 0x425310 (`*(_BYTE*)v1`): zero velX, accelX, maxVelX |
| 0x01 | u8 | `clearVelY` | zero velY, accelY |
| 0x02 | u8 | `addVelX` | add velX/accelX facing-aware, subtract `maxVelX` from the cap |
| 0x03 | u8 | `addVelY` | add velY/accelY |
| 0x04 | 4 B | `unused_04` | zero in all frames |
| 0x08 | i16 | `velX` | sub_425310 `v1+8` |
| 0x0A | i16 | `velY` | `v1+10` |
| 0x0C | i16 | `accelX` | `v1+12` |
| 0x0E | i16 | `accelY` | `v1+14` |
| 0x10 | u8 enum | `stance` | 0 ground, 1 air, 2 crouch. Copied to actor+594 (ObjRunActionScript); `ObjIntegrateMotion` 0x4257D0 air physics; `Actor_DispatchInputByStance` 0x4263E0 builds `stance | canAct << 4`. Data: 0 (3103), 1 (81) |
| 0x11 | u8 enum | `normalCancel` | 0 never, 1 on hit/guard (`actor+205 & 0xF`), 2 always (`Actor_RequestLocomotionFromInput`) |
| 0x12 | u8 enum | `specialCancel` | **E**: no reader in gof.exe; 0/1/2 in the data (113 frames use 1, 21 use 2) |
| 0x13 | u8 | `attackHitCount` | `ObjEnterCurrentAction` -> actor+220 (attack uses left); ObjCheckAttackVs* skip the attack boxes when 0 (values 0, 1, 10, 24, 100) |
| 0x14 | u8 | `canAct` | 1 = actor may act (ObjRunActionScript, ObjFighterStateAndHitReaction) |
| 0x15 | 3 B | `unused_15` | zero in all frames |
| 0x18 | u32 bitmask | `flags1` | bit0 carry velocity as inertia (ObjIntegrateMotion `*(v1+24) & 1`), bit1 clear inertia (`& 2`), bit31 motion only on first entry (`v2 >= 0 \|\| actor+10 == 2`, sub_425310) |
| 0x1C | u32 bitmask | `flags2` | bit1 throw-techable (`sub_425E10` `*(v3+28) & 2`); bits 16..19 = invulnerability class (ObjCheckAttackVsFighters / VsObjects `*(frame+616+30) & 0xF` -> `AtClass_HitsInvulnClass` 0x43B050, `g_AtClassVsInvulnHitTable`): 0 none, 1 evade high, 2 evade low, 3 strike, 4 throw; bits 20..23 = counter command (`ObjEnterCurrentAction` `(dword >> 20) & 0xF`: 1 -> actor+346 = 1, 2 -> 2, 3 -> 0); bit2 appears once and has no reader |
| 0x20 | i16 | `maxVelX` | sub_425310 `v1+32`; 0 except one frame |
| 0x22 | 6 B | `unused_22` | zero in all frames |

**Index slots, frame +0x50**

| off | type | name | meaning / proof |
|---|---|---|---|
| 0x50 | u8 | `atIndex` | signed char index into the AT table; 0xFF = none (`26 * *(char*)(frame+80)` in ObjCheckAttackVsFighters, ObjCheckAttackVsObjects, ObjCheckReflectBoxVsAttacks, Actor_FindOverlappingTarget) |
| 0x51 | u8 | `atIndexHighByte` | **E**: 0, 0xFF or garbage (45 distinct values); the engine reads only the low byte |
| 0x52 | i16[3] | `ifIndex` | IF table indices, -1 = empty (`ObjRunFrameEventsForPhase` 0x427130: `for (i = 82; i < 88; i += 2)`) |
| 0x58 | i16[4] | `efIndex` | EF table indices, -1 = empty (`Actor_RunFrameEFs` 0x413750: `for (i = 88; i < 96; i += 2)`) |
| 0x60 | i16[10] | `boxIndex` | box table indices, -1 = empty (actor+628): [0] pushbox (`ObjResolvePushboxCollision` 0x43AD20), [1..3] hurt boxes (the loops at `+2..+6` of ObjCheckAttackVsFighters/Objects), [4] grab/probe box (own kind 6 of `Actor_FindOverlappingTarget` 0x43A740, used by IF 14/15/19/20), [5] second special box (only `Actor_GetBoxSlotIndex` 0x43A6B0 reaches it; 1 frame uses it), [6] reflect/clash box (`ObjCheckReflectBoxVsAttacks` 0x43B6F0 `+12`), [7] projectile hurt box (target kind 6 of IF 19; never present in the data), [8..9] attack boxes (`+16`, `+18`); slot 9 is never present |

Unused bytes: AF 20 + AS 13 = 33 of 116 bytes are `unused_*` (zero in 3,184 of 3,184 frames, no reader). Fields: 36 named (15 AF + 16 AS + 5 index fields), 6 `unused_` ranges,
2 named E fields (`specialCancel`, `atIndexHighByte`).

### 3.4 Box table (8 bytes) - `Gof1Box`

`{i16 x1, y1, x2, y2}` read as `*(__int16*)(box + 8*idx + 0/2/4/6)` (ObjResolvePushboxCollision, ObjCheckAttackVs*). Parts frames store half-scale/double-resolution coordinates;
`ObjBoxToWorld` (0x42E4D0; shifts by the actor position, mirrors with `256 - x` when facing left and applies `0.75`/offset scaling for parts frames) produces a `Gof1RectI32` for
`RectOverlap` 0x43A540 / `RectIntersection` 0x43A590.

### 3.5 AT table (26 bytes) - `Gof1AtRecord`

`pattern + atTableOffset + 26 * frame.atIndex`. Consumers: `ObjFighterStateAndHitReaction` 0x43BF40, `ObjCheckAttackVsFighters`, `ObjCheckAttackVsObjects`, `ObjCheckReflectBoxVsAttacks`,
`sub_43D3A0` (`Actor_StartKnockback`), `Actor_CanGuardAttack` 0x43D7A0, `ComboRecord_RegisterHit` 0x43F4C0, `Damage_ComputeFromAt` 0x43F9E0.

| off | type | name | meaning / proof |
|---|---|---|---|
| 0x00 | u8 | `hitReaction` | `v87` in ObjFighterStateAndHitReaction: indexes `g_HitReactionClassTable` 0x465098, `g_HitReactionActionByStance` 0x465200, `g_HitReactionRequestActionByStance` 0x465188 (what the victim plays) |
| 0x01 | u8 | `hitstopLevel` | index into `g_HitstopFramesByLevel` 0x1622408 (7 bytes, `HitVectors_LoadFromVectorTxt` 0x42C1D0 reads them from `VECTOR.TXT`) -> `Actor_SetHitstopFrames` 0x43BEF0, screen shake |
| 0x02 | u8 | `hitSparkClass` | < 14 selects the hit spark effect (`SpawnCommonEffectObject`) and the default sound `word_4650D8[6*class]` |
| 0x03 | u8 | `attackKind` | `g_AtKindTargetsObjectsOnly` 0x465078, `g_AtKindImmunityChannel` 0x465058, the big switch in ObjFighterStateAndHitReaction (cases 0..17), `Actor_CanGuardAttack` table 0x465278; 0 or 7 in the data |
| 0x04 | u8 | `hitSoundOverride` | `Se_RequestPlay(this)` when non-zero |
| 0x05 | u8 bitmask | `flagsA` | 0x01 chip damage on guard (`Damage_ComputeChip`), 0x02 cannot KO (life clamps to 0), 0x08 use the standing reaction row (`byte_465200[3*v87]` ignoring stance), 0x10 knockback toward the attacker (`sub_43D3A0 a4 & 0x10`), 0x20 not reflectable (ObjCheckReflectBoxVsAttacks `& 0x20`), 0x40 air guard rule (`Actor_CanGuardAttack`), 0x80 use knockback row 0 (`sub_43D3A0 a4 < 0`) |
| 0x06 | i16 | `damage` | `Damage_ComputeFromAt` -> `Damage_ScaleByComboAndLife` 0x43F570 |
| 0x08 | i16 | `meterGain` | `AddSuperMeter(attacker, +8)`, victim gets `+8 >> 1` / `>> 2` |
| 0x0A | i16 | `guardBreakValue` | added to the guarding victim's guard gauge actor+108 (decays in `Actor_DecayGuardGauge` 0x4248C0); 0 in the data |
| 0x0C | 3 B | `unused_0C` | zero in all records |
| 0x0F | u8 | `statusEffect` | written to actor+248 when non-zero; `DrawFighterSprite` tints (1 red pulse, 2 blue shimmer, 3 yellow/black/grey cycle); 0 in the data |
| 0x10 | u8 bitmask | `flagsB` | 0x01 extra hit spark 257, 0x02 no horizontal knockback, 0x04 do not count the hit in the combo (`ComboRecord_RegisterHit`), 0x08 screen shake, 0x10 air-combo starter (actor+226/232 state), 0x20 no throw tech (actor+233), 0x40 hits the own team (`& 0x40` in ObjCheckAttackVs*) |
| 0x11 | u8 | `unused_11` | zero in all records |
| 0x12 | u16 | `hitClass` | 0..4: row of `g_AtClassVsInvulnHitTable` (5 x 5, `AtClass_HitsInvulnClass`); guard-height check in ObjFighterStateAndHitReaction (`v32`: 0/2 need guard type 1, 1/3 need type 2) |
| 0x14 | 6 B | `unused_14` | zero in all records |

`g_AtClassVsInvulnHitTable[hitClass][invulnClass]` = 1 when the hit connects. Reading the 5 x 5 table: invulnClass 1 (evade high) lets classes 1 and 3 through, 2 (evade low) lets 0 and 2,
3 (strike) lets 2 and 3, 4 (throw) lets 0 and 1; class 4 only connects against invulnClass 0. So classes 0/1 are strikes, 2/3 are throws, even = high, odd = low.

### 3.6 IF table (28 bytes) - `Gof1IfRecord`

`pattern + ifTableOffset + 28*idx`, run by `Actor_RunFrameIfRecord` 0x4271C0 (a `switch` on the type byte). `+0` = `type`, `+1..3` unused (zero), `+4/+8/+12/+16` = four i32
parameters, `+20..+27` unused (zero; no handler reads `+20/+24`). `g_IfTypePhaseTable` 0x464030 decides the phase: types 14, 15, 19, 20 run in phase 1 (after hit detection), all others in phase 0.
`Actor_JumpToFrame` 0x4271A0 (`frame = value`, restart tick) is the "jump" used by most types; a target of 10000 or more means "request action `value - 10000`".

| type | name | parameters (p0..p3 = +4, +8, +12, +16) |
|---|---|---|
| 1 | DIRECTION_JUMP | p0 = held direction (10 = 6/3), p1 = target, p2 = 0 while held / 1 while not held |
| 2 | END_CONDITION | p0 = outside the camera, p1 = landed, p2 = animation ended; sets the finished flag actor+577; p3 = counter spend (`tens` = counter index, `units` = amount) |
| 3 | HIT_RESULT_JUMP | p0 = frame, p1 = accepted hit-result mask (actor+205), p2 = 0 any / 1 / 2 stance bits, p3 = counter spend; only when no attack uses are left (actor+220 == 0) |
| 4 | VELOCITY_SIGN_JUMP | p0 = frame, p1 = X velocity sign test (0 any, 1, 2), p2 = Y sign test |
| 5 | INVINCIBLE_JUMP | p0 = frame, runs when actor+260 is set |
| 6 | INPUT_JUMP | p0 = direction, p1 = button mask, p2 = frame, p3 = flags (1 negate, 2 alt button bank, 4 any-of, 8/16 condition on the hit state, 4 sets actor+590) |
| 7 | INPUT_ACTION | like 6 but requests action p2 (`ObjRequestAction`) |
| 8 | RANDOM_JUMP | p0 = frame, p1 = probability out of 512 |
| 9 | SET_LOOP_COUNTER | p0 -> actor+20 |
| 10 | LOOP_ZERO_JUMP | p0 = frame when actor+20 == 0 |
| 11 | COMMAND_MOVE_ACTION | p0 = command-move index (46-byte records at actor+640+692), p2 = frame (0 = the move's own action); needs the meter cost |
| 12 | NEAREST_DISTANCE_JUMP | jump to p0 when the nearest fighter (`sub_425CF0`) is closer than p1 pixels (p2 = 1: and visible on screen) |
| 13 | SCREEN_EDGE_ACTION | p0 = frame/action, p1 = jump(0)/action, p2 = direction filter (255 any); when the actor is within 16 pixels of the screen edge |
| 14 | BOX_HIT_JUMP | phase 1: own probe box (slot 4) vs the target box kind p2 (0 attack, 1 push, 2 hurt, 3..6 slots 4..7) -> jump to p0, hitstop p3 on both |
| 15 | BOX_GRAB_JUMP | phase 1: as 14 then `Actor_TryGrabFighter` 0x43BCE0 |
| 16 | CAMERA_FOLLOW | p0 = 0: follow camera X delta; p1 = 0: follow camera Y delta |
| 17 | HIT_COUNT_JUMP | p0 = frame, p1 = minimum hit count (actor+336), p2 = counter spend |
| 18 | PARENT_ACTION_JUMP | p0 = frame, p1 = parent action (256 = parent finished) |
| 19 | REFLECT_OBJECT | phase 1: probe slot 4 vs projectile slot 7 of other fighters' objects: take them over (flip owner/direction, p0 = speed scale) |
| 20 | BOX_ENEMY_JUMP | phase 1: probe box vs target box kind p2, own kind p1, p3 = on-screen filter -> jump p0 |
| 21 | FIGHTER_PRESENT_JUMP | p1 = character id present (`sub_4270F0`) |
| 22 | STAGE_JUMP | p1 = stage index |
| 23 | GAME_MODE_JUMP | p1 = `dword_16C9990` value |
| 24 | COUNTER_AT_LEAST_JUMP | p1 = `tens` counter index, `units` minimum (parent's counters at +268+idx) |
| 25 | VARIABLE_COMPARE_JUMP | p0 = frame/action, p1 = word variable (actor+280+2*p1), p2 = value, p3 = 0 less-or-equal fails / 1 / 2 compare mode |
| 26 | ADJUST_VELOCITY_BY_INPUT | p0 = direction (10/11 groups), p1/p2 = X/Y velocity additions |
| 27 | PARENT_GRABBED_JUMP | p0 = action, p1 = 0 action / non-zero frame |
| 28 | ROUND_END_JUMP | when `g_round_end_phase != 0` |
| 29 | X_LIMIT_JUMP | p1 = X limit (-30000 = actor word), p2 = direction of the comparison |
| 30 | FACING_JUMP | p1 = facing |
| 31 | COMMAND_MOVE_JUMP | as 11 but writes variable `actor+280+2*p1 = p2` |
| 32 | CPU_PLAYER_JUMP | p1 = 1: only for the human player; 0: only for CPU (mode 0) |
| 50 | ALLY_BOX_JUMP | jumps to p0 when `Actor_AttackBoxTouchesAlly` (`sub_428580`) says an attack box overlaps an own-team fighter |

Types present in the data: 1, 2, 3, 6, 7, 8, 11, 13, 14, 20, 25, 29, 30, 31, 32 (259 records). These descriptions follow the handler bodies; the parameter naming for the
rarely used types (17..27) was not exercised against live play.

### 3.7 EF table (20 bytes) - `Gof1EfRecord`

`pattern + efTableOffset + 20*idx`, dispatched by `Actor_RunFrameEFs` 0x413750 once per frame entry. Layout: `+0` u8 `type`, `+1` u8 `subType`, `+2/+4/+6/+8/+10` five i16 `arg[0..4]`,
`+12..+19` unused (zero; no handler reads them).

| type | name | handler | notes |
|---|---|---|---|
| 1 | SPAWN_CHILD_OBJECT | `SpawnChildObjectFromFrameEffect` 0x414990 -> `InitChildObject` 0x414710 | subType = child action; arg0/arg1 = x/y; byte +6 flags (b0 follow, b1 mirror, b2.., b4 screen position, b7 absolute Y), byte +8 flags |
| 2 | SPAWN_EFFECT | `EfType2_SpawnEffectById` 0x414DC0 -> `Actor_SpawnEffectById` 0x414EC0 | subType = effect id, arg0/arg1 position, arg2..arg4 parameters |
| 3 | SPAWN_PARTICLES | `EfType3_SpawnParticlesById` 0x414D50 | subType = particle set id, arg0/1 position |
| 4 | CONTROL_GRABBED | `EfType4_ControlGrabbedOpponent` 0x41D8F0 | subType = victim pattern, arg0/1 offset, arg2 = `frame % 1000 + 1000 * variant` (variant -> victim actor+208; `arg2 / 10000 != 0` places the actor relative to the victim), byte +8 flags (1 release, 2 flagged knockback, 4 zero Y velocity) |
| 5 | THROW_DAMAGE | `EfType5_ThrowDamageToOpponent` 0x41DBD0 | subType 0 set victim effect, 1 apply damage arg0 + hitstop arg3, 2 turn victim, 3 add meter |
| 6 | ACTOR_OP | `EfType6_ActorOp` 0x41DF00 | subType = `Gof1EfActorOp` (trail effect, screen shake + freeze, life/meter/gauge add, set velocity range, aim at opponent, hitstop, counters, force round end, ...) |
| 7 | SYSTEM_PARTICLE | `EfType7_SpawnSystemParticle` 0x414E30 | subType = id |
| 8 | SPAWN_COMMON_OBJECT | `SpawnCommonEffectObject` 0x414A10 | like type 1 with a default block |
| 9 | PLAY_SOUND | inline | subType 0 = shared sound arg0, 1 = per-player bank (`arg0 + 50*(playerIndex+8)`); arg1 = skip chance out of 512 |
| 30 | SET_ACTOR_PARAMS | `EfType30_SetActorParams` 0x41E8C0 | subType 0 = set actor words +34/+36/+32 from arg0..2, 1 = `word[40 + 2*arg0] = arg1` |
| 50 | SPAWN_CHILD_FROM_PARENT | `SpawnChildEffectFromParent` 0x41E920 | subType = child pattern, arg0 = Y, arg1/2 = velocity |

Types present in the data: 1, 2, 3, 4, 5, 6, 9 and one record of type 42 (0x2A), which has no handler (`default: continue`). Type 6 sub-types in the data: 1, 3, 4, 7, 8, 10, 11, 12, 13, 100, 101, 105, 252, 253, 254.

## 4. Parts blob = old PAT v2 (`Gof1PartsHeader`)

The block between `patternAreaEnd` and `cgOffset` is the pre-`PAniDataFile` parts format with magic `2, 0x01234567` (the format of HA4 section 1.8 / BG_HA4_RE.md A.5; a
header of 0x18 bytes instead of 0x28 here). `SpriteDataSlot_UploadPartsAndCG` 0x42B6D0 uploads it:

| off | type | meaning |
|---|---|---|
| 0x00 | u32 | `version` = 2 (E: unchecked) |
| 0x04 | u32 | `magic` = 0x01234567 (E) |
| 0x08 | 16 B | `unused_08` |
| 0x18 | u32[1000] | blob-relative offset of each parts pattern, 0 = absent (T: loop `v13 = v8 + 6`, 1000 iterations). Present entries are 3680 bytes apart; the first is 0x8CBC |
| 0xFB8 | char[1000][32] | parts pattern names (`000_00`, ...), E |
| 0x8CB8 | u32 | `textureRegionOffset` = offset right after the last parts pattern (T: `v8[9006]`) |
| 0x8CBC | | parts patterns, 40 parts x 92 bytes each, then the texture region |

Part record (92 bytes, `Gof1Part`; `sub_42FFE0` loops 40 per pattern, `UploadPartsAndCG` adds the texture slot base `50*char + 2` to `+0x28`):
`+0 x, +4 y, +8 width, +12 height` (i32), `+0x10` u8 flip (1 X, 2 Y, 3 both), `+0x11` u8 linear magnification filter, `+0x12` u8 E flag (set in 29% of the parts, unread), `+0x14/+0x18` scale
X/Y in 1/1000, `+0x1C` rotation (1/10000 turn), `+0x20..0x23` modulation A, R, G, B, `+0x24..0x26` additive R, G, B, `+0x28` texture slot, `+0x2C..0x38` cut-out x, y, w, h (w = 0: not drawn),
`+0x3C` u8 draw order pass (0..255), `+0x40/+0x44` scale origin X/Y; `+0x13`, `+0x27`, `+0x3D..0x3F` and `+0x48..0x5B` are zero in all 96,480 parts and unread.

Texture region (`Gof1TextureRegion`, header 0x2E30 bytes): `+0x14` u32 = 1 (E), `+0x18` u32 texture count (3..7, E), `+0x1C` u32[50] raster offsets (0 = unused slot),
`+0xE4` char[50][64] source bitmap names (`ayu00.bmp`, E), `+0xD64` u32[50] edge length (always 512), zero padding to the first raster at 0x2E30. Rasters are raw A8R8G8B8 squares of
`edge*edge*4` bytes (1,048,576 bytes), contiguous; the region ends exactly at the end of the parts blob.

## 5. CG blob

`cgSize` is 0 in all 51 shipped `.DAT` files, so no CG bank is stored. The loader and `UploadPartsAndCG` still contain the CG path (a bank whose dword 4 is 255 stores
per-image 3-byte BGR rasters with a palette, otherwise 8-bit indexed images; 7-short image records at `+24`), and the draw path treats sprite ids >= 10000 as CG; no GOF1 character uses it.

## 6. Pattern names tail

The last 0x4000 bytes are 256 x 64-byte CP932 pattern names (`Gof1PatternName`), enciphered with `g_CharDatBlobKey` from the tail start. Only the editor reads them; the game never decrypts them.
Example (AKIKO): `立ち`, `立ち弱攻撃`, `立ち中攻撃`, `立ち強攻撃`, `しゃがみ弱攻撃`. Named present patterns per file: AKIKO 37/39, AYAKA 43/45, AYU 35/36, CIEL 35/39, DIGIKO 34/38, ECOCO 35/37, MORI 37/38, SATSUKI 34/36, OBJECT 6/15; 41..63 names are filled per file, so some name slots belong to absent patterns.

## 7. Measured data (gof_03.p, retail)

| file | bytes | patterns | frames | boxes | AT | IF | EF | parts patterns | textures | patternAreaEnd | partsSize |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AKIKO.DAT | 5,432,164 | 39 | 381 | 780 | 20 | 22 | 73 | 304 | 4 | 54,904 | 5,360,876 |
| AYAKA.DAT | 8,492,944 | 45 | 377 | 656 | 24 | 30 | 108 | 281 | 7 | 54,596 | 8,421,964 |
| AYU.DAT | 6,164,206 | 36 | 326 | 645 | 23 | 23 | 73 | 220 | 5 | 47,490 | 6,100,332 |
| CIEL.DAT | 6,642,364 | 39 | 408 | 706 | 26 | 33 | 92 | 347 | 5 | 58,288 | 6,567,692 |
| DIGIKO.DAT | 6,415,970 | 38 | 393 | 700 | 25 | 43 | 74 | 286 | 5 | 56,374 | 6,343,212 |
| ECOCO.DAT | 4,313,482 | 37 | 326 | 613 | 31 | 15 | 79 | 287 | 3 | 47,358 | 4,249,740 |
| MORI.DAT | 8,338,684 | 38 | 305 | 489 | 24 | 31 | 61 | 242 | 7 | 43,856 | 8,278,444 |
| SATSUKI.DAT | 5,728,922 | 36 | 578 | 1,167 | 25 | 47 | 143 | 377 | 4 | 83,022 | 5,629,516 |
| OBJECT.DAT | 5,570,268 | 15 | 90 | 0 | 0 | 15 | 33 | 68 | 5 | 12,912 | 5,540,972 |
| total | | 323 | 3,184 | 5,756 | 198 | 259 | 736 | 2,412 | 45 | | |

Only 15..47 of the 256 pattern slots are used per file. gof_00 (13 files) and gof_01 (29 files) verify with the same checker (older builds: AKIKO/AYAKA/CIEL/DIGIKO 35..44 patterns, `LAST*` copies with `frameSizeTag` 116).

## 8. Where the engine reads what (call chain)

`ObjRunActionScript` 0x424A20 (per tick: duration, aniFlag, hit-stop/freeze timers) -> `ObjEnterCurrentAction` 0x424360 (`Actor_ResolveFrameDataPointers`, `Actor_ApplyDrawPriority`,
loopCount, attackHitCount, counter command, `Actor_ApplyFrameMotionFlags`) -> `ObjRunFrameEventsForPhase` / `Actor_RunFrameIfRecord` (IF) and `Actor_RunFrameEFs` (EF);
`ObjIntegrateMotion` 0x4257D0 (motion); `Battle_RunObjectHitDetection` -> `ObjCheckAttackVsFighters`/`Objects` (boxes + AT) -> `ObjFighterStateAndHitReaction`; `DrawFighterSprite`
0x432150 -> `Render_QueueFighterSprite` 0x42FFE0 (parts patterns); `ObjBoxToWorld` for every box.

## 9. Invariants verified by `gof1_dat.py` (all 51 decrypted `.DAT` files: 9 + 13 + 29, 0 failures)

* `version == 18`; header bytes +0x08..0x0F and +0x24..0x43 zero; `cgOffset == patternAreaEnd + partsSize`; `file size == cgOffset + cgSize + 0x4000`; `cgSize == 0`.
* Pattern table: first present entry is 0x444, entries strictly ascending, all others -1, the last pattern ends exactly at `patternAreaEnd`; `1 <= frameCount <= 100`.
* Per pattern: the tables are in the order frames, box, AT, IF, EF with non-decreasing offsets, absent tables are -1, each table size is an exact multiple of its record size
  (8 / 26 / 28 / 20) and the last table ends at the pattern end.
* Every frame: the 33 `unused_*` bytes are zero; `drawMode <= 9`, `aniFlag <= 5`, `normalCancel`/`specialCancel <= 2`, AS move flags are 0/1; `boxIndex[9] == -1`.
* Index fields: every `atIndex`, `ifIndex`, `efIndex` and `boxIndex` value is in range of its table, **and the used indices of a pattern are exactly 0, 1, 2, ... in frame order**
  (the editor appends every used record without de-duplication), so the table length equals the number of used slots.
* AT records: `unused_0C`, `unused_11`, `unused_14` zero. IF records: bytes +1..3 and +0x14..0x1B zero. EF records: +0x0C..0x13 zero.
* Parts blob: header `(2, 0x01234567)` plus 16 zero bytes; present pattern offsets ascending from 0x8CBC and 3680 apart; texture region right after the last pattern;
  texture count equals the number of non-zero offsets/sizes, `edge^2*4` equals the distance between rasters, header/padding zero; every part's unused bytes zero.

## 10. IDB changes (gof.exe.i64)

Types: `docs/formats/ida/gof1_types.h` loaded with `idc.parse_decls` (19 structs, 18 enums; flag enums are bitmask). Renamed (behaviour-based) and typed: the archive and record functions above
(`Actor_RunFrameIfRecord`, `Actor_ApplyFrameMotionFlags`, `Pattern_GetMoveLevel/GetMoveType/IsCgDoubleRes/GetLinearFilterBit/GetFrameSpriteId`, `Actor_GetPatternFrameRecords`,
`Actor_FindOverlappingTarget`, `AtClass_HitsInvulnClass`, `Actor_CanGuardAttack`, `ComboRecord_RegisterHit`, `Damage_*`, `EfType2..30_*`, `Actor_SetHitstopFrames`, `Render_QueueFighterSprite`, ...),
prototypes applied to `SpriteDataSlot_LoadCharacterDat`, `SpriteDataSlot_UploadPartsAndCG`, `Slot_BindCharacterData`, `LoadArchiveAndDecryptIndex`, `Archive_*`, `Load_File_From_Archive`, `ObjBoxToWorld`,
`InitChildObject`, `SpawnCommonEffectObject`, `SpawnChildEffectFromParent`, `RectOverlap`; globals `g_CharPatternData`/`g_CharPartsData` (typed pointer arrays), `g_DataArchiveSlots`
(`Gof1ArchiveHandle[10]`), `g_ArchiveGof00/01/02`, the three cipher keys and the lookup tables (`g_IfTypePhaseTable`, `g_AtClassVsInvulnHitTable`, `g_AtKindTargetsObjectsOnly`, `g_AtKindImmunityChannel`,
`g_DrawModeTransformTable`, `g_HitstopFramesByLevel`, ...).

## 11. Not resolved

* `specialCancel` (frame +0x3A) and `atIndexHighByte` (frame +0x51) carry data but have no reader in gof.exe (named, tagged E).
* `flags2` bit 2, AT `flagsA` bit 0x40 / 0x80 and the AT `attackKind` values 1..17 were taken from the handler shapes (I), not exercised in play; AT `+0x0A` (`guardBreakValue`), `statusEffect`
  and `hitSparkClass` values other than those listed are always 0 in the data.
* IF parameter semantics for types 17..27 and EF parameter semantics for types 4, 5 and the type 6 sub-types are summarised from the handler bodies; unhandled EF type 42 appears once.
* The CG blob layout was not observed in data (never present); the in-loader description is from `SpriteDataSlot_UploadPartsAndCG` only.
* gof_02.p's role: indexed by `MainGameLoop`, but no entry read through the archive API was traced (BGM streaming path not followed); the nested archives `0083`/`933` hold 5 copies of AYU.DAT.
