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

## 3. RBO frame record (300 bytes, section 1) and the other typed records

Field-by-field tables (offset, type, name, proving IDA function) are generated from the IDA structs and live in
[rbo_frame_container_fields.md](rbo_frame_container_fields.md). Source of truth: `docs/formats/ida/rbo_frame_types.h` and
`rbo_container_types.h` (the same text is loaded into the RBO IDB with `idc.parse_decls`, and `tools/ida/gen_cpp_types.py`
turns it into `src/han2/rbo_types_gen.h` with `static_assert`s on every size and offset). The AT record, script lists, PAT
and CG are in `docs/formats/ida/rbo_at_record.md` and `rbo_scripts_pat.md` (C++ twins `src/han2/rbo_at_record.h`,
`rbo_scripts_pat.h`).

Invariants measured on all 346 RBO files / 113 165 frames and relied on by the writer (`framedata_han2.cpp`):

- Frames are packed in pattern order; pattern table entry = {frame count, flags (0, 2, 0x40, 0x80), first frame}.
- Box table (section 2): every box slot of every frame is numbered in first-use order, walking frames in order and slots in
  ascending frame offset (0xD4 .. 0x128). Re-references of an earlier index are shared slots. 342 files use every rectangle;
  4 have unreferenced rectangles after the last one (kept verbatim as `Han2Container::boxTail`). `EMO.DAT` and `SYSTEMEFFECT`
  hold frame slots whose index points past the box table (kept verbatim, no rectangle).
- Each box group count field equals the number of slots whose index is not -1 (holes occur: 177 hurt, 37 attack, ...).
- `hasAttack` (+0xC4) == (attackBoxCount > 0) == (attackRecordIdx >= 0). AT records are numbered sequentially in frame order, no sharing.
- Script lists A and B (sections 6, 7): record 0 is a dummy, records are numbered in first-use order, sharing happens.
- Box rectangles are game pixels relative to the actor anchor and are added to the actor position unscaled
  (`BoxRect_ApplyFacing` 0x440BB0, `Actor_GetAttackBoxWorldRect` 0x440C70); there is no MBAC-style (128,224) bias.
- CG images are drawn at actor + frame offset + CG canvas position (`Actor_DrawCgSprite` 0x4470E0); Hantei-chan's renderer shifts CG
  layers by (-128,-224), so the model stores CG frame offsets as raw + (128,224) (parts frames: raw).

Open conflict, recorded honestly: the script/PAT trace (`rbo_scripts_pat.md` section 1) reads frame +0xD0 / +0xE0 as indices into
sections 4 / 5 (which the engine never reads). The data says otherwise: both fields are group counts with the slots that follow
holding box-table indices, and the first-use numbering of the box table only holds when those slots are included. They are typed
as box groups (`sousai` / `tobi`, matching GOF2's `CAppHanteiSousai` / `CAppHanteiTobi` slot counts 3 / 4). Sections 4 and 5
stay opaque blobs until a consumer is found.

## 4. Parts (PAT v3 / v4) and CG

See `docs/formats/ida/rbo_scripts_pat.md` section 3/4. `src/han2_pat.cpp` converts a PAT block to Hantei-chan's Parts model:
pose offset table -> part sets, every 92-byte part record -> a part property plus a (texture, source rect, quad size, origin)
cutout, textures are B,G,R,A squares of 256 or 512 px. Pose slots with `src_w == 0` or `texture_index == 0xFFFF` (clip rectangle,
which the engine does not draw) are skipped.

## 5. GOF2 (HAN2RBO sub 2 + PAT v4 + CHP)

Full field tables with IDA evidence: [gof2_frame.md](gof2_frame.md) (404-byte frame, box slot list, invariants, differences from RBO),
`docs/formats/ida/gof2_frame_types.h` / `gof2_container_types.h` (IDA decl source, generated C++ `src/han2/gof2_types_gen.h`).
The editor (`framedata_han2.cpp`, `Layout` structs) handles both layouts: 12+9+3 dword pattern-area header, 404-byte frames,
236-byte attack records (kept verbatim; new attacks start zero-filled), 24 box slots in ascending frame offset, script lists A/B
(sections 6/7), effect-spawn records (section 8, frame +0x190, 1-based, record 0 dummy), sections 4/5 kept as blobs.
Verified (`han2tool modelrt`): 68/68 GOF2 .DT2 load through the model and save byte-identical.
Parts: `<C>00.PAT` / `<C>01.PAT` are v4 (2000 poses, same 92-byte part record); `han2tool patrt`: reader + writer reproduce all 64 shipped
GOF2 .PAT byte-identical (together with the 208 RBO PAT blocks). Both costume variants cover every pose the .DT2 uses (AMIY: 1163/1163).
Sprites: `<C>00.CHP` is a bare BMP Cutter3 bank (same CG loader). GOF2 .DT2 has no pattern-name area, so patterns are unnamed.
Saving: a GOF2 character is saved as .DT2 (pattern area); edited parts/sprites are written next to it as `<stem>NN.PAT` / `<stem>NN.CHP`
(first version of each file kept as .bak).
Parts-bug found by the GOF2 run: a part record with an out-of-range texture id (DATE/TOKUGAWA _CHR_SELECT) has no model part and is
now kept verbatim by the PAT writer.

## 6. Sections 4 and 5 (RBO) and unread flag bits: closed

`docs/formats/ida/sections45_ex.md`: no code in rbo.exe or rbo_ex1/2/3.exe reads pattern-area sections 4 and 5 (flow-sensitive scan over
2989/3212/3271/3284 functions, with sections 0-3, 6, 7 found in exactly their known consumers as control). Data: sections 4/5 hold one record per
sousai / tobi box slot (frame +0xD0 x 28 == section 4 size and +0xE0 x 20 == section 5 size in 346/346 files), so the frame +0xD0/+0xE0 fields are
box counts, not indices (this supersedes rbo_scripts_pat.md 1.2). Record layouts (data-named, 4 fields unused with proof):
`docs/formats/ida/rbo_sections45_types.h`. The writer keeps both sections verbatim and warns when clash/projectile boxes are added or removed.
Attack record flag bits: the Ex executables read two bits rbo.exe never does: `RboAtFlags76 0x80 SELF_ONLY_TARGET` (Hit_TeamTargetTest returns
attacker == victim) and `RboAtFlags80 0x2000 UNEVADABLE` (Hit_RollEvade returns 0 before the roll). Nine bits stay read by no executable (their
only possible reader is FOB condition-script bytecode that receives the AT pointer; not checked).

## 7. Round-trip verification (`tools/han2/run_roundtrip.sh`, `han2tool`)

Zero skips. Every byte round-trip section reports `skipped 0`; the counter exists only so the report can state it and no code path increments it.
Each section visits EVERY entry of EVERY archive given (12 RBO PACs: DATA01 DATA02 Update01 Ex1Disc Ex2Disc Ex3Disc BG01 BG02 BGM SE ETC CG; 6 GOF2
`data0x.dat`; 4 GOF1 `gof_0x.p`), decides what the entry really is **from its bytes** (never from the extension alone), and either checks it or counts
it as **n/a** (it belongs to another section). `n/a` is listed in the section line by extension and real type. The script fails on any `fail`, any
non-zero `skipped`, or a non-zero exit code. Games are read in place (nothing is copied).

Classification (`Classify`, han2tool.cpp), by bytes: `HAN2RBO ` magic = HAN2RBO container; PAT magic 0x01234567 + version 3/4 (2 with the GOF1 size test) =
bare PAT; `IsImg` = IMG; anything else = opaque. Owners: HAN2RBO -> container, model, pat, cg, animtest; bare PAT -> pat; IMG -> img (also `.CG` entries of
ETC.PAC, which are IMG files); opaque -> opaque. An entry whose extension names a structured format (`.DAT .DT2 .PAT .IMG`) but whose bytes are not it is a
FAIL, unless it is in the known-opaque table (one file, below).

Totals of the last run (RBO + GOF2 + GOF1 shipped files, 7,971 non-container entries counted as n/a in the container section):

| Section | What is proven byte-exact | pass | fail | skipped | n/a (owner) |
|---|---|---|---|---|---|
| `count` / `pacrt` | entry tables, every PAC rebuilt from its entries (18 archives) | 18 / 18 | 0 | 0 | - |
| `roundtrip` (container) | HAN2RBO parse -> serialize | 414 | 0 | 0 | 7,971 (not HAN2RBO) |
| `modelrt` | load into the Hantei-chan model -> save | 414 | 0 | 0 | 7,971 |
| `patrt` | PAT block -> Parts -> PAT block, and the file with the rebuilt block put back | 478 (7 .DAT + 199 .DT2 empty sections included) | 0 | 0 | 7,907 |
| `imgrt` | IMG parse -> serialize, all four pixel formats | 928 (924 format 2, 4 format 3) | 0 | 0 | 7,457 |
| `cgrt` | bank load is a no-op; every image re-imported; bank byte-identical | 414 files (334 empty CG areas included), 5,494 images | 0 | 0 | 7,971 |
| `opaquert` | raw passthrough of entries with no reader | 6,979 | 0 | 0 | 1,406 |
| `gof1rt` | GOF1 archive rebuilt from plain entries + 51 character `.DAT` | 55 | 0 | 0 | 442 (opaque, proven by the archive rebuild) |

`animtest` is a BEHAVIOUR check, not a byte round trip: the live stepper (`han2_anim.cpp`) must reproduce `SimulateFlow`'s tick count. Last run:
5,335 patterns agree, 0 differ. 10,884 patterns are not comparable because their flow does not end by itself: 3,003 reach an ani flag that branches on game
state (hit, input, ground; `Actor_AdvanceByAniFlag`), 6,998 are unconditional loops (idle/walk cycles), 808 hit the 100,000-tick cap, 75 run off the end.
Those are not skips of a byte check; there is nothing to compare because no finite reference exists.

### The files that used to be skipped

| File | Old section | What it is | Fix |
|---|---|---|---|
| DATA02::ORC_FIRE, DATA02::PRO_C_OBJ, Ex1Disc::FIREWALL, GESUI_OBJ, KAIZOKU_OBJ, Ex2Disc::PYRAMID_FIRE, Ex3Disc::AMATSU (.DAT) | patrt "empty PAT section" | valid HAN2RBO objects with a zero-length PAT area (they borrow another file's parts) | defined round trip: `PatSectionToParts` gives an empty, unloaded model; `BuildPatSection` emits exactly the original (empty) block; the whole file with that block re-serializes identically. Now `pass`, noted as "empty PAT section". The 199 `.DT2` files (pattern-only) follow the same rule |
| DATA01::DUSTNESS.DAT | container (`not HAN2RBO`), silently ignored by patrt/modelrt | stale misspelt duplicate of DUSTINESS.DAT; 7.998 bits/byte entropy over the whole file, no magic, size 760,162; the game only names DUSTINESS | no reader exists; raw passthrough in `opaquert` (PAC reader bytes == archive file bytes), in the known-opaque table with this reason |
| CG / IMG "unsupported type / empty" images | cgrt | 53 type-3 images (one colour + 8-bit alpha plane) had no importer; 19 slots have no record | type 3 import added (`CG::replace_image_rgba`: index = alpha); absent slots have nothing to import and their bytes are covered by the whole-bank comparison |

Bugs this strictness exposed (all fixed, all with a test above):

* **IMG format 3 / version 8** (GOF2 `data05`: ACED_11, ACOP_11, PRE_BG00, PRE_BG13) were reported `FAIL not an IMG` by imgrt when run on that archive, and the
  suite only ever ran imgrt on CG.PAC (88 of 928 files). Header (GOF2.exe `lib::CCGImageRead::ReadImgStream_fmt0to3` 0x4024A0, `GetPixelRGBA` 0x402580):
  `u32 0 | u32 version 6..8 | u32 format | u32 w | u32 h | pixels`, format 0 = ARGB1555 (2 B), 1 = ARGB4444 (2 B), 2 = R,G,B,A (4 B), 3 = R,G,B (3 B). `ImgFile`
  keeps the file's own pixels (`native`) for formats other than 2 and writes them back verbatim (0/1 are lossy to RGBA).
* **CG bank load rewrote the palette** (binary alpha, entry 0 zeroed) inside the bank itself, so any save that synced the bank (`SyncPartsToContainer`) changed
  bytes from 0x15 on (the first diff) of every bank that has a non-canonical palette (every RBO character bank sampled). The normalised palette now lives in a copy (`CG::m_basePalette`); the bank stays as shipped.
* **CG re-import of unchanged pixels re-ordered the palette of type 2/4 images.** `replace_image_rgba` now leaves the stored bytes alone when the incoming
  pixels equal what the bank already renders (`force=true` always re-encodes; cgrt uses it on a scratch bank to prove the encoder).
* **GOF1 archive index**: the 56-byte name slot keeps uninitialised bytes after the NUL in the shipped `.p` files; a rebuilt archive differed until
  `gof1::Entry::rawName` kept them (same as `pac::Entry::rawName`).
* The old suite also never covered ETC/BG/BGM/SE PACs, `.DT2`/bare-PAT in patrt, or GOF1; all are in now.

Known-opaque table (han2tool.cpp `kKnownOpaque`): `DATA01.PAC::DUSTNESS.DAT` only.
