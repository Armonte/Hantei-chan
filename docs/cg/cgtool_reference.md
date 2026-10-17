# cgtool reference ("cgrepack"): what the community tool does, exactly

Source of this analysis: `hantei_forks/faith_hantei/tools/cgtool/` (`cgtool.exe`, `palettehelper.exe`,
`GamePaletteMethods.dll`, `MiscUtil.dll`), decompiled with ILSpy (`ilspycmd`, .NET 6+ runtime via
`DOTNET_ROLL_FORWARD=Major`). Hantei-chan's CG manager reimplements the *behaviour* (credit: the cgtool /
palettehelper authors); no code was copied. Facts below were read from the decompiled code, not guessed.

## 1. Command line

```
cgtool -input <X> -output <Y> [-pal <file>] [-spritesdir <dir>] [-ps3]
```

| input | meaning |
|---|---|
| `X.cg` / `X.dat` | **extract**: every image -> PNG in folder `Y` |
| a folder, or a `.txt` list of folders/files | **make**: build a new `.cg` file `Y` from images (PNG, BMP or DDS) |

`cgtool_extract_cg.bat` = `-input f.cg -output f` (folder next to the file). `cgtool_make_cg.bat` =
`-input folder -output folder.cg`. `-pal` loads a single 256-colour palette (see 4.) used as the palette
of the extraction / of the new bank. `-ps3` writes the big-endian console flavour (`BmpCutBin`), out of scope here.

## 2. Folder layout and metadata

* One PNG per image, named `<stored name>_ID_<index>.png`, e.g. `aki00_000.bmp_ID_0.png`
  (the stored 32-byte name keeps its `.bmp`).
* **There is no metadata file at all.** Type, bounds, offsets, palette choice, copy blocks and cell layout are
  not recorded anywhere.
* Extraction decodes the bank to the full *canvas* (the header width x height, e.g. 256x256), not the
  tight bounds; bounds are rediscovered on re-make by cropping empty 32x32 tiles.
* 8-bit images (type 0): a GDI+ bitmap is re-indexed with the bank palette (palette 0 of the bank, or `-pal`)
  and saved as an 8-bit PNG. Alpha is not represented (index 0 = black colour entry, alpha 255).
* 32-bit images (types 1, 2, 3, 4): saved as ARGB PNG. Type 4 images are saved with alpha forced to 255
  (the alpha plane is lost).
* Images with an empty/zero-sized area are not written (`getSprite` returns null), so file indices after a
  hole do not match image ids. Re-make numbers images by the *file order*, not by the `_ID_n` in the name.

## 3. Make (folder -> .cg): what is generated

Files are sorted with an alphanumeric natural comparer (`AlphanumComparatorFast`) and become images 0..n-1.

For every file: load as BMP / PNG / DDS (PNG: PLTE + tRNS handled by hand; indexed PNG -> raw indices,
other -> 4 bytes/pixel BGRA from GDI+). Then:

1. the picture is cut into **32x32 tiles**; completely empty tiles (all index 0 / alpha 0) are dropped;
2. each kept tile becomes one alignment block (24 bytes: dst x/y, 32x32, source x/y/page);
   source slots fill an 8-tile-high column, then next column; a new "page" every 64 tiles;
   pages run sequentially over the whole bank (`srcImg` keeps counting across images);
3. image header: type = 0 for 8-bit input, **1 for any 32-bit input** (types 2/3/4 are never produced), bpp 8 or 32,
   width/height = file size, bounds from the first/last non-empty tile (tile-granular, +32 padding
   on the far side), tiles_offset/count;
4. file header: `BMP Cutter3`, palette 0 = the `-pal` palette (or FF-fill if none), palettes 1..7 FF-filled,
   `filedata[0]` = number of pages, `[2]` = total tiles, `[3]` = image count, `[4]` = 32 (cell unit),
   offsets table for `n` images, tile table, no copy blocks, no sharing, no size/total fields (`fsize` = 0).

## 4. Palettes (palettehelper)

`palettehelper -input <pal|png|list.txt> -basepal <UNI .pal> -output <o> -mode <unist|uniel|melty|nitroplus>`
operates on **fighting-game `.pal` files** (UNIST / UNI-EL / Melty / Nitro+ layouts; left/right palette
lists, `[from,len,to]` range-merge directives from a text list, per-character side fixes, save-file editing).
Palette sources understood by `PalMethods.loadpalette`: raw BGRA 256x4 at offset 0, RIFF `.pal` (24-byte
offset), `.act`-like 148-byte header files, or PNG PLTE. cgtool itself only takes **one** palette (index 0).

## 5. What it preserves and what it normalizes

| aspect | cgtool |
|---|---|
| image order | extraction skips empty images (shifts ids); make re-numbers by file order |
| storage types | everything becomes type 0 (8-bit) or 1 (32-bit); 2/3/4 compression is lost |
| bounds / offsets | recomputed at 32 px granularity |
| cell layout / copy blocks | discarded: no dedupe, every non-empty tile stored, new atlas pages |
| palettes 1..7, palette order | dropped (FF fill); only palette 0 survives |
| header unknowns, tail fields | rewritten with defaults |
| alpha of type 4 | lost on extract |
| unchanged round trip | **not byte-exact** (every file changes; sizes grow because types 2/3/4 are re-expanded) |
| edit locality | none: the whole bank is regenerated |
| frame references | not touched; works only because image order is kept by file order |

## 6. What the CG manager does differently

Byte-exact load -> save; per-image edit locality; metadata manifest (type, bounds, palette, hash, cell layout);
palette-preserving indexed PNG *and* RGBA export; hash-based change detection on import; keeps types 2/3/4 and
copy blocks; fixes frame references on add/remove/reorder. Where useful it also offers a cgtool-compatible
folder (`<name>_ID_<n>.png`, canvas-sized) via `--layout cgtool`.
