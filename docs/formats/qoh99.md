# Queen of Heart '99 (qoh99.exe, Nov/Dec 2000): character .chr, .Fob scripts, .Img images

Status: **EXACT container + cipher, every shipped file accounted for to the byte** (`tools/fb/qoh99_verify.py` passes: 37 .chr, 30 .Fob, 553 .Img;
`tools/fb/qoh99_cipher.py` round-trips all 37 .chr bit-exactly). Record internals are traced in the IDB; the unproven items are listed in section 10.
IDB: `C:\dev\ida\server\qoh99_dec.exe.i64` (December build; the November VAs in the PovertyCaster adapter are the same below 0x4736C0, +0x20 above).
IDA types: `docs/formats/ida/qoh99_types.h` (18 structs, 5 enums, applied in the IDB; every offset in the file was cross-checked against IDA's member offsets).
Tags: **T** traced in code, **D** verified on the shipped data, **U** unproven, **X** never read.

Ancestors/relatives: the frame record is the 52-byte `Qoh98Sprite` of `qoh98.md` plus 12 bytes (same offsets up to +0x30); the attack record is `Qoh98HitBox`
unchanged; the box record is `Qoh98BodyRect` unchanged. The container is a rearranged, enciphered `Qoh98DatHeader`.
Descendant: `pb2k1.md` (96-byte frames).

## 1. Files

| kind | count | loader | note |
|---|---|---|---|
| `<dir>\<name>.chr` | 37 (35 characters/forms + `System\obj1.chr` + `System\rina_cra.chr`) | `LoadAndDecryptCharacterFile` 0x42C3A0, header check `ValidateAndDecryptFileHeader` 0x42C1F0 | enciphered, section 2/3 |
| `<dir>\com_<name>.Fob` | 30 | `LoadFobScriptIntoBank` 0x460900 (from `LoadCharacterFobScript` 0x445250, path `.\<dir>\com_<name>.fob`) | CPU-player scripts, section 6 |
| `*.Img` | 553 (System 8, System\WinMes, face, select, title, bg, ...) | `LoadQohImgFile` 0x45CB20 (from `LoadImageFile` 0x42C880, image type 2 at all 47 call sites) | raw palettised/24-bit bitmaps, section 7 |

Not decoded here: `System\Bgm`, `.wav`, `System.bin/main.bin/data1.bin/data2.bin`, `QOHcnf.key`, the type-1 image path `LoadPsxTimImageFromArchive` 0x42F770 reads PS1 TIM
files: flags word `&7` = bpp mode 0/1 = 4/8 bit, bit 3 = CLUT present, 5-bit RGB palette).

Revision table: the game compares the high half of the 4-byte tag of every .chr with `g_chr_revision_stamps` (0x4A715C, 39 ints, index = position in
`g_character_filename_table` 0x4A3EE0 = Saori, Mizuho, Ruriko, Hatune, Kaede, Azusa, Chizuru, Mizuki, Akari, Serika, Tomoko, Siho, Aoi, Yuki, Serio, Multi, Kotone, Remy, Rio, Ayaka, rina,
o_akari, m_multi, o_chizuru, kanako, c_serika, s_siho, s_kotone, lf97_aoi, H_Hatune, Tiria, R_Ayaka, Corin, Yosie, Edifel; 37 = obj1, 38 = rina_cra).
D: the shipped files carry 0 (24 files), 1 (Mizuho, Azusa, Kotone, s_kotone, m_multi, Remy, Rio, c_serika, R_Ayaka), 2 (Corin, Kaede, Tiria), 5 (Yosie). The December table says 6 for Yosie
(index 33), so this install's Yosie.chr is the older revision (U: which build ships 5).

## 2. The .chr cipher (T + D, exact)

Procedure (`tools/fb/qoh99_cipher.py` is the reference implementation):

```
PRIM_STATIC = bf62a497a0da0dbd91bc97af92afe5aff0          # 17 bytes at g_primary_xor_key 0x4A3834 (followed by 0x2E 0x00 in the image)
# InitializeEncryptionKeys 0x48CFA0, run once at start-up (reads 256 bytes at offset 32725477 of the encrypted config file, b0 = first byte;
# the literal "SZUKI MASAMI" is the only other input: its first byte 'S' = 0x53)
PRIM[j] = PRIM_STATIC[j] ^ b0 ^ 0x53        (j = 0..16),  PRIM[17] = 0       b0 = 0x7E  =>  PRIM = 924f89ba8df72090bc91ba82bf82c882dd (17 bytes)
stem   = upper_case(file name without directory and extension)               # "CORIN" for .\Corin\Corin.Chr, from the PATH the game opens
name block (16 B at offset 0)  = (stem repeated to 16 bytes)[i] ^ PRIM[i % 17]       # the game decodes it with PRIM, upper-cases it, and requires it to match the path stem
for every other region R (tag 4 B, counts 24 B, each body section) decode independently with i restarting at 0:
    p[i] = ((c[i] ^ stem[i % len(stem)] ^ ((~(8*i) + (i >> 3)) & 0xFF)) - PRIM[i % 17] - 109) & 0xFF
encode (exact inverse):
    c[i] = ((p[i] + PRIM[i % 17] + 109) & 0xFF) ^ stem[i % len(stem)] ^ ((~(8*i) + (i >> 3)) & 0xFF)
```

* The PRIM index wraps when `g_secondary_xor_key[j] == 0`; `g_secondary_xor_key` is `g_primary_xor_key + 1` (0x4A3835), so the period is 17 (T: the 3 inlined loops in the two functions are identical).
* The stem is NUL-terminated at its true length (`sz[i] = 0` after the name comparison, T), so a name shorter than 16 characters cycles with its own period.
  **The prior-art scripts and `Corin.chr_decrypted.bin` use the unterminated 16-byte block ("CORINCORINCORINC") as the key. That is wrong after byte 16 of every region:
  `decrypt(Corin.chr, key16)[44:] == Corin.chr_decrypted.bin[16:]` byte for byte (D), the file is the 16 name bytes followed by the three-region tail without the 28 header bytes, and 1,098,038 of its 1,106,844 body
  bytes differ from the game's decode.** The game's decode is the one that yields exact size accounting and every structural invariant below.
* b0 = 0x7E is solved, not read: it is the only value for which all 37 name blocks decode to their own file name (and PRIM[1]^... agree with every file). If the config file ever changes, only `KEY_B0` moves.
* Round trip: `encrypt(decrypt(x)) == x` for all 37 files (D, bijection per byte); the name block is reproduced by `encode_name_block(stem)` for all 37 (D).
* Only the header and body regions are enciphered; nothing in the file is plain.

## 3. Container (plaintext layout, T + D: sizes add to the file size exactly in all 37 files, no trailing bytes)

| offset | size | content |
|---|---|---|
| 0x00 | 16 | name block (section 2) |
| 0x10 | 4 | revision stamp: high 16 bits compared with `g_chr_revision_stamps[fileKind]`, low 16 bits = 0x000C in all files (X) |
| 0x14 | 24 | `c0..c5` = imageCount, tileCount, actionCount, frameCount, boxCount, attackCount |
| 0x2C | 8*c4 | boxes (`QohBox`) |
| | 24*c2 | actions (`QohAction`) |
| | 12*c0 | image records (`QohImageRecord`) |
| | 1024 | palette (`QohChrPalette`) |
| | 260*c1 | tiles (`QohTile`) |
| | 64*c3 | frames (`QohFrame`) |
| | 24*c5 | attacks (`QohAttack`) |

The loader reads them in this order and stores them in the first 48 bytes of the actor (`QohCharAssets`, c0 -> +0x0C, c1 -> +0x04, c2 -> +0x14, c3 -> +0x1C, c4 -> +0x24, c5 -> +0x2C, pointers at
+0x00 tiles, +0x08 images, +0x10 actions, +0x18 frames, +0x20 boxes, +0x28 attacks). The palette is copied to the caller's 1024-byte buffer (`g_player_chr_palette_buffers`, `g_obj1_chr_palette`, `g_rina_chr_palette`) and
the loader then zeroes B,G,R of entries 0, 16, 32, 48. Per-file counts: `qoh99_verify.py --table` (Corin: 271 images, 3758 tiles, 256 actions, 1716 frames, 722 boxes, 156 attacks; actionCount is 256 in all 37 files).

Plaintext invariants verified on all 37 files (D):
* image records tile `[0, tileCount)` contiguously in order; image size is 256x256 (6923) or 320x240 (15, all in `O_CHIZURU.CHR`); 34 images have 0 tiles.
* tile `x, y` are multiples of 16 with `x+16 <= width`, `y+16 <= height`; pixels are 0..15 (4-bit indices, row-major, 0 = transparent).
* palette: 4 banks of 16 BGRX colours in entries 0..63, entries 64..255 are 0; the reserved byte is 0 everywhere; entry 0 of each bank is stored non-zero in the file and zeroed by the loader.
* actions: `firstFrame == -1` marks an unused action (5,535 of 9,472); the used ones partition `[0, frameCount)` exactly, in order; `firstBox`/`firstAttack` are -1 or within the tables;
  `boxCount`/`attackCount` equal the extent of the action's frame references except at the 0xFF sentinels (X: never read).
* frames: `imageIndex` always `< imageCount`; `nextFrame` is 0xFFFE/0xFFFF or `< frameCount of the action` (except 5 of 43,410 frames); reserved bytes are 0; bytes +0x2A/+0x2B are both 0 or both 0xCD (27,920 frames carry the 0xCD build fill).

## 4. Records

All little endian; offsets are `+0xNN` within the record. Full field list with evidence tags: `docs/formats/ida/qoh99_types.h`.

### 4.1 Image record (12 B) `QohImageRecord`
`u32 firstTile; u32 tileCount; u16 width; u16 height` (T: `GetImageRecord` 0x4629A0 returns them for `RenderCharacterSprite`). An image is the set of tiles `[firstTile, firstTile+tileCount)` laid out inside a width x height canvas by each tile's own x, y.

### 4.2 Tile (260 B) `QohTile`
`u16 x; u16 y; u8 pixels[256]` (16x16, row-major palette indices 0..15; 0 transparent). Drawn one tile at a time by `QueueSpriteRenderWithAlignment` at `actor.pos + frame.draw + (tile.x, tile.y)` minus the camera.
The 16 colours are picked from the actor's palette at `paletteTable[paletteEntryBase ...]` (4-byte entries).

### 4.3 Action (24 B) `QohAction` (index = action id = `actor.actionIndex`, 256 per file)
`i32 firstFrame` (-1 unused; `AdvanceActorFrameAndApplyFrameEvents` resets the actor to action 0), `i32 frameCount` (T: wraps the frame cursor to 0 when `frameCount < nextFrameIndex`),
`i32 firstBox` (T), `i32 boxCount` (X), `i32 firstAttack` (T), `i32 attackCount` (X).

### 4.4 Frame (64 B) `QohFrame`
frame = `frames[action.firstFrame + actor.frameIndex]`. Offsets and meaning (T unless stated; the `actor.*` names are in section 8):

| off | field | meaning |
|---|---|---|
| 0x00 | i32 imageIndex | image record to draw (out of range = nothing) |
| 0x04/0x06 | i16 drawX/drawY | canvas origin relative to the actor position (drawX negated when facing left; the shadow pass uses drawY/3) |
| 0x08 | i32 duration | -> `actor.frameTimer`; the step runs again when it reaches 0 |
| 0x0C | u16 nextFrame | frame index in the action entered next; 0xFFFF = this+1, 0xFFFE = hold and flag `animEnded` |
| 0x0E | u16 attackLevelAndFlags | low nibble -> `actor.attackLevel`; bit 0x10 arms the attack boxes (`contactFlags` bit 0 = the active latch, `attackHitMask = 0xFF`); bit 0x20 read by the hit code |
| 0x10 | u8 moveFlags | 1 = load xAccel/xJerk, 2 = load yAccel/yJerk, 4 = on landing jump to `landFrame` (else hold) |
| 0x11/0x12 | u8 clearVelX/clearVelY | non-zero zeroes the velocity |
| 0x13/0x14 | u8 xAccelAbsolute/yAccelAbsolute | -> actor (D: 0x14 is 0 in all files) |
| 0x15 | u8 tileXform | QohTileXform 0..7, bit 4 = flip (xor 4 when facing left) |
| 0x16 | u8 landFrame | frame entered on landing |
| 0x17 | u8 bodyBoxIndex | body (push) box = `boxes[action.firstBox + this]` |
| 0x18 | u8 bodyBoxMode | 0 / 0xFF = no body box, otherwise 1 |
| 0x19/0x1A | u8 hurtBoxFirst/Count | vulnerable boxes; count 0 = untouchable |
| 0x1B/0x1C | u8 unused_1B/unused_1C | D: 0 in all frames, no reader |
| 0x1D/0x1E | u8 guardBoxFirst/Count | projectile-guard boxes (`CheckProjectileVsGuardBoxCollision`); count != 0 sets `contactFlags |= 2` |
| 0x1F/0x20 | u8 attackFirst/Count | attack records `attacks[action.firstAttack + first ..]`; count 0xFF / 0xFE = special hit kinds 1 / 2 |
| 0x21 | u8 stanceBits | -> `actor.stanceBits` (low 3 bits stance class; 0x40 = turn toward the nearest opponent; 0x20, 0x80 read by the hit code) |
| 0x22 | i16 xAccel | -> `actor.accelX` (negated facing left) when moveFlags & 1 |
| 0x24 | i16 yAccel | -> `actor.accelY` when moveFlags & 2 |
| 0x26 | i16 xJerk | -> `actor.jerkX` when moveFlags & 1 |
| 0x28 | i16 yJerk | -> `actor.jerkY` when moveFlags & 2 |
| 0x2A/0x2B | u8 unused_2A/2B | D: both 0 or both 0xCD (uninitialised build fill), no reader |
| 0x2C | i32 aiMoveClass | -> `actor.aiMoveClass`, read by the CPU code (D: 0..11) |
| 0x30 | u32 fxFlags | bits: 0x1F sound id-1, 0xC0 sound bank, 0x10000 spawn fx 1, 0x20000 hit-damage phase 0, 0x40000 screen mode 4, 0x80000 clear combo state, 0x200000 spawn fx 2, 0x400000 KO effect, 0x800000 positioned effect, 0x1000000 hit-damage phase 1; byte +0x31 = second sound |
| 0x34 | u32 drawClass | `& 3` -> `actor.drawClass` (D: 0, 1, 3) |
| 0x38 | u8 flickerFlags | bit 0 = draw every second tick only |
| 0x39..0x3F | u8[7] unused_39 | D: 0 in all frames, no reader |

### 4.5 Box (8 B) `QohBox` and Attack (24 B) `QohAttack`
Box: `i16 left, top, right, bottom` (inclusive, relative to `actor.posX/posY`; facing left mirrors inside a 256-wide canvas: `[x - right + 255, x - left + 256)`, T `GetBoxWorldRect` 0x462AC0).
Attack: the same 8 rectangle bytes (T `GetAttackBoxWorldRect`), then `i16 damage` (+8), `i16 unresolved_0A` (+10, U/D: non-zero in 34 records, no reader), `i16 guardDamage` (+12),
`i16 meterGain` (+14), `u8 flags` (+16, bit 0 = damaging), `u8 guardMask` (+17, bit 0..2), `u8 hitClass` (+18, 0..15: 0..9 and 12 reaction poses, 10/11 special, 13..15 clash rules),
`u8 unresolved_13` (+19, U/D), `u16 knockMode` (+20, bits 0..1 -> defender `hitFacingMode`, bit 2/3 tested; bit 9 = no guard clash), `u16 hitEffectFlags` (+22, bits 12..15 = screen flash type; bit 0 set in 3000 of 4733 (U)).
Hit test order (T `ProcessCharacterHitDetectionAndDamage` 0x466770): the attacker needs `contactFlags & 1`, `attackHitMask` bit of the victim slot, `attackCount != 0`; every attack box of `[action.firstAttack + attackFirst, +attackCount)` is intersected
with every hurt box of the victim `[firstBox + hurtBoxFirst, +hurtBoxCount)`, hit classes 13..15 excluded.

## 5. Image rendering path (T)
`RenderCharacterSprite` 0x40D420: frame = `frames[actions[actionIndex].firstFrame + frameIndex]`; image record = `images[frame.imageIndex]`; for tiles `firstTile .. firstTile+tileCount-1` call the sprite queue with the
tile pointer, position `(actor draw origin + frame.drawX/Y)`, flags `frame.tileXform | caller flags`. Render modes 0/2 scale y by 1/3 (shadow), 1/3/4 draw directly.

## 6. .Fob (CPU-player script) (T + D, container exact, every code byte decoded)

```
u32  nFuncs                         0..3 in the shipped files (0: 17 files, 1: 12, 3: com_mizuho)
{ char name[32]; u32 entryPc; } [nFuncs]       36 bytes each; name NUL terminated, the bytes after the NUL are uninitialised build garbage ("1", "Mizuho_Idou", "Mizuho_Idou2")
u32  codeSize
u8   code[codeSize]                 the loader appends 2 zero bytes; all pcs are byte offsets into this block (starting at 0)
```
`8 + 36*nFuncs + codeSize == file size` in all 30 files (D). `LoadFobScriptIntoBank` caches the file in one of 20 `QohScriptBank` slots (272 B: path[260], code, funcCount, funcs) at `g_fob_script_banks` 0x62D640 keyed by the upper-cased path;
`g_fob_bank_index[player]` (0x623920) remembers the slot.

Interpreter: `RunFobScript` 0x461550. A stack machine over `g_fob_value_stack` (depth `g_fob_vm_state.stackDepth`, push pre-increments) with pc `g_fob_pc` (0x66EBA0). Binary/assign ops carry a flags word (bit 0 = top operand is a
reference to dereference, bit 1 = second operand is a reference). Instruction forms (sizes decoded exactly on all 30 files: **155,220 reachable + 50,280 dead-code bytes decoded, 1,224 switch tables, 0 unexplained bytes**):

| op | bytes | meaning |
|---|---|---|
| 0x01, 0x02 | 6+4n | skip: `pc += 6 + 4*u32` (inline data; present only in unreachable code in the shipped files) |
| 0x09 | 2 | swap top two |
| 0x0A | 6 | compound assign: flags, sub: 0 `=`, 0x64 `+=`, 0x65 `-=`, 0x66 `*=`, 0x67 `/=`, 0x68 `%=`, 0x69 `&=`, 0x6A `|=`, 0x6B `^=` (`FobOp_CompoundAssign`) |
| 0x0B..0x13, 0x16, 0x17 | 4 | `+ - * / % << >> & |`, logical and, logical or on the top two (flags word). 0x14/0x15 are not implemented |
| 0x18 | 6 | compare (`FobOp_Compare`), sub 0 `==`, 1 `!=`, 2 `<=`, 3 `>=`, 4 `<`, 5 `>` |
| 0x1A | 6 | push imm32 |
| 0x1B | 6 | push `code + imm32` (address of data in the code block) |
| 0x1C / 0x1D | 2 | push `&g_fob_return_value` / push `&g_fob_script_globals[16]` |
| 0x1E | 2 | drop top |
| 0x28 | 8 | switch (`FobOp_Switch`): `u16 kind (2 = by reference)`, `u32 tableAddr`; table in the code block = `{u32 default; u32 count; {u32 key; u32 target}[count]}` (always behind an unconditional jump) |
| 0x2D | 10 | conditional jump (`FobOp_ConditionalJump`): `u16 kind (2 = by reference)`, `u16 cond (0 ==0, 1 !=0, 2 <=0, 3 >=0, 4 <0, 5 >0)`, `u32 target` |
| 0x2E | 6 | jump |
| 0x2F / 0x30 | 6 / 2 | call (pushes the return address and a marker) / return |
| 0x33 | 2 | irand: `*ref = rand() % n` |
| 0x34, 0x36 | 2 | yield to the scheduler (resume at the next instruction) |
| 0x3C | 6 | source line marker, stored in `g_fob_vm_head.currentLine` |
| 0x3D..0x40 | 2 | AI queries TryBlock / TrySpecialMove / TryThrow / TryJump -> `g_fob_return_value` |
| 0x41 | 2 | pop -> `g_fob_return_value` |
| 0x42..0x45 | 2 | AI command ops (arm slot, wait on direction bit 0x20000 / 0x10000 / 0x80000) |
| 0x46 | 2 | default key data: pops 3, `ExecuteAIScriptCommandHandler` |
| 0x47..0x5C, 0x5D..0x64, 0x66..0x69 | 2 | per-character AI handlers: 0x47 Saori, 0x48 Nonoka-slot, 0x49 Mizuho, 0x4A Ruriko, 0x4B Hatune, 0x4C Kaede, 0x4D Azusa, 0x4E Chizuru, 0x4F Mizuki, 0x50 Akari, 0x51 Serika, 0x52 Tomoko, 0x53 Siho, 0x54 Aoi, 0x55 Yuki, 0x56 Serio, 0x57 Multi, 0x58 Kotone, 0x59 Remy, 0x5A Rio, 0x5B Ayaka, 0x5C Rina; special forms 0x5D o_akari, 0x5E m_multi, 0x5F o_chizuru, 0x60 Kanako, 0x61 c_serika, 0x62 s_siho, 0x63 s_kotone, 0x64 lf97_aoi, 0x66 Tiria, 0x67 R_Ayaka, 0x68 Yosie, 0x69 Corin |
| 0x6A, 0x6B | 2 | halt (the pc is not advanced) |
| others | | `ScriptUnknown Code!!` |

Decoding recipe (D): recursive descent from pc 0 and every entry pc following jump/call/conditional-jump targets and switch-table targets; unconditional jumps, switches, return and halt end a path; the 0x28 tables are data;
the remaining gaps (dead code after unconditional jumps, about 1.5-2 KB per file) decode linearly into instructions and tile exactly. Every entry pc, every jump/call target and every switch target is an instruction start.

## 7. .Img (T + D, exact on 553 files)

```
u32 reserved            T: returned to the caller if requested; D: 0 in all files
u32 paletteEntries      T/D: 0 (24-bit), 3 .. 256; 4 bytes per entry (blue, green, red, 0)
u32 bitsPerPixel        T/D: 4, 8 or 24 (24 only with paletteEntries == 0)
u32 width, height
u8  palette[4 * paletteEntries]
u8  pixels[]            paletteEntries != 0 : height rows, row = ((width+3)&~3) bytes for 8 bpp or half of that for 4 bpp (two pixels per byte, low nibble first, U), rounded up to 4 bytes
                        paletteEntries == 0 : 3 * height * ((width+3)&~3) bytes (B,G,R)
```
`20 + 4*paletteEntries + pixel bytes == file size` in all 553 files: 293 8-bit, 172 4-bit, 88 24-bit (D). Row order (bottom-up DIB convention) and the nibble order are U (the loader copies the bytes unchanged into a DDraw surface / DIB).
(`LoadBmpDibFromFile` 0x45C8B0, a standard BMP file reader, has no call site with the shipped data.)

## 8. Runtime state (IDB)

`struct QohActor` (1532 = 0x5FC bytes; `docs/formats/ida/qoh99_types.h`) is shared by the 4 fighters (`g_p1_actor` 0x623E00, `g_p2_actor` 0x626880, `g_p3_actor` 0x4ECB40, `g_p4_actor` 0x617A20), the 4 character-state / form actors
(`g_character_state_actors` 0x614060, pointers `g_character_state_actor_ptrs` 0x623500), the per-slot echo copies (`g_slot_actor_groups` 0x6275C0 = 4 slots x 4 actors), the projectiles (`g_projectile_actors` 0x526FA0 x120), helpers
(`g_helper_actors` 0x618160 x30), effects (`g_effect_actors` 0x8719E0 x30), `g_obj1_actor` 0x624EE0 and the rina effect actors (`g_rina_effect_actors` 0x8A3740 x4, copies of one load).
Field groups (all offsets decimal in the header comments):

| offset | fields |
|---|---|
| 0..47 | `QohCharAssets` (the loaded .chr tables) |
| 48, 52, 56, 64 | actionIndex, nextFrameIndex, frameTimer, frameIndex |
| 72..92 | requestedAction, requestFlag, dispatchAction, animEnded, forcedAction |
| 96..134 | 8.8 fixed point motion: vel (96,98), accel (100,102), jerk (104,106), impulse group (108..118), slide group (120..132) |
| 136, 140 | posX, posY |
| 156..177 | stanceBits, reactionHeight, attackLevel, contactFlags, attackHitMask, guardHitMask, commandChild (input ring object, `QohCommandChild`), accel flags, facing, team, playerSlot, entityKind (0xFF free), paletteEntryBase, hitAttrib, hitDirection |
| 204..212 | stateNibbles, stateFlags, tagTimer |
| 216..1295 | `QohTrailEntry trail[30]` (pose history, 36 B each) |
| 1296..1420 | pending hit data, snapshots, hitPendingFlags, landingMode, wallFlags, paletteTable |
| 1428..1528 | subState, currentFrame, lastContactActor, formMode, drawClass, hitstopFrames, aiMoveClass, ... |

Functions named/typed in the IDB (old -> new): `HandleCharacterMovement` -> `AdvanceActorFrameAndApplyFrameEvents`, `UpdateCharacterLogic` -> `IntegrateActorMotion`, `ProcessCharacterSpriteData` -> `LatchFrameAttackAndGuardFlags`,
`ProcessCharacterAnimationAndState` -> `RunActorStateMachineAndPostTick`, `GetAnimationFrameData` -> `GetImageRecord`, `CalculateCollisionBounds` -> `GetBoxWorldRect`, `CalculateProjectileHitbox` -> `GetAttackBoxWorldRect`,
`CalculateCharacterHitbox` -> `GetBoxWorldRectInclusive`, `GetCharacterBounds` -> `GetActorBodyBox`, `InitializeCharacterBuffer` -> `ResetActorKeepingAssets`, `InitializeSpawnedEntityFromTemplate` -> `InitializeSpawnedActorFromTemplate`,
`GenerateScriptFilenameHash` -> `CopyPathBasename`, `ProcessHeaderValidation` -> `CopyStemWithoutExtension` (these two copy the path tail and strip the extension), `LoadRawImageFromFile` -> `LoadQohImgFile`, `LoadIMGFile` -> `LoadBmpDibFromFile`,
`LoadCompressedImageFromArchive` -> `LoadPsxTimImageFromArchive`, `ProcessCharacterAnimations` (an MCI stop+close) -> `StopAndCloseMciDevice`, script VM `ExecuteScriptBytecode` -> `RunFobScript`, `LoadAndCacheScriptFile` -> `LoadFobScriptIntoBank`,
`FreeScriptCacheEntry` -> `FreeFobScriptBank`, `LoadCharacterFOB` -> `LoadCharacterFobScript`, `CleanupCharacterCollisionData` -> `FreeAllFobScriptBanks`, `SetActiveCharacterAIScript` -> `SelectActiveFobScriptBank`, VM ops `FobOp_*`, `AiScriptOp_*`.
Globals: `g_chr_cipher_key` 0x4A3834, `g_chr_cipher_key_plus1`, `g_chr_revision_stamps` 0x4A715C, `g_actor_palette_p1..p4` / `_ko_slot` (256-entry palettes, `QohPaletteEntry[256]`), `g_fob_script_banks`, `g_fob_vm_head`, `g_fob_vm_state`,
`g_fob_value_stack`, `g_fob_pc`, `g_fob_return_value` 0x6248E0, `g_fob_bank_index`, `g_camera_x/y`, `g_camera_ground_offset`, `g_player_state_timers`.
The dangling local-type reference `#200` (a deleted struct) that typed `g_player1_character..4`, `character_data` and 12 hit/state functions is replaced by `struct QohActor *`.

## 9. Evidence summary (verifier output, 2026-10-03)

```
chr files 37, fob files 30, img files 553
images 6938 (256x256 6923, 320x240 15)  tiles 115342  actions 9472 (5535 unused)  frames 43410  boxes 24242  attacks 5008
frame nextFrame out of range 5, box range violations: body 2, guard 1, attack 5 (0xFF sentinel frames), 0xFE attack counts 2
fob: 17 entries, 155220 instructions, 1224 switch tables, 50280 dead-code bytes, residual 0
ALL EXACT CHECKS PASSED
```

## 10. Unproven / open

* b0 = 0x7E is derived from the data, not read from the config file (the file that `g_encrypted_config_path` names was not opened). The cipher is exact for the shipped files either way.
* Meaning of: attack `unresolved_0A`, `unresolved_13`, `hitEffectFlags` bits 0..11, `knockMode` bits 2..9 (partly read, mapping to reactions not written down); the ~60 actor fields named `unresolved_<off>` (all accessed by code, semantics not resolved) and the gap fields
  of the actor/command child that no traced function touches (`unresolved_<off>` arrays, marked U).
* Frame bytes +0x1B, +0x1C, +0x2A/+0x2B, +0x39..0x3F are named `unused_` on the basis of a ctree scan of all 995 decompiled functions plus constancy in all 43,410 frames; a reader through a pointer the scan did not follow cannot be excluded.
* `.Img` pixel row order / nibble order; `QOHcnf.key`, `System.bin`, `.TIM`, `Bgm` formats.
* The Yosie revision (5 in the shipped file vs 6 in the December table).
* Fob: the per-character handlers (0x47..0x69) and AI queries are named but their stack contracts are not specified here; opcode 0x2F call marker value and the exact scheduling use of 0x34/0x36 are U.
