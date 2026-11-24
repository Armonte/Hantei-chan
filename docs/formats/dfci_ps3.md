# DFCI PS3 asset formats

Comparison of the PS3 release (BLJM-61322) UI data `grpdat_ps3/` against the
arcade/PC (`RingGame.exe`, SDCS distribution) `grpdat/`. Game data is never
copied into the repo; regenerate everything below with:

```
python3 -I tools/dfci_ps3/inventory.py \
  --arcade ".../Fighting Climax Ignition [SDCS]/grpdat" \
  --ps3    ".../Fighting Climax Ignition [SDCS]/grpdat_ps3" [--json inv.json]
```

Paths are matched case-insensitively after stripping a trailing `.gz`
(so `foo.dds.gz` pairs with `foo.dds`).

## Inventory

### Totals

| | files |
|---|---|
| arcade `grpdat/` | 2400 |
| PS3 `grpdat_ps3/` | 3806 |
| common logical paths | 1698 |
| only in arcade | 702 |
| only in PS3 | 2101 |

**Only in PS3 (2101)** by top dir: Special 887, Customize 756, enemyselect 105,
CSel 94, VsScreen 74, MainMenuCSTGS 36, MainMenuCS 28, Title 27, Winner 26,
Challenger 20, then single digits (BattleResult, Network, Ranking, exstorysel,
Announce, System, Continue, InstallDataIcon, SaveDataIcon, plate, csstory,
BootLogo, Cockpit, Font, cutinBG, icon). By type: dds 1989, csv 75, pat 31,
PNG 5, txt 1.

- `Special/` (gallery: Illustration, Movie, Music, Novel, Replay) is a
  console-only mode with no arcade counterpart.
- `MainMenuCSTGS/`, `InstallDataIcon/`, `SaveDataIcon/` are PS3-only dirs
  (TGS menu variant, XMB install/save icons — the 5 PNGs live here).
- The 75 extra CSVs and most extra Customize/CSel/VsScreen/enemyselect
  textures belong to console modes/unlockables.

**Only in arcade (702)** by top dir: Customize 579, Ranking 104, autograph 6,
CharaColorBar 4, sys_card 4, Challenger 2, command 2, BootLogo 1. By type:
dds 694, txt 6, pat 2. `Ranking/` and `sys_card/` are arcade network/card
features; the extra `Customize/cus_chacolor2`, `cus_platesam` etc. and the
extra CharaColorBar entries come from the SDCS custom-palette mod
(`DFCI_custom_palettes_credits.txt`), not from the original arcade ROM.

### Per extension (common paths)

| ext | PS3 wrapper | result | count |
|---|---|---|---|
| dds | gzip | inner DDS byte-identical | 672 |
| dds | gzip | inner DDS different size/dims | 547 |
| dds | gzip | same size, header flags differ only | 26 |
| dds | gzip | same header, pixels differ | 12 |
| dds | gzip | same size, mip count differs | 2 |
| dds | none | identical (uncompressed on both sides) | 1 |
| pat | none | **PANIBin (PS3) vs PAniDataFile (arcade)** | 327 |
| csv | none | identical | 19 |
| txt | none | identical | 43 |
| txt | none | content differs | 47 |
| fnt | none | identical | 1 |
| jpg | none | identical | 1 |

Notes:

- **No byte-swapping anywhere** in DDS/CSV/TXT. Every PS3 `.dds` except 8 is
  stored as plain gzip (`1f 8b`, 3248 `.dds.gz`); inside is a standard
  little-endian DDS. Only `.pat` is a different (big-endian binary) format.
- The 547 dims-differ DDS are all `Customize/cus_chacolor1/color_samp_*`:
  arcade (SDCS) has 512x512 swatch sheets, PS3 has 224x448 — the SDCS palette
  mod enlarged them. Use the arcade copy.
- The 26 header-only differences (24 in `icon/`) differ in `dwFlags`
  (`0x1007`/`0x100F` vs PS3 `0x81007`, i.e. LINEARSIZE set) and the
  pitch/linear-size field; pixel data is identical. Harmless to the loader.
- The 2 mip-differ files (`Ranking/DIAGRAMfaceA.dds`, `System/APM_NetworkIcon.dds`):
  arcade has `dwMipMapCount=1`, PS3 has 0. Equivalent.
- The 12 same-header/pixel-differ files (plate x4, exstorysel x2, BattleResult,
  CSel, Customize, MainMenuCS, Network, Ranking) are genuine art revisions.
- All 327 shared pats differ in container format (see PANIBin). 2 pats exist
  only in arcade (`Challenger/callenge_cha/challe_ako.pat`, `challe_yuk.pat`);
  31 only in PS3.

### CSV / TXT

- Encoding is the same on both sides for every shared file: 76 cp932
  (Shift-JIS) CRLF, 28 ASCII CRLF, 4 ASCII mixed EOL, 1 UTF-8 CRLF. No BOMs.
  No re-encoding needed.
- All 19 shared CSVs are byte-identical.
- 62/109 text files identical after decoding. All 47 differing files are
  `CharaColorBar/cb_*.txt`: the arcade (SDCS) copy has 40 more lines (4 extra
  palette entries `{ // 24 ... }`) and/or numbered `name = "1"`… where PS3 has
  `name = ""`. These are SDCS-mod edits, so the arcade copy is the superset.

### What the arcade build needs to use PS3 grpdat

| asset | action |
|---|---|
| `*.dds.gz` | gunzip; result is a drop-in DDS |
| `*.pat` (PANIBin) | convert to PAniDataFile (`tools/dfci_ps3/panibin2pat.py`) unless RingGame reads PANIBin |
| csv / txt / fnt / jpg | use as-is (same encoding) |
| PNG (Install/SaveDataIcon) | XMB-only, not used by the arcade build |

## PANIBin (PS3 .pat)

Big-endian, offset-table "compiled" form of PAniDataFile. All 358 PS3 pats share the header bytes `01 00 12 34`.

| off | field |
|---|---|
| 0x00 | `PANIBin\0` |
| 0x08 | u8 1, u8 0, u16 0x1234 (byte-order mark) |
| 0x0C | u32 file size - 0x10 |
| 0x10 | 8 x (u32 offset, u32 count); offsets are relative to 0x50 |

| table | record | arcade equivalent |
|---|---|---|
| 0 | 32 B texture name, NUL-padded | PGST/PGNM (no PGTE, no pixel data: texture is the external `.dds.gz`) |
| 1 | i32 cutout id -> index, -1 = none | PPST ids |
| 2 | 84 B cutout: name[32], PPTP, PPTE(u32), PPUV x,y,w,h, PPSS w,h, PPCC x,y, PPPP, PPJP x,y | PPST |
| 3 | i32 part set id -> index | P_ST ids |
| 4 | 56 B part: PRID, PRPR, PRXY x,y, u32 @16 (unknown, 0 in all samples so far), PRZM f32 x2, PRA3 f32 x4, PRCL, PRSP, u8 PRRV, u8 PRAL, u8 PRFL, u8 pad | PRST |
| 5 | 232 B part set: name[32] (PANA), i32 slots[40] (PRST id -> part index), u8 draw order[40] (derived from PRPR, 0xFF-filled) | P_ST |
| 6 | u32[1000] (unknown; values 0..shapes-1) | none |
| 7 | 64 B shape: 16 dwords = VEST params | VEST (no VNST names) |

RingGame.exe (arcade) has no PANIBin loader (see the IDB note in the PKG section), so the PS3 pats have to be converted.

### Converter: tools/dfci_ps3/panibin2pat.py

`panibin2pat.py in.pat out.pat` writes a PAniDataFile with Hantei-chan tag order (PRXY PRAL PRRV PRFL PRZM PRCL PRSP PRA3 PRPR PRID; PPNA PPCC PPUV PPSS PPTP PPTE PPJP PPPP), and only writes a tag when its value is not the default.
`verify_panibin.py ARCADE PS3` converts all 327 shared pats and compares each record with the arcade twin.

Result (shared paths, ignoring PGST):

| record | byte-identical | differ |
|---|---|---|
| P_ST | 2451 | 11211 |
| PPST | 2645 | 13489 |
| VEST (params only) | 313 | 14 |

Only 1 of the 327 files is fully byte-identical. The differences are mostly content, not converter bugs:
- The PS3 compiler **drops parts whose PRID points to a cutout that doesn't exist** (e.g. `PRID 999` placeholders in every arcade `challe_*.pat`), and **drops unused cutouts** (e.g. csel_bg00 PPST 80). These can't be recovered from PANIBin.
- **PPPP**: some arcade cutouts have PPPP 1/2 where the PS3 field is 0 (csel_bg00 PPST 69/70). Either it isn't stored or the PS3 data differs; not resolved.
- **Tag presence**: PANIBin stores every field, so a tag the arcade file wrote with its default value is omitted on output. This changes bytes but not the values Hantei-chan loads.
- **PGST**: arcade embeds LZ-compressed PGT2 texture data and leaves garbage after the PGNM NUL. Output writes PGNM only (zero-padded), so the texture has to come from the gunzipped `.dds` next to the pat.
- **VNST** shape names don't exist in PANIBin, so they are written as empty.
- Many PS3 pats are different revisions of the arcade file (e.g. round_d00 has 2590 parts on PS3 and 3291 on arcade), so a byte-identical result isn't possible for them.

Open items: what the u32 at part+16 and table 6 mean, PPPP, and a value-level comparison (parse both sides into models) to separate converter errors from content changes.


## PS3 package

### What is installed

- RPCS3 has **no installed copy** of BLJM61322. `dev_hdd0/game/` holds only
  NPEB00563, NPJB00076, NPUB30505 and TEST12345, and there is no `dev_hdd0/disc`.
  `cache/BLJM61322/` holds only a PPU cache of EBOOT.BIN.
- There is no disc folder, so the base-game disc data is **not available**.

### The pkg file

The file is `BLJM-61322 (install this pkg).pkg`, 174,834,240 bytes.

| field | value |
|---|---|
| magic | `7F 50 4B 47` |
| pkg_revision | `0x0000`, a **debug** pkg (not encrypted with a retail key) |
| pkg_type | 1 (PS3) |
| item count | 1688 |
| data offset / size | `0x1B0` / `0x0A6BC030` |
| content id | `2PATCH-BLJM61322_00-DFCIGNITION00001` |
| `USRDIR/PatchVersion.txt` | `1.01` |

- The pkg decrypts with the public debug key stream: SHA1 over the QA digest at 0x60, plus a 64-bit block counter.
- `tools/dfci_ps3/pkg_list.py` lists the items and, with `--extract DIR`, extracts them. Extract only into a scratch dir.
- The pkg is a **game-data / update patch**, not the full game. It holds a signed `USRDIR/EBOOT.BIN` (SCE header). The content id and the text show it is a community English patch built on the 1.01 update.
- Only one item is NPD-encrypted: `USRDIR/cli-exe-data00.edat` (320 bytes, an NPD license stub). **All game files are plain once the pkg is decrypted.** No klicensee is needed.

### Contents, compared with the arcade/SDCS install

Counts exclude directories.

| PS3 path | count | format | compared with arcade |
|---|---|---|---|
| `PackFile/data/<Chr>.pac.gz` | 39 | gzip around **UKArc** (see below), one per character `data/<Chr>_0/` | see the per-format table |
| `PackFile/se/battle_se` | 1 | UKArc | not checked |
| `data/` (loose) | 25 | HA6/LST/cg/pal/pat/txt | `BaseData.HA6`, `effect_s.HA6`, `*.LST`, `_csel/*.cg` are identical. `effect.HA6`, `_csel/*.pal` and `sys_effect.pat` (PANIBin) differ. 8 `*_se_list.txt` exist only on PS3 |
| `grpdat/` | 698 | dds.gz / pat / csv / txt | 364 dds.gz and 21 pat are identical to `grpdat_ps3`. **146 dds.gz and 25 pat differ** (1.01 updates of CSel, Challenger and others). 22 dds.gz and 2 pat exist only here (`MainMenuCS/main_chara/main_018yuk`, `main_020ako`) |
| `script/` | 7 | Squirrel txt | 2 identical. 5 differ: the PS3 copies have no `debug_HitMuteki*` hitbox-viewer code, and `btl_MvFunc` / `btl_StdMoveTbl` are the 1.01 versions |
| `System/` | 29 | csv/ini/txt | identical except `str_jp/PadSet.csv` (PS3 pad layout) |
| `BattleRes/` | 228 | txt/csv, `st_face/*.psb.gz` | 113 txt identical, 111 txt differ (only the English translation wording), 2 `st_face_00{18,20}.psb.gz` exist only on PS3. The arcade build uses plain `.psb`; gunzip gives the M2 `PSB\0` v3 |
| `se/` | 502 | `.vag` = **MSF** (`MSFC`, codec 3 = PS-ADPCM), 193 files; `.msf` = MSF codec 7 = MP3 (voices), 309 files | the arcade build uses RIFF WAV PCM16 (8,497 files). None match |
| `bg/BgList_str.txt` | 1 | txt | identical |

### UKArc (`.pac`, `cgarc.uka`)

All fields are big-endian. The `0x1234` byte-order marker is the same one PANIBin uses.

```
0x00 char[8] "UKArc\0\0\0"
0x08 u16 version (=2)   u16 bom (=0x1234)   u32 count
0x10 entry[count], 0x4C bytes each:
       char name[0x40]   u32 size   u32 unk (hash?)   u32 offset   ; data at 0x10+offset, stored uncompressed
```

`tools/dfci_ps3/ukarc.py PACK[.gz] [--extract DIR] [--compare GAMEROOT]` reads this format.

### Per-format result inside the 39 character packs

| ext | identical | differ | PS3 only | notes |
|---|---|---|---|---|
| HA6 | 77 | 2 (Oni.HA6, Oni `_temp.HA6`) | - | Same `Hantei6DataFile`. Oni comes from 1.01, so the content differs but the format does not |
| LST | 84 | 3 (Oni) | - | same format |
| txt | 285 | 3 (`Fro_s/Mou_s/Oni _mv.txt`) | - | content only (1.01) |
| cg | 0 | **39** | - | PS3 `BmpCutBin\0`, BE, BOM 0x1234, about 100 KB. It keeps the palette and cut table, but **the pixel data is removed** and moved to `cgarc.uka` |
| uka | - | - | 39 | `cgarc.uka` is a UKArc of `<Chr>NNN_NN.dds.gz`. Each is a DDS **L8** (8-bit luminance = palette index) per BMP cut |
| pal | 0 | 41 | - | Same size (65,540). The PS3 file is the original. The SDCS arcade pals carry **custom palettes** from slot 24 on (`DFCI_custom_palettes_credits.txt`). Keep the arcade pals |
| pat | 0 | 39 | - | PS3 `<Chr>_ef00.pat` is **PANIBin** (as in grpdat_ps3). Arcade is PAniDataFile |
| dds | - | - | 65 | `*_cut.dds`, `rendering*.dds`, `*_potential.dds`, `*_ef0NN.dds`: textures external to the PANIBin effect pats (in arcade they are embedded in the PAniDataFile `PGST` blocks) |

### What each format needs to load in the arcade build (RingGame.exe)

- **HA6 / LST / txt / script / csv / ini:** no conversion. Copy them over to take the 1.01 balance changes. The `script/` copies lose the hitbox-viewer debug helpers.
- **cg:** convert to arcade `BMP Cutter3`. Take the cut table and palette from `BmpCutBin` (byteswap it), and put the L8 DDS pixel data from `cgarc.uka` back inline. This is a lossless repack. **It is not needed for the 39 characters**, because their arcade `.cg` files exist and the HA6 files are identical.
- **pal:** keep the arcade files. The PS3 copies lack the SDCS custom palettes.
- **pat (PANIBin) + external dds:** convert with `panibin2pat.py` and embed the referenced `.dds` files as PGST texture blocks. RingGame.exe's pat loader is `dfci_PAniDataFile_Load` @0x76D000. It accepts only `PAniDataFile`, through `dfci_PAniDataFile_CheckMagic` @0x769940.
- **dds.gz:** gunzip it. The DDS inside is byte-identical to the arcade DDS. The executable has no gzip path for assets: zlib is used only by amCts networking (`amCts_zlib_Compress/Decompress`) and by the emote device model loader (`%s.gz`).
- **psb.gz:** gunzip it to `.psb`.
- **se `.vag`/`.msf`:** MSF decode (PS-ADPCM / MP3) to PCM16 WAV, for example with vgmstream. Only needed if PS3-only sounds are wanted; the arcade build has a full WAV set.
- **EBOOT.BIN / edat:** irrelevant to the arcade build.

