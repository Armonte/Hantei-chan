# HA4 (Hantei4 `.DAT`) character format: byte layout, HA6 mapping, audit

Input section for `BG_HA4_RE.md`. Scope: the MBAC (Melty Blood Act Cadenza PC) character container that
`hantei4.exe` (判定くんＲＸ 4.85) edits and `mbacPC.exe` loads. Background/stage files are covered elsewhere.

Evidence tags used below:
- **V-ed**: read or written by hantei4.exe at the cited address (IDA session `hantei4`).
- **V-rt**: consumed by mbacPC.exe at the cited address (IDA session `mbacpc`).
- **V-data**: invariant checked on all 50 MBAC `.DAT` files (`tools/ha4_verify.py`), or measured against the MBAACC HA6
  conversion of the same patterns (`tools/ha4_vs_ha6.py`, 1355 patterns / 21445 frames, 10 characters).
- **I**: inferred.

Kept artifacts (all in git, nothing in /tmp):
- `docs/bg_research/tools/ha4lib.py`: reference parser. Every field is taken from the functions cited here.
- `docs/bg_research/tools/ha4_verify.py`: strict container verifier plus statistics on unused bytes. Output: `data/ha4/ha4_verify_all.txt`.
- `docs/bg_research/tools/ha4_vs_ha6.py`: empirical HA4 to HA6 joint histograms. Output: `data/ha4/ha4_vs_ha6_mapping.txt`.
- `docs/bg_research/tools/han4_parser_audit.py`: runs han4_parser, mbacdat.py and the reference against all 50 files. Output: `data/ha4/han4_parser_audit.txt`.

---

## 1. Byte layout

### 1.0 Key facts

1. HA4 is **not** a tag format. It is a fixed-record binary container: `"Hantei4\0"`, an offset table, packed
   pattern records, then an embedded parts blob, an embedded CG blob, and the pattern names. There are no `PSTR`, `AFST` or
   other tags. Any "tag-based HA4" (`ha4_format.h`, `framedata_load_ha4.cpp` on Hantei-chan's `han4` branch)
   is fiction.
2. One `.DAT` holds **one** character: 256 pattern slots, each with at most 100 frames. It is not "multi-character".
   `CSEL_*.DAT` (character-select idle animations), `EFFECT.DAT` and the boss files use the same container
   (V-data: all 50 files pass the strict check).
3. The per-pattern tables (boxes, AT, IF, EF) are **not deduplicated**. The writer appends every used record
   in frame order, so indices are sequential 0,1,2,... (V-ed `WriteHantei4DatFile` 0x40CF60; V-data 50/50).
4. The game ignores the pattern names. `SpriteDataSlot_Load` 0x43DB20 copies only the pattern image, the parts blob and the CG blob.

### 1.1 File header (0x44 bytes), V-ed `WriteHantei4DatFile` 0x40CF60 / `LoadHantei4DatFile` 0x40EF70, V-rt `SpriteDataSlot_Load` 0x43DB20

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| 0x00 | char[8] | `"Hantei4\0"` (`g_hantei4DatSignature` 0x431EF0) | V-ed |
| 0x08 | 8 B | zero | V-data |
| 0x10 | u32 | version = 1 | V-ed (writer sets `file_buffer[4]=1`) |
| 0x14 | u32 | **partsOff**: end of the pattern area and start of the parts blob. The game copies `[0, partsOff)` as the "pattern image"; `SpriteDataSlot_LoadHeaderFile` 0x445480 uses it the same way. | V-ed, V-rt |
| 0x18 | u32 | **partsSize** (0 when the character has no .pat parts) | V-ed, V-rt |
| 0x1C | u32 | **cgOff** = partsOff + partsSize | V-ed, V-rt |
| 0x20 | u32 | **cgSize** | V-ed, V-rt |
| 0x24 | 32 B | zero | V-data |
| 0x44 | i32[256] | absolute file offset of pattern *p*, or -1 if empty. The first pattern is at 0x444, and patterns are packed back to back in slot order. | V-ed `UnpackHantei4DatToPatterns` 0x40EC60, V-rt `Actor_ResolveFrameDataPointers` 0x432EC0 (`base + 4*pattern + 68`) |
| partsOff | partsSize B | parts blob (see 1.8) | V-ed |
| cgOff | cgSize B | CG blob `"BMP Cutter3"`. GAKIHA.DAT has `"BMP Cutter2"`. | V-ed, V-data |
| cgOff+cgSize | 256 × 64 B | pattern names, CP932, NUL-padded. The editor reads them at `partsOff + partsSize + cgSize` (0x40F0CA). In all 50 files this equals `EOF - 0x4000`. | V-ed, V-data |

`.DOF` is the same writer with `export_cg_flag`. The header keeps the parts and CG sizes, but no blobs are written,
so the names follow the pattern area directly. The game loads a `.DOF` only through `SpriteDataSlot_LoadHeaderFile` (V-ed/V-rt; there are no .DOF files in the MBAC dump).

### 1.2 Pattern record, V-ed 0x40CF60/0x40EC60, V-rt 0x432EC0

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| +0x00 | i32 | frame count (1..100; the editor stores it as a byte, `H4Pattern.frameCount`) | V-ed |
| +0x04 | i32 | moveInfo, 技情報: low nibble = move type (0 movement, 1 normal, 2 throw, 3 special, 4 special throw, 5 other, 6..11 supers). Bits 0x40/0x80 = checkboxes 1149/1148. Values seen: 0..4. | V-ed, V-data. HA6 **PSTS** (V-data) |
| +0x08 | i32 | move level 技レベル (0,10,20,30,40,45,50,100,200 seen) | V-ed. HA6 **PLVL** (V-data) |
| +0x0C | i32 | box table offset (relative to the pattern start), -1 if none. Always `0x44 + 216*nf`. | V-ed, V-rt (+0x2B8) |
| +0x10 | i32 | AT table offset = boxOff + 8·nBox, -1 if none | V-ed, V-rt `Actor_GetActiveAttackData` 0x432F80 |
| +0x14 | i32 | IF table offset = … + 88·nAT, -1 if none | V-ed, V-rt (+0x2D0) |
| +0x18 | i32 | EF table offset = … + 52·nIF, -1 if none | V-ed, V-rt (+0x2D4) |
| +0x1C..+0x43 | 40 B | **garbage**. These are dwords 7..16 of the writer's uninitialised stack buffer, holding user32 pointers or autosave file names. 4446 of 4446 patterns are non-zero here. | V-ed, V-data |
| +0x44 | 216 B × nf | frame records | V-ed, V-rt |
| then | 8 B × nBox, 88 B × nAT, 52 B × nIF, 52 B × nEF | tables, in this order, tightly packed | V-ed, V-data |

In-editor-only bytes: `H4Pattern` +3 `withoutReleaseSave`. With `g_saveWithoutNames` set, the pattern is written as -1.

### 1.3 Frame record (216 B) = AF 44 | AS 56 | int16 idx[58]

| idx | Meaning | Evidence |
|---|---|---|
| [0] | AT index. Present **iff** at least one attack box is non-empty. | V-ed; V-rt 0x432F80 (`hdr+0x10 + 88*idx[0]`, gated by idx[49..56]); V-data |
| [1..7] | **never written**: heap garbage from `malloc` | V-ed, V-data (random values) |
| [8..15] | IF slot 0..7 index (-1 = empty; written iff type != 0) | V-ed, V-rt `Actor_CurrentFrameHasIFType` (frame+116) |
| [16..23] | EF slot 0..7 index | V-ed |
| [24] | box 0 (collision / pushbox 重なり判定) | V-ed, V-rt `Character_GetPushboxIfCollidable` 0x44F480 |
| [25..32] | boxes 1..8 hurt (食らい判定1-8) | V-ed, V-rt `Actor_HasActiveHurtbox` 0x434540 |
| [33..48] | boxes 9..24: 9-10 special 1-2, 11 clash 相殺, 12 projectile 飛び道具, 13-24 special 5-16 (`g_hitbox_type_strings`) | V-ed |
| [49..56] | attack boxes 1..8 (editor memory: `H4FrameBoxes` box 40..47) | V-ed, V-rt 0x432F80 |
| [57] | never written (garbage) | V-ed, V-data |

Runtime pointer at actor+0x2D8 is `frame+148` = &idx[24]. It is the **box index array**, not the AT
(MBACPC_SYMBOLS.md line 1496 says "AT(+148)", which is wrong; the IDB comment has been corrected).

### 1.4 AF (44 B, `H4AnimFrame`), V-ed `SaveDialogValuesToEditorState` 0x407D40, V-rt `Sprite_DrawActorFrame` 0x445D70

| Off | Type | Meaning | Evidence / HA6 |
|---|---|---|---|
| 0x00 | i16 | sprite: ≥10000 = CG image (id−10000 in the embedded CG); 0..9999 = parts (.pat) entry; -1 none | V-ed, V-rt. HA6 AFGP usePat=1 for parts (V-data 158/158), usePat=0 for CG. CG ids are **not** stable vs MBAACC (MBAACC re-cut the CG: offsets 0,10,−393,… per character) |
| 0x02 | i16 | offset X | V-ed. HA6 AFOF, or AFY for (0, 7..13) (V-data) |
| 0x04 | i16 | offset Y | V-ed |
| 0x06 | i16 | wait (duration) | V-ed. HA6 AFD (95.5% equal; the rest are MBAACC edits) |
| 0x08 | u8 | 表示 flip/rotate mode 0..9 (normal, flipH, flipV, 90, 180, 270, H+90, H+270, arbitrary, arbitrary-R). Seen: 0,1,2,3,4,8. | V-ed ctl 1080, V-rt (`dword_493A70`). HA6: 1→AFAY 0.5, 3→AFAZ 0.25, 4→AFAZ 0.5, 8→AFAZ rot/10000 (V-data) |
| 0x09 | u8 | 半透明 blend: 0 none, 1 translucent (hantei4 caption 乗算 "multiply", but mbacPC renders it as normal alpha), 2 additive, 3 subtractive (D3D ZERO/INVSRCCOLOR) | V-ed ctl 1052; V-rt 0x445D70 + `D3D7_ApplyBlendMode` 0x410010. HA6 **AFAL blend = this value, verbatim** |
| 0x0A | u8 | 深度 blend amount (alpha 0..255; used only when +9 != 0) | V-ed, V-rt. HA6 AFAL alpha (V-data) |
| 0x0B | u8 | AniFlag 0 end, 1 next, 2 jump, 3 next+end on landing, 4 jump+end on landing, 5 loop check | V-ed ctl 1047. HA6 AFF/AFFE: 0→(0), 1→(1), 2→(2), 3→(1,E1), 4→(2,E1), 5→(2,E2) (calibrated in compare_states.py) |
| 0x0C | u8 | JUMP target (frame, or pattern when AniFlag=0) | V-ed. HA6 AFJP |
| 0x0D | u8 | 着地時JP landing-jump frame | V-ed. HA6 AFJC |
| 0x0E | u8 | 優先度 priority (0 unchanged, 1 front, 2 back, 3-12 front 0-9, 13-22 back 0-9, 23/24 parent±1, 25 before BG, 26 front+1) | V-ed ctl 1071. HA6 AFPR (mostly verbatim) |
| 0x0F | u8 | pad (always 0) | V-data |
| 0x10 | i16 | zoom X (256 = 1.0; 0 = 1.0) | V-ed, V-rt. HA6 AFZM x = v/256; omitted when 0/256 (V-data) |
| 0x12 | i16 | zoom Y | V-ed, V-rt. HA6 AFZM y |
| 0x14 | u8 | アニメ補間 interpolate toward the next frame (0/1) | V-ed, V-rt. HA6 **AFHK** 1 (V-data 577/578) |
| 0x15 | u8 | LOOP EDJP (loop end jump frame) | V-ed. HA6 AFLP |
| 0x16 | u8 | COUNT SET (loop count) | V-ed. HA6 AFCT |
| 0x17..0x23 | 13 B | pad (always 0) | V-data |
| 0x24 | i16 | arbitrary rotation, 1/10000 turn (runtime ×0.036°), modes 8/9 | V-ed, V-rt. HA6 AFAZ = v/10000 (V-data) |
| 0x26..0x2B | 6 B | pad (0) | V-data |

### 1.5 AS (56 B, `H4StateFrame`), V-ed 0x407D40, V-rt `Actor_ApplyFrameMovementAndHitVector` 0x4335F0

| Off | Type | Meaning | Evidence / HA6 |
|---|---|---|---|
| 0x00 | u8 | clear X (zero X vel, X accel, X max) | V-rt |
| 0x01 | u8 | clear Y | V-rt |
| 0x02 | u8 | add X: vel += facing·speedX; accel = facing·accelX; maxX = facing·AS+0x30 | V-rt |
| 0x03 | u8 | add Y | V-rt |
| 0x04..07 | 4 B | pad | V-data (3 frames non-zero, garbage) |
| 0x08/0x0A | i16 | X/Y VEC (speed) | V-ed, V-rt |
| 0x10/0x12 | i16 | X/Y ADD (accel) | V-ed, V-rt |
| 0x18 | u8 | 状態 stance 0 ground, 1 air, 2 crouch | V-ed; V-rt `FrameState_MatchesFlagAndStance` 0x434AD0. HA6 ASS1/ASS2 |
| 0x19 | u8 | 通常C normal cancel 0 never, 1 on hit, 2 always, 3 on damage | V-ed; V-rt `FrameState_GetCancelFlag` 0x434B40. HA6 ASCN |
| 0x1A | u8 | 必殺C special cancel | V-ed, V-rt. HA6 ASCS |
| 0x1C | u8 | 攻撃出現HIT (attack appear / hit count) | V-ed. HA6 **ASAA** (V-data 98.6%) |
| 0x1D | u8 | 行動可能 can act | V-ed. HA6 ASMV |
| 0x24 | u32 | flags1: b0 carry vector into next anim, b1 force vector clear, b2 no walk, b4 ground tech ok, b5 air tech ok, b31 init vector on first frame only | V-ed; V-rt 0x4335F0 (b0/b1/b31). HA6 **ASF0 verbatim** (V-data 99.7%) |
| 0x28 | u32 | flags2: b0 EX cancel, b1 throw-escapable, b2 jump-cancel only, b16-19 invuln enum (0 normal, 1 evade high, 2 evade low, 3 strike inv, 4 throw inv), b20-23 counter (0 none, 1 HI, 2 LO, 3 clear), b31 no guard | V-ed. HA6: bits 16-19 → **ASYS**, bits 20-23 → **ASCT**, the rest → **ASF1** (V-data) |
| 0x2C | u32 | reserved, always written 0 by the editor | V-ed, V-data |
| 0x30 | **i16** | X MAX (values up to 2500 and -1 seen) | V-ed, V-rt. HA6 ASMX (V-data) |

ASV0 rule (V-rt + V-data): X = bytes 0/2, Y = bytes 1/3. clear+add = **Set** (HA6 0x10 X / 0x01 Y); add only = **Add**
(0x20 X / 0x02 Y); neither = 0. Observed: (1,1,1,1)→0x11, (0,1,0,1)→0x01, (1,0,1,0)→0x10, (0,0,1,1)→0x22, (0,0,0,1)→0x02, (0,0,1,0)→0x20.
This matches Hantei-chan's tooltips (bit0 Set Y, bit4 Set X, bit1 Add Y, bit5 Add X). A clear without an add zeroes
the axis at runtime. MBAC keeps stale speed values on axes whose flag is off; HA6 zeroes them.

### 1.6 AT (88 B, `H4AttackFrame`, **type corrected in IDA**), V-ed `SaveAttackDialogToEditorState` 0x408E90 / `LoadFrameDataToAttackDialog` 0x408BD0 (byte offsets from disasm), V-rt 0x432F80

| Off | Type | Meaning (dialog 114 caption, ctl) | HA6 (V-data) |
|---|---|---|---|
| 0x00 | u32 | b0-2 ガード属性 guardable stand/air/crouch (1199-1201), b8-10 空振り条件 whiff stand/air/crouch (1202-1204), b11 のけぞり (1205), b12 ガードのけぞり (1206), b13 ダウン (1208) | ATGD verbatim (86%; the rest are MBAACC edits) |
| 0x08,0x0A,0x0C | i16×3 | HITベクトル stand/air/crouch: low byte = vector.txt id, b8 X反転, b9 X無効 (1089-1091, 1229-1234) | ATHV (97%) |
| 0x18,0x1A,0x1C | i16×3 | ガードベクトル stand/air/crouch (1092-1094, 1235-1240) | ATGV (92%) |
| 0x28 | i16 | ヒットエフェクト hit effect (1069) | ATHE[0] |
| 0x2A | i16 | 効果音 sound (1071) | ATHE[1] (ATHE verbatim 94%) |
| 0x30 | i16 | 停止時間 hit-stop time override (1081). Always 0 in MBAC data. | ATSN (I) |
| 0x32 | i16 | HIT時受け身不可時間 untech time (1241) | ATSU (98.5%) |
| 0x38 | u8 | 付加効果 added effect 0 none/1 burn/2 freeze/3 shock/4 confuse (1072) | ATKK |
| 0x39 | u8 | 投げ throw (1207) | ATNG |
| 0x3A | u8 | ヒットストップ hitstop enum (1068) | ATSP (omitted when 0) |
| 0x3B | u8 | ダメージ補整値 proration value (1095) | ATHS |
| 0x3C | u8 | 補整 op 0 overwrite, 1 multiply, 2 subtract (1070) | ATHT (omitted when 0) |
| 0x40 | u32 | flags (table `g_attackFlagCheckboxTable` 0x431C28, ended by ctl 0): 0x1 削り能力 chip, 0x2 KO不可, 0x4 追撃不可, 0x8 相殺されない, 0x10 エアリアル始動, 0x20 コンボ数を加算しない, 0x40 画面揺れ, 0x80 空中受け身不可, 0x100 地上受け身不可, 0x200 味方にもHIT, 0x400 自分はストップしない, 0x1000 バースト不可, 0x8000 カウンター時間が多くなる. Not wired: 1082 シールド不可, 1096 クリティカル. | ATF1: low bits verbatim; MBAACC ORs in new bits (0x20000, 0x1000…) |
| 0x48 | i16 | 攻撃力 damage (1073) | ATVV[1] |
| 0x4A | i16 | ゲージ増加量 meter gain (1074) | ATVV[3] |
| 0x4C | i16 | ガード減少値 guard reduction (1077) | ATVV[0] (Hantei-chan calls it "red/VS damage"; needs an MBAA check) |
| 0x4E | i16 | 気絶値 stun (1075) | ATVV[2] (Hantei-chan "guard_damage"); MBAACC rebalanced most values to 50/100 |
| 0x50 | i16 | ブレイクタイム break time (1078) | ATBT |
| rest | | pad (0x04, 0x0E-0x17, 0x1E-0x27, 0x2C, 0x34, 0x3D-0x3F, 0x44, 0x52-0x57) | |

The previous IDA type and HANTEI4_EF_IF_LABELS.md §4 (lines 187 and 201) had flags at +0x10 and the hit effect at +0x14. Those numbers are the
decompiler's dword and word **indices** (16×4 = 0x40, 20×2 = 0x28), not byte offsets.

### 1.7 IF / EF records (52 B) and boxes (8 B)

- **IF** (`H4Condition`): +0 i16 type, +2 i16 pad, +4 i32 params[12]. The UI exposes params[0..4] only, and only 0..4 are
  non-zero in the data. Types 0-37 and 50 are labelled in the tool; the data also uses 38, 39, 51-54, 100, 150 and 151 (V-ed, V-data).
- **EF** (`H4Effect`): +0 i16 type, +2 i16 No, +4 i32 p[8], +0x24 **i16 q[4]**, +0x2C 8 B pad. The EF text box shows all 12 values
  (`FormatEffectDataToText` prints p6 twice and never p7). q is non-zero in only 27 records (types 1 and 11). V-ed 0x407A60/0x407BB0.
  HA6 EFPR = p0..p7 then q0..q3 (V-data 4065/4287 equal; the rest are MBAACC edits).
- **EF position bias**: for types 1, 2, 3, 4, 8 and 11, p0/p1 are frame-space coordinates with origin (128,224). HA6 stores
  (p0−128, p1−224) (V-data; `EF03_SpawnPresetEffect` → `Actor_FrameOffsetToWorld`).
- **Box**: 4 × i16 {x1,y1,x2,y2}. It is stored only if x1≠x2 and y1≠y2 (`IsBoxNonEmpty` 0x40F730). It is in frame space with origin
  (128,224). HA6 = (x−128, y−224) for CG frames (V-data 92.8%). **Frames whose sprite is < 10000 (parts)** use double-resolution
  coordinates: runtime `x' = (x>>1) − 32`, `y' = y>>1` (V-rt `Actor_BoxToWorldRect` 0x440750). Facing mirrors x as 256−x.

### 1.8 Embedded blobs

- **Parts** (`.pat`, hantei4 `LoadPartsFile` 0x40F170): +0 u32 version = 2, +4 u32 magic 0x01234567, +0x28 i32 offset[1000]
  (non-zero = present), +0x1F68 char[1000][32] part names. This is the pre-`PAniDataFile` parts format, **not** the MBAACC
  `.pat` (which starts with `PAniDataFile`). 17 of 50 files have a parts blob.
- **CG**: `"BMP Cutter3"` image bank (GAKIHA: `"BMP Cutter2"`). The image offset table is at +0x2044 (8260) and the 24-byte align
  records are at +0x4F24 (20260) (`Cg_GetImageByIndex` 0x406A40, `Cg_GetAlignRecord` 0x406A80). Offsets are relative to the blob
  start (= cgOff). Nothing precedes "BMP Cutter3" inside the CG blob.

---

## 2. HA4 → HA6

### 2.1 Structural conversion

| HA4 | HA6 |
|---|---|
| file header + offset table | `Hantei6DataFile` header + `_STR`/`PSTR n`…`PEND`/`_END` |
| pattern names (64 B) | PTT2/PTIT (669/822 identical; MBAACC renamed the rest) |
| pattern +4 moveInfo, +8 level | PSTS, PLVL (V-data) |
| 216-B frame, full AF+AS every frame | FSTR … FEND; AFST…AFED; ASST…ASED or ASSM (dedupe identical AS); ATST…ATED only when idx[0] != -1 |
| idx[8..15] IF | IFST/IFTP/IFPR (keep params 0..4, or all 12) |
| idx[16..23] EF | EFST/EFTP/EFNO/EFPR (12 values = p[8] + q[4]); remove the EF position bias |
| idx[24..48] boxes 0..24 | HRNM loc 0..24 (x−128, y−224; parts frames: half-scale first) |
| idx[49..56] attack boxes | HRAT loc 0..7 (= combined loc 25..32) |
| CG blob (id = sprite−10000) | separate `.cg`; the MBAC CG must be used (MBAACC CG ids differ) |
| parts blob (sprite < 10000) | AFGP usePat=1. Needs a converter from the v2 0x01234567 parts format to `PAniDataFile`. |

### 2.2 Field rules (V-data unless marked)

- AFD = wait; AFOF = (x,y) or the AFY shorthand; AFJP = +0xC; AFJC = +0xD; AFLP = +0x15; AFCT = +0x16; AFPR = +0xE; AFHK = +0x14.
- AniFlag → (AFF aniType, AFFE aniFlag): 0→(0,0), 1→(1,0), 2→(2,0), 3→(1,1), 4→(2,1), 5→(2,2).
- AFAL = (AF+9, AF+0xA) when AF+9 != 0. The numbering is unchanged (1 normal-alpha, 2 add, 3 sub). This settles the TODO at
  `Hantei-chan/src/frame_disp/frame_disp_af.h:312`: the HA6 values equal MBAC's, and mbacPC renders 1 as normal alpha, not multiply.
- AFZM = (zx/256, zy/256) if the value is not 0/256. AFAZ = rot/10000 for flip mode 8. Flip 1 → AFAY 0.5; 2 → AFAX 0.5 (I); 3/4/5 → AFAZ 0.25/0.5/0.75; 6/7 → AFAY 0.5 + AFAZ (I).
- ASV0 flags as in 1.5; ASV0 speed = (AS+8, AS+0xA), accel = (AS+0x10, AS+0x12); zero the axes whose flags are off.
- ASMX = AS+0x30 (i16), ASS1/ASS2 = stance 1/2, ASCN/ASCS, ASMV = +0x1D, ASAA = +0x1C, ASF0 = flags1, ASYS = (flags2>>16)&0xF,
  ASCT = (flags2>>20)&0xF, ASF1 = flags2 & ~0x00FF0000.
- AT: ATGD = +0; ATHV/ATGV = the three i16 each (3 entries); ATHE = (+0x28, +0x2A); ATVV = (+0x4C, +0x48, +0x4E, +0x4A);
  ATF1 = +0x40; ATSU = +0x32; ATKK = +0x38; ATNG = +0x39; ATSP = +0x3A; ATHS = +0x3B; ATHT = +0x3C; ATBT = +0x50; ATSN = +0x30 (I).
- EF/IF type numbers are unchanged. MBAC's EF 9 is "play sound" (No 0/1 range, 2/3 list), the same as MBAA. MBAC data contains no EF types that are MBAA-only.
  IF 38/39/51-54/100/150/151 are used by MBAC data but not labelled in hantei4 (see HANTEI4_EF_IF_LABELS.md §1).

### 2.3 No counterpart

- HA4 only: pattern moveInfo bits 0x40/0x80; pattern byte +3 `withoutReleaseSave`; flip modes 6/7/9 as enums; AT +0x30 hit-stop time
  (unused); the 4 IF params 5..11 (always 0); the v2 parts format.
- HA6 only: AFRG tint, AFAX/AFAY free angles, AFRT, AFGX multi-layer, AFPL/AFID/AFPA/AFJH (UNI); AST0 sine motion, ASCF; ATUH extra
  gravity, ATGN block-stop, ATV2/ATAT/ATHH/ATAM/ATCA/ATC0/ATSA/ATSH/ATS3/5/6/ATRF/ATBC/ATVD (UNI/MBTL); PFLG/PUPS/PDST/PDS2;
  EF types 14, 101, 111, 1000, 10002; ATF1 high bits added by MBAACC.

---

## 3. Audit

### 3.1 Hantei-chan

**Master (current checkout): Hantei-chan cannot load HA4 at all.** No `Hantei4` detection exists in `framedata.cpp`. The only
HA4 remnants are labels (`framedata_labels.h:104,358,368`), the blend TODO (`frame_disp/frame_disp_af.h:312`), and comments in
`background/bg_file.h:11` and `bg_types.h:43`. HA4 import exists only on branch **`han4`** (commits 53a82b5 "hantei4 alpha" and b475a13 "need pat",
not merged). That code targets the old single-layer `AF` (`frame->AF.spriteId`), so it would not compile against master's
`AF.layers`. Bugs on `han4` (read via `git show han4:<file>`):

| # | File:line (han4) | Bug | Evidence |
|---|---|---|---|
| H1 | `src/framedata_load_ha4_binary.cpp:98-99` | Offset table read from **0x40** instead of 0x44, so every pattern gets the previous slot's offset, and slot 0 reads the header dword. | 1.1 |
| H2 | same `:736-755` | Loaded patterns are compacted into sequential indices (`pattern_index++`), which destroys pattern numbering. Every jump or pattern reference breaks. | 1.1 |
| H3 | same `:46-56`, `framedata.cpp:176-184` | Header fields misnamed: +0x14 is partsOff (not "anim size"), +0x18 is partsSize (not "always 0"), +0x1C is cgOff (not a duplicate). | 1.1 |
| H4 | same `:773` | Names at `partsOff + cgSize`. This is wrong for the 17 files with parts (the correct value is `partsOff + partsSize + cgSize`). | 1.1 |
| H5 | `framedata.cpp:191,214` (han4) | The CG blob is taken from partsOff with length cgSize. For files with parts it holds the parts blob plus a truncated CG. That is the real cause of the "BMP Cutter3 not at offset 0" / out-of-range image offsets that `CG_LOADER_FIX_2025_10_13.md` and `cg.cpp` (b475a13) worked around. | 1.1, 1.8 |
| H6 | `framedata_load_ha4_binary.cpp:31-43` | Pattern header: +0x0C is called frame_data_offset but is the box table; +0x10 is AT, not hitbox; +0x14 is IF, not EF; +0x18 is EF, not IF; +0x28 "vector offset" is stack garbage. | 1.2 |
| H7 | same `:557-558` | EF indices read from idx[1..8] and IF from idx[9..16]. idx[1..7] is garbage; the correct ranges are IF idx[8..15] and EF idx[16..23]. | 1.3 |
| H8 | same `:562-628` | The AT record is treated as a hitbox with coords at +0x24..+0x2A (those bytes are pad; damage is at +0x48). It is put in a slot chosen by a "type ≥ 10" heuristic, and no AT data is converted. | 1.6 |
| H9 | same `:660-701` | The only box loaded comes from frame+134 (= idx[17], an EF index) using header+0x28 (garbage) as the table. Coords are converted to width/height (HA6 is x1,y1,x2,y2), and the (128,224) bias is not removed. Hurt, special and attack boxes are never loaded. | 1.3, 1.7 |
| H10 | same `:152-162` | Flip is read from +0x10 (zoom) and blend from +0x12 (zoom Y). Flip is at +8 and blend at +9. | 1.4 |
| H11 | same `:165` | Jump is read as int16 at +0xC, which merges the landing-jump byte +0xD. | 1.4 |
| H12 | same `:206-210, 228-232` | AniFlag 5 (loop) is mapped to "next". interp, blend, loopCount, loopEnd, zoom and rotation are forced to defaults (all present in HA4 at +0x14/+9/+0x16/+0x15/+0x10/+0x24). | 1.4 |
| H13 | same `:316-320` | ASV0 is derived from **flags1** bits. It must come from AS bytes 0..3 (1.5). statusFlags[1] keeps the invuln and counter bits. maxSpeedX is set to 0 although it is at AS+0x30 (`:333`). | 1.5 |
| H14 | same `:348-350` | EF params 8..11 are read as int32 from +0x24. They are 4 × int16 (params 8/9 get packed shorts, 10/11 get padding). EF p0/p1 bias is not removed. | 1.7 |
| H15 | same `:845` | Returns `0x440 + anim_data_size` (meaningless). | — |
| H16 | `src/ha4_format.h` (whole), `src/framedata_load_ha4.cpp` (whole), `framedata.cpp:47-58` (han4) | This code describes a tag-based HA4 (`_STR`/`PSTR`/`AFST`, 8 layers, "HA4_TAG_*"). No such format exists. Only the binary container exists (1.0). | 1.0 |
| H17 | `src/framedata_load_ha4_binary.cpp:119-146` | Parts sprites (< 10000) are marked usePat=0. They must be usePat=1 with the parts blob converted, which Hantei-chan cannot do (its parts loader requires `PAniDataFile`, `src/parts/parts.cpp:93`). | 1.8 |

### 3.2 han4_parser (`/mnt/c/dev/hantei-chan/han4_parser`)

Run on all 50 files (`data/ha4/han4_parser_audit.txt`): it does not crash, but **0/4446 patterns decode correctly**
(3 frame counts match by accident), 48/50867 sprite ids are right, and names are right only for the 33 files without parts (1631/2859).

| # | File:line | Bug |
|---|---|---|
| P1 | `parsers/dat_parser.py:56,124` | Table at 0x40, with offsets treated as relative to 0x440. They are absolute and the table is at 0x44. |
| P2 | `dat_parser.py:227` | Frames start at pattern+0xA8. They start at +0x44 (0xA8 = frame 0's idx array, 0x44+100). |
| P3 | `dat_parser.py:144` | Names at anim+image. The correct offset is partsOff+partsSize+cgSize. |
| P4 | `dat_parser.py:308-315` | AF flip/blend/AniFlag/priority read as int16 at +0x10/+0x12/+0x16/+0x1C. These are u8 at +8/+9/+0xB/+0xE; the legacy doc doubled the byte offsets. |
| P5 | `dat_parser.py:342-345, 367` | AS bytes 0..3 are named set/1034/1033. They are clearX, clearY, addX, addY. +0x30 is called "unknown_1073"; it is X MAX. |
| P6 | `dat_parser.py:379-412` | Index arrays are misread. Conditions come from idx[1..8] (should be [8..15]), 16 effects from idx[9..24] (should be [16..23]), and the "main vector" from idx[25] (collision is [24]). |
| P7 | `dat_parser.py:532-581` | The AT is modelled as a hitbox: x/y/w/h at +0x24..+0x2A (really pad), "damage" at +0x3B (proration), "hitbox_type" at +0x14 (pad; hit effect is at +0x28), flags at +0x10 (really +0x40), untech at +0x19 (really +0x32). |
| P8 | `dat_parser.py:493, 532, 588, 627` | Tables are parsed with fixed max_count 50/100 and no end, so they spill into neighbouring tables. The table sizes are exactly derivable (1.2). |
| P9 | `dat_parser.py:608, 651` | IF "number" at +2 is padding. EF params are read as 12×int32 but are 8×int32 + 4×int16. |
| P10 | `parsers/ha4_parser.py:56-59,106-111,130,147` | Same header and table errors as P1/P3, plus a false "anim size mismatch" warning (+0x1C is cgOff). |
| P11 | `ha4_parser.py:186-189` | +0x0C read as frames (it is boxes), +0x10 as hitboxes (it is AT), +0x14/+0x18 as effects/conditions (swapped). |
| P12 | `ha4_parser.py:261-262` | Sprite read at +4 (that is offset Y) and duration at +0x10 (that is zoom). |
| P13 | `ha4_parser.py:339,379` | Treats type 0 as a table terminator. The tables have no terminator. |
| P14 | `parsers/character_data_parser.py:349-387` | In-memory `H4Pattern` view: `wait` read at +2 (it is at +6), "stance" at AF+8 (it is flip), AS+8/+A called scale (they are speed), +0x19 called invuln (it is normal cancel), `action` at +0x1E (canAct is at +0x1D), x_max read at +0x21 (it is at +0x30). |
| P15 | `character_data_parser.py:405, 461-524` | IF type read as u32 (it is i16 + pad). AT: flags read at +0x10 as u16 (they are at +0x40 as u32), and the flag names are shifted by one bit (0x1 is chip, not "cannot KO"). Guard vectors at +0x12 (they are at +0x18). Hit effect at +0x14 (it is at +0x28). attack_power/gauge/guard/stun at +0x24..0x27 as bytes (they are i16 at +0x48/+0x4A/+0x4C/+0x4E). |
| P16 | `file_detector.py:65` | `"BMP Cutter3"` is labelled `PATTERN_PAT`. It is the CG image bank; parts use 0x01234567 v2 (or `PAniDataFile` in MBAACC). |

### 3.3 mbacdat.py (`docs/tag_research/tools/mbacdat.py`)

This is the only correct parser. It matches the reference on all 50 files (names, frame counts, sprites, IF/EF counts, boxes). Two bugs:
- `:12` docstring says `AS: +0 setY +1 setX +2 addY +3 addX`. The runtime shows +0 clear X, +1 clear Y, +2 add X, +3 add Y. The
  X/Y labels in `compare_states.py:54-55` are swapped in the same way. The HA6 bit mapping those scripts use is still right, because
  both sides use the same swapped labels.
- `:63` `maxx = b[a+0x30]` reads a byte, but X MAX is i16. 99 frames have X MAX ≥ 256 (1000, 1200, 2000, 2500, −1).
- Minor: `:37` treats offset 0 as empty (harmless). The AT is kept only as raw bytes (use `ha4lib.parse_at`).

### 3.4 Wrong claims in existing docs

- `docs/tag_research/HANTEI4_EF_IF_LABELS.md` §4: line 187 "+10 flags" should be **+0x40**; line 201 "+14 hit effect" should be **+0x28**; line 235 "Deduplicated
  tables" is wrong (tables are sequential, no dedup). Line 155 "+2 SetX, +3 SetY": these bytes *add* at runtime.
- `docs/tag_research/HANTEI4_SYMBOLS.md`: the IDA `H4AttackFrame` type it references had the same +0x10/+0x14 error (now fixed in the IDB).
  The `WriteHantei4DatFile` note "deduped boxes" is wrong.
- `docs/tag_research/MBACPC_SYMBOLS.md:1496`: "+0x2D8 AT(+148)" is wrong. It is the box-index array (idx[24..]).
- `docs/HA4_PARSING_STATUS_FINAL.md`:
  - FrameHeader: wait@+2, "stance"@+8, u16 depth/jump, loop_edjp u16@+0x15, count@+0x17, ex_flag@+0x19 are all wrong (see 1.4).
  - LayerData: set_y@+2/set_x@+3, "scale"@+8, invuln@+0x19 are wrong.
  - AT: x/y/w/h@+0x24 and flags@+0x10 are wrong.
  - The special-flag table is shifted one bit (it omits 0x1 = chip 1076).
  - "VectorArray 48 vectors" is the in-memory box array, not movement vectors.
- `docs/COMPLETE_HITBOX_88BYTE_STRUCTURE.md` / `COMPLETE_HITBOX_88_BYTE_MAPPING.md`:
  - The 88-byte record is the **attack data**, not a hitbox, and it has no geometry.
  - Every word-sized field offset is half the real value (word index shown as a byte offset).
  - flags_2 is at +0x40 with chip = 0x1.
- `docs/COMPLETE_HANTEI4_FIELD_MAPPING*.md`: the same doubling and halving errors as above. They come from reading decompiler
  `*(WORD*)p + N` / `*(DWORD*)p + N` as byte offsets.
- `docs/CG_LOADER_FIX_2025_10_13.md`:
  - "Both CG and PAT files use BMP Cutter3" is wrong.
  - "pixel data before BMP Cutter3" is wrong: the bytes before it are the parts blob, because of bug H5.
  - The layout "+0x40 offset table … +0x440 animation data" is wrong (table at 0x44, patterns from 0x444).
- `docs/legacy/Hantei4_File_Variants_Guide.md`:
  - "Compressed .DAT = multi-character" is wrong: it is one character with 256 patterns.
  - "DOF has 0xFFFFFFFF CG/PAT offsets" is wrong: a DOF keeps the header values and omits the blobs.
  - The `.pat` "BMP Cutter3" signature is wrong.
- The claim "frame compression 216 B in file → 832 B in memory (3.85:1)" (han4_parser, ha4_format.h) is wrong. 832 B is only the IF+EF
  script block. A full frame in the editor is 1404 B spread over 5 parallel arrays.

---

## 4. Verification

- `tools/ha4_verify.py`: **50/50** MBAC `.DAT` pass every writer invariant: header, sizes, contiguous patterns, the table-offset
  formulas, sequential non-deduplicated indices, AT iff an attack box exists, names at EOF−0x4000. In total: 4446 patterns, 67721 frames.
  Statistics (`data/ha4/ha4_verify_all.txt`):
  - AniFlag 0-5 all used.
  - Blend 0-3; flip ∈ {0,1,2,3,4,8}; interp ∈ {0,1}.
  - AF pads are always 0. AS pads are non-zero in ≤ 6 frames (garbage). AS+0x2C is always 0.
  - AT: every non-zero byte lies inside a named field.
  - EF q non-zero in 27 records.
  - Pattern header tail non-zero in all 4446 patterns (stack garbage). idx[1..7] and idx[57] are random.
- `tools/han4_parser_audit.py`:
  - mbacdat.py is identical to ha4lib on all 50 files.
  - han4_parser `CompressedDATParser` / `HA4Parser` parse without exceptions but decode 0 patterns correctly (numbers in 3.2).
  - `detect_file_type` returns COMPRESSED_DAT for all files.
- `tools/ha4_vs_ha6.py`: produces the per-field agreement figures quoted in 1.4–1.7 and 2.2.

---

## 5. IDA changes

| Database | Renames | Comments / types |
|---|---|---|
| `hantei4` | 24 local-variable renames (16 in `UnpackHantei4DatToPatterns` 0x40EC60: box_table, at_table, if_table, ef_table, collision_box_idx, hurtbox_idx_ptr, specialbox_idx_ptr, attackbox_idx_ptr, dst_boxes, dst_attack, dst_anim, dst_state, pattern_offset_table, pattern_hdr, frame_idx_array, at_idx; 8 in `WriteHantei4DatFile` 0x40CF60: box_table_buf, at_table_buf, at_count, box_count, pattern_hdr_buf_17dw_only0to6set, has_attack_box, src_boxes, src_attack). No function/global placeholders remained. | Type `H4AttackFrame` redefined (flags +0x40, hitEffect +0x28, full field list). Function comments appended (CONF: high): `WriteHantei4DatFile`, `UnpackHantei4DatToPatterns`, `IsBoxNonEmpty`, `SaveAttackDialogToEditorState`, `LoadFrameDataToAttackDialog`. Saved. |
| `mbacpc` | 5 globals: `byte_991600`→`g_SpriteDataSlots_Name`, `unk_991704`→`g_SpriteDataSlots_PatternImage`, `unk_991708`→`g_SpriteDataSlots_PartsData`, `unk_99170C`→`g_SpriteDataSlots_CgData`, `dword_991710`→`g_SpriteDataSlots_Palette` | 5 global comments. 7 function comments: `SpriteDataSlot_Load` 0x43DB20, `SpriteDataSlot_LoadHeaderFile` 0x445480, `Actor_ResolveFrameDataPointers` 0x432EC0 (corrects "AT(+148)"), `Actor_GetActiveAttackData` 0x432F80 (med→high), `Actor_ApplyFrameMovementAndHitVector` 0x4335F0, `Actor_BoxToWorldRect` 0x440750, `Sprite_DrawActorFrame` 0x445D70. Saved. |
| `212fe94c` (MBAA) | none (read-only) | none |

Open items: confirm the MBAA meaning of ATVV[0]/[2] (MBAC ガード減少値 / 気絶値), AFAX for flip mode 2, and whether AT+0x30 maps to ATSN.
