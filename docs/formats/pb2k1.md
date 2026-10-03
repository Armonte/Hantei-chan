# Queen of Heart 2001 ~Party's Breaker~ (pb2k1.exe): character .DAT format (PARTIAL, in progress)

Status 2026-10-03: the IDA session was stopped early (load cap). Proven below: container/cipher, header, pattern header, frame record
split, IF/EF dispatch. NOT done: sprite-section field semantics beyond the loader, AT/box record layouts, CT/CCT/WMT/CPF loaders,
IDB struct application, the 16-file verifier. Anything marked (?) is unproven. The IDB was saved and closed with no renames made.

## 1. Corrections to PB_CHAR_DAT_FORMAT_SOLVED.md
* The offset table has **256** entries (0x1C + 256*4 = 0x41C), not 250.
* Key 3 is NOT `hiまでno...`: the bytes (read from the IDB, 34 bytes before the NUL) are
  `68 69 82 DC C5 6E 6F 83 4A 82 C9 82 E5 81 48 20 82 B2 8B EA 98 4A 83 69 82 B1 54 6F 82 BE 82 C9 82 E5`
  (`hi` `ま` `ﾅ`(0xC5) `no` `カ` `にょ` `？` ` ` `ご苦労` `ナ` `こ` `To` `だにょ`). Use the hex, not a retyped string.
* Section 3 is not "raw"; it is a palettised sprite bank (section 4 here).

## 2. Archive and cipher
Stage 1 (PB/GOF1 archive, key 0xFA261EFB) is handled by fbarctool. Stage 2, `XOR_Decrypt_WithString` 0x423B90 (buf, size1, size2, key), n = min(size1,size2):
`buf[p] ^= (p + key[p % strlen(key)]) & 0xFF`, p relative to the start of the range.
Loader `Load_DAT_File_And_Runtime_Decrypt` 0x42C460(slot, name):
1. read 0x41C bytes, decrypt [0,0x41C) with K1 (`g_xor_key_memory_error` 0x4614D8, 30 bytes, hex `4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e`); h5 = u32@0x14.
2. read h5 bytes from file offset 0 (-> `0x161E6C8[slot]`), decrypt [0,0x41C) with K1 again, decrypt [0x41C, h5) with K2
   (`g_xor_key_mendou_troublesome` 0x4614B0, 38 bytes, hex `4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb`).
3. read h6+0x4000 bytes from file offset h5 (-> `0x17E9CE0[slot]`, the sprite bank), decrypt [0,h6) with K3 (`g_xor_key_hima_gokurou` 0x4614F8, hex above);
   h6 = u32@0x18; fail (return 0) unless u32@0x10 == 15; then decrypt the final 0x4000 bytes (starting at h6 in that buffer) with K3, key restarting at p = 0.
File size = h5 + h6 + 0x4000 (ASAHI: 148902 + 1370704 + 16384 = 1535990).

python (verified on ASAHI.DAT; keys K1,K2,K3 as hex above):
```
def xd(b,off,n,key):
    for p in range(n): b[off+p]^=(p+key[p%len(key)])&0xFF
def decrypt(raw):
    b=bytearray(raw); xd(b,0,0x41C,K1)
    h5,h6=struct.unpack_from('<II',b,0x14)
    xd(b,0x41C,h5-0x41C,K2); xd(b,h5,h6,K3); xd(b,h5+h6,0x4000,K3); return b
```

## 3. Header (0x41C bytes, decrypted)
| off | type | meaning |
|---|---|---|
| 0x00 | char[8] | `時は来た` (8E 9E 82 CD 97 88 82 BD), never compared by the loader (?) |
| 0x08 | u32 | 0x10000 (unread so far, ?) |
| 0x0C | u32 | 0 |
| 0x10 | u32 | 15, checked by the loader |
| 0x14 | u32 | h5 = end of the pattern area = start of the sprite bank |
| 0x18 | u32 | h6 = size of the sprite bank (excluding the 0x4000 tail) |
| 0x1C | u32[256] | absolute offset of pattern i, 0xFFFFFFFF = absent; first present = 0x41C; ASAHI: 102 present |
Pattern lookup (PB_ObjResolveFramePointers 0x424780): `pattern = charData + *(u32*)(charData + 4*patternIndex + 28)` -> obj+556.

## 4. Pattern area [0x41C, h5)
Pattern header (20 bytes): u8 frameCount, 3 bytes (?), i32 boxTableOffset (+4), i32 atTableOffset (+8), i32 ifTableOffset (+0xC), i32 efTableOffset (+0x10),
offsets relative to the pattern start, -1 = none. Frames follow at +0x14, 96 bytes each (obj+560 = pattern+20+96*frame, 0x424780).
ASAHI pattern 0: frameCount 16, box @0x614 (16 boxes of 8 bytes?), AT -1, IF @0x694 (2 records x 20), EF @0x6BC (1 record x 12), length 0x6C8.

Frame record (96 bytes), split by the engine: anim part = frame+0 (obj+564), state part = frame+24 (obj+568), index block:
* +62,+64,+66: i16 IF indices (-1 empty) -> `ifTable + 20*i` (PB_ObjRunFrameEventsForPhase 0x4280E0; phase = byte_461048[type])
* +68..+74: four i16 EF indices -> `efTable + 12*i` (PB_ObjRunFrameEntryEffects 0x413220)
* +76..+94: ten i16 box indices (obj+580); slots 0, 6, 8, 9 read by 0x430760, 0x438A10, 0x4382C0, 0x438680 (same slot roles as GOF1 gof1.md 3.3 (?))
Anim part (read offsets seen): +0 u16 sprite id, +2 u16, +4 u16, +6 u16 duration (compared in PB_ObjRunActionScript), +7 u8 draw/rotation mode
(index into 0x461108, 8 = special origin shift), +8 u8 blend, +9 u8 alpha, +10 u8 aniFlag (0 end->pattern jump +11, 1 next, 2 jump to frame +11, 3 next+land,
4 jump+land, 5 counted loop with +17 as exit frame), +11 u8 jump target, +12 u8 land target, +13 u8 (sub_426010 arg: draw priority?), +14 u16 zoom (0 = none),
+16 u8 loop count (-> obj+15), +17 u8 loop end frame. State part (offsets relative to +24): +0 u8 clear vx, +1 clear vy, +2 add vx, +3 add vy, +4/+6/+8/+10 i16 vx, vy, ax, ay
(0x425560), +12 u8 stance (obj+546), +13 u8 normal cancel(?), +15 u8 (-> obj+191 hit budget), +16 u8 canAct, +20 u32 flags (bit0 carry inertia, bit1 clear inertia),
+24 u8/u32 (command cancel, 0x426D40), +28 i16 maxVx. Table sizes: IF 20 bytes {u8 type, 3 pad, i32 param[4]}, EF 12 bytes {u8 type, u8 subType, ...}.
IF types 1..28 and 50 and EF types 1..9, 30, 50 match the GOF1 dispatch set (sub_428160 and 0x413220); GOF1 evolved from this engine.
Box (8 bytes?) and AT record layouts: NOT yet read.

## 5. Sprite bank (the buffer read at h5; size h6 + 0x4000 tail), loader Parse_Character_DAT_Data 0x42C560(slot, paletteIndex)
* +0..+19: not read (ASAHI shows text remnants of the key); +16 u32 == 255 selects the 24-bit path (3 bytes per pixel), else 8-bit palettised.
* +20: u32 groupOffset[1000] (0xFFFFFFFF = none; ASAHI 330 present), relative to the bank start.
* +4020: palettes, 256 x u32 each, selected by `paletteIndex << 10`; entry 0 is forced to 0 (transparent); 8 palettes fit before the sprite table (+4020 .. +12212, 12 spare bytes).
* +12224: sprite table, 14 bytes each, indexed by global sprite number: i16 width, i16 height, i16 x, i16 y (placement in the 256x256 texture), i16 textureSlot
  (loader rewrites it to `50*slot + 2 + slot`), i16 +10 and i16 +12 not read (ASAHI: running source x,y).
* Group (at groupOffset): 52-byte header: +0 char name[32] (e.g. `ASA0000.TIM`), +44 u32 firstSpriteIndex, +48 u16 spriteCount, then pixel data from +52:
  for each sprite, width*height 8-bit indices (or 3 bytes per pixel in the 255 mode), consecutive. 0x40CB10 converts through the palette (blit to the 256x256 surface).
* Last 0x4000 bytes of the file: 256 x 64-byte CP932 pattern names (ASAHI: starts `立ち`), decrypted with K3, editor only.

## 6. Not done (next steps)
1. Re-open C:\dev\ida\server\pb2k1.exe.i64; apply pb2k1_types.h; type obj (608-byte header), fighter slot (5864), `g_fighter_slots` 0x161E980, `g_object_slots` 0x1624528.
2. Resolve box/AT record sizes from 0x437FD0 (pushbox), 0x4382C0 (attack vs fighters), 0x439280; frame state-part bytes +13/+24 from 0x426010, 0x426D40.
3. Find loaders for .CT (4232 B, CHARSEL.CT 2356 B), .CCT (4232), .WMT (1390..1852), .CPF (56048), *COM.TXT (59048): xrefs to the extension strings; census with the register-tracking script (ANIM/STATE/IDX labels) already prototyped.
4. Verifier over the 16 files (12 chars + EFFECT + SAMPLE in 01.dat, SUBARU in 01p and 02p): offsets ascending, tables tile patterns, indices in range.
