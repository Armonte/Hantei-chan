# French-Bread RBO / GOF2 / GOF1 formats (as used by Hantei-chan)

Every field here is either measured on the real files (tool named) or traced in the IDA database (function named).
RBO = `C:\games\rbo\rbo.exe` (functions below are from that IDB). GOF2 = `GOF2.exe.i64`.
Verification tool: `han2tool` (src/han2/), run `tools/han2/run_roundtrip.sh`.

## 1. PAC archive (RBO `*.PAC`, GOF2 `data0x.dat`)

Identical in both games.

| Off | Field |
|---|---|
| 0 | u32 magic = 1 |
| 4 | u32 count XOR 0xE3DF59AC |
| 8 | count x 68-byte entries: name[60], u32 offset, u32 size XOR 0xE3DF59AC |

- Name byte j of entry i is XORed with `(i*j*3+61)&0xFF` over all 60 bytes. The bytes after the NUL are NOT zero in the
  shipped archives (decoded they hold leftovers), so a byte-exact rebuild keeps the decoded 60-byte slot (`pac::Entry::rawName`).
- File data is stored raw, packed back to back after the entry table in entry order.
- Verified (`han2tool count`, `pacrt`): entry counts DATA01 183, DATA02 70, Update01 63, Ex1 383, Ex2 316, Ex3 140;
  GOF2 data00..05 2323/192/142/3097/784/208. `pacrt` rebuilds all 12 archives byte-identical from their own entries
  (header and data), so the writer is exact.
- Extracted `ACOLYTE_F.DAT` equals `C:\games\rbo\_lineage_samples\ACOLYTE_F.DAT` byte for byte.

## 2. HAN2RBO container (RBO `.DAT`/`.DT2`, GOF2 `.DT2`)

| Off | Field | RBO .DAT | RBO .DT2 | GOF2 .DT2 |
|---|---|---|---|---|
| 0x00 | "HAN2RBO " | | | |
| 0x08 | kind | 0 | 3 | 3 |
| 0x10 | version | 2 | 2 | 2 |
| 0x18 | sub | 1 | 1 | 2 |
| 0x1C | XOR flag | 0 | 0 | 0 |
| 0x20 | 4 x {off,size}: pattern area, parts (PAT), CG (BMP Cutter3), names (256 x 64) | all | pattern only | pattern only |

- The four areas follow the 0x40 header back to back (checked: every one of the 346 + 68 files). A `.DT2` keeps the `.DAT`'s offsets for
  the absent areas and size 0.
- Loader: `Han2Dat_LoadPatternSectionPreferDt2` 0x403690. It takes the pattern area from `<name>.DT2` when that exists
  (header read by `Han2Dat_ReadFileHeader32` 0x4035F0 / `Han2Dat_ReadPatternHeader32` 0x403610), and parts (`Han2Dat_LoadPartsPatBlock` 0x4037D0) and CG
  (`Han2Dat_LoadCgBank` 0x4038F0) always from the `.DAT`.
- Pattern area: `Han2Dat_LoadPatternAreaAndResolve` 0x403630 -> `Han2Dat_ComputeSectionPointers` 0x403550. The header is 11 lead dwords, 8 section
  sizes (dwords 11..18) and 3 trailing dwords = 0x58 bytes (RBO). GOF2 is 12 + 9 + 3 = 0x60 bytes. Sections follow the header in order; a size 0 = absent.
- XOR: `Han2Dat_DecryptBlockIfFlag` 0x403430 (flag at +0x1C). No shipped file sets it; Hantei-chan refuses such a file and always writes 0.
- Verified (`han2tool roundtrip`): 346 RBO (215 .DAT + 131 .DT2) and 68 GOF2 .DT2 parse and serialize byte-identical.

### 2.1 Pattern-area sections (engine indices from the runtime container `actor+2032`: +4 = section 0, +8 = section 1, ...)

| # | Content | Stride | Evidence |
|---|---|---|---|
| 0 | pattern table, 256 entries | 12 | `Actor_GetFrameRecord` 0x4408F0: `+0` frame count (non-zero = pattern exists, `Actor_GetFrameSpriteId` 0x440B10), `+4` byte flags (bit 0x40 read by `Actor_CacheCurrentFrame` 0x440920), `+8` index of the first frame |
| 1 | frame records | 300 (RBO), 404 (GOF2) | `Actor_GetFrameRecord`: `frames + 300 * (firstFrame + frameNo)` |
| 2 | box rectangles | 8 (4 x i16 x1,y1,x2,y2) | `Actor_CollectAttackBoxes2` 0x440F40 etc.: `boxes + 8 * idx`; `Actor_PickBoxRect_244` 0x442190 copies 4 shorts |
| 3 | attack (AT) records | 120 (RBO) | `Actor_CollectAttackBoxes2`: `sec3 + 120 * frame[+200]` (the earlier "80-byte" guess was wrong: 6720 = 56 x 120) |
| 4,5 | small tables (RBO 196 B / 40 B; GOF2 empty) | ? | not yet traced |
| 6,7 | script lists, 20 bytes = 5 dwords | 20 | `Actor_RunFrameScriptList6` 0x41DA50 (frame[+188], section 6) and `Actor_RunFrameScriptList7` 0x41F900 (frame[+192], section 7); GOF2 has a 9th (sec 8) |
