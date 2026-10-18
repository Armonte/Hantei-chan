Hantei-chan 0.1.1: fixes for issues reported by UNI2 modders after 0.1. Built from `main` at `48fe6b3` (CI run 39093535402).

## Fixes

- **Animation "End" is back** (#89).
  - The AF Animation dropdown offers End (value 3) again.
  - Values the list doesn't know show as a number instead of a blank box.
  - The animation-flag bits now match what each game actually reads, traced in IDA:
    - UNI2 and MBTL read bits 0–3, plus bit 7, which is the draw interpolation mode.
    - MBAACC reads bits 0–3.
    - Bits that are set in a file but not read by the game stay visible and editable.
  - Patterns that end on End are now summarised as "ends (holds last frame)" instead of "loops".
- **Script panel file list** (#90).
  - It lists every layer:
    - fpan-im mods and packs;
    - UNI2-IM / MBTL-IM Mods and Packs;
    - loose files;
    - the game archive.

    Each file is tagged with where it came from.
  - New `data/` scripts show up, as do VectorTable, BattleInfo, BoundEff and `_combase`.
  - You can pick a pack or its `data` folder, and saves go into that pack.
  - A Rescan button and auto-rescan were added.
- **Opening UNI2/MBTL/MBAACC .pat files** (#91). File > Open, the archive browser (including files inside UNI2 `d\` and MBTL archives), drag-drop, Recent and `--open` now open French-Bread .pat files in the PAT editor.
- **Project files keep PAT tabs** (#92).
  - Standalone PAT tabs and the `.pat` of HA6 characters are saved and restored.
  - A missing file shows "(file not found)".
- **CG manager undo** (#92).
  - Replacing an image in the MB/QoH banks is undoable.
  - "New PUPS bank" and the PovertyCaster palette export can be undone (created files are deleted after a confirm).
- **DXT3** (#92). Standalone DXT3 `.dds` import is supported.

## Download

- `Hantei-chan-0.1.1-windows-x64.zip`: the full package (unzip and run `gonptechan.exe`).
- `gonptechan_0.1.1.exe`: the exe alone, to update an existing 0.1 install.

## Checksums (SHA-256)

```
07601e7a9877311012c343ef13851b5dd42c0856023ab241f7bf11e9422cefe8  Hantei-chan-0.1.1-windows-x64.zip
cdee9e60f644540073f360e99c7737505ba8c1a99156b787934b8716f72e7250  gonptechan_0.1.1.exe
```
