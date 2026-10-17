# Melty Blood (Dec 2002, mb.exe): archives, character container, satellite files and runtime character state

Scope: the French-Bread / Watanabe Seisakujo fighter *Melty Blood* v1.0 (Dec 2002), 32-bit `mb.exe` (IDB `C:\dev\ida\server\mb.exe.i64`), game files
`C:\games\MB\MeltyBlood\data00.p .. data03.p`. Every statement was traced in the IDB and then checked against the shipped data with small scripts
(`fbarctool.exe ls/extract` for single entries, the GOF1 stage-2 cipher, `tools/gof1/gof1_dat.py` adapted to MB; numbers below are measured, not copied).
IDA types: `docs/formats/ida/mb_types.h` (40 structs, 11 enums, applied in the IDB, every offset checked against IDA's own member offsets and the exact
byte sizes `MbActor` 672, `MbFighterSlot` 6196, `MbObjectSlot` 676, `MbCtFile` 4496, `MbCpfFile` 193388, `MbCharSelFile` 2180, `MbWmtRecord` 156).
The container half is the GOF1 format (`docs/formats/gof1.md`, `src/framedata_gof1.cpp` round-trips all 18 MB characters byte-exact): this document lists only
what differs and everything GOF1 did not have (CG blob, 4 satellite formats, the MB runtime structs).

Evidence tags (same as gof1.md): **T** = reader/writer traced in mb.exe, **I** = traced but meaning inferred from code shape and data, **E** = data-bearing in the
files but no reader in mb.exe (editor-only), **U** = no access found anywhere (typed Hex-Rays ctree scan of all 763 gameplay-range functions plus a raw pointer scan of
the slot/pool iterator functions; the field is named `unused_<hexoff>`), **V** = validated by the loader. Addresses are VAs; function names are the IDB names after this
work (170 `sub_*` were renamed, 139 prototypes carry `Mb*` types, 38 globals are typed).

Lineage: MB is GOF1's data model with an extended runtime. The 0x444 header, the 256-entry pattern table, the 116-byte frame record and the AT/IF/EF/box tables are
byte-identical to GOF1; MB adds a CG blob (GOF1 never stores one), a CPU-AI file with a much larger script format, a 92-byte CT header and 44-byte commands, a 156-byte WMT,
and a 672-byte actor (GOF1: 656) whose layout is a reordered superset (appendix A maps every GOF1 field).

## 1. Archives (`dataNN.p`, "PAC v0")

Opened by `MainGameLoop` 0x439C10 through `LoadArchiveAndDecryptIndex` 0x424F90 (data03, data02, data01 into `g_PkArchives[7..9]`, data00 into `g_ArchiveData00` 0x16D1AC8 which
nothing ever reads again). Lookup `PKArchive_FindEntryIndex` 0x425110 (base name of the requested path, upper-cased by `Str_CopyUppercase`, `_strcmpi` over the index, first match wins);
`DataArchives_LoadEntry` 0x42E6A0 / `DataArchives_LoadEntryEx` 0x42E880 try `g_PkArchives[0..9]` in order and use the first slot whose entry size is non-zero (slots 0..6 are
empty), so the search order is data03, data02, data01. Existence test `Find_File_In_Archives` 0x42E660. Names are matched case-insensitively and without directory: the game's
`.\data\AKIHA_c.txt` becomes `AKIHA_C.CT` because `Slot_LoadCommandCT` / `CSS_LoadCharselTxtGridWithUnlocks` replace the extension with `.ct` (string at 0x46C040) before the lookup.

| off | type | meaning | evidence |
|---|---|---|---|
| 0x00 | u32 | `plainFlag`: 0 = payload enciphered, non-zero = plain (0 in data01/02/03; non-zero in data00, whose 34 entries are plain MP3 BGM files) | T: `PKArchive_ReadEntry` returns before the cipher when `handle+12 != 0` |
| 0x04 | u32 | `count ^ 0xE3DF59AC` | T |
| 0x08 | 68 x count | index entries | T |
| 8 + 68*count | | data, entries contiguous in index order; the decoded sizes must sum to <= file size | T: the loader returns `File_GetSize >= sum` |

Index entry (68 bytes, `MbArchiveEntry`), entry index `i`: `name[60]` where byte `j` (0..58) is `^= (3*j*i + 61) & 0xFF` (byte 59 and the NUL padding are plain), `u32 offset` at +0x3C (plain,
absolute), `u32 size ^ 0xE3DF59AC` at +0x40. Payload cipher (`PKArchive_ReadEntry` 0x425210): only the first `min(size, 0x2173)` bytes of an entry, and only when the read starts below 0x2173:
`data[p] ^= (p + key[p % len(key)] + 3) & 0xFF` with `key` = upper-cased base name (ASCII only; identical to `fbarctool`'s "name-keyed cipher over the first 0x2173 bytes").
Runtime handle `MbArchiveHandle` (28 bytes): `hFile, hMapping, view` (mapping created, never read), `plainFlag`, `entryCount`, `unused_14`, `entries`.

Contents (`fbarctool.exe ls`): data03.p = 76 entries, 139 MB: 18 `.DAT`, 20 `.CPF`, 18 `.WMT`, 20 command tables; data01.p = 1432 entries (346 `.EX3` images, 1029 `.WAV`, 54 `.TXT`, 2 `.FNT`, `CHARSEL.CT`, `VECTOR.TXT`); data02.p = 238 entries (154 `.EX3`, 25 `.WAV`, 59 `.TXT`); data00.p = 34 entries (30 MP3 + 4 numbered files), plain payload. The 18 data03 characters: AKAAKIHA, AKIHA, ARC, CIEL, EFFECT, EFFECT_, GAKIHA, HISUI, KOHAKU, M_HISUI, MIYAKO,
NANAYA, NERO, SHIKI, SION, V_SION, WARAKIA, WARC (EFFECT = effect-object patterns bound by `Load_Effect_DAT`, EFFECT_ is never loaded: the code builds only `.\data\effect.dat`).

## 2. Loader call chain (what the game reads, in order)

Character selection data is read once at the character-select scene: `CSS_LoadCharselTxtGridWithUnlocks` 0x42F6B0 (from `CharSelectScene_Update` 0x403760 and `MenuControllerMain` 0x44BE20)
fills `g_CharTable[100]` (13600 bytes, 0x181FB88), `g_CharFileIdToTableIndex[100]`, `g_CssCharGrid[100]` and `g_CharTableCount` (section 6.1).

Per match, the character files are loaded by one of three paths that share the same primitives:

1. **Versus / arcade / story (normal)**: `VsDemo_MatchLoaderThread` 0x410960 (a worker thread started by the VS scene, `Sleep(50)` between loads) calls, for each side `s`:
   `SpriteDataSlot_LoadCharacterDat(s, ".\data\<datName>.dat")` 0x42ED30 and `SpriteDataSlot_LoadPatternOverrideDof(s, "...dof")` 0x42EC50; when the third CHARSEL string names a partner
   (neither `"0"` nor `"1"`) and `Find_File_In_Archives` succeeds, the same two calls for data slot `s + 2` (`g_PlayerSlotRecords[s].partnerPresent = 1`). The main thread then runs
   `Battle_BindPreloadedCharacterSlots` 0x404400 (from `VsDemoScene_Update` 0x410CE0): `SpriteDataSlot_UploadPartsAndCG(slot, paletteIndex)` 0x42F0C0, `Slot_BindCharacterData(slotStruct, slot)` 0x42F670,
   `Slot_LoadCommandCT(slotStruct, ".\data\<datName>_c.txt")` 0x429200, `Slot_InitPointCharacterForRound` 0x437760, `CPU_LoadCpfForSlot(slot)` 0x404770, then the same for the partner. The VS scene also calls
   `Battle_LoadFaceCutinsAndVoices` 0x402A20 (face bitmap, five cut-in bitmaps, 100 voice WAVs, section 6.6).
2. **Replay playback** (`Replay_LoadCharactersForPlayback` 0x44D870): `Battle_LoadAllCharacterSlots` 0x404070 does the whole sequence synchronously per side:
   `SpriteDataSlot_LoadCharacterDatAndUpload(side, "<datName>.dat", paletteIndex)` 0x42EC20 (= `LoadCharacterDat` + `UploadPartsAndCG`), `Slot_BindCharacterData`, `Slot_LoadCommandCT`,
   `Slot_InitPointCharacterForRound`, `CPU_LoadCpfForSlot`, stores `characterId = charFileId`, `paletteIndex`, `slotIndex`, `controlType = (a5 == 1)`, then the partner with `LoadCharacterDat` +
   `LoadPatternOverrideDof` followed by a second full `LoadCharacterDatAndUpload` of the same `.dat` (so in this path the `.dof` override is overwritten again).
3. **Effect objects**: `Load_Effect_DAT` 0x42EBC0 (from `Battle_LoadSystemGraphics` 0x438AE0 and `Sys_HandleF1ToMenuAndPauseRequest` 0x431C10) loads `.\data\effect.dat` into data slot 4 and binds it to
   `g_EffectCharSlot` (0x16A01A0, a `MbFighterSlot`, team 255); EF type 8 children (`SpawnChildObjectFromFrameEffect` 0x416F30) take `charData/partsData/cgData` from it, EF type 1 children inherit the
   parent's (`SpawnChildObjectInheritingCharData` 0x416EB0).

Common primitives:

* `SpriteDataSlot_LoadCharacterDat(dataSlot, path)` (0x42ED30): `DataArchives_LoadEntry(path, &g_CharDatLoadBuffer)` (the whole entry, stage-1 cipher already undone by `PKArchive_ReadEntry`); frees the old
  `g_CharPatternData/g_CharPartsData/g_CharCgData[dataSlot]`; `Crypto_XorWithKeyString(buf, 0x444, 0x444, g_HeaderKey)` (0x46C660, "Memory" + CP932); `size = header.patternAreaEnd`, **or `header.cgOffset` when it is 0**;
  `GlobalAlloc(size)`, copy `[0, size)` into `g_CharPatternData[dataSlot]` and decrypt `[0x444, size)` with the pattern key (0x46C638); if `partsSize != 0`: `GlobalAlloc(partsSize + 0x4000)`, copy
  `[patternAreaEnd, +partsSize)` into `g_CharPartsData[dataSlot]`, decrypt with the blob key (0x46C680); if `cgSize != 0`: the same with `[cgOffset, +cgSize)` into `g_CharCgData[dataSlot]`. **The version dword (18) is never compared**, the
  signature is never compared, the 0x4000-byte pattern-name tail is never copied.
* `SpriteDataSlot_LoadPatternOverrideDof(dataSlot, path)` (0x42EC50): optional `<datName>.dof` (extension replaced from the `.dat` path). If present it must be a character container: header decrypted, `[0, header.patternAreaEnd)` copied and decrypted
  exactly as above, and it **replaces `g_CharPatternData[dataSlot]`** (parts and CG stay from the `.dat`). If the entry does not exist the call returns 1 and does nothing. No `.dof` exists in any shipped archive.
* `SpriteDataSlot_UploadPartsAndCG(dataSlot, paletteIndex)` (0x42F0C0): section 4 (parts textures) and section 5 (CG strips). Fails with a message box when pattern data or both blobs are missing.
* `Slot_BindCharacterData(slot, dataSlot)` (0x42F670): `slot->active = 1`, `actor.charData = g_CharPatternData[dataSlot]`, `actor.partsData = g_CharPartsData[dataSlot]`, `actor.cgData = g_CharCgData[dataSlot]`.
  Per-frame pointer resolution is `Actor_ResolveFrameDataPointers` 0x425C50: `patternHeader = charData + patternOffset[pattern]`, `frameRecord/animFrame = patternHeader + 20 + 116*frame`, `stateFrame = frameRecord + 0x28`,
  `boxIndex = frameRecord + 0x60`, `boxTable/ifTable/efTable = patternHeader + {box,if,ef}TableOffset` (0 when -1; MB has no range check, GOF1 had error boxes).
* `CharData_FreeAll` 0x42F620 frees all 20 entries of the three pointer arrays (`g_CharPatternData`, `g_CharPartsData`, `g_CharCgData`, 0x16D1830 / 0x189557C / 0x17DC690, 20 entries each; slots 0..3 fighters, 4 = EFFECT).

Data-slot layout: slot 0/1 = main fighter of side 0/1, slot 2/3 = their partner (`side + 2`), slot 4 = EFFECT. Each data slot owns 200 texture surfaces: surface id = `200*dataSlot + 2 + n`.

What the game never reads: container header signature, version, `unused_08`, `unused_24`; the pattern-name tail (last 0x4000 bytes); `frameSizeTag`; the CG title stub, the three CG header zero runs and the `0x80` byte, the image `sourceName`, `bound` and the garbage word at +0x32, the palette flag byte, `pieceCount` / `imageDataSize` (the strip table is addressed by image headers only); every `unused_*` field in `mb_types.h`.

## 3. Character `.DAT` container (deltas to gof1.md section 3)

Layout, ciphers and keys are exactly GOF1's: header `[0,0x444)` with key `0x46C660`, pattern area `[0x444, patternAreaEnd)` with key `0x46C638`, parts blob and CG blob with key `0x46C680`
(`buf[p] ^= (p + key[p % strlen(key)]) & 0xFF`, `p` relative to the section start; `Crypto_XorWithKeyString` 0x424D40), file size = `cgOffset + cgSize + 0x4000`.

| item | GOF1 | MB |
|---|---|---|
| version dword (+0x10) | 18, **checked** (`!= 18` fails the load) | 18 in all 18 files, **never checked** |
| `cgSize` (+0x20) | 0 in all files | **non-zero in all 18** (2.7 MB MIYAKO .. 12.4 MB WARC; `0xB08398` = 11,568,024 is AKAAKIHA's) |
| `partsSize` (+0x18) | non-zero in all | 0 in 9 files (AKAAKIHA, AKIHA, EFFECT, EFFECT_, NANAYA, NERO, SHIKI, SION, V_SION), non-zero in the other 9 |
| `patternAreaEnd == 0` | invalid | tolerated: the loader falls back to `cgOffset` |
| patterns per file | 15..47 | 16..173 (1985 patterns, 31,662 frames over the 18 files) |
| frame record | 116 B | 116 B, byte-identical layout (`MbFrameRecord`); the AF/AS unused ranges are zero in all 31,662 frames except AS +0x04..0x07, which holds data in 16 CIEL frames and is never read (`unreadData_04`) |
| AT record | 26 B, bytes +0x0C..+0x0E and +0x14..+0x19 unused | 26 B, **+0x0C is a data-bearing s16 `heatGaugeGain` and +0x14 a u16 `statusEffectParam`** (see below) |
| box slots | slot 9 never used | slots 8 **and 9** are two attack boxes (slot 9 present in 434 frames); slot 7 is never present |
| pattern name tail | editor only | editor only, never copied by the loader |

`bytes 8..0x0F` and `0x24..0x43` of the header are zero in all files; the 256-entry pattern table is ascending from 0x444 with -1 for absent patterns, the last pattern ends exactly at `patternAreaEnd` (verified by the adapted
`gof1_dat.py` on all 18 files: 0 violations apart from the three MB-specific facts: the AT bytes +0x0C / +0x14, box slot 9, and the 16 CIEL frames above).

Measured data (decrypted, `strips` = 14-byte CG strip records, `images` = CG images):

| file | bytes | patterns | frames | boxes | AT | IF | EF | partsSize | parts patterns | cgSize | CG images | CG strips | CG mode |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AKAAKIHA.DAT | 11982626 | 159 | 2956 | 3534 | 225 | 296 | 432 | 0 | 0 | 11568024 | 1210 | 11462 | 8bpp |
| AKIHA.DAT | 11877982 | 154 | 2874 | 3511 | 207 | 289 | 423 | 0 | 0 | 11474020 | 1180 | 11332 | 8bpp |
| ARC.DAT | 11806184 | 173 | 2840 | 3816 | 101 | 382 | 510 | 2479884 | 91 | 8921874 | 1098 | 10241 | 8bpp |
| CIEL.DAT | 7295488 | 145 | 2764 | 3942 | 116 | 535 | 632 | 1111148 | 4 | 5781168 | 1005 | 7160 | 8bpp |
| EFFECT.DAT | 8465956 | 54 | 742 | 0 | 0 | 96 | 81 | 0 | 0 | 8357020 | 354 | 2372 | 24bpp |
| EFFECT_.DAT | 1570580 | 60 | 558 | 0 | 0 | 102 | 124 | 0 | 0 | 1481840 | 200 | 1478 | 8bpp |
| GAKIHA.DAT | 3041340 | 16 | 106 | 104 | 23 | 6 | 22 | 2281164 | 37 | 728046 | 45 | 545 | 8bpp |
| HISUI.DAT | 9308736 | 143 | 2248 | 3022 | 156 | 429 | 449 | 3425420 | 63 | 5552988 | 895 | 6758 | 8bpp |
| KOHAKU.DAT | 7470376 | 147 | 2373 | 3020 | 75 | 221 | 466 | 1166348 | 19 | 5966726 | 936 | 7011 | 8bpp |
| M_HISUI.DAT | 7124744 | 125 | 2101 | 2627 | 93 | 333 | 306 | 1125868 | 8 | 5696306 | 951 | 6419 | 8bpp |
| MIYAKO.DAT | 3978108 | 87 | 1028 | 1712 | 31 | 55 | 242 | 1103788 | 2 | 2714974 | 468 | 3407 | 8bpp |
| NANAYA.DAT | 5291058 | 104 | 1575 | 2451 | 93 | 152 | 318 | 0 | 0 | 5056160 | 787 | 6396 | 8bpp |
| NERO.DAT | 4058268 | 60 | 896 | 1128 | 24 | 51 | 101 | 0 | 0 | 3922560 | 431 | 4260 | 8bpp |
| SHIKI.DAT | 5214490 | 104 | 1585 | 2450 | 93 | 153 | 317 | 0 | 0 | 4978432 | 773 | 6272 | 8bpp |
| SION.DAT | 6145894 | 100 | 1561 | 2585 | 86 | 109 | 348 | 0 | 0 | 5912414 | 839 | 7405 | 8bpp |
| V_SION.DAT | 6146862 | 101 | 1568 | 2586 | 86 | 110 | 353 | 0 | 0 | 5912414 | 839 | 7405 | 8bpp |
| WARAKIA.DAT | 10183030 | 94 | 1231 | 1743 | 50 | 128 | 277 | 1103788 | 2 | 8892722 | 592 | 7789 | 8bpp |
| WARC.DAT | 13851750 | 159 | 2656 | 3705 | 82 | 329 | 484 | 1103788 | 2 | 12368546 | 1130 | 10937 | 8bpp |


**AT record, MB-specific fields** (`MbAtRecord`; everything else as GOF1 `Gof1AtRecord`):

* `+0x0C s16 heatGaugeGain` (T): `Battle_ResolveHit` 0x43FF40 passes it to `Actor_AddHeatGauge` 0x443320 (`victim, attacker, gain, guardState`); the **victim** gains the gauge when the hit is not guarded and the heat state is idle.
  Values 0..150 (0 in 608 of 1541 records). GOF1 had `unused_0C[3]` here.
* `+0x14 u16 statusEffectParam` (T): third argument of `Character_ApplyAddedEffect` 0x435B00; only effect mode 4 stores it (into `actor.statusEffectParam`, counted down by `Actor_TickWordTimers`); 444 in 16 records, 0 elsewhere.
* `+0x0F statusEffect` is non-zero in MB data (1 in 416 records, 3 in 10, 4 in 16): `Character_ApplyAddedEffect` modes 1..3 set `tintMode` and a 120-frame `tintTimer`, mode 4 sets `tintMode = 4` and `statusEffectParam`.
* `+0x12 hitClass` is 0 in all 1541 MB records (the `AtClass_HitsInvulnClass` 0x43EE50 table `g_AtClassVsInvulnHitTable[5][5]` at 0x46CCEC is still indexed with it and with `(frame.flags2 >> 16) & 0xF`).
* `+0x0E` (`unused_0E`) and `+0x16..0x19` (`unused_16[4]`) are zero in all records and never read.

Pattern header: `moveInfo` bit 0x80 (double-resolution CG, `Pattern_IsCgDoubleRes` 0x427AF0) and bit 0x40 (linear filter, `Pattern_GetLinearFilterBit` 0x427B10) are read; `moveLevel` (`Pattern_GetMoveLevel` 0x427AD0, used by
`Actor_RequestLocomotionFromInput` 0x428180) is 0 in all 1985 patterns; the low nibble of `moveInfo` (value 2 in 4 patterns, 0 elsewhere) has no reader in MB (GOF1's `Pattern_GetMoveType` does not exist here).
Frame record fields in MB data: `drawMode` 0/1/2/4, `blendMode` 0..3 (0 opaque 22,711, 1 alpha 1,327, 2 7,226, 3 398), `aniFlag` 0/1/2/5, `stance` 0/1/2, cancel permissions 0/1/2.

**Box slots** (T, `Actor_BoxKindOverlapsActor` 0x43E2D0, `Actor_FindOverlappingTarget` 0x43E570, `Actor_AttackBoxHitsTeammateHurtbox` 0x42B970, `Hitbox_Detect*` 0x43EE70 / 0x43F210):
`boxIndex[0]` push box, `[1..3]` hurt boxes, `[4]` probe/grab box (IF 19/20, IF 100, EF op 13/14), `[5]` second special box, `[6]` reflect/clash box, `[7]` projectile hurt box (never present in the data), `[8]` and `[9]` attack boxes
(two independent attack boxes in MB; measured slot usage over 31,662 frames: 0 push 18,312, 1 16,696, 2 3,695, 3 235, 4 571, 5 35, 6 492, 8 1,466, 9 434).

**IF types** (`Actor_RunFrameIfRecord` 0x429F00; the dispatch is `switch(type)`; same numbering and parameter meaning as gof1.md section 3.6 for 1..32 unless noted): present in the data 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 20, 21, 22, 24, 25, 26, 32, 33, 100
(counts over all files: 2 = 1000, 14 = 528, 3 = 375, 11 = 254, 26 = 253, 6 = 250, 1 = 218, 16 = 169, 12 = 163, 8 = 151, 13 = 134, 25 = 74, 7 = 51, 5 = 46, 21 = 33, 4 = 32, 33 = 18, 20 = 12, 22 = 7, 100 = 3). Handlers also exist for 15, 18, 19, 23, 27..31 and 50.
MB-specific: **0x21 (33) VOICE_FINISHED_JUMP** (param[1] = voice number or 255 = any; jumps to param[0] when `SoundSlot_IsVoicePlaying(i + 100*(slotIndex+4))` is false), **0x64 (100) PARTNER_BOX_CONTACT_JUMP**
(partner call: when `Actor_BoxKindOverlapsActor(actor, teamPartnerActor, 6)` both actors jump to param[0] and `g_PlayerSlotRecords[team].partnerAiState` is incremented), 0x20 CPU_PLAYER_JUMP also tests `g_GameMode == 10`, 0x32 ALLY_BOX_JUMP
(`Actor_AttackBoxHitsTeammateHurtbox`). Gating: IF 1 and 11 are skipped while `actor.inputLock != 0`; IF 6/7 select the actor's owner for child objects (`objKind & 0x80`).

**EF types** (`Character_RunFrameEFs` 0x415910, once per frame entry): 1 `SpawnChildObjectInheritingCharData` 0x416EB0 (child shares the parent's data), 2 `EfType2_SpawnEffectById` 0x417390, 3 `EfType3_SpawnParticlesById` 0x417320, 4 `EF04_PositionGrabbedTarget` 0x41EF80,
5 `EF05_DamageGrabbedVictim` 0x41F360, 6 `EfType6_ActorOp` 0x41F6E0, 7 `EfType7_SpawnSystemParticle` 0x417400, 8 `SpawnChildObjectFromFrameEffect` 0x416F30 (child uses EFFECT.DAT), 9 `EF09_PlaySound` 0x4202E0, 0x1E `EfType30_SetActorParams` 0x420280,
0x32 `SpawnChildEffectFromParent` 0x4204D0. Present in the data: 1, 2, 3, 4, 5, 6, 8, 9. Type 6 sub-ops (`ef.subType`): 0 trail colour (255 = off), 1 screen shake + freeze (`g_ScreenShakeTimer`), 2 `freezeTimerA/B`, 3 afterimage config, 4 add life/meter/gauge,
5 facing ops (0..5), 6 set velocity range, 7 spawn frame ghost (`EffectObject_SpawnGhostCopy` 0x416FB0), 8 aim/spawn loop over the 4 players, 9 `inputLock = arg0`, 10 `scriptTimer[arg0] = arg1`, 13/14 box-contact variants, 15 BGM volume, and through the byte table at 0x4201D8:
100 add to a `tobiCount` / `g_GlobalCounterBank` counter, 101 subtract (clamped at 0), 102 `dashCount += arg0`, 103 `dashCount -= arg0`, 105 `paramVar[arg0] = / += arg1`, 150 request pattern `arg0` on the team partner (priority 300), 252 `koFlag = arg0`,
253 `++g_KoState` (force round end), 254 `introFinished = 1`, 255 `invulnFlags |= 0x10`. Sub-ops seen in the data: 0, 1, 3, 4, 5, 6, 7, 8, 10, 13, 14, 15, 100, 101, 102, 105, 150, 254, 255.

**Parts blob** = old PAT v2, byte-identical to gof1.md section 4 in all 9 MB files that have one (header `(2, 0x01234567)`, 1000 pattern offsets from 0x8CBC and 3680 B apart, texture region right after the last pattern with `edge = 512` rasters,
all `unused` bytes zero in all 9,120 parts; patterns: ARC 91, CIEL 4, GAKIHA 37, HISUI 63, KOHAKU 19, M_HISUI 8, MIYAKO 2, WARAKIA 2, WARC 2). `SpriteDataSlot_UploadPartsAndCG` consumes it as follows (T): for each of the 50 texture slots `j` with a non-zero
`textureOffset[j]`: surface `200*dataSlot + j + 2` is created `edge x edge` (first with creation flag `0x20000000`, then 0 on failure; `Surface_CreateTextureSlot`) and filled by `Surface_UploadA8R8G8B8Square` 0x42EEE0 (A8R8G8B8 -> the 16-bit surface format; a 32-bit surface is memcpy'd); then for all 1000
pattern slots with a non-zero offset, all 40 parts get `part.textureSlot += 200*dataSlot + 2` **in the loaded copy**. Let `n` = number of parts textures actually present.

## 4. CG blob (MB only)

`g_CharCgData[dataSlot]` points at the decrypted blob (`cgSize` bytes). It is read by `SpriteDataSlot_UploadPartsAndCG` (upload, once per load) and by `Draw_ActorSpriteScratch` 0x4333B0 (draw, every sprite with `spriteId >= 10000`, image id `spriteId - 10000`).

| off | size | field (`MbCgBlobHead`) | meaning | evidence |
|---|---|---|---|---|
| 0x0000 | 6 | `titleStub` | CP932 bytes 82 C9 82 E5 81 48 in all 18 files | E |
| 0x0006 | 10 | `unused_06` | zero | U |
| 0x0010 | 1 | `trueColorFlag` | **0xFF = images are 3-byte BGR rasters (EFFECT.DAT only), anything else (0) = 8-bit indexed** | T: `v16[16] == -1` selects the path |
| 0x0011 | 2 | `unused_11` | zero | U |
| 0x0013 | 1 | `unused_13` | 0x80 in all files | U |
| 0x0014 | 12000 | `imageOffset[3000]` | s32 blob-relative offset of image `i`, -1 = absent (images present: 45 (GAKIHA) .. 1210 (AKAAKIHA), ids contiguous from 0 in every file) | T |
| 0x2EF4 | 8192 | `palette[8][256]` | 8 palette sets (the character colour, `paletteIndex` 0..7) of 256 4-byte entries `{r, g, b, flag}`; flag is 1 in all entries and never read; also present (unused) in the 24-bit file | T |
| 0x4EF4 | 4 | `pieceCount` | number of 14-byte strip records (E: never read) | E |
| 0x4EF8 | 4 | `imageDataSize` | bytes from the first image header to the end of the blob (E) | E |
| 0x4EFC | 14 x pieceCount | `MbCgPiece[]` | strip table, **starts at 20220 = 0x4EFC** (not 20224: the two dwords above precede it) | T: `20220 + 14*first` in the draw path |
| first image | | images | contiguous, in id order; the first image starts exactly at `0x4EFC + 14*pieceCount` | verified in all 18 files |

Image (`MbCgImageHeader`, 52 bytes) followed by the raw pixels of its strips (`w * h` bytes per strip for 8-bit, `3 * w * h` for 24-bit, concatenated in strip order; the pixel size of an image is the sum over its strips and the next image starts right after, the last
image ends exactly at `cgSize`; verified for all 18 files):

| off | type | field | meaning | evidence |
|---|---|---|---|---|
| 0x00 | char[36] | `sourceName` | editor source bitmap, e.g. `AKI00_000.BMP`, NUL terminated, the rest is uninitialised editor memory | E (all 18 files: every name ends in `.BMP`) |
| 0x24 | s16[4] | `bound` | x1, y1, x2, y2: inclusive bounding box of the strips | E |
| 0x2C | u32 | `firstPiece` | index of the image's first strip; consecutive images have consecutive ranges (verified) | T |
| 0x30 | u16 | `pieceCount` | strip count (max 105 in EFFECT_, 79 in NANAYA, <= 57 elsewhere; the upload path reads it as a signed byte, so a value above 127 would break it) | T |
| 0x32 | u16 | `unusedGarbage_32` | uninitialised editor memory in a few images | E |

Strip (`MbCgPiece`, 14 bytes; all strips in the data are **16 rows high**, 1..256 wide):

| off | type | field | meaning |
|---|---|---|---|
| 0x00 | s16 | `x` | destination x inside the image canvas |
| 0x02 | s16 | `y` | destination y (strip top) |
| 0x04 | s16 | `width` | 1..256 |
| 0x06 | s16 | `height` | 16 in every strip of the data |
| 0x08 | s16 | `u` | source x in the 256x256 texture (`u + width <= 256`) |
| 0x0A | s16 | `v` | source y, a multiple of 16 (`v + 16 <= 256`) |
| 0x0C | s16 | `textureSlot` | texture index; non-decreasing along the table and contiguous from 0 (AKIHA 0..171, WARC 0..185); the upload rewrites it to the surface id |

Upload (T, `SpriteDataSlot_UploadPartsAndCG`): `n` = parts textures present; for strip `k` (table order) the surface is `s = 200*dataSlot + 2 + n + textureSlot`; whenever `textureSlot` differs from the previous strip's, a new **256x256** surface is created and cleared to black
(`Surface_CreateTextureSlot(s, 256, 256, 0)` for 8-bit, `Surface_CreateTextureSlotNoFlags(s, 256, 256)` for 24-bit), so the slot order must be monotonic (it is). 8-bit path: the strip's `w*h` bytes are converted through `palette[paletteIndex]` (entry 0 forced to 0, i.e. **index 0 = transparent**;
`Surface_BlitIndexedBitmapToSlot` skips index 0 with colour-key mode 1, the red/green/blue bytes index the surface's per-channel conversion tables built from the DirectDraw pixel-format masks (`Surface_BuildChannelConversionTables`), any non-zero 16-bit result additionally gets the alpha bit `dword_759180`);
24-bit path: bytes are B, G, R, black = transparent (same tables, alpha bit when non-zero); the strip's `textureSlot` field is then overwritten with `s`. `paletteIndex` is not range-checked (it is the CSS colour 0..7). The strips of one 256x256 texture are therefore packed as 16-row bands with per-band x extents (the first AKIHA image is 7 strips `(112,112,32x16)`, `(96,128,64x16)` ... at `u/v` (0,0), (32,0), ...).

Draw (T, `Draw_ActorSpriteScratch`, `spriteId >= 10000`): `image = cg + imageOffset[spriteId - 10000]` (-1 draws nothing), strips from `piece[firstPiece ...]`; the canvas origin is **(128, 224)** for single-resolution patterns and (256, 448) with all coordinates halved when `moveInfo & 0x80` (`Pattern_IsCgDoubleRes`);
a strip is the quad `(frame.offsetX + x - originX, frame.offsetY + y - originY, width, height)` relative to the actor, textured from `(u, v)` of surface `textureSlot`, mirrored for `facingLeft` (`canvas width - x - width`), with the frame's zoom / blend / alpha / rotation applied to the whole image.
`spriteId < 10000` draws a parts pattern from the parts blob instead (section 3).

## 5. Satellite files

All names below are built from the CHARSEL entry of the character (`g_CharTable[g_CharFileIdToTableIndex[characterId]]`): `name` (+0x00) keys the face bitmap and the WMT, `datName` (+0x20) keys `.dat`, `_c.ct`, `.cpf` and the voices, and the partner
uses `partnerDatName` (+0x40) for the same files. Every satellite is read through `DataArchives_LoadEntry` (stage-1 cipher already undone); none has a magic number.

### 5.1 `CHARSEL.CT` (data01.p, 2180 bytes) - character select table (`MbCharSelFile`)

Loader `CSS_LoadCharselTxtGridWithUnlocks` 0x42F6B0 (the code asks for `CHARSEL.TXT`, `Path_ReplaceExtension(..., ".ct")` turns it into `CHARSEL.CT`): `u32 count` followed by `count * 136` bytes. The entry block is enciphered with
`Crypto_XorWithKeyString(block, 136*count, 136*count, key)` where `key` is the CP932 string `83 74 83 40 83 43 83 8B 82 AA 8C A9 82 C2 82 A9 82 E8 82 DC 82 B9 82 F1` ("file not found") at 0x46C6D4 (`block[p] ^= (p + key[p % 24]) & 0xFF`, `p` relative to the block;
the count dword is plain; verified: the decrypted names are readable). There is no bound check on `count` (the block is copied into a local buffer first; the table holds 100). Loader steps (T): `memset(g_CssCharGrid, 0xFF, 400)`, `memset(g_CharFileIdToTableIndex, 0xFF, 100)`, `memset(g_CharTable, 0, 0x3520)`;
for each entry `i`: `g_CharFileIdToTableIndex[charFileId] = i`, copy the 136 bytes to `g_CharTable[i]`, `row = gridPos / 10` (`+10` when `page == 1`), `col = gridPos % 10`; if `unlockMask == 0 || (unlockMask & g_UnlockMask) != 0` (`g_UnlockMask` 0x1895A80)
the entry is visible: `g_CssCharGrid[10*row + col] = i`, `entry.runtimeTableIndex = i`; otherwise (locked) it is **moved to the end of the table** (`g_CharTable[99 - k]`, `k` = number of locked entries so far), its old slot is zeroed,
`g_CharFileIdToTableIndex[charFileId] = 99 - k`, and the visible count is decremented. `g_CharTableCount` (0x16A011C) receives the final count.

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `char` | `name[32]` | T: face bitmap / display base name (".\grp\system\<name>_face.bmp") |
| 0x020 | `char` | `datName[32]` | T: data file base name (<datName>.dat, _c.ct, .cpf, .wmt) |
| 0x040 | `char` | `partnerDatName[32]` | T: second character file of a two-character team, "0" = none, "1" = special single-fighter marker |
| 0x060 | `unsigned int` | `runtimeTableIndex` | T: written by CSS_LoadCharselTxtGridWithUnlocks (file value 0) |
| 0x064 | `unsigned int` | `charFileId` | T: id used by g_CharFileIdToTableIndex, voice ids, statistics |
| 0x068 | `unsigned int` | `homeStageId` | T: stage chosen for the character (bg%02dinfo.txt) |
| 0x06C | `unsigned int` | `gridIconIndex` | T: CSS_DrawCharGridIcons: sheet (>>4) + 1670, column (&3), row ((>>2)&3) |
| 0x070 | `unsigned int` | `unlockMask` | T: tested against g_UnlockMask, 0 = always available |
| 0x074 | `unsigned int` | `page` | T: 1 = grid rows 10+ |
| 0x078 | `unsigned int` | `gridPos` | T: row * 10 + column |
| 0x07C | `unsigned int` | `portraitWidth` | T: CSS_DrawPanelPortraitAndInfo draws (width >> 1) |
| 0x080 | `unsigned int` | `portraitHeight` | T |
| 0x084 | `unsigned int` | `unused_84` | U: zero in all files, never read |

The 16 shipped entries (decrypted; `runtimeTableIndex` and `unused_84` are 0 in the file):

| # | `name` | `datName` | `partnerDatName` | `charFileId` | `homeStageId` | `gridIconIndex` | `unlockMask` | `page` | `gridPos` | `portraitWidth` x `portraitHeight` |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | SION | SION | 0 | 0 | 1 | 0 | 0 | 0 | 29 (r2 c9) | 120 x 96 |
| 1 | ARC | ARC | 0 | 1 | 2 | 1 | 0 | 0 | 39 (r3 c9) | 120 x 96 |
| 2 | CIEL | CIEL | 0 | 2 | 3 | 2 | 0 | 0 | 49 (r4 c9) | 120 x 96 |
| 3 | AKIHA | AKIHA | 0 | 3 | 4 | 3 | 0 | 0 | 59 (r5 c9) | 120 x 96 |
| 4 | HISUI&KOHAKU | HISUI | KOHAKU | 4 | 5 | 4 | 0 | 0 | 69 (r6 c9) | 120 x 96 |
| 5 | SHIKI | SHIKI | 0 | 7 | 6 | 7 | 0 | 0 | 79 (r7 c9) | 120 x 96 |
| 6 | MIYAKO | MIYAKO | 0 | 8 | 7 | 8 | 1 | 0 | 89 (r8 c9) | 120 x 96 |
| 7 | M_HISUI | M_HISUI | 0 | 14 | 8 | 14 | 1 | 0 | 68 (r6 c8) | 120 x 96 |
| 8 | KOHAKU | KOHAKU | 0 | 6 | 5 | 6 | 1 | 0 | 48 (r4 c8) | 120 x 96 |
| 9 | NERO | NERO | 0 | 10 | 9 | 10 | 4 | 0 | 48 (r4 c8) | 88 x 45 |
| 10 | WARAKIA | WARAKIA | 0 | 9 | 10 | 9 | 1 | 0 | 19 (r1 c9) | 106 x 63 |
| 11 | V_SION | V_SION | 0 | 11 | 11 | 11 | 1 | 0 | 28 (r2 c8) | 120 x 96 |
| 12 | WARC | WARC | 0 | 12 | 13 | 12 | 1 | 0 | 38 (r3 c8) | 120 x 96 |
| 13 | AKAAKIHA | AKAAKIHA | 0 | 13 | 11 | 13 | 1 | 0 | 58 (r5 c8) | 101 x 81 |
| 14 | NANAYA | NANAYA | 0 | 15 | 11 | 15 | 1 | 0 | 78 (r7 c8) | 120 x 96 |
| 15 | GAKIHA | GAKIHA | 1 | 16 | 14 | 16 | 4 | 0 | 77 (r7 c7) | 53 x 65 |

`portraitWidth/Height` = size of the CSS portrait bitmap (`CSS_DrawPanelPortraitAndInfo` 0x401B00 draws `(width >> 1) + 32`, `height >> 1`). `gridIconIndex` selects the 64x64 icon in sprite sheet `1670 + (idx >> 4)`, column `idx & 3`, row `(idx >> 2) & 3`
(`CSS_DrawCharGridIcons` 0x401A20). The `"1"` partner string (GAKIHA) sets `g_PlayerSlotRecords[team].bossFlag = 1` and loads no partner, `"0"` clears both partner flags, any other string loads that character as the team partner.

### 5.2 `<datName>_C.CT` - command table (`MbCtFile`, 4496 bytes)

Loader `Slot_LoadCommandCT` 0x429200: upper-cases the path, replaces the extension with `.ct`, `DataArchives_LoadEntry`, then `slot.commandCount = u32[0]`, `slot.command[100] = bytes[4, 4404)` (0x1130), `slot.ct = bytes[4404, 4496)` (0x5C), frees the buffer. **It returns 1 even when the file is missing** (the slot keeps a zero CT) and does
not check the file size. `Slot_InitPointCharacterForRound` then zeroes `sequence[16..31]` of the first `commandCount` commands, so only the first 16 tokens of a sequence are ever matched (the longest in the data has 9). The `count` dword is 21..48 in 14 of the 16 loaded files (GAKIHA 1, NERO 3); 444 commands are defined in total (`commandId == index`);
unused slots are `FF 00 ...` (43 zero bytes), `unused_01` and `unused_2B` are 0 in all of them.

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `commandId` | T: own index, 0xFF = unused slot |
| 0x001 | `unsigned char` | `unused_01` | U: zero in all files |
| 0x002 | `unsigned char` | `sequence[32]` | T: token string, 0xFF terminated: 0..9 direction, 0x41..0x46 buttons A..F, 0x2B '+' joiner, 0x54 'T' tap |
| 0x022 | `unsigned char` | `targetPattern` | T: pattern requested when the command fires |
| 0x023 | `MbCommandMoveClass` | `moveClass` | T |
| 0x024 | `unsigned __int16` | `meterCost` | T: super meter needed (10000 per level), copied to actor.activeMoveMeterCost |
| 0x026 | `unsigned char` | `requirePartner` | T: 1 = only usable when the team has a partner (g_TeamRecords.partnerPresent) |
| 0x027 | `unsigned char` | `counterRequirement` | T: >= 100: global counter bank; < 100: tens digit = tobiCount[] index, units digit = amount |
| 0x028 | `unsigned char` | `usageLimit` | T: refused once actor.dashCount reaches it (0 = unlimited) |
| 0x029 | `MbCommandFlags` | `flags` | T |
| 0x02A | `MbCommandFlags2` | `flags2` | T |
| 0x02B | `unsigned char` | `unused_2B` | U: zero in all files, never read |

Sequence tokens (T, `CommandSequence_ParseFirstStep` 0x4292B0 / `CommandSequence_ParseNextStep` 0x4293B0, matched backwards from the last token against the input ring by `Fighter_MatchCommandSequence` 0x4294C0): bytes 0..9 = numpad direction, 0x41..0x46 = buttons A..F (bit `1 << (c - 0x41)`),
0x2B `+` = joiner (direction and button of the same step), 0x54 `T` = tap, 0xFF = end. Measured: lengths 1..9 (4 tokens in 293 commands), moveClass 0 in 259 / 1 in 185 commands, `flags` 5 (ground+crouch) 220, 2 (air) 64, 37 (0x25) 59, 69 (0x45) 56, 13 17, 66 14, 34 6, 45 4, 7 4,
`flags2` 1 (ring B) 272, 0 113, 4 59, `meterCost` 0 (374), 10000 (42), 30000 (14), 12000/15000/29000/1000/20000/25000/500 (a few), `requirePartner` 1 in 8, `usageLimit` 1 in 33, `counterRequirement` 1/11/31/61/71/91.
`flags` (`MbCommandFlags`): 1/2/4 = allowed while the stance is ground/air/crouch (`stateFrame.stance` 0/1/2), 8 = no cancel entry (when set the command is not offered through the cancel tables), 0x10 = `actor.clearHistoryOnAction`, 0x20 = script-only (`Fighter_ScanCommandMoves`
skips it; IF 11/31 start it), 0x40 = guard cancel (usable while `guardState`). `flags2` (`MbCommandFlags2`): 1 = match against ring B (held | released), 2 = strict sequence, 4 = allowed while `g_BattleControlPhase == 1`, 0x40 = only startable as a heat cancel, 0x80 = heat cancel (startable out of a non-actionable frame after a hit
while `heatState == ACTIVE`, arms `heatCancelPending`). `Fighter_TryStartCommandMove` 0x429780 (T) checks, among others: slot used (`commandId != 0xFF`), `requirePartner` vs `partnerPresent`, `counterRequirement` (>= 100: `g_GlobalCounterBank[v/100] > v%100`; else `tobiCount[v%100/10] > v%10`),
the cancel rule of the running frame (when `stateFrame.canAct == 1`, or `flags & 8`, the move is allowed; otherwise `moveClass` 0 needs `normalCancel`, class 1 and 2 need `specialCancel`, permission 1 = after `attackResult != 0`, 2 = always; class 2 with `stateFrame.flags2 & 1` is allowed after `attackResult` / `ownerAttackResult` regardless; `guardState != 0` only allows `flags & 0x40`),
the stance bit of `flags`, `usageLimit` vs `dashCount`, `commandUsedMask` when `ct.flags & 1` and `moveClass == 0`, `meterCost <= superMeter`, `targetPattern != pattern`, then `ObjRequestAction(pattern, 0, 4200 - index, 0)`; on success it records `pendingCommandId`, `activeMoveMeterCost`, `clearHistoryOnAction` and `heatCancelPending`.

Header (`MbCtHeader`, 92 bytes at CT +0x1134; slot +0x2A4):

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `maxAirJumps` | T: Character_ProcessPlayerInput (inputHeldFlags & 0x7F counts the jumps used) |
| 0x001 | `unsigned char` | `unused_01` | U: 9 in all files, never read |
| 0x002 | `unsigned char` | `recoveryStyle` | T: ObjRunActionScript (0, 1 or 2) |
| 0x003 | `unsigned char` | `unused_03` | U |
| 0x004 | `unsigned char` | `unused_04` | U |
| 0x005 | `MbCtFlags` | `flags` | T |
| 0x006 | `unsigned char` | `unused_06[2]` | U |
| 0x008 | `float` | `knockbackScale` | E: 1.0 (GAKIHA 1.0); no reader found in MB |
| 0x00C | `float` | `damageScale[4]` | T: only [0] is read (Damage_ScaleByDifficultyAndTeam, Damage_ScaleByGuardGaugeAndStance); data 0.9 (1.0 in WARC, 0.4/0.3/0.3/0.1 in GAKIHA) |
| 0x01C | `MbCtEvadeEntry` | `evade[4]` | T: Fighter_DetectEvadeDoubleTap |

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned __int16` | `enabled` | T |
| 0x002 | `unsigned __int16` | `pattern` | T |
| 0x004 | `unsigned char` | `unused_04[8]` | U: zero in all files |
| 0x00C | `unsigned __int16` | `window` | T |
| 0x00E | `unsigned __int16` | `cooldown` | T |

All 16 files have `maxAirJumps = 1`, `unused_01 = 9`, `recoveryStyle = 1`, `flags = 0x39`, `knockbackScale = 1.0`, `damageScale = 0.9` (WARC 1.0; GAKIHA 0.4/0.3/0.3/0.1; AKAAKIHA 0.9/1.0/1.0/1.0). Evade entries: 26 of 64 enabled (`window` 7 or 8, `cooldown` 16), entries 0 and 3 are disabled everywhere that matters; M_HISUI, NERO and GAKIHA have none.
`ct.flags` bits: 1 = `MBCT_RESTRICT_REPEAT_MOVES` (locomotion patterns 1..9 and `moveClass` 0 commands cannot repeat inside a chain: `locomotionUsedMask`, `commandUsedMask`), 0x10 = `MBCT_GUARD_BY_INPUT_DIRECTION` (`Battle_ResolveHit` lets the held direction pick the guard type),
0x20 = `MBCT_JUMP_CANCEL` (`Character_ProcessPlayerInput` allows the jump-cancel patterns 35/36/37/41 after a connected attack), 0x08 has no reader.

**Orphans (not loadable by this build)**: `ARC_C.CT2`, `HIS_KOH_C.CT`, `MIYKO_C.CT` (4232 bytes each) and `HISKOH_C.CT` (4432) are older formats of the same table: `u32 count | 100 x 42-byte commands | 28-byte header` (4 + 4200 + 28 = 4232; the command is the MB record without `usageLimit` and the trailing byte, so `flags` is at +0x28 and
`flags2` at +0x29) for the first three, `u32 count | 100 x 44-byte commands | 28-byte header` (the 42-byte record zero padded) for `HISKOH_C.CT`; the 28-byte header is `MbCtHeader` without the evade table. No `.ct2` string exists in mb.exe, and `HISUI&KOHAKU` loads `HISUI_C.CT` and `KOHAKU_C.CT`.
Feeding them to `Slot_LoadCommandCT` would over-read (it always copies 4 + 4400 + 92 bytes from the buffer).

### 5.3 `<name>.WMT` - win message table (`MbWmtRecord`, `u32 count` + `count x 156`)

Loader `CharFile_LoadWmt` 0x40ED80 (called by `WinScreen_Update` 0x40F4D0 with the winner's character id): `memset(g_WmtTable, 0, 15600)`, `DataArchives_LoadEntry(".\data\<name>.wmt")` (**`name` = CHARSEL +0x00**, hence `HISUI&KOHAKU.WMT`), `g_WmtCount = u32[0]`,
`memcpy(g_WmtTable, bytes + 4, 156 * count)` (100 records max, no check), then it builds (and discards) a `_win.txt` name. `WinScreen_PickWmtEntry` 0x40EE50 collects the records whose `loserCharId` equals the loser's character id, else those with 0xFF, picks one at random and
re-rolls while `oneInN != 0 && rand() % oneInN != 0`; the win picture is `.\grp\win\win_<name><imageVariant:02d>.bmp` (falls back to `win_<name>.bmp` when `imageVariant == 0` or the file is missing) and the voice is `.\effect\win_voice\<name>_win<entryIndex:02d>.wav`.
Only `+0`, `+2`, `+4` are read; the other 150 bytes are zero in all 17 matching files (one stray `0x30` at +6 in the 3 V_SION records). The files hold 1..13 records; `HISKOH.WMT` (774 bytes) is an orphan in the 154-byte GOF1 layout (`4 + 5 x 154`, records carry Shift-JIS text).

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `loserCharId` | T: loser's character file id this message applies to, 0xFF = any |
| 0x001 | `unsigned char` | `unused_01` | U |
| 0x002 | `unsigned __int16` | `oneInN` | T: acceptance odds (0 = always); the picker retries while rand() % oneInN != 0 |
| 0x004 | `unsigned __int16` | `imageVariant` | T: win_<char><nn>.bmp variant (0 = win_<char>.bmp) |
| 0x006 | `unsigned char` | `unused_06[150]` | U: zero (one stray 0x30 at +6 in V_SION) |

### 5.4 `<datName>.CPF` - CPU AI script file (`MbCpfFile`, 193,388 bytes)

Loader: `CPU_LoadCpfForSlot(slotIndex)` 0x404770 builds `.\data\<datName of slot.actor.characterId>.cpf` and calls `CPU_ReadCpfFileToBuffer` 0x404720, which tries `g_PkArchives[0..9]` with `PKArchive_ReadEntryWhole` and **reads the whole entry into the fixed 193,388-byte buffer
`g_CpfBuffers[slotIndex]`** (0x477F58, 4 buffers; no size check); it then clears `g_CpuStates[slotIndex]` (224 bytes) and repeats for the partner buffer `slotIndex + 2` when `partnerPresent`. The training dummies (`Practice_LoadDummyCpf` 0x44CBF0, mode `g_PracticeDummyMode` 0..3 = `DEKU`, `JUMP`, `CROUCH`, `GUARD`;
mode 4 = the character's own file) load `.\data\deku.cpf` etc. (`g_DummyCpfPathTable` 0x46F8E8) into both buffers. 20 files exist: 16 characters (AKAAKIHA .. WARC) and the 4 dummies; guardPercent 30 (most), 0 (dummies, GAKIHA), 34 MIYAKO, 42 M_HISUI, 50 NERO, 60 WARAKIA, 100 GUARD.

Layout: `s32 guardPercent`, 184 bytes of header (zero in all 20 files, never read), 100 scripts of 1892 bytes, then 100 x 40-byte CP932 script names (editor labels, never read: "idle", "forward", "dash", ...). A **script** is 43 step-sized slots (the game indexes
`step[43*script + step]`): slots 0..39 are 44-byte steps, slots 40..42 (132 bytes) are the script's **condition record**.

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `int` | `guardPercent` | T: CPU_RollGuardReaction (reaction chance, scaled by difficulty in CPU_GuardPercentByDifficulty) |
| 0x004 | `unsigned char` | `unused_04[184]` | U: zero in all 20 files |
| 0x0BC | `MbCpfScript` | `script[100]` | T |
| 0x2E3CC | `char` | `scriptName[40]` | E: CP932 editor labels, never read |

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `MbCpfStep` | `step[40]` | T |
| 0x6E0 | `MbCpfCondition` | `condition` | T |

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned short` | `inputCode` | T: CPU_ApplyCpfStepInput switch (0/15 neutral, 1 fwd, 2 back, 3 down, 4 up, 5 up-fwd, 6 up-back, 7..10 A..D, 11..14 down+A..D, 16 = command move) |
| 0x002 | `unsigned short` | `durationBase` | T: frames |
| 0x004 | `unsigned short` | `durationRandom` | T: rand() % durationRandom added to durationBase |
| 0x006 | `unsigned short` | `commandIndex` | T: code 16: index handed to Fighter_TryStartCommandMove |
| 0x008 | `unsigned char` | `unused_08[16]` | U |
| 0x018 | `unsigned int` | `flags` | T: 1 = end script, 2 = end script (alt), 4 = wait for hit, 8 = ..., 0x10 = press button mask, bit31 = set cpuCommandReady |
| 0x01C | `unsigned int` | `unused_1C` | U |
| 0x020 | `unsigned char` | `dirCode` | T: CPU_CpfDirCodeToNumpad |
| 0x021 | `unsigned char` | `commandReadyLevel` | T: when flags bit31 is set, cpuCommandReady = clamp(value + 1, 1, 3) |
| 0x022 | `unsigned char` | `unused_22[10]` | U |

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `int` | `minDistX` | T: CPU_FindTargetInRange (distance window to the target, world/256) |
| 0x004 | `int` | `maxDistX` | T |
| 0x008 | `int` | `useYRange` | T |
| 0x00C | `int` | `minDistY` | T |
| 0x010 | `int` | `maxDistY` | T |
| 0x014 | `int` | `weight` | T: 0 = script is never a candidate; also used as the percent chance after selection |
| 0x018 | `int` | `unused_18` | U |
| 0x01C | `int` | `unused_1C` | U |
| 0x020 | `int` | `ownStanceCond` | T: 0 any, 1..4 ground/air/crouch rules |
| 0x024 | `int` | `targetStanceCond` | T |
| 0x028 | `int` | `targetCanActCond` | T |
| 0x02C | `int` | `enemyAttackNearbyCond` | T |
| 0x030 | `int` | `patternCondEnable` | T |
| 0x034 | `int` | `patternId` | T |
| 0x038 | `int` | `patternFrame` | T: 255 = any frame |
| 0x03C | `int` | `ownLifeCond` | T |
| 0x040 | `int` | `unused_40` | U |
| 0x044 | `int` | `targetNotDownedCond` | T |
| 0x048 | `int` | `interruptible` | T |
| 0x04C | `int` | `cooldown` | T: frames before the script can be chosen again |
| 0x050 | `int` | `minDifficulty` | T: skipped when g_Opt_CpuDifficulty < value |
| 0x054 | `unsigned char` | `unused_54[48]` | U |

Execution (T unless marked; `CPU_RunAiForSlot` 0x405500 per slot and tick): `CPU_TickScriptCooldowns` decrements the 100 per-script counters in `g_CpuStates[slot].scriptCooldown`; `CPU_TickCpfStep` 0x4053C0 decrements `actor.aiActionTimer` and the state's `stepTimer`; when the timer runs out, or a step flagged 4 sees
`actor.contactMark` (flag 8 additionally requires the attack to have been guarded: `attackResult & 2`; otherwise any `attackResult`), the script advances (`CPU_StartCpfActionStep` 0x4052E0: `aiActionTimer = durationBase + rand() % durationRandom`, `cpuCommandReady = clamp(commandReadyLevel + 1, 1, 3)` when `flags` bit 31 is set,
then `CPU_ApplyCpfStepInput` 0x4050D0) or ends (`flags & 1` or `& 2` resets script/step/timer). `inputCode`: 0/15 neutral, 1 forward (numpad 6), 2 back (4), 3 down (2), 4 up (8), 5 up-forward (9), 6 up-back (7), 7..10 buttons A, B, C, D (held), 11..14 down + A..D, 16 = `Fighter_TryStartCommandMove(slot, commandIndex)` and
direction `dirCode` (`CPU_CpfDirCodeToNumpad` 0x404FE0: 1 -> 6, 2 -> 4, 3 -> 8, 4 -> 2, 5 -> 3, 6 -> 1, 7 -> 9, 8 -> 7); `flags & 0x10` presses the buttons of the code (`CPU_ButtonMaskFromStepCode` 0x405050: 7/11 -> 0x1000, 8/12 -> 0x2000, 9/13 -> 0x4000, 10/14 -> 0x8000). Bosses (`bossFlag`) only keep forward/back/neutral.
Selection (`CPU_ScoreAndSelectAction` 0x404AE0, I for the arithmetic): every script 1..99 that is non-empty (`step0.durationBase`, `inputCode` or `flags & 1`), off cooldown, not blocked by `interruptible`, `minDifficulty <= g_Opt_CpuDifficulty` and satisfies the optional conditions (distance window found by `CPU_FindTargetInRange` 0x404860, own/target stance, target `canAct`, enemy attack nearby
(`Character_DetectNearbyEnemyAttack` 0x427870), own pattern/frame, own life ladder 10000/5000/2500/1250/625) gets a score (+10 per satisfied optional condition, +20 for the stronger variants) and `weight`; a weighted draw (`rand() % candidates`, accumulate score until it reaches the best score) picks one; the script's `cooldown` is stored in `scriptCooldown[script]`; the choice is accepted with
probability `rand() % 100 <= weight`, otherwise `decisionCooldown = CPU_DecisionCooldownByDifficulty()` (60/40/15/8/0 frames for difficulty 0..3/other) and nothing starts. `guardPercent` feeds `CPU_RollGuardReaction` 0x404980 (via `CPU_GuardPercentByDifficulty` 0x404690) which writes `actor.cpuGuardRequest` (1 = guard, 2 = do not).
Measured (20 files): 66 / 57 / 67 steps in AKIHA / ARC / SION (7 in each dummy); step fields used: `inputCode` (0..16), `durationBase`, `durationRandom`, `commandIndex`, `flags` (values 0, 1, 3, 4, 5, 6, 14, 17...), `dirCode` (0..4), `flags` bit 31 (`0x80` at +27, 135 steps); condition fields used: `minDistX/maxDistX`, `useYRange` + `minDistY/maxDistY` (46 scripts), `weight`, `ownStanceCond`
(1 / 2), `targetStanceCond` (1 / 2 / 4), `patternCondEnable/patternId/patternFrame` (1 script), `targetNotDownedCond` (46), `interruptible` (1 / 2), `cooldown`, `minDifficulty` (1..3). `unused_18/1C`, `ownLifeCond`, `targetCanActCond`, `enemyAttackNearbyCond`, `unused_54` are zero in all files; `unused_40` is 1 in one script (E, never read).
`MbCpuState` (g_CpuStates[4], 0x534D08, 224 bytes) and the candidate scratch `g_CpuCandidates[100]` (`{weight, score}`, 0x535088) + `g_CpuCandidateWeightSum` (0x5353A8):

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `int` | `scriptIndex` | T: running script (CPU_StartCpfActionStep) |
| 0x004 | `int` | `stepIndex` | T: running step |
| 0x008 | `int` | `stepTimer` | T: frames left in the step (decremented by CPU_TickCpfStep) |
| 0x00C | `int` | `decisionCooldown` | T: frames before the next script decision (CPU_RunAiForSlot, set from the difficulty in CPU_ScoreAndSelectAction) |
| 0x010 | `int` | `targetDistX` | T: \|dx\| to the nearest enemy (CPU_FindNearestEnemyDistance) |
| 0x014 | `int` | `targetDistY` | T: dy to the nearest enemy |
| 0x018 | `unsigned short` | `scriptCooldown[100]` | T: per-script cooldown counters (decremented by CPU_TickScriptCooldowns) |

### 5.5 `VECTOR.TXT` (data01.p, 1830 bytes, plain Shift-JIS text) - hit and tech vectors

`HitVectors_LoadFromVectorTxt` 0x42FCA0 reads it with `Text_SkipWhitespaceAndComments` (skip comments/whitespace), `Text_AdvanceToNextToken`, `Text_AdvanceToNextLine` and `atoi` / `atof`. Three sections, each ended by a line starting with `END`:
(1) `g_HitVectorTable[32]` (0x188E4C4, `MbHitVectorRow`, 9 integers per row stored as int16; row = hit reaction id; columns 0..3 knockback velX, velY, accelX, accelY, 4..7 the stage-2 launch vector, 8 = stun frames (<240) or the launch stage (255 blow-away, 241 air hit-stun, 220 guard-stun)),
(2) `g_TechVectorTable[7]` (0x1602934, `MbTechVectorRow`, 5 integers per row: velX, velY, accelX, accelY, invulnerability frames; row 0 wall recovery, 1..3 ground neutral/forward/back, 4..6 air forward/back/neutral; stride is 18 bytes, columns 5..8 are never written),
(3) seven hit-stop levels into `g_HitstopFramesByLevel[7]` (0x16D1A40; weak 8, medium 10, strong 12, none 1, XL 24, XXL 40, ultra-weak 3) followed by one float into `g_HitstopScale` (0x178D440, 1.5).
Consumers: `Actor_StartKnockback` 0x441790 (section 1 + 3), `Actor_StartTechRecovery` 0x427C30 (section 2).

### 5.6 Other per-character files the loader touches

`Battle_LoadFaceCutinsAndVoices` 0x402A20 (VS scene, per side; `usePartner` selects `partnerDatName`): `.\grp\system\<name>_face.bmp`, `.\grp\cut\cut_<datName>.bmp` and `cut_<datName>_1..4.bmp` (five cut-in bitmaps), 100 voice WAVs `.\effect\<datName><00..99>.wav` (a missing one falls back to `Sfx_LoadWavFileToVoiceSlot`).
`Load_Effect_DAT`: `.\data\effect.dat`. Optional `.dof` pattern override: section 2. Win screen assets: section 5.3.

## 6. Runtime character state

### 6.1 Pools and tables (VAs)

| symbol | address | type | notes |
|---|---|---|---|
| `g_PlayerSlots` | 0x16D1AE8 | `MbFighterSlot[4]` (6196 B each, ends 0x16D7BB8) | slot 0/1 = main fighter of side 0/1, slot 2/3 = their partner (`side + 2`); `active` byte at +0, actor at +4 |
| `g_EffectPool` | 0x16D7BC8 | `MbObjectSlot[1000]` (676 B each, 676,000 B, ends 0x177CC68) | effect objects / child objects; `active` byte, 3 unused bytes, actor at +4 |
| `g_EffectCharSlot` | 0x16A01A0 | `MbFighterSlot` | EFFECT.DAT bound as a pseudo character (data slot 4, team 255) |
| `g_PlayerSlotRecords` | 0x1602820 | `MbTeamRecord[4]` (68 B each) | per-team round/match record, section 6.5 |
| `g_CharPatternData` / `g_CharPartsData` / `g_CharCgData` | 0x16D1830 / 0x189557C / 0x17DC690 | `MbCharDatHead *[20]` / `MbPartsHeader *[20]` / `MbCgBlobHead *[20]` | loader output per data slot |
| `g_CpfBuffers` | 0x477F58 | `MbCpfFile[4]` | one CPU AI file per slot (0..3) |
| `g_CpuStates` / `g_CpuCandidates` / `g_CpuCandidateWeightSum` | 0x534D08 / 0x535088 / 0x5353A8 | `MbCpuState[4]` / `MbCpuCandidate[100]` / `int` | CPU AI runtime |
| `g_CharTable` / `g_CharTableCount` / `g_CharFileIdToTableIndex` / `g_CssCharGrid` | 0x181FB88 / 0x16A011C / 0x178CAB4 / 0x18955D0 | `MbCharSelEntry[100]` / `int` / `u8[100]` / `int[100]` | CHARSEL.CT |
| `g_WmtTable` / `g_WmtCount` | 0x79F0B0 / 0x79F08C | `MbWmtRecord[100]` / `u32` | win messages of the last winner |
| `g_HitVectorTable` / `g_TechVectorTable` / `g_HitstopFramesByLevel` / `g_HitstopScale` | 0x188E4C4 / 0x1602934 / 0x16D1A40 / 0x178D440 | `MbHitVectorRow[32]` / `MbTechVectorRow[7]` / `u8[7]` / `float` | VECTOR.TXT |
| `g_PkArchives` / `g_ArchiveData00` | 0x1895878 / 0x16D1AC8 | `MbArchiveHandle[10]` / `MbArchiveHandle` | slots 7, 8, 9 = data03, data02, data01 |
| `g_EvadeScratchCommand`, `g_PartnerCallScratchCommand`, `g_DoubleTapScratchCommand` | 0x1602630 / 0x1602660 / 0x1602690 | `MbCtCommand` | synthesised sequences matched by `Fighter_MatchCommandSequence` (evade double-tap, partner call = back + D, tech double-tap) |
| `g_BattleControlPhase` | 0x1602932 | `u8` | 0 fighting, 1 restricted (commands with `flags2 & 4` only), 2 intro/locked (no input) |
| `g_GlobalCounterBank` | 0x18F9780 | `int[10]` | counters addressed by `counterRequirement >= 100` and EF op 100/101 |
| `g_PracticeDummyMode` / `g_PracticeDummyModeApplied` / `g_DummyCpfPathTable` | 0x1602B60 / 0x1901014 / 0x46F8E8 | `u8` / `int` / `char *[4]` | training dummy |
| `g_AtClassVsInvulnHitTable` | 0x46CCEC | `u8[5][5]` | AT.hitClass x frame invulnerability class |
| `g_ScreenShakeTimer`, `g_ScreenScaleShift`, `g_TeamComboRecordCursor`, `g_TeamHeatBankRequest`, `g_TeamGuardFloorEnabled`, `g_CharDatLoadBuffer` | 0x177CC94, 0x177CCA0, 0x1602818, 0x1602B50, 0x18F9800, 0x1880CB8 | scalars / small arrays | see the IDB |

### 6.2 Fighter slot (`MbFighterSlot`, 6196 bytes) and object slot (`MbObjectSlot`, 676 bytes)

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `active` | T: 1 = slot in use (Slot_BindCharacterData / Slot_InitPointCharacterForRound set 1, FighterSlot_ResetKeepingDataPointers / Battle_LoadAllCharacterSlots clear) |
| 0x001 | `unsigned char` | `unused_01[3]` | U: no access |
| 0x004 | `MbActor` | `actor` | T: the actor (actor = slot + 4; memset 0x2A0 by Slot_InitPointCharacterForRound) |
| 0x2A4 | `MbCtHeader` | `ct` | T: CT header copied by Slot_LoadCommandCT (slot + 676) |
| 0x300 | `unsigned int` | `commandCount` | T: CT count dword (slot + 768) |
| 0x304 | `MbCtCommand` | `command[100]` | T: command table (slot + 772) |
| 0x1434 | `MbInputRing` | `ringA` | T: input history A (CommandBuffer_PushInput) |
| 0x1634 | `MbInputRing` | `ringB` | T: input history B with releases |

`MbFighterSlot` = 4 + 672 (actor) + 92 (CT header) + 4 (count) + 4400 (100 commands) + 1024 (two input rings) = 6196, with the CT file's `[count][commands][header]` split by `Slot_LoadCommandCT` around the actor
(header at slot+676, count at slot+768, commands at slot+772). The input rings (`MbInputRing`, 64 states newest first + 64 ages, ages capped at 1,000,000; `Fighter_InvalidateInputHistory` 0x429290 sets `age[0]` of both rings to 1,000,000) are written by `CommandBuffer_PushInput` 0x43D810:
ring A stores `buttons | (direction << 24)` of the held state, ring B ORs in `actor.inputButtonsReleased`; directions are mirrored through `word_46CCD8` when the facing changed while the actor is not airborne.

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `active` | T: 1 = slot in use (InitChildObject) |
| 0x001 | `unsigned char` | `unused_01[3]` | U: no access |
| 0x004 | `MbActor` | `actor` | T: the actor (InitChildObject memset(slot + 4, 0, 0x2A0)) |

**Slot life cycle** (T): `Slot_BindCharacterData` (data pointers, `active = 1`); `Slot_InitPointCharacterForRound` 0x437760 (per round, from `Battle_InitRoundCharactersAndTeams` 0x437C60 and after the loaders): zeroes `sequence[16..31]` of the first `commandCount` commands, saves
`slotIndex/characterId/controlType/paletteIndex`, `superMeter`, `heatGauge` and the data pointers, `memset(actor, 0, 0x2A0)`, restores them, then `teamIndex = team`, `posX = startX - 0x8000` (start X 49152 / 81920 for slots 0/1 and 36352 / 94720 for the partners), `posY = startY`, `life = maxLife = 10000`,
`facingLeft = nextFacingLeft = facing`, `writeOnlyFF_25C = 0xFF`, `drawDepth = 0x80`, `fighterSlot = slot`, `active = 1`. `FighterSlot_ResetKeepingDataPointers` 0x4378E0 is the same routine for a reserve partner (`drawDepth = 120`, **`koFlag = 1`**). `Battle_InitRoundCharactersAndTeams` additionally sets `superMeter` 10000 (30000 in attract mode),
`guardGauge` and `writeOnly5000_74` to 5000, zeroes `heatGauge`, `pendingHeatGain`, `teamCopiedOnly_54`, and queues pattern 50 (round intro) when the character has it.
Effect objects: `InitChildObject` 0x416B30 (`memset(slot + 4, 0, 0x2A0)`, owner/parent/flags from the EF record) and `EffectObject_SpawnGhostCopy` 0x416FB0 (`objKind` 31) allocate the first free `g_EffectPool` entry.

### 6.3 Actor (`MbActor`, 672 bytes = 0x2A0)

One struct for fighters and spawned objects. Fields are named from the functions that read or write them (names in the comments are IDB names). Where MB differs from GOF1 the comment says so; appendix A maps every GOF1 field.
The "heat" fields are MB's version of GOF1's reserve gauge: `heatGauge` (0..100) is filled by the victim-side gain `AT.heatGaugeGain` (via `pendingHeatGain` and `Fighter_BankPendingHeatGain` 0x426AF0 when the combo ends), at 100 `Actor_TickHeatGauge` 0x425F70 switches `heatState` to ACTIVE
(after `heatTimer` frames the gauge drains by 4 every 3rd frame, an aura is drawn, damage dealt is +10 %: `Damage_ScaleByDifficultyAndTeam`), at 0 it enters COOLDOWN (`heatTimer` 60) and then IDLE. `superMeter` is the 0..30000 super meter (3 levels, `activeMoveMeterCost` is spent by `Actor_SpawnEffectById`).

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `unsigned char` | `slotIndex` | T: player slot / owner player index 0..3 (Slot_InitPointCharacterForRound writes the slot id); children copy it (InitChildObject); EfType9 sound bank |
| 0x001 | `unsigned char` | `characterId` | T: roster character file id (Battle_LoadAllCharacterSlots); ObjRunActionScript compares it with 16 (debug reset) |
| 0x002 | `MbControlType` | `controlType` | T: MbControlType, 1 = CPU (Battle_LoadAllCharacterSlots a5 == 1), CPU_RunAiForSlot / IF 11 / IF 32 |
| 0x003 | `unsigned char` | `unused_03` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x004 | `MbObjKind` | `objKind` | T: MbObjKind; 0 fighter, 31 ghost trail (EffectObject_SpawnGhostCopy), 0xFF / 0x8F spawned child (InitChildObject); values >= 0xF0 are tested as "is a child" |
| 0x005 | `unsigned char` | `paletteIndex` | T: colour palette chosen at character select (written by Battle_LoadAllCharacterSlots, preserved by Slot_InitPointCharacterForRound); the palette is applied at upload time, the byte itself is never read back |
| 0x006 | `unsigned char` | `pattern` | T: current pattern (action) id; Actor_ResolveFrameDataPointers indexes charData.patternOffset with it |
| 0x007 | `unsigned char` | `frame` | T: current frame inside the pattern |
| 0x008 | `unsigned __int16` | `spriteId` | T: sprite id of the current frame (ObjEnterCurrentAction), read by the draw code |
| 0x00A | `unsigned char` | `ifRunState` | T: IF-table run gate (ObjEnterCurrentAction sets 1/2, Actor_TickWordTimers clears) |
| 0x00B | `unsigned char` | `efRunState` | T: EF-table run gate (Character_RunFrameEFs clears it after running) |
| 0x00C | `unsigned char` | `airReactionActive` | T: 1 while a launch/air hit-reaction stage (hitStage >= 240) is running (Actor_ApplyFrameMotionFlags) |
| 0x00D | `unsigned char` | `unused_0D` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x00E | `unsigned __int16` | `frameTicks` | T: ticks spent on the current frame (compared with AnimFrame.duration) |
| 0x010 | `unsigned __int16` | `unused_10` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x012 | `unsigned char` | `animationEnded` | T: set by ObjRunActionScript when the animation reached its last frame; IF 2 |
| 0x013 | `unsigned char` | `unused_13` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x014 | `unsigned char` | `loopCounter` | T: AnimFrame.loopCount is loaded here; IF 9/10 and aniFlag 5 use it |
| 0x015 | `unsigned char` | `unused_15` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x016 | `unsigned __int16` | `spawnFlagsA` | T: child flags from EF arg2 bits (InitChildObject); 0x40 no ground landing, 0x20/0x8/0x4/0x2/0x1 see gof1.md Gof1ObjFlagsA (same meaning) |
| 0x018 | `unsigned __int16` | `spawnFlagsB` | T: child flags from EF arg3 bits (0x40 ownership test in Actor_IsTickAllowed, 8 hitstop to owner, 1 draw shadow copy) |
| 0x01A | `unsigned char` | `homingMode` | T: EF arg4 low byte (InitChildObject); Actor_UpdateHomingObject |
| 0x01B | `unsigned char` | `homingBaseAction` | T: first pattern of the homing action family |
| 0x01C | `unsigned char` | `spawnFlagsC` | T (write-only): InitChildObject writes 0 / 1 and ORs 2 / 4 from EF arg bits; no reader found |
| 0x01D | `unsigned char` | `unused_1D` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x01E | `unsigned __int16` | `homingTick` | T: counts up while homingTimeout != 0 |
| 0x020 | `unsigned __int16` | `homingTimeout` | T: EfType30 sub 0 arg2 |
| 0x022 | `__int16` | `homingOffsetX` | T: target offset X, 128 = centred |
| 0x024 | `__int16` | `homingOffsetY` | T: target offset Y, 224 = baseline |
| 0x026 | `unsigned __int16` | `homingCooldown` | T: set to 40 after a hit and counted down |
| 0x028 | `__int16` | `homingParam[10]` | T: word table written by EfType30_SetActorParams sub 1 (word[40 + 2*arg0] = arg1) |
| 0x03C | `unsigned __int16` | `aiActionTimer` | T: CPU AI: frames left in the running step (CPU_StartCpfActionStep loads durationBase + rand % durationRandom, CPU_TickCpfStep decrements) |
| 0x03E | `unsigned char` | `unused_3E[2]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x040 | `__int16` | `aiScriptIndex` | T: CPU AI: running script, -1/0 = none |
| 0x042 | `__int16` | `aiScriptStep` | T: CPU AI: step inside the script |
| 0x044 | `unsigned char` | `cpuCommandReady` | T: CPU command request latch (CPU_StartCpfActionStep sets 1..3 from step.commandReadyLevel; IF 11/31 consume it) |
| 0x045 | `unsigned char` | `unused_45[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x048 | `int` | `superMeter` | T: super meter 0..30000 (10000 per level); AddSuperMeter, EfType6 op 4, Actor_SpawnEffectById spends activeMoveMeterCost |
| 0x04C | `unsigned char` | `superLevelCrossMark` | T: AddSuperMeter sets 10 when the level changes; partner mirror copies it |
| 0x04D | `unsigned char` | `unused_4D[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x050 | `int` | `heatGauge` | T: 0..100 gauge filled by AT.heatGaugeGain (Actor_AddHeatGauge); at 100 Actor_TickHeatGauge enters heatState ACTIVE (MB analog of GOF reserveGauge) |
| 0x054 | `unsigned char` | `teamCopiedOnly_54[4]` | T: copied between a lead and its partner (MaidsPartner_UpdateReserveAI, Players_ProcessSlotSwap) and cleared at round start; never read otherwise |
| 0x058 | `int` | `pendingHeatGain` | T: gauge gain waiting to be banked into heatGauge when the combo ends (Fighter_BankPendingHeatGain clears it) |
| 0x05C | `int` | `life` | T: hit points, 10000 at round start (30000 in attract demo); Damage_*, Battle_ResolveHit, EfType5/6 |
| 0x060 | `int` | `maxLife` | T: 10000, written next to life by Slot_InitPointCharacterForRound; partner mirror copies it |
| 0x064 | `__int16` | `heatTimer` | T: delay / duration counter of the heat gauge (60 on gain, 60 when heat ends, counts down in Actor_TickHeatGauge) |
| 0x066 | `MbHeatState` | `heatState` | T: MbHeatState; 1 = heat active (gauge drains, aura drawn by Actor_DrawHeatAuraAndTintParticles), 2 = cooldown |
| 0x067 | `unsigned char` | `heatCancelPending` | T: armed by Fighter_TryStartCommandMove for a heat-cancel move; Actor_OnActionChanged then forces heatState = 2 |
| 0x068 | `unsigned char` | `unused_68[2]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x06A | `__int16` | `heatLastGain` | T: last gain passed to Actor_AddHeatGauge; doubled into heatTimer when the gauge fills |
| 0x06C | `unsigned char` | `clearHistoryOnAction` | T: set from command flag 0x10 (Fighter_TryStartCommandMove); Actor_OnActionChanged invalidates both input rings |
| 0x06D | `unsigned char` | `unused_6D[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x070 | `int` | `guardGauge` | T: guard balance 0..10000, pulled toward 5000 by Actor_DecayGuardGauge, changed by Actor_AdjustGuardGauge; Character_Draw flashes above 8000; damage scale input |
| 0x074 | `int` | `writeOnly5000_74` | T (write-only): Battle_InitRoundCharactersAndTeams sets it to 5000 together with guardGauge; nothing reads it |
| 0x078 | `unsigned char` | `guardRegenDelay` | T: frames before the guard gauge starts to decay (30 / 60 set by Actor_AdjustGuardGauge); byte access only |
| 0x079 | `unsigned char` | `unused_79[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x07C | `int` | `posX` | T: world X in 1/128 pixel units |
| 0x080 | `int` | `posY` | T: world Y in 1/128 pixel units, 0 = ground, negative = up |
| 0x084 | `int` | `posZ` | T: read by the camera (Camera_UpdateFocusYZFromFighters, averaged into g_CameraZoomFocus) and never written: always 0 |
| 0x088 | `int` | `posXAfterIntegrate` | T (write-only): posX when ObjIntegrateMotion returns |
| 0x08C | `int` | `posXAtTickStart` | T: posX before the tick |
| 0x090 | `int` | `posYAtTickStart` | T: posY before the tick |
| 0x094 | `int` | `velX` | T: velocity X |
| 0x098 | `int` | `velY` | T: velocity Y |
| 0x09C | `__int16` | `accelX` | T: acceleration X |
| 0x09E | `__int16` | `accelY` | T: acceleration Y (gravity) |
| 0x0A0 | `__int16` | `maxVelX` | T: X speed clamp |
| 0x0A2 | `unsigned char` | `unused_A2[2]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x0A4 | `int` | `knockVelX` | T: knockback velocity X (Actor_StartKnockback) |
| 0x0A8 | `int` | `knockVelY` | T: knockback velocity Y |
| 0x0AC | `__int16` | `knockAccelX` | T: knockback friction X |
| 0x0AE | `__int16` | `knockAccelY` | T: knockback gravity Y |
| 0x0B0 | `int` | `inertiaX` | T: carried inertia added to posX each tick, decays by inertiaDecay |
| 0x0B4 | `int` | `carriedVelX` | T: velX saved when frame flags1 bit 0 is set, becomes inertiaX on the next pattern |
| 0x0B8 | `int` | `knockCarryVelX` | T: MB-specific: knockVelX saved by the wall-bounce stage (0xFE) and converted to inertia on the pattern change checked against knockCarryPattern |
| 0x0BC | `__int16` | `inertiaDecay` | T: inertiaX / 10 (GOF: / 16), Actor_ApplyFrameMotionFlags |
| 0x0BE | `unsigned __int16` | `carryPattern` | T: pattern in which carriedVelX was saved |
| 0x0C0 | `unsigned __int16` | `knockCarryPattern` | T: MB-specific: pattern in which knockCarryVelX was saved |
| 0x0C2 | `__int16` | `grabDrawOffsetX0` | T: start draw offset X of a grabbed actor (EF04/EF05, Character_Draw lerps to grabDrawOffsetX1) |
| 0x0C4 | `__int16` | `grabDrawOffsetY0` | T: start draw offset Y |
| 0x0C6 | `__int16` | `zoomOriginX0` | T: zoom/rotation origin X at the start, 320 by default |
| 0x0C8 | `__int16` | `zoomOriginY0` | T: origin Y, 448 by default |
| 0x0CA | `__int16` | `grabDrawOffsetX1` | T: end draw offset X |
| 0x0CC | `__int16` | `grabDrawOffsetY1` | T: end draw offset Y |
| 0x0CE | `__int16` | `zoomOriginX1` | T: origin X at the end |
| 0x0D0 | `__int16` | `zoomOriginY1` | T: origin Y at the end |
| 0x0D2 | `__int16` | `rotAngleA` | T: rotation angle, 1/10000 turn (+950 per tick while thrown) |
| 0x0D4 | `__int16` | `rotAngleB` | T: second rotation angle |
| 0x0D6 | `unsigned char` | `clearInertiaRequest` | T: set by Actor_StartKnockback; Actor_ApplyFrameMotionFlags then clears inertiaX/inertiaDecay |
| 0x0D7 | `unsigned char` | `attackResult` | T: result of this actor attack on its victim: bit0 hit, bit1 guarded, ... (Battle_ResolveHit, Hitbox_Detect*); 0 = none |
| 0x0D8 | `unsigned char` | `ownerAttackResult` | T: copy of a child attack result written into its owner |
| 0x0D9 | `unsigned char` | `drawDepth` | T: draw order key (SetCharacterDrawPriority, SpawnGhostCopy) |
| 0x0DA | `unsigned char` | `drawModeVariant` | T: row offset into the grabbed draw-mode remap table (Character_Draw, Box_ApplyDrawModeTransform) |
| 0x0DB | `unsigned char` | `unused_DB` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x0DC | `unsigned __int16` | `activeMoveMeterCost` | T: super meter cost of the running command move (IF 11 copies command.meterCost; Actor_SpawnEffectById spends it) |
| 0x0DE | `unsigned char` | `unused_DE` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x0DF | `MbLauncherState` | `launcherState` | T: MbLauncherState: 1 set by Battle_ResolveHit when an AT with flagsB bit 0 connects first (victim goes to pattern 30), cleared by the next hit; 2 while executing pattern 41 (follow-up); Character_ProcessPlayerInput uses it for the jump-cancel into pattern 41 |
| 0x0E0 | `unsigned char` | `unused_E0` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x0E1 | `unsigned char` | `hitstopFrames` | T: remaining hit-stop frames (Actor_SetHitstopFrames) |
| 0x0E2 | `unsigned char` | `hitstopTrailing` | T: set to 1 by Battle_ResolveHit; counted down with hitstopFrames by ObjRunActionScript |
| 0x0E3 | `unsigned char` | `contactMark` | T: set by the hit detection when this actor touched a target; CPU_TickCpfStep reads it, CPU_ApplyCpfStepInput clears it |
| 0x0E4 | `unsigned char` | `grabState` | T: 0 free, 1 held by a fighter, 2 held by a child object (Actor_TryGrabFighter) |
| 0x0E5 | `unsigned char` | `inputLock` | T: EfType6 op 9 sets it; blocks input dispatch and IF input handling; ObjRunActionScript sets it when swapOutTimer expires; Players_ProcessSlotSwap clears it |
| 0x0E6 | `unsigned char` | `freezeTimerA` | T: EfType6 op 2 arg0; Actor_TickFreezeTimers counts it down |
| 0x0E7 | `unsigned char` | `freezeTimerB` | T: EfType6 op 2 arg1; checked by the hit detection |
| 0x0E8 | `unsigned char` | `koFlag` | T: set at KO by Battle_ResolveHit (EfType6 op 5 writes it); camera and IF 100 test it |
| 0x0E9 | `unsigned char` | `cameraExcludeFlag` | T (read-only): Camera_UpdateFollowActiveCharacters / Camera_UpdateFocusXFromFighters / Camera_UpdateFocusYZFromFighters skip fighters with 1; never written |
| 0x0EA | `unsigned char` | `attackHitsLeft` | T: hits the current attack may still land (AS.attackHitCount loaded by ObjEnterCurrentAction) |
| 0x0EB | `unsigned char` | `guardState` | T: non-zero while guarding (Battle_ResolveHit sets 1) |
| 0x0EC | `unsigned char` | `cpuGuardRequest` | T: CPU_RunAiForSlot (random guard decision by CPU_RollGuardReaction); Battle_ResolveHit |
| 0x0ED | `unsigned char` | `guardInputWindow` | T: MB-specific: 8-frame window after Character_DetectNearbyEnemyAttack during which pattern 17/18 (guard) keeps being entered by direction input (Character_ProcessPlayerInput) |
| 0x0EE | `unsigned char` | `recoverMode` | T: wake-up/recovery selector set by Battle_ResolveHit and Actor_WallBounceReaction |
| 0x0EF | `unsigned char` | `airTechLatch` | T: 0/1 latch in CPU_TryEvadeOrTechReaction / Character_ProcessTechInput: set to 1 when Actor_CanAirTech allows an air tech, tested before starting the tech; ObjRunActionScript clears it |
| 0x0F0 | `unsigned char` | `bounceCount` | T: wall/ground bounces taken |
| 0x0F1 | `unsigned char` | `recoveryTimer` | T: frames since the last hit |
| 0x0F2 | `unsigned char` | `throwTechSeen` | T: set on the grabber when a throw tech succeeds; ComboRecord_RegisterHit flags the combo |
| 0x0F3 | `unsigned char` | `stageEdgeSide` | T: 1/2 = pushed past the right/left stage edge (Actor_ClampToStageEdge) |
| 0x0F4 | `unsigned char` | `airComboStarterLatch` | T: set when an AT.flagsB 0x10 hit lands, cleared by ObjRunActionScript |
| 0x0F5 | `unsigned char` | `jumpCancelUsed` | T: MB-specific: set by Character_ProcessPlayerInput after the jump-cancel patterns 35/36/37 started, cleared by Fighter_BankPendingHeatGain |
| 0x0F6 | `unsigned char` | `throwTechAllowed` | T: Battle_ResolveHit sets !(AT.flagsB & 0x20); Actor_TryStartThrowTech requires it |
| 0x0F7 | `unsigned char` | `inputBlockFlag` | T: Fighter_CanReceiveInput refuses input while it is 1; set by Character_ProcessTechInput, cleared by ObjRunActionScript |
| 0x0F8 | `unsigned char` | `counterHitSide` | T (write-only here): 1 = this actor landed a counter-hit, 2 = was counter-hit |
| 0x0F9 | `unsigned char` | `unused_F9` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x0FA | `unsigned __int16` | `scriptTimer[5]` | T: five countdown words (Actor_TickWordTimers); EfType6 op 10 sets [arg0]; [0] doubles as the Battle_ResolveHit "launcher hit seen" lock |
| 0x104 | `unsigned char` | `hitQueueClear` | T: hit detection sets 1 to discard last tick hit queue; Battle_ResolveHit zeroes hitQueueCount |
| 0x105 | `unsigned char` | `shakeTimer` | T: draw shake after a hit (Character_Draw offsets X by shakeTimer >> 2 on odd ticks); counted down by Actor_TickFreezeTimers |
| 0x106 | `unsigned char` | `hitQueueCount` | T: number of valid hitAt/hitRect/hitAttacker entries, max 8 |
| 0x107 | `unsigned char` | `tintMode` | T: AT.statusEffect 1..4 (Character_ApplyAddedEffect); Character_Draw / Actor_DrawHeatAuraAndTintParticles |
| 0x108 | `unsigned char` | `tintTimer` | T: MB-specific: 120 when a tint effect starts, counted down by Actor_TickWordTimers; Character_Draw tints while non-zero |
| 0x109 | `unsigned char` | `unused_109` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x10A | `unsigned __int16` | `statusEffectParam` | T: MB-specific: AT.statusEffectParam stored by effect mode 4, counted down by Actor_TickWordTimers |
| 0x10C | `unsigned char` | `hitStage` | T: 0 = not in hit-stun; 1..239 = remaining stun frames; 240..255 = launch/air stages dispatched by Actor_ApplyFrameMotionFlags (byte in MB, word in GOF) |
| 0x10D | `unsigned char` | `juggleState` | T: non-zero while the victim is in an air juggle (set by Actor_StartKnockback; cleared on landing) |
| 0x10E | `unsigned char` | `hitReactionId` | T: reaction index passed to Actor_StartKnockback (g_HitVectorTable row); 27 = wall bounce |
| 0x10F | `unsigned char` | `hitStageAtHit` | T (write-only here): copy of hitStage taken by Actor_StartKnockback / Battle_ResolveHit |
| 0x110 | `unsigned char` | `unused_110` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x111 | `unsigned char` | `invulnFlags` | T: non-zero = takes no damage (Battle_ResolveHit); EfType6 op 8 sets 0x10 |
| 0x112 | `unsigned char` | `introFinished` | T: 1 once the intro is over; set by EF op 254 and by Battle_InitRoundCharactersAndTeams when there is no intro, waited on by Round_IntroStateMachine |
| 0x113 | `unsigned char` | `koMoveType` | T: (attacker move type & 0x7F) + 1 when the actor is KO by it; non-zero blocks pushbox resolution |
| 0x114 | `unsigned char` | `inputHeldFlags` | T: low 7 bits = air jumps used (compared with ct.maxAirJumps), bit 7 = direction neutral seen since the last jump (Character_ProcessPlayerInput) |
| 0x115 | `unsigned char` | `writeOnlyZero_115` | T (write-only): ObjRunActionScript clears it |
| 0x116 | `unsigned char` | `guardRecoveryFlag` | T: 1 after a guard-cancel / early guard; read by Actor_CountStunMashAndCheckGuardRecovery and Fighter_ScanCommandMoves |
| 0x117 | `unsigned char` | `unused_117` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x118 | `unsigned __int16` | `mashCounter` | T: counts button presses while in hit-stun (Actor_CountStunMashAndCheckGuardRecovery) |
| 0x11A | `signed char` | `tobiCount[10]` | T: ten projectile / counter bytes ("TOBI" counters): EfType6 op 3 adds / subtracts, IF 24/25 and command counterRequirement test them |
| 0x124 | `unsigned char` | `dashCount` | T: EfType6 op 1/2 add/subtract; command usageLimit compares it; set to 100 by the tech recovery |
| 0x125 | `unsigned char` | `unused_125` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x126 | `__int16` | `paramVar[10]` | T: ten word variables: EfType6 op 105 sets/adds [arg0]; IF 25/29/31 compare them |
| 0x13A | `unsigned char` | `trailMode` | T: ghost trail colour mode + 1 (EfType6 op 0); EffectObject_Draw switches on it |
| 0x13B | `unsigned char` | `unused_13B` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x13C | `unsigned __int16` | `decorLifeTotal` | T: ghost object total life (EffectObject_SpawnGhostCopy a5; EffectObject_Draw fades alpha by frameTicks / this) |
| 0x13E | `unsigned __int16` | `decorFlags` | T: ghost object flags (bit 0: depth + 10) |
| 0x140 | `__int16` | `decorZoomGrowX` | T: ghost zoom change per remaining tick X |
| 0x142 | `__int16` | `decorZoomGrowY` | T: ghost zoom change per remaining tick Y |
| 0x144 | `unsigned char` | `afterimageMode` | T: afterimage config id + 1 (EfType6 op 3); AfterImage_* switch on it, 0 = off |
| 0x145 | `unsigned char` | `afterimageSteps` | T: number of afterimage steps <= 119 |
| 0x146 | `unsigned char` | `afterimageSpacing` | T: spacing of the steps |
| 0x147 | `unsigned char` | `afterimageRestart` | T: EfType6 op 3 sets 1 when the mode changes; raw read/write in Characters_DrawAll |
| 0x148 | `unsigned char` | `justGuardActive` | T: set when a just-guard absorbs a hit; Battle_ResolveHit clears it, Actor_StartKnockback reads it |
| 0x149 | `unsigned char` | `unused_149` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x14A | `unsigned __int16` | `locomotionUsedMask` | T: bit n = pattern n (1..9 walk/run/dash/jump) used since the last landing (ObjRunActionScript) |
| 0x14C | `unsigned char` | `commandUsedMask[14]` | T: bit i = command move i already used in this chain (ObjRunActionScript sets it, Fighter_TryStartCommandMove refuses repeats) |
| 0x15A | `unsigned char` | `pendingCommandId` | T: command move accepted by Fighter_TryStartCommandMove this tick (0xFF = none) |
| 0x15B | `unsigned char` | `startedByCommandId` | T: command id that requested the action just entered; read by Fighter_TryStartCommandMove (moveClass compared with MBMOVE_SUPER) |
| 0x15C | `__int16` | `hitsTakenInCombo` | T: consecutive hits taken (Battle_ResolveHit increments, ObjRunActionScript indexes the recovery table) |
| 0x15E | `unsigned __int16` | `attackHitsConnected` | T: hits landed by the current action (hit detection increments; IF 17 compares with param[1]) |
| 0x160 | `unsigned char` | `evadeDir` | T: 1..4 double-tap evade detected by Fighter_DetectEvadeDoubleTap; Battle_ResolveHit picks the evade entry |
| 0x161 | `signed char` | `evadeTimer` | T: evade window counter (signed; negative = cooldown) |
| 0x162 | `unsigned __int16` | `swapOutTimer` | T: MB-specific: Players_ProcessSlotSwap sets 1 on the character leaving the field; ObjRunActionScript counts it 1..61 and then releases (sets inputLock, clears launcherState); pushbox resolution and EffectObjects_UpdateAll skip actors with a non-zero value; 4095 = held |
| 0x164 | `unsigned char` | `flashMode` | T: colour flash mode (Actor_SetColorFlash); Character_Draw / EffectObject_Draw switch |
| 0x165 | `unsigned char` | `flashTimer` | T: remaining flash frames (Actor_TickFreezeTimers) |
| 0x166 | `unsigned char` | `flashDuration` | T: total flash frames, denominator of the fade modes |
| 0x167 | `unsigned char` | `requestParam` | T (write-only): argument of ObjRequestAction, cleared by ObjRunActionScript |
| 0x168 | `unsigned char` | `counterWindowState` | T: counter-hit window set by frame flags2 bits 20..23 (ObjEnterCurrentAction); Battle_ResolveHit reads it |
| 0x169 | `unsigned char` | `unused_169` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x16A | `__int16` | `launchVelX` | T: stage-2 launch velocity X loaded when hitStage advances (Actor_StartKnockback fills it from g_HitVectorTable.launchVelX) |
| 0x16C | `__int16` | `launchVelY` | T: stage-2 launch velocity Y |
| 0x16E | `__int16` | `launchAccelX` | T: stage-2 acceleration X |
| 0x170 | `__int16` | `launchAccelY` | T: stage-2 acceleration Y |
| 0x172 | `unsigned char` | `unused_172[2]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x174 | `MbAtRecord *` | `hitAt[8]` | T: AT record of each hit received this tick (hit detection writes, Battle_ResolveHit reads) |
| 0x194 | `MbRectI32` | `hitRect[8]` | T: world rectangle of each overlap |
| 0x214 | `MbActor *` | `hitAttacker[8]` | T: attacking actor of each hit |
| 0x234 | `MbActor *` | `lastAttacker` | T: attacker of the last accepted hit (Battle_ResolveHit); Actor_StartKnockback applies the push to it |
| 0x238 | `MbActor *` | `grabbedBy` | T: grabber while grabState != 0 (Actor_TryGrabFighter) |
| 0x23C | `MbActor *` | `ownerFighter` | T: owning fighter of a child object (InitChildObject); homing target; ghost owner |
| 0x240 | `MbActor *` | `linkedActor` | T: parent actor of a child / grab partner (Character_Draw reads its frame while grabbed) |
| 0x244 | `unsigned char` | `inputDir` | T: numpad direction facing-corrected (ProcessPlayerInput, CPU_ApplyCpfStepInput; Actor_MirrorInputDirection) |
| 0x245 | `unsigned char` | `unused_245[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x248 | `unsigned int` | `inputButtons` | T: buttons held (bits 0..5) and pressed this tick (bits 12..17) |
| 0x24C | `unsigned int` | `inputButtonsReleased` | T: buttons released this tick (bits 0..5); input ring B ORs it in (CommandBuffer_PushInput) |
| 0x250 | `unsigned char` | `teamIndex` | T: side/team 0 or 1; children copy it |
| 0x251 | `unsigned char` | `endConditionMet` | T: set by IF 2 END_CONDITION; the object is removed on the next EffectObjects_UpdateAll |
| 0x252 | `unsigned __int16` | `pendingPattern` | T: pattern requested by ObjRequestAction, 0xFFFF = none |
| 0x254 | `__int16` | `pendingPatternPriority` | T: priority of the pending request, -1 = none; a request wins when higher |
| 0x256 | `unsigned __int16` | `pendingFrame` | T: frame requested by IF jumps (Actor_JumpToFrame / ObjEnterCurrentAction), 0xFFFF = none |
| 0x258 | `__int16` | `pendingFramePriority` | T: priority of the pending frame jump |
| 0x25A | `unsigned char` | `facingLeft` | T: 0 faces right, 1 faces left |
| 0x25B | `unsigned char` | `nextFacingLeft` | T: facing to turn to when the action changes |
| 0x25C | `unsigned char` | `writeOnlyFF_25C` | T (write-only): Slot_InitPointCharacterForRound writes 0xFF |
| 0x25D | `unsigned char` | `mirrorHistoryPending` | T: Fighter_MatchCommandSequence / CommandBuffer_PushInput: when the facing changed and it is 1, the recorded directions are mirrored once |
| 0x25E | `unsigned char` | `inputActionLatch` | T: set 1 by IF 6/7 input jumps with flag 4; ObjRunActionScript steps it 1 -> 2 -> 0 |
| 0x25F | `unsigned char` | `actionChangedFlag` | T: Actor_OnActionChanged sets 1, Actor_TickWordTimers clears |
| 0x260 | `unsigned char` | `landed` | T: 1 when posY reached the ground (ObjIntegrateMotion): drives aniFlag 3/4 landing jumps |
| 0x261 | `unsigned char` | `actionEnded` | T (write-only): ObjRunActionScript sets it when a landing/animation end finished the action |
| 0x262 | `unsigned char` | `stanceCopy` | T: AS.stance of the previous tick (ObjRunActionScript) |
| 0x263 | `unsigned char` | `unused_263` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |
| 0x264 | `MbBox *` | `boxTable` | T: pattern + boxTableOffset or 0 (Actor_ResolveFrameDataPointers) |
| 0x268 | `MbCharDatHead *` | `charData` | T: character pattern block (Slot_BindCharacterData from g_CharPatternData) |
| 0x26C | `MbPatternHeader *` | `patternHeader` | T: current pattern header |
| 0x270 | `MbFrameRecord *` | `frameRecord` | T: current 116-byte frame record |
| 0x274 | `MbAnimFrame *` | `animFrame` | T: same address as frameRecord, the AF half |
| 0x278 | `MbStateFrame *` | `stateFrame` | T: frameRecord + 40, the AS half |
| 0x27C | `MbIfRecord *` | `ifTable` | T: pattern + ifTableOffset or 0 |
| 0x280 | `MbEfRecord *` | `efTable` | T: pattern + efTableOffset or 0 |
| 0x284 | `__int16 *` | `boxIndex` | T: frameRecord + 96, the 10 box indices (slots 8 and 9 are two attack boxes in MB) |
| 0x288 | `void *` | `partsData` | T: character parts blob (Slot_BindCharacterData from g_CharPartsData) |
| 0x28C | `void *` | `cgData` | T: character CG blob (Slot_BindCharacterData from g_CharCgData) |
| 0x290 | `MbFighterSlot *` | `fighterSlot` | T: pointer to the owning fighter slot (Slot_InitPointCharacterForRound stores the slot itself); 0 for child objects |
| 0x294 | `MbActor *` | `teamPartnerActor` | T: the other member of the team (Players_ProcessSlotSwap sets it on the entering character to the leaving one); MaidsPartner_UpdateReserveAI mirrors life/meter/gauges from it |
| 0x298 | `int` | `actionTicks` | T: ticks since the action started (ObjRunActionScript increments, Actor_OnActionChanged clears) |
| 0x29C | `unsigned char` | `ranThisTick` | T: ObjRunActionScript sets 1 once it passed all gates, 0 at its start |
| 0x29D | `unsigned char` | `unused_29D[3]` | U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions |

Evidence scan (typed ctree + raw iterator scan over the 763 functions of 0x401000..0x457000): 30 fields/runs are U (`unused_*`: 0x03, 0x0D, 0x10, 0x13, 0x15, 0x1D, 0x3E, 0x45..47, 0x4D..4F, 0x68..69, 0x6D..6F, 0x79..7B, 0xA2..A3, 0xDB, 0xDE, 0xE0, 0xF9, 0x109, 0x110, 0x117, 0x125, 0x13B, 0x149, 0x169, 0x172..173, 0x245..247, 0x263, 0x29D..29F);
write-only fields (`spawnFlagsC`, `posXAfterIntegrate`, `writeOnly5000_74`, `counterHitSide`, `hitStageAtHit`, `writeOnlyZero_115`, `requestParam`, `writeOnlyFF_25C`, `actionEnded`) and read-only fields (`cameraExcludeFlag`, `posZ`) are tagged in their comments.

### 6.4 Per-tick pipeline (T, `MainGameLoop` 0x439C10 battle case)

`Players_ProcessSlotSwap` 0x437620 (partner swap: toggles `g_PlayerSlotRecords[team].pointSlotIndex`, sets `swapOutTimer = 1` on the leaving character, copies life/gauges to the entering one, links `teamPartnerActor`) -> `Characters_ResolveFrameDataAll` 0x43D7E0 (`Actor_ResolveFrameDataPointers` for every active slot) ->
`Camera_UpdateFollowActiveCharacters` -> `Battle_RunObjectHitDetection` 0x4160E0 -> `Battle_ApplyPendingHitsAll` 0x43DE60 (`Battle_ResolveHit` 0x43FF40 per slot) -> `Battle_RunFrameIFsPhase0All` / `EffectObjects_RunFrameIFsAll` (`Character_RunFrameIFs` 0x429E70 -> `Actor_RunFrameIfRecord`) -> `Battle_TickFrameTimers` -> `Round_IntroStateMachine` 0x441B70 / `Battle_RoundStateMachine` 0x441DB0 ->
`Battle_UpdateAllCharactersInputAndAI` 0x43DB20 (per slot: `Input_ReadPlayerDirAndButtons` or `CPU_RunAiForSlot`, `Replay_RecordOrPlayActorInput`, `CommandBuffer_PushInput`, `Character_ProcessPlayerInput` 0x428930 / `Fighter_ScanCommandMoves`, then `ObjRunActionScript` 0x426120 (frame stepping, pending pattern/frame, `ObjEnterCurrentAction` 0x425A30) and `ObjIntegrateMotion` 0x427070, `Fighter_BankPendingHeatGain`, `MaidsPartner_UpdateReserveAI` 0x4285F0 for partners) ->
`Characters_ResolvePushboxAll` / `EffectObjects_ResolvePushboxAll` (`Actor_ResolvePushboxCollisions` 0x43EB60) -> `Battle_RunCharacterHitDetection` 0x43DD80 (`Character_RunFrameIFs` phase 1, `ObjCheckReflectBoxVsAttacks` 0x43F550, `Hitbox_DetectAttackHitsForAttacker` 0x43EE70, `Hitbox_DetectThrowGrabForAttacker` 0x43F210 fill `hitAt/hitRect/hitAttacker`) ->
`EffectObjects_UpdateAll` 0x415E30 (objects: `ObjRunActionScript`, `ObjIntegrateMotion`, homing) -> `Characters_RunFrameEFsAll` / `EffectObjects_RunFrameEFsAll` (`Character_RunFrameEFs` 0x415910) -> `Characters_DrawAll` 0x43DEF0 (`Character_Draw` 0x435EC0 -> `Draw_ActorSpriteScratch` 0x4333B0) -> `EffectObjects_DrawAll` -> `SysEffects_UpdateAndDrawAll` -> HUD.
The loader data is consumed only through `actor.charData / patternHeader / frameRecord / animFrame / stateFrame / boxTable / boxIndex / ifTable / efTable / partsData / cgData`.

### 6.5 Team record (`MbTeamRecord`, `g_PlayerSlotRecords[4]`, 68 bytes)

| off | type | name | evidence / meaning |
|---|---|---|---|
| 0x000 | `int` | `pointSlotIndex` | T: slot index of the team's on-field character (Players_ProcessSlotSwap toggles it) |
| 0x004 | `int` | `partnerAiState` | T: MaidsPartner_UpdateReserveAI state machine |
| 0x008 | `int` | `partnerPresent` | T: 1 when a second character was loaded for the team |
| 0x00C | `int` | `bossFlag` | T: 1 when the third CHARSEL string is "1" (single boss-type fighter) |
| 0x010 | `int` | `score` | T: WinScreen_Update adds the round score |
| 0x014 | `int` | `teamStat_14` | I: best value copied into a per-character record by Battle_RoundStateMachine |
| 0x018 | `int` | `hitsTaken` | T: ComboRecord_RegisterHit increments it for the victim team on every hit not flagged AT.flagsB & 4 |
| 0x01C | `int` | `teamStat_1C` | I: best value copied by Battle_RoundStateMachine |
| 0x020 | `int` | `teamStat_20` | I |
| 0x024 | `int` | `teamStat_24` | I |
| 0x028 | `int` | `evadesPerformed` | T: Hitbox_DetectAttackHitsForAttacker increments it when an attack is evaded (attackResult bit 3) |
| 0x02C | `int` | `counterHitsLanded` | T: Battle_ResolveHit increments it next to counterHitSide = 1 |
| 0x030 | `int` | `roundFrameTotal` | T: Battle_RoundStateMachine adds g_RoundFrameCounter |
| 0x034 | `int` | `teamStat_34` | I: story / continue counter |
| 0x038 | `int` | `continuesUsed` | I: ContinueScene_Update increments it; with teamStat_34 it is tested for 'first stage' |
| 0x03C | `int` | `guardEnabled` | I: Battle_ResolveHit skips the guard logic when 0 (training-mode guard setting) |
| 0x040 | `int` | `damageScalePercent` | I: read by Damage_ScaleByDifficultyAndTeam for victim and attacker team |

## Appendix A. GOF1 actor (656 B) to MB actor (672 B) offset map

Same-name fields only differ by position; GOF1 `reserveGauge/reservePending/superMoveState/superMoveFlag` became `heatGauge/pendingHeatGain/heatState/heatCancelPending`, `lifeTimer` and `aiReactCooldown` and `hitRecoverFrames` are `unused_*` in MB, `unused_1C` became `spawnFlagsC`, `unreferenced_80/70` became `posZ` / `writeOnly5000_74`. MB-only fields:
`knockCarryVelX` 0xB8, `knockCarryPattern` 0xC0, `launcherState` 0xDF, `guardInputWindow` 0xED, `jumpCancelUsed` 0xF5, `tintTimer` 0x108, `statusEffectParam` 0x10A, `swapOutTimer` 0x162, `teamPartnerActor` 0x294, `heatTimer` 0x64, `heatLastGain` 0x6A.
GOF1 `superCancelFlag` (0xD2) has no MB counterpart; GOF1 `hitStage/juggleState/hitReactionId/hitStageAtHit` are 16-bit and became 8-bit (0x10C..0x10F). Offsets shift by +4 after 0x68, +8 after 0xB4, +10 after 0xB8, +11..14 in 0xD1..0xEB, +12..13 in 0xE2..0xEA, +14 after 0xEC, +16 from the hit queue (0x164 -> 0x174) to the end; the table is exact:

| GOF1 off | GOF1 name | MB off | MB name |
|---|---|---|---|
| 0x000 | `slotIndex` | 0x000 | `slotIndex` |
| 0x001 | `characterId` | 0x001 | `characterId` |
| 0x002 | `controlType` | 0x002 | `controlType` |
| 0x004 | `objKind` | 0x004 | `objKind` |
| 0x005 | `paletteIndex` | 0x005 | `paletteIndex` |
| 0x006 | `pattern` | 0x006 | `pattern` |
| 0x007 | `frame` | 0x007 | `frame` |
| 0x008 | `spriteId` | 0x008 | `spriteId` |
| 0x00A | `ifRunState` | 0x00A | `ifRunState` |
| 0x00B | `efRunState` | 0x00B | `efRunState` |
| 0x00C | `airReactionActive` | 0x00C | `airReactionActive` |
| 0x00E | `frameTicks` | 0x00E | `frameTicks` |
| 0x010 | `lifeTimer` | 0x010 | `unused_10` |
| 0x012 | `animationEnded` | 0x012 | `animationEnded` |
| 0x014 | `loopCounter` | 0x014 | `loopCounter` |
| 0x016 | `spawnFlagsA` | 0x016 | `spawnFlagsA` |
| 0x018 | `spawnFlagsB` | 0x018 | `spawnFlagsB` |
| 0x01A | `homingMode` | 0x01A | `homingMode` |
| 0x01B | `homingBaseAction` | 0x01B | `homingBaseAction` |
| 0x01C | `unused_1C` | 0x01C | `spawnFlagsC` |
| 0x01E | `homingTick` | 0x01E | `homingTick` |
| 0x020 | `homingTimeout` | 0x020 | `homingTimeout` |
| 0x022 | `homingOffsetX` | 0x022 | `homingOffsetX` |
| 0x024 | `homingOffsetY` | 0x024 | `homingOffsetY` |
| 0x026 | `homingCooldown` | 0x026 | `homingCooldown` |
| 0x028 | `homingParam` | 0x028 | `homingParam` |
| 0x03C | `aiActionTimer` | 0x03C | `aiActionTimer` |
| 0x03E | `aiReactCooldown` | 0x03E | `unused_3E` |
| 0x040 | `aiScriptIndex` | 0x040 | `aiScriptIndex` |
| 0x042 | `aiScriptStep` | 0x042 | `aiScriptStep` |
| 0x044 | `cpuCommandReady` | 0x044 | `cpuCommandReady` |
| 0x048 | `superMeter` | 0x048 | `superMeter` |
| 0x04C | `superLevelCrossMark` | 0x04C | `superLevelCrossMark` |
| 0x050 | `reserveGauge` | 0x050 | `heatGauge` |
| 0x058 | `reservePending` | 0x058 | `pendingHeatGain` |
| 0x05C | `life` | 0x05C | `life` |
| 0x060 | `maxLife` | 0x060 | `maxLife` |
| 0x066 | `superMoveState` | 0x066 | `heatState` |
| 0x067 | `superMoveFlag` | 0x067 | `heatCancelPending` |
| 0x068 | `clearHistoryOnAction` | 0x06C | `clearHistoryOnAction` |
| 0x06C | `guardGauge` | 0x070 | `guardGauge` |
| 0x070 | `unreferenced_70` | 0x074 | `writeOnly5000_74` |
| 0x074 | `guardRegenDelay` | 0x078 | `guardRegenDelay` |
| 0x078 | `posX` | 0x07C | `posX` |
| 0x07C | `posY` | 0x080 | `posY` |
| 0x080 | `unreferenced_80` | 0x084 | `posZ` |
| 0x084 | `posXAfterIntegrate` | 0x088 | `posXAfterIntegrate` |
| 0x088 | `posXAtTickStart` | 0x08C | `posXAtTickStart` |
| 0x08C | `posYAtTickStart` | 0x090 | `posYAtTickStart` |
| 0x090 | `velX` | 0x094 | `velX` |
| 0x094 | `velY` | 0x098 | `velY` |
| 0x098 | `accelX` | 0x09C | `accelX` |
| 0x09A | `accelY` | 0x09E | `accelY` |
| 0x09C | `maxVelX` | 0x0A0 | `maxVelX` |
| 0x0A0 | `knockVelX` | 0x0A4 | `knockVelX` |
| 0x0A4 | `knockVelY` | 0x0A8 | `knockVelY` |
| 0x0A8 | `knockAccelX` | 0x0AC | `knockAccelX` |
| 0x0AA | `knockAccelY` | 0x0AE | `knockAccelY` |
| 0x0AC | `inertiaX` | 0x0B0 | `inertiaX` |
| 0x0B0 | `carriedVelX` | 0x0B4 | `carriedVelX` |
| 0x0B4 | `inertiaDecay` | 0x0BC | `inertiaDecay` |
| 0x0B6 | `carryPattern` | 0x0BE | `carryPattern` |
| 0x0B8 | `grabDrawOffsetX0` | 0x0C2 | `grabDrawOffsetX0` |
| 0x0BA | `grabDrawOffsetY0` | 0x0C4 | `grabDrawOffsetY0` |
| 0x0BC | `zoomOriginX0` | 0x0C6 | `zoomOriginX0` |
| 0x0BE | `zoomOriginY0` | 0x0C8 | `zoomOriginY0` |
| 0x0C0 | `grabDrawOffsetX1` | 0x0CA | `grabDrawOffsetX1` |
| 0x0C2 | `grabDrawOffsetY1` | 0x0CC | `grabDrawOffsetY1` |
| 0x0C4 | `zoomOriginX1` | 0x0CE | `zoomOriginX1` |
| 0x0C6 | `zoomOriginY1` | 0x0D0 | `zoomOriginY1` |
| 0x0C8 | `rotAngleA` | 0x0D2 | `rotAngleA` |
| 0x0CA | `rotAngleB` | 0x0D4 | `rotAngleB` |
| 0x0CC | `clearInertiaRequest` | 0x0D6 | `clearInertiaRequest` |
| 0x0CD | `attackResult` | 0x0D7 | `attackResult` |
| 0x0CE | `ownerAttackResult` | 0x0D8 | `ownerAttackResult` |
| 0x0CF | `drawDepth` | 0x0D9 | `drawDepth` |
| 0x0D0 | `drawModeVariant` | 0x0DA | `drawModeVariant` |
| 0x0D1 | `activeMoveMeterCost` | 0x0DC | `activeMoveMeterCost` |
| 0x0D2 | `superCancelFlag` | - | (no MB counterpart) |
| 0x0D3 | `hitstopFrames` | 0x0E1 | `hitstopFrames` |
| 0x0D4 | `hitstopTrailing` | 0x0E2 | `hitstopTrailing` |
| 0x0D5 | `contactMark` | 0x0E3 | `contactMark` |
| 0x0D6 | `grabState` | 0x0E4 | `grabState` |
| 0x0D7 | `inputLock` | 0x0E5 | `inputLock` |
| 0x0D8 | `freezeTimerA` | 0x0E6 | `freezeTimerA` |
| 0x0D9 | `freezeTimerB` | 0x0E7 | `freezeTimerB` |
| 0x0DA | `koFlag` | 0x0E8 | `koFlag` |
| 0x0DB | `cameraExcludeFlag` | 0x0E9 | `cameraExcludeFlag` |
| 0x0DC | `attackHitsLeft` | 0x0EA | `attackHitsLeft` |
| 0x0DD | `guardState` | 0x0EB | `guardState` |
| 0x0DE | `cpuGuardRequest` | 0x0EC | `cpuGuardRequest` |
| 0x0E2 | `recoverMode` | 0x0EE | `recoverMode` |
| 0x0E3 | `unused_E3` | 0x0EF | `airTechLatch` |
| 0x0E4 | `bounceCount` | 0x0F0 | `bounceCount` |
| 0x0E5 | `recoveryTimer` | 0x0F1 | `recoveryTimer` |
| 0x0E6 | `throwTechSeen` | 0x0F2 | `throwTechSeen` |
| 0x0E7 | `stageEdgeSide` | 0x0F3 | `stageEdgeSide` |
| 0x0E8 | `airComboStarterLatch` | 0x0F4 | `airComboStarterLatch` |
| 0x0E9 | `throwTechAllowed` | 0x0F6 | `throwTechAllowed` |
| 0x0EA | `inputBlockFlag` | 0x0F7 | `inputBlockFlag` |
| 0x0EB | `counterHitSide` | 0x0F8 | `counterHitSide` |
| 0x0EC | `scriptTimer` | 0x0FA | `scriptTimer` |
| 0x0F6 | `hitQueueClear` | 0x104 | `hitQueueClear` |
| 0x0F7 | `hitQueueCount` | 0x106 | `hitQueueCount` |
| 0x0F8 | `tintMode` | 0x107 | `tintMode` |
| 0x0F9 | `shakeTimer` | 0x105 | `shakeTimer` |
| 0x0FA | `hitStage` | 0x10C | `hitStage` |
| 0x0FC | `juggleState` | 0x10D | `juggleState` |
| 0x0FE | `hitReactionId` | 0x10E | `hitReactionId` |
| 0x100 | `hitStageAtHit` | 0x10F | `hitStageAtHit` |
| 0x102 | `hitRecoverFrames` | 0x110 | `unused_110` |
| 0x104 | `invulnFlags` | 0x111 | `invulnFlags` |
| 0x105 | `introFinished` | 0x112 | `introFinished` |
| 0x106 | `koMoveType` | 0x113 | `koMoveType` |
| 0x107 | `inputHeldFlags` | 0x114 | `inputHeldFlags` |
| 0x108 | `unused_108` | 0x115 | `writeOnlyZero_115` |
| 0x109 | `guardRecoveryFlag` | 0x116 | `guardRecoveryFlag` |
| 0x10A | `mashCounter` | 0x118 | `mashCounter` |
| 0x10C | `tobiCount` | 0x11A | `tobiCount` |
| 0x116 | `dashCount` | 0x124 | `dashCount` |
| 0x118 | `paramVar` | 0x126 | `paramVar` |
| 0x12C | `trailMode` | 0x13A | `trailMode` |
| 0x12E | `decorLifeTotal` | 0x13C | `decorLifeTotal` |
| 0x130 | `decorFlags` | 0x13E | `decorFlags` |
| 0x132 | `decorZoomGrowX` | 0x140 | `decorZoomGrowX` |
| 0x134 | `decorZoomGrowY` | 0x142 | `decorZoomGrowY` |
| 0x136 | `afterimageMode` | 0x144 | `afterimageMode` |
| 0x137 | `afterimageSteps` | 0x145 | `afterimageSteps` |
| 0x138 | `afterimageSpacing` | 0x146 | `afterimageSpacing` |
| 0x139 | `afterimageRestart` | 0x147 | `afterimageRestart` |
| 0x13A | `justGuardActive` | 0x148 | `justGuardActive` |
| 0x13C | `locomotionUsedMask` | 0x14A | `locomotionUsedMask` |
| 0x13E | `commandUsedMask` | 0x14C | `commandUsedMask` |
| 0x14C | `pendingCommandId` | 0x15A | `pendingCommandId` |
| 0x14D | `startedByCommandId` | 0x15B | `startedByCommandId` |
| 0x14E | `hitsTakenInCombo` | 0x15C | `hitsTakenInCombo` |
| 0x150 | `attackHitsConnected` | 0x15E | `attackHitsConnected` |
| 0x152 | `evadeDir` | 0x160 | `evadeDir` |
| 0x153 | `evadeTimer` | 0x161 | `evadeTimer` |
| 0x156 | `flashMode` | 0x164 | `flashMode` |
| 0x157 | `flashTimer` | 0x165 | `flashTimer` |
| 0x158 | `flashDuration` | 0x166 | `flashDuration` |
| 0x159 | `requestParam` | 0x167 | `requestParam` |
| 0x15A | `counterWindowState` | 0x168 | `counterWindowState` |
| 0x15C | `launchVelX` | 0x16A | `launchVelX` |
| 0x15E | `launchVelY` | 0x16C | `launchVelY` |
| 0x160 | `launchAccelX` | 0x16E | `launchAccelX` |
| 0x162 | `launchAccelY` | 0x170 | `launchAccelY` |
| 0x164 | `hitAt` | 0x174 | `hitAt` |
| 0x184 | `hitRect` | 0x194 | `hitRect` |
| 0x204 | `hitAttacker` | 0x214 | `hitAttacker` |
| 0x224 | `lastAttacker` | 0x234 | `lastAttacker` |
| 0x228 | `grabbedBy` | 0x238 | `grabbedBy` |
| 0x22C | `ownerFighter` | 0x23C | `ownerFighter` |
| 0x230 | `linkedActor` | 0x240 | `linkedActor` |
| 0x234 | `inputDir` | 0x244 | `inputDir` |
| 0x238 | `inputButtons` | 0x248 | `inputButtons` |
| 0x23C | `inputButtonsReleased` | 0x24C | `inputButtonsReleased` |
| 0x240 | `teamIndex` | 0x250 | `teamIndex` |
| 0x241 | `endConditionMet` | 0x251 | `endConditionMet` |
| 0x242 | `pendingPattern` | 0x252 | `pendingPattern` |
| 0x244 | `pendingPatternPriority` | 0x254 | `pendingPatternPriority` |
| 0x246 | `pendingFrame` | 0x256 | `pendingFrame` |
| 0x248 | `pendingFramePriority` | 0x258 | `pendingFramePriority` |
| 0x24A | `facingLeft` | 0x25A | `facingLeft` |
| 0x24B | `nextFacingLeft` | 0x25B | `nextFacingLeft` |
| 0x24C | `unused_24C` | 0x25C | `writeOnlyFF_25C` |
| 0x24D | `mirrorHistoryPending` | 0x25D | `mirrorHistoryPending` |
| 0x24E | `inputActionLatch` | 0x25E | `inputActionLatch` |
| 0x24F | `actionChangedFlag` | 0x25F | `actionChangedFlag` |
| 0x250 | `landed` | 0x260 | `landed` |
| 0x251 | `actionEnded` | 0x261 | `actionEnded` |
| 0x252 | `stanceCopy` | 0x262 | `stanceCopy` |
| 0x254 | `boxTable` | 0x264 | `boxTable` |
| 0x258 | `charData` | 0x268 | `charData` |
| 0x25C | `patternHeader` | 0x26C | `patternHeader` |
| 0x260 | `frameRecord` | 0x270 | `frameRecord` |
| 0x264 | `animFrame` | 0x274 | `animFrame` |
| 0x268 | `stateFrame` | 0x278 | `stateFrame` |
| 0x26C | `ifTable` | 0x27C | `ifTable` |
| 0x270 | `efTable` | 0x280 | `efTable` |
| 0x274 | `boxIndex` | 0x284 | `boxIndex` |
| 0x278 | `partsData` | 0x288 | `partsData` |
| 0x27C | `cgData` | 0x28C | `cgData` |
| 0x280 | `fighterSlot` | 0x290 | `fighterSlot` |
| 0x288 | `actionTicks` | 0x298 | `actionTicks` |
| 0x28C | `ranThisTick` | 0x29C | `ranThisTick` |

## Appendix B. IDB changes (`mb.exe.i64`, saved)

* Types: `docs/formats/ida/mb_types.h` parsed in one `idc.parse_decls` call (40 structs, 11 enums, all packed). Because IDA keeps ordinal references, the header is always re-parsed after deleting the old `Mb*` types and every dependent prototype/global is re-applied from a registry; the final state was applied from exactly this file.
* 139 function prototypes carry `Mb*` types (entity arguments `MbActor *`, slot arguments `MbFighterSlot *`, effect records `MbEfRecord *`, command/CPF records). Type propagation was done by a ctree pass that types every argument passed to, or received from, an already typed function until a fixed point (0 conflicts), plus 19 local variables retyped from `int` aliases of a typed pointer (for example `result = slot` in `CommandBuffer_PushInput`).
* 170 `sub_*` functions and 3 generic names were renamed (table below; names transferred from the matching GOF1 function where the matcher found one with >= 0.8 mnemonic similarity and the Hex-Rays bodies agreed, the rest named from their behaviour). 340 `sub_*` remain in the gameplay address range: renderer, DirectDraw/DirectSound glue, menus, statistics and replay screens that neither the loader nor the character state touches.
* 38 globals named and typed (table below). `g_PlayerSlots` / `g_EffectPool` carry their array types so Hex-Rays prints `g_PlayerSlots[i].actor.field`.

### B.1 Typed prototypes

| address | prototype |
|---|---|
| 0x404860 | `MbActor *__cdecl CPU_FindTargetInRange(MbActor *actor, int minDistX, int maxDistX, int minDistY, int maxDistY)` |
| 0x404980 | `int __cdecl CPU_RollGuardReaction(MbCpfFile *cpf)` |
| 0x4049B0 | `int __cdecl CPU_TryEvadeOrTechReaction(MbFighterSlot *slot)` |
| 0x404AE0 | `int __cdecl CPU_ScoreAndSelectAction(MbActor *actor, MbCpfFile *cpf, int scriptRunning, int slotIndex)` |
| 0x404FE0 | `int __cdecl CPU_CpfDirCodeToNumpad(MbCpfStep *step)` |
| 0x405050 | `int __cdecl CPU_ButtonMaskFromStepCode(MbCpfStep *step)` |
| 0x4050D0 | `unsigned __int8 __cdecl CPU_ApplyCpfStepInput(MbFighterSlot *slot, MbCpfFile *cpf, __int16 script, __int16 step, int slotIndex)` |
| 0x4052E0 | `unsigned __int8 __cdecl CPU_StartCpfActionStep(MbFighterSlot *slot, MbCpfFile *cpf, int script, int step, int slotIndex)` |
| 0x4053C0 | `char __cdecl CPU_TickCpfStep(MbFighterSlot *slot, MbCpfFile *cpf, int slotIndex)` |
| 0x405500 | `char __cdecl CPU_RunAiForSlot(MbFighterSlot *slot, int slotIndex)` |
| 0x415910 | `int __cdecl Character_RunFrameEFs(MbActor *actor)` |
| 0x415A90 | `int __cdecl Actor_UpdateHomingObject(MbActor *actor)` |
| 0x416210 | `int __cdecl EffectObject_Draw(MbActor *a1)` |
| 0x4168F0 | `int __cdecl Actor_DrawTintedDebug(MbActor *actor, int a2, int a3, int a4, unsigned __int8 a5, MbActor *a6)` |
| 0x416B30 | `int __cdecl InitChildObject(struct MbActor *parent, struct MbEfRecord *ef, struct MbObjectSlot *objSlot)` |
| 0x416EB0 | `int __cdecl SpawnChildObjectInheritingCharData(MbActor *parent, MbEfRecord *ef)` |
| 0x416F30 | `int __cdecl SpawnChildObjectFromFrameEffect(MbActor *actor, int a2)` |
| 0x416FB0 | `int __cdecl EffectObject_SpawnGhostCopy(MbActor *actor, char a2, __int16 a3, __int16 a4, __int16 a5, __int16 a6)` |
| 0x417170 | `int __cdecl Ef_ResolveSpawnPosition(MbActor *actor, int x, int y, int *outX, int *outY)` |
| 0x417320 | `int __cdecl EfType3_SpawnParticlesById(MbActor *actor, MbEfRecord *ef, int relativeToActor)` |
| 0x417390 | `int __fastcall EfType2_SpawnEffectById(MbActor *actor, int a2, MbActor *a3, int a4, int a5)` |
| 0x417400 | `int __cdecl EfType7_SpawnSystemParticle(MbActor *actor, struct MbEfRecord *ef, int relativeToActor)` |
| 0x417490 | `int __cdecl Actor_SpawnEffectById(MbActor *actor, MbActor *a2, int a3, int a4, int a5, int a6, float a7, char a8)` |
| 0x417D20 | `int __cdecl SpawnEffectParticlesById(__int16 a1, MbEfRecord *a2, int a3, __int16 a4, __int16 a5, __int16 a6, char a7)` |
| 0x419B00 | `int __cdecl SysEffect_SpawnPresetExt(__int16 a1, MbEfRecord *a2, int a3, __int16 a4, int a5, int a6, char a7)` |
| 0x41EF80 | `int __cdecl EF04_PositionGrabbedTarget(MbActor *a1, MbEfRecord *a2)` |
| 0x41F360 | `int __cdecl EF05_DamageGrabbedVictim(MbActor *a3, int a4)` |
| 0x41F6E0 | `int __cdecl EfType6_ActorOp(MbActor *actor, struct MbEfRecord *ef)` |
| 0x420280 | `int __cdecl EfType30_SetActorParams(MbActor *actor, struct MbEfRecord *ef)` |
| 0x4202E0 | `int __cdecl EF09_PlaySound(MbActor *a1, MbEfRecord *a2)` |
| 0x4204D0 | `int __cdecl SpawnChildEffectFromParent(MbActor *actor, struct MbEfRecord *ef)` |
| 0x424F90 | `HGLOBAL __cdecl LoadArchiveAndDecryptIndex(LPCSTR fileName, struct MbArchiveHandle *archive)` |
| 0x425110 | `int __cdecl PKArchive_FindEntryIndex(struct MbArchiveHandle *archive, char *path)` |
| 0x425190 | `int __cdecl DataArchives_GetEntrySize(MbArchiveHandle *archive, const char *path)` |
| 0x4251C0 | `int __cdecl PKArchive_GetEntryOffset(MbArchiveHandle *archive, const char *path)` |
| 0x4251F0 | `unsigned int __cdecl PKArchive_ReadEntryWhole(MbArchiveHandle *archive, const char *path, void *outBuffer)` |
| 0x425210 | `DWORD __cdecl PKArchive_ReadEntry(struct MbArchiveHandle *archive, char *path, LPVOID outBuffer, int startOffset, int bytesToRead)` |
| 0x425690 | `int __cdecl Actor_MirrorInputDirection(MbActor *actor)` |
| 0x425990 | `int __cdecl Actor_OnActionChanged(MbActor *actor)` |
| 0x425A30 | `int __cdecl ObjEnterCurrentAction(MbActor *actor)` |
| 0x425B60 | `int __cdecl ObjRequestAction(MbActor *actor, int pattern, int allowSamePattern, int priority, char requestParam)` |
| 0x425BD0 | `int __cdecl Actor_RequestFrameJump(struct MbActor *actor, int frame, int allowSameFrame, int priority)` |
| 0x425C20 | `int __cdecl Actor_GetOwnerFighter(MbActor *actor)` |
| 0x425C50 | `int __cdecl Actor_ResolveFrameDataPointers(MbActor *a1)` |
| 0x425D20 | `int __cdecl Actor_GetPatternFrameRecords(MbActor *actor, struct MbPatternHeader **outPattern, struct MbFrameRecord **outFrame, struct MbStateFrame **outState, unsigned __int8 pattern, unsigned __int8 frame)` |
| 0x425D90 | `__int16 __cdecl Pattern_GetFrameSpriteId(MbActor *actor, int pattern, int frame)` |
| 0x425DD0 | `BOOL __cdecl Actor_IsWallBounceDue(MbActor *actor)` |
| 0x425E30 | `int __cdecl Actor_WallBounceReaction(MbActor *actor)` |
| 0x425EF0 | `int __cdecl Actor_DecayGuardGauge(MbActor *actor)` |
| 0x425F70 | `int __cdecl Actor_TickHeatGauge(MbActor *actor)` |
| 0x426050 | `int __cdecl Actor_TickWordTimers(MbActor *actor)` |
| 0x4260D0 | `_BYTE *__cdecl Actor_TickFreezeTimers(MbActor *actor)` |
| 0x426120 | `int __cdecl ObjRunActionScript(MbActor *actor)` |
| 0x426AF0 | `int __cdecl Fighter_BankPendingHeatGain(MbFighterSlot *slot)` |
| 0x426B30 | `int __cdecl Actor_ClearVelocity(MbActor *actor)` |
| 0x426B60 | `char __cdecl Actor_ApplyFrameMotionFlags(MbActor *actor)` |
| 0x426FF0 | `int __cdecl ObjScreenEdgeOverlap(MbActor *actor)` |
| 0x427030 | `int __cdecl Actor_GetScreenSideOutside(MbActor *actor)` |
| 0x427070 | `int __cdecl ObjIntegrateMotion(MbActor *actor)` |
| 0x427310 | `int __cdecl Actor_ClampToStageEdge(MbActor *actor)` |
| 0x427700 | `unsigned __int8 __cdecl SetCharacterDrawPriority(MbActor *actor, int a2)` |
| 0x427870 | `int __cdecl Character_DetectNearbyEnemyAttack(MbActor *a1)` |
| 0x427980 | `char *__cdecl Actor_FindNearestFighter(struct MbActor *actor, int *distanceOut)` |
| 0x4279F0 | `MbActor *__cdecl CPU_FindNearestEnemyDistance(MbActor *actor, int *distX, int *distY)` |
| 0x427A90 | `int __cdecl Actor_IsTeamPointCharacter(MbActor *actor)` |
| 0x427AD0 | `unsigned __int8 __cdecl Pattern_GetMoveLevel(MbActor *actor, int pattern)` |
| 0x427AF0 | `int __cdecl Pattern_IsCgDoubleRes(MbActor *actor, int pattern)` |
| 0x427B10 | `int __cdecl Pattern_GetLinearFilterBit(MbActor *actor, int pattern)` |
| 0x427B30 | `int __cdecl Actor_TryStartPartnerCall(MbActor *actor)` |
| 0x427BD0 | `int __cdecl Fighter_CheckPartnerCallInput(MbActor *actor)` |
| 0x427C30 | `int __cdecl Actor_StartTechRecovery(MbActor *actor, int vectorRow)` |
| 0x427D30 | `int __cdecl Actor_StartTechRecoveryInPlace(MbActor *actor, int unusedRow)` |
| 0x427DF0 | `char __cdecl Actor_CanAirTech(MbActor *actor)` |
| 0x427E60 | `int __cdecl Throw_TryThrowEscape(MbActor *a1)` |
| 0x427FD0 | `int __cdecl Character_ProcessTechInput(MbActor *a1, char a2, char a3)` |
| 0x428180 | `int __cdecl Actor_RequestLocomotionFromInput(MbActor *actor, int buttons, int direction, int stanceCanAct)` |
| 0x4285A0 | `int __cdecl Actor_CountStunMashAndCheckGuardRecovery(MbActor *actor, char buttons)` |
| 0x4285F0 | `char __cdecl MaidsPartner_UpdateReserveAI(MbActor *actor)` |
| 0x428930 | `char __cdecl Character_ProcessPlayerInput(MbActor *a1, char a2, unsigned __int8 a3)` |
| 0x429200 | `int __cdecl Slot_LoadCommandCT(MbFighterSlot *slot, const char *path)` |
| 0x429290 | `int __cdecl Fighter_InvalidateInputHistory(MbFighterSlot *slot)` |
| 0x4292B0 | `int __cdecl CommandSequence_ParseFirstStep(struct MbCtCommand *cmd, int *dirOut, int *buttonsOut, int *cursor, unsigned __int8 *tapFlag)` |
| 0x4293B0 | `int __cdecl CommandSequence_ParseNextStep(struct MbCtCommand *cmd, int *dirOut, int *buttonsOut, int *cursor, unsigned __int8 *tapFlag)` |
| 0x4294C0 | `BOOL __cdecl Fighter_MatchCommandSequence(struct MbFighterSlot *slot, struct MbCtCommand *cmd)` |
| 0x429780 | `int __cdecl Fighter_TryStartCommandMove(MbFighterSlot *slot, int commandIndex)` |
| 0x429AE0 | `int __cdecl Fighter_ScanCommandMoves(MbFighterSlot *slot)` |
| 0x429BE0 | `unsigned __int8 __cdecl Fighter_DetectEvadeDoubleTap(MbFighterSlot *slot)` |
| 0x429DD0 | `int __cdecl Fighter_MatchDoubleTapDirection(MbFighterSlot *slot, int direction)` |
| 0x429E30 | `int __cdecl Fighter_IsCharacterOnField(MbActor *actor, int a2)` |
| 0x429E70 | `int __cdecl Character_RunFrameIFs(MbActor *actor, int a2)` |
| 0x429EE0 | `int __cdecl Actor_JumpToFrame(MbActor *actor, char frame)` |
| 0x429F00 | `int __cdecl Actor_RunFrameIfRecord(MbActor *actor, struct MbIfRecord *ifRec)` |
| 0x42B970 | `int __cdecl Actor_AttackBoxHitsTeammateHurtbox(MbActor *actor)` |
| 0x42F670 | `int __cdecl Slot_BindCharacterData(MbFighterSlot *slot, int dataSlot)` |
| 0x431770 | `_DWORD *__cdecl Box_ApplyDrawModeTransform(MbActor *actor, _DWORD *a2)` |
| 0x4319F0 | `int __cdecl ObjBoxToWorld(MbActor *actor, struct MbRectI32 *rect, int shift)` |
| 0x431EC0 | `int __cdecl Actor_IsTickAllowed(MbActor *actor)` |
| 0x4333B0 | `int __cdecl Draw_ActorSpriteScratch(MbActor *actor, int a2, int a3, int a4, int a5, int a6, int a7, int a8, MbActor *a9, unsigned int a10, int a11, unsigned int a12, __int16 a13, int a14, int a15, int a16, int a17, int a18, char a19, int a20, int a21, int a22)` |
| 0x435AE0 | `unsigned __int8 *__cdecl Actor_SetColorFlash(MbActor *actor, char mode, char frames)` |
| 0x435B00 | `int __cdecl Character_ApplyAddedEffect(MbActor *a1, int a2, __int16 a3)` |
| 0x435B40 | `char __cdecl Actor_DrawHeatAuraAndTintParticles(MbActor *actor)` |
| 0x435EC0 | `int __cdecl Character_Draw(MbActor *actor)` |
| 0x436610 | `int __cdecl AfterImage_ResetHistory(int a1, MbActor *a2)` |
| 0x4366F0 | `int __cdecl AfterImage_PushHistory(int a1, MbActor *a2)` |
| 0x4367D0 | `char __cdecl AfterImage_DrawTrailEntry(int a1, MbActor *a2, int a3)` |
| 0x437760 | `char __cdecl Slot_InitPointCharacterForRound(struct MbFighterSlot *slot, char team, int posX, int posY, char facingLeft)` |
| 0x4378E0 | `char __cdecl FighterSlot_ResetKeepingDataPointers(MbFighterSlot *slot, char team, int startX, int startY, char facingLeft)` |
| 0x437B30 | `int __cdecl Actor_SpawnStageMarkerObject(MbActor *actor, int markerIndex, int force)` |
| 0x43D810 | `int __cdecl CommandBuffer_PushInput(struct MbFighterSlot *slot)` |
| 0x43DAC0 | `int __cdecl Fighter_CanReceiveInput(MbActor *actor)` |
| 0x43DF70 | `int Actor_SetHitstopFrames(struct MbActor *actor, char frames, ...)` |
| 0x43DFC0 | `int __cdecl RectOverlap(struct MbRectI32 *a, struct MbRectI32 *b)` |
| 0x43E0A0 | `BOOL __cdecl Rect_ContainsPoint(struct MbRectI32 *rect, int x, int y)` |
| 0x43E0D0 | `int __cdecl ApplyPushboxSeparation(MbActor *actor, int dx, MbActor *other, int overlap, int step)` |
| 0x43E240 | `int __cdecl Actor_GetBoxSlotIndex(MbActor *actor, int kind)` |
| 0x43E2D0 | `int __cdecl Actor_BoxKindOverlapsActor(MbActor *actor, MbActor *other, int boxKind)` |
| 0x43E570 | `int __cdecl Actor_FindOverlappingTarget(MbActor *actor, int a2, int a3, int a4, int a5, int a6)` |
| 0x43EAD0 | `BOOL __cdecl Actor_PushboxHitsRect(MbActor *actor, int a2, int a3)` |
| 0x43EB60 | `void __cdecl Actor_ResolvePushboxCollisions(MbActor *actor)` |
| 0x43EE70 | `char __cdecl Hitbox_DetectAttackHitsForAttacker(MbActor *actor)` |
| 0x43F210 | `char __cdecl Hitbox_DetectThrowGrabForAttacker(MbActor *actor)` |
| 0x43F550 | `char __cdecl ObjCheckReflectBoxVsAttacks(MbActor *actor)` |
| 0x43FCB0 | `int __cdecl Actor_TryGrabFighter(MbActor *grabber, MbActor *victim)` |
| 0x43FE70 | `BOOL __cdecl Fighter_HasAtMostOneUsedMove(MbActor *actor)` |
| 0x43FEE0 | `int __cdecl Actor_RequestHitReactionAction(MbActor *actor, int pattern)` |
| 0x43FF40 | `char __cdecl Battle_ResolveHit(MbActor *a1)` |
| 0x441790 | `int __cdecl Actor_StartKnockback(MbActor *victim, int reaction, int stunFrames, int flags, int flagsB, int guarded, int attackerFacing)` |
| 0x441AE0 | `BOOL __cdecl Actor_CanGuardAttack(MbActor *actor, struct MbAtRecord *at, unsigned __int8 attackKind, int mode)` |
| 0x442640 | `char __cdecl Character_FaceNearestOpponent(MbActor *a1)` |
| 0x442780 | `int __cdecl Actor_FaceTeamPartner(MbActor *actor)` |
| 0x4431F0 | `unsigned int __cdecl AddSuperMeter(struct MbActor *actor, int amount)` |
| 0x443270 | `int __cdecl ComboRecord_RegisterHit(MbActor *attacker, MbActor *victim, struct MbAtRecord *at)` |
| 0x443320 | `int __cdecl Actor_AddHeatGauge(MbActor *actor, MbActor *source, int gain, int guardState)` |
| 0x443350 | `MbActor *__cdecl Actor_AdjustGuardGauge(MbActor *actor, int amount, int mode)` |
| 0x4433C0 | `int __cdecl Damage_ScaleByGuardGaugeAndStance(MbActor *victim, MbActor *attacker, int baseDamage)` |
| 0x443560 | `int __cdecl Damage_ScaleByDifficultyAndTeam(MbActor *victim, MbActor *attacker, int baseDamage)` |
| 0x443690 | `int __cdecl Damage_ComputeFromAt(MbActor *victim, MbActor *attacker, MbAtRecord *at)` |
| 0x4436B0 | `int __cdecl TeamScore_AddForDamage(MbActor *attacker, int damage)` |
| 0x443710 | `int __cdecl Damage_ComputeChip(MbActor *victim, MbActor *attacker, int baseDamage)` |

### B.2 Renamed functions (previous name -> name)

| address | previous name | name |
|---|---|---|
| 0x402A20 | `sub_402A20` | `Battle_LoadFaceCutinsAndVoices` |
| 0x404400 | `sub_404400` | `Battle_BindPreloadedCharacterSlots` |
| 0x404690 | `sub_404690` | `CPU_GuardPercentByDifficulty` |
| 0x4046B0 | `sub_4046B0` | `CPU_DecisionCooldownByDifficulty` |
| 0x4046F0 | `sub_4046F0` | `CPU_TickScriptCooldowns` |
| 0x404720 | `sub_404720` | `CPU_ReadCpfFileToBuffer` |
| 0x404860 | `sub_404860` | `CPU_FindTargetInRange` |
| 0x404980 | `sub_404980` | `CPU_RollGuardReaction` |
| 0x4049B0 | `sub_4049B0` | `CPU_TryEvadeOrTechReaction` |
| 0x405050 | `sub_405050` | `CPU_ButtonMaskFromStepCode` |
| 0x4053C0 | `sub_4053C0` | `CPU_TickCpfStep` |
| 0x405500 | `sub_405500` | `CPU_RunAiForSlot` |
| 0x405FE0 | `sub_405FE0` | `Surface_CreateTextureSlot` |
| 0x4065A0 | `sub_4065A0` | `Surface_CreateTextureSlotNoFlags` |
| 0x407310 | `sub_407310` | `D3D7_ResetRenderStates` |
| 0x408540 | `sub_408540` | `RenderQuad_InitDefaults` |
| 0x408610 | `sub_408610` | `RenderRect_InitDefaults` |
| 0x4098E0 | `sub_4098E0` | `DDrawSetModeAndTiming` |
| 0x40AA10 | `sub_40AA10` | `DDrawCreateSetCoopAndDisplayMode` |
| 0x40DA30 | `sub_40DA30` | `Surface_BlitIndexedBitmapToSlot` |
| 0x40E080 | `sub_40E080` | `Surface_BuildChannelConversionTables` |
| 0x40ED80 | `sub_40ED80` | `CharFile_LoadWmt` |
| 0x40EE50 | `sub_40EE50` | `WinScreen_PickWmtEntry` |
| 0x412DF0 | `sub_412DF0` | `ClassifyJoystickAxis` |
| 0x412E20 | `sub_412E20` | `ReadJoystickState` |
| 0x413980 | `sub_413980` | `Bgm_StartStreamThread` |
| 0x413B40 | `sub_413B40` | `BgmStream_FillBuffer` |
| 0x413DC0 | `sub_413DC0` | `BgmStreamThread` |
| 0x414F80 | `sub_414F80` | `StreamingSound_StopAndRelease` |
| 0x415170 | `sub_415170` | `StopStreamingSound` |
| 0x415A90 | `sub_415A90` | `Actor_UpdateHomingObject` |
| 0x416B30 | `sub_416B30` | `InitChildObject` |
| 0x416EB0 | `sub_416EB0` | `SpawnChildObjectInheritingCharData` |
| 0x416F30 | `sub_416F30` | `SpawnChildObjectFromFrameEffect` |
| 0x417170 | `sub_417170` | `Ef_ResolveSpawnPosition` |
| 0x417320 | `sub_417320` | `EfType3_SpawnParticlesById` |
| 0x417390 | `sub_417390` | `EfType2_SpawnEffectById` |
| 0x417400 | `sub_417400` | `EfType7_SpawnSystemParticle` |
| 0x417490 | `sub_417490` | `Actor_SpawnEffectById` |
| 0x417D20 | `sub_417D20` | `SpawnEffectParticlesById` |
| 0x41F6E0 | `sub_41F6E0` | `EfType6_ActorOp` |
| 0x420280 | `sub_420280` | `EfType30_SetActorParams` |
| 0x4204D0 | `sub_4204D0` | `SpawnChildEffectFromParent` |
| 0x4206B0 | `sub_4206B0` | `Vec3_ToAngles` |
| 0x424DB0 | `sub_424DB0` | `Decompress_EX3_File` |
| 0x4251C0 | `sub_4251C0` | `PKArchive_GetEntryOffset` |
| 0x425690 | `sub_425690` | `Actor_MirrorInputDirection` |
| 0x425990 | `sub_425990` | `Actor_OnActionChanged` |
| 0x425A30 | `sub_425A30` | `ObjEnterCurrentAction` |
| 0x425B60 | `sub_425B60` | `ObjRequestAction` |
| 0x425BD0 | `sub_425BD0` | `Actor_RequestFrameJump` |
| 0x425C20 | `sub_425C20` | `Actor_GetOwnerFighter` |
| 0x425D20 | `sub_425D20` | `Actor_GetPatternFrameRecords` |
| 0x425D90 | `sub_425D90` | `Pattern_GetFrameSpriteId` |
| 0x425DD0 | `sub_425DD0` | `Actor_IsWallBounceDue` |
| 0x425E30 | `sub_425E30` | `Actor_WallBounceReaction` |
| 0x425EF0 | `sub_425EF0` | `Actor_DecayGuardGauge` |
| 0x425F70 | `Character_TickGuardGauge` | `Actor_TickHeatGauge` |
| 0x426050 | `sub_426050` | `Actor_TickWordTimers` |
| 0x4260D0 | `sub_4260D0` | `Actor_TickFreezeTimers` |
| 0x426120 | `sub_426120` | `ObjRunActionScript` |
| 0x426AF0 | `sub_426AF0` | `Fighter_BankPendingHeatGain` |
| 0x426B30 | `sub_426B30` | `Actor_ClearVelocity` |
| 0x426B60 | `sub_426B60` | `Actor_ApplyFrameMotionFlags` |
| 0x426FF0 | `sub_426FF0` | `ObjScreenEdgeOverlap` |
| 0x427030 | `sub_427030` | `Actor_GetScreenSideOutside` |
| 0x427070 | `sub_427070` | `ObjIntegrateMotion` |
| 0x427310 | `sub_427310` | `Actor_ClampToStageEdge` |
| 0x427390 | `sub_427390` | `Camera_UpdateFocusXFromFighters` |
| 0x427410 | `sub_427410` | `Camera_UpdateFocusYZFromFighters` |
| 0x427980 | `sub_427980` | `Actor_FindNearestFighter` |
| 0x4279F0 | `sub_4279F0` | `CPU_FindNearestEnemyDistance` |
| 0x427A90 | `sub_427A90` | `Actor_IsTeamPointCharacter` |
| 0x427AD0 | `sub_427AD0` | `Pattern_GetMoveLevel` |
| 0x427AF0 | `sub_427AF0` | `Pattern_IsCgDoubleRes` |
| 0x427B10 | `sub_427B10` | `Pattern_GetLinearFilterBit` |
| 0x427B30 | `sub_427B30` | `Actor_TryStartPartnerCall` |
| 0x427BD0 | `sub_427BD0` | `Fighter_CheckPartnerCallInput` |
| 0x427C30 | `sub_427C30` | `Actor_StartTechRecovery` |
| 0x427D30 | `sub_427D30` | `Actor_StartTechRecoveryInPlace` |
| 0x427DF0 | `sub_427DF0` | `Actor_CanAirTech` |
| 0x428180 | `sub_428180` | `Actor_RequestLocomotionFromInput` |
| 0x4285A0 | `sub_4285A0` | `Actor_CountStunMashAndCheckGuardRecovery` |
| 0x429290 | `sub_429290` | `Fighter_InvalidateInputHistory` |
| 0x4292B0 | `sub_4292B0` | `CommandSequence_ParseFirstStep` |
| 0x4293B0 | `sub_4293B0` | `CommandSequence_ParseNextStep` |
| 0x4294C0 | `sub_4294C0` | `Fighter_MatchCommandSequence` |
| 0x429780 | `sub_429780` | `Fighter_TryStartCommandMove` |
| 0x429AE0 | `sub_429AE0` | `Fighter_ScanCommandMoves` |
| 0x429BE0 | `sub_429BE0` | `Fighter_DetectEvadeDoubleTap` |
| 0x429DD0 | `sub_429DD0` | `Fighter_MatchDoubleTapDirection` |
| 0x429E30 | `sub_429E30` | `Fighter_IsCharacterOnField` |
| 0x429EE0 | `sub_429EE0` | `Actor_JumpToFrame` |
| 0x429F00 | `sub_429F00` | `Actor_RunFrameIfRecord` |
| 0x42B970 | `sub_42B970` | `Actor_AttackBoxHitsTeammateHurtbox` |
| 0x42DC20 | `sub_42DC20` | `Load_Background_Graphics_For_Stage` |
| 0x42E660 | `sub_42E660` | `Find_File_In_Archives` |
| 0x42E9A0 | `sub_42E9A0` | `Load_BMP_From_Archive` |
| 0x42EBC0 | `sub_42EBC0` | `Load_Effect_DAT` |
| 0x42EC20 | `sub_42EC20` | `SpriteDataSlot_LoadCharacterDatAndUpload` |
| 0x42EC50 | `SpriteDataSlot_LoadHeaderFile` | `SpriteDataSlot_LoadPatternOverrideDof` |
| 0x42ED30 | `sub_42ED30` | `SpriteDataSlot_LoadCharacterDat` |
| 0x42EEE0 | `sub_42EEE0` | `Surface_UploadA8R8G8B8Square` |
| 0x42F0C0 | `sub_42F0C0` | `SpriteDataSlot_UploadPartsAndCG` |
| 0x42F620 | `sub_42F620` | `CharData_FreeAll` |
| 0x42F670 | `sub_42F670` | `Slot_BindCharacterData` |
| 0x42FCA0 | `sub_42FCA0` | `HitVectors_LoadFromVectorTxt` |
| 0x42FE00 | `sub_42FE00` | `CSS_BuildRandomPickOrder` |
| 0x42FEA0 | `sub_42FEA0` | `Screen_DrawNoiseOverlay` |
| 0x430050 | `sub_430050` | `Hud_SetCounterHitIndicator` |
| 0x430080 | `sub_430080` | `Font_DrawText_Gof1` |
| 0x4304E0 | `sub_4304E0` | `Render_QueueClipRect` |
| 0x430E10 | `sub_430E10` | `Render_QueuePartSprite` |
| 0x4316D0 | `sub_4316D0` | `World_ToScreenScaled` |
| 0x431730 | `sub_431730` | `Screen_ToWorldPos` |
| 0x431770 | `sub_431770` | `Box_ApplyDrawModeTransform` |
| 0x4319F0 | `sub_4319F0` | `ObjBoxToWorld` |
| 0x431D50 | `sub_431D50` | `IsMenuCancelPressed` |
| 0x431E80 | `sub_431E80` | `PlayerTimerBank_Set` |
| 0x431EC0 | `sub_431EC0` | `Actor_IsTickAllowed` |
| 0x4332C0 | `sub_4332C0` | `RotAngle_LerpToDegrees` |
| 0x435AE0 | `sub_435AE0` | `Actor_SetColorFlash` |
| 0x435B40 | `sub_435B40` | `Actor_DrawHeatAuraAndTintParticles` |
| 0x437720 | `sub_437720` | `Match_ClearEffectPoolsAndTeamCounters` |
| 0x4378E0 | `sub_4378E0` | `FighterSlot_ResetKeepingDataPointers` |
| 0x437B30 | `sub_437B30` | `Actor_SpawnStageMarkerObject` |
| 0x437F20 | `sub_437F20` | `EffectPools_ClearAll` |
| 0x43A650 | `sub_43A650` | `DrawTextGDIOnDDrawSurface` |
| 0x43B970 | `sub_43B970` | `OpeningSub2_WarningScreen` |
| 0x43BC50 | `sub_43BC50` | `OpeningDemoScriptStep` |
| 0x43CBB0 | `sub_43CBB0` | `OpeningSub4_PlayDemoScript` |
| 0x43DAC0 | `sub_43DAC0` | `Fighter_CanReceiveInput` |
| 0x43DF70 | `sub_43DF70` | `Actor_SetHitstopFrames` |
| 0x43DFC0 | `sub_43DFC0` | `RectOverlap` |
| 0x43E010 | `sub_43E010` | `RectIntersection` |
| 0x43E0A0 | `sub_43E0A0` | `Rect_ContainsPoint` |
| 0x43E0D0 | `sub_43E0D0` | `ApplyPushboxSeparation` |
| 0x43E240 | `sub_43E240` | `Actor_GetBoxSlotIndex` |
| 0x43E2D0 | `sub_43E2D0` | `Actor_BoxKindOverlapsActor` |
| 0x43E570 | `sub_43E570` | `Actor_FindOverlappingTarget` |
| 0x43EA10 | `sub_43EA10` | `Fighters_PointHitsHurtbox` |
| 0x43EAD0 | `sub_43EAD0` | `Actor_PushboxHitsRect` |
| 0x43EE50 | `sub_43EE50` | `AtClass_HitsInvulnClass` |
| 0x43F550 | `sub_43F550` | `ObjCheckReflectBoxVsAttacks` |
| 0x43FCB0 | `sub_43FCB0` | `Actor_TryGrabFighter` |
| 0x43FE70 | `sub_43FE70` | `Fighter_HasAtMostOneUsedMove` |
| 0x43FEE0 | `sub_43FEE0` | `Actor_RequestHitReactionAction` |
| 0x441790 | `sub_441790` | `Actor_StartKnockback` |
| 0x441AE0 | `sub_441AE0` | `Actor_CanGuardAttack` |
| 0x441D20 | `sub_441D20` | `Score_BonusByRoundTime` |
| 0x442780 | `sub_442780` | `Actor_FaceTeamPartner` |
| 0x442870 | `sub_442870` | `ComboRecords_TickAndShift` |
| 0x4431F0 | `sub_4431F0` | `AddSuperMeter` |
| 0x443270 | `sub_443270` | `ComboRecord_RegisterHit` |
| 0x443320 | `Character_AddGuardGauge` | `Actor_AddHeatGauge` |
| 0x443350 | `sub_443350` | `Actor_AdjustGuardGauge` |
| 0x4433C0 | `sub_4433C0` | `Damage_ScaleByGuardGaugeAndStance` |
| 0x443560 | `sub_443560` | `Damage_ScaleByDifficultyAndTeam` |
| 0x443690 | `sub_443690` | `Damage_ComputeFromAt` |
| 0x4436B0 | `sub_4436B0` | `TeamScore_AddForDamage` |
| 0x443710 | `sub_443710` | `Damage_ComputeChip` |
| 0x443730 | `sub_443730` | `ScreenZoom_Start` |
| 0x447140 | `sub_447140` | `Sfx_LoadWavFileToVoiceSlot` |
| 0x447360 | `sub_447360` | `Voice_StopSlot` |
| 0x447380 | `sub_447380` | `Voice_ReleaseSlot` |
| 0x4474E0 | `sub_4474E0` | `Text_AdvanceToNextLine` |
| 0x447510 | `sub_447510` | `Text_SkipWhitespaceAndComments` |
| 0x4475E0 | `sub_4475E0` | `Text_AdvanceToNextToken` |
| 0x44B550 | `sub_44B550` | `Replay_OpenRpdFile` |
| 0x44C830 | `sub_44C830` | `BattlePauseMenuTick` |
| 0x44CBF0 | `sub_44CBF0` | `Practice_LoadDummyCpf` |
| 0x44D870 | `sub_44D870` | `Replay_LoadCharactersForPlayback` |
| 0x44E1B0 | `sub_44E1B0` | `MainWindowProc` |

### B.3 Typed / renamed globals

| address | global | type |
|---|---|---|
| 0x46CCEC | `g_AtClassVsInvulnHitTable` | `unsigned __int8[5][5]` |
| 0x46F8E8 | `g_DummyCpfPathTable` | `const char *[4]` |
| 0x477F58 | `g_CpfBuffers` | `MbCpfFile[4]` |
| 0x534D08 | `g_CpuStates` | `MbCpuState[4]` |
| 0x535088 | `g_CpuCandidates` | `MbCpuCandidate[100]` |
| 0x5353A8 | `g_CpuCandidateWeightSum` | `int` |
| 0x79F08C | `g_WmtCount` | `unsigned int` |
| 0x79F0B0 | `g_WmtTable` | `MbWmtRecord[100]` |
| 0x1602630 | `g_EvadeScratchCommand` | `MbCtCommand` |
| 0x1602660 | `g_PartnerCallScratchCommand` | `MbCtCommand` |
| 0x1602690 | `g_DoubleTapScratchCommand` | `MbCtCommand` |
| 0x1602818 | `g_TeamComboRecordCursor` | `int[2]` |
| 0x1602820 | `g_PlayerSlotRecords` | `MbTeamRecord[4]` |
| 0x1602932 | `g_BattleControlPhase` | `unsigned __int8` |
| 0x1602934 | `g_TechVectorTable` | `MbTechVectorRow[7]` |
| 0x1602B50 | `g_TeamHeatBankRequest` | `int[2]` |
| 0x1602B60 | `g_PracticeDummyMode` | `unsigned __int8` |
| 0x16A011C | `g_CharTableCount` | `int` |
| 0x16A01A0 | `g_EffectCharSlot` | `MbFighterSlot` |
| 0x16D1830 | `g_CharPatternData` | `MbCharDatHead *[20]` |
| 0x16D1A40 | `g_HitstopFramesByLevel` | `unsigned __int8[7]` |
| 0x16D1AC8 | `g_ArchiveData00` | `MbArchiveHandle` |
| 0x16D1AE8 | `g_PlayerSlots` | `MbFighterSlot[4]` |
| 0x16D7BC8 | `g_EffectPool` | `MbObjectSlot[1000]` |
| 0x177CC94 | `g_ScreenShakeTimer` | `unsigned __int8` |
| 0x177CCA0 | `g_ScreenScaleShift` | `int` |
| 0x178CAB4 | `g_CharFileIdToTableIndex` | `unsigned __int8[100]` |
| 0x178D440 | `g_HitstopScale` | `float` |
| 0x17DC690 | `g_CharCgData` | `MbCgBlobHead *[20]` |
| 0x181FB88 | `g_CharTable` | `MbCharSelEntry[100]` |
| 0x1880CB8 | `g_CharDatLoadBuffer` | `HGLOBAL` |
| 0x188E4C4 | `g_HitVectorTable` | `MbHitVectorRow[32]` |
| 0x189557C | `g_CharPartsData` | `MbPartsHeader *[20]` |
| 0x18955D0 | `g_CssCharGrid` | `int[100]` |
| 0x1895878 | `g_PkArchives` | `MbArchiveHandle[10]` |
| 0x18F9780 | `g_GlobalCounterBank` | `int[10]` |
| 0x18F9800 | `g_TeamGuardFloorEnabled` | `int[2]` |
| 0x1901014 | `g_PracticeDummyModeApplied` | `int` |

## Appendix C. Not proven / open

* Field meanings tagged I in `mb_types.h`: `launcherState` (values 1/2 inferred from `Battle_ResolveHit` and `Character_ProcessPlayerInput`; pattern 41 is the follow-up the jump-cancel input starts), `swapOutTimer` (value 4095 is tested but no writer was found), `heatTimer` / `heatLastGain` arithmetic, `guardInputWindow`, `tintTimer` / `statusEffectParam`, `teamStat_14/1C/34/3C`, `damageScalePercent`, `guardEnabled`, `continuesUsed`; the semantics of `MbTeamRecord` offsets +0x14, +0x1C, +0x20, +0x24, +0x34, +0x3C are only partially known and `g_ComboRecords` (14-byte HUD combo records) is not typed.
* `writeOnly5000_74`, `teamCopiedOnly_54`, `posZ`, `cameraExcludeFlag`: accessed only as described in their comments; their purpose is unknown.
* `MbCtHeader.knockbackScale` and `damageScale[1..3]`, `ct.flags` bit 3, `MbCpfCondition.unused_40` (one non-zero value), `MbStateFrame.unreadData_04` (16 CIEL frames) and the CG `bound` rectangle are data-bearing but have no reader in mb.exe.
* IF handlers 15, 18, 19, 23, 27..31, 50 and EF type 6 sub-ops 8, 11, 12, 13, 14 are described from the handler bodies, they do not occur in the shipped data (or only a handful of times) and were not exercised in play. Nothing was run: all runtime statements are static (IDA) plus data checks.
* The optional `.dof` pattern override has no sample in any archive; its format is inferred from the loader (a character container whose `[0, patternAreaEnd)` replaces the pattern area).
* The CPU scoring arithmetic (`+10` / `+20` per satisfied condition) is read from `CPU_ScoreAndSelectAction` but was not measured against play.
* `data00.p` (plain MP3 payload) is opened but nothing in the traced paths reads it through the archive API; `EFFECT_.DAT`, `HISUI.WMT`, `HISKOH.WMT`, `HISKOH_C.CT`, `HIS_KOH_C.CT`, `MIYKO_C.CT`, `ARC_C.CT2` are never requested by this build.
* The GOF1 IDB (`gof.exe.i64`) was only read (function matching), never modified.
