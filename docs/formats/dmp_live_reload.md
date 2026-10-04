# dMp live reload: edit in Hantei-chan, see it in the running game (design)

Status: design for the Hantei-chan side and the PovertyCaster seam. The PovertyCaster side has a matching document, `docs/design/asset_hot_reload.md`
in the PovertyCaster tree (worktree branch `feat/asset-hot-reload`, to be landed by the owner of PovertyCaster main). Facts about the game come from
`docs/formats/dmp.md` section 5 (reverse engineered from `DMP.EXE`, 2003-10-20 build, IDB `C:\dev\frenchbread\dmp_1020\DMP.EXE.i64`).

## 1. What the user gets

1. Open `GAMEDATA.PAC` (or a loose `data\...` folder) in Hantei-chan, edit a sprite sheet (`.IMG`) or a script (`.FOB`).
2. Press **Game > Reload in running game** (or save with "reload on save" enabled). The running dMp, started by PovertyCaster **offline** (training / local), shows the change
   within a frame or two: no restart, no PAC rewrite.
3. Netplay sessions refuse the request ("not allowed in a netplay session"), because the bank and texture set is part of the synchronised state.

## 2. Why this is cheap for dMp (facts that shape the design)

* **Loose files override the PACs.** Every script, image and demo is opened as a loose file first (`File_OpenLooseOrPack`, 0x4128A0: `CreateFileA(".\data\...")`
  relative to the game directory), then from `GameData.PAC` by upper-cased basename. The editor therefore never rewrites the PAC for live work: it writes the edited bytes
  to `<game dir>\data\<path>` (the same relative path the game asks for, e.g. `data\grp\p_font00.img`, `data\script\chara\chara01.fob`) and tells the game to re-read.
* **IMG v6 has no compression or obfuscation**: 20-byte header + raw 16-bit pixels. The D3D format (A1R5G5B5 = 25 or A4R4G4B4 = 26) is chosen by the loader, not by the file,
  so an editor-side sheet is just `w*h` words and the format must be remembered per sheet (players/enemies/type-0 script images use 25, type-1 images 26).
* **The game already has a texture reload path**: the device-lost recovery runs `Scene_Release(0)` then `Scene_Init(0)`, which releases every texture of the scene and
  re-runs `Tex_LoadImageIntoSlot` for each (loose file first). Banks, entities, RNG and pattern tables are untouched. A single sheet can be swapped with
  `Tex_ReleaseSlot(&slot)` + `Tex_LoadImageIntoSlot(path, format, &slot)`.
* **Scripts have no reload path** (`Script_LoadBank` returns the cached slot for a loaded path), but every engine-to-script call re-resolves bank + label by path, so a bank without
  live threads can be swapped between ticks: `Script_UnloadBanksFrom(idx)` + `Script_LoadBank(path)` + the load-time copy-out entry points (`Player_LoadAll(1)` for `CHARAnn`,
  `Event_LoadResources(1)` for `ENEMYnnn`, `Init`/`Start` for `STAGEnn`). Script globals live inside the code image, so a reload resets them.

## 3. Protocol (generic, game independent)

PovertyCaster's `pchost` (the injected DLL) polls a request file in the game directory from its existing per-frame driver pump, exactly like the screenshot seam
(`pchost_shot.req`). The editor never talks to the game directly.

```
<game dir>\pchost_reload.req        written by the editor (UTF-8, one request per line, written to a temp name and renamed so the pump never sees half a line)
    reload <path>                   path = the entry as the game names it, relative to the game directory, forward slashes, e.g.  data/grp/p_font00.img
    reload-all-textures             re-read every texture of the scene (Scene_Release(0) + Scene_Init(0))
<game dir>\pchost_reload.ack        written by pchost: one line per request, in order:  ok <path> | refused <path> <reason> | failed <path> <reason>
```

Why a file and not a named pipe: pchost already polls files on its safe seam (no listener thread, nothing to arm, works for every adapter, survives game restarts, trivially
testable from a shell). A named pipe would add a second transport next to `pc-ipc` for a few bytes per user action. The adapter interface does not care, so the transport can
change later without touching the editor contract (`ack` stays the answer channel).

Adapter interface (PovertyCaster side, default = refuse): `IGameAdapter::reloadAsset(const AssetReloadRequest&) -> AssetReloadResult`, called on the game thread **at the adapter's
safe point**, never inside a simulation tick, and only when no netplay session is attached. The generic seam in pchost owns the polling, parsing, the netplay refusal and the ack.

## 4. DMP adapter (PovertyCaster `pc-adapters/dmp`)

* Safe point: the existing frame seam (`DmpSim_Frame.cpp`, hook on `Scene_StepAndDispatch` 0x41ADD0): the main thread between two dispatcher spins, outside `GameMain_SimTick`.
* `reload data/grp/*.img | data/system/*.img | data/bg/*.img`: resolve the texture slot that holds the sheet (script image table `g_GfxImageTable[i]`, `g_TexPlayer`, `g_TexEnemy`,
  system CG slots), then `Tex_ReleaseSlot` + `Tex_LoadImageIntoSlot(path, same format, &slot)`; unknown slot or a changed width/height -> fall back to `reload-all-textures`
  (a resized sheet also needs its script rows changed, so the editor warns).
* `reload data/script/chara/charaNN.fob` (and `enemy`): refuse while a match is in progress (script globals would reset mid-round); at the character select / between rounds run
  `Script_UnloadBanksFrom` + `Script_LoadBank` + `Player_LoadAll(1)` + `Player_LoadTextures`. `STAGEnn` and `INITCONFIG`: refused in the first version (they own long-lived threads).
* Every reload invalidates the rollback manifest's assumptions (bank set, texture slots). The seam therefore refuses whenever a netplay or replay session is attached; in offline
  training the adapter re-baselines its saved-state bookkeeping after a bank swap (`DmpSim_State`), or the synctest in the precommit gate would flag it.

## 5. Hantei-chan side

* `File > Open` routes `GAMEDATA.PAC` to the archive browser; `.IMG` opens the sheet viewer/editor (`han2ui::OpenFileViewer`, 16-bit formats shown as A1R5G5B5 or A4R4G4B4 by a per-file
  setting that defaults from the file name), `.FOB` opens the script viewer (disassembly with function labels, `han2::dmpfob`).
* **Game > Live reload**: settings `gameDir` (the game folder PovertyCaster runs, auto-filled from the working folder) and `reloadOnSave`. "Reload" does: write the edited entry to `<gameDir>\data\<path>`
  (atomic, new file, never in place over a hard link), append `reload <path>` to `pchost_reload.req`, poll `pchost_reload.ack` for 3 s and show the answer in the status bar.
* The editor never modifies `GAMEDATA.PAC` for live work; "Save into PAC" is a separate explicit command (writes a new archive with the entries replaced).

## 6. Proof plan (in-game, windowed, killed by PID, hard-linked files only)

1. Start dMp offline under PovertyCaster from a hard-link copy of the install; take a backbuffer screenshot (`tools/pc_shot.sh`).
2. Edit one sheet (e.g. recolour the player's `.IMG`) with `fbchartool`/Hantei-chan, write it loose, send `reload`, screenshot again: the sprite changed without a restart.
3. Edit a `CHARAnn.FOB` constant at the character select, reload, enter the match, observe the new value.
4. Negative controls: the same request with a netplay session attached is refused; a request for an unknown path answers `failed`.
