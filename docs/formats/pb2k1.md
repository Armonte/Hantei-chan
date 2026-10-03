# Queen of Heart 2001 ~Party's Breaker~ (pb2k1.exe, 2001): character loader, data files and runtime character state

Scope: the 32-bit `pb2k1.exe` (MSVC6, base 0x400000, IDB `C:\dev\ida\server\pb2k1.exe.i64`) and the data in `C:\games\pb\*.dat`.
Everything here was traced in the IDB and checked against the shipped files read with
`build/fbarctool.exe extract 'C:/games/pb/01.dat' <NAME> <outdir>` (the stage-1 archive layer is undone by fbarctool; the character-file ciphers of section 2 are applied by
`tools/fb/pb2k1_verify.py`, which also checks every invariant claimed below on all 16 shipped character files and the satellite files).
IDA types: `docs/formats/ida/pb2k1_types.h` (every struct and enum below, exactly as applied in the IDB; machine-checked by `tools/ida/gen_cpp_types.py`:
Pb2DatHeader 0x41C, Pb2PatternHeader 20, Pb2FrameRecord 96 = Pb2AnimFrame 24 + Pb2StateFrame 36 + 36, Pb2Box 4, Pb2AtRecord 26, Pb2IfRecord 20, Pb2EfRecord 12,
Pb2SpriteRecord 14, Pb2SpriteGroupHeader 52, Pb2SpriteBankHeader 12220, Pb2CommandMove 42, Pb2CtHeader 28, Pb2CtFile 4232, Pb2WmtRecord 154, Pb2CharTableEntry 168,
Pb2CpfFile 56048, Pb2Object 604, Pb2FighterSlot 5864, Pb2ObjectSlot 608).

Evidence tags: **T** = access traced in the code (function names are the IDB names after this work, addresses are VAs), **D** = verified on the shipped files,
**U** = UNPROVEN guess, **X** = proved never read by the game (data-bearing or constant).

Lineage: PB2K1 is the engine between QoH '99 (`docs/lineage/qoh99_to_pb2k1.md` in PovertyCaster) and Glove on Fight 1 / Melty Blood. `docs/formats/gof1.md` is its direct
descendant (same archive container, same three ciphers, same IF / EF / AT idea); section 12 lists every difference. All offsets are hexadecimal unless written decimal, little endian.

## 1. How a character gets loaded

Archives are opened once by `PB_MainInitAndFrameLoop` 0x433D30 (`PB_LoadPFile` 0x423E20): `.\01.dat` -> `g_DataArchiveSlots[9]`, `.\01p.dat` -> `[8]`, `.\02p.dat` -> `[7]`, `.\00.dat` ->
`g_BgmArchiveSlots[4]`, `.\00p.dat` -> `[3]` (`g_DataArchiveSlots` 0x17E9FE0, 10 x 28 bytes; `g_BgmArchiveSlots` 0x161E8F0). `Load_File_From_PB_Archive` 0x42BFA0 searches slots 0..9 in order and the
first archive that has the (upper-cased basename) name wins, so **`02p.dat` shadows `01p.dat` shadows `01.dat`** (`SUBARU.DAT`, `EIMI.WMT` ... exist in several). `.\02.dat`, `.\03.dat`, `.\04.dat`
(54..64 MB movie archives) are not data archives.

| step | function | file / action |
|---|---|---|
| 0 | `PB_LoadSystemAssetsAndEnterOpening` 0x432F40 | `Load_Effect_DAT` 0x42C3D0 = `Load_DAT_File_Wrapper` 0x42C430(4, `.\data\effect.dat`) into slot 4 (`g_EffectDatSlot` 0x15ED108, a Pb2FighterSlot used by EF type 8 and PB_ObjSpawnDecorEffectObject) and `PB_LoadVectorTxt` 0x42CF70 (`.\vector.txt`, section 9) |
| 1 | `CSS_LoadCharselCtGrid` 0x42CA40 (called by `PB_SceneCharaStageSelectTick` 0x403490, `PB_SceneMenuTick` 0x4413F0) | `CHARSEL.CT` (section 8), fills `g_CharTable` 0x177CDD0 |
| 2 | `PB_SceneVsDemoAndMatchLoadTick` 0x40F270 -> thread `PB_MatchLoaderThread` 0x40F030 | per slot 0..3 (slot + 2 = tag partner when `g_tag_two_char_mode == 1`): `Load_DAT_File_And_Runtime_Decrypt` 0x42C460(slot, `.\data\<datFile>`), `datFile` = `Pb2CharTableEntry.datFile` of `g_CharFileIdToTableIndex[charFileId]`; story mode takes the opponent from `g_story_opponent_table` |
| 3 | `PB_SceneVsDemoAndMatchLoadTick` per slot: `PB_SetupFighterSlotFromTableIndex` 0x403EC0 | `Parse_Character_DAT_Data` 0x42C560(slot, palette) -> `PB_BindCharacterData` 0x42CA10 (slot.object.charData / spriteBank) -> `PB_SlotLoadCommandTable` 0x4276B0 (`<ctFile>` with `.txt` replaced by `.ct`, section 6) -> `PB_InitFighterSlotForRound` 0x431E50 -> `PB_SlotLoadCpuScript` 0x403FE0 (`<comFile>`, section 10) |
| 3b | replay: `PB_ReplayBeginPlayback` 0x442DA0 -> `PB_SetupFighterSlotFromCharTable` 0x403CD0 | same chain, loading the DAT synchronously through `Load_DAT_File_Wrapper` |
| 4 | win screen `PB_SceneWinScreenTick` 0x40E090 | `PB_LoadWinQuoteTable` 0x40DE50 (`.\data\<name>.wmt`, section 7) + `PB_PickWinQuote` 0x40DF20 |
| 5 | `PB_BattleCreate_InitFightersAndCamera` 0x432280 | `PB_InitFighterSlotForRound` for every slot |
| 6 | `PB_ReloadTrainingDummyCpuScripts` 0x442160 (called by `PB_BattlePauseMenuTick`, `PB_ReplayBeginPlayback`, `PB_PauseAndReplayControlTick`) | the four dummy training CPU scripts `guard.cpf`, `crouch.cpf`, `jump.cpf`, `deku.cpf` (`g_DummyCpuScriptNames` 0x4634C0) via `PB_SlotLoadCpuScript` |

Anti-tamper quirks in the same chain (T): `PB_SetupFighterSlotFromCharTable` and `PB_ObjRunActionScript` reset the config and `exit(0)` when `charFileId == 34` (AZUSA) is **human controlled**
(`controlCode == 0`) or when a human-controlled slot loads a data path that contains the string at `SubStr`; AZUSA is CPU-only.

## 2. Ciphers

Stage 1 (PB/GOF1 archive, key 0xFA261EFB, `PB_Archive_XOR_Decrypt_With_Filename` 0x424000) is handled by fbarctool: the first 9696 bytes of every entry are enciphered with the upper-cased entry name
unless the archive is stored plain. The loader reads the character file in pieces (offsets 0 and `patternAreaEnd`), the piece at `patternAreaEnd` is always >= 9696 so it is never touched by stage 1.

Stage 2 is `XOR_Decrypt_WithString` 0x423B90(buf, size, limit, key): for `p` in `[0, min(size, limit))`: `buf[p] ^= (p + key[p % strlen(key)]) & 0xFF`, `p` restarting at 0 for every call.
`Load_DAT_File_And_Runtime_Decrypt` 0x42C460(slot, path), with `h5 = u32@0x14` and `h6 = u32@0x18` of the decrypted header:

1. read 0x41C bytes, decrypt them with **K1** (only to learn `h5`), free;
2. read `h5` bytes from file offset 0 into `g_CharPatternData[slot]`; decrypt `[0, 0x41C)` with K1 and `[0x41C, h5)` with **K2**;
3. read `h6 + 0x4000` bytes from file offset `h5` into `g_CharSpriteBank[slot]`; decrypt `[0, h6)` with **K3**; fail (return 0, no further work) unless `u32@0x10 == 15`;
4. decrypt the final 0x4000 bytes (`bank + h6`, the pattern-name tail, never read by the game afterwards) with K3, `p` restarting at 0.

File size = `h5 + h6 + 0x4000` (ASAHI: 148902 + 1370704 + 16384 = 1535990, D for all 16 files).

| key | IDB name | bytes (hex, NUL terminator not included) |
|---|---|---|
| K1 (30 B) | `g_xor_key_memory_error` 0x4614D8 | `4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e` = `Memoryえらーってことにシテオク` |
| K2 (38 B) | `g_xor_key_mendou_troublesome` 0x4614B0 | `4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb` = `Meンどう--な事はYAりたくNaいんだけどね` |
| K3 (34 B) | `g_xor_key_hima_gokurou` 0x4614F8 | `686982dcc56e6f834a82c982e581482082b28bea984a836982b1546f82be82c982e5` = `hiま` + **0xC5** + `noカにょ？ ご苦労ナこToだにょ` |

Use the hex: K3 contains the half-width byte 0xC5 (not `で`), and the earlier note `PB_CHAR_DAT_FORMAT_SOLVED.md` has K3 retyped wrongly and the header table size wrong (it is 256 entries, not 250).

```
def xd(b, off, n, key):
    for p in range(n): b[off+p] ^= (p + key[p % len(key)]) & 0xFF
def decrypt(raw):                       # raw = stage-1 plain file
    b = bytearray(raw); xd(b, 0, 0x41C, K1)
    h5, h6 = struct.unpack_from('<II', b, 0x14)
    xd(b, 0x41C, h5 - 0x41C, K2); xd(b, h5, h6, K3); xd(b, h5 + h6, 0x4000, K3); return b
```

Encryption is the same function (XOR with a position-keyed stream). A rewriter must keep `u32@0x10 == 15`, `h5`, `h6` consistent and the file size `h5 + h6 + 0x4000`.

## 3. Shared conventions

Pattern ids and frame indices are u8, positions are 1/256 pixel units (`PB_InitChildObject`: `posX = parent.posX + ((arg0 - 128) << 8)`; screen x = `(posX - cameraX + 0x8000) >> 8`), the screen is 640x480 (frame
space 256x256 with origin (128, 224)), facing 0 = right. Fighters live in `g_fighter_slots` 0x161E980 (4 x 5864, ends exactly at `g_demo_attract_mode` 0x1624520), spawned objects in
`g_object_slots` 0x1624528 (1000 x 608); both hold the same 604-byte `Pb2Object` at slot + 4. Slot layout (Pb2FighterSlot): +0 active, +4 Pb2Object (0x25C), +0x260 `Pb2CtHeader` (28 B, CT file +4204),
+0x27C `commandCount`, +0x280 100 x `Pb2CommandMove` (42 B), +0x12E8 / +0x14E8 two 512-byte input history rings (section 11).

## 4. Character `.DAT`

```
0x000            Pb2DatHeader (0x41C): magic[8], editorFlags08, editorValue0C, checkValue = 15, patternAreaEnd (h5), spriteBankSize (h6), patternOffset[256]
0x41C            patterns in slot order: Pb2PatternHeader (20) + frameCount * 96 + box / AT / IF / EF tables, up to h5
h5               sprite bank (h6 bytes): header 12220, sprite table, sprite groups with pixel data
h5 + h6          256 x 64-byte pattern names (CP932, editor only), 0x4000 bytes
```

### 4.1 Header (`Pb2DatHeader`, decrypted)

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | char[8] | `magic` = `時は来た` (8E 9E 82 CD 97 88 82 BD) | X D: never compared |
| 0x08 | u32 | `editorFlags08`: 0x10000 in 8 files, 0x18000, 0x10100, 0x1010100, 0x1E000, 0x1FF00, 0x668BF000, 0x668AF000 | X D: never read |
| 0x0C | u32 | `editorValue0C`: 0 (7 files), 0x100, 0x200, 0x600100, 0xD, 0x1000100, garbage | X D: never read |
| 0x10 | u32 | `checkValue` = 15 | T D: `Load_DAT_File_And_Runtime_Decrypt` returns 0 otherwise |
| 0x14 | u32 | `patternAreaEnd` (h5) = start of the sprite bank | T D |
| 0x18 | u32 | `spriteBankSize` (h6) | T D |
| 0x1C | u32[256] | `patternOffset[i]`: absolute file offset of pattern i, 0xFFFFFFFF = absent. First present = 0x41C, strictly ascending, packed (each pattern ends where the next starts, the last at h5) | T D (`PB_ObjResolveFramePointers` 0x424780: `charData + *(u32*)(charData + 4*pattern + 28)`) |

Present patterns per file: 56..127 (SAMPLE.DAT 1). The engine hard-codes pattern ids (section 12.2).

### 4.2 Pattern header (`Pb2PatternHeader`, 20 bytes)

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u8 | `frameCount` 1..100 | T D |
| 0x01 | u8 | `moveInfo`: bit 7 = double-resolution pattern (`PB_ObjIsPatternDoubleRes` 0x4262D0 halves boxes and zoom, 17 patterns); bit 6 = linear texture filter (`PB_ObjIsPatternLinearFiltered` 0x4262F0, never set in the data); bits 0..6 + 1 = KO severity -> `obj.koMoveType` (HitReaction reads `pattern[1] & 0x7F`); values 0, 2, 6, 0x80 | T D |
| 0x02 | u8 | `moveLevel`: priority class (`PB_ObjGetPatternMoveLevel` 0x4262B0; `PB_ObjRequestAttackFromInput` only lets a request through when the running level `<=` the requested one); 0, 2, 3, 4 | T D |
| 0x03 | u8 | `editorTag`: 0xC7 in 1300 of 1461 patterns, 0x87 in 80, 0 in 80, 0x6B in 1 | X D: never read |
| 0x04 | i32 | `boxTableOffset`, relative to the pattern start, -1 = none -> `obj.boxTable` | T D |
| 0x08 | i32 | `atTableOffset`, -1 = none (`PB_ObjCheckAttackVsFighters` reads `pattern + this + 26 * frame.atIndex`) | T D |
| 0x0C | i32 | `ifTableOffset` -> `obj.ifTable` | T D |
| 0x10 | i32 | `efTableOffset` -> `obj.efTable` | T D |

Frames follow at +0x14 (96 bytes each), then the present tables in the order **box (4 B), AT (26 B), IF (20 B), EF (12 B)**, each starting exactly where the previous ended, the last table ending at the pattern end
(D: all 1461 patterns tile exactly, each table size is a multiple of its record size).

### 4.3 Frame record (`Pb2FrameRecord`, 96 bytes = `Pb2AnimFrame` 24 + `Pb2StateFrame` 36 + indices 36)

`PB_ObjResolveFramePointers`: `frame = pattern + 20 + 96 * obj.frame` -> `obj.frameRecord` = `obj.animFrame`, `obj.stateFrame = frame + 24`, `obj.boxIndexBlock = frame + 76`.

`Pb2AnimFrame` (frame +0x00, 24 bytes):

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u16 | `spriteId`: index into the bank `groupOffset[1000]` table, i.e. a sprite **group** (`PB_QueueFighterSprite` 0x42F760: `bank[spriteId + 5]` = group offset, -1 = not drawn) | T D (all 14694 frames reference a present group) |
| 0x02 | i16 | `offsetX` (the hit shake subtracts from it) | T D |
| 0x04 | i16 | `offsetY` | T D |
| 0x06 | u8 | `duration`: ticks before advancing (`PB_ObjRunActionScript`: `++obj.frameTicks >= anim[6]`) | T D |
| 0x07 | u8 | `drawMode`: index into `g_DrawModeTransformTable` 0x461108 (`[2 * mode + facing]`); 8 shifts the origin by 100 (`PB_ObjBoxToWorld`); data 0, 1, 8 | T D |
| 0x08 | u8 | `blendMode`: 0 opaque, non-zero = blended using `alpha`; data 0, 1, 2 (meaning of 2 U) | T D |
| 0x09 | u8 | `alpha` 0..255 | T D |
| 0x0A | u8 | `aniFlag`: 0 end of animation (-> pattern `jumpTarget`), 1 next frame, 2 jump to frame `jumpTarget` (consumes `loopCounter`), 3 = 1 and land -> pattern, 4 = 2 and land -> pattern, 5 counted loop (`loopCounter` reaches 0 -> `loopEndFrame`) | T D |
| 0x0B | u8 | `jumpTarget` | T D |
| 0x0C | u8 | `landJumpTarget` (frame / pattern entered when the actor lands; aniFlag 3 / 4) | T D |
| 0x0D | u8 | `drawPriority`: 0 none, 1 front, 2 back (`PB_ObjApplyDrawPriority` 0x426010 -> `obj.drawDepth` 0x7D..0x80) | T D |
| 0x0E | u16 | `zoom`: 256 = 1.0, 0 = none | T D |
| 0x10 | u8 | `loopCount` -> `obj.loopCounter` when non-zero | T D |
| 0x11 | u8 | `loopEndFrame` | T D |
| 0x12 | u8[6] | `unused_12` | D: zero in all 14694 frames, no access in the register-tracking census over all 932 functions |

`Pb2StateFrame` (frame +0x18, 36 bytes):

| off (frame) | type | name | evidence |
|---|---|---|---|
| 0x18 | u8 | `clearVelX`: zero velX / accelX / maxVelX (`PB_ObjApplyFrameMotionFlags` 0x425560) | T D |
| 0x19 | u8 | `clearVelY` | T D |
| 0x1A | u8 | `addVelX`: add velX / accelX / maxVelX (negated when facing left) | T D |
| 0x1B | u8 | `addVelY` | T D |
| 0x1C | i16 | `velX` | T D |
| 0x1E | i16 | `velY` | T D |
| 0x20 | i16 | `accelX` | T D |
| 0x22 | i16 | `accelY` | T D |
| 0x24 | u8 | `stance` 0 ground, 1 air, 2 crouch (-> `obj.stanceCopy`; air physics, cancel rules) | T D |
| 0x25 | u8 | `normalCancel` 0 / 1 / 2: class 0 command moves (2 always, 1 after hit / guard) | T D |
| 0x26 | u8 | `specialCancel`: class 1 / 2 command moves | T D |
| 0x27 | u8 | `attackHitCount` -> `obj.attackHitsLeft` on frame entry; attack boxes are ignored when 0 | T D |
| 0x28 | u8 | `canAct` 1 = actor may act (`stance | canAct << 4` selects the input dispatch class, `g_InputDispatchClassByStanceCanAct`) | T D |
| 0x29 | u8[3] | `unused_11` | D |
| 0x2C | u32 | `flags1`: bit0 carry velocity as inertia, bit1 clear inertia, bit31 motion only on first entry | T D |
| 0x30 | u32 | `flags2`: bit0 class-2 moves need a connected attack, bit1 throw techable (`PB_ObjTryThrowTech`), bit2 jump-cancel on hit (U), bit31 (U: blocks the dash-turn in `PB_ObjDispatchInputByStance`); bits 16..23 (GOF1 invulnerability / counter) are 0 in all PB data | T D |
| 0x34 | i16 | `maxVelX` | T D |
| 0x36 | u8[6] | `unused_1E` | D |

Index block (frame +0x3C, 36 bytes):

| off | type | name | evidence |
|---|---|---|---|
| 0x3C | u8 | `atIndex`: index into the AT table, 0xFF = none; always < table size, used indices are 0, 1, 2 ... in frame order | T D |
| 0x3D | u8 | `atIndexHighByte`: 0 / 1 / 0xFF / editor garbage (119 distinct values) | X D |
| 0x3E | i16[3] | `ifIndex`, -1 = empty (`PB_ObjRunFrameEventsForPhase` 0x4280E0: `i = 62; i < 68; i += 2`) | T D |
| 0x44 | i16[4] | `efIndex`, -1 = empty (`PB_ObjRunFrameEntryEffects` 0x413220: `i = 68; i < 76; i += 2`) | T D |
| 0x4C | i16[10] | `boxIndex`, -1 = empty: [0] pushbox (`PB_ObjResolvePushboxCollision`), [1..3] hurt boxes (`PB_ObjCheckAttackVsFighters` loop at +2), [4] probe / grab box (IF 14 / 15 / 19 / 20), [5] special (read only via `PB_ObjFindOverlappingTarget`), [6] reflect / clash box (`PB_ObjCheckReflectBoxVsAttacks`), [7] projectile hurt box (IF 19 target), [8..9] attack boxes | T D |

Used indices of a pattern are exactly 0, 1, 2 ... in frame order (the editor appends records without de-duplication): the table length equals the number of used slots (D).

### 4.4 Box table (`Pb2Box`, 4 bytes)

`u8 x1, y1, x2, y2` in frame space with origin (128, 224) (GOF1 / MBR use 8-byte 16-bit boxes). `PB_ObjBoxToWorld` 0x42E1E0 transforms it (`(x - 128) << 8 + posX`, mirrored `256 - x` when facing left, double-resolution patterns
halve it, x / y are swapped when reversed). D: 18434 boxes in the 16 files, 232 have `x1 > x2` or `y1 > y2` (the transform swaps them).

### 4.5 AT record (`Pb2AtRecord`, 26 bytes)

`pattern + atTableOffset + 26 * frame.atIndex`; the pointers are queued in `obj.hitAt[8]` (`PB_ObjCheckAttackVsFighters`) and consumed by `PB_ObjFighterStateAndHitReaction` 0x439280.

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u8 | `hitReaction`: row of `g_HitReactionActionByStance` 0x461E68 / `g_HitReactionClassByStance` 0x461EE0 (what the victim plays) and of `g_HitVectorTable` | T D |
| 0x01 | u8 | `hitstopLevel`: index into `g_HitstopFramesByLevel` 0x161E8D8 (VECTOR.TXT section 3) -> hit-stop and screen shake | T D |
| 0x02 | u8 | `hitSparkClass`: < 14 selects the spark (`g_HitSparkTable` 0x461DB0, rows of 6 words) and the default hit sound | T D |
| 0x03 | u8 | `attackKind`: `g_AtKindTargetsObjectsOnly` 0x461D5C / `g_AtKindImmunityChannel` 0x461D3C, switch in HitReaction | T D |
| 0x04 | u8 | `hitSoundOverride` (`PB_QueueSoundPlay` when non-zero) | T D |
| 0x05 | u8 | `flagsA` (`Pb2AtFlagsA`): 1 chip damage, 2 no KO, 8 standing reaction, 0x10 knockback toward attacker, 0x40 air-guard rule (`PB_AtClassVsInvulnHit`); 0x20 / 0x80 U | T D |
| 0x06 | i16 | `damage` (-> `PB_Damage_ScaleByComboAndLife` 0x43C800) | T D |
| 0x08 | i16 | `meterGain` (attacker +8, victim >> 2 / guarded >> 3) | T D |
| 0x0A | i16 | `guardBreakValue` (added to the victim `guardGauge`) | T D |
| 0x0C | u8[3] | `unused_0C` | D: zero in all 1545 records |
| 0x0F | u8 | `statusEffect` -> `obj.tintMode` when non-zero | T D (always 0) |
| 0x10 | u8 | `flagsB` (`Pb2AtFlagsB`): 1 extra spark, 2 no horizontal knockback, 4 no combo count, 8 screen shake, 0x10 air-combo starter, 0x20 no throw tech; 0x40 U | T D |
| 0x11 | u8 | `unused_11` | D |
| 0x12 | u16 | `hitClass`: 0 high strike, 1 low strike, 2 high throw, 3 low throw, 4 unguardable (`PB_ObjCheckAttackVsObjects` reads the word, guard-height check) | T D |
| 0x14 | u8[6] | `unused_14` | D |

### 4.6 IF record (`Pb2IfRecord`, 20 bytes)

`u8 type; u8 unused_01[3]; i32 param[4]` (p0 = +4, p1 = +8, p2 = +12, p3 = +16). `PB_ObjRunFrameIfRecord` 0x428160 (switch); `g_IfTypePhaseTable` 0x461048 decides the phase (types 14, 15, 19, 20 run in phase 1
after hit detection, all others in phase 0). "Jump(x)" = `PB_ObjJumpToFrame` 0x428140 (frame = x, ticks 0, re-enter at once); "Request(x)" = `PB_ObjRequestAction` (pattern x, only when nothing is pending).
Types present in the data (3232 records, D): 1 (129), 2 (1511), 3 (615), 4 (30), 5 (16), 6 (111), 7 (12), 8 (87), 9 (60), 10 (56), 11 (168), 12 (108), 13 (23), 14 (107), 15 (9), 16 (4), 17 (12), 19 (17), 21 (6), 22 (12), 24 (9), 25 (16), 26 (4), 27 (102), 28 (4), 50 (4).
Types 29..32 of GOF1 do not exist.

| type | name | parameters (T, from the handler) |
|---|---|---|
| 1 | DIRECTION_ACTION | p0 = held direction (10 = 6 or 3), p1 = pattern to Request, p2 = 0 fires while held / 1 while not held |
| 2 | END_CONDITION | sets `endConditionMet` (the object is removed next tick): p0 != 0 when x is outside the camera window (-45568 .. +61952), p1 != 0 when `landed`, p2 != 0 when `animationEnded`; p3 = counter spend (tens digit = counter, applied to the root fighter) |
| 3 | HIT_RESULT_JUMP | needs `attackHitsLeft == 0` and `attackResult != 0`: p0 = frame, p1 = accepted result mask, p2 = 0 any / 1 victim on ground / 2 victim in air, p3 = counter spend; clears `landed` / posY after the jump |
| 4 | VELOCITY_SIGN_JUMP | p0 = frame, p1 = X sign test (0 any, 1 forward, 2 backward), p2 = Y sign test (0 any, 1 down, 2 up) |
| 5 | INVINCIBLE_JUMP | p0 = frame, fires while `invulnFlags != 0` (clears `tintMode`) |
| 6 | INPUT_JUMP | p0 = direction (255 any), p1 = button mask (0xFF any), p2 = frame, p3 = flags: 1 released-variant, 2 use the pressed-button bank (>> 12), 4 negate and set `inputActionLatch`, 8 / 0x10 gate on the root fighter hit state; root fighter for children |
| 7 | INPUT_ACTION | like 6 but Request(p2) with priority 100 |
| 8 | RANDOM_JUMP | p0 = frame, p1 = probability out of 512 |
| 9 | SET_LOOP_COUNTER | `loopCounter = p0` |
| 10 | LOOP_ZERO_JUMP | Jump(p0) when `loopCounter == 0` |
| 11 | COMMAND_MOVE_ACTION | p0 = command index in the slot table, p1 = required `attackResult` (0 none, 1 any of 7, 2 guarded, 3 any of 5), p2 = frame (0 = Request the command target pattern with priority 300); needs the meter cost, CPU: `cpuCommandReady`, human: `PB_SlotMatchCommandSequence`; sets `activeMoveMeterCost`, flag 0x10 invalidates the input history |
| 12 | NEAREST_DISTANCE_JUMP | Request frame p0 (priority 255) when the nearest fighter is at least p1 pixels away; p2 == 1: only when that fighter is on screen (8 .. 312) |
| 13 | SCREEN_EDGE_ACTION | when the actor is outside 8 .. 312 screen pixels (and direction == p2, 255 any): p1 == 0 Request frame p0 (255), else Request pattern p0 (priority 1000) |
| 14 | BOX_HIT_JUMP | phase 1: probe box slot 4 vs target box kind p2 (0 attack, 1 push, 2 hurt, 3..6 slots 4..7), team filter p1, `PB_ObjFindOverlappingTarget` 0x437AA0 -> Jump(p0); p2 == 0 consumes the target `attackHitsLeft`; hit-stop p3 on both |
| 15 | BOX_GRAB_JUMP | like 14 against fighters, then `PB_ObjTryGrabFighter` |
| 16 | CAMERA_FOLLOW | p0 == 0: `posX += camera dx`; p1 == 0: `posY += camera dy` |
| 17 | HIT_COUNT_JUMP | `attackResult != 0` and `attackHitsConnected >= p1`: Jump(p0); p2 counter spend |
| 18 | PARENT_ACTION_JUMP | children only: root fighter pattern == p1 (256 = root `actionChangedFlag`) -> Jump(p0) |
| 19 | REFLECT_OBJECT | phase 1: probe slot 4 vs slot 7 of other-team objects: takes the object over (flips owner / facing, p0 = speed scale / 256) |
| 20 | BOX_ENEMY_JUMP | probe slot p1 vs target kind p2 among fighters; p3 == 1 requires the target on screen; Jump(p0) |
| 21 | FIGHTER_PRESENT_JUMP | Jump(p0) when another fighter with `characterId == p1` is in the battle |
| 22 | STAGE_JUMP | Jump(p0) when `g_stage_index == p1` |
| 23 | MODE_JUMP | Jump(p0) when the byte at 0x16B8C28 == p1 (stage-effect mode, U) |
| 24 | COUNTER_AT_LEAST_JUMP | root `tobiCount[p1 / 10] >= p1 % 10` -> Jump(p0) |
| 25 | VARIABLE_COMPARE_JUMP | `paramVar[p1]` (root when p3 / 10 == 0) vs p2: p3 % 10 == 0 jumps when greater, 1 when less, 2 when equal; Jump(p0) |
| 26 | ADJUST_VELOCITY_BY_INPUT | p0 = direction group (10 / 11 / exact), p1 / p2 added to velX / velY |
| 27 | PARENT_GRABBED_JUMP | root `grabState != 0`: p1 != 0 Jump(p0) else Request(p0) priority 300 |
| 28 | ROUND_END_JUMP | Jump(p0) when `g_round_end_phase != 0` |
| 50 | ALLY_BOX_JUMP | frame = p0 when `PB_ObjAttackBoxTouchesAlly` (an own-team fighter overlaps an attack box) |

### 4.7 EF record (`Pb2EfRecord`, 12 bytes)

`u8 type; u8 subType; i16 arg[5]` (arg0 = +2 ... arg4 = +10). `PB_ObjRunFrameEntryEffects` 0x413220 runs the (up to four) records of a frame once on frame entry (`efRunState`).
Types present (D): 1 (1074), 2 (246), 3 (303), 4 (479), 5 (94), 6 (934), 8 (1100), 9 (1680), 30 (1); types 7 and 50 never appear.

| type | name | handler | meaning (T) |
|---|---|---|---|
| 1 | SPAWN_CHILD_OBJECT | `PB_SpawnChildObjectFromFrameEffect` 0x414330 / `PB_InitChildObject` 0x414100 | subType = child pattern; arg0 / arg1 = x / y offset (128 / 224 = origin); arg2 = `spawnFlagsA` (1, 2, 4, 8, 0x10 screen-relative, 0x20, 0x40, sign bit = absolute Y); arg3 = `spawnFlagsB` (1, 2, 4, 8); arg4 low byte = homing mode (subType = homing base action) |
| 2 | SPAWN_EFFECT | `PB_EfType2_SpawnEffectById` 0x414660 -> `PB_SpawnEffectById` 0x414760 | subType = effect id, arg0 / arg1 position, arg2..arg4 parameters |
| 3 | SPAWN_PARTICLES | `PB_EfType3_SpawnParticlesById` 0x4145F0 | subType = particle set id, arg0 / 1 position |
| 4 | CONTROL_GRABBED | `PB_EfType4_ControlGrabbedOpponent` 0x41C7A0 | subType = victim pattern; arg0 / arg1 offset; arg2 = `frame % 1000 + 1000 * drawModeVariant` (`/ 10000 != 0` places the victim relative to the grabber); arg3 flags: 1 release (applies `PB_ObjStartKnockback`), 2 flagged knockback, 4 zero Y velocity; arg4 = hit reaction id |
| 5 | THROW_DAMAGE | `PB_EfType5_ThrowDamageToOpponent` 0x41CA00 | subType 0 set the victim `tintMode` (arg0), 1 apply damage arg0 (arg4 != 0 uses the combo scaling) + combo stats arg1 + hit-stop arg2 + sound arg3, 2 turn the victim |
| 6 | ACTOR_OP | `PB_EfType6_ActorOp` 0x41CB20 | subType = op (table below) |
| 7 | SYSTEM_PARTICLE | `PB_EfType7_SpawnSystemParticle` 0x4146D0 | subType = id |
| 8 | SPAWN_EFFECT_DAT_OBJECT | `PB_EfType8_SpawnEffectDatObject` 0x4143A0 | like type 1 but the child uses `g_EffectDatSlot` (EFFECT.DAT) pattern data |
| 9 | PLAY_SOUND | inline | subType 0 = sound id arg0, 1 = per-player bank `arg0 + 50 * (slotIndex + 8)`; arg1 = skip chance out of 512 (0 = always) |
| 30 | SET_ACTOR_PARAMS | `PB_EfType30_SetActorParams` 0x41D330 | subType 0: homing offsetX / offsetY / timeout = arg0 / arg1 / arg2 (when non-zero); 1: `homingParam[arg0] = arg1` |
| 50 | SPAWN_CHILD_FROM_PARENT | `PB_EfType50_SpawnChildFromParent` 0x41D390 | subType = child pattern, arg0 = Y, arg1 / arg2 / arg3 = velX / velY / accelY; x = parent +-320 px |

Type 6 sub-types (D counts): 0 SET_TRAIL_EFFECT (1; arg0 255 = off, else `trailMode = arg0 + 1`), 1 SCREEN_SHAKE_KO_SLOWMO (160; arg0 shake timer, arg1 / arg2 effect id / level via `PB_RequestScreenEffectLevel`, arg3 KO slow-motion frames),
2 SET_FREEZE_TIMERS (6; `freezeTimerA / B = arg0 / arg1`), 3 SET_AFTERIMAGE (162; arg0 mode (255 off), arg1 steps <= 119, arg2 spacing), 4 ADD_LIFE_METER_RESERVE (6; life += arg0 clamped 0..11400,
superMeter += arg1 capped at 10000 * ct.maxSuperLevels, reserveGauge += arg2 clamped 0..10000), 5 FLIP_FACING (99; arg0 selects victim / nearest opponent / input based turn variants), 6 SET_VELOCITY_RANGE (304; random velX / accelX in
[arg0, arg1) / [arg2, arg3), arg4 != 0 selects velY / accelY), 7 SPAWN_FRAME_GHOST (46; `PB_ObjSpawnFrameGhost`: arg0 trail mode, arg1 / arg2 zoom growth, arg3 life ticks, arg4 flags), 8 AIM_AT_OPPONENT (11; speed arg1 along the
`g_SinTableDegrees` / `g_CosTableDegrees` angle to the nearest opponent, arg2 = number of direction sprites, arg0 = base frame), 9 SET_INPUT_LOCK (4; root `inputLock = arg0`), 100 ADD_COUNTER (39; arg0 tens = counter, units = amount, on the root),
101 SUB_COUNTER (11), 102 ADD_DASH_COUNT (8), 103 SUB_DASH_COUNT (0 in the data), 105 SET_OR_ADD_PARAM_VAR (22; `paramVar[arg0] = arg1`, `+=` when arg2 % 10 == 1), 254 SET_SCRIPT_FLAG (23), 255 SET_INVULN_FLAG (32; `invulnFlags |= 0x10`).

### 4.8 Sprite bank (`Pb2SpriteBankHeader` ... , h6 bytes at file offset h5)

Loader `Parse_Character_DAT_Data` 0x42C560(slot, paletteIndex); the bank is `g_CharSpriteBank[slot]`.

| off | type | name | evidence |
|---|---|---|---|
| 0x0000 | u8[16] | `editorGarbage` (the tail of key K3 `82 C9 82 E5 81 48` ...) | X D |
| 0x0010 | u32 | `is24BitFlag`: 255 = 3 bytes (BGR) per pixel, otherwise 8-bit palette indices; always 0 in the shipped files | T D |
| 0x0014 | u32[1000] | `groupOffset[g]`: bank-relative offset of sprite group g, 0xFFFFFFFF = none (330 .. 549 present per file); `frame.anim.spriteId` indexes this table | T D |
| 0x0FB4 | u32[2048] | `palette`: 8 palettes of 256 entries, 4 bytes `{R, G, B, 1}` each (channel order from `PB_BlitPalettedToSurface`: byte 0 -> `g_PixelTableChannel0` = red; the 4th byte is 1 in all 16384 entries and never read). The loader copies palette `paletteIndex << 10` (0..7, `Pb2Object.paletteIndex`) and forces **entry 0 to 0 (transparent)** | T D |
| 0x2FB4 | u32 | `spriteCount` = sum of all group counts (D) | D |
| 0x2FB8 | u32 | `groupRegionSize` = h6 - first group offset (D) | D |
| 0x2FBC | Pb2SpriteRecord[spriteCount] | sprite table, 14 bytes each; ends **exactly** at the first group offset (D) | T D |

`Pb2SpriteRecord` (14 bytes, base bank + 12220): `i16 offsetX` (+0), `i16 offsetY` (+2) (draw offset of the piece inside the group image, 0..848; read by `PB_QueueFighterSprite` as `*v22`, `v22[1]`),
`i16 width` (+4), `i16 height` (+6), `i16 texU` (+8), `i16 texV` (+10) (placement in the 256 x 256 texture page), `i16 texturePage` (+12; 0..49, the loader rewrites it to `50 * slot + 2 + page`; more than 50 pages shows an error box). The loader reads from +4.

Sprite groups (`Pb2SpriteGroupHeader`, 52 bytes at `groupOffset[g]`): `char name[12]` (`ASA0000.TIM`, X D), `u8 editorGarbage[24]` (X D), `u16 rect[4]` (+36: x1 y1 x2 y2 of the assembled image, X D), `u32 firstSprite` (+44, T D),
`u16 spriteCount` (+48, T D), `u16` (+50, X D, non-zero in 36 of 5259 groups), then `sum(width * height)` bytes of pixel data (consecutive per sprite, rows of `width` palette indices). A group is one frame image cut into pieces;
`PB_QueueFighterSprite` draws its `spriteCount` consecutive sprites starting at `firstSprite`. D: each file's groups tile the sprite table (ranges `[firstSprite, firstSprite + count)` cover 0 .. spriteCount - 1 exactly once),
every rectangle fits its 256 x 256 page, every group's pixels fit inside the bank.

### 4.9 Pattern-name tail

The last 0x4000 bytes: 256 x 64-byte CP932 names (D: 35..106 per file), e.g. ASAHI 0 `立ち`, 1 `立ち弱攻撃`, 4 `しゃがみ弱攻撃`, 10 `前進`, 17 `立ちガード`, 23 `頭やられ`, 30 `登場`, 35 `交代`. The loader decrypts the tail
with K3 but nothing reads it afterwards (X).

## 5. Where the engine reads what (call chain)

`PB_Fighters_InputAndControlStep` 0x437460 (per fighter: `PB_ReadBattleInputForPlayer` 0x424410, `PB_ReplayRecordOrPlaybackFighterInput` 0x437380, `PB_UpdateFighterInputHistoryRings` 0x437190, `PB_ObjRunActionScript` 0x424BA0,
`PB_ObjIntegrateMotion` 0x4259F0, `PB_FighterCpuAiStep` 0x4042A0 or `PB_SlotStartFirstMatchingCommand` 0x427F10 / `PB_SlotRunInputDispatch` 0x427680 -> `PB_ObjDispatchInputByStance` 0x426D40) ->
`PB_ObjEnterCurrentAction` 0x424620 (`PB_ObjResolveFramePointers`, `PB_ObjApplyDrawPriority`, `loopCounter`, `attackHitsLeft`, `PB_ObjApplyFrameMotionFlags`) -> `PB_ObjRunFrameEventsForPhase` 0x4280E0 / `PB_ObjRunFrameIfRecord` 0x428160 (IF)
and `PB_ObjRunFrameEntryEffects` 0x413220 (EF); `PB_Objects_HitDetectionPass` 0x413950 -> `PB_ObjCheckAttackVsFighters` 0x4382C0 / `PB_ObjCheckAttackVsObjects` 0x438680 (boxes + AT) -> `PB_ObjFighterStateAndHitReaction` 0x439280
(-> `PB_ObjStartKnockback` 0x43A740, `PB_Damage_ScaleByComboAndLife` 0x43C800, `PB_AddSuperMeter` 0x43C5F0, `PB_ComboRecord_RegisterHit` 0x43C690); draw: `PB_DrawFighterSprite` 0x430900 -> `PB_QueueFighterSprite` 0x42F760;
`PB_ObjBoxToWorld` 0x42E1E0 for every box. The battle tick itself is the inline body of `case 1` of `PB_MainInitAndFrameLoop` (see the lineage document).

## 6. `<CHAR>_C.CT` command table (4232 bytes) - `PB_SlotLoadCommandTable` 0x4276B0

`Pb2CtFile`: `u32 commandCount` (-> slot + 0x27C), `Pb2CommandMove commands[100]` (file +4, 4200 bytes -> slot + 0x280), `Pb2CtHeader ct` (file +4204, 28 bytes -> slot + 0x260). The identical `<CHAR>.CCT` is never loaded (X D: byte-identical to the CT in ASAHI).

`Pb2CommandMove` (42 bytes):

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u8 | `commandId`: own index, 0xFF = unused slot | T D |
| 0x01 | u8 | `unused_01` | D: zero in all 1400 records |
| 0x02 | u8[32] | `sequence`: tokens 0..9 direction, 0x41..0x46 buttons A..F, 0x2B joiner, 0x54 hold, 0xFF terminator; parsed backwards (`PB_CommandSeq_ParseNextToken` 0x427750 / `PB_CommandSeq_ParsePrevToken` 0x427850 from `PB_SlotMatchCommandSequence` 0x427970). **Record bytes +0x12..+0x21 (sequence bytes 16..31) are cleared by `PB_InitFighterSlotForRound`**, so real sequences are at most 16 bytes | T D |
| 0x22 | u8 | `targetPattern` requested when the command fires | T D |
| 0x23 | u8 | `moveClass` 0 normal / 1 special / 2 super (cancel rule, `PB_SlotTryStartCommandMove` 0x427C30) | T D |
| 0x24 | u8 | `meterCostLevels` (needs `superMeter / 10000 >=` this) | T D |
| 0x25 | u8 | `requestParam` -> `obj.requestParam` | T D (0 in all data) |
| 0x26 | u8 | `counterRequirement`: tens = `tobiCount` index, units = amount (< 100) | T D |
| 0x27 | u8 | `repeatLimit`: refused when it is `<= *(slot + 236)` (an object counter at obj + 232 = `dashCount`); 0 / 1 / 2 in the data | T D |
| 0x28 | u8 | `flags`: 1 / 2 / 4 allowed stance (ground / air / crouch), 8 no cancel entry, 0x10 clear input history, 0x20 script-only (hidden from the input matcher `PB_SlotStartFirstMatchingCommand`), 0x40 guard-cancel capable (**166 shipped records**, e.g. 0x45) | T D |
| 0x29 | u8 | `flags2`: 1 use ring B, 2 strict sequence (`PB_SlotMatchCommandSequence`), 0x40 evade-only (refuses the normal path), 0x80 evade move | T D |

`Pb2CtHeader` (28 bytes): `+0 maxAirActions` (air dashes / air jumps, `PB_ObjDispatchInputByStance`; 1, IKUMI 2), `+1 maxSuperLevels` (meter cap = 10000 * this; 9 in all files), `+2 recoveryStyle` (`PB_ObjRunActionScript` switch, 1 in all files),
`+3`, `+4`, `+6..7` zero (U: no reader), `+5 flags` (`Pb2CtFlags`: 1 restrict repeat moves, 2 evade enabled, 8 just-guard, 0x10, 0x20 jump-cancel; 0x39 in every file), `+8 float knockbackScale` (0.9 .. 1.3 per character, `PB_ObjStartKnockback`),
`+12 float damageScaleByLifeTier[4]` (0.9 in all files, `PB_Damage_ScaleByComboAndLife` by victim life 2850 / 5700 / 8550). D (13 CT files, 395 definitions incl. duplicates): `commandCount` equals the number of defined records except AZUSA (21 vs 20);
`commandCount` only bounds the round-start clear loop (all 100 records are scanned when matching); CHISA record 0 holds editor text (id 0, `\r\nEND\r\n//...`), REIKO94 record 92 a CP932 space token.

## 7. `<CHAR>.WMT` win quotes (4 + N x 154 bytes) - `PB_LoadWinQuoteTable` 0x40DE50

`u32 count` (<= 100, buffer `g_WinQuoteRecords` 0x6ED0F8) then `Pb2WmtRecord` x count: `u8 opponentCharId` (0xFF = any; `PB_PickWinQuote` 0x40DF20 compares it with the losing character id `g_WinScreenOpponentCharId`), `u8 unreferenced_01` (D 0),
`u16 chanceDivisor` (0 always, n accepted with probability 1/n), `char text[150]` (CP932, NUL padded; drawn by `PB_QueueWinQuoteText` 0x42D360). D: file = 4 + 154 * count for all 14 shipped files (4..14 quotes).

## 8. `CHARSEL.CT` and `Pb2CharTableEntry` - `CSS_LoadCharselCtGrid` 0x42CA40

`CHARSEL.CT` (2188 bytes in `01.dat` = 4 + 168 * 13, 2356 in `01p.dat` / `02p.dat` = 14 entries incl. SUBARU): `u32 count` + `count x Pb2CharTableEntry`, enciphered with `g_xor_key_charsel_ct` 0x461564 (`837483408343838b82aa8ca982c282a982e882dc82b982f1`, 24 bytes,
`XOR_Decrypt_WithString` over the whole entry block, `p` from the first entry). The loader builds `g_CharTable` 0x177CDD0 (100 x 168), `g_CharFileIdToTableIndex` (index by `charFileId`) and `g_css_grid_char_ids`; entries hidden by `unlockMask` are swapped to the end of the table.

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | char[32] | `name` (`MIZUKI`; `sel_%s.bmp`, `win_%s.bmp`, `%s.wmt`) | T D |
| 0x20 | char[32] | `datFile` (`mizuki.dat`; `yuu.dat` for YU, `reiko.dat` for REIKO94) | T D |
| 0x40 | char[32] | `ctFile` (`mizuki_c.txt`: the extension is replaced by `.ct`, upper-cased) | T D |
| 0x60 | char[32] | `comFile` (`mizukicom.txt`) | T D |
| 0x80 | u32 | `tableIndex` (written at load) | T |
| 0x84 | u32 | `charFileId`: 0..12, IKUMI 20, AZUSA 34 (the CPU-only id), SUBARU 12 | T D |
| 0x88 | u32 | `homeStage` | T D |
| 0x8C | u32 | `selectVoiceId` (`PB_QueueSoundPlay(id + 360)`) | T D |
| 0x90 | u32 | `unlockMask` (0 always available; 256 SUZUKA, 65536 IKUMI, 1048576 REIKO94, 1024 PEACH, 1 AZUSA) | T D |
| 0x94 | u32 | `secondPage` | T D |
| 0x98 | u32 | `gridCell` = 10 * row + column | T D |
| 0x9C | i32 | `portraitX` | T D |
| 0xA0 | i32 | `portraitY` | T D |
| 0xA4 | u32 | `unreferenced_A4` | D 0 |

## 9. `VECTOR.TXT` - `PB_LoadVectorTxt` 0x42CF70

Plain text (CRLF; `//` comments and blank lines skipped by `PB_Text_SkipWhitespaceAndComments` 0x43D180 / `PB_Text_SkipToNextLine` 0x43D150 / `PB_Text_SkipToken` 0x43D240); three sections, each terminated by a line starting `END`:
1. rows of **9 integers** -> `g_HitVectorTable` 0x17E4688 (9 x i16 per row): `knockVelX, knockVelY, knockAccelX, knockAccelY, launchVelX, launchVelY, launchAccelX, launchAccelY, hitStage` (`PB_ObjStartKnockback`; row = AT `hitReaction`);
2. rows of **5 integers** -> `g_EvadeVectorTable` 0x154F908 (stride 9 i16): `velX, velY, accelX, accelY, freezeFrames` (`PB_ObjStartEvade` 0x426360);
3. one row of **7 integers** -> `g_HitstopFramesByLevel` 0x161E8D8 (u8): `10 14 18 1 24 40 3`.

`EFFECT.DAT` is an ordinary character file (56 patterns, 0 boxes / AT) loaded into slot 4 at startup; its patterns are the shared effect objects (EF type 8).

## 10. CPU scripts: `<CHAR>COM.TXT` (59048 bytes) and `*.CPF` (56048 bytes) - `PB_SlotLoadCpuScript` 0x403FE0

The whole archive entry is read to `g_CpuScriptData + 56048 * slot` (`Pb2CpfFile`, 56048 bytes; the 18 `*COM.TXT` files carry an extra 3000 bytes = 50 x 60-byte CP932 script names such as `コンボ１`, `対空`, `ダッシュ`, `投げ`, `飛び道具`, `3ゲージ`: X, editor only; reading them overruns into the next slot buffer, harmless because slots are loaded in order).

| off | type | name | evidence |
|---|---|---|---|
| 0x00 | u8 | `guardChancePercent`: `rand() % 100 <` this sets `obj.cpuGuardRequest` (`PB_FighterCpuAiStep` 0x4042A0) | T D (60, 90, 75, 100 ...) |
| 0x01 | u8 | `reactChancePercent`: `rand() % 100 >=` this skips the reaction table | T D |
| 0x02 | u8 | `evadeChancePercentAndReversalA`: used as a percent (tech / evade) **and** as command id #1 of the reversal list | T D |
| 0x03 | u8 | `reversalCommandB` | T D |
| 0x04 | u8 | `reversalCommandC` | T D |
| 0x05 | u8[3] | zero | D |
| 0x08 | u8 | 0x5A in 15 of 18 files, never read | X D |
| 0x09 | u8[199] | zero | D |
| 0xD0 | Pb2CpfRow[24] | 160-byte rows: `i32 scriptIndex[20]`, `i32 weight[20]` (`PB_CpuPickScriptFromRow` 0x404040 subtracts the weights from a 0..99 roll in order, the first entry that brings it to <= 0 wins, -1 if none). Row = `opponentStance + 3 * (distanceBucket + 4 * state)`, bucket thresholds 30 / 110 / 230 pixels | T D |
| 0xFD0 | Pb2CpfScript[50] | 1040 bytes = 20 x `Pb2CpfStep` (52 bytes): `u8 inputCode` (0 neutral, 1..6 dir 6 4 2 8 9 7, 7..10 buttons 1 2 4 8, 11..14 down + button, 15 command move, 0..15 D), `+2 u8 commandMoveId`, `+4 i16 duration`, `+6 i16 durationRandom`, `+8 u8 endOfScript`, `+9 u8` editor flag (0 / 1, X), `+0x2A u8 flags` (bit0 / bit1 advance early after a hit, bit2 sets `cpuCommandReady`), `+0x2B u8 commandDirection`; all other bytes 0 (D) | T D |

`PB_CpuApplyScriptStep` 0x404080 converts a step into `obj.inputDir` / `obj.inputButtons` and the step timer. The four `*.CPF` training dummies are the same layout (`GUARD.CPF` has only `guardChancePercent = 100`).

## 11. Runtime: `Pb2Object` (604 bytes), `Pb2FighterSlot` (5864), `Pb2ObjectSlot` (608), input rings

The full field list with offsets and evidence tags is `docs/formats/ida/pb2k1_types.h` (applied to `g_fighter_slots`, `g_object_slots`, `g_EffectDatSlot` and the prototypes of ~110 functions). Highlights (offsets in `Pb2Object`):

| off | field | notes |
|---|---|---|
| 0x00..0x07 | `slotIndex`, `characterId`, `controlType` (0 human / 1 CPU), `unreferenced_03`, `paletteIndex`, `objKind` (0 fighter, 31 ghost, 143, 255 child), `pattern`, `frame` | T |
| 0x08 | u16 `frameTicks` (doubles as the remaining life of a ghost object) | T |
| 0x40 / 0x44 / 0x48 | `life` (11400), `lifeDisplay`, `superMeter` (10000 per level, cap `10000 * ct.maxSuperLevels`) | T |
| 0x50 / 0x58 | `reserveGauge` (0..10000, preserved across rounds), `reservePending` | T |
| 0x5C / 0x5E | u16 `reserveHoldTimer`, `superMoveState` (reserve mode 0 charging / 1 full / 2 draining) | T |
| 0x64 | `guardGauge` (guard break above 10000; `guardRegenDelay` u8 at 0x61) | T |
| 0x6C..0x84 | `posX`, `posY` (1/256 px), `posXAtTickStart`, `posYAtTickStart`; 0x84 `velX`, 0x88 `velY`, i16 `accelX/Y`, `maxVelX`; 0x94 `knockVelX/Y`, i16 `knockAccelX/Y`; `inertiaX`, `carriedVelX` | T |
| 0xB0..0xC3 | `attackResult`, `hitstopFrames`, `grabState`, `inputLock`, freeze timers, `koFlag`, `attackHitsLeft`, `guardState`, `cpuGuardRequest`, `recoverMode`, `bounceCount`, `recoveryTimer`, `hitStage` (0xD0), `invulnFlags` (0xD5) | T |
| 0xDE.. | `tobiCount[10]` (0xDE), `dashCount` (0xE8), `paramVar[10]` (0xEA), `trailMode` (0xFE), afterimage config, `locomotionUsedMask`, `commandUsedMask[14]`, `hitsTakenInCombo`, `attackHitsConnected` | T |
| 0x12C | `launchVel/Accel` (4 x i16) | T |
| 0x134 / 0x154 / 0x1D4 | `hitAt[8]`, `hitRect[8]`, `hitAttacker[8]`; 0x1F4 `lastAttacker`, `grabbedBy`, `ownerFighter`, `linkedActor` | T |
| 0x204 / 0x208 / 0x20C | `inputDir`, `inputButtons`, `inputButtonsReleased` | T |
| 0x210.. | `teamIndex`, `endConditionMet`, `facingLeft`, `nextFacingLeft`, `pendingPattern` (0x216), priorities, `pendingFrame`, `inputActionLatch`, `actionChangedFlag`, `landed`, `actionEnded`, `stanceCopy` | T |
| 0x224..0x258 | `boxTable`, `charData`, `patternHeader`, `frameRecord`, `animFrame`, `stateFrame`, `ifTable`, `efTable`, `boxIndexBlock`, `spriteBank`, `slotPtr`, `selfPtr`, `actionAge`, `ranThisTick` | T |

Input rings (`Pb2InputRing`, 512 bytes, slot + 4840 = ring A buttons, slot + 5352 = ring B buttons | released): `u32 state[64]` = `(direction << 24) | buttons`, newest first; `u32 age[64]` frames in that state (capped at 1000000;
`PB_SlotInvalidateInputHistory` sets 1000000 to invalidate the history). Matched backwards by `PB_SlotMatchCommandSequence`.

## 12. Differences from GOF1 and engine pattern ids

### 12.1 Differences from GOF1 (`docs/formats/gof1.md`)

| item | PB2K1 | GOF1 |
|---|---|---|
| file header | 0x41C bytes, magic `時は来た`, `checkValue` 15 at 0x10, `patternAreaEnd` 0x14, `spriteBankSize` 0x18, `patternOffset[256]` at 0x1C | 0x44 bytes, magic `備前長船`, version 18 at 0x10, `patternAreaEnd` 0x14, `partsSize` 0x18, `cgOffset`, `cgSize`, offsets at 0x44 |
| ciphers | K1 header, K2 pattern area, K3 sprite bank + tail (the tail is decrypted by the game) | same keys; header cipher over 0x444 bytes, tail not decrypted |
| graphics | one palettised sprite bank: 1000 groups -> pieces (14-byte records) -> 8-bit pixels, 8 palettes | PAT v2 parts blob (92-byte parts, A8R8G8B8 rasters) + optional CG |
| frame | 96 bytes: AF 24 + AS 36 + atIndex + 3 IF + 4 EF + 10 box indices | 116 bytes: AF 40 + AS 40 + ... |
| duration | u8 | u16 |
| box | 4 bytes (u8 x1 y1 x2 y2) | 8 bytes (i16) |
| AT | 26 bytes, same fields | 26 bytes |
| IF | 20 bytes (type + 4 params), types 1..28, 50 | 28 bytes, types 1..32, 50 |
| EF | 12 bytes (type, subType, 5 x i16) | 20 bytes |
| CT | 4232 bytes: count + 100 x 42-byte commands + 28-byte header; cost / counter / repeat limit are single bytes; guard-cancel flag 0x40 used | 4632 bytes, 46-byte commands, u16 cost / counter |
| runtime | `Pb2Object` 604, fighter slot 5864 (4 + 604 + 28 + 4 + 4200 + 1024), object slot 608 | actor 656, slot 6316 |
| CPU script | 56048-byte `*COM.TXT` / `.CPF` (24 rows, 50 scripts x 20 steps x 52 bytes) | see GOF1 |
| name tail | 256 x 64 | 256 x 64 |

### 12.2 Engine-fixed pattern ids (T from the request / dispatch code, names from the shipped name tail)

0 stand, 1 / 2 / 3 stand A / B / C, 4 / 5 / 6 crouch A / B / C, 7 / 8 / 9 jump A / B / C, 10 walk forward, 11 walk back, 12 crouch transition, 13 crouch, 14 stand up, 15 / 16 stand / crouch turn, 17 / 18 / 19 stand / crouch / air guard,
20 / 21 / 22 jump neutral / forward / back, 23 / 24 / 25 hurt (head / body / crouch), 26 down, 27 air hurt, 28 tech / evade (`PB_ObjStartEvade`), 30 intro, 31 / 32 / 33 air dashes (neutral / forward / back), 35 swap (`交代`, also the KO-flight action requested by `PB_ObjRunActionScript`).

## 13. Invariants verified by `tools/fb/pb2k1_verify.py` (65 files, 0 failures)

16 character `.DAT` (12 characters + EFFECT + SAMPLE in `01.dat`, SUBARU in `01p.dat` and `02p.dat`), 14 `_C.CT`, 14 `.WMT`, 18 `COM.TXT` / `.CPF`, 3 `CHARSEL.CT`:

* header: magic, `checkValue` 15, file size `h5 + h6 + 0x4000`, first pattern at 0x41C, strictly ascending offsets, all patterns tile `[0x41C, h5)`;
* per pattern: 1..100 frames, tables in the order frames / box / AT / IF / EF with non-decreasing offsets, absent tables -1, each table size a multiple of its record size and the last table ending at the pattern end;
* per frame: the zero bytes (`unused_12`, `unused_11`, `unused_1E`) are zero, `drawMode` in {0, 1, 8}, `aniFlag <= 5`; every `atIndex`, `ifIndex`, `efIndex`, `boxIndex` is in range of its table and the used indices are 0, 1, 2 ... in frame order; every `spriteId` is a present group;
* IF records: bytes +1..3 zero, types in {1..28, 50}; AT records: `unused_0C`, `unused_11`, `unused_14` zero;
* sprite bank: `is24BitFlag != 255`, `spriteCount` and `groupRegionSize` match, the sprite table ends exactly at the first group, groups tile the sprite table, sprite rectangles inside 256 x 256, texture page < 50, offsets 0..1024, pixels inside the bank;
* CT: size 4232, ids equal indices, undefined records are 0xFF + zeros, header constants (`maxSuperLevels` 9, `recoveryStyle` 1, `flags` 0x39, floats), `requestParam` 0; WMT: size `4 + 154 * count`, NUL-padded CP932 text; CHARSEL: size `4 + 168 * count`, name suffixes, unique `charFileId`; CPU: size 56048 / 59048, row ids < 50 and weights 0..100, steps' unread bytes zero.

Counts (D, 16 character files): 1461 patterns, 14694 frames, 18434 boxes, 1545 AT, 3232 IF, 5911 EF, 5259 sprite groups, 34202 sprites, 1265 pattern names; per file `patterns / frames / boxes / AT / IF / EF / groups / sprites / names`:

| file | patterns | frames | boxes | AT | IF | EF | groups | sprites | names |
|---|---|---|---|---|---|---|---|---|---|
| 01/ASAHI | 102 | 1284 | 1749 | 169 | 295 | 438 | 330 | 1930 | 76 |
| 01/AYA | 127 | 1181 | 1420 | 86 | 339 | 343 | 549 | 3485 | 106 |
| 01/AZUSA | 96 | 1134 | 1041 | 172 | 402 | 321 | 381 | 3028 | 82 |
| 01/CHISA | 106 | 1123 | 1492 | 241 | 384 | 692 | 376 | 1845 | 94 |
| 01/EFFECT | 56 | 540 | 0 | 0 | 95 | 115 | 199 | 1373 | 46 |
| 01/EIMI | 96 | 1126 | 1617 | 68 | 205 | 448 | 357 | 1979 | 86 |
| 01/IKUMI | 105 | 868 | 888 | 96 | 262 | 346 | 361 | 2753 | 86 |
| 01/MINAMI | 86 | 944 | 1135 | 92 | 222 | 532 | 313 | 1954 | 77 |
| 01/MIZUKI | 101 | 890 | 1369 | 119 | 143 | 369 | 336 | 2425 | 84 |
| 01/PEACH | 127 | 1019 | 1242 | 95 | 203 | 337 | 386 | 3183 | 95 |
| 01/REIKO | 113 | 1057 | 1832 | 139 | 232 | 423 | 439 | 2729 | 99 |
| 01/SAMPLE | 1 | 6 | 0 | 0 | 0 | 0 | 9 | 71 | 35 |
| 01/SUZUKA | 75 | 703 | 1026 | 61 | 96 | 259 | 315 | 2256 | 61 |
| 01/YUU | 110 | 993 | 1434 | 91 | 148 | 481 | 364 | 2305 | 98 |
| 01p/SUBARU | 80 | 913 | 1099 | 58 | 103 | 404 | 272 | 1443 | 70 |
| 02p/SUBARU | 80 | 913 | 1090 | 58 | 103 | 403 | 272 | 1443 | 70 |

EF type 6 sub-types in the data: 0 (1), 1 (160), 2 (6), 3 (162), 4 (6), 5 (99), 6 (304), 7 (46), 8 (11), 9 (4), 100 (39), 101 (11), 102 (8), 105 (22), 254 (23), 255 (32).

## 14. What the game ignores / unproven

* Ignored (X): header `magic`, `editorFlags08`, `editorValue0C`; pattern `editorTag` (+3); frame `atIndexHighByte`; AT `unused_*`; IF `unused_01`; sprite-bank `editorGarbage`, group `name` / `editorGarbage` / `rect` / the u16 at +50; palette 4th byte; WMT byte +1; CT header +3, +4, +6; CT file `.CCT` copy; CPU `COM.TXT` names and the byte at 0x08 / step byte +9; the whole pattern-name tail (decrypted, never read); `hdr+8 / +0xC`.
* Unproven (U): IF type 6 / 7 flag bit meanings beyond the quoted shape, IF 23's global, `blendMode == 2`, `flags2` bits 2 and 31, AT `flagsA` bits 0x20 / 0x80 and `flagsB` 0x40, `Pb2CharTableEntry.homeStage` meaning of the CSS path, runtime fields tagged U in the header (`techLatch`, `evadeTimer`, `decor*`, `cameraExcludeFlag`, `selfPtr`), the order dependence of the CPU-script overrun, and the exact blend meaning of `drawModeFlags`.

## 15. IDB changes (`pb2k1.exe.i64`)

Types: `docs/formats/ida/pb2k1_types.h` loaded with `parse_decls` (30 structs, 26 enums; the flag enums are bitmask). Applied to `g_fighter_slots` (`Pb2FighterSlot[4]`), `g_object_slots` (`Pb2ObjectSlot[1000]`), `g_EffectDatSlot` 0x15ED108, `g_CharTable` (`Pb2CharTableEntry[100]`),
`g_CharPatternData` / `g_CharSpriteBank` (pointer arrays), `g_WinQuoteRecords`, `g_CpuScriptData` (`Pb2CpfFile[4]`), `g_DataArchiveSlots` / `g_BgmArchiveSlots` (`Pb2ArchiveHandle`), `g_EvadeCommandTemplateA/B`, and the lookup tables
(`g_IfTypePhaseTable`, `g_DrawModeTransformTable`, `g_InputDirMirrorTable`, `g_AtKindTargetsObjectsOnly`, `g_AtKindImmunityChannel`, `g_AtClassVsInvulnHitTable`, `g_HitReactionActionByStance`, `g_HitReactionClassByStance`, `g_HitSparkTable`,
`g_RecoveryTimeByHitCount`, `g_HitstopFramesByLevel`, `g_HitVectorTable`, `g_EvadeVectorTable`, the difficulty / handicap scale tables). About 135 `sub_*` functions were renamed behaviour-based and given `Pb2Object *` / `Pb2FighterSlot *` / `Pb2EfRecord *` prototypes:
`PB_Obj*` (action script, frame pointers, motion, boxes, hit passes, hit reaction, knockback, grabs, input dispatch, evade / tech, reserve / guard gauge), `PB_Slot*` (command table / sequence matching / CPU script loading), `PB_EfType*_*` (EF handlers), `PB_Damage_*`, `PB_Cpu*`,
`PB_SetupFighterSlot*`, `PB_InitFighterSlotForRound`, `PB_ResetFighterSlotKeepingDataPointers`, `PB_BindCharacterData`, `PB_LoadWinQuoteTable`, `PB_PickWinQuote`, `PB_LoadVectorTxt`, `CSS_LoadCharselCtGrid`, `PB_LoadArchiveFileToBuffer`,
`PB_BlitPalettedToSurface`, `PB_Create/Lock/UnlockTextureSurface`; the cipher / loader functions carry function comments.
