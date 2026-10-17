Changes after Hantei-chan 0.1 (`a615b56`), for the next release.

## Hot reload from every save (UNI2 / MBTL, fpan-im)

In 0.1, only saves in the Squirrel Script panel triggered an fpan-im reload. Now every save of a UNI2/MBTL character file triggers one:

- the main editor's HA6 Save / Ctrl+S;
- the PAT editor's .pat save;
- the CG manager's .cg bank save, CG + .pat "Save to mod folder", and "Save .pal" / "Save .pal to mod folder".

The mapping is `sqedit::FpanimPlanSave` in `src/squirrel/fpanim_link.h`, unit tested in `tests/fpanim_link_test.cpp`:

- `data/chrNNN/<file>` sends `reloadchara chrNNN [live]`.
- A shared file (every `script/*.txt`, `data/_combase.txt`, `VectorTable.txt`, `BattleInfo.txt`, `BoundEff.txt`) sends `reloadstd [live]`.
- Anything else is saved but not reloaded.
- The pipe is `\\.\pipe\fpan-im-uni2` / `fpan-im-mbtl`, falling back to `fbmod-<game>`.

**Settings** (Edit > Preferences > UNI2/MBTL: mod folder and hot reload):

- **UNI2: / MBTL: save into the IM mod folder.** On by default, and it applies when the game folder exists. After the normal save, the file is also written to `<game folder>\UNI2-IM\Mods` (`MBTL-IM\Mods`) at its archive-relative `data\...` path, with the earlier file kept as `.hcbak`. This uses the CG + .pat tab's mod-folder helper, which now refuses absolute paths and `..`. Save As works as before.
- **UNI2 / MBTL game folder.** The same setting as the Script panel's Game folder. The default is the Steam folder.
- **Hot reload** and **Reload mode** (rebuild / live). These are the Script panel's own switches (`SqHotReload` / `SqHotReloadMode`), so both places always show the same state.

**Status:** the menu bar shows where the file went and the summarised fpan-im reply: rebuild time, HA6 root before -> after, or the refusal outside a training battle. Hover it to see the raw reply.

**UI script:**

- `hotsave status|on|off|mode rebuild|live|tomods uni2|mbtl on|off|gamedir uni2|mbtl <dir>`
- `framedur <pattern> <frame> <ticks>`

**Live copy:** UNI2-IM / MBTL-IM index their `Mods` folder once at boot, so a file that first appears there later is not loaded by the running game. Before each reload, Hantei-chan therefore also copies the file into fpan-im's own mods folder (the `mods=` path from the ping reply), which fpan-im rescans on every reload. The Script panel already did the same. "Restore mod folder" restores only the IM copy.

**In-game proof (UNI2):** fpan-im `tests/ingame/run_mainsave.sh` with `fpanim_uni2_hotreload_mainsave.gcs` and `hc_mainsave_editor.txt`. Evidence is in fpan-im `docs/evidence/hotreload/mainsave_uni2_2026-10-17`.

- In the first boot, before the live copy existed, the editor's save was written to `UNI2-IM\Mods\data\chr000\chr000.ha6`. fpan-im answered `OK engine rebuild chr000: ...; ha6 root 1E980288 -> 37AC1728; back in battle after 1125 ms`.
- The 5A frame 10 frames in was still 3, though, because fpan-im's VFS had 0 files and UNI2-IM had not indexed the new file. That run is what led to the live copy.
- The live copy has not had its own boot yet.
