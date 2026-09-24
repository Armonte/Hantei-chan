# Hantei-chan: open GitHub issue triage (update/ex-mbac)

- **Branch:** `feat/issues` in worktree `wt/issues`, based on `update/ex-mbac` at `c015c09`. Not pushed.
- **Scope:** all 32 open issues on Armonte/Hantei-chan as of 2026-09-23. The text of each issue, its comments and its attached videos and images were read. The two crash videos (#81, #8) were split into frames to see the exact repro.
- **UNI/TL issues** (#14, #46, #68, #71, #74, #76) belong to the UNI agent (worktree `wt/uni`, branch `feat/uni`). They are listed here as **handled in feat/uni**. Two commits on this branch touch the same HA6 load/save code; see [Overlap with feat/uni](#overlap-with-featuni).
- **#3** (detachable windows) belongs to the wave2 agent and is not touched here.
- Nothing was posted to GitHub. No issue was commented on, labelled or closed.

Status key:

- **fixed-before**: already fixed on the base branch. The evidence column says which commit and how it was checked.
- **fixed-now**: fixed or implemented on this branch.
- **partial**: some of the request is done; the rest is listed.
- **feat/uni**: owned by the UNI agent.
- **wave2**: owned by the wave2 agent.

## Status table

| # | Title | Status | Evidence | Commit |
|---|---|---|---|---|
| 82 | Left pane changes size when there are [Modified] patterns | fixed-now | The "Modified patterns: N" line and the "[Modified]" line only appeared once something was edited, so every widget below them moved. They are now one status line that is always drawn (dimmed at 0). Checked with `--capture`. | 2df4324 |
| 81 | Crash with certain words in Pattern Search | fixed-now | Reproduced headlessly: typing "OD DA" over names shaped like the video's fires `IM_ASSERT(i < Size)` in ImSearch's highlight pass. Whitespace glyphs have 0 indices, so a match reached `IdxBuffer[Size]`. CI's MinGW Makefiles build had no `CMAKE_BUILD_TYPE`, so asserts were live and the app aborted. imsearch is an upstream submodule, so the pattern searches now pass `ImSearchFlags_NoTextHighlighting`. The upstream fix is kept in `tools/patches/imsearch-highlight-index.patch`. Single-config builds now default to Release. `pattern_search_test` typed 6,930 queries over all MBAACC and UNIB names with 0 asserts; `--highlight` still reproduces the bug. | 7fbd8fe, bd79cfb |
| 80 | Ctrl+S doesn't work when you load a project | fixed-now | With a project open, Ctrl+S wrote only the `.hproj`. It now saves the active character, then the project. The File menu shows the chords. | 9c79b03 |
| 79 | Can't identify "null" attack properties | fixed-now | Both the HA6 and MBAC savers write ATST only for frames with an attack box. On any other frame the attack data is null to the game, whatever the fields say. The Attack data header now reads "(null: no attack box)" with a tooltip, and a Reset AT button was added. | 2df4324 |
| 78 | 0-alpha sprite hides boxes | fixed-before | `7b45460` (on master since May). Verified in `render.cpp::DrawLayers`: the box pass no longer skips `alpha == 0` layers, and PAT layers keep their boxes too (the #68 note). | 7b45460 |
| 77 | Some condition edits don't trigger undo/modified | fixed-before | `7d3428c` added `markModified` to the IF widgets. The document-level undo (0ef8fe7) also diffs the whole document at every gesture end, so even widgets that ignore their return value become an undo step. `undo_manager_test` "unnotified edit caught at the gesture edge" passes. | 7d3428c, 0ef8fe7 |
| 76 | PUPS not included (corrupts UNI) | feat/uni | Checked here only: `05f0280` loads and saves PUPS. PUPS was injected into a UNIB file and survived a round trip. | 05f0280 |
| 75 | Batch replace for variables | fixed-now | Windows > Variable references: find/rename a variable ID across EF1/101/1000/11/111, EF2/100/101/105, IF2/3/24/25/31/38, with tens-place encodings. One undo step. `refs_test` covers it. | fbffc85 |
| 74 | Attack flag means CH vector swap in UNI/TL | feat/uni | Already relabelled on this base (`b9cb3eb`, `frame_disp_at.h` bit 4 when `usesUniFormat()`). | b9cb3eb |
| 73 | Tab should remember its scroll position | fixed-before + extended | `7c9d40a` keeps the right pane's (EF/IF) scroll per view. This branch does the same for the left pane's frame-info area. | 7c9d40a, fbffc85 |
| 71 | UNI+TL saved edits corrupt data | feat/uni | Two causes found and fixed here, also affecting MBAACC; see [Overlap with feat/uni](#overlap-with-featuni). | 85d903f, 8d90da1 |
| 70 | Project palette not applied until the Palette menu is opened | fixed-now | Project load stored the number but never called `changePaletteNumber`. Verified: a project with palette 5 captures byte-identical to `--palette 5`, and different from palette 0. | 7fbd8fe |
| 68 | TL characters missing boxes | feat/uni | The PAT-layer box fix is on this base. The stacked-load fix in 8d90da1 (BaseData templates overriding the character) is a likely cause. Needs TL data. | 8d90da1 |
| 66 | Preview BGMs and their loops | fixed-now | Windows > BGM preview: the `bgm.txt` table (File/IsLoop/LoopPos), Ogg playback that wraps to LoopPos, and "Preview loop" (jump N s before the end). `bgm_test` parses MBAACC's 78 entries and decodes tracks. | a1a7862 |
| 64 | Pausing leaves a spawned effect frozen | fixed-before | Spawned actors are now a pure function of `currentTick` in the cached simulator (`6937af1`, merged in b4eaa9a). The old `activeSpawns` list that froze is no longer read anywhere (grep). Pausing shows the actors alive at that tick, and stepping frames moves them. | 6937af1 |
| 63 | Pattern comparison | fixed-now | Windows > Pattern comparison: overlays any pattern of any open character. Tint and opacity controls, boxes as outlines, stepping with the view's tick or a fixed frame, X/Y offset and snap, mirror, and "apply movement" (simulated root displacement). Checked with `--compare` + `--capture`. | ce9cf6d |
| 61 | Useful shortcuts | fixed-now | Num* / Num/ step keyframes even while typing in a field: the value is committed and the same field is re-focused (tested headlessly). P toggles the spawn preview. Ctrl+PgUp/PgDn and Ctrl+Shift+Tab switch tabs. Ctrl+Shift+T reopens a closed tab. Q/E were left out because they would type letters. | 9c79b03, 421000b |
| 58 | Annotate effects and conditions | fixed-now | Right-click an EF/IF header to add a note, which is shown under it as "// ...". The Pattern data section also has a pattern note. Windows > Notes lists every note. Stored in `<ha6>.notes.json` beside the file, never in the HA6. | 31a24c8 |
| 57 | Pattern picker for condition 18 | fixed-now | Filterable pattern picker on every condition pattern field. IF18 gets a "256 = owner standing" entry and shows the pattern name. | 2df4324 |
| 56 | HUD preview/editor | fixed-now | Windows > HUD preview: edit the 11 meter/guard colours, read them from MBAA.exe, write a patched copy (never the original), load/save `hud_theme.json`, preview animated bars, and view the gauge sheets and portraits. Offsets were verified on the exe; two guard offsets were corrected (see below). `hud_test` covers it. | 2a5d656 |
| 50 | `_c.txt` command file tool | fixed-before + extended | The command editor (`a0f19b4`) has a table, detail panel, search by ID/input/pattern/comment/note, notes and undo. This branch adds the "property" search: filter by a flagset bit. | a0f19b4, 5fcd3d1 |
| 46 | Load-through-txt picks the right UNI HA6 | feat/uni | The save-target rule (File01 for `_temp` stacks) is on this base (`b9cb3eb`). The stacked-load fixes are in 8d90da1. | b9cb3eb, 8d90da1 |
| 45 | Frame increment for sprites | fixed-now | Tools: "Append frame, sprite +1" and "Number sprites from here". | ebf7c53 |
| 44 | Mass-move patterns and update their calls | fixed-now | Pattern manager: move selected patterns with every reference updated (AF end jump, spawn effects including relative 101/111, the IF pattern fields and 10000+N). Paste remaps references between copied patterns. "New character from selection" builds a system-states template. | d4f2456, c3e6ae8 |
| 42 | Rework pattern management | fixed-now | Pattern manager: multi-select (click/Ctrl/Shift, keeps click order), copy across tabs and instances, the three paste modes from the issue (next empty / consecutive overwrite / original ids), reference remap, clear. The old push stack gains Drop last/Clear. | d4f2456 |
| 30 | Keyboard shortcut for Save Character | fixed-now | Ctrl+S saves the character (now also inside projects), and the menu shows it. Ctrl+arrows nudge the selected layer 1 px (Ctrl+Shift: 10 px). Every binding is in the remappable registry. | 9c79b03 |
| 14 | UNI2/MBTL support | feat/uni | Umbrella issue. | |
| 10 | Timeline functions | fixed-now | "Timeline" is always shown. It has a ruler with scrubbing, zoom (slider, Ctrl+wheel), middle-drag pan and pinned labels. A keyframes row supports select, drag-to-reorder, and copy/paste before/after, duplicate and delete. Indicator rows show box types, landing targets, EF and IF. Right-drag a spawn bar moves its effect to another keyframe; Alt copies it. | 88f670d |
| 9 | Remappable bindings | fixed-now | Edit > Keyboard shortcuts: capture a new chord, clear, reset, conflict marks. Saved in the settings ini. An invert-wheel-zoom option was added. Remapping mouse buttons is not done. | ed30f6d |
| 8 | Crash when pasting patterns | fixed-now | Root cause: tinyalloc keeps its heap pointer in per-process statics, and only the first instance called `ta_init`. The first copy or paste in a second instance dereferenced a null heap. Now the second instance attaches, a failed mapping falls back to a private clipboard, and the mapping name carries the layout size. `shared_clipboard_test` reproduces 0xC0000005 without the fix and passes with it. | e0803cb |
| 4 | Show the start of the pattern name in tabs | fixed-now | Tab label = "char - N <first 12 chars of the name>" (UTF-8-safe). | 9e7d832 |
| 3 | Detach tabs to new windows | wave2 | Not touched. | |

## Overlap with feat/uni

Two commits here change HA6 load/save in ways the UNI agent should rebase onto. They also fix MBAACC data loss, which is why they are here.

- **85d903f**
  - A pattern slot with only a name or flags (PTT2/PTCN/PSTS/PLVL, no PDS2) lost them on load and on every save. This is the "pattern names removed" part of #71. It also affected MBAACC (for example satsuki 315/316 and akiha 372).
  - `WriteAF` rewrote single-layer frames of AFGX patterns as AFGP.
- **8d90da1** fixes loading and saving a `.txt` stack:
  - **Merged saves.** Save wrote the whole merged stack into the target file, which baked the other files' patterns into it. Across the 117 multi-file MBAACC `.txt` that is 36,362 patterns (moons and base baked into the target moon file).
  - **BaseData templates won.** In UNI stacks (`_temp`, character, `../BaseData`), BaseData loaded last and won. BaseData's patterns are ★-named templates with sprite −1/−2, for slots the character also defines. The editor therefore showed and saved the templates in place of the character's own guard, hit and down patterns.
  - **Pattern count shrank.** A patch file with fewer slots (UNIB BaseData has 48) cut the pattern count.
  - **The fix:**
    - Per-pattern origin tracking.
    - Save writes the target's own patterns plus edited patterns.
    - Files after the character's own file only fill empty slots.
    - The pattern count never shrinks.
    - "Save Merged Stack As..." keeps the old flattening.
  - Verified by `tools/ha6_regress.sh`: the stacked save of all 117 MBAACC and 16 UNIB multi-file `.txt` is byte-identical to a single-file round trip of the target.

## Other findings

- **CI build type.** `.github/workflows/build.yml` configures MinGW without `CMAKE_BUILD_TYPE`, so `assert()`/`IM_ASSERT` were live in the user-facing builds. `CMakeLists.txt` now defaults single-config builds to Release. The workflow is unchanged.
- **HUD offsets (#56).**
  - The 8 meter colours are exactly at the reported RVAs (`mov [esp+14h], imm32`).
  - The guard colours are `mov r32, imm32` with the immediates at **0x252CC** (as reported), **0x252C4** (reported 0x252C6) and **0x252B9** (reported 0x252B8).
  - Checked on both MBAA.exe copies (`mbaacc`, `mbaacc_tag`).
- **UNIB `Unknown AT tag: ATBG`** appears when loading UNIB Hyd.HA6. This is for the UNI agent.
- **`--frame N`** sets the frame but not the tick (existing behaviour, seen in captures).

## Tests and regressions

All of these were run from WSL through Windows interop.

**New test tools** (all in `CMakeLists.txt`, built by `./build.sh`):

| Tool | What it checks |
|---|---|
| `pattern_search_test [--font F] HA6...` | #81 and #57, plus the Num* in-field keyframe step |
| `shared_clipboard_test` | #8. It spawns itself as a second process. |
| `refs_test [HA6...]` | #75, #44/#42, #58 |
| `bgm_test [BgmDir]` | #66 |
| `hud_test [MBAA.exe]` | #56 |
| `roundtrip.exe --stack OWN OUT FILES...` | Emulates the editor's stacked load and save |
| `tools/ha6_regress.sh` | Single-file round-trip exit codes, and stacked save == own-file save for every multi-file `.txt` (MBAACC and UNIB) |

**Existing suites.** All green at 5fcd3d1. bd79cfb only changes the search flags; pattern_search_test was re-run after it.

| Suite | Result |
|---|---|
| ha4tool roundtrip | 50/50 byte-identical |
| `tools/bg_regress.sh` | 124 ok, 0 failed |
| cmdfile_test | 272 checks, 0 failed |
| undo_manager_test, shortcut_router_test | PASS |
| cmdfile_ui_smoke | 0 asserts |

**HA6 round trip.** MBAACC gives 274 × exit 0 and 31 × exit 5 (the documented box cleanup), and UNIB gives 58 × 0 and 3 × 5. The same files return the same codes as before these changes.

**Not UI-tested by hand.** These were checked only by compiling them, with `--capture`/`--tool` screenshots and the headless tests:

- Timeline mouse interactions (drag, zoom, right-drag).
- Pattern manager operations in the live app.
- BGM audio output (the device was never opened in tests).
- Key capture in the shortcuts window.

## Suggested GitHub replies (not posted)

- **#82**: The Modified counter and the [Modified] tag are now a single status line that is always there, so nothing below it moves when patterns become modified.
- **#81**: Found it. With "OD DA" the search library's match highlighting indexed one past the end of ImGui's index buffer, and our CI build had asserts enabled, so the app aborted. The highlighting is off for now (results are still ranked), and builds default to Release. We added a headless test that types thousands of queries over every MBAACC/UNIB pattern list.
- **#80**: Ctrl+S now saves the active character and then the project file when a project is loaded. Before, it only rewrote the .hproj.
- **#79**: The game only gets attack data for frames that have an attack box, and the editor only writes it for those. Any frame without an attack box is "null", whatever the fields show. The Attack data header now says "(null: no attack box)" on those frames, and there is a Reset AT button.
- **#78**: Fixed in the May update: boxes are drawn even when the layer alpha is 0.
- **#77**: Fixed. Every condition widget marks the file modified, and the new undo system also catches any edit when the mouse or field is released.
- **#75**: Windows > Variable references: find every use of a variable ID (projectile vars, EF2/105, IF24/25/31/38, and more) and replace it in one undoable step.
- **#73**: Both side panes now keep their scroll position per tab.
- **#70**: Fixed. The saved palette is applied as soon as the project loads.
- **#66**: Windows > BGM preview reads bgm.txt. Play a track, and "Preview loop" jumps a few seconds before the end so you can hear it wrap to LoopPos.
- **#64**: With the new tick simulator, spawned effects are computed from the current tick, so pausing shows what is alive at that tick, and moving frames updates them. Nothing stays stuck.
- **#63**: Windows > Pattern comparison. Overlay any pattern from any open character with tint and opacity, outline boxes, offsets, mirror, and optional movement so you can compare range.
- **#61**: Num* and Num/ now step keyframes even while typing a value, and you keep typing in the same field. P toggles the spawn preview, Ctrl+PgUp/PgDn and Ctrl+Shift+Tab switch tabs, and Ctrl+Shift+T reopens a closed tab. Everything is remappable.
- **#58**: Right-click any effect or condition header to add a note. Patterns have a note field too. Notes are saved beside the HA6 (`<file>.notes.json`), never inside it.
- **#57**: Condition pattern fields have a searchable picker, and condition 18 includes 256 (owner standing).
- **#56**: Windows > HUD preview: edit the meter and guard colours with a live preview, read them from MBAA.exe or write a patched copy, save hud_theme.json, and browse the gauge sheets and portraits. Two guard offsets are really +252C4 and +252B9.
- **#50**: The _c.txt editor is in. Its search now also has a flag-bit filter (for example "Flagset 1 bit 1").
- **#45**: Tools > "Append frame, sprite +1" and "Number sprites from here".
- **#44 / #42**: Windows > Pattern manager. Select patterns (Ctrl/Shift), copy them to another tab or instance, and paste into the next empty slots, consecutive slots or the original ids, with references remapped. You can also move patterns with every caller updated, or start a new character from a selection (system-states template).
- **#30**: Ctrl+S (shown in the menu) saves the character, and Ctrl+arrows nudge the selected layer.
- **#10**: The Timeline is always visible. It now has zoom/pan, scrubbing, a keyframe row you can drag and copy/paste, indicator rows (boxes, landing, EF, IF), and right-drag to move spawn effects between frames.
- **#9**: Edit > Keyboard shortcuts lets you rebind anything (saved in settings). Invert wheel zoom is there too. Mouse button remapping isn't done yet.
- **#8**: Found the crash. The second gonptechan instance never initialised the shared clipboard heap, so its first copy or paste crashed. Fixed, and the fix is covered by a two-process test.
- **#4**: Tabs now show the first 12 characters of the pattern name.
