# Hantei-chan: UNDER NIGHT IN-BIRTH II and MELTY BLOOD: TYPE LUMINA

Branch `feat/uni` (worktree `wt/uni`, based on `update/ex-mbac` c015c09 with `feat/issues` merged in).
Issues: #14 UNI2/MBTL support, #46 txt projects, #68 TL boxes, #71 saved-edit corruption,
#74 attack flag meaning, #76 PUPS. Game-data layout: `docs/UNI2_MBTL_DATA.md`.

Every rule below comes from the games' own code, from IDA:

- UNI2: `uni2.exe.i64` in the UNI2 install folder
- MBTL: `MBTL.exe.i64` in the MBTL install folder
- MBAA (read only, baseline for comparison): session `212fe94c`

The functions named here were renamed and commented in both IDBs, and the IDBs were saved. Annotated
decompilations of every function cited (tags shown as strings) are in `docs/uni_re/uni2/` and
`docs/uni_re/mbtl/`, named by address.

## 1. Result

| check | before | after |
|---|---|---|
| UNI2 + MBTL HA6: load → save byte-identical | 0 / 262 (all rewritten; chr016 boxes moved) | **262 / 262** |
| same, encoding forgotten (`--fresh`), field compare | 261 / 262 | 261 / 262 (chr016: see §4.4) |
| UNI2 + MBTL `.pat` byte-identical | 0 / 135 (and cut-outs saved with tags the games don't read) | **135 / 135** (`--fresh`: 135 / 135 field-identical) |
| MBAACC `.pat` byte-identical | 0 / 19 | **19 / 19** |
| `.pal` byte-identical (192 UNI2/MBTL with 130 palettes, 80 MBAACC) | no writer | **272 / 272** |
| #71: edit one pattern in a `.txt` stack and save; which patterns change? | UNI2 chr000: **269** patterns | **only the edited one** (5 projects incl. an MBTL variant) |
| MBAACC HA6 single round trip (field compare) | 274 rc0 + 31 rc5 | **305 rc0** |
| Stacked save == save of the target file alone | MBAACC 117, UNIB 16 | + UNI2 29, MBTL 60 (all ok) |
| MBAC `.DAT` (ha4tool roundtrip) | 50 / 50 | 50 / 50 |

The full suite is `tools/uni/uni_regress.sh`. Its output is in §6.

Commits on `feat/uni`:

| commit | change | issues |
|---|---|---|
| 986ab7a | UNI2/MBTL HA6 loaded and saved byte for byte, following the games' readers | #71, #14, chr016 |
| 3000e28 | Palette file chosen by the pattern's PUPS; AFGX layers drawn in the games' order and buckets | #76, #68 |
| 469136c | `.txt` projects resolved as the games resolve them; variant projects save to their own file | #46 |
| 653f6a5 | `.pat` round trip byte for byte, using the cut-out tags the games read; lossless `.pal` | #14 |
| b8f06c5 | Field labels switch by game (attack flag 4), game-format override, UNI/MBTL fields in the UI | #74, #14 |
| 8330411 | MBAACC `.pat`: no empty VEST block on save | – |
| 9b29e06 | Merge of `feat/issues` (second time); the compare overlay uses AFPL buckets, per-layer AFRT and PUPS | – |
| e37e335 | `tools/uni/uni_regress.sh` | – |

The first `feat/issues` merge (8d90da1, 85d903f and later) happened before 986ab7a. 469136c corrects 8d90da1's
save-target rule for MBTL variant projects (§4, #46).

## 2. How the games load a character (RE)

### 2.1 Project `.txt`: `Han6_ParseProjectTxt`
UNI2 0x4C3BA0 / MBTL 0x4A5A50.

- The project name comes from `[System] ProjectName`, or from `[Project] File` (effect.txt uses the latter).
- `[DataFile]` holds `FileNum` N plus `File%0*d`. The width is 2, or 3 when N ≥ 100. If `FileNum` is missing,
  a single `File=` entry is used.
- `[BmpcutFile]` and `[PAniFile]` each hold `FileNum` plus `File%02d`.

### 2.2 Load order and merging
`Han6_LoadProjectDataFiles_Reverse` (UNI2 0x4C3950 / MBTL 0x4A9BA0) loads File(N-1) first and File00 last.
UNI2 resolves a `../` prefix against the txt folder. `Han6_LoadPattern_PSTR` (0x4C45E0 / 0x4A64A0) then decides
what each pattern does:

- **Slot already holds a pattern with frames and PFLG bit 0 clear:** the new pattern is parsed in mode 250
  and discarded. The file loaded first, which is the highest-indexed one, wins.
- **Pattern with no frames (a name-only stub, as in BaseData):** discarded.
- **Slot holds a PFLG&1 template (BaseData's ★ patterns):** `Han6_MergeTemplatePattern_PFLG`
  (0x4C4B40 / 0x4A6A00) keeps the template's frames (timing, AS, AT, EF, IF). From the character's pattern it
  copies, for each frame (up to the smaller frame count), each layer's usePat, sprite and AFOF (5 layers in UNI2,
  3 in MBTL), and the hurt boxes. It also copies the attack boxes on frames where the template has an AT block.
  The template's PFLG then becomes the character's, so File00 (_temp) can no longer merge into it.

So for a `_temp, chrNNN, [chrNNN_N], ../BaseData` project, a pattern comes from the variant file, else chrNNN,
else _temp. BaseData templates only provide the timing of guard and hit patterns.

### 2.3 HA6 tags
All tables follow the loaders, with offsets into the game's structures. UNI2 and MBTL are identical except that
MBTL has 3 AFGX layers. The MBTL frame record is also smaller (0xC8 bytes vs 0x128), so its field offsets differ.

**Pattern level** (`Han6_LoadPattern_PSTR`)

| tag | payload | what the game does |
|---|---|---|
| PTT2 | u32 len + len bytes | name buffer. MBTL/UNI2 buffers keep stale bytes after the NUL, and some MBAACC names are 32 bytes with no NUL |
| PTIT | 32 bytes | fixed-length name |
| PTCN | u32 len + bytes | code name (the name the move scripts use) |
| PUPS | 1 dword → byte +44 | **palette file** used while this pattern is drawn (§2.5) |
| PFLG | 1 dword → +36 | bit 0 = template (§2.2). Only set in BaseData (48 UNI2 and 39 MBTL patterns) |
| PSTS, PLVL | 1 dword | read and **ignored** by UNI2/MBTL |
| PDS2 | 32 bytes | frames, box pool size, EF, IF, AT, 0, AS pool size, frames. All shipped files are consistent, and word 5 is always 0 |
| PDST | 1 dword | legacy frame count |

**Frame level** (`Han6_LoadPatternFrames` 0x4C4E70 / 0x4A6CC0)

| tag | payload | meaning |
|---|---|---|
| FSNH / FSNA / FSNE / FSNI | 1 | slot counts: hurt boxes, attack boxes, effects, conditions. Arrays are sized from these |
| HRNM / HRAT | idx + 4 | new hurt (0..FSNH-1) or attack (0..FSNA-1) box, 4 × int16, added to the pattern's box pool |
| HRNS / HRAS | idx + pool | reference to box pool entry n. The pool may be referenced forwards |
| HRFF | box, byte | box flag byte (+8). Negative box = attack. Not used in shipped files |
| ASST / ASSM n | – / 1 | new AS block / reference to AS pool entry n (backwards only) |
| ATST | – | AT block. Present exactly when the frame has attack boxes (12,143 of both in the corpus) |
| EFST n / IFST n | 1 | effect / condition in **slot n**. Slots can be sparse: 67 UNI2 frames have `EFST 0, 2, 3` with FSNE 4 |

**AF** (`Han6_LoadFrameAF` 0x4C5E40 / 0x4A7C90). A layer is 48 bytes; UNI2 has 5 layers and MBTL 3. The loader
ignores `AFGX` with id ≥ 5 (UNI2) or ≥ 3 (MBTL). Every AFGX frame in the corpus lists all of its game's layers,
with sprite -1 for unused ones.

| tag | where | note |
|---|---|---|
| AFGX id pat sprite / AFGP pat sprite | layer | AFGP = layer 0 |
| AFOF x y, AFY1/2/3/7/8/9/X | layer +4/+8 | AFY* sets x=0, y=11/12/13/7/8/9/10. UNI2/MBTL never use AFY |
| AFRG r g b | layer +12..14 | |
| AFAL mode alpha | layer +15, +16 | mode 0 → alpha forced to 255; 1 normal; 2 additive; 3 subtractive (§2.5) |
| AFAX / AFAY / AFAZ, AFAN | layer +20/+24/+28 | AFAN writes the Z slot as an int (unused) |
| AFTN a b | layer | sets Y rotation (from a) and X rotation (from b) to 0.5 |
| **AFRT** | layer +32 | **per layer** (MBAACC: per frame) |
| AFZM x y | layer +36/+40 | |
| **AFPL** | layer +44 | **draw bucket** (§2.5), not a sort key inside the frame |
| AFD1..9 / AFDL | frame | duration. Duration 0 is written as no tag at all |
| AFF1 / AFF2 / AFFL, AFFE | frame | animation type (AFFL only holds 3) / flags |
| AFPR, AFHK, AFJP, AFJC, AFCT, AFLP | frame | as in MBAACC |
| AFID | frame, int16 | frame id for the Squirrel scripts |
| **AFPA** | frame | **two int16**, not four bytes |
| AFJH | frame, byte | flag, set on the first frame of most patterns |

**AS** (`Han6_LoadFrameAS` 0x4C65E0 / 0x4A8430): ASV0, ASVX (flags 0x11), AST0, ASS1/2, ASMV, ASAA, ASCN, ASCS,
ASCT, ASYS, ASF0/1, ASMX and ASCF (+28), as in MBAACC. ASV1, ASVA, ASVC, ASAT, ASKV, ASSS, ASDF, ASCL, ASSE,
ASDE, ASF2 and ASF3 are also read, but no shipped file uses them.

**AT** (`Han6_LoadFrameAT` 0x4C6CB0 / 0x4A8B00). The AT record is 76 bytes.

| tag | field | note |
|---|---|---|
| ATV2 3 2 [flags,vec]×2×3 | +4.. | hit and guard vectors (stand, air, crouch). Used instead of ATHV/ATGV |
| ATAT | +60 | damage. ATVV also works: only its high words (damage, meter) are read |
| ATCA | +64 | meter gain |
| ATAM | +62 | minimum damage |
| ATHH | +48 | proration |
| ATHS | +44 | correction |
| ATHT | +45 | correction type |
| ATSH | +46 | starter correction |
| ATSA | +43 | attacker's own hitstop preset, used when ATF1 bit 25 is set (`Hit_ResolveHitstop` 0x50D740) |
| ATRF | +47 | byte |
| ATBC | +52 | word |
| ATVD | +68 | attack power %: when > 0 it replaces the attacker's damage rate (`Hit_ComputeDamage_ATVDRateOverride` 0x50CCD0) |
| ATC0 a b c | +71, **+73, +72** | hitstun decay. Defaults: 100 for the first value, 50 for the third |
| ATSP / **ATS1..ATS6** | +42 | hitstop preset. **ATSn is the compact form of the same field.** Every file that has ATS3/5/6 (UNI2 only) also has a later ATSP, which wins |
| ATSN, ATSU, ATGN, ATKK, ATHE, ATGD, ATF1 | | as in MBAACC. Counter-hit untech is ×1.8 (`Hit_ResolveUntech` 0x50D9F0) |
| ATNG | +41 | **a byte**, with values up to 65 in UNI2/MBTL. Bit 0 is also tested as "can't KO" |
| ATAB (+66), ATBG (+70), ATGE (+32/+34) | | read. No shipped file uses them |
| ATKZ, ATGS, ATUH, ATF2 | | read and ignored |

**EF / IF**: EFTP, EFNO, and EFPR (up to 12 params). IFTP and IFPR (the game reads up to 10 params; shipped files
use at most 8).

### 2.4 Attack flag 4 (#74)
- **UNI2** `Hit_ResolveVector_CounterHitSwap` 0x537511 and **MBTL** 0x50C9C3:
  `if (counterHit && (ATF1 & 0x10)) { v = VectorTable[vec].counterHitVector (+48); if (v >= 0) vec = v; }`
- **MBAA** `Victim_ApplyPendingHits` 0x47238E: `if (ATF1 & 0x10) attacker+365 = 1`. This is the flag Hantei-chan
  has always labelled "auto super jump cancel".

The same function confirms bits 1 ("can't KO") and 18 ("can't counter hit"). Bits 10, 16 and 25
(`Hit_ResolveHitstop`) and bits 7, 8, 17 and 23 (`Hit_ResolveUntech`) behave as their MBAACC labels say. Bit 25
is new: the attacker's hitstop comes from ATSA.

### 2.5 Drawing (MBTL `Han6Object_Draw` 0x596FA0 → `Han6Draw_DrawFrameLayers_2to0` 0x4A0580 → `Han6Draw_DrawLayer` 0x4A0070; UNI2 0x4BDA20 / 0x4BD510)
- **Layer order.** Layers are submitted from N-1 down to 0, and a layer with sprite < 0 is skipped.
  `RenderList_InsertIntoBucket_LIFO` (0x49EEF0) inserts each command after its bucket's sentinel, and
  `RenderList_Execute` walks the chain, so within a bucket **layer 0 is drawn first (bottom)**.
- **AFPL.** It picks the bucket: drawctx[1 + AFPL] = {object+256, 403, 338, object+258, object+254}. The same
  constants are used in UNI2 (0x5CAF19). Buckets are drawn in ascending order.
- **Blend** (AFAL mode → `D3D_SetBlendModeFromTable` 0x48CBF0, table 0xA1BB80):
  - 1: ADD, SRCALPHA, INVSRCALPHA (normal)
  - 2: ADD, SRCALPHA, ONE (additive)
  - 3: REVSUBTRACT, SRCALPHA, ONE (subtractive)
  - 0 (no AFAL): normal, with the alpha forced to 255
  - The alpha is multiplied by the object's alpha. AFHK interpolates RGB and alpha toward the next frame's layer
    when the blend mode is non-zero.
- **PUPS.** `CharaPalette_LoadPalAndPupsVariants` (UNI2 0x5C6F10 / MBTL 0x5934B0) loads `<cg>.pal` into palette
  file 0 and `<cg>_p1.pal` .. `_p7.pal` into files 1..7. For CG layers, `Han6Draw_DrawLayer` takes the palette
  file from `drawctx+128` if that is ≥ 0, otherwise from the drawn pattern's PUPS byte. The file is passed to the
  shader as `fPalPlusV = file/32` (`Han6Draw_SubmitCgSprite` 0x4A1440).
- **`.pal` layout** (`CharaPalette_ParsePalToSlot` 0x5928A0):
  - The header is `FFFF, split, 0, count`.
  - With split = 1, 130 palettes = 65 colours × 2 sets: palette c goes to row PUPS and palette c+65 to row PUPS+8
    (the colour's alternate set).
  - In 80 of 86 UNI2 files and all 106 MBTL files the second set is identical to the first. It differs in UNI2
    chr008 and chr010, among others.
  - The older layout is `count` palettes, with the 65th used as an alpha mask.

### 2.6 PAniDataFile (`PatFile_Load` UNI2 0x4A6350 / MBTL 0x4878B0)
- The parsers are `PatFile_ParsePartSet_PST`, `PatFile_ParseCutouts_PPST`, `PatFile_ParseBlock_PGST` and
  `PatFile_ParseShapeDefs_VEST`.
- **Cut-outs: both games read only PPCC, PPSS, PPTP, PPPA, PPPP, PPUV, PPTE, PPTX, PPJP, PPNM and PPNA.**
  Hantei-chan used to save non-MBAACC files with PPXY, PPWH, PPGR, PPCL and PPVT, which neither game reads.
- Parts: PRPA (part +16) exists. PRAL (+53) and PRFL (+54) are bytes; PRAL values 2 and 3 are separate modes.
  PRAS is read by UNI2 but not by MBTL.
- There is **no version field**. The "PAniDataFile v1 / v3" in `UNI2_MBTL_DATA.md` is the id of the first
  `P_ST` record (1 in UNI2 chr000, 3 in MBTL chr000; 0 in MBAACC arc.pat).

## 3. Fixes

### 3.1 HA6 loader and writer (986ab7a)
- **Semantics.**
  - AFRT is stored per layer.
  - ATS1..6 are loaded as the hitstop form (`Frame_AT::hitStopLegacy`), replacing the unknown `ats3/5/6` flags.
  - ATNG is an int. Before, every save turned 65 into 1.
  - Rare tags are parsed and kept verbatim (`Ha6FrameEnc::extra`).
- **Encoding** (`src/ha6_enc.h`: `Ha6FrameEnc` per frame, `Ha6SeqEnc` per pattern, both POD):
  - The ASSM / HRNS / HRAS reference structure is kept. The FB tool shares blocks by pointer, and the shipped
    files show that the structure can't be derived from content: a frame whose AS equals block 1 is written out
    in full while the next frame references block 1.
  - A reference is written only while its target is unchanged; otherwise the frame is written in full.
  - Tags present with default values, EF/IF slots with FSNE/FSNI, and raw PTT2/PTCN buffers are kept.
  - Tags are written in the canonical order measured on the whole corpus (`tools/uni/ha6_order.py`: no pair of
    tags ever appears in both orders).
- **Writer.** `WriteSequenceUni` (framedata_save.cpp) handles patterns with AFGX or ATV2, and new patterns in a
  UNI/MBTL file. New AFGX frames are padded to the game's layer count. The MBAACC writer is unchanged, except
  that it now also reuses raw names, which fixes 6 MBAACC names that were cut to 31 bytes.
- **Boxes.** Boxes exactly as loaded are written as they are. Only boxes created or edited in the session get
  the degenerate/inverted cleanup (§4.4).
- **Game detection.** `FrameData::detectedGame()` uses the AFGX layer count (5 = UNI, 3 = MBTL; no AFGX/ATV2 =
  MBAACC), and `g_ha6GameOverride` overrides it (View > Game format, or `--game`).

### 3.2 Rendering (3000e28)
- **PUPS.** `CG::loadPupsPalettes` loads the `_pN.pal` banks, and each `RenderLayer` carries its pattern's PUPS.
  `DrawCgLayerItem` selects that bank, keeping the palette number, and the base bank is restored after the layers
  are drawn. View > PUPS palette files turns this off (`--no-pups`). A missing bank falls back to the base palette;
  the game would show its default grey ramp instead.
- **Layer order.** `LayerDrawBucket(AFPL, priority)` gives each layer its bucket. Layers keep the 0-first order
  inside a bucket, and per-layer AFRT is applied.

### 3.3 Projects (469136c)
- The file list is read as the games read it (`File%03d` from 100 files, or a single `File=`).
- **Save target:** `FrameData::StackSaveTarget`, the highest-indexed file that is not shared data (`../` or
  BaseData). Files after it only fill empty slots, and the others overlay in index order. This gives the games'
  "highest index wins" for every pattern that has frames.
- "Load chr HA6 only" loads the same target.

### 3.4 PAT / PAL (653f6a5, 8330411)
- Cut-outs keep the tag family they were loaded with; new ones use the games' tags.
- PRAL, PRFL and PRPA values are kept.
- `Parts` keeps each loaded record's bytes together with a model fingerprint, and writes the original bytes while
  the fingerprint is unchanged. This preserves the texture compression too.
- MBAACC files get no empty VEST block.
- `PalFile` (`src/pal_file.h`) loads and saves `.pal` files losslessly; `pal_roundtrip.exe` tests it.

### 3.5 UI (b8f06c5)
- **AT panel:**
  - UNI/MBTL fields: ATHH, ATAM, ATSH, ATSA, ATC0, ATRF, ATBC, ATVD, and the ATSn hitstop form.
  - ATNG as a number.
  - Flag 4 and flag 25 labels per game.
- **AF panel:** AFPL as a draw-bucket combo, AFRT per layer, AFID, AFJH, AFPA as two int16, and blend-mode
  tooltips.
- **Pattern panel:** PUPS (0..7), code name, and PFLG as the template flag.
- **AS panel:** ASCF.
- Rare tags are listed read-only under "Other tags".
- The View menu gains Game format and PUPS palette files. Palette number explains the 65 × 2 layout and lists the
  loaded `_pN` files.

## 4. Per-issue closure notes

### #14: UNI2/MBTL support
- **Root cause:** Hantei-chan treated UNI2/MBTL files as MBAACC with extra tags. Semantics were wrong in several
  places:
  - AFRT per frame
  - ATS3/5/6 as "unknown flags"
  - ATNG as a bool
  - AFPA as bytes
  - inverted boxes rewritten
  - PAT cut-out tags that the games don't read
  - no PUPS
  - the wrong file-stack rules

  Every save also rewrote the whole file in a different encoding.
- **Fix:** 986ab7a, 3000e28, 469136c, 653f6a5 and b8f06c5. The game is detected automatically (with a manual
  override), and every tag and field follows `uni2.exe`/`MBTL.exe` (§2).
- **Verify:** `./build.sh && tools/uni/uni_regress.sh` gives `UNI_REGRESS_PASS`:
  - 262/262 HA6 files byte-identical
  - 135/135 PAT files byte-identical
  - 272/272 PAL files byte-identical
  - MBAACC, UNIB and MBAC suites green

### #46: txt project loading and saving
- **Root cause.** The save target was chosen as "File01 if File00 starts with _temp". For MBTL's 4-file variant
  projects (`_temp, chrNNN, chrNNN_N, ../BaseData`), saving from `chrNNN_7.txt` wrote into `chrNNN.ha6`, which
  changes the base character. The earlier fix (8d90da1) also loaded `chrNNN_N.ha6` as a fill-only fallback, so its
  patterns lost to `chrNNN.ha6`'s in the editor, which is the opposite of the game.

  The games load File(N-1) first and keep the first pattern loaded. The highest-indexed file therefore wins, and
  BaseData's PFLG patterns are templates (§2.2).
- **Fix:** 469136c.
  - The target is the highest-indexed non-shared file: chrNNN.ha6 for base projects, chrNNN_N.ha6 for variants.
  - Files after it only fill empty slots.
  - `File%03d` and `[DataFile] File=` are supported.
- **Verify:**
  - `tools/ha6_regress.sh` reports "MBTL stacked save == own-file save: ok=60 bad=0" and "UNI2 … ok=29".
  - In `uni_regress.sh` §4, `chr001_7.txt` edits pattern 17 and saves into `chr001_7.ha6`; only pattern 17
    changes.
  - Opening `mbtl/data/chr001/chr001_7.txt` shows "Saves to: chr001_7.ha6".

### #68: TL characters missing boxes
- **Root cause.** In MBTL, most characters' neutral frames have **layer 0 empty** (sprite -1) and the body on
  layer 1. For example, chr000 p0 is `AFGX[0,0,-1] AFGX[1,0,0] AFGX[2,0,-1]`. Saber (chr012) is the exception,
  with layer 0 = sprite 0.

  Hantei-chan attaches the frame's boxes to the layer-0 render entry. The renderer skipped layers with sprite < 0
  (and, later, PAT layers) including their boxes. Boxes therefore vanished, or appeared "late" once layer 0 got a
  sprite. The header is identical to MBAACC's (all zeros).
- **Fix.** The renderer fixes are already in the base: fff53fd stopped skipping CG layers with sprite -1, and
  b9cb3eb stopped the box pass from skipping PAT layers. This branch checked the result on every rendered TL
  frame below, and the renderer still keeps the boxes independent of the layer sprites. It also changes the layer
  handling to follow the games (3000e28: per-layer AFRT, the AFPL draw bucket, and layer 0 drawn at the bottom of
  its bucket). The box data itself no longer changes on save, where inverted boxes used to be "fixed" (986ab7a).
- **Verify:**
  - `gonptechan --open mbtl\data\chr000\chr000_0.txt --pattern 0 --frame 0 --zoom 1 --capture x.png` →
    `docs/uni_captures/mbtl_chr000_p0_f0.png` (boxes on the empty-layer-0 frame).
  - `mbtl_chr011_ciel_p0_f0.png` and `mbtl_chr012_saber_p0_f0.png` for comparison.
  - `mbtl_chr016_p6_f6_inverted_atk.png` shows attack boxes, including the inverted one.

### #71: UNI+TL saved edits corrupt data
- **Reproduced.** `tests/uni_edit_test.cpp` is built against the old code (c015c09, `-DOLD_API`) and the new. It
  loads `uni2/data/chr000/chr000_0.txt`, adds 1 to the duration of pattern 100's first frame, and saves:
  - old: **269 patterns differ** from the original (3,195 tag-level diff blocks), including patterns 0..25
  - new: **1 pattern differs (100)**
- **Root causes.** Everything in the old save differed from the original:
  - (a) the other stack files' patterns were baked in
  - (b) frameless name-only patterns were dropped (8d90da1 and 85d903f fixed these two)
  - (c) the AS/box reference structure was rewritten (ASST ↔ ASSM, HRNM ↔ HRNS) and PDS2 counts changed
  - (d) names lost their stale tail bytes
  - (e) ATNG values were clamped to 1
  - (f) inverted boxes were rewritten
  - (g) AFRT moved between layers
  - (h) duration-0 frames gained AFDL 0

  The report's screenshot shows (c): PDS2 box count 2 → 0x3A, AS count 1 → 0, ASSM → ASST, HRNS → HRNM. It also
  shows AFJH missing and the old UTF-8 header flag (0xFF at byte 31).
- **Fix:** 986ab7a on top of 85d903f and 8d90da1.
- **Verify.** `tools/uni/uni_regress.sh` §1 and §4. To check by hand:
  `build/uni_edit_test.exe 1 100 out.ha6 <_temp.ha6> <chr000.ha6> <../BaseData.HA6>` followed by
  `python3 tools/uni/ha6patdiff.py chr000.ha6 out.ha6`, which prints "1 pattern(s) differ: 100".

### #74: attack flag has a different function in UNI/TL
- **Root cause.** ATF1 bit 4 is the counter-hit vector swap in UNI2 and MBTL, but the attacker super-jump-cancel
  flag in MBAA (§2.4, with addresses).
- **Fix:** b8f06c5. Labels follow the detected or overridden game. Bit 25 (the attacker's hitstop from ATSA) is
  added for UNI/MBTL.
- **Verify:** open a UNI2 or MBTL character, go to Attack data > Hit Flags, and hover bit 4. View > Game format >
  MBAACC switches the label back.

### #76: PUPS / UNI HA6 corruption
- **Root causes.**
  - Builds from the time of the report did not load or save PUPS, so every save dropped it. This was fixed in
    05f0280, before this branch.
  - The rest of the "corruption" is the save issues of #71.
  - PUPS was still never used for drawing: `ini.cpp` loaded only `<cg>.pal`.
- **Fix:** 3000e28 selects the palette file per pattern (§2.5), and 986ab7a covers the corruption. PUPS is
  editable in Pattern data, with a tooltip.
- **Verify.** `docs/uni_captures/uni2_chr027_p128_pups1.png` (Zohar p128, PUPS 1: `chr027_p1.pal`) vs
  `uni2_chr027_p128_nopups.png` (`--no-pups`). With the base palette, the doppel parts show the base file's green
  placeholder colours.

### 4.4 chr016 (Powered Ciel) "box position changes after save/reload"
- **Cause.** MBTL chr016 p6 f6 contains `HRAT[0,645,-26,600,17]` (x1 > x2), and p25 f2-3 have boxes of the same
  kind. `FixBoxesForSave` swapped inverted corners on every save. The same fix accounted for the 31 MBAACC files
  that used to report differences.
- **Fix.** Boxes exactly as loaded (`Ha6FrameEnc::boxXY`) are written unchanged. Boxes drawn backwards in the
  editor are still normalised.
- **Verify.** `roundtrip.exe --bytes mbtl\data\chr016\chr016.ha6 out.ha6` reports "Bytes identical". With
  `--fresh` (all boxes treated as edited) it still reports the 3 normalised boxes, as expected.

## 5. Corrections to `docs/UNI2_MBTL_DATA.md`
- **ATS3/ATS5/ATS6** are not unknown flags. They are the hitstop preset (same field as ATSP, which follows them).
- **ATRF/ATBC/ATVD** are real AT fields (+47/+52/+68). ATVD is the attack-power override.
- **AFPL** is the layer's draw bucket, not an in-frame priority.
- **PSTS/PLVL** are ignored by UNI2/MBTL.
- **`.pat` "P_ST version 1/3"** is the first part-set id; there is no version field.
- **`.pal`**: 130 palettes = 65 colours × 2 sets (the second set is the alternate).
- **The #46 suggestion "last non-BaseData file"** is what the games do.
- **BaseData** is not a plain overlay: its non-stub patterns are PFLG templates (§2.2).

## 6. Test runs (`tools/uni/uni_regress.sh`, full log in `build/uni_regress.log`)

```
== 1. UNI2/MBTL HA6
HA6 round trip (): rc0=262
HA6 round trip (--fresh): rc7=261 rc5=1          (rc7 = fields equal, bytes differ; rc5 = chr016, §4.4)
== 2. PAT
PAT round trip (--bytes): rc0=135
PAT round trip (--fresh): rc0=135
MBAACC .pat byte round trip: 19 files, 0 differ
== 3. PAL
PAL round trip: 272 files, 0 differ   (UNI/MBTL palettes=130: 192, MBAACC palettes=64: 80)
== 4. #71 saved edits (only the edited pattern changes)
  chr000_0.txt -> chr000.ha6, edit p100: 1 pattern(s) differ: 100      (UNI2)
  chr027_0.txt -> chr027.ha6, edit p128: 1 pattern(s) differ: 128      (UNI2, PUPS pattern)
  chr000_0.txt -> chr000.ha6, edit p0: 1 pattern(s) differ: 0          (MBTL)
  chr001_7.txt -> chr001_7.ha6, edit p17: 1 pattern(s) differ: 17      (MBTL variant, #46)
  chr016_0.txt -> chr016.ha6, edit p6: 1 pattern(s) differ: 6          (MBTL, inverted boxes)
== 5. HA6 stacks and MBAACC/UNIB round trips
MBAACC single round trip: rc0=305
MBAACC stacked save == own-file save: ok=117 bad=0
UNI single round trip: rc0=61
UNI stacked save == own-file save: ok=16 bad=0
UNI2 stacked save == own-file save: ok=29 bad=0
MBTL stacked save == own-file save: ok=60 bad=0
HA6_REGRESS_PASS
== 6. MBAC, stages, command files, unit tests
roundtrip: 50/50 byte-identical                                        (ha4tool, MBAC .DAT)
roundtrip: 124 ok, 0 failed | sim(600 ticks): 124 ok, 0 failed | determinism/rng failures: 0 | edit failures: 0
cmdfile_test: 272 checks, 0 failed; undo_manager, shortcut_router, refs, pattern_search, shared_clipboard: PASS
UNI_REGRESS_PASS
```
Log from the final merged tree (9b29e06 + e37e335): `docs/uni_re/uni_regress.log`.

The old-code comparison for #71 uses `uni_edit_test_old.exe`, built from c015c09 with `-DOLD_API`:
269 patterns differ, against 1 with this branch.

### Captures (`docs/uni_captures/`, via `gonptechan --open … --pattern P --frame F --zoom Z [--no-pups] --capture`)
| file | shows |
|---|---|
| `mbtl_chr000_p0_f0.png` | #68: Arcueid neutral, layer 0 empty, boxes drawn |
| `mbtl_chr011_ciel_p0_f0.png`, `mbtl_chr012_saber_p0_f0.png` | TL characters' boxes (Saber was the only one working before) |
| `mbtl_chr016_p6_f6_inverted_atk.png` | Powered Ciel 2C attack boxes, including the inverted box kept as stored |
| `uni2_chr027_p128_pups1.png` / `uni2_chr027_p128_nopups.png` | #76: Zohar PUPS 1 with `chr027_p1.pal` vs the base palette |
| `uni2_chr006_merkava_p0_3layers.png` | UNI2 three-CG-layer frame (layers 0-2) |

## 7. Tools added
- `tools/uni/ha6walk.py`: HA6 walker with the games' payload sizes.
- `ha6diff.py`: tag-level diff.
- `ha6patdiff.py`: which patterns differ.
- `ha6_order.py`: tag-order conflict analysis.
- `ha6_census.py`: value census.
- `patwalk.py`: PAT record split and diff.
- `rt_all.sh`, `pat_rt_all.sh`, `pal_rt_all.sh`, `uni_regress.sh`.
- `roundtrip.exe --bytes/--fresh`, `pat_roundtrip.exe --bytes/--fresh`, `pal_roundtrip.exe`, `uni_edit_test.exe`.
- `gonptechan --zoom/--game/--no-pups`.

## 8. Known gaps
- **BaseData templates:** the editor shows and saves the character's own pattern. It does not preview the game's
  merged result (template timing + character sprites/boxes, §2.2).
- **PUPS:** the PAT (parts) layers of a PUPS pattern still use the parts' base palette. A missing `_pN.pal` falls
  back to the base palette instead of the game's grey ramp.
- **Palettes:** there is no palette editor, so `PalFile` is only used by the round-trip tool so far. The alternate
  colour set (palette c+65) is chosen by palette number, as before.
- **Unused fields:** ATRF, ATBC, ATAB, ATBG and ATGE are shown or kept with their offsets, but their gameplay
  meaning past the loader was not traced.
- **Game detection:** it relies on the AFGX layer count. A UNI-family file without AFGX (only ATV2) is treated as
  UNI; `--game` or View > Game format overrides it.
