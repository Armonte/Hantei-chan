Hantei-chan 0.1: the 2026-10 update, merged as #84 (`a615b56`). The full change list, evidence and per-issue status are in #84.

## Download

- `Hantei-chan-0.1-windows-x64.zip`: the editor (`gonptechan.exe`), `config/` and `res/`, including the Squirrel API tables in `res/squirrel/`. Unzip it and run `gonptechan.exe`. Use this one.
- `gonptechan_0.1.exe`: the bare exe, for updating an existing install. Squirrel hover and checks need `res/squirrel/` next to it.

## Highlights

- **Games:** MBAACC, ActCadenza (moon 3), UNIB, UNI2, MBTL, DFCI and RBO/GOF2. HA6, .pat and .pal round-trip byte-exact (297/297 HA6, 197/197 .pat, 290 .pal).
- **Rendering:** .pat and sprite rendering is measured against UNI2 and MBTL game captures. Every test frame is at its measured floor, and it is the closest of the editors we measured on all seven showcase frames.
- **CG manager:** insert, delete, move and replace sprites with reference remap.
  - A unified CG + .pat texture browser with PNG import/export, "Used by", undo, and Save to mod folder (UNI2-IM / MBTL-IM).
  - RBO/GOF2 part sheets, and UNIB DXT3 textures.
  - Palettes with a live sprite preview, UNI2 palette pairs and new `_pN.pal` files.
  - MBAACC effect recolour authoring for PovertyCaster, following DGV's "Better Akiha" method.
- **UNI2/MBTL/UNIB data:** cross-game paste conversion, layer tools, 10 condition parameters, the BaseData template banner, per-game effect/condition labels and spawn placement.
- **Squirrel script editor** for UNI2, MBTL, DFCI and UNIB: hover with the game's native functions, outline, checks, and links from scripts to HA6.
- **Hot reload** into a running UNI2/MBTL training session through fpan-im (formerly fbmod). It covers characters, the shared `script/` files, VectorTable, BattleInfo and BoundEff, in rebuild or live mode.
- **3D stage view** for UNI2, MBTL, DFCI and UNIB, with an MBAACC alignment mode (`stage3d.ini` for PovertyCaster) and Open in / Reload from Blender 5.2.2.
- **Mod projects** with conflict checks and PovertyCaster `.pcmod` export.
- **ActCadenza:** converter fixes and moon-3 data tools.
- **UI:** full English and Japanese, with layouts that scale with the font and DPI.
- **Build:** imgui 1.93 docking. CI builds Windows and a MinGW cross-compile and runs ctest.

## Known gaps

- **F21:** hit-effect and hitstop lists are still MBAACC's for every game.
- **D17:** there is no preview of the merged template pattern.
- **Stage shadows** are not drawn in the editor.
- **No in-game proof:**
  - RBO;
  - a new UNI2 `_pN.pal`;
  - the PovertyCaster effect-recolour runtime (queued).
- **Untested:** UNIEL with real stage data; mouse-driven UI by hand.
- **Undo:** foreign CG banks (MB/QoH) have no undo for replace. "New PUPS bank" and the PovertyCaster palette export write files with no undo step.
- **DXT3:** a standalone DXT3 `.dds` import is refused.
- **Licence:** none chosen yet.

## Checksums (SHA-256)

```
35fe11ae95f18cef9576c698493cd94f18c2ccd1219bf88b1086ce721ca14841  Hantei-chan-0.1-windows-x64.zip
7cffeec6c774e25d27ca6859040423ace15315c9ef1d301f2a5d9308dd923c6a  gonptechan_0.1.exe
```

Built by CI run 38997623352 on `a615b56`.
