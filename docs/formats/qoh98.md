# Queen of Heart '98 (Qoh98.exe) character .dat

Status: PARTIAL. Container layout is exact and proven (tools/fb/qoh98_verify.py passes all 13 files:
size adds up to the byte, version 5, image ranges partition the tile array, tile positions 16-aligned).
Record internals below the container level are NOT yet resolved. IDB: C:\dev\ida\server\Qoh98.exe.i64
(saved; no renames/structs applied yet).

## Container (loader LoadAndParseCharacterDATFile 0x44C290, fopen-style helpers OpenFileForReading/ReadFileData)
All little endian, no encryption. Read order:

| offset | size | content |
|---|---|---|
| 0 | 4 | version = 5 (read to a throwaway buffer) |
| 4 | 24 | six u32 counts c0..c5 |
| 28 | 260*c0 | TIM path entries (read and discarded) |
| | 8*c0 | image table: (firstTile, tileCount) pairs |
| | 260*c1 | tile blocks |
| | 24*c2 | 24-byte records (c2 == 256 in every file) |
| | 52*c3 | 52-byte records (sprite definitions) |
| | 1024 | palette, copied to the caller's buffer (256 x 4 bytes) |
| | 8*c4 | 8-byte records (animation definitions) |
| | 24*c5 | 24-byte records (hurtboxes; 0 in rina.dat) |

Count to runtime struct (a1 = dword[12]): c0 -> a1[3] (array a1[2] = image table), c1 -> a1[1] (a1[0] = tiles),
c2 -> a1[5] (a1[4]), c3 -> a1[7] (a1[6]), c4 -> a1[9] (a1[8]), c5 -> a1[11] (a1[10]).
This corrects the old note, whose "animation frame count / state count" naming was wrong.

## Resolved by data (verified on all files)
- The 260-byte "state record" is a sprite TILE: u16 x, u16 y (both multiples of 16, <= 256: position in the
  TIM sheet), then 256 bytes = a 16x16 8bpp block of palette indices.
- The 8-byte "animation frame" array is an IMAGE TABLE: image i (TIM i, same order as the path table) owns
  tiles [first, first+count); entries are contiguous and sum to c1 (e.g. akari: (0,14),(14,14),...,
  final tile total 1894).
- TIM path entry (260): 8-byte short name ("Aka0000", NUL), then Shift-JIS display text, then "\\AKARI\\Aka0000.tim", rest 0xCD.
- Loaded files: .\rina\rina.dat (effects), .\System\Obj\Obj1.dat, and the two selected characters
  (.\Akari\Akari.Dat string table, 32-byte stride, indexed by selection byte at 0x4DC780 / 0x4DD490).
  Runtime char-data structs: 0x4DC6D0 (P1), 0x4DD3E0 (P2), 0x4A2680 (rina), 0x4DCFB0 (system obj);
  palette buffers 0x4A1960 (P1), 0x4A1D60 (P2), 0x4A21A0 (rina), 0x4A0DD0 (obj). Initialiser sub_44C1D0.

## NOT yet resolved (next steps)
1. 52-byte sprite record (c3), 8-byte anim record (c4), 24-byte hurt record (c5), 24-byte c2 array
   (256 entries, role unknown; first guess a lookup/hit table): decompile consumers. Primary consumer is the
   huge sub_41F4A0 (referenced from ~50 sites for the char structs); also sub_4467..sub_4453F0 etc.
   Approach: xrefs to 0x4DC6D0+0x18 (a1[6]), +0x20 (a1[8]), +0x28 (a1[10]), +0x10 (a1[4]).
2. Whether tile pixels are row-major and how palette row selection works (palette is 1024 bytes = 256 BGRA?
   engine copies 16-entry banks: sub_43AE50(slot, ptr, 16), so likely 16 colors x 4 bytes per bank, 16 banks).
3. TIM loader: no standalone TIM loader found yet; tiles inside .dat appear to replace TIM parsing for
   characters. System\SYS_OBJ.TIM / sys_eff1.tim loaders still to locate (look for 0x10 magic compare).
4. command.txt parser (System\command.txt, text), other tables.
5. Runtime player/effect structs; IDA struct creation and application (nothing applied yet).
