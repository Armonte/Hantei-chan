# Queen of Heart '98 (Qoh98.exe, Himeya Soft 1998): character .dat, TIM/BMP/WAV/MIDI loaders, config text, runtime entity

Scope: the 32-bit `Qoh98.exe` (MSVC, base 0x400000, 677 functions, DirectDraw 320x240 8-bit, IDB `C:\dev\ida\server\Qoh98.exe.i64`) and the
install `C:\games\qoh98`. IDA types: `docs/formats/ida/qoh98_types.h` (exactly as applied in the IDB; sizes machine-checked:
Qoh98DatHeader 0x1C, Qoh98TimPathEntry 0x104, Qoh98ImageTileRange 8, Qoh98Tile 0x104, Qoh98AnimRecord 0x18, Qoh98Sprite 0x34,
Qoh98BodyRect 8, Qoh98HitBox 0x18, Qoh98CharData 0x30, Qoh98Entity 0x3F4). Verifier: `tools/fb/qoh98_verify.py` (exit 0 = every layout below is exact on
all 13 character `.dat` + `System/obj/obj1.dat`, 16 TIM, 113 BMP, 187 WAV, 13 MIDI, `command.txt`).

Evidence tags: **T** = access traced in the decompiled code, **D** = verified on the shipped files by the verifier, **U** = unproven.
Offsets hexadecimal, little endian. Hex-Rays offsets in the IDB are decimal; everything here was converted with python.

The prior notes (`C:\games\qoh98\docs\DAT_FORMAT_COMPLETE.md`) were wrong in their naming: the "animation frame" array is an image table, the
"260-byte state record" is a 16x16 pixel tile, the "hitbox" array is the animation table, the "hurtbox" array is the attack box table. Corrected below.

## 1. Files the game reads

| file(s) | reader | model |
|---|---|---|
| `<name>\<name>.dat` x13 (akari aoi ayaka kotone multi remy rina rio serika serio siho tomoko yuki), `System\Obj\Obj1.dat` | `LoadAndParseCharacterDATFile` 0x44C290 | section 2 |
| `System\*.tim` (`SYS_OBJ.TIM`, `sys_eff1.tim`), `System\face\*.tim` | `Image_LoadFileToSurface` 0x44C4C0 (isTim=1) -> `Tim_LoadTo256x256Dib` 0x460460 | section 3 |
| `System\*.bmp` (sys_font, Select, Title, bg, Ending, WinMes, face\ranyu) | `Image_LoadFileToSurface` (isTim=0) -> `Bmp_LoadDibFromFile` 0x440FA0 | section 3 |
| `<name>\NN.wav`, `System\wav`, `System\Koe\Syskoe`, `System\Obj\14.wav` | `Sound_LoadWavToSlot` 0x422380 | section 4 |
| `System\Bgm\Midi\*.mid` | MCI (`open %s` / `play %s notify` / `seek %s to start` / `stop %s` / `close %s`, `sub_401000` family) | section 4 |
| `System\command.txt` | `Config_LoadCommandTxt` 0x401450 | section 5 |
| `System\QOHcnf.cnf` (not shipped; also looked up as `<WindowsDir>\QOHcnf.cnf`) | `WinMain` | section 5 |
| `System\QOHerror.txt` | written by `sub_45DAD0` (error log), never read | n/a |

The `.dat` per-character TIM path table names the original source TIMs (`Aka0000.tim` ...) but the game never opens them: the pixels are inside the `.dat`.
The data directories `System\Bgm\wave` (BgmType Wave) and `System\Koe\Syskoe\Full_*.wav` partly do not exist in the install; the code has the strings for the
16-character roster (Akari Serika Tomoko Siho Aoi Multi Kotone Remy Rio Ayaka Chizuru Azusa Yuki Kaede Hatune Serio) but only the 13 shipped characters exist.
Rina is not a fighter: `.\rina\rina.dat` is loaded into `g_RinaEffectData` (effects) and the 15 fighter slots are the `.\<Name>\<Name>.Dat` table at 0x475290 (32-byte stride, selection byte per player).

## 2. Character .dat

### 2.1 Container (T: LoadAndParseCharacterDATFile 0x44C290; D: all 14 files exact to the byte)

No encryption, no compression. Read order (C0..C5 = the six header counts):

| offset | size | content |
|---|---|---|
| 0x00 | 4 | `version`, read into a throwaway buffer, never compared; 5 in every file |
| 0x04 | 24 | C0 imageCount, C1 tileCount, C2 animCount, C3 spriteCount, C4 bodyRectCount, C5 hitBoxCount |
| 0x1C | 260*C0 | `Qoh98TimPathEntry[C0]`: read into a stack buffer and DISCARDED |
| | 8*C0 | `Qoh98ImageTileRange[C0]` -> CharData.images (+0x08), count -> +0x0C |
| | 260*C1 | `Qoh98Tile[C1]` -> CharData.tiles (+0x00), count -> +0x04 |
| | 24*C2 | `Qoh98AnimRecord[C2]` -> CharData.anims (+0x10), count -> +0x14 (C2 = 256 in all files) |
| | 52*C3 | `Qoh98Sprite[C3]` -> CharData.sprites (+0x18), count -> +0x1C |
| | 1024 | palette, copied to the caller's buffer (`g_Palette1P/2P/Rina/Obj`) |
| | 8*C4 | `Qoh98BodyRect[C4]` -> CharData.bodyRects (+0x20), count -> +0x24 |
| | 24*C5 | `Qoh98HitBox[C5]` -> CharData.hitBoxes (+0x28), count -> +0x2C |

`Qoh98CharData` (48 bytes, 12 dwords) is the loader output and the first 0x30 bytes of every runtime entity (section 6). Allocation is `malloc` per array; a failed
allocation writes `error:%s` through `sub_45DAD0` and returns 0. `CharData_Free` 0x44C1D0 frees and nulls the six arrays.

Counts (D): akari 156/1894/256/634/395/85, aoi 136/1879/256/738/692/136, ayaka 164/2375/256/807/498/68, kotone 225/3106/256/832/445/82,
multi 159/2236/256/661/414/91, remy 152/2301/256/619/555/60, rina 9/118/256/29/3/0, rio 156/2481/256/694/436/84,
serika 181/3533/256/654/283/73, serio 219/3435/256/589/297/63, siho 129/1583/256/528/472/110, tomoko 131/2010/256/643/377/77,
yuki 152/2086/256/828/511/102 (image/tile/anim/sprite/body/hit).

### 2.2 Qoh98TimPathEntry (260 bytes) - not used at run time (D)
`+0x00` char[8] short name ("Aka0000", NUL, rest 0xCD fill); `+0x08` char[252] CP932 display text then the original build path (`...\AKARI\Aka0000.tim`), NUL, 0xCD fill.
Entry i corresponds to image table entry i. An editor should preserve it byte-for-byte.

### 2.3 Qoh98ImageTileRange (8 bytes)
`int firstTile`, `int tileCount` (T: `Entity_DrawSpriteWithShadow` 0x416EB0 reads `*(images + 8*img)` and `+4`). D: ranges are contiguous from 0 and the counts sum to C1.
An image is one 256x256 sprite canvas, built from at most 256 tiles (one TIM of the original build).

### 2.4 Qoh98Tile (260 bytes) - blitter input (T: `DrawQueue_AddTile` 0x415350, `DrawQueue_AddTileShadow` 0x4160D0)
| off | type | meaning |
|---|---|---|
| +0x00 | int16 x | destination column of the 16x16 tile inside the image canvas, multiple of 16, 0..240 (D) |
| +0x02 | int16 y | destination row, multiple of 16, 0..240 (D) |
| +0x04 | uint8[256] pixels | 16x16 palette indices, row-major (D: rendering akari images 3 and 20 with this layout and the BGRX palette gives the correct sprite; no tile overlap inside an image; all values 0..15, index 0 = transparent) |

The blitter queues a 40-byte draw command per tile (destination x/y, w=h=16, pixel pointer = tile+4, palette bank, `tileXform`, flags). The destination of the tile
for `Qoh98TileXform` m (W = H = 256, tile at x,y) is, T (`DrawQueue_AddTile` cases 0..7):

| m | name | tile destination |
|---|---|---|
| 0 | IDENTITY | (x, y) |
| 1 | ROT_90_CW | (W-16-y, x) |
| 2 | ROT_180 | (W-16-x, H-16-y) |
| 3 | ROT_90_CCW | (y, H-16-x) |
| 4 | FLIP_H | (W-16-x, y) |
| 5 | TRANSPOSE | (y, x) |
| 6 | FLIP_V | (x, H-16-y) |
| 7 | ANTI_TRANSPOSE | (W-16-y, H-16-x) |

Facing left XORs the sprite's `tileXform` with 4 (FLIP_H composition). The pixel orientation inside a tile for m != 0 is done by the rasteriser, which was NOT traced (U);
all shipped sprites that were rendered (m = 0) are exact. The shadow pass (first argument 0 of `Entity_DrawSpriteWithShadow`) draws the same tiles with y scaled by 1/3.

### 2.5 Palette (1024 bytes)
256 entries of 4 bytes, byte order B, G, R, 0 (D: all X bytes 0; entry 4 of akari = `d8e8f800` = skin colour only as BGR). Values are multiples of 8 (PS1 5-bit scaled by 8).
Tile pixels use only 0..15, so a character is 16-colour banks: akari has banks 0..2 populated (entries 0..47 = three colour variants), 48..255 are zero (D).
The game copies a 16-entry window from `g_Palette1P + 4*<variant byte>` (`byte_4DC781` / `byte_4DD491`, P1/P2 colour selection) into the DirectDraw palette (T: `sub_4133F0`);
whether the variant byte stores `16*n` or `n` was not decided (U).

### 2.6 Qoh98AnimRecord (24 bytes, 256 per file; index = animation id = entity.animId)
| off | type | meaning |
|---|---|---|
| +0x00 | int32 firstSprite | first sprite record; -1 = animation not present (T: `Entity_AdvanceFrame` tests `== -1`) |
| +0x04 | int32 frameCount | number of sprites; the step code resets to frame 0 when `frameCount < nextFrame` (T). D: sprite ranges of the used animations follow each other in file order |
| +0x08 | int32 bodyRectBase | base into `bodyRects`; sprite fields +0x17/+0x19/+0x1D are offsets from it (T) |
| +0x0C | int32 bodyRectCount | D: contiguous allotment of this animation; never read by the game |
| +0x10 | int32 hitBoxBase | base into `hitBoxes`; sprite field +0x1F is an offset from it (T) |
| +0x14 | int32 hitBoxCount | D: contiguous allotment; never read |

Entries with `firstSprite = -1` keep stale values in the other fields; they are never read (verifier ignores them).

### 2.7 Qoh98Sprite (52 bytes) - one animation frame
Current sprite of an entity = `sprites[anims[animId].firstSprite + frameIndex]`.
| off | type | name | meaning |
|---|---|---|---|
| +0x00 | int32 | imageIndex | image table index; outside [0,imageCount) draws nothing (T) |
| +0x04 | int16 | drawX | canvas x offset relative to entity x (negated when facing left) (T) |
| +0x06 | int16 | drawY | canvas y offset (T; /3 in the shadow pass) |
| +0x08 | int32 | duration | copied to entity.frameTimer (T); D: bytes 2..3 always 0 |
| +0x0C | uint16 | nextFrame | frame index inside the animation; 0xFFFF = frame+1, 0xFFFE = animation ends (T) |
| +0x0E | uint16 | attackLevelAndFlags | low nibble -> entity.attackLevel; bit 0x10 arms the hit boxes (entity.boxFlags |= 1); byte +0x0F always 0 (T,D) |
| +0x10 | uint8 | moveFlags | 1 = load accel from +0x22/+0x26, 2 = from +0x24/+0x28, 4 = on landing jump to anim `landAnim` (T) |
| +0x11 | uint8 | clearVelX | non-zero zeroes velX (T) |
| +0x12 | uint8 | clearVelY | non-zero zeroes velY (T) |
| +0x13 | uint8 | xAccelAbsolute | copied to entity +0xAC; when set the accel clamps at sign change (T, `Entity_StepPhysics`) |
| +0x14 | uint8 | yAccelAbsolute | copied to entity +0xAD (T) |
| +0x15 | uint8 | tileXform | `Qoh98TileXform` 0..7 (T,D) |
| +0x16 | uint8 | landAnim | animation entered on landing when moveFlags & 4 (T) |
| +0x17 | uint8 | anchorRect | body rect offset used to re-centre the sprite when the entity turns, and for screen edge push (T: `Entity_FlipFacingKeepAnchor`, `Entity_ScreenEdgeCheck`, `Entity_TurnTowardIfFacingAway`) |
| +0x18 | uint8 | anchorMode | 0xFF = no anchor; 0/1/2 observed (T) |
| +0x19 | uint8 | hurtRectFirst | offset of the first hurt rect from bodyRectBase (T: `Entity_FindHitBoxVsHurtRect`) |
| +0x1A | uint8 | hurtRectCount | 0 = cannot be hit (T) |
| +0x1B,+0x1C | uint8 | unused_1B, unused_1C | 0 in all 14 files (D), never read |
| +0x1D | uint8 | grabRectFirst | first grab-vulnerable rect, offset from bodyRectBase (T: `Entity_FindHitBoxVsGrabRect`) |
| +0x1E | uint8 | grabRectCount | non-zero also sets entity.boxFlags |= 2 (T) |
| +0x1F | uint8 | hitBoxFirst | first attack box, offset from anim.hitBoxBase (T) |
| +0x20 | uint8 | hitBoxCountHere | number of attack boxes, 0 = none (T) |
| +0x21 | uint8 | stanceBits | copied whole to entity.stanceBits (+0x9C): low 3 bits stance class, 0x40 = turn toward the opponent through `Entity_TurnTowardIfFacingAway` at frame start, 0x20/0x80 read by the hit code (T) |
| +0x22 | int16 | xAccel | entity.accelX (negated when facing left, and by hit direction) (T) |
| +0x24 | int16 | yAccel | entity.accelY (T) |
| +0x26 | int16 | xJerk | entity.jerkX (T) |
| +0x28 | int16 | yJerk | entity.jerkY (T) |
| +0x2A,+0x2B | uint8 | unused_2A, unused_2B | 0xCD (7106 sprites) or 0 (1150), uninitialised build fill, never read (D) |
| +0x2C | int32 | aiMoveClass | read only by the CPU player code (`sub_404300` and 21 siblings through `*(a2[6] + 52*frame + 44)`); values 0..11 (T); bytes 1..3 are 0 (D) |
| +0x30 | uint32 | fxFlags | low nibble: sound slot (+16*player), bits 4..7: second sound bank, 0x10000 / 0x200000 spawn effects (`sub_41BF30`), 0x20000 throw effect, 0x40000 sets mode 4, 0x80000 clears combo state (T, `Entity_AdvanceFrame`); byte +0x33 is 0 (D) |

### 2.8 Qoh98BodyRect (8 bytes) and Qoh98HitBox (24 bytes)
Rectangles are relative to the entity position (entity.posX/posY); left/top/right/bottom are int16, the right/bottom edge is inclusive (T: `Entity_GetBodyRectWorld`
adds 1). Facing left mirrors x as `256 - left` / `255 - right` around posX (T). Used with Win32 `IntersectRect`.

`Qoh98BodyRect`: `int16 left, top, right, bottom` (hurt / grab / anchor rectangles, selected by sprite fields +0x17/+0x19/+0x1D).

`Qoh98HitBox`:
| off | type | name | meaning |
|---|---|---|---|
| +0x00..+0x06 | int16[4] | left, top, right, bottom | `Entity_GetHitBoxWorld` 0x409630 (T) |
| +0x08 | int16 | damage | life removed; 0 = no damage; the detail resolver scales it by defender stance (1: /4, 2: x125 /100) (T) |
| +0x0A | int16 | unresolved_0A | 0..10, no reader found (D) |
| +0x0C | int16 | guardDamage | subtracted from the defender guard gauge (entity +0x380) (T) |
| +0x0E | int16 | meterGain | `sub_41C260` super gauge gain for the attacker (T) |
| +0x10 | uint8 | flags | bit0 = damaging hit (T) |
| +0x11 | uint8 | defenderStateMask | bit0/1/2: hits a defender whose state value (entity +0x48) is 0x12 / 0x13 / 0x14 (T: `Entity_ResolveHitDetailed` switch) |
| +0x12 | uint8 | hitClass | 0..15 (D); 0..2 reaction height row (anim 21/23, entity +0x9D = 0/1/2), 3..5 second row (anim 22), 6/7/8/9 grounded and launch reactions (33, 25, 24), 10 -> 36, 11 -> 35/21, 13..15 projectile clash rules (`Entity_ProjectileClashTest` compares 13..14) (T) |
| +0x13 | uint8 | unused_13 | 0 in all 14 files (D) |
| +0x14 | uint16 | knockMode | bits 0..1 -> defender entity +0x38C; 1 turns the defender away (T); byte +0x15 is 0 (D) |
| +0x16 | uint8 | unresolved_16 | 0 or 1 in 102 of 1031 boxes, no reader found (D) |
| +0x17 | uint8 | unused_17 | 0 in all 14 files (D) |

## 3. TIM and BMP images (T: Image_LoadFileToSurface 0x44C4C0)

`Image_LoadFileToSurface(isTim, path, &surface, &w, &h, &palette, paletteCount, paletteSlot, transparent)` loads the file into a memory DIB, copies `paletteCount`
palette entries to the caller, creates a DirectDraw surface of the DIB size (`sub_431660`) and blits the pixels (`sub_437960`, or `sub_437D90` with colour key when `transparent`).
Used by `Graphics_LoadSystemImages` 0x412B50: `Sys_obj.tim` (16 colours -> slot 192), `sys_font.bmp` (16 -> 208), `sys_eff1.tim`, `face\cut_<char>.tim` (32 -> slot 64, falls back to
`cut_aka.tim`), `cut_bg.tim`, `Pause.tim`, `face\ranyu.BMP`, bg and Select bitmaps.

### 3.1 TIM (Tim_LoadTo256x256Dib 0x460460) - PlayStation TIM, fixed 256x256
| off | size | field | use |
|---|---|---|---|
| 0x00 | 4 | magic (0x10 in a real TIM) | NOT read; the shipped files have it zeroed (D) |
| 0x04 | 4 | flags | `flags & 7`: 0 = 4bpp, 1 = 8bpp, 2 = 16bpp, 3 = 24bpp; the loader only accepts <= 8bpp (else returns NULL); bit 3 (8) = CLUT present |
| 0x08 | 4 | CLUT block size `bnum` | `(bnum-12)/2` RGB555 entries are converted; the CLUT block is `bnum` bytes: 12-byte header then the entries |
| 0x14 | 2*n | CLUT entries, u16 RGB555 | converted to BGRX bytes: B = 8*c, G = (c>>2)&0xF8, R = (c>>7)&0xF8, X = 0 |
| after CLUT | 4 | image block size | NOT read; the image data is found at `8 + bnum + 12` |
| +12 | | pixels | exactly 65536 bytes are consumed |

Pixels: 8bpp reads 256 columns per row; 4bpp reads 128 bytes per row (low nibble = left pixel, high nibble = right pixel); 256 rows; rows are written bottom-up into a 256x256 8-bit DIB
(the loader ignores the TIM width/height fields; D: every shipped TIM is 256x256: 8bpp = 128 words, 4bpp = 64 words, 256 rows). The CLUT x/y and image x/y VRAM fields are ignored.
D: SYS_OBJ.TIM and sys_eff1.tim and Pause.tim = 4bpp (16 / 16 / 256-entry CLUT), the 13 `face\*.tim` = 8bpp with a 256-entry CLUT.

### 3.2 BMP (Bmp_LoadDibFromFile 0x440FA0)
Opens with `OpenFile`, reads 14 bytes (BITMAPFILEHEADER), takes `bfSize` (dword at offset 2) and reads `bfSize - 14` bytes: the DIB in memory is BITMAPINFOHEADER + palette + bits.
Never checks `BM`: the shipped bitmaps start with `00` (0x3030) instead of `BM` (D, all 113). `bfSize` must equal the file size (D). Accessors: `Dib_GetWidth` 0x440F40 = biWidth, `Dib_GetHeight` 0x440F50 = biHeight,
`Dib_GetPalette` 0x440F90 = DIB + 40, `Dib_GetPixels` 0x440F60 = DIB + biSize + 4*(biClrUsed or 1<<biBitCount) (24-bit: no palette). D: all bitmaps are 8bpp, uncompressed, biSize 40,
`bfOffBits == 54 + 4*colours`. They are also the source of the bottom-up row order.

## 4. Audio
- WAV (`Sound_LoadWavToSlot` 0x422380): `OpenFile`+`_lread` the whole file, requires the first dword `RIFF`, scans byte by byte for the dwords `WAVE`, `fmt ` and `data`, builds a static DirectSound
  buffer (flags 32992 = 0x80E0) from the `fmt ` chunk, copies the `data` chunk. Sound slots: 1..16 = P1 voice `<Name>\NN.wav`
  (`NN` is `%02d` below 10 else `%d`, loaded by `Game_LoadSoundsAndCharacterData` for slots `i+1` (P1) and `i+17` (P2)), 41..51 = `System\wav\*` effects, 61..70 = `System\Koe\Syskoe\*` announcer.
  D: all 187 shipped WAVs have `RIFF`/`WAVE`/`fmt `/`data`.
- MIDI: `System\Bgm\Midi\<Name>.mid` (+ `Select`, `Title`, `Win`, `Ranyu`) are standard MIDI files (D: all 13 start with `MThd`) played through MCI string commands. Wave BGM (`System\Bgm\wave`) is selected by
  `BgmType Wave` but is not shipped.

## 5. Text and config

### 5.1 System\command.txt (Config_LoadCommandTxt 0x401450; this is a launcher/option file, NOT a move-command table)
The whole file is read, two NUL bytes appended, then tokenised by `Text_NextToken` 0x401250: tokens separated by space, TAB or CR LF; `//` starts a comment to the end of the line; CP932 lead bytes
(0x81..0x9F, 0xE0..0xFC) consume two bytes. Each recognised keyword reads the NEXT token as its value; an unrecognised token is ignored (the shipped file holds `BarbarianTest`, `yukitest`, `seriotest`).
| keyword | value | global |
|---|---|---|
| `WaitCount` | int | `g_CfgWaitCount` (also resets the frame skip counter) |
| `ExtraSkip` | int | `g_CfgExtraSkip` |
| `OpenScreenType` | int | `g_CfgOpenScreenType` |
| `CenterFlag` | `True` / `False` | `g_CfgCenterFlag` |
| `KeyBordOffset` | True/False | `g_CfgKeyboardOffset` |
| `YukiTest`, `SerioTest` | (no value) | `g_CfgYukiTest`, `g_CfgSerioTest` = 1 (enable the unreleased characters) |
| `SystemPause` | True/False | bit 2 of `g_CfgFlags_1ExtraFunction_2SystemPause` |
| `ExtraFunctionEnable` | True/False | bit 1 of the same |
| `ScreenMode` | `Window` / `FullScreen` | `g_CfgScreenMode_0Window_1Full` |
| `BgmType` | `Midi` / `Wave` / `None` | `g_CfgBgmType_1Wave_2Midi_3None` = 2 / 1 / 3 |
| `DoubleBgMode` | True/False | `g_CfgDoubleBgMode` |
| `DebugWindow` | True/False | `g_CfgDebugWindow` |
| `Controller1p`, `Controller2p` | `Auto` `KeyBord1` `KeyBord2` `Pad1` `Pad2` | `g_CfgController1p/2p` = 0..4 |
A missing file prints `warning:Commad.txtが読めませんでした。`. Values are compared with `strcmp` (case-sensitive) except where noted by the code: the keyword tests are also exact.

### 5.2 QOHcnf.cnf (WinMain; not shipped, created by the game)
Binary: 0x64 bytes into `g_QohCnfBlock100` (0x4D4E60, holds the option dwords 0x4D4E80..), then 0x640 bytes into `g_QohCnfTable1600` (0x4DD8F0): 16 characters x 5 records of 5 dwords
(initialised in `WinMain` to `{character, 1000*n, n, 100*n, 36000*k}` with n = 5..1, k = 1..5: a score/ranking table; field meaning U). Looked up in `.\System\QOHcnf.cnf`, else in the Windows directory.

### 5.3 Win-quote bitmaps
`WinMes_BuildBmpPath` 0x44C7B0 builds `.\System\WinMes\<three-letter name>_mes<N>.bmp` (N 1..6) from the winner id and the loser id (fixed N for specific pairs, otherwise `rand()%3+1`).

## 6. Runtime structs

### 6.1 Qoh98Entity (1012 = 0x3F4 bytes) - fighter prefix, projectile, effect and template all share it
Pools (T): projectiles `g_ProjectilePool[60]` 0x4C5F70 and effects `g_EffectPool[30]` 0x4D4ED0, `g_EffectPool2[30]` 0x498690 (slot free when `kind` (+0xB0) == 0xFF);
fighters `g_Fighter1` 0x4DC6D0 and `g_Fighter2` 0x4DD3E0 (stride 0xD10: the fighter is the entity plus a larger tail; the `Obj1.dat` effect template `g_ObjTemplateEntity` lies at 0x4DCFB0 = fighter 1 + 0x8E0).
New effects are `qmemcpy(pool slot, g_ObjTemplateEntity, 1012)` (`Effect_SpawnFromObjTemplate` 0x414070, `Effect_SpawnFourFromObjTemplate` 0x41D030, `Effect_SpawnRandomSparkFromObjTemplate` 0x41ED40) followed by setting
animId (+0x30) / nextFrame (+0x34) / posX / posY / facing (+0xAE) / alive.

Fields resolved (full list with unresolved gaps in `qoh98_types.h`):
| off | name | T |
|---|---|---|
| +0x00 | `Qoh98CharData data` | sprite/animation data by value (spawned objects copy the owner's) |
| +0x30 | animId | current animation |
| +0x34 | nextFrame | frame after the current one; 0xFFFE = ends |
| +0x38 | frameTimer | decremented per tick from sprite.duration |
| +0x40 | frameIndex | current frame inside the animation |
| +0x58 | animEnded | set 1 at animation end |
| +0x5C | reactionAnim | reaction anim requested by hits |
| +0x60..+0x6A | velX, velY, accelX, accelY, jerkX, jerkY (int16, 8.8 fixed point) | `Entity_StepPhysics` 0x419A10: vel += accel, accel += jerk, pos += vel/256 |
| +0x6C..+0x76 | pushVelX/Y, pushAccelX/Y, pushJerkX/Y | knock back groups |
| +0x78..+0x84 | slideVelX, slideAccelX, slideJerkX, slideTimer | second x group with a countdown |
| +0x88, +0x8C | posX, posY (int32) | rectangles are relative to these |
| +0x9C | stanceBits | sprite.stanceBits |
| +0x9D | reactionHeight | 0/1/2 |
| +0x9E | attackLevel | sprite.attackLevelAndFlags & 0xF |
| +0xA0 | boxFlags | bit0 hit boxes armed, bit1 grab rects present |
| +0xAC / +0xAD | xAccelAbsolute / yAccelAbsolute | |
| +0xAE | facing | 0 right, 1 left |
| +0xAF | side | owner player 0/1 |
| +0xB0 | kind | 0xFF = free pool slot |
| +0x374 | damageTaken | accumulated life loss; entity is alive while `(life terms at +0x370/+0x378) - damageTaken >= 0` |
| +0x380 | guardGauge | reduced by hit.guardDamage |
| +0x38C | knockMode | hit.knockMode & 3 |
| +0x3A0 | edgeFlags | bit0 set by `Entity_ScreenEdgeCheck` |
| +0x3A4 | lastHitResult | |
| +0x3B0 | previousFacing | |
Unresolved (read by many sites, meaning not decided, named `unresolved_<off>` in the IDB): +0x3C, +0x44, +0x48 (hit gating state, values 0/0x12/0x13/0x14/35), +0x4C (98 sites), +0x50, +0x54 (34 compared in the hit code),
+0x7A/+0x7E/+0x82/+0x86, +0x90, +0x94, +0x98, +0xA4 (pointer to the owner control block, 275 sites), +0xB1, +0xB4..+0x373, +0x378, +0x37C, +0x384, +0x388, +0x390.., +0x3A8, +0x3B4...

### 6.2 Frame step (T: Entity_AdvanceFrame 0x419470)
`frame = frameIndex; nextFrame = sprite.nextFrame` (0xFFFF -> frame+1, 0xFFFE kept); loads `stanceBits`, `attackLevel`, plays the sound slots in `fxFlags`, applies the move flags (accel/jerk,
velocity clears, absolute modes, hit direction and facing sign), `frameTimer = duration`, re-aligns on facing change using the anchor rect, sets `boxFlags |= 1` when bit 0x10 of sprite +0x0E is set and `|= 2` when grabRectCount != 0.
`Entity_StepPhysics`: when the entity lands (posY>0 with positive velocity and not locked), the next frame becomes `landAnim` if moveFlags & 4, else 0xFFFE.

## 7. Unproven (honest list)
1. Pixel orientation inside a tile for tileXform != 0 (rasteriser not traced); mode table above is the tile POSITION mapping only.
2. Whether the palette variant byte is a bank number or a bank offset.
3. `Qoh98HitBox.unresolved_0A`, `.unresolved_16`; the unresolved runtime entity fields; the fighter tail beyond +0x3F4 (stride 0xD10) and the controller/AI block (`unk_4DDF40`, 408 bytes per player).
4. `QOHcnf.cnf` record meaning; rasteriser/surface blit details (`sub_437960`/`sub_437D90`).
5. Exact semantics of `anchorMode` values 0/1/2 and of the `aiMoveClass` values (only 3 and 5 seen in `sub_42B1B0`: projectile-like threats).
