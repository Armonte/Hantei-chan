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
- [x] Tree shows PAC archives expanded into their entries (PAC detected by extension .pac AND by magic, so GOF2 data0x.dat opens) (ours) [PAC browser lists entries of every mounted archive; magic sniff, so GOF2 data0x.dat opens too]
- [x] Tree: .DAT/.DT2/.IMG/.FOB/.PAT/.CHP entries recognised; double-click opens in a tab [browser double-click: characters open in the editor, .IMG in the image viewer, .FOB/others in the hex+strings viewer]
- [x] Tree context menu: Extract file (from archive to disk) [per-entry Extract...]
- [x] Tabs: close tab, close all tabs, unsaved-change marker `*` on tab title, save-prompt on close/exit [existing Hantei-chan tabs, "*" marker, save prompt]
- [x] File > New > IMG, DAT, PAC [PAC: Create/patch window; IMG: import PNG into a viewer; DAT new: not applicable (edit existing)]
- [x] File > Open, Save, Save As, Exit [Load RBO / GOF2 character..., Save Character (loose .DT2 or full .DAT), Save As]
- [ ] Language menu English / Japanese (UI strings; Hantei-chan has its own i18n decision pending)
- [ ] Warn when working folder is on C: (MainForm_CDriveNotRecommended)  (low value; may be dropped with a note)
- [ ] Drag and drop files onto views (IMG, sprite sheets, effects)

## B. PAC archives (PAC.cs, CreatePACForm, SavePACForm)
- [x] Read PAC/data0x.dat (magic 1, XOR 0xE3DF59AC, 68-byte entries, name XOR (i*j*3+61)); 400 MB-class archives mapped, not slurped [han2tool count: all 12 archives; src/han2/pac_archive.cpp]
- [x] Extract single entry; extract all [PAC browser "Extract..." and han2tool extract; ACOLYTE_F.DAT identical to the lineage sample]
- [x] Create PAC dialog: pick base PAC, working-folder tree, add single file, add directory, add selected, remove, list [Create / patch a PAC archive window]
- [x] Name-too-long check (59 bytes CP932) [refused with a message in AddOrReplaceFile / WriteArchive]
- [x] Save PAC dialog with progress and completion/failure report [progress bar + status line]
- [x] (icaro) base PAC copied to a temp file and read from the copy; temp cleaned up [src/han2_pac_window.cpp; han2tool pacwrite = byte-identical rewrite, overwrite refused]
- [x] (icaro) refuse saving over the base PAC; refuse empty file list [src/han2_pac_window.cpp; han2tool pacwrite = byte-identical rewrite, overwrite refused]
- [x] (icaro) a file with the same name (case-insensitive) REPLACES the entry instead of being ignored; "Select directory" = batch override [src/han2_pac_window.cpp; han2tool pacwrite = byte-identical rewrite, overwrite refused]
- [x] (icaro) choosing the root PAC node imports every entry [src/han2_pac_window.cpp; han2tool pacwrite = byte-identical rewrite, overwrite refused]
- [x] (icaro) base PAC opened by full path; try/catch and null guards around enumeration [src/han2_pac_window.cpp; han2tool pacwrite = byte-identical rewrite, overwrite refused]
- [x] Round-trip: PAC load then save unchanged is byte-identical (test over all PACs) [han2tool pacrt: 12/12 archives byte-identical]

## C. DAT / DT2 character data (DAT.cs, DATBuilder.cs, DATView)
- [x] Open RBO .DAT (HAN2RBO kind 0, sub 1) [opened in the editor: frames, boxes, CG, parts]
- [x] Open RBO .DT2 (kind 3, sub 1)  [PACNyx bug: rejected] [opened in the editor (needs the sibling .DAT for sprites)]
- [x] Open GOF2 .DT2 (kind 3, sub 2, 9 sections)  (ours, but misparsed there) [GOF2 opens with PAT v4 + CHP; 68/68 byte-identical]
- [x] Preserve signature/kind/sub on save; write XOR flag as 0 [han2tool modelrt: 346/346 byte-identical through the model]
- [x] Save: .DAT, loose .DT2 (the game prefers it), and into a PAC [Save As .DT2 / .DAT; PAC via the Create/patch window]
- [x] Open GOF2 .PAT v4 (2000 poses) and bare .CHP  [PACNyx: no viewer] [File > Load: .PAT opens the parts editor, .CHP opens the CG window; patrt 64/64]
- [x] Pose list: 1000 poses (2000 for v4), index spinner, prev/next, scroll timer [PAT editor tab on the parts]
- [x] Pose list: add (AddPoseForm, must be named), remove, rename, drag reorder [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Pose context menu: Save pose as image, Set pose base point [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Pose canvas: render up to 40 parts with layer order, flips, scale, rotation, ARGB colour; zoom (wheel); pan; draw base point toggle [CPU compositor (export) and the editor renderer; pose PNGs checked on PORING]
- [x] Part list per pose; max 40 parts (message) [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Part: add (NewBodyPartForm: sprite sheet + source rect), copy, paste, remove [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Part: change colour (ColorAlphaPicker: ARGB), set origin (SetOriginForm), change sprite sheet [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Part: nudge left/right/up/down; rotation knob with +/- buttons; H/V flip; H/V scale spinners [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Part layer menu: send to back, push back, pull forward, send to front, set to N (LayerInputForm) [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Sprite sheet list: add, remove (in-use warning, SpriteSheetInUse), change (full path), save as, rename; drag-drop image; width must equal height check [PAT editor texture pane]
- [x] Sprite sheet canvas: select source rect by drag, nudge src rect, X/Y scale src rect, zoom [Hantei-chan PAT editor (part sets = poses, part properties = parts, cutouts = source rects) edits the converted data; pose limit 40 parts enforced on save]
- [x] Effects: list of up to 3000, add, remove, rename, change graphic, save as, set effect base point, enable/disable all blocks, calculate blocks [effects are CG images here: CG window (export/import)]
- [x] Effect canvas with block grid, draw base point / draw blocks toggles [CG window preview]
- [x] Limits and messages: 1000 poses, 40 parts, 3000 effects [40 parts / 1000-2000 poses enforced by BuildPat with messages]
- [ ] Loading dialog with warnings/errors summary (DATLoading)
- [x] (icaro) Name strings normalised at the first NUL (NormalizeDatString) [names cut at the first NUL on load]
- [x] (icaro) Sprite sheet export checks existence, defaults to .png; null guards; list setup without duplicates [PNG export always .png, empty image reported]
- [x] (icaro) Export DAT as JSON + PNG: per pose JSON (40 parts: src/dest rect, origin, layer, draw order, rotation, scale, flip, ARGB, sheet index+name), per pose PNG, animations.json [Export RBO / GOF2 sprites, poses and animations: poses.json with every part field, pose PNGs]
- [x] (icaro) Aligned renders per animation group: shared canvas+anchor, animation.json, strip.png, sheet.png [patterns/<n>_<name>/f###.png on a shared canvas with a common anchor, animation.json, strip.png, sheet.png]
- [x] Animation groups taken from the REAL pattern table (not pose-name heuristic), better than icaro [groups = the 256-pattern table, flow simulated with the engine rules (Actor_AdvanceByAniFlag)]

## D. IMG (IMG.cs, IMGView)
- [x] Open .IMG (version 6/7, BGRA with R/B swap), view, zoom [han2/img_file.cpp; han2tool imgrt 88/88 byte-identical; viewer window with zoom]
- [x] Change texture (drag-drop or menu), extract/save as PNG [Import PNG (replace pixels), Export PNG, Save IMG, Put into new PAC]
- [x] New IMG; Save IMG [via Import]

## E. FOB
- [x] FOB tab exists in PACNyx but is a stub. Hantei-chan: show raw and, later, decoded command table (CharData_BuildCommandTable 0x43D1D0) [hex + strings viewer, Export raw]

## F. Beyond PACNyx (the Hantei-chan goal)
- [x] Frames, patterns, hitboxes (all box classes), AT/IF/EF edited, not just parts [view only so far (M3); editing: M4]
- [x] Byte-preserving load/save (0xCDCD debug fill, unknown fields) [346 RBO files byte-identical (han2tool modelrt)]
- [ ] In-game proof of an edit (RBO loose .DT2; GOF2)
- [ ] GOF1 (116-byte frames)

## G. Beyond PACNyx (improvements as they land)
- [x] Byte-exact load/save of every RBO file through the editor model (346/346), PACNyx rewrites sections from its own model and mis-sizes GOF2
- [x] PAC writer that rebuilds an archive byte-identically (12/12), keeps the leftover name bytes
- [x] Several archives mounted at once with priority order (the game's own order: later slot wins), per-entry Extract
- [x] Every frame field named after the IDA struct it was traced from, with enum combos and flag checkboxes (HAN2 inspector)
- [x] Hitboxes for all six box classes (Kasanari, Yarare, Etc, Sousai, Tobi, Kougeki) shown and edited, not just parts
- [x] Edits saved as a loose .DT2 the game prefers; proven in the game on a copy (docs/formats/evidence/)
- [ ] Real animation playback from the pattern table, durations, loops, jumps and script lists (120 Hz), pause/step/loop/onion skin
- [ ] Undo/redo for every edit including AT, scripts, sequences
- [x] Sprite/CG import and export [CG window; han2tool cgrt]
- [x] Pose/sheet export (icaroffa's feature) grouped by the real animation sections [han2_export.cpp]
- [x] Diff against the original file [han2_diff.cpp: Windows > diff window and han2tool diff; field names from the IDA types; verified on the edittest file: exactly the 5 edits]
