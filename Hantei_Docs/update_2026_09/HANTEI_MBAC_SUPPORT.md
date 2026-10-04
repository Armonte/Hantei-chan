# MBAC (Melty Blood Act Cadenza) / Hantei4 "HA4" support in Hantei-chan

Branch `feat/mbac` (based on `update/ex-mbac`). Everything described here was built with `./build.sh`
(MinGW) and tested on all 50 MBAC `.DAT` files in `C:\games\MB\AC\install\MBACPC\02_extracted`.

The byte layout of the container is not repeated here. It is in `docs/bg_research/HA4_SECTION.md`, with
V-ed/V-rt/V-data evidence. This document covers how Hantei-chan uses that layout.

## 1. What works

| Feature | Status |
|---|---|
| Open an MBAC character `.DAT` | **Works.** Use File → *Load MBAC .DAT (Act Cadenza)...*. *Load HA6...*, `.txt` `[DataFile]` lists, projects (`.hproj`) and `--open` also work, because the file type is detected by content (`"Hantei4\0"`) in `FrameData::load`. |
| All editor views (frames, AF/AS/AT/EF/IF panes, boxes, spawn visualisation, undo) | **Works.** HA4 is mapped into the normal HA6 in-memory model. |
| Sprites + palettes | **Works.** The embedded CG bank (`BMP Cutter3`, the same format as MBAACC `.cg`) is loaded from memory. The sibling `<name>.PAL` provides 64 palettes, the same format as MBAACC `.pal`. |
| Parts (`sprite < 10000`) | **Works.** The embedded old-format PAT is converted to the editor's PAniDataFile Parts model and rendered. |
| `EFFECT.DAT` as the effect character | **Works.** It is loaded automatically from the same folder (EF 8 targets). |
| Save back to `.DAT` | **Works.** Load → save is byte-identical on **50/50** files. Edits re-encode only the fields that changed. |
| MBAC → HA6 conversion (GUI + CLI) | **Works.** It writes `.ha6`, `.cg`, `.pat`, `.pal` and `.txt`. The 44/44 tag-in/out golden patterns are identical to MBAACC. |
| HA4-only fields in the UI | "MBAC (HA4)" inspector window (Windows menu). |

## 2. Design

```
.DAT ──ha4::Load──► FrameData (m_sequences = 256 Sequence, m_ha4 = header + parts blob + CG blob)
                     │   Frame.ha4    = Ha4FrameRaw (original 216-B record, AT/IF/EF records, raw boxes)
                     │   Sequence.ha4 = Ha4SeqRaw  (0x44-B pattern header, raw 64-B name)
                     ├─ ha4::AttachCharacterResources: CG from blob, .PAL, parts → Parts, EFFECT.DAT
                     ├─ FrameData::save(*.DAT)  ──ha4::SaveFile──► .DAT (byte-exact when unedited)
                     └─ FrameData::save(*.ha6) / ha4conv::Convert ──► .ha6 (+ .cg .pat .pal .txt)
```

### Files (new)

| File | Purpose |
|---|---|
| `src/ha4_raw.h` | `Ha4FrameRaw`, `Ha4SeqRaw`: fixed-size POD blocks with the original bytes. They are members of `Frame_T` / `Sequence_T`, so copy/paste, the shared-memory clipboard, frame insert and undo all carry them along. |
| `src/framedata_ha4.h/.cpp` | Container parse (`ha4::Load`) and writer (`ha4::Serialize` / `SaveFile`), field codecs, last-save error/warnings. |
| `src/ha4_parts.h/.cpp`, `src/ha4_parts_gl.cpp` | Old-format PAT → `Parts` conversion (no GL) and texture upload (GUI only). Reuses `bg::OldPat` (stage code). |
| `src/ha4_convert.h/.cpp` | MBAC → HA6 export (`ha4conv::Convert`, `ConvertFile`, `PrepareForHA6`). |
| `src/ha4_character.h/.cpp` | Editor glue: resource attach, "MBAC (HA4)" inspector, *Export MBAC as HA6*. |
| `src/ha4tool.cpp` | CLI target `ha4tool` (`roundtrip`, `reencode`, `edittest`, `convert`, `dump`). |
| `src/startup_args.h/.cpp` | `gonptechan --open <file> [--pattern N --frame N --palette N] [--capture out.png]`. Opens a file at startup and can screenshot and quit; used for the render checks. |
| `tools/ha4/ha4_golden.py`, `tools/ha4/pat_compare.py` | Golden test against MBAACC HA6 / `.pat`. |
| `tools/ha4/results/*.txt` | Test outputs quoted in §6. |

### Edits to shared files (all additive)

- `framedata.h`: `#include "ha4_raw.h"`; `Frame_T::ha4` and `Sequence_T::ha4` (with their copy operators);
  `FrameData::m_ha4` and `isHA4()`; `initEmpty(unsigned count = 1000)`.
- `framedata.cpp`: HA4 detection in `load()`. `save()` writes HA4 when `m_ha4` is set and the target is not
  `*.ha6`. `Free()` resets `m_ha4`.
- `undo_manager.cpp`: `frameEquals` / `SequenceContentEquals` also compare the HA4 raw blocks. The size asserts
  are updated (`Sequence` 112 → 248, `Frame` formula + `sizeof(Ha4FrameRaw)`), as the coordinator's static
  checks require.
- `cg.h/.cpp`: `CG::loadFromMemory()`. `load()` now shares `loadOwned()`. Also accepts `"BMP Cutter2"`
  (GAKIHA.DAT).
- `character_instance.cpp`: after `loadHA6`, `ha4::AttachCharacterResources` runs when the data is HA4.
- `filedialog.h/.cpp`: `fileType::HA4` filter.
- `ui/main_menu_impl.h`: File → *Load MBAC .DAT (Act Cadenza)...* and *Export MBAC as HA6...*; Windows →
  *MBAC (HA4) Inspector*.
- `ui/main_ui_impl.h`: one line, `ha4ui::DrawInspector(...)`.
- `main_frame.cpp/.h`: includes; the save-error text uses `ha4::LastSaveError()`; the
  `ProcessStartupArgs()` call in `Draw()`.
- `main.cpp`: parses `--open/--pattern/--frame/--palette/--capture`.
- `parts/parts_partset.cpp`: in MBAACC mode, a rotation-only part is saved as `PRAN`, not `PRA3`. MBAA.exe
  contains `PRAN` but no `PRA3` string, so before this change every rotated part in an MBAACC-style `.pat`
  saved by Hantei-chan lost its rotation in the game.
- `CMakeLists.txt`: new sources; `framedata_ha4.cpp` in `roundtrip`/`ha6_dump`; new `ha4tool` target.

### Writer strategy (why the round trip is byte-exact)

Each HA4 field has a decoder (raw → model) and an encoder (model → raw). For every field the writer checks whether
the frame still carries its original bytes **and** whether decoding those bytes gives the current model value. If
both are true, it keeps the original bytes. Otherwise it encodes the model value. Untouched data therefore
reproduces:
- the heap garbage in `idx[1..7]` / `idx[57]`;
- the 40-byte stack garbage in the pattern header;
- AS pads;
- IF params 9..11;
- odd parts-frame box coordinates;
- stale speeds on flag-less AS axes;
- name bytes after the NUL;
- flip-mode vs free-rotation choices.

Edited fields are encoded canonically.

Tables are rebuilt exactly as `WriteHantei4DatFile` does:
- records are appended in frame and slot order (box slots 0..24 then attack 1..8, AT, IF 0..7, EF 0..7), with no dedup;
- the offsets are `0x44 + 216*nf`, running, and -1 when a table is empty;
- an AT is written iff an attack box is non-empty;
- a box is written iff `x1≠x2 && y1≠y2`.

IF/EF slot positions are reused when the record count is unchanged. Otherwise they are packed from slot 0.

Save refuses (with a message in the error popup and in the inspector) when the data can't be represented:
a pattern index ≥ 256 with frames, more than 100 frames, or a table index overflow. Lossy but saveable edits
produce warnings, listed under "Last save" in the inspector:
- extra layers beyond layer 0;
- an AFRG tint;
- AST0 sine motion;
- ATUH / ATGN;
- more than 8 IF or EF;
- EF params 8..11 outside int16;
- speed on an axis with no flag;
- flip V combined with another rotation.

## 3. Field mapping (HA4 → editor model → HA6 tag)

| HA4 | Model | HA6 | Notes |
|---|---|---|---|
| pattern +4 moveInfo & 0xF | `Sequence::psts` | PSTS | Bits 0x40/0x80 are kept raw and shown in the inspector. |
| pattern +8 level | `level` | PLVL | |
| pattern +0x1C..0x43 | raw | — | Stack garbage, preserved. |
| name slot (64 B CP932) | `name` (UTF-8) | PTT2 | Empty slots keep their default names in HA4. The HA6 export drops them. |
| AF +0 sprite | layer0 `spriteId`, `usePat` | AFGP | ≥10000 → CG `id-10000`, usePat 0; 0..9999 → parts, usePat 1. |
| AF +2/+4 offset | `offset_x/y` | AFOF / AFY | |
| AF +6 wait | `duration` | AFD | |
| AF +8 flip mode, +0x24 angle | layer `rotation[0..2]` | AFAX/AFAY/AFAZ | 1→Y .5; 2→X .5; 3/4/5→Z .25/.5/.75; 6/7→Y .5+Z .25/.75; 8→Z rot/10000; 9→Y .5+Z rot/10000. |
| AF +9 blend, +0xA amount | `blend_mode`, `rgba[3]` | AFAL | Verbatim (1 translucent, 2 add, 3 sub). Alpha = amount/255 when blend ≠ 0. |
| AF +0xB AniFlag | `aniType`, `aniFlag` | AFF/AFFE | 0→(0,0) 1→(1,0) 2→(2,0) 3→(1,1) 4→(2,1) 5→(2,2) |
| AF +0xC/+0xD | `jump`, `landJump` | AFJP/AFJC | |
| AF +0xE priority | `priority` | AFPR | |
| AF +0x10/+0x12 zoom | layer `scale` | AFZM | v/256; 0 = 1.0 |
| AF +0x14/+0x15/+0x16 | `interpolationType`, `loopEnd`, `loopCount` | AFHK/AFLP/AFCT | |
| AS +0..+3 clear/add X/Y | `movementFlags` | ASV0 | clear+add → Set (0x10/0x01); add → Add (0x20/0x02); clear only → Set with 0 speed/accel. |
| AS +8/+A, +10/+12 | `speed[]`, `accel[]` | ASV0 | Zeroed on axes without a flag (MBAC keeps stale values; kept raw). |
| AS +0x18/+0x19/+0x1A | `stanceState`, `cancelNormal`, `cancelSpecial` | ASS1/2, ASCN, ASCS | |
| AS +0x1C / +0x1D | `hitsNumber` / `canMove` | ASAA / ASMV | |
| AS +0x24 flags1 | `statusFlags[0]` | ASF0 | |
| AS +0x28 flags2 | `statusFlags[1]` (bits 16-23 cleared), `invincibility` = b16-19, `counterType` = b20-23 | ASF1, ASYS, ASCT | |
| AS +0x30 (i16) | `maxSpeedX` | ASMX | |
| AT +0 | `guard_flags` | ATGD | |
| AT +8/+0x18 (3× i16) | `hitVector`/`hVFlags`, `guardVector`/`gVFlags` | ATHV/ATGV | Low byte = vector id, high byte = flags. |
| AT +0x28/+0x2A | `hitEffect`, `soundEffect` | ATHE | |
| AT +0x30/+0x32 | `hitStopTime`, `untechTime` | ATSN/ATSU | |
| AT +0x38/+0x39/+0x3A | `addedEffect`, `hitgrab`, `hitStop` | ATKK/ATNG/ATSP | |
| AT +0x3B/+0x3C | `correction` (0 → 100), `correction_type` | ATHS/ATHT | |
| AT +0x40 | `otherFlags` | ATF1 | |
| AT +0x48/+0x4A/+0x4C/+0x4E | `damage`, `meter_gain`, `red_damage`, `guard_damage` | ATVV (d0 = +0x4C, d1 = +0x48, d2 = +0x4E, d3 = +0x4A) | |
| AT +0x50 | `breakTime` | ATBT | |
| IF slot (type, params[12]) | `Frame_IF` type + params 0..8 | IFTP/IFPR | Params 9..11 are always 0; kept raw. |
| EF slot (type, No, p[8] i32, q[4] i16) | `Frame_EF` type, number, params 0..7 + 8..11 | EFTP/EFNO/EFPR | Types 1,2,3,4,8,11: p0−128, p1−224. |
| box slots idx[24..48] / idx[49..56] | `hitboxes[0..24]` / `[25..32]` | HRNM / HRAT | CG frame: (x−128, y−224). Parts frame: ((x>>1)−160, (y>>1)−224) (mbacPC `Actor_BoxToWorldRect` 0x440750). |
| idx[1..7], idx[57], AS/AF pads | raw | — | |

HA6-only fields get defaults on load and are not stored in HA4:
- AFRG, extra layers, AFRT/AFID/AFPA/AFJH;
- AST0, ASCF;
- ATUH, ATGN, UNI AT fields;
- PFLG, PUPS.

## 4. MBAC → HA6 conversion rules

The HA6 export is the normal Hantei-chan HA6 writer applied to the mapped model, plus:
1. Patterns without frames are not written (their default names are dropped).
2. HA4 raw blocks are ignored (HA6 has no place for them).
3. **CG:** the embedded bank is written verbatim as `<name>.cg`. It must be the MBAC bank, because MBAACC
   re-cut the CG, so its ids differ.
4. **Palette:** `<NAME>.PAL` is copied to `<name>.pal`, which is byte-compatible. Files without a `.PAL`
   (`CSEL_*`, `EFFECT`, bosses) use the CG's built-in palette.
5. **Parts:** the old PAT becomes `<name>.pat` (PAniDataFile, MBAACC tag variants). The mapping was measured
   against MBAACC `arc.pat`, where all 580 parts are equal:

   | Tag | Rule |
   |---|---|
   | PRXY | (x−320, y−448) |
   | PRZM | scale/1000 |
   | PRAN | rot/10000 |
   | PRPR | +71 |
   | PRID | cutout |
   | PRRV | +48 |
   | PRAL | +49 |
   | PRFL | +50 |
   | PRCL | B,G,R,A from +67..+64 |
   | PRSP | +68..70 |
   | PPUV | src rect in 256-unit space |
   | PPSS | quad W/H |
   | PPCC | origin |
   | PPTP | texture |
   | PPTX | cutout dword 1 |
   | PGTX | 32-bpp BGRA square texture (named `texN`) |

6. **`<name>.txt`** gets `[DataFile] File00=<name>.ha6`, `[BmpcutFile] File00=<name>.cg` and
   `[PAniFile] File00=<name>.pat`, so the result opens with *Load from .txt*.
7. **EF/IF numbering:** no renumbering is applied, because MBAC and MBAACC use the same numbering.
   - EF 9 is "play sound" in both, per hantei4 `ExportEF9SoundUsageToIni`.
   - MBAC data contains no MBAA-only EF types (14, 101, 111, 1000, 10002).
   - The MBAC IF types that hantei4 leaves unlabelled (38, 39, 51–54, 100, 150, 151) have the same meaning in
     MBAA. See `docs/tag_research/HANTEI4_EF_IF_LABELS.md` §1–2 and `docs/bg_research/HA4_SECTION.md` §2.2.
   - The measured agreement below (EF 97.1%, IF 97.2% over 49,701 frames) confirms this. The remaining
     differences are MBAACC balance edits and are not systematic.
   - The hantei4 hit-effect table lists 12/13 as small/big flash, which is the reverse of Hantei-chan's labels.
     This is a **label** difference only; ATHE is copied verbatim and agrees in 94% of cases.
8. **EFFECT.DAT** converts like any other file (`effect.ha6/.cg/.pat/.txt`). EF 8 targets keep their pattern
   numbers.

CLI: `ha4tool convert <A.DAT> <B.DAT>... -o <outdir> [--name base]`. GUI: File → *Export MBAC as HA6...*
(pick the `.ha6` name; the other files are written next to it). *Save Character As* with a `.ha6` name
converts only the frame data.

## 5. UI notes

- **MBAC (HA4) inspector** (Windows → *MBAC (HA4) Inspector*; it appears only for HA4 data). It shows:
  - the source path and blob sizes;
  - move type (技情報) and the moveInfo bits 0x40/0x80;
  - the raw sprite id;
  - the display mode as the hantei4 enum 表示, editable, with the arbitrary angle for modes 8/9;
  - blend 半透明 as MBAC names;
  - AniFlag as the MBAC 6-value enum;
  - a priority label (hantei4 `g_layer_priority_strings`);
  - warnings for HA6-only values HA4 can't store;
  - the preserved garbage/pad fields;
  - the last save error and warnings.
- The normal panes edit everything else. AniFlag combinations that HA4 cannot express are flagged in the
  inspector and saved as the nearest AniFlag.
- The status bar shows "Save to: X.DAT". Ctrl+S / *Save Character* writes HA4.

## 6. Test results

All outputs are in `tools/ha4/results/`.

| Test | Result |
|---|---|
| `ha4tool roundtrip` (load → save, byte compare), 50 files, 4446 patterns / 67721 frames | **50/50 byte-identical** |
| `ha4tool reencode`: encode from the model alone (raw blocks dropped), reload, compare all model fields | **50/50, 0 diffs** |
| `ha4tool edittest`: duplicate frames, move/add/remove boxes, add/remove EF/IF, change AF/AS/AT fields, add a brand-new pattern; save, reload, compare | **50/50, 0 diffs** |
| `ha4tool convert` all 50 | 50/50 OK; 23 palettes copied; 17 `.pat` written |
| Converted HA6 in the editor (`roundtrip.exe`: load → save → reload) | **50/50, 0 field diffs**, no loader warnings |
| Converted HA6 parsed by `ha6lib.py` | 50 files, 4446 patterns, 67721 frames |
| **Golden:** tag-in/out patterns vs shipped MBAACC HA6 (all fields except CG sprite id) | **44/44 identical** |
| Field agreement vs MBAACC over 3178 same-length patterns / 49701 frames | AF 95.8–100%, AS 99.3–99.9%, EF 97.1%, IF 97.2%, AT 93.9%, boxes 89.9% (the rest are MBAACC balance/art edits) |
| Converted `.pat` vs MBAACC `.pat` (`pat_compare.py`) | arc: 2443/2462 fields equal (19 are PRAN 0.036 vs MBAACC 0.0359); len and warc: 100%; hisui/kohaku/neco/nechaos differ only in PRFL (MBAACC turned on filtering); m_hisui/v_sion/effect were reworked in MBAACC |
| Rendering (screenshots via `--capture`) | AKIHA p0 (CG + `.PAL`), ARC p195 (parts), GAKIHA (Cutter2 CG + large parts), SION via `.hproj` (palette 7), converted `arc.txt` |

Golden-test notes:
- ha6lib ignores the `AFY` shorthand, so `ha4_golden.py` decodes AFY itself (m_hisui p241 uses AFY3).
- Boxes are compared by area: kishima p241 has an inverted box that MBAACC kept inverted, while Hantei-chan's HA6
  writer normalizes it. The game swaps inverted corners itself, so the difference has no effect.

Reproduce:

```
./build.sh
cd /mnt/c/games/MB/AC/install/MBACPC/02_extracted
../../../../../dev/hantei-chan/wt/mbac/build/ha4tool.exe roundtrip *.DAT      # or reencode / edittest
ha4tool.exe convert *.DAT -o 'C:\out\mbac_ha6'
python3 tools/ha4/ha4_golden.py <out>/mbac_ha6
gonptechan.exe --open C:\...\ARC.DAT --pattern 195 --frame 3 --capture C:\tmp\arc.png
```

## 7. Known gaps

- **Undo memory:** each frame carries its 1.4 KB raw block, so an undo snapshot of an HA4 pattern is larger.
  The undo tests pass: `undo_manager_test` → PASS.
- **No HA6 → HA4 direction:** only data loaded from a `.DAT` can be written as HA4. An HA6 character saved to a
  `*.dat` name is still written as HA6.
- **Parts editing:** the parts are converted in memory for display. *Save Parts As...* writes them as a
  PAniDataFile `.pat`, but edits are not written back into the `.DAT` parts blob; saving the `.DAT` writes the
  original blob unchanged. That would need an old-PAT writer.
- **PAT edge case:** a part slot with cutout 0, position (320,448) and every other field at its default is
  omitted by `PartSet::Save`, which only writes modified props. No shipped data hits this case; the MBAACC
  converter has the same behaviour.
- **`BMP Cutter2`** (GAKIHA): it loads and renders with the same table layout, but its "has palette" dword is 0,
  and the game's handling of that flag is not verified.
- **Unverified mappings:** flip modes 2/5/6/7/9 map to rotations by inference only (no shipped data uses them).
  AT +0x30 → ATSN is also inferred (always 0 in the data).
- **Converted HA6 is not a playable MBAACC character on its own.** It has no `_c.txt` command list (MBAC ships
  `_C.TXT`/`_C.CT`, which another branch handles) and no moon files. The data files are complete.
- **Save As `*.ha6` from an HA4 character** converts only the frame data. Use *Export MBAC as HA6* to get the
  `.cg/.pat/.pal`.

## 8. `han4` branch bugs (HA4_SECTION.md §3.1): status in this implementation

None of the `han4` code was reused except for UI ideas. The table below shows how each bug is handled here.

| # | Bug | Here |
|---|---|---|
| H1 | Offset table at 0x40 | The table is at 0x44. |
| H2 | Patterns compacted | Slot numbers are kept (256 slots). |
| H3/H4/H5 | Header misnamed / names and CG at the wrong offsets | partsOff/partsSize/cgOff/cgSize are used as documented; names are at partsOff+partsSize+cgSize. |
| H6 | Table offsets swapped | box/AT/IF/EF are read at +0x0C/+0x10/+0x14/+0x18. |
| H7 | IF/EF index ranges | idx[8..15] / [16..23]. |
| H8 | AT treated as a hitbox | The full AT is mapped (§3). |
| H9 | Boxes | All 33 slots; corners kept; the bias is removed; parts frames are half-scaled. |
| H10/H11/H12 | AF offsets and forced defaults | Every AF field is mapped at the correct offset. |
| H13 | ASV0 from flags1 | ASV0 comes from the AS bytes; ASMX is at +0x30. |
| H14 | EF q as int32 | q[4] is read as int16; the EF position bias is removed. |
| H15 | Bogus return value | — |
| H16 | Tag-based HA4 | Only the binary container exists and only it is parsed. |
| H17 | Parts marked usePat=0 | usePat=1; the parts are converted to PAniDataFile and rendered. |
