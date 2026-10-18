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


<!-- PKG -->
