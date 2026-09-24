# UNI2 and MBTL game data: archives, file layout and formats

Written for testing and fixing Hantei-chan against UNDER NIGHT IN-BIRTH II Sys:Celes (UNI2) and
MELTY BLOOD: TYPE LUMINA (MBTL). Related GitHub issues on Armonte/Hantei-chan: #14, #46, #68, #71, #74 and #76.

- Extracted data (not committed): `gamedata/uni2/`, `gamedata/mbtl/`. Each has a `MANIFEST.md`.
- Extractors (stdlib Python): `tools/gamedata/uni2_extract.py`, `tools/gamedata/mbtl_extract.py`
- Rough HA6 tag census/diff: `tools/gamedata/ha6_tagcount.py`
- Test logs: `gamedata/_reports/*.log`

`/mnt/c/dev/hantei-chan` is not a git repo; only `Hantei-chan/` is. `gamedata/` sits outside that repo, so no
`.gitignore` change was needed. Do not copy game files into `Hantei-chan/`.

---

## 1. Archives

### UNI2 (`<UNI2>/d/`)
- About 113 entries, all with obfuscated names. 27 of them are **index files** (hard-coded list in Ekey's
  UNIB.Unpacker and in `uni2_extract.py`). Each index names a **data file** in the same folder.
- Index layout (little-endian):
  ```
  header : i32 nFolders, i32 nFiles, u32 archiveSize, char[52] dataFileName
  folder : i32 filesInFolder, i32 ?, i32 ?, char[116] folderName        x nFolders
  pad    : 4 bytes
  entry  : i32 decompSize, i32 compSize, u32 offset, char[64] name, 4-byte pad   x nFiles
  ```
  Entries belong to folders in order: folder[0] owns the first N entries, and so on.
- The data is **stored raw**: no encryption, and compSize == decompSize for every entry seen.
- The ~50 remaining loose obfuscated files (4 MB each, dated Oct 2025) are Ogg Vorbis BGM (`OggS`).
- Key pairs: `hexeojmpimrjs` -> `ehex` = `data/` (all character data), `fmjisrkojmp` -> `id` = `bg/`,
  `kluioyxovozlort` -> `dcmbsz` = `script/`, `rcwWiiqjmxpmojs` -> `joiIuc` = `System/`, `IQHoknqjnskt` -> `FNE` = `DLC/`.
- `<UNI2>/mod/___Csel`, `___English`, ... is **not** game data. It is the override tree used by the nuuni bypass
  winmm proxy: loose files with the same relative paths replace the archive's copies.

### MBTL (`<MBTL>/data000..019.bin`)
- The archives have **no TOC**. The file table is compiled into `MBTL.exe` (.rdata):
  - a `char*[20]` array -> `"data000"`..`"data019"`, **immediately followed by `i32 size[N]`**
    (file offset 0x4A0E00 in the Sep-2026 exe, N = 22177);
  - then 3 dwords, then **ENTRY_KEY[1024]** (file offset 0x4B6890), **immediately followed by `char* name[N]`**.
  - **No offsets are stored.** Entries of one top-level folder are packed back to back in table order, so
    offset = running sum of sizes per archive. This matches every archive's file size exactly.
  - `BattleRes` and `grpdat` overflow: once the running sum reaches data005/data010's file size, the remaining
    entries continue at offset 0 of data018 (Append_0) or data019 (Append_1). Their names keep the
    `BattleRes/`/`grpdat/` prefix.
- Mapping of top-level folder to archive: `___English` 000, `___Korean` 001, `___Region` 002, `___S_Chinese` 003,
  `___T_Chinese` 004, `BattleRes` 005(+018), `bg` 006, `Bgm` 007, **`data` 008**, `DLC` 009,
  `grpdat` 010(+019), `script` 011, `se` 012, `Shader` 013, `System` 014, `___French` 015,
  `___Portuguese` 016, `___Spanish` 017.
- **Per-entry XOR cipher** (Ekey's BinCipher):
  ```
  b[0]^=0xA5; b[1]^=0x18; A=b[0]^0xAC; B=A^b[1]^0x76381
  for i = len-1 downto 2: b[i] ^= KEY[A ^ (B & 0x3FF)]; B++
  ```
  The keystream depends on the entry length and repeats with period 1024. `mbtl_extract.py` vectorises it and
  checks the result against a literal port.
- **The static caches are stale.** Ekey's `Cache_FN/OT/ST` files (in the tool and in `<MBTL>/0.10.2.0/`) predate
  the 2026 patches. data008 and the other updated archives decrypt to garbage from the first changed entry
  onward (for example, `data/chr022/*` and `data/effect.HA6`). Always read the table from the current exe
  (`mbtl_extract.py` does this by default). `MBTL.Unpacker.bat` would also write about 21 GB into `<MBTL>\OUT`.
- `<MBTL>/fu/data` (filepatch-tl) and `<MBTL>/__Mods` (mbtl_mod_loader) are user override folders.

---

## 2. Per-character file set (both games share the French-Bread "Hantei6" layout)

Folder `data/chrNNN/`. The prefixes are identical in both games; only the variant suffixes differ.

| file | format | notes |
|---|---|---|
| `chrNNN_0.txt` | INI "txt project" | what Hantei-chan's "load via txt" opens (section 4) |
| `_temp.ha6` | HA6 | File00: template/shared patterns |
| `chrNNN.ha6` | HA6 | File01: the character's own frame data (**save target**) |
| `../BaseData.HA6` | HA6 | last file: common system patterns (in `data/`) |
| `chrNNN.cg` | "BMP Cutter3" | sprite container, the same container as MBAACC `.cg` |
| `chrNNN.pal` | palette bank | 133,136 bytes = 16-byte header (`FFFF0000 01000000 00000000 82000000`) + 130 x 256 BGRA |
| `chrNNN_pN.pal` | palette bank | alternate banks selected by the HA6 **PUPS** tag (PUPS 0 = `chrNNN.pal`, PUPS n = `_pn.pal`) |
| `chrNNN.pat` | "PAniDataFile" | parts/PAni: textured parts, shapes and animation sets (the `P_ST` version is 1 in UNI2 and 3 in MBTL) |
| `chrNNN.lst`, `BaseData.lst`/`basedata.lst`, `_temp.lst` | name list | `u32 count` + a fixed table of 32-byte `"000_000.bmp"` names (the cg's source bitmap names) |
| `chrNNN_mv_0.txt` | Squirrel | move scripts: `t.Mv_* <- { Init_After / FrameUpdate_After ... }` (Hantei-chan `mv_script.cpp`) |
| `chrNNN_cmd_0.txt` | Squirrel | command table (`Skill_236EX = { CmdCheck = ... }`); the `_c.txt` equivalent |
| `chrNNN_com_0.txt`, `_com_ranking_0.txt` | Squirrel | CPU AI |
| `chrNNN_sc_0.txt` | Squirrel | small per-character script |
| `chrNNN_se_list.txt`, `_se_category.txt` | Squirrel | SE tables |

Shared in `data/`: `BaseData.HA6`; the effect project `effect.txt` (`effect_temp.ha6` + `effect.HA6` +
`sys_effect.pat`, BmpcutFile FileNum=0, plus `effect.LST`/`effect_temp.LST`); `VectorTable.txt`, `BoundEff.txt`,
`BattleInfo.txt`, `_combase*.txt`; `_csel/` (CSS: `csel_chrNNN.{ha6,cg,lst,txt}` + `chrNNN.pal`, some `.pat`);
`_coloredit/` (`edit_chrNNN.{ha6,cg,pat,lst,txt}`). MBTL also has `data/_talk/npcNNN/` (story NPCs with their
own ha6/cg/pat/pal and `_mv/_cmd/_sc` scripts). Training command lists are in `System/cmd_chrNNN.txt`, and the
roster is in `System/BtlCharaTbl.txt` (Squirrel, cp932).

**Case sensitivity:** file names mix case (`BaseData.HA6` vs `basedata.lst`, `chr001_7.LST`, one `edit_chrNNN.LST`
in UNI2). On Windows this does not matter; a native Linux build would need case-insensitive lookup.

### UNI2 roster (28): chr000-chr027
Hyde, Linne, Waldstein, Carmine, Orie, Gordeau, Merkava, Vatista, Seth, Yuzuriha, Hilda, Eltnum, Nanase, Byakuya,
Akatsuki, Chaos, Wagner, Enkidu, Londrekia, Tsurugi, Uzuki (DLC), Mika, Kaguya, Kuon (DLC), Phonon, Ogre (DLC),
Izumi (DLC), Zohar (DLC). Every character has exactly the 15-file set above, plus `chr027_p1.pal`.

### MBTL roster (22 folders): chr000-chr022 without chr007
Arcueid, Hisui, Akiha, Shiki, Kohaku, Roa, Kouma, [chr007 Hisui & Kohaku: no folder], Noel, Vlov, Red Arcueid,
Ciel, Saber, Miyako, Dead Apostle Noel, Aoko, Powered Ciel, Mario, "Sister" (chr018), Neco-Arc, Mash,
Ushiwakamaru, Edmond. MBTL also has **variant projects** `chrNNN_6/_7/_8/_9.txt`, with matching
`chrNNN_N.ha6`, `_N.lst`, `_cmd_N`, `_com_N`, `_mv_N` and `_sc_N` files. The exe loads `chrNNN_%d.txt`.
`chrNNN.txt`, where present, is byte-identical to `chrNNN_9.txt`.

---

## 3. HA6 format

- Header: the same 32-byte `"Hantei6DataFile"` magic as MBAACC, then `_STR <n>` (1000 patterns), and per pattern
  `PSTR <i>`, `PTT2 <len=0x20><name>`, ... `PEND`, with a final `_END`. **The header does not differ from MBAACC.**
  MBTL PTT2 name buffers contain uninitialised bytes after the terminating NUL. A zero-padding saver changes
  those bytes, which is harmless.
- Tag census (`ha6_tagcount.py --union` over MBAACC `data/*.HA6`, all UNI2 and all MBTL HA6 files; count/files):

| tag | MBAACC | UNI2 | MBTL | meaning |
|---|---|---|---|---|
| AFGP | 84262/239 | 1/1 | 2/1 | MBAACC single-layer frame graphic |
| **AFGX** | 0 | 144115/115 | 89383/147 | multi-layer frame graphic (with **AFPL** layer priority) |
| **AFID**, **AFPA**, **AFJH** | 0 | yes | yes | frame ID and params for Squirrel scripts; jump helper |
| **ATV2** | 0 | 1983/59 | 1865/74 | combined hit+guard vectors, replacing ATVV/ATHV/ATGV (absent in UNI2/MBTL) |
| **ATAM, ATAT, ATC0, ATHH, ATSA, ATSH** | 0 | yes | yes | separate damage, meter, proration, hitstun decay and starter correction |
| ATRF, ATBC, ATS3, ATS5, ATS6 | 0 | UNI2 only | 0 | semantics unknown, preserved verbatim |
| ATVD | 0 | 0 | MBTL (chr020) | semantics unknown, preserved |
| ATCA | 0 | 0 | 39/16 | MBTL only |
| ASCF | 0 | 8/7 | 21/12 | |
| **PUPS** | 0 | 9/2 | 129/9 | pattern-level palette-bank switch (issue #76) |
| PLVL, PSTS, PDST, ATHT, ATUH, ASF0 | yes | 0 | 0 | MBAACC-only |

  PUPS users: UNI2 `chr027.ha6` (8x) and `_coloredit/edit_chr027.ha6`. MBTL `chr017.ha6` (7x), `chr017/_temp.ha6`,
  `_coloredit/edit_chr011/018/019`, and `_talk/npc003` (61x), `npc013` (32x), `npc020` (24x), `npc002`.
  Palettes `_p1.._p3.pal` also exist for MBTL chr001, chr011 and chr019 even though their HA6 files contain no PUPS.
- Hantei-chan's loader (`framedata_load.cpp`) prints `Unknown ... tag` for anything it does not parse.
  **No unknown tags were reported for any of the 115 UNI2 and 147 MBTL HA6 files.** PUPS is loaded and saved
  (`framedata_save.cpp:677`), but the renderer never switches to `_pN.pal`: `ini.cpp` only loads `<cg>.pal`.
- Boxes use the MBAACC scheme: HRNM/HRNS hurt/collision, HRAT/HRAS attack (+25), references via `*S` tags.

## 4. "txt project" files (issue #46)

```
[System]            ProjectName=chr000
[DataFile]          FileNum= 3 | File00=_temp.ha6 | File01=chr000.ha6 | File02=../BaseData.HA6
[BmpcutFile]        FileNum= 1 | File00=chr000.cg        (the .pal is <cg name>.pal)
[PAniFile]          FileNum= 1 | File00=chr000.pat
```
- Paths are relative to the txt's folder (`../BaseData.HA6` points up to `data/`).
- The patterns are merged in File order (a later file wins).
- MBTL variants use **FileNum=4**: `_temp, chrNNN, chrNNN_N, ../BaseData`. `chr019_0_nobase.txt` uses FileNum=2 (no BaseData).
- `data/effect.txt` puts `[Project] File=effect` first, uses `FileNum= 2` (`effect_temp.ha6`, `effect.ha6`), and has `BmpcutFile FileNum= 0`.
- Current `LoadFromIni` save-target rule: if File00 starts with `_temp`, the target is File01. For MBTL variant
  projects this picks `chrNNN.ha6`, **not** the variant `chrNNN_N.ha6` (File02) that the project actually
  overlays. Saving from `chrNNN_7.txt` would therefore write the merged result into the base file. Consider
  "last non-BaseData file" as the target.
- Pattern 0 overlap: MBTL chr000, chr001, chr013 and chr021 have a non-empty pattern 0 in `_temp.ha6` that differs
  from `chrNNN.ha6` (for example, chr001 has 12 frames vs 21, and chr013 has 22 vs 19). Every other MBTL character,
  and every UNI2 character, has an empty pattern 0 in `_temp.ha6`. Consider this when working on issue #68
  (boxes missing or delayed in pattern 0).

## 5. Sprites, palettes, parts

- `.cg` is `"BMP Cutter3"` in both games. The layout matches MBAACC (Hantei-chan `cg.cpp` handles Cutter2 and
  Cutter3). The files are large: UNI2 chr002.cg is 143 MB, and MBTL chr000.cg is 27 MB.
- `.pal` holds 130 palettes of 256 BGRA colours after a 16-byte header (133,136 bytes). MBAACC `.pal` files are 65,540 bytes: a 4-byte count followed by 64 palettes.
- `.pat` "PAniDataFile" is used for characters, sys_effect, CSS and colour-edit, NPCs, and stage objects.
  `pat_dump` output for UNI2 chr000.pat: 683 part sets, 1004 cutouts, 12 shapes.
- Frames reference PAT parts through AFGX layers flagged `usePat` (negative sprite id in `ha6_dump`).

## 6. Stages

Both games use 3D stages: `bg/bgNNN/bg.fbx.bin` + `bg.fbx.json` + DDS textures + `stage_{color,specular,bokashi_alpha}.img`.
Neither game has an HA6 or cg stage format. `object.txt` places PAni `.pat` objects:
- UNI2: bg003/nizi.pat, bg022/kuon.pat, bg024/bg24.pat, bg027/nizi.pat (extracted: bg003, bg022, bg024)
- MBTL: bg004/gyakko.pat, bg014, bg015, bg016, bg017, bg019, bg026, bg031, bg032 (extracted: bg004, bg015, bg019)
`bg/BgList.txt`, `bg/bg_chara_height.csv` and `bg/bg99Info.txt` are extracted for both games.

## 7. Round-trip results with the current build (`Hantei-chan/build/*.exe`)

| test | UNI2 | MBTL |
|---|---|---|
| `roundtrip.exe` (load, save, reload, field compare) | 115/115 pass, 0 unknown tags | 146/147 pass; **chr016.ha6: attack box 25 xy changes** (seq 6 frame 6, seq 25 frames 2-3) |
| saved bytes identical? | no: repeated AS blocks are re-emitted as ASSM refs (chr000: ASST 688 -> 256, ASSM 295 -> 727) | same, and the uninitialised bytes after the PTT2 name are zeroed |
| `pat_roundtrip.exe` | 64/64 pass | 71/71 pass |

Logs: `gamedata/_reports/{uni2,mbtl}_roundtrip.log` and `{uni2,mbtl}_pat_roundtrip.log`.

## 8. Re-extracting

```
cd /mnt/c/dev/hantei-chan/tools/gamedata
python3 uni2_extract.py list "<UNI2>/d" [--index NAME] [--filter REGEX]
python3 mbtl_extract.py list "<MBTL>"   [--filter REGEX]         # table read from MBTL.exe
python3 mbtl_extract.py extract "<MBTL>" OUT --filter '^data/chr016/'
```
Both scripts skip files that already exist with the same size. They only read the install and never write to it.
