# Hantei-chan command-file (`_c.txt`) editor

Status: 2026-09-23. Branch `feat/cmdfile` (worktree `wt/cmdfile`), based on `update/ex-mbac`. This covers phases C and D of the port plan in `GONPTECHAN_EX_INVENTORY.md` §5.

## 1. Summary

- **New lossless parser and serializer** (`src/cmdfile/cmd_document.*`). An unmodified save is byte-identical for all 116 MBAACC and 24 MBAC command files (section 4). It replaces `FrameData::load_commands`, which read ExComCheck rows and MBAC tables as commands and cut CP932 lead bytes out of comments.
- **Command workspace and editor**, ported from Gonptechan EX `1abc27f9` and restructured. It covers commands and ExComChecks, linked selection, search, inline editing, reordering, insert, duplicate, delete, comment-out and restore, notes, per-file undo and staged saves.
- **Extension Profile setting**: Preferences > Extension profile, either Vanilla MBAACC (the default) or Extended Melty / BOF.
  - It gates the BOF `_c` rules and ExComCheck Types 2/3.
  - It also gates the BOF-only frame-data panels: IF 152-157, IF 14 collider filters, EF6 154 and the EF6 105 Var6 presets.
- **Every EX bug in inventory §3.3 and §3.5 that applies to this code is fixed.** Section 5 lists them.
- **Tag display.** The team/solo column is labelled correctly as the first trailing column. `[TeamChangeData]` is shown and editable, with its MBAC meaning. For MBAACC files the editor states that the section is not read.
- **Tests.** Three statically linked MinGW tools: `cmdfile_roundtrip`, `cmdfile_test` and `cmdfile_ui_smoke`. All pass, and all run from WSL through Windows interop.

### Commits (`feat/cmdfile`)

| Commit | What |
|---|---|
| `55392a8` | Parser, validator, notes, workspace, editor UI, profile setting, loader replacement, `OFN_NOCHANGEDIR`, tools and tests |
| `82ac69a` | BOF-only frame-data panels gated behind the Extended profile |
| `34db0be` | Merge `update/ex-mbac` (feat/undo). The editor claims `ShortcutContext::commands` and registers its own undo/redo/save handler |
| `d896e36` | Headless ImGui smoke test, plus an InputInt flag fix it found |

## 2. Engine facts used (MBAA.exe, IDA `212fe94c`)

`LoadCharacterCommandFile` (`0x46ba10`), `Parser_SkipWhitespaceAndComments` (`0x41e730`), `Command_CheckExComConditions` (`0x46d3d0`), `ExComCheck_TestOwnEffectBoxOverlap` (`0x46d080`) and Effect6 case 105 (`0x45e8d4`). Comments were added in the IDB, and it was saved.

- **Command region.**
  - Every line before the first line that starts with `END` (after leading whitespace) is a command, unless it is blank or starts with `//`.
  - Fields are read with `atol`, so missing fields become 0 and extra tokens are ignored.
  - The input column ends at the first *space* (`strstr(" ")`).
- **Lines end only at CR (0x0D).** An LF-only file merges command lines. The validator reports this as an error in the command region.
- **Flagsets.**
  - Flagset 1: `value % 1e8 / 1e7` is the cancel class, and the remaining seven digits are bits 0-6.
  - Flagset 2: eight digits give bits 0-6. Bit 7 is not read.
  - Flagset 2 bit 5 (whiff cancel only) also sets Flagset 1 bit 3 and Flagset 2 bit 3 (`|= 0x2808`).
- **Trailing bytes.** The three trailing bytes are +36 team/solo (`Command_CheckCmdVars`: 1 = team only, 2 = solo only), +37 projectile limit ("tobi") and +38 air-dash limit.
- **Duplicate IDs:** the first definition wins, and later records are freed.
- **After END**, values are read by *global* key lookup (`Num`, `%03d_CheckNum`, `%03d_Type`, `%03d_P0..P4`, `AirJumpNum`, `Guard`, `Flags`, ...). MBAACC has no `TeamChangeData` token.
- **ExComCheck.**
  - Every row whose CheckNum equals the command ID must pass (AND).
  - Type 0: own collision box (P0 = 0) or hurtboxes (P0 = 1) against box #(P1+8) of a live effect owned by the character. P1 must be at least 1.
  - Type 1: a range of `P0 >> 6` against the same effect box. P2 is the range shape: 0 gives -r..2r, 1 gives 0..r, 2 gives -r..r, and 1 and 2 swap when facing left.
  - **Any other type returns 0**, so a vanilla game never allows the command.
- **EF6 105.** Mode `p3 % 10` is set (0) or add (1). When `p3 / 10 != 0` the write goes to the actor itself instead of its owner. **Modes 10/11 are vanilla.** Inventory §3.8 left this unverified; the vanilla combo already had them, and they stay ungated.

## 3. What was ported, and how it changed

| EX (`1abc27f9`) | Ours | Change |
|---|---|---|
| `mbaacc_command_file.*`: `Document` = raw bytes plus records, reparsed after every edit | `cmdfile/cmd_document.*` | Line list with per-line terminator and a stable per-line uid. Records are derived views that keep their identity across edits. Engine-accurate classification (section 2). Malformed command rows are recognised, and a `[Section]` before END is a command to the engine. CP932-aware `]` and `//` scanning. Edits keep column alignment. `[TeamChangeData]` in both the MBAACC form (`test = a, b`) and the MBAC form (`a b`). MBAC/MBAACC dialect detection. |
| `validateForSave` (BOF rules always on) | `cmdfile/cmd_validate.*` | Profile-aware, with Error/Warning/Info severity. The ExComCheck type registry is rewritten from IDA (Types 0/1), and Types 2/3 appear only under Extended. |
| `mbaacc_command_workspace.*` (`WorkspaceHistory`) | `cmdfile/cmd_workspace.*` | The same snapshot history (capacity, rollback, redo cleared on a new step). Adds `Workspace` with the save pipeline, revert (undoable), no-op edit suppression, a changed-line count and external-change detection. |
| `mbaacc_command_metadata.*` (`.notes` JSON: visual order, rule titles, positional ids) | `cmdfile/cmd_notes.*` | Free-text notes on commands, ExComChecks and comment lines, keyed by content (section 5). EX's visual-order overlay is not ported: reordering here moves the real lines. |
| `mbaacc_excom_registry.*` | inside `cmd_validate.cpp` | Type 0/1 meanings corrected from the engine. EX called Type 1 P0 "floor(P0/64)*2 / *4", which is not right. |
| `mbaacc_command_browser.cpp` (1,635 lines, 50 macros into `MainFrame`, multi-host dock plumbing, JSONL traces) | `cmdfile/cmd_editor_ui.*` + `ui/main_frame_cmdfile.cpp` | A self-contained `CommandFileEditor` with one ImGui window per open file. No `MainFrame` state, docking plumbing or traces. `MainFrame` holds one member and four small methods. |
| `mbaacc_command_check_cli.cpp`, `tests/mbaacc_command_workspace_test.cpp`, fixtures | `tests/cmdfile_test.cpp`, `cmdfile/cmdfile_roundtrip.cpp`, `tests/fixtures/cmdfile/ex_*` | Ported (section 6). The EX fixtures were restored to CRLF, because the patch transport had stripped the CRs. |
| BOF panels in `frame_disp_if.h` / `effect_misc.h` | `frame_disp/bof_extensions.h` + small hooks | Profile-gated. The IF combo gets 152-157 only under Extended. |

### Editor features

Open the editor from **File > Command File Editor (active character)** or **File > Open Command File...**. **File > Load Commands (_c.txt)...** now uses the new parser to feed command names to the IF panels.

- **Commands tab.** Every non-blank line of the command region, in file order:
  - Commands: an editable table with ID, input, both flagsets, pattern, meter, team/solo, projectile limit, dash, ExComCheck count, comment and note.
  - Comments.
  - Commented-out commands: greyed out, with "Restore as active command".
  - Malformed rows: red.
  - Duplicate IDs are shown in orange.
  - Row actions: double-click a cell to edit it. Ctrl/Shift multi-select. Drag rows to reorder (drop above or below). Arrow buttons and Alt+Up/Down. Insert command or comment above or below. Duplicate (gets the lowest free ID). Comment out. Delete. A right-click menu offers the same actions.
  - Search matches ID, input, pattern, comment and note.
  - Detail panel: flagset bit toggles with names, a cancel-class combo, a team/solo combo, a decoded projectile limit, and the linked ExComChecks with "+ Add check".
- **ExComChecks tab.**
  - Table of row number, CheckNum, Type, P0-P4, a decoded meaning, comment and note.
  - Duplicate, delete, move by arrows or drag, and a "linked to the selected command" filter.
  - Detail panel: a type combo (profile-gated), named parameters with engine help, and add or remove for optional P fields.
- **Linked selection.** Selecting a command highlights its checks. Selecting a check selects and scrolls to its command and highlights the other commands with that ID. The ExCC count opens the checks tab.
- **Tag data tab.** Lists the team-only and solo-only commands, and edits `[TeamChangeData]` tag-in/tag-out (section 7).
- **Other sections tab.** Everything after END, read-only and syntax coloured.
- **Problems tab.** Diagnostics for the active profile. Click one to jump to its row.
- **Undo, redo and save.**
  - Undo and redo are per file. Ctrl+Z, Ctrl+Y and Ctrl+S go through the app `ShortcutRouter` (`ShortcutContext::commands` plus a context handler), so they never reach the character's HA6 history. Text fields keep native Ctrl+Z.
  - Edits are staged. "Save..." shows a review (changed lines, notes changed, profile, warnings) and is disabled while errors remain. Revert returns to the saved state and can be undone.
  - Closing a window with unsaved changes asks for confirmation.
- **IF panels stay in sync.** Staged edits refresh `FrameData::m_commands` for every character whose command table came from that file, so IF 11/31/35 command names follow along.

## 4. Round-trip results

Produced with `build/cmdfile_roundtrip.exe --markdown FILE...`. Columns:

- **Round trip**: `serialize(parse(bytes)) == bytes`, plus a stable reparse (same command and check counts).
- **Vanilla E/W** and **Extended E/W**: validator errors and warnings per profile (Info is not counted).

`cmdfile_test` additionally runs the per-file structural suite on every one of these files (section 6). All pass.

### Totals

| Set | Files | Byte-identical | Vanilla errors | Commands (engine) | Old loader rows |
|---|---:|---:|---:|---:|---:|
| MBAACC `C:\games\mbaacc_tag\data\*_c.txt` | 116 | **116** | **0** | 5,553 | 5,832 (+279 bogus: ExComCheck keys, `[TeamChangeData]` pairs, ...) |
| MBAC `MBACPC\02_extracted\*_C.TXT` | 24 | **24** | **0** | 1,047 | 1,594 (+547 bogus: blocking, throw and system tables) |
| MBAC `docs/tag_research/mbac_data/cp932/02` | 24 | **24** | **0** | same files (byte-equal to 02_extracted) | |
| EX fixtures `tests/fixtures/cmdfile` | 7 | **7** | see below | | |

"Old loader rows" is how many entries the previous `FrameData::load_commands` created. It accepted any line that began with a number, anywhere in the file.

The only warnings across all shipped files are in MBAC `KISHIMA_C.TXT`, lines 42-43: commands 161/162 have Flagset 1 `10210010` / `20210010`, a digit 2 in a bit position (an original data typo). Under the Vanilla profile these are warnings; under Extended they are errors, because the BOF rule requires 0/1 bits. There are no duplicate command IDs in any shipped file. MBAACC `ryougi_0_c.txt`/`b_ryougi_9_c.txt` command 170 has a tenth column, which is reported as Info.

### EX fixtures

The BOF fixtures use ExComCheck Type 2. They fail Vanilla, because vanilla MBAACC blocks those commands, and pass Extended with no errors. The two `invalid_*` fixtures fail both profiles on `Num`, as EX intended.

| File | Dialect | Bytes | Lines | Commands | Commented | ExComChecks | TeamChange | Round trip | Vanilla E/W | Extended E/W |
|---|---|---:|---:|---:|---:|---:|---|---|---|---|
| ex_bof_angel_0_c.txt | MBAACC | 25449 | 628 | 111 | 4 | 27 | 235/236 | identical | 27/0 | 0/0 |
| ex_bof_kyo_lo_0_c.txt | MBAACC | 24971 | 615 | 108 | 14 | 25 | 235/236 | identical | 25/0 | 0/0 |
| ex_bof_shermie_0_c.txt | MBAACC | 28785 | 718 | 112 | 22 | 33 | 235/236 | identical | 33/0 | 0/0 |
| ex_invalid_missing_row_c.txt | MBAACC | 99 | 8 | 1 | 0 | 1 | - | identical | 1/1 | 1/1 |
| ex_invalid_outside_num_c.txt | MBAACC | 131 | 10 | 1 | 0 | 2 | - | identical | 1/2 | 1/2 |
| ex_vanilla_arc_0_c.txt | MBAACC | 7404 | 170 | 78 | 18 | 0 | 235/236 | identical | 0/0 | 0/0 |
| ex_vanilla_ryougi_1_c.txt | MBAACC | 8105 | 210 | 93 | 6 | 6 | 235/236 | identical | 0/0 | 0/0 |

### MBAACC (all 116 files)

| File | Dialect | Bytes | Lines | Commands | Commented | ExComChecks | TeamChange | Round trip | Vanilla E/W | Extended E/W |
|---|---|---:|---:|---:|---:|---:|---|---|---|---|
| Hermes_0_c.txt | MBAACC | 1525 | 50 | 11 | 1 | 0 | - | identical | 0/0 | 0/0 |
| akaakiha_0_c.txt | MBAACC | 5310 | 141 | 56 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| akaakiha_1_c.txt | MBAACC | 5444 | 142 | 56 | 12 | 0 | 241/242 | identical | 0/0 | 0/0 |
| akaakiha_2_c.txt | MBAACC | 5018 | 131 | 50 | 12 | 0 | 241/242 | identical | 0/0 | 0/0 |
| akiha_0_c.txt | MBAACC | 5625 | 141 | 58 | 15 | 0 | 177/178 | identical | 0/0 | 0/0 |
| akiha_1_c.txt | MBAACC | 5270 | 138 | 44 | 21 | 0 | 177/178 | identical | 0/0 | 0/0 |
| akiha_2_c.txt | MBAACC | 5292 | 136 | 50 | 16 | 0 | 177/178 | identical | 0/0 | 0/0 |
| aoko_0_c.txt | MBAACC | 6847 | 172 | 72 | 20 | 0 | 177/178 | identical | 0/0 | 0/0 |
| aoko_1_c.txt | MBAACC | 6350 | 166 | 58 | 26 | 0 | 177/178 | identical | 0/0 | 0/0 |
| aoko_2_c.txt | MBAACC | 5979 | 156 | 68 | 11 | 0 | 177/178 | identical | 0/0 | 0/0 |
| arc_0_c.txt | MBAACC | 7404 | 170 | 78 | 18 | 0 | 235/236 | identical | 0/0 | 0/0 |
| arc_1_c.txt | MBAACC | 6848 | 171 | 61 | 25 | 0 | 235/236 | identical | 0/0 | 0/0 |
| arc_2_c.txt | MBAACC | 6925 | 170 | 76 | 12 | 0 | 235/236 | identical | 0/0 | 0/0 |
| b_Hermes_9_c.txt | MBAACC | 1465 | 49 | 11 | 0 | 0 | - | identical | 0/0 | 0/0 |
| b_akiha_8_c.txt | MBAACC | 5349 | 135 | 50 | 17 | 0 | 177/178 | identical | 0/0 | 0/0 |
| b_aoko_9_c.txt | MBAACC | 5924 | 154 | 68 | 10 | 0 | 177/178 | identical | 0/0 | 0/0 |
| b_arc_9_c.txt | MBAACC | 4731 | 117 | 54 | 5 | 0 | 235/236 | identical | 0/0 | 0/0 |
| b_kohaku_m_9_c.txt | MBAACC | 4796 | 123 | 36 | 24 | 0 | 235/236 | identical | 0/0 | 0/0 |
| b_m_hisui_p_9_c.txt | MBAACC | 791 | 32 | 0 | 0 | 0 | 241/242 | identical | 0/0 | 0/0 |
| b_miyako_9_c.txt | MBAACC | 5137 | 133 | 45 | 18 | 0 | 241/242 | identical | 0/0 | 0/0 |
| b_ryougi_9_c.txt | MBAACC | 8049 | 200 | 99 | 5 | 3 | 235/236 | identical | 0/0 | 0/0 |
| b_warakia_9_c.txt | MBAACC | 4354 | 118 | 46 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| b_wlen_9_c.txt | MBAACC | 4987 | 133 | 44 | 21 | 0 | 241/242 | identical | 0/0 | 0/0 |
| ciel_0_c.txt | MBAACC | 9056 | 213 | 93 | 28 | 0 | 235/236 | identical | 0/0 | 0/0 |
| ciel_1_c.txt | MBAACC | 5823 | 148 | 60 | 13 | 0 | 235/236 | identical | 0/0 | 0/0 |
| ciel_2_c.txt | MBAACC | 7319 | 186 | 91 | 7 | 0 | 235/236 | identical | 0/0 | 0/0 |
| g_akiha_9_c.txt | MBAACC | 2022 | 71 | 14 | 4 | 0 | 235/236 | identical | 0/0 | 0/0 |
| hisui_0_c.txt | MBAACC | 6469 | 150 | 62 | 26 | 0 | 235/236 | identical | 0/0 | 0/0 |
| hisui_1_c.txt | MBAACC | 6415 | 155 | 65 | 19 | 0 | 235/236 | identical | 0/0 | 0/0 |
| hisui_2_c.txt | MBAACC | 6296 | 150 | 61 | 23 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kishima_0_c.txt | MBAACC | 6131 | 143 | 61 | 18 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kishima_1_c.txt | MBAACC | 5751 | 133 | 55 | 16 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kishima_2_c.txt | MBAACC | 4898 | 118 | 57 | 4 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_0_c.txt | MBAACC | 5403 | 125 | 54 | 14 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_1_c.txt | MBAACC | 7448 | 172 | 61 | 37 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_2_c.txt | MBAACC | 5677 | 141 | 56 | 17 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_m_0_c.txt | MBAACC | 5657 | 134 | 41 | 33 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_m_1_c.txt | MBAACC | 5496 | 132 | 41 | 26 | 0 | 235/236 | identical | 0/0 | 0/0 |
| kohaku_m_2_c.txt | MBAACC | 4343 | 111 | 44 | 7 | 0 | 235/236 | identical | 0/0 | 0/0 |
| len_0_c.txt | MBAACC | 4297 | 112 | 46 | 8 | 0 | 241/242 | identical | 0/0 | 0/0 |
| len_1_c.txt | MBAACC | 4687 | 121 | 45 | 13 | 0 | 241/242 | identical | 0/0 | 0/0 |
| len_2_c.txt | MBAACC | 4372 | 116 | 46 | 8 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_0_c.txt | MBAACC | 6725 | 153 | 55 | 33 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_1_c.txt | MBAACC | 4234 | 110 | 34 | 14 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_2_c.txt | MBAACC | 5029 | 124 | 50 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_m_0_c.txt | MBAACC | 3591 | 99 | 37 | 5 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_m_1_c.txt | MBAACC | 3348 | 96 | 34 | 2 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_m_2_c.txt | MBAACC | 3568 | 101 | 39 | 1 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_p_0_c.txt | MBAACC | 796 | 34 | 0 | 0 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_p_1_c.txt | MBAACC | 796 | 34 | 0 | 0 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_p_2_c.txt | MBAACC | 796 | 34 | 0 | 0 | 0 | 241/242 | identical | 0/0 | 0/0 |
| m_hisui_p_9_c.txt | MBAACC | 5645 | 137 | 51 | 22 | 0 | 241/242 | identical | 0/0 | 0/0 |
| miyako_0_c.txt | MBAACC | 5375 | 134 | 51 | 16 | 0 | 241/242 | identical | 0/0 | 0/0 |
| miyako_1_c.txt | MBAACC | 5826 | 148 | 55 | 19 | 0 | 241/242 | identical | 0/0 | 0/0 |
| miyako_2_c.txt | MBAACC | 4935 | 125 | 49 | 12 | 0 | 241/242 | identical | 0/0 | 0/0 |
| miyako_9_c.txt | MBAACC | 4855 | 125 | 46 | 14 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nanaya_0_c.txt | MBAACC | 5161 | 127 | 39 | 26 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nanaya_1_c.txt | MBAACC | 6271 | 161 | 47 | 32 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nanaya_2_c.txt | MBAACC | 4690 | 117 | 45 | 12 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nechaos_0_c.txt | MBAACC | 4674 | 118 | 53 | 10 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nechaos_1_c.txt | MBAACC | 4374 | 116 | 44 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nechaos_2_c.txt | MBAACC | 4837 | 122 | 49 | 13 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_0_c.txt | MBAACC | 6939 | 233 | 57 | 17 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_1_c.txt | MBAACC | 6050 | 224 | 50 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_2_c.txt | MBAACC | 5840 | 213 | 50 | 5 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_p_0_c.txt | MBAACC | 6066 | 219 | 54 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_p_1_c.txt | MBAACC | 6306 | 224 | 54 | 10 | 0 | 241/242 | identical | 0/0 | 0/0 |
| neco_p_2_c.txt | MBAACC | 6068 | 220 | 54 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nero_0_c.txt | MBAACC | 4072 | 101 | 37 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nero_1_c.txt | MBAACC | 3707 | 95 | 33 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| nero_2_c.txt | MBAACC | 4030 | 99 | 40 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| p_arc_0_c.txt | MBAACC | 6250 | 156 | 62 | 17 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_1_c.txt | MBAACC | 6051 | 153 | 55 | 21 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_2_c.txt | MBAACC | 5820 | 146 | 59 | 13 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_9_c.txt | MBAACC | 6112 | 153 | 61 | 16 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_D_0_c.txt | MBAACC | 864 | 35 | 0 | 0 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_D_1_c.txt | MBAACC | 864 | 35 | 0 | 0 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_D_2_c.txt | MBAACC | 864 | 35 | 0 | 0 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_arc_D_9_c.txt | MBAACC | 864 | 35 | 0 | 0 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_ciel_0_c.txt | MBAACC | 6527 | 169 | 50 | 28 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_ciel_1_c.txt | MBAACC | 5287 | 136 | 48 | 10 | 0 | 235/236 | identical | 0/0 | 0/0 |
| p_ciel_2_c.txt | MBAACC | 5136 | 132 | 51 | 5 | 0 | 235/236 | identical | 0/0 | 0/0 |
| ries_0_c.txt | MBAACC | 5670 | 139 | 43 | 30 | 0 | 241/242 | identical | 0/0 | 0/0 |
| ries_1_c.txt | MBAACC | 3900 | 107 | 31 | 13 | 0 | 241/242 | identical | 0/0 | 0/0 |
| ries_2_c.txt | MBAACC | 3858 | 104 | 40 | 3 | 0 | 241/242 | identical | 0/0 | 0/0 |
| ries_9_c.txt | MBAACC | 5666 | 139 | 35 | 38 | 0 | 241/242 | identical | 0/0 | 0/0 |
| roa_0_c.txt | MBAACC | 5363 | 138 | 42 | 23 | 0 | 235/236 | identical | 0/0 | 0/0 |
| roa_1_c.txt | MBAACC | 4254 | 119 | 34 | 11 | 0 | 235/236 | identical | 0/0 | 0/0 |
| roa_2_c.txt | MBAACC | 4244 | 119 | 39 | 7 | 0 | 235/236 | identical | 0/0 | 0/0 |
| ryougi_0_c.txt | MBAACC | 11045 | 265 | 138 | 14 | 6 | 235/236 | identical | 0/0 | 0/0 |
| ryougi_1_c.txt | MBAACC | 8105 | 210 | 93 | 6 | 6 | 235/236 | identical | 0/0 | 0/0 |
| ryougi_2_c.txt | MBAACC | 8963 | 228 | 108 | 6 | 6 | 235/236 | identical | 0/0 | 0/0 |
| s_akiha_0_c.txt | MBAACC | 6794 | 165 | 57 | 36 | 0 | 235/236 | identical | 0/0 | 0/0 |
| s_akiha_1_c.txt | MBAACC | 5204 | 145 | 40 | 24 | 0 | 235/236 | identical | 0/0 | 0/0 |
| s_akiha_2_c.txt | MBAACC | 5117 | 138 | 45 | 16 | 0 | 235/236 | identical | 0/0 | 0/0 |
| satsuki_0_c.txt | MBAACC | 4761 | 120 | 43 | 17 | 0 | 241/242 | identical | 0/0 | 0/0 |
| satsuki_1_c.txt | MBAACC | 4221 | 111 | 39 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| satsuki_2_c.txt | MBAACC | 4060 | 111 | 40 | 8 | 0 | 241/242 | identical | 0/0 | 0/0 |
| shiki_0_c.txt | MBAACC | 5017 | 124 | 47 | 16 | 0 | 241/242 | identical | 0/0 | 0/0 |
| shiki_1_c.txt | MBAACC | 4304 | 116 | 35 | 14 | 0 | 241/242 | identical | 0/0 | 0/0 |
| shiki_2_c.txt | MBAACC | 4078 | 107 | 40 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| sion_0_c.txt | MBAACC | 5491 | 136 | 52 | 19 | 0 | 241/242 | identical | 0/0 | 0/0 |
| sion_1_c.txt | MBAACC | 4925 | 126 | 41 | 19 | 0 | 241/242 | identical | 0/0 | 0/0 |
| sion_2_c.txt | MBAACC | 4890 | 131 | 45 | 15 | 0 | 241/242 | identical | 0/0 | 0/0 |
| v_sion_0_c.txt | MBAACC | 5777 | 146 | 39 | 36 | 0 | 241/242 | identical | 0/0 | 0/0 |
| v_sion_1_c.txt | MBAACC | 6033 | 152 | 40 | 38 | 0 | 241/242 | identical | 0/0 | 0/0 |
| v_sion_2_c.txt | MBAACC | 4866 | 122 | 41 | 19 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warakia_0_c.txt | MBAACC | 5689 | 146 | 56 | 20 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warakia_1_c.txt | MBAACC | 4697 | 125 | 46 | 12 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warakia_2_c.txt | MBAACC | 4566 | 120 | 51 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warc_0_c.txt | MBAACC | 5587 | 148 | 52 | 17 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warc_1_c.txt | MBAACC | 4793 | 128 | 42 | 15 | 0 | 241/242 | identical | 0/0 | 0/0 |
| warc_2_c.txt | MBAACC | 5250 | 138 | 54 | 9 | 0 | 241/242 | identical | 0/0 | 0/0 |
| wlen_0_c.txt | MBAACC | 6024 | 158 | 60 | 23 | 0 | 241/242 | identical | 0/0 | 0/0 |
| wlen_1_c.txt | MBAACC | 4923 | 139 | 48 | 14 | 0 | 241/242 | identical | 0/0 | 0/0 |
| wlen_2_c.txt | MBAACC | 5389 | 145 | 60 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |

### MBAC PC (`02_extracted`, 24 files)

| File | Dialect | Bytes | Lines | Commands | Commented | ExComChecks | TeamChange | Round trip | Vanilla E/W | Extended E/W |
|---|---|---:|---:|---:|---:|---:|---|---|---|---|
| AKAAKIHA_C.TXT | MBAC | 4880 | 150 | 47 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| AKIHA_C.TXT | MBAC | 5193 | 152 | 47 | 12 | 0 | 177/178 | identical | 0/0 | 0/0 |
| AOKO_C.TXT | MBAC | 6036 | 216 | 56 | 4 | 0 | 240/241 | identical | 0/0 | 0/0 |
| ARC_C.TXT | MBAC | 7447 | 246 | 62 | 17 | 0 | 235/236 | identical | 0/0 | 0/0 |
| CIEL_C.TXT | MBAC | 7273 | 218 | 80 | 9 | 0 | 243/244 | identical | 0/0 | 0/0 |
| DAMIEN_C.TXT | MBAC | 5252 | 194 | 45 | 4 | 0 | 241/242 | identical | 0/0 | 0/0 |
| G_CHAOS_C.TXT | MBAC | 3107 | 109 | 26 | 0 | 0 | - | identical | 0/0 | 0/0 |
| HISUI_C.TXT | MBAC | 5656 | 164 | 50 | 16 | 0 | 241/242 | identical | 0/0 | 0/0 |
| KISHIMA_C.TXT | MBAC | 4669 | 139 | 49 | 0 | 0 | 241/242 | identical | 0/2 | 2/0 |
| KOHAKU_C.TXT | MBAC | 4856 | 141 | 40 | 13 | 0 | 188/189 | identical | 0/0 | 0/0 |
| LEN_C.TXT | MBAC | 5167 | 196 | 40 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| MIYAKO_C.TXT | MBAC | 4497 | 139 | 40 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| M_HISUI_C.TXT | MBAC | 4884 | 141 | 47 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| NANAYA_C.TXT | MBAC | 3996 | 128 | 32 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| NECHAOS_C.TXT | MBAC | 5252 | 194 | 45 | 4 | 0 | 241/242 | identical | 0/0 | 0/0 |
| NECO_C.TXT | MBAC | 5297 | 197 | 47 | 3 | 0 | 241/242 | identical | 0/0 | 0/0 |
| NERO_C.TXT | MBAC | 3801 | 119 | 30 | 5 | 0 | 244/245 | identical | 0/0 | 0/0 |
| SATSUKI_C.TXT | MBAC | 4041 | 131 | 33 | 6 | 0 | 241/242 | identical | 0/0 | 0/0 |
| SHIKI_C.TXT | MBAC | 4118 | 126 | 29 | 11 | 0 | 241/242 | identical | 0/0 | 0/0 |
| SION_C.TXT | MBAC | 4490 | 139 | 40 | 7 | 0 | 241/242 | identical | 0/0 | 0/0 |
| V_SION_C.TXT | MBAC | 4638 | 146 | 30 | 19 | 0 | 241/242 | identical | 0/0 | 0/0 |
| WARAKIA_C.TXT | MBAC | 4312 | 137 | 41 | 5 | 0 | 241/242 | identical | 0/0 | 0/0 |
| WARC_C.TXT | MBAC | 5894 | 209 | 40 | 15 | 0 | 197/198 | identical | 0/0 | 0/0 |
| WLEN_C.TXT | MBAC | 5848 | 215 | 51 | 8 | 0 | 241/242 | identical | 0/0 | 0/0 |

## 5. EX bugs fixed while porting

| EX problem (inventory ref) | Fix |
|---|---|
| BOF rules and duplicate-ID checks block valid vanilla saves (§3.3 **H**) | Extension Profile, Vanilla by default. BOF-only checks are warnings under Vanilla and errors under Extended: Flagset 1 class 0-2, 0/1 bit digits, 8-digit flagsets, duplicate IDs (the engine keeps the first), and the Type 2/3 parameter rules. Type 2/3 rows are *errors* under Vanilla, because the engine fails any type other than 0/1 and the command becomes unusable. Every shipped file saves under Vanilla (section 4). |
| Backup name has second resolution, so a second save in the same second is refused (§3.3 **M**) | `WriteUniqueBackup`: `<dir>\.hantei-backups\<name>.<YYYYmmdd-HHMMSS>[-N].bak`, created with `CREATE_NEW`, adding a suffix until the name is free, then flushed. Test: two saves in the same second give two distinct backups, each holding the previous file. |
| Narrow-path `ofstream` + `MoveFileExA`, and `flush()` is not `FlushFileBuffers` (§3.3 **M**) | The command file and the notes file are both written with the existing `WriteFileAtomic` (`misc.cpp`: unique temp name, `FlushFileBuffers`, `MoveFileEx(REPLACE_EXISTING, WRITE_THROUGH)`). A save also refuses to overwrite a file that changed on disk since it was opened, unless the user confirms. The disk version is backed up first. |
| `rewriteDenseExComRows` renumbers `NNN_` keys in every section (§3.3 **M**) | `renumberExCom` touches only lines between `[ExComCheck]` and the next header, then updates `Num` (or inserts it). `NNN_` keys outside the section are left alone and reported as a warning, because the engine's global lookup can match them. Test: `000_Note` / `005_Label` in `[Other]` survive a delete. |
| SJIS trail-byte `]` in section detection (§3.3 **L**) | Header, comment and `//` scans skip CP932 trail bytes. There is a test for this. |
| Notes keyed by record position (§3.5) | In memory, a note is attached to the record's line uid, so it follows reorders and inserts. On disk (`<file>.notes.json`) it is keyed by content: `command:<ID>#<n>` (n-th active command with that numeric ID) or `excom:<CheckNum>#<n>`. Notes that no longer match are kept as `orphaned` and written back, never dropped. A note on a commented-out line is kept as `line:N`. Test: a note on command 12 survives moving 12 to the top and inserting another command before it. |
| A malformed notes file is silently discarded and later overwritten (§3.5) | `ParseNotes` returns an error for bad JSON, a wrong format tag, a wrong version or malformed entries. The editor shows the error, disables note editing, and never writes the notes file. The command file can still be saved. There is a test. |
| Always-on JSONL traces, and `FileDialog` without `OFN_NOCHANGEDIR` (§3.9) | There are no trace writers in the port. `FileDialog` now always sets `OFN_NOCHANGEDIR` (inventory A7), so the working directory no longer follows the last picked folder. |
| No-op edits create undo steps (EX records before every attempt) | `Workspace::apply` drops the step when neither the bytes nor the notes changed, and rolls back failed edits. |
| Moving an ExComCheck row separates it from its heading comment | The comment/blank block directly above a row moves with it. Moving a row away and back is byte-identical, which is tested on the BOF fixtures. |

### Behaviour kept from EX, with its limits

- Command-field edits replace tokens in place and keep columns aligned where the padding allows.
- New rows copy the neighbouring row's layout.
- New lines use the file's own terminator style (CRLF for every shipped file).

## 6. Tests and tools

All three tools are MinGW, statically linked (`-static-libgcc -static-libstdc++ -static`) and built by `./build.sh`.

| Target | What it does |
|---|---|
| `cmdfile_roundtrip.exe [--markdown] [--diagnostics] FILE...` | Byte round trip plus a validation summary. It produced section 4. |
| `cmdfile_test.exe [FIXTURE_DIR] [FILE...]` | 272 checks on the fixtures (3,616 with every MBAACC and MBAC file added). See the list below. |
| `cmdfile_ui_smoke.exe FILE...` | Draws every editor tab for both profiles, with and without a selection, headless (ImGui without a renderer). ImGui is compiled with `IM_ASSERT` enabled (`tests/imgui_assert_config.h`). It found an `InputInt` + `EnterReturnsTrue` misuse, which is now fixed. |

**What `cmdfile_test` covers:**

- **`WorkspaceHistory`** (EX's `mbaacc_command_workspace_test`): undo, redo, capacity, rollback and clear. Also workspace apply, undo, redo, rollback of failed edits and undoable revert.
- **Per-file structural suite** (EX `--structural` and `--mutation`, tightened to byte-exact inverses). Each of these must round-trip to the original bytes:
  - a surgical field edit touches exactly one line;
  - comment-out and uncomment;
  - insert and delete a command;
  - insert and edit a comment line;
  - move a command away and back;
  - batch move;
  - move an ExComCheck away and back;
  - insert and delete an ExComCheck.
- **Regressions**: section-scoped renumbering; duplicate IDs (Vanilla warning, Extended error); BOF Type 2 gating and the P4 rule; LF-only detection; malformed rows; CP932 trail bytes; a file with no final EOL, including creating an `[ExComCheck]` section; MBAC TeamChangeData edits; column alignment.
- **Notes**: stable keys across reorder and insert; orphans preserved; malformed JSON and a foreign format rejected.
- **Save pipeline** in a temp folder: a broken notes file is left untouched; unique backups on two saves in the same second; notes written with the file; external change detected; an explicit overwrite works; a validation error blocks the write and leaves the file unchanged.

**Running.** WSL2 runs these `.exe` files directly through Windows interop, so no wine is needed:

```sh
./build.sh
./build/cmdfile_test.exe 'tests\fixtures\cmdfile'               # 272 checks
./build/cmdfile_ui_smoke.exe 'tests\fixtures\cmdfile\ex_vanilla_ryougi_1_c.txt' 'tests\fixtures\cmdfile\ex_bof_angel_0_c.txt'
./build/cmdfile_roundtrip.exe --markdown $(for f in /mnt/c/games/mbaacc_tag/data/*_c.txt; do wslpath -w "$f"; done)
# the structural suite over the game data (copy first; the tool only reads, but keep the data read-only):
./build/cmdfile_test.exe 'tests\fixtures\cmdfile' $(for f in /path/to/copy/*_c.txt; do wslpath -w "$f"; done)
```

- File arguments must be Windows paths (`wslpath -w`).
- On Windows, run the same commands from the repo root in `cmd` or PowerShell.
- `ctest` registers `cmdfile` and `cmdfile_ui_smoke` next to `shortcut_router` and `undo_manager`, with the working directory set to the source root.
- `wine` 9.0 is installed on this WSL host, but its prefix is broken (`could not load kernel32.dll`). Use interop instead.

## 7. Tag-related display

- **Team/solo column.**
  - It is labelled "Team/solo" as the *first* of the three trailing columns (`0` both, `1` team only, `2` solo only), with the engine reference in the tooltip.
  - The detail panel uses a combo, and the file's own header ("ｶｳﾝﾀｰ時間") is explained.
  - "Projectile limit" (tobi, 飛び道具制限) is decoded as `var<tens> < <ones>`.
  - The Tag data tab lists the team-only and solo-only commands. Hisui has 40-42 as team-only ("琥珀への指示") and 50-53 as solo-only.
- **`[TeamChangeData]`.**
  - Both forms are parsed and editable in place: MBAC `241 242` and MBAACC `test = 235, 236`. Neco's MBAACC file uses the MBAC form.
  - MBAC files: the text says these are the tag-in/tag-out patterns, compiled into `_C.CT` at 0x2280/0x2284.
  - MBAACC files: a warning banner says MBAACC does not read the section (there is no token in `LoadCharacterCommandFile`), with a pointer to `STATE_COMPARISON_MBAC_vs_MBAACC.md` §5 for the correct numbers. The validator adds an Info diagnostic.
  - The round-trip tables list the pair for every file. They match STATE_COMPARISON (Hisui MBAC 241/242, Kohaku 188/189, WArc 197/198, Nero 244/245, Aoko 240/241, Ciel 243/244), and every stale MBAACC 235/236 is visible.

## 8. Extension Profile details

- Stored in the ImGui ini as `[Extension profile][] Profile=0|1`. `src/extension_profile.*` adds its own settings handler, registered from `InitIni` (one line).
- **Vanilla** (default): no BOF IDs anywhere. BOF-only `_c` rules are warnings, and ExComCheck Type 2/3 rows are errors.
- **Extended**:
  - IF combo entries 152-157 (152/153 are labelled as no-ops).
  - IF 154-157 panels and the IF 14 "collider filters" tree.
  - EF6 154 in the sub-type list and its panel.
  - EF6 105 Var6 "Level 1/2/3" presets.
  - ExComCheck Types 2/3 in the type combo, with EX's ABI v1 parameter rules as errors.
  - Duplicate IDs and flag-digit rules as errors.
- EF6 105 modes 10/11 are **not** gated: IDA shows they are vanilla (section 2).
- The panels live in `frame_disp/bof_extensions.h`. The hooks in `frame_disp_if.h` and `effect_misc.h` are about six lines each.

## 9. Known gaps

- **The GUI has not been exercised interactively.** It was verified by the headless smoke test (layout, tabs, both profiles, selection) and by the document-level tests that the UI calls. Drag-drop, inline editing focus and the modals still need a manual pass in the app.
- **EX's "visual order" overlay** (a display-only reorder stored in the sidecar) and **rule titles** were not ported. Reordering changes the file, and notes replace titles.
- **Multi-host and detached windows**: each file is a normal dockable ImGui window. EX's workspace-session and dock-tree plumbing depends on phase G4.
- **Paths are ANSI** (`CreateFileA`, `WriteFileAtomic`, `FileDialog`), like the rest of the app. Non-CP932 folder names are not supported.
- **Deleting an ExComCheck** removes its field lines but not its heading comment. A move does carry the comment.
- **New commands are not checked against HA6.** The editor does not check that the pattern exists in the loaded HA6, or that CheckNum refers to a command with a matching input.
- **The MBAC tail tables** (blocking table, throw info, system row) are shown read-only in "Other sections" without labels. The `_C.CT` compiled form is not produced; MBAC reads only the `.CT`.
- **The command file does not auto-load when a character loads.** Use the File menu items. Opening the editor for the active character also loads its table.
- **`annotation_store` (EX's HA6 pattern notes, inventory F4) is not part of this port.** The notes fixes here apply to command-file notes only.
- **Labels.** IF/EF labels from inventory §7 (D3) were already applied in `236eb5b` and were not changed here.
