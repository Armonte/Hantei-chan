Hantei-chan 0.1.2: fixes for issues reported by UNI2 modders and on GitHub after 0.1.1. Built from `main` at `874ae5f` (CI run 39192595794).

## Fixes

- **Giant hurtbox in the view** (#100). A frame that spawns an effect drew the effect's boxes filled, in the frame's own colours. UNI2 chr000 J236A showed the 600×600 attraction box of pattern 148 as "Hurtbox 1". The data and saves were never affected.
  - Spawned effects' boxes are now hidden.
  - **Spawned Patterns Visualization > Show spawned boxes (outline)** shows them as outlines.
- **Hot reload** (#104).
  - Custom-named UNI2-IM characters (`data/Okita/Okita.ha6`, `Okita_0.txt`, …) now reload like `chrNNN` ones, from the main editor and from the Script panel.
  - A file saved straight into a `UNI2-IM\Packs\<pack>\` (or `Mods`) folder now reloads where it is. This works without a game folder set, and the file is never copied into Mods, where it would shadow the pack.
- **Timeline** (#105, issue #88). The Collision, Hurt, Attack, Clash, Landing, Effects and Conditions rows now follow the simulated flow, the same as the playhead and the Main (sim) row. Before, they were laid out in file order and drifted off the playhead after the first Go to or loop. The Keyframes row stays in file order for editing.
- **Script panel: large files** (#99). Squirrel files over about 6,500 lines (VectorTable-shaped) showed a black editor. Now:
  - files of any size render;
  - the outline groups repeated entries (`VectorSample.Tbl (49)`).
- **CG manager**
  - **Big images** (#101). Sprites larger than the preview (UNI2 1569×993 and similar) showed only the checkerboard. Now:
    - **Fit** (on by default) shows the whole image;
    - with Fit off, the zoom is exact and the preview scrolls; middle or right drag pans.
  - **Banks the bank editor can't parse** (#102), for example ones rebuilt by other tools:
    - The Browser's Palette number now recolours the thumbnails and the preview.
    - **Save to the character's .cg** and **Save bank as…** now write the bank and show an error when they fail. Saving to the character's .cg also triggers the mod-folder / hot-reload hook.
    - The panel says why the bank editor rejected the bank.
  - **"GL Error: 0x501" box on every scroll** (#102, issue #87). A failing thumbnail upload now shows as empty and is logged with its size. GL errors are logged instead of opening a message box.
- **Archive browser** (#97). It no longer opens at startup. Open it with **Ctrl+B** or **Window > Archive Browser**; it docks as a tab. To open it at startup again, tick **Preferences > Open the archive browser on startup**.

## Download

- `Hantei-chan-0.1.2-windows-x64.zip`: the full package (unzip and run `gonptechan.exe`).
- `gonptechan_0.1.2.exe`: the exe alone, to update an existing 0.1 / 0.1.1 install.

## Checksums (SHA-256)

```
67f076b4acb337f03a7fc2a9e2735d0e2103f3860e6087f897597e85d7f9b527  Hantei-chan-0.1.2-windows-x64.zip
1502fdbf518eb13e727b42a6fe448e3a6409b6619f217f230667a8ebc8e40158  gonptechan_0.1.2.exe
```
