# Archive / game-folder UX audit, 2026-10-04

Scope: every way a user gets a French Bread file into Hantei-chan (gonptechan.exe) and what they can see before committing to anything.
Baseline: `3b6caef` (update/ex-mbac). Fix: branch `ui/archive-browser` (this document ships with it).
Screenshots are the app's own capture (`tools/ui/shoot_all.sh`, `--ui-script`, no real mouse or keyboard involved) and live next to this file.

## 0. Headline: "how does a new user view a GOF2 character today, step by step?"

Before (baseline 3b6caef), with the GOF2 install at `C:\games\gof2_run`:

| # | Step | Problem |
|---|---|---|
| 1 | Launch. A green void and a menu bar: "No character loaded". | No start screen, no hint that archives exist. |
| 2 | Find the right menu. File > Open... is the obvious one, but the archive browser is only under **Tools > French Bread archives (PAC, .p, .dat)...** (moved there from File on 10-03). | Hidden menu path, name says nothing about "game folder". |
| 3 | Add archive... (Windows file dialog, one archive at a time). GOF2 splits characters over `data00..data05.dat`. | Which one has AMIY? Nobody knows. |
| 4 | Add the others too. A character's `.DT2` is in one archive, its `.DAT`/`.PAT`/`.CHP` in others; the mount order matters (later wins). Mount them in the wrong order, or forget one: the character opens with "no .DAT next to the .DT2: no sprites / parts". | Manual mount list with Up/Down buttons; failure is a warning in a popup, not an explanation. |
| 5 | Scroll a 2,300-row table of names. Only a filter box. No thumbnails, no type column, no way to tell a character from an effect file. | You pick by file name. |
| 6 | Double-click `AMIY.DT2`. The editor tab opens at 3x zoom (sprites bigger than the viewport), a floating "Loading report" pops over the left pane, a floating inspector covers the right pane; the animation player is not open. | Nothing is where PACNyx or a game tool would put it. |
| 7 | Find the playback controls: Tools > Animation player... (floating). | Hidden again. |
| 8 | Edit something, press Ctrl+S: "No save target set for AMIY" / Save Error. Only Save As to a loose file exists. | A character opened from an archive cannot be saved back. |

New flow (this branch): **launch, click "Open GOF2" (a remembered folder opens in one click; first time one folder dialog), double-click a character**. Two or three clicks.
Screens: `01_welcome.png` (start screen), `03_rbo_archive.png` / `03b_rbo_grid.png` (browser), `07_gof2_character_view.png`, `08_gof1_character_view.png`, `09_rbo_character_view.png` (what you land in).

## 1. PACNyx+ (what the community already knows)

Sources: `C:\dev\pacnyx\dotPeek\PACNyx+`, `C:\dev\pacnyx-icaroffa-2026\screenshot-6.png`, `docs/formats/pacnyx_parity.md`.

* **Panel layout**: a left tree (working folder > `.PAC` expanded into entries `X.DAT`, `X.FOB`, greyed when not openable); the rest is one tab per opened DAT. Menu bar: File / Sprite Sheets / Poses / Body Parts / Effects. Inside a DAT tab sub-tabs Poses / Effects / ...
* **Picking a character**: click `.DAT` in the tree (RBO only; GOF2 and `.DT2` were rejected or misparsed before our fork).
* **Sprites / animations / frames**: the "Poses" tab: a pose list (NOMAL, PATTERN001 ...) on the left, a canvas with +/- zoom and a "Draw Basepoint" toggle in the middle, nudge / rotate / scale panels on the right, the sprite-sheet list and the sheet canvas (cut-out rectangles) below. A pose is one still: there is **no playback, no frame strip, no timing, no boxes**. "Animations" exist only as an export (JSON + PNG strips).
* **Editing**: parts of a pose (nudge, rotate, scale, layer, colour), sprite-sheet cut-outs, effects blocks. No hitboxes, no frame data, no scripts.
* **What is good (kept)**: one tree from the working folder down into the archives; pick a character and land in its visuals immediately; sheets and poses visible side by side.
* **What is clunky (improved)**: folder + archive handling is manual; no previews inside the tree; no search; no thumbnails; modal dialogs for save; no playback.

Hantei-chan already covers everything PACNyx edits (PAT editor, CG window) and far more (frames, 6 box classes, AT/IF/EF, scripts, playback). The gap was purely discoverability: see 0.

## 2. Every way a file gets opened (baseline), clicks to SEE something, dead ends

| Entry point | What happened | Clicks to see an asset | Dead ends |
|---|---|---|---|
| File > Open... (smart, header based) | Characters / stages / projects load. `.PAC` / `.p` / GOF2 `data0x.dat` open the archive window (hidden under Tools). | archive: 4+ | "Unrecognized file format" popup for an `.IMG`, `.EX3`, `.CG`, `.PAL`, `.FOB`, `.txt`... Points the user to *File > Import*. |
| File > Open Recent | Files opened through Open... only. | 1 | No game folders, no archives' last entry, nothing remembered between launches for the archive window. |
| File > Import > ... | One loader per format. | 3+ | Power-user path; `.HA6` opened alone has no sprites (`.cg` not loaded). |
| Drag and drop | `.img` -> floating viewer; `.dt2 .dat .pat .chp .pac` -> openHan2File; `.p` -> smart open; `.png/.bmp` -> a popup telling you to open something else first. | 1 | Dropping an `.EX3`, `.cg`, `.FOB`, a folder: "Not an RBO / GOF2 file type". A folder is not accepted at all. |
| Tools > French Bread archives | Floating window: mounted archive list (Up/Down/Unmount), entry table (name, size, **Extract...** button per row), filter, Working folder tree, EN/JP button. | n/a | **Every non-character row only offers "Extract..."** (a file dialog per entry) or a double-click that opens a *separate* viewer window; no preview in the list, no thumbnails, no multi-select, no extract-all for PAC (only for PKFileInfo kinds), no remembering of what was mounted. |
| `.p` (PKFileInfo, MBAC Act Cadenza, MB, ReAct) | Browser lists entries; double-click on a `.DAT` **extracts to %TEMP% and opens the copy**. | 3 | Save writes the temp copy silently; the only way back into the archive is Save As ... .p to a NEW archive. |
| GOF1 `gof_0N.p` | Own code path (`\x01gof1`): characters open in memory. | 3 | No save-back (Save As .DAT / new .p only, and the `.p` writer left the section cipher off the entry, see below). |
| GOF2 `data0N.dat` / RBO `*.PAC` | Characters via a merged reader over the mounted list. | 5+ | See section 0. Save impossible ("No save target"). |
| `.EX3` (GOF1, MB line) | Hex dump / strings in a viewer window. | 3 | The image is never shown: extract to disk, convert with an external tool. |
| `.cg` / `.CHP` banks | `.CHP` opens the CG window on an empty character; MBAACC `.cg` has no inline path. | 3 | No frame list or palette preview outside a loaded character. |
| `.IMG` (every RBO/GOF2 sprite sheet) | Floating viewer window with zoom, Export PNG, Import PNG, "Save IMG..." to a loose file. | 3 | Opening the next image means closing / closing the window and going back; no "next image". Edits cannot go back into the archive except via the separate "Create / patch a PAC archive" tool. |
| MBAACC folder (`data/*.HA6`) | File > Open... on one `.HA6`. | 3 | Loads **without sprites**: the `.cg` / `.pal` stack is described by `<stem>.txt`, which the user must know to open instead. 1,279 files in the folder, no character list. |

## 3. Other human-hostile flow found on the way

1. Default view zoom 3.0 for every character: GOF2 / RBO sprites overflow the viewport on open.
2. Warning "no .DAT next to the .DT2" fires for **every GOF2 character** (they keep parts in `<stem>NN.PAT/.CHP`), opening a floating Loading report each time.
3. The HAN2 inspector and animation window float over the editor panes instead of docking (the animation window was not even open by default).
4. Right pane / animation window / box pane are three places for "what frame am I on"; the frame strip had no pictures (grey blocks).
5. No pattern list with pictures; the pattern combo box is the only index of 100+ animations.
6. `Save` on a character opened from an archive reports an error and keeps the character dirty; the status bar says "No save target set".
7. `.bak`: GOF2 companions had one, a PAC entry did not.
8. A real bug on the way: **GOF1 Save As ... .p wrote the new entry without the character section cipher** (`FrameData::save` -> `gof1::SaveFile`, `.p` case passed the decrypted sections to `WriteArchiveReplacing`). Archives produced that way differ from the original by the whole entry on a no-edit round trip; the browser's write-back proved it (fixed, see section 5).
9. File dialogs and the Windows folder picker are the only way to point at a game; nothing remembered between sessions (the last browsed folder was lost; the old browser kept a "working folder" for a different job).
10. Floating tool windows that overlap the main viewport leave the main window and become OS windows (Dear ImGui viewports): fine for the user, invisible to screenshots. Scripted captures use sizes that fit inside the main window.

## 4. What changed (branch `ui/archive-browser`)

### One archive browser, for every archive and every game folder (`src/archive_browser*.cpp`)
* File > **Open Game Folder** (GOF2 / GOF1 / RBO / MBAACC / auto-detect), File > **Archive Browser** (Ctrl+B), Tools menu entry, drag-drop and Open... of an archive all land in the same dockable window.
* **Game detection** from file names only (`abrowser::DetectGame`): `data0N.dat` PAC magic -> GOF2, `gof_0N.p` / gof.exe -> GOF1, `*.PAC` -> RBO, MBAA.exe / `*.HA6` -> MBAACC, `data0N.p` -> Melty Blood, `0N.dat` -> Party Breakers; archives are mounted in the game's own priority order (DATA01 .. Update01 .. Ex discs; later overrides earlier).
* Left: **Sources** (the merged "game view", the folder's loose files, each archive with its entry count). Middle: sortable list with **name, type, size, lazy thumbnails**, list or grid, filter box (Ctrl+F), type filter (Characters, Images, Sprite banks, Parts, Scripts, Audio, Text...). Right: **inline preview**.
* The merged game view lists every file once, as the game sees it; a `.DT2` hides its sibling `.DAT` (character data), MBAACC version files `akiha_0.HA6`, `_r` are character data of `akiha.HA6`.

### Inline preview (nothing is extracted)
| Entry | Preview |
|---|---|
| `.IMG` (RBO/GOF2 v6-8, QoH, Rosa), `.EX3` (GOF1, MB line), `.BMP`, `.PNG` | image view: wheel zoom about the cursor, drag pan, Fit / 1:1 / 2x / 4x, checkerboard, palette swatches, format line (`02_gof2_pac_image.png`, `04_ex3_image.png`) |
| `BMP Cutter` banks (`.cg`, `.CHP`) | frame list (index, name, size) + palette 1-8 selector + zoomable frame (`06_mbaacc_cg_bank.png`) |
| Characters (RBO `.DT2/.DAT`, GOF2, GOF1 `.DAT`, MBAACC `.HA6`, MBAC `.DAT`) | idle-frame thumbnail rendered on the CPU by the existing compositor, plus format, pattern / frame / sprite / pose counts and first pattern names (`03_rbo_archive.png`, `05_mbaacc_character.png`) |
| Text, `.ini`, `.txt` | scrollable text, UTF-8 or Shift-JIS detected |
| Anything else | hex view with ASCII, WAVE / PAT facts |

### Open, save, extract
* **Double-click or Enter** opens in the right editor, in memory: RBO/GOF2 characters read through the mounted archives, GOF1 straight from its `.p`, `.IMG`/`.FOB`/typed tables in their existing viewer windows, MBAACC `.HA6` through its `<stem>.txt` stack (so sprites and palettes load). The browser then closes itself unless **Stay open** is ticked.
* **Save** (Ctrl+S) on a character from an archive writes **back into that archive**: `abrowser::SaveCharacterIntoArchive` (PAC `.DT2/.DAT` + changed GOF2 `.PAT/.CHP`; GOF1 entry) and `SaveEntryBytesIntoArchive` (managed copies of `.p` entries). The first `<archive>.bak` is kept, the rebuilt archive is written next to it and swapped in, mounted copies re-read. IMG / FOB / typed viewers got a "Save into the archive (keeps a .bak)" button. All writers are the existing ones (`fbarc::Archive::rebuild`, `gof1::SaveFile`, `han2::SaveGof2Companions`).
* **Extract** is optional: right-click, the preview's Extract... button or the source menu; multi-select (click, Ctrl, Shift, Ctrl+A), chosen folder, progress and cancel on the status bar, never blocks the UI.
* **Remembered**: last browsed folder / archive (restored on Ctrl+B), ten recent items, one folder per game (the start-screen buttons reopen it in one click), thumbnails toggle.
* **Keyboard**: arrows / PgUp / PgDn / Home / End (Shift extends), Enter opens, Ctrl+A, Ctrl+F, type-to-search by file name.
* **Never blocks**: entry tables, previews, thumbnails and extraction run on one worker thread; thumbnails are produced lazily for the visible rows only and cached as GL textures (LRU, 700).

### A GOF/RBO workspace you land in (the PACNyx-equivalent, plus what PACNyx never had)
When a HAN2 character is opened from the browser the editor now docks:
* **Animations** list (top left): every pattern with a picture of its first frame, name, frame count, search; follows the selection; click selects.
* **Frame strip with pictures** beside the playback controls (Play / Pause / Restart / frame step / tick step / loop / rate / speed / onion skin): one card per frame with duration; click selects; the playing frame is outlined.
* **Boxes overlay**, view, Left Pane (pattern / frame properties), HAN2 inspector tab (Right Pane), Box Pane tab: all the existing panes, docked, at a sprite-sized zoom (1.25x).
* Window > **Animations (list with thumbnails)** toggles the list; `Window` menu keeps the rest.
* **Start screen** (no character open): Open GOF2 / GOF1 / RBO / MBAACC, auto-detect, open archive, open file, recent folders and files.

## 5. Verification

* Build: the repo's Windows (mingw) build, `nice -n 15 make -j3`.
* Format suites (all on the real installs under `C:\games`, read-only): `tools/han2/run_roundtrip.sh` 15 sections, every one `fail 0 skipped 0`, exit 0; `tools/fb/run_fb_suite.sh` 118 archives, fbrt / ex3 / fbedit / react / mb / pb2k1 / dmp / rosa / qoh99 / qoh98 / locality all `fail 0 skipped 0`, exit 0; `tools/cg/run_cgm_suite.sh`: see `docs/ux/suite_logs/`. Logs: `docs/ux/suite_logs/{han2,fb,cgm}.log`.
* Write-back, on **copies** of the game archives (scratch folder, game files never touched), through the browser (open character, Save): RBO `Update01.PAC`, GOF2 `data05.dat`, GOF1 `gof_01.p` each rewritten **byte-identical** to the original, `.bak` created and identical to the original. (GOF1 only after the cipher fix above: before it, 5.4 MB of the archive differed.)
* Keyboard / type-to-search: scripted (`10_keyboard.txt`): Down, Down moves the cursor 0 -> 2; typing `CY` jumps to CYOSOKABE.
* Screenshots (app's own capture): `01_welcome`, `02_gof2_pac_image`, `03_rbo_archive` (+ `03b_rbo_grid`), `04_ex3_image`, `05_mbaacc_character`, `06_mbaacc_cg_bank`, `07_gof2_character_view`, `08_gof1_character_view`, `09_rbo_character_view`.

## 6. Not done / left

* Editing an IMG in the viewer and saving into the archive uses the new button but was not exercised through a script (the byte-exact write path is the same `ReplaceEntries` the character saves use).
* The PAT parts editor and the standalone CG window opened from a `.p` entry still work on a managed temporary copy; their own save paths are not wired to write back (characters and the IMG / FOB / typed viewers are).
* Pattern names for GOF2 are empty in the files (numbers only); RBO/GOF1 show their Japanese names.
* The thumbnail renderer calls the existing composition entry points (`DrawPose`, `DrawCgImage`) and does not touch their math; it ignores flip mode 1 (mirror) and layer rotation of HA6 layers. Rotation / spacing problems the user reported in GOF/RBO compositions belong to `fix/gof-rbo-render` and are not changed here.
* Floating tool windows that were already floating (CG window, diff window, load report) are not auto-docked.
