# CG manager: design (Tools > CG manager)

Goal: sprite-bank management better than cgtool (`docs/cg/cgtool_reference.md`) for MBAACC/MBAC first, then
every bank format Hantei-chan reads. Four features: (1) browse/search with usage, (2) batch export/import +
CLI, (3) palette tools, (4) add/remove/reorder with reference fix-up. Undo/redo for all of it.

## 1. The bank format (MBAACC `.cg`, "BMP Cutter3/2"), as verified over all 72 files of `mbaacc/data`

```
0x0000  "BMP Cutter3\0..." (16) + dword (1)
0x0014  8 palettes x 256 x BGRA (only #0 is used by the engine path Hantei mirrors)
0x2014  H[12] dwords: H0 = pages-1, H2 = nAlign, H3 = nImages, H4 = cell unit (16, 32, 8), others 0
0x2044  image offset table, 3000 dwords (0xFFFFFFFF = absent)
0x4f24  align-table offset | 0 | total file size           (3 dwords, "tail")
0x4f30  image 0 ... image n-1, then the alignment table (24 bytes per block) to EOF
image   72-byte header: name[32] type w h bpp x1 y1 x2 y2 alignStart alignLen  + data blob
align   dstX dstY w h (int32) srcX srcY srcPage copyFlag (int16)
```
Verified invariants: blobs are contiguous in image order (the span between headers IS the blob; type -1 images
carry a 4-byte blob), data end == align-table offset, file size == alignOff + 24*nAlign, tail dwords as above.
Blob contents: type 0 = 8-bit indices of bank palette (blocks w*h each), type 1 = BGRA, type 2 = 1024-byte palette + indices,
type 3 = one dword colour + alpha plane, type 4 = palette + indices + alpha. A block with `copyFlag != 0` owns no
data: it points at cells (page, srcX, srcY) owned by ANOTHER image (pixel dedupe done by the original packer).
Measured on akiha.cg: 2137 copy blocks, each pointing into exactly one owner image, no owned cell overlaps.
Consequence: **pixels of image A can be drawn by image B**; replace/delete must know about it.
The "atlas" is the (page, 16px cell) grid: pages 256x256, `pages = H0+1`; `CG::build_image_table` mirrors it.

## 2. Architecture (all new code in `src/cgm/`, no GL/ImGui in the core so the CLI and tests link it)

| unit | job |
|---|---|
| `cgm_bank.h/.cpp` | `CgmBank`: parse a bank into a model, `serialize()` byte-exact. Model = header fields + palettes + `vector<Image>`; each `Image` = header, `vector<Block>`, blob as `shared_ptr<const vector<uint8_t>>` (snapshots are O(images), unchanged blobs are shared). Unknown/odd banks fall back to "opaque mode" (replace only) and the suite counts them. |
| `cgm_ops.cpp` | operations: replacePixels (reuses the verified encoders of `CG::replace_image_rgba` logic), add, remove, move/renumber, unshare (give a copy block's dependants their own data), atlas allocation (first free cells, new page when full), compaction ("repack") |
| `cgm_usage.h/.cpp` | reverse index image id -> (pattern, frame, layer) from `FrameData` (AFGP/AFGX `Layer::spriteId`, `usePat` layers excluded) plus the cell-sharing graph (owner/dependants). Other formats register their own reference walkers (HA4 parts, PAT parts, FOB). |
| `cgm_export.h/.cpp` | folder export/import: `manifest.json` (nlohmann, already in `third_party/json`), `idx/NNNN_name.png` (8-bit indexed, bank palette embedded), `rgba/NNNN_name.png`, `palettes/*.pal`; SHA-256-less FNV-1a 64 hash per image over the decoded RGBA, stored in the manifest; import re-encodes only images whose hash changed |
| `cgm_palette.h/.cpp` | palette model + file I/O (.pal MBAACC, .pal UNI, RIFF .pal, .act, GIMP .gpl, PNG strip), edit ops (set, range fill, gradient, hue/sat/val shift, swap/recolour whole bank by map), PUPS banks |
| `cgm_undo.h/.cpp` | manager-level undo stack: entries = {label, bank snapshot before/after (shared), pattern-reference remap (old<->new id map), palette snapshot}; character undo (`UndoManager`) is told via `markModified` so frame edits from fix-up are one step with the bank change |
| `src/ui/cgm_window_impl.h` | the ImGui window(s) (browser grid, inspector, palette editor, ops dialogs) |
| `src/cgm/cgmtool.cpp` | CLI `cgmtool.exe` (info, export, import, check, pack, palette ops) |

Integration with `CG`: the renderer keeps using `CG` (reads raw bytes). After an op the manager serializes the model and
calls `CG::restore_bank`/`loadFromMemory` (24 MB memcpy, acceptable per op; per-image pixel edits patch in place via
`replace_image_rgba` and update the model blob). `CG::generation()` already invalidates the render cache.
Foreign banks (MB/PB2K1/QoH/RBO CHP...) are exposed through `CgmBankAdapter` (the existing `CgForeignBank` plus
a `capabilities()` mask: browse / replace / palette / structural), so the browser, export/import and palette tools work
on all of them and structural edits are only offered when the format supports them.

## 3. Features

**Browse/search (M2).** Thumbnail grid (lazy GL texture cache, LRU), per tile: id, name, WxH, type/format, bpp, palette
used, usage count, shared-cell flag. Filters: unused, used-by-pattern N, size range, type, name substring/regex.
Preview with zoom + palette selector (live: palette number and PUPS bank). "Usage" = patterns/frames/layers
referencing the id; click to jump to the pattern/frame in the main view. Reverse index is rebuilt on edit (generation stamp).

**Batch export/import (M3).** Export writes manifest + indexed PNG + RGBA PNG + palettes. Import is manifest driven:
decode each changed PNG (hash differs from manifest), same-size check (or size change allowed via re-bounds when not shared),
re-encode only those images (type kept), everything else keeps its stored bytes => unchanged export->import is byte-exact and an edit changes only that
image's blob (+ nothing in the index when the size is unchanged; a size change moves following offsets, so edit-locality for
the suite is defined as "only the changed images' blobs and the offset/tail words differ"). `--layout cgtool` writes
`<name>_ID_<n>.png` canvas-size PNGs for tool compatibility; indexed PNG import without a manifest maps by id.

**Palette tools (M4).** Bank palette (0x14, 8 slots), `.pal` files (MBAACC count + n x 256 BGRA; UNI header variant),
PUPS banks `_pN.pal`, per-character colour variants (palette number = colour). Editor: per colour (RGB/HSV, hex), ranges,
gradients between two selected colours, copy/paste ranges, swap palettes, "recolour" = hue shift / map applied to one or all
palettes and optionally to the type 2/4 embedded palettes. Import/export: `.pal`, `.act`, GIMP `.gpl`, PNG strip (16x16 or 256x1).
Live preview: palette edits update the CG palette pointer (generation bump) and show in the main view and browser at once.

**Add/remove/reorder (M5).** Add: PNG (or new empty) -> new image appended (or inserted at id, shifting) with cells allocated
in free atlas cells, type chosen (8-bit if <=255 colours and bank palette matches, else 2/4). Remove: impact report first
(patterns/frames/layers using it, dependants sharing its cells); refuse when used unless "clear references" (set spriteId -1)
or "unshare then delete" is picked; unused + not shared deletes freely. Reorder/renumber: permutation applied to the bank, then
`Layer::spriteId` of every pattern is remapped (with `usePat` layers skipped), all as ONE undo step. Repack: drops absent slots,
compacts ids (the same remap), recompacts atlas pages. External references (game code to `csel_*.cg`, `effect.cg`, other
characters' effect spawns) cannot be seen from a character: the dialog warns for non-character banks.

**Undo/redo.** Every op is an entry in `cgm_undo` (snapshot sharing keeps memory small); Ctrl+Z/Y inside the window and
Edit menu hooks when the window is focused; the existing `g_cgUndo` whole-bank snapshot logic of the RBO window is replaced.

## 4. Quality gates

* `cgmtool check <banks...>`: parse -> serialize == input for every bank (suite section "cgm" in `tools/han2/run_roundtrip.sh`-style
  runner `tools/cg/run_cgm_suite.sh`: MBAACC `data/*.cg`, MBAC DAT CG blobs, GOF2/RBO CHP+CG area via han2tool, foreign banks).
* export-all -> import unchanged -> identical bytes (every bank); edit-locality check (one image changed -> blob of that image
  + offsets/tail only); add -> remove round trip == original; renumber permutation -> inverse permutation == original,
  frame references too.
* i18n: all strings in EN + JP through the i18n system, `python3 tools/i18n_check.py` must pass.
* In-game proof (MBAACC, hard-linked files, windowed): recoloured palette + replaced sprite visible, `pc_shot.sh`.

## 5. Milestones

M1 docs (this) -> M2 `cgm_bank` + usage + browser -> M3 export/import + CLI (+suite) -> M4 palette tools -> M5 structural ops + fix-up (+suite)
-> M6 adapters for HA4 CG, RBO/GOF2 CHP/CG, MB/PB2K1 strips, EX3, QoH.

## 6. Status (as built) and the command line

Built in `src/cgm/` (core, no GL/ImGui) + `src/ui`-free window `cgm_window.cpp` (Tools > CG manager):

| file | role |
|---|---|
| `cgm_bank` | byte-exact model of a BMP Cutter bank (parse/serialize, decode, atlas, owners/dependants); keeps hidden images past the declared count |
| `cgm_ops` | `ReplaceImage`: types 0/1/2/3/4, keeps stored indices and palette slots of unchanged pixels (edit locality) |
| `cgm_struct` | add / insert / delete / move / permute / clear / unshare / duplicate, atlas allocation |
| `cgm_export`, `cgm_io` | folder export/import over `BankIO` (BMP Cutter model, or any bank through the CG object) |
| `cgm_palette` | `.pal` sets, colour ops, whole-bank recolour, `.act`/`.gpl`/`.png`/`.pal` files |
| `cgm_usage` | usage index image -> patterns/frames/layers; `RemapSprites` reference fix-up (+ restore for undo) |
| `cgm_undo` | step history (shared-blob snapshots) |

Folder layout written by Export all: `manifest.json` (per image: id, name, type, bpp, canvas, bounds, blocks, owners/dependants, FNV-1a hash of the decoded RGBA,
file names; bank-level: `indexPaletteHash`), `rgba/NNNN_name.png`, `indexed/NNNN_name.png` (types 0 and 2: 8-bit PNG with the bank/own palette and tRNS),
`palettes/bank_slotN.pal`. Import re-encodes only images whose pixels differ from the recorded hash (indexed PNG first: exact indices when it still uses the exported palette).

`cgmtool.exe` (built by `build.sh`):

```
cgmtool check <bank.cg>...                 parse -> serialize byte-exact, decode == CG::draw_texture
cgmtool info <bank.cg>
cgmtool export <bank.cg> <dir> [--no-rgba] [--no-indexed] [--cgtool]    (--cgtool: <name>_ID_<n>.png canvas-sized, no manifest)
cgmtool import <bank.cg> <dir> -o <out.cg>                              (writes a NEW file; the input is never overwritten)
cgmtool roundtrip <bank.cg>... [--tmp dir] export all -> import unchanged -> identical; edit-locality (one image changed -> only its blob changes)
cgmtool struct-check <bank.cg>...          add/insert/delete/move/permute/clear/unshare/duplicate proofs, engine loader agrees
cgmtool refs-check <char.HA6> <bank.cg>    frame references still show the same pictures after permute/insert/delete; undo restores them
cgmtool pal-info|pal-export|pal-import|pal-recolor|pal-check ...
cgmtool recolor-bank <bank.cg> --hue D --sat M --val M -o <out.cg>
```
Suites: `tools/cg/run_cgm_suite.sh` (MBAACC `data/*.cg` and `*.pal`: sections cgm-bank, cgm-roundtrip, cgm-palette, cgm-struct, cgm-refs) and
`tools/han2/run_roundtrip.sh` section `cgm` (every RBO / GOF2 CG area and CHP: model parse/serialize, decode == CG, re-import keeps bytes).

## 7. In-game proof (MBAACC, 2026-10-03)

A hard-linked copy of the install (`cp -al`, game dir untouched) with `data/sion.cg` and `data/sion.pal` written as NEW files by `cgmtool`
(`export` -> edit 500 PNGs with a stripe -> `import -o`; `pal-recolor --hue 120 --palette 0`), run windowed through PovertyCaster
(`pc_inject.exe MBAA.exe pchost.dll`, `PCHOST_AI_INPUT=1` = CPU vs CPU), screenshots by `tools/pc_shot.sh`, process killed by PID.
`docs/cg/evidence/ingame_before_sion_vs_tatari.png` (stock) vs `ingame_after_recolour_and_replaced_sprites.png`: Sion is green (palette 0 recoloured)
and carries the injected stripe on every replaced sprite. The match picks Sion vs Tatari (no character knob), so Sion stands in for Akiha; the Akiha
files (`akiha.cg` 12 replaced images, `akiha.pal`) were produced the same way.

## 8. Format coverage (M6) and limits

| format | browse / usage | export / import | palettes | add / remove / reorder |
|---|---|---|---|---|
| MBAACC `.cg` (+ `.pal`, PUPS) | yes | yes (byte-exact unchanged) | yes (file + bank slots) | yes, frame references fixed up |
| MBAC `.DAT` (HA4, embedded CG) | yes | yes | yes | yes; the embedded blob is rewritten on save |
| RBO / GOF2 CG area and `.CHP` | yes | yes | bank slots | yes; the CG area is rewritten at its new size on save (HAN2RBO) |
| MB / PB2K1 strip banks, QoH tiles (foreign) | yes | yes (via the CG object) | view only | no (fixed layout); replace-image only, no undo here |
| storage type 5 (index + alpha planes, slot palette) | read / view / byte-exact | decode only | - | refused for authoring (user decision: recolour is runtime-only) |

Known limits: references from outside the character (menu code using `csel_*.cg`, other characters' effect spawns) are not visible to the usage index;
the character's pattern undo and the CG manager's undo are separate stacks (undo a structural CG step from the CG manager window).
