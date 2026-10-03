# PACNyx+ parity checklist (Hantei-chan replaces PACNyx+)

Inventory taken 2026-10-03 from our modified PACNyx+ (`C:\dev\pacnyx\dotPeek\PACNyx+`, `...\PACNyx`) and from icaroffa's 2026
exes (decompiled to `C:\dev\pacnyx-icaroffa-2026\_releases\decomp\{orig2010,ic10,ic12}`). "100%" = every box ticked.
Tick a box only when Hantei-chan does it AND a test/tool run or in-game check is cited next to it.

Credit: the 2026 GUI behaviour (PAC creation hardening, DAT crash fixes, JSON/PNG/aligned export) was first done by
ícaro Fellini (github.com/icaroffa/RAGNAROK-BATTLE-OFFLINE-PACNYX-2026). It has no source and no licence. Hantei-chan
reimplements the behaviour from the decompiled reference. No code is copied. The original PACNyx authors (2010) own the base tool.

Legend: [x] done, [ ] open. "(icaro)" = added by icaroffa 2026. "(ours)" = in our modified PACNyx (GOF2 work).

## A. Main window (MainForm)
- [ ] Working-folder picker + refresh; file tree of the folder (config.pcf remembers it)
- [ ] Tree shows PAC archives expanded into their entries (PAC detected by extension .pac AND by magic, so GOF2 data0x.dat opens) (ours)
- [ ] Tree: .DAT/.DT2/.IMG/.FOB/.PAT/.CHP entries recognised; double-click opens in a tab
- [ ] Tree context menu: Extract file (from archive to disk)
- [ ] Tabs: close tab, close all tabs, unsaved-change marker `*` on tab title, save-prompt on close/exit
- [ ] File > New > IMG, DAT, PAC
- [ ] File > Open, Save, Save As, Exit
- [ ] Language menu English / Japanese (UI strings; Hantei-chan has its own i18n decision pending)
- [ ] Warn when working folder is on C: (MainForm_CDriveNotRecommended)  (low value; may be dropped with a note)
- [ ] Drag and drop files onto views (IMG, sprite sheets, effects)

## B. PAC archives (PAC.cs, CreatePACForm, SavePACForm)
- [ ] Read PAC/data0x.dat (magic 1, XOR 0xE3DF59AC, 68-byte entries, name XOR (i*j*3+61)); 400 MB-class archives mapped, not slurped
- [ ] Extract single entry; extract all
- [ ] Create PAC dialog: pick base PAC, working-folder tree, add single file, add directory, add selected, remove, list
- [ ] Name-too-long check (59 bytes CP932)
- [ ] Save PAC dialog with progress and completion/failure report
- [ ] (icaro) base PAC copied to a temp file and read from the copy; temp cleaned up
- [ ] (icaro) refuse saving over the base PAC; refuse empty file list
- [ ] (icaro) a file with the same name (case-insensitive) REPLACES the entry instead of being ignored; "Select directory" = batch override
- [ ] (icaro) choosing the root PAC node imports every entry
- [ ] (icaro) base PAC opened by full path; try/catch and null guards around enumeration
- [ ] Round-trip: PAC load then save unchanged is byte-identical (test over all PACs)

## C. DAT / DT2 character data (DAT.cs, DATBuilder.cs, DATView)
- [ ] Open RBO .DAT (HAN2RBO kind 0, sub 1)
- [ ] Open RBO .DT2 (kind 3, sub 1)  [PACNyx bug: rejected]
- [ ] Open GOF2 .DT2 (kind 3, sub 2, 9 sections)  (ours, but misparsed there)
- [ ] Preserve signature/kind/sub on save; write XOR flag as 0
- [ ] Save: .DAT, loose .DT2 (the game prefers it), and into a PAC
- [ ] Open GOF2 .PAT v4 (2000 poses) and bare .CHP  [PACNyx: no viewer]
- [ ] Pose list: 1000 poses (2000 for v4), index spinner, prev/next, scroll timer
- [ ] Pose list: add (AddPoseForm, must be named), remove, rename, drag reorder
- [ ] Pose context menu: Save pose as image, Set pose base point
- [ ] Pose canvas: render up to 40 parts with layer order, flips, scale, rotation, ARGB colour; zoom (wheel); pan; draw base point toggle
- [ ] Part list per pose; max 40 parts (message)
- [ ] Part: add (NewBodyPartForm: sprite sheet + source rect), copy, paste, remove
- [ ] Part: change colour (ColorAlphaPicker: ARGB), set origin (SetOriginForm), change sprite sheet
- [ ] Part: nudge left/right/up/down; rotation knob with +/- buttons; H/V flip; H/V scale spinners
- [ ] Part layer menu: send to back, push back, pull forward, send to front, set to N (LayerInputForm)
- [ ] Sprite sheet list: add, remove (in-use warning, SpriteSheetInUse), change (full path), save as, rename; drag-drop image; width must equal height check
- [ ] Sprite sheet canvas: select source rect by drag, nudge src rect, X/Y scale src rect, zoom
- [ ] Effects: list of up to 3000, add, remove, rename, change graphic, save as, set effect base point, enable/disable all blocks, calculate blocks
- [ ] Effect canvas with block grid, draw base point / draw blocks toggles
- [ ] Limits and messages: 1000 poses, 40 parts, 3000 effects
- [ ] Loading dialog with warnings/errors summary (DATLoading)
- [ ] (icaro) Name strings normalised at the first NUL (NormalizeDatString)
- [ ] (icaro) Sprite sheet export checks existence, defaults to .png; null guards; list setup without duplicates
- [ ] (icaro) Export DAT as JSON + PNG: per pose JSON (40 parts: src/dest rect, origin, layer, draw order, rotation, scale, flip, ARGB, sheet index+name), per pose PNG, animations.json
- [ ] (icaro) Aligned renders per animation group: shared canvas+anchor, animation.json, strip.png, sheet.png
- [ ] Animation groups taken from the REAL pattern table (not pose-name heuristic), better than icaro

## D. IMG (IMG.cs, IMGView)
- [ ] Open .IMG (version 6/7, BGRA with R/B swap), view, zoom
- [ ] Change texture (drag-drop or menu), extract/save as PNG
- [ ] New IMG; Save IMG

## E. FOB
- [ ] FOB tab exists in PACNyx but is a stub. Hantei-chan: show raw and, later, decoded command table (CharData_BuildCommandTable 0x43D1D0)

## F. Beyond PACNyx (the Hantei-chan goal)
- [ ] Frames, patterns, hitboxes (all box classes), AT/IF/EF edited, not just parts
- [ ] Byte-preserving load/save (0xCDCD debug fill, unknown fields)
- [ ] In-game proof of an edit (RBO loose .DT2; GOF2)
- [ ] GOF1 (116-byte frames)
