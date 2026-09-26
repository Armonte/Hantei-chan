# Hantei-chan Tag / Team panel (EXPERIMENTAL)

> **Superseded by Authoring Mode (2026-09-25).** The per-character sidecar files (`povertycaster\tag\`) replace `tag_tuning.ini`, and the **Experimental: Authoring** workspace replaces this panel: *Windows > Tag / Team (experimental)* now opens it on its Tuning tab. This panel stays reachable as *Experimental: Authoring > Legacy tag_tuning.ini panel*, and with `--tag-ini`, for a `tag_tuning.ini` that has not been migrated yet. See `docs/HANTEI_AUTHORING_MODE.md` (§11 describes what was built).

**Windows > Tag / Team (experimental)** edits the running MBAACC's `tag_tuning.ini` (PovertyCaster's native TAG levers) while you play:
* the styles, and the global levers;
* per-character entries;
* the directional assist slots.

It also shows the live tag state over the Game Link.

| | |
|---|---|
| Branch | `feat/tagpanel` in `/mnt/c/dev/hantei-chan/wt/tagpanel` (update/ex-mbac + feat/stage-link) |
| Commits | see §8 |
| Model | `src/tag_tuning/` (`tag_levers.h`, `tag_ini.{h,cpp}`, `tag_assist.{h,cpp}`) |
| UI | `src/tag_panel.{h,cpp}`, glue in `src/ui/main_frame_gamelink.cpp` |
| Test | `tests/tag_tuning_test.cpp` (`tag_tuning_test.exe`), fixtures in `tests/fixtures/tag/` |
| Authority | PovertyCaster `pc-adapters/mbaacc/include/mbaacc/MbaaccTagTuning.hpp` + `MbaaccTagAssist.hpp` (main `5f043727`) |
| User guide for the levers | `docs/tag_research/TAG_TUNING_GUIDE.md` |

Evidence: full-size UI captures and the capture ini are in `docs/tag_panel/`. In the repo, `Hantei_Docs/update_2026_09/tag_panel/` holds a contact sheet (`tag_panel_sheet.png`), the capture ini and its md5.

---

## 1. How it drives the game

* **Applying is saving.** Offline, the game stats `tag_tuning.ini` next to `MBAA.exe` about once a second, and re-reads it when the mtime or size changes. It also re-reads it after every character reload.
  * So **Apply to game** is an atomic save: `WriteFileAtomic`, a temp file plus `MoveFileEx`, so the game never reads a half-written file. The previous bytes are kept as `tag_tuning.ini.bak`, as the F3 panel does.
* **auto-apply** saves 0.4 s after each edit. You tweak a slider, and the next second of play uses it.
* **+ reload characters** also sends a Game Link `Reload` after the save, so HA6 edits (a tag-in pattern's timing, velocity or cancel flags) land together with the ini. The reload is sent only when the link reports an offline battle.
* **Where the file is:**
  * By default, `<game folder>\tag_tuning.ini`. The game folder comes from the linked MBAA.exe's pid (`gamelink::GameDirOf`), or from the *game folder* box.
  * **Open ini...** (or `--tag-ini`) edits any file.
  * A missing file is fine: it means "all defaults", and the file is created on the first save.
* **External changes.** The panel stats the file once a second.
  * With no unsaved edits, it reloads silently.
  * With unsaved edits, it asks: *Load theirs* or *Keep mine*. The in-game F3 panel's *Save to ini* rewrites the whole file, so this is a real case.
  * A save is refused while that question is open.

## 2. The ini editor (byte-preserving)

`tagtune::TagIni` keeps the file as raw bytes: every line with its own terminator.
* An edit rewrites only the value text of one key. Spacing and a trailing `; comment` stay.
* Otherwise an edit inserts or removes whole lines.
* Result: an unedited save is byte-identical, and comments, unknown keys, CP932 bytes, CRLF and a missing final newline all survive.

The syntax and the "which occurrence counts" rules are the game's own (`parseTuningIni`):

| Section | Scope the editor reads and edits |
|---|---|
| `[tuning]` | every `[tuning]` section in file order; the later key wins (`active_style` too) |
| `[char.<file>]` | every section of that name, merged; the later key wins; new sections are written lower-case |
| `[style.<name>]` | only the **first** section of that name (`findIniStyle`) |

* Comments: `;` or `#` at the start of a line, or after a space/tab.
* Section names and keys are case-insensitive.

**Placement of edits:**
* A new key goes right after the last key line of its section: the last instance for `[tuning]`/`[char]`, the first instance for `[style]`.
* A new section goes at the end of the file.
* *Clear overrides* removes a `[char]` header and its key lines (and one blank line before the header); comment lines stay.

**Panel operations** mirror the F3 panel, but are surgical:
* **Pick a style:** sets `active_style`. With *picking a style drops [tuning] overrides* (on by default, like F3 `iniSelectStyle`), the `[tuning]` **lever** keys are removed; unknown keys stay.
* **Save as style:** `[style.<name>]` gets every lever that differs from the defaults (F3 `iniSaveAsStyle`), the style becomes active, and the `[tuning]` lever keys go.
* **Global lever edit:** writes the key into `[tuning]`. **x** removes the override, so the value falls back to the style's.

## 3. The lever table: one copied table, pinned to the source

`src/tag_tuning/tag_levers.h` is a copy of `kLevers`:
* same order;
* key, group, kind, scope, range, the `Tuning{}` default, the choice names, help and `perChar`;
* the enum name tables;
* `packMotion`/`unpackMotion`, `parseLeverValue`, `formatLeverValue`;
* `kBuiltinStyles`;
* `kTeamChangeTable` (for the "0 = table" hint).

A comment at its top points at the header and commit.

Hantei-chan does not build against PovertyCaster; this is the same arrangement as `game_link_proto.h`. The drift is caught by the test (§6):
* When `PC_TAG_TUNING_HPP` (a CMake cache path, defaulting to the PovertyCaster checkout) exists at configure time, `tag_tuning_test` **includes the real header**. That works because the header is pure standard C++. The test then compares:
  * every row, default, choice name and help string, and the built-in styles;
  * the Sim+SimBoot field count against `tuningBlockFields()` (41);
  * `parseLeverValue`/`formatLeverValue` on 35 tricky inputs × 59 levers;
  * the **full resolution** (global, per character, style list, warning count) of the sample and of four edited files, through both parsers.
* Without the header, it checks the table against the reference comment list in `tag_tuning.sample.ini`.

**Resolution** mirrors the game's: defaults ← the active style (ini section first, then built-in) ← `[tuning]` keys ← per character `[char.<file>]` (perChar levers only).
* An unknown key, a bad or out-of-range value, a CharOnly key in `[tuning]` or a team-wide key in `[char]` is a warning and is skipped. It is never half-applied.
* Env overrides are not shown (dev only).
* The panel therefore shows what the game will do, and lists the same warnings.

## 4. The window

The header shows:
* **EXPERIMENTAL**;
* the file path (with `*modified`);
* the write gate line (§5), in green, orange or red;
* Apply to game, Reload from disk, Revert, auto-apply and + reload characters;
* the last save result;
* a collapsible list of every ini warning the game would log.

### Global
* **Style picker:** the built-ins plus the ini styles, with a hover description; an ini section that shadows a built-in is labelled.
* **Save as style:** a name box. Names containing `[ ] = ; #` or blanks are refused.
* **Every non-CharOnly lever**, grouped as in the header:
  * Checkboxes for switches, combos for choices.
  * Sliders (Ctrl+click to type) for ranges up to 6001 wide; step boxes for positions and speeds (±200000 and wider).
  * The value shown is the resolved one. The key turns blue when `[tuning]` sets it, and "(style)" marks a value that comes from the active style.
  * Tooltips give the help, the guide's notes for the levers that need them (cancel window, entry style, air tag, KO rule, assist entry/damage, …), the default, the range and the style's value.
  * `parkX` is greyed and marked boot-time.
* The assist slot **entries** (`assist.<d>.entry`) are here too, because the game accepts them in `[tuning]`.

### Character
* **Character picker**, in this order:
  1. the character open in the editor;
  2. the linked game's slots;
  3. existing `[char]` sections;
  4. every `data\<name>_<moon>_c.txt` in the game folder;
  5. the TeamChangeData table.

  `*` means the character has overrides.
* **Buttons:** *editor's* (pick the open character) and *Clear overrides*.
* **One row per per-character lever:**
  * The override tick box: ticked means the character's own value; unticked means it follows the global value, which is shown greyed.
  * **tagIn / tagOut** get a **pattern picker from the loaded HA6**: "N name", with a filter, empty patterns hidden. It is available when the character open in the editor is the one being edited (`sion_0.txt` → `sion`); otherwise it is a number box.
  * **jump** opens that pattern in the editor.
  * The hint "(0 = table: 241)" gives the TeamChangeData default, flagged when it exists only in the tag_mod HA6. **jump table** opens it.

### Assists
The character picker again, plus a moon selector when the `_c.txt` comes from the game folder. The command list is read through the **cmdfile parser** (`cmdfile::Document`), from:
* the open character's own `_c.txt` (`FrameData::m_commandsPath`), or
* `<game>\data\<file>_<moon>_c.txt`.

The first definition of an id wins, as in the engine; names are converted from CP932.

* **Character default**, shown with a jump button: the pick `pickDefaultCommand` makes. That is the lowest id whose input is 2+ direction digits then `A`, costing no meter, usable standing (flagset 1 bit 0), with a pattern. A pick that has ExComChecks is flagged "conditional", because the game checks moon/ExCom at call time.
* **Per slot (5, 2, 6, 4, 8):**
  * **Entry:** default / behind / edge / drop / arc.
  * **Mode:** default / pattern / command / motion. Switching the mode removes the slot's other action keys, so the file never relies on the pattern > command > motion precedence by accident.
    * *pattern:* the HA6 picker.
    * *command:* a combo of `id input -> pattern $meter (air) name`.
    * *motion:* a text box validated live: at most 6 characters from `0-9 A-F + V`, the same rule as `packMotion`.
  * A resolved line runs `resolveAssistSlot`, for example `-> motion 236C -> command 47 -> pattern 107 $10000, entry drop`. It says when the slot borrows slot 5's action, marks in red an action that points at nothing (an unknown command id, or no command with that input), and has **jump** plus the pattern name.
* **Per-character assist levers:** assistEntry, OffsetX, EntryTicks, Cooldown, MaxTicks, DamagePct.
* A warning is shown when `assistEnabled` is off.

### Live
From the Game Link:
* the scene, frame and TAG flag;
* per team: point and reserve (file, tagFlag), the point's pattern/frame, *edit this character*, and "tag in progress";
* a slot table.

With `QueryTag` (§5; a pchost.dll from PovertyCaster `mbaacc/link-tag`) it also shows:
* the raw tag state (idle / requested / exit / cooldown / 150 forced / entering / 300–303 assist phases), the counter, the tag-in tick and the **cooldown left**;
* the **assist state**: phase, slot, tick, cooldown, pattern, calls;
* health/red and the TagIn/TagOut in force per slot;
* the style, sha and warnings the game resolved.

**Follow point** jumps the editor to the pattern/frame the chosen team's point plays, when the open character is what that slot loaded (`SlotMaskForFile`). This is the Game Link window's follow, keyed on the team's point instead of a fixed slot, so it switches across tags.

### Raw
The file as it will be written.

**Startup options** (for scripted captures):
* `--tool tag`
* `--tag-ini <path>`
* `--tag-char <file>`
* `--tag-tab Global|Character|Assists|Live|Raw`

They run at frame 3, after `--open`, and work with or without it. `--tool gamelink` opens the Game Link window.

## 5. Safety, and the link fields it needs

**Validation before every save.** `NewWarnings(disk, edited)` must be empty: an edit may not add a warning the game would log. Warnings already in the file on disk (for example, a hand-written unknown key) are kept and shown, not blocked. The widgets clamp to each lever's range, and motions are validated as typed.

**The session gate: never write in a session.** On every Apply the panel decides whether it may write:

| What the link says | Verdict |
|---|---|
| not connected | allowed, labelled "session state unknown". Without the link nothing can be known, and the game itself never re-reads the file in a session. |
| `QueryTag` `sessionFlags != 0` | **refused** |
| `LinkState.gameModeKind == 0xFFFFFFFF` (native netplay) | **refused** |
| in battle, `LinkState.reloadAllowed == 0` (session / replay / recording / non-CE) | **refused** (conservative) |
| in battle, `reloadAllowed == 1` | allowed |
| outside a battle | **gate probe** (below) |

**The gate probe.** It sends `SetChar(slot 0, chara -1, moon -1, palette -1, flags 0)`.
* The DLL checks the arguments (all "keep"), then `gateVerdict`, which checks **session before scene**. It answers:
  * `RefusedSession` or `RefusedRecording`: refused;
  * `Ok` (a keep-everything pick "held until the next reload", which writes nothing), `RefusedScene` or `RefusedVariant`: allowed.
  * No answer within 1.5 s: refused.
* The probe is sent only on Apply, never polled, because a refused probe writes one line to the game's log. The result is cached for 3 s.
* One side effect: an `Ok` probe replaces a slot-0 pick that was *held without a reload* (`SetChar` without `kFlagReload`) with keep-everything. The Game Link window always sends picks with reload, so this does not happen in practice.

### The PovertyCaster side: `QueryTag` (implemented on `mbaacc/link-tag`)

`LinkState` carries only a boolean "tag request != 0". It has nothing of the swap counter (cooldown), the assist block, the session kind or the tuning the game resolved. So PovertyCaster branch `mbaacc/link-tag` (worktree `/mnt/c/dev/castergroup/pc-linktag-wt`, off main `5bf0bc46`) adds an op for them.

The change is additive: `kLinkVersion` stays 1, the unknown-op path is unchanged, and an older editor never sends the op.

**`pc-proto/Proto.hpp`:**
* `LinkOp::QueryTag = 8` and `IpcKind::LinkTag = 0x104`.
* `kLinkTagSess*` (six bits, one per reload-gate input).
* `LinkTagTeam` (40 B), `LinkTagSlot` (12 B) and `LinkTag` (180 B), mirrored field for field in Hantei-chan's `game_link_proto.h` `Tag`/`TagTeam`/`TagSlot`. The **sizes and offsets are pinned on both sides**: PovertyCaster `tests/mbaacc_link_tag`, and `static_assert`s here.

**`LinkTag` fields:**
* `sessionFlags`, `frozen`, `iniPresent`;
* `koRule` in force (the host's in netplay);
* `tuningLoads`, `warnings`, `assistEnabled`;
* `activeStyle[24]`, and `sha[16]` (the first 15 hex digits of the tuning set's sha256);
* per team: point, the raw `tagRequest`, counter, tag-in tick, `cooldownLeft` (state 101: `cooldownTicks - counter`, H6's comparison), and the AssistTeam fields: slot + mode, placement, flags, tick, cooldown, pattern, calls, meterPaid;
* per slot: TagIn/TagOut in force, health/red.

**`mbaacc/LinkTag.hpp`** (new, pure) holds `linktag::build(Inputs, LinkTag&)`, `sessionFlags(GateInputs)` and `cooldownLeft`.

**`MbaaccSim_TagTuning.cpp` `tagFillLinkTag`** is the live half, running on the game thread from `linkFrame`. It **only reads**:
* g_TeamAux +0/+4/+8;
* the savestated AssistBlock, copied out with `memcpy`;
* the player blocks and CharaSystemData;
* the tuning's own state, under `s_mx`.

Details:
* The F3 readout's memory reads were factored into `readTeam`/`readSlot`, so both paths read the same fields.
* The team and slot parts are filled only in battle, like `fillLinkState`.
* A query never triggers the first ini load: the koRule accessor is skipped until the tuning is loaded.
* `etm::lastGateInputs()` (EtmReload) gives the completed gate flags.
* `tagKoRuleInForce()` (MbaaccSim_Tag.cpp) exposes the existing `requestedRule()`.

**`MbaaccSim_EtmLink.cpp`:**
* `case QueryTag` answers with `LinkTag`. It is never gated: it is answered in a session too, which is how the editor learns that one is live.
* At most one QueryTag answer per frame, like QueryState and QueryStage.

**Nothing new writes game memory, touches a savestate region or feeds a hash.**

**Also on that branch:** `tag_tuning.sample.ini` now lists `assistFromBlockstun`, and its four stale help strings are synced. `mbaacc_tag_tuning` gained a drift test that checks every lever's default, group, range and help in the sample's reference list.

## 6. Verification

`tag_tuning_test.exe`, built by `./build.sh`, run from the repo root:

| Check | Result |
|---|---|
| Round trip, LF and CRLF | the unedited sample is byte-identical |
| Edits touch only the edited key | the diff span is exactly the value, e.g. `one -> all` for koRule, `1 -> 30` for `[style.Chaos] cooldownTicks`, and a new key is exactly one inserted line after the last `[tuning]` key. Setting a value back, clearing an added key, or adding then removing a `[char]` section gives the original bytes back. Trailing `; comment`, `#` comment, spacing, unknown keys and a missing final newline survive. Duplicate `[char.miyako]`/`[char.MIYAKO]` sections merge. |
| Validation | an out-of-range edit (`cooldownTicks=0`) is exactly one new warning; an unknown key already on disk is not a new one |
| Save as style / select style | resolves to the same values, no warnings, `[tuning]` lever keys gone |
| Lever table vs `tag_tuning.sample.ini` | defaults, groups, ranges and scope tags for all 38 listed levers match. **Sample drift found** (NOTE, not a failure): the sample does not list `assistFromBlockstun`, and its help wording for `assistEnabled`, `assistEntry`, `assistMaxTicks` and `assistMeter` lags the header. For PovertyCaster: refresh the sample's reference list (`writeTuningIni` generates it). |
| **Lever table vs `MbaaccTagTuning.hpp` (main 5f043727), compiled in** | 59/59 rows identical (key, order, group, kind, scope, range, default, names, help, perChar), 41 wire fields, the built-in styles identical, `parseLeverValue`/`formatLeverValue` identical on 35 inputs × 59 levers. The resolution of the sample plus four edited files (CRLF, unknown style, CharOnly in `[tuning]`, team-wide in `[char]`, `marvel` alias, slot actions) is identical: global values, style, warning count, per-character values for four names and the style list. |
| Default assists vs TAG_TUNING_GUIDE.md §3.1 | **100/100 rows** match (id, input, pattern) with `--data /mnt/c/games/mbaacc_tag/data`, including the 6 "none" rows. The 3 shiki rows run from the fixtures in ctest. |
| Slot resolution | precedence (pattern > command > motion), borrowing slot 5, entry inheritance slot > slot 5 > assistEntry; the motion `236C` resolves to command 47 → pattern 107 (the lowest id with that input) |
| Existing regressions | see §7 |

Total: 3645 checks with `--data`, 3457 with the fixtures only (ctest); 0 failures.

**Link paths against a mock DLL.** `src/game_link_mock.{h,cpp}` (`gamelink::MockDll`) serves `\\.\pipe\povertycaster-link-<own pid>` the way pchost.dll does, and also answers `QueryTag` with the same `LinkTag` layout. It is used in two places:
* `game_link_test` (3 modes: tag ops offline / no tag ops, i.e. today's DLL / session). It checks:
  * the `LinkTag` decode (assist phase 301 pattern 455, cooldown 80, style);
  * the `Unknown` fallback (`tagUnsupported`);
  * `sessionFlags`;
  * the gate probe's reply: `Ok` offline, `RefusedSession` in a session; `PeekReply` and the probe count.
  * Result: all passed.
* `game_link_cli mock-dll <s> [notag] [session] [menu]` serves the same thing for UI captures. `game_link_cli tag` prints the readout.

**UI captures.** Taken headless, with no game running: `--open C:\games\mbaacc_tag\data\shiki_0.txt --tool tag --tag-ini <copy> --tag-char shiki --tag-tab <tab> --capture <png>`. The ini is `docs/tag_panel/capture_tag_tuning.ini`: the sample, plus `cancelWindowTicks=4`, an unknown key `myNote`, and a `[char.shiki]` section with a cancel window, a drop entry, invulnerability, tagIn and three assist slots (command / motion / pattern).

| File | Shows |
|---|---|
| `tag_Global.png` | the EXPERIMENTAL label, file, "session state unknown" (no link), Apply / auto-apply / reload characters, 1 warning (`myNote`), the Classic style, levers with "(style)" / override marks, parkX greyed |
| `tag_Character.png` | shiki `*`, the override tick boxes, cancelWindowTicks 6 and entryStyle drop owned, the rest following the global values |
| `tag_Assists.png` | 47 commands from `shiki_0_c.txt`. Character default: command 40 (623A) → 455, matching the guide. Slot 5: command 45 236A → 105 百裂 (jump). Slot 2: motion 236C → command 47 → 107 $10000. Slot 6: pattern 455 ●対空 (the HA6 picker). The assistEnabled-off warning. |
| `tag_Live_mock.png` | `--tag-link` against `game_link_cli mock-dll` (`HANTEI_GAME_LINK_PID`): green "offline (QueryTag)". Team 1: Shiki point, Sion in assist-act with pattern 455. Team 2: Miyako point, cooldown 80, V.Sion parked. The slot table with hp/red and TagIn/Out. |
| `tag_Live_session.png` | the same with `mock-dll ... session` and `--tag-apply`: a red "the game reports a session" and "NOT saved: the game reports a session (tuning frozen)". The readout shows FROZEN, and the assist line reads "6+FN1 (pattern, drop entry)". The ini's md5 is unchanged (`ini_before.md5`). |
| `tag_Live_notag_menu.png` | `mock-dll ... notag menu` (today's DLL, outside a battle) with `--tag-apply`: the "no QueryTag" fallback note. The Apply sent exactly **one** gate probe (the mock counted 1), got `refused: not in battle`, which means offline, and so the header reads "offline (gate probe: ...)" and the result is "nothing to save (identical)". |
| `tag_Raw.png` | the file as it will be written |

`md5sum -c docs/tag_panel/ini_before.md5` still passes after every capture: opening, switching tabs and drawing never write the file.

No game was launched. The real DLL's side of the probe (`requestSetChar` → `gateVerdict`, session checked first) is code-reviewed against `MbaaccSim_EtmReload.cpp` / `EtmReload.hpp`; QueryTag does not exist in the DLL yet (§5).

## 7. Status and limits

**Regressions:** full `make -j4` of every target in `build/` (the `./build.sh` build, under the shared lock).

| Suite | Result |
|---|---|
| `tag_tuning_test` | PASS (3457 checks with the fixtures; 3645 with `--data`) |
| `game_link_test` | all passed |
| `cmdfile_test` | 272 checks, 0 failed, 7 files |
| `shortcut_router`, `workspace_session`, `package_tools`, `shared_clipboard`, `refs`, `bgm`, `hud`, `pattern_search` | PASS |
| `preview_sim_tool --selftest` | passed |
| `undo_manager_test` | PASS on a rerun. The first run, under machine load, reported one failure; it is timing-sensitive, and nothing in the undo code changed. |

**Limits:**
* **No live game test** (as asked). `QueryTag` exists only on PovertyCaster `mbaacc/link-tag` (§5). With an older pchost.dll, the Live tab has point/reserve/tagFlag/"tag in progress", and no cooldown or assist state.
* **Session state without the link is unknown.** The panel says so and still writes. The game never re-reads the file in a session, and reads it at the next session start, where the tuning handshake refuses a mismatch.
* **The probe's one side effect** (§5): a held slot-0 pick without a reload is replaced by keep-everything.
* **The default assist is the file rule.** The game can pick differently when an ExComCheck fails at call time. Those cases are flagged "conditional", and the game's own log line (`ASSIST default ...`) is the truth.
* **Pattern pickers and jump** need the character open in the editor (its `.txt` stem must match the `[char]` name). Otherwise the field is a number.
* **Env overrides** (`PCHOST_MBAACC_TAG_*`) are not shown; they are dev-only, and they win over the file in the game.
* **Sample drift** (fixed on PovertyCaster `mbaacc/link-tag`): the sample did not list `assistFromBlockstun`, and 4 of its help strings were older than the header.
* **Windows position.** The window is placed relative to the main viewport. A build that opened it before this fix may have saved a detached position in `hanteichan.ini`; closing and reopening it, or deleting its `[Window][Tag / Team (experimental)]` block, fixes that.

## 8. Commits (`feat/tagpanel`, on `b5567f8`; not pushed)

| Commit | What |
|---|---|
| `143c3c6` | lever table mirror (`tag_levers.h`) and the byte-preserving `TagIni` editor + resolution |
| `e2d39ce` | assist slot model (`tag_assist.*`), `tag_tuning_test`, fixtures (shiki C/F/H `_c.txt`, the guide's default table) |
| `ecf3580` | panel UI first cut (a WIP commit made across a machine restart) |
| `1e1ece6` | the sample ini fixture (force-added: `*.ini` is ignored) |
| `a83c47a` | `tag_tuning_test` passing: round trip, PovertyCaster header drift check, 100/100 default assists |
| `5954e3c` | client: proposed `QueryTag`/`LinkTag`, `ProbeGate`, `PeekReply`, `GameDirOf` exported |
| `fbdb4e5` `7a14678` `4ab7f23` `3312cb6` | the Tag / Team window, menu, startup options, viewport-relative placement, `_c.txt` / game folder from the open character, colours |
| `1d6aaf3` | `MockDll`, `game_link_test` coverage, `game_link_cli tag` / `mock-dll` |
| `3be7cfd` `865fcd9` | `--tag-link`, `--tag-apply`, live readout polish |
| `af81e51` `90bbfb9` | this document (copy in `Hantei_Docs/update_2026_09/`) and the capture evidence |
| `2390b0a` `a83d792` `224c5c8` | the mirror follows the implemented PovertyCaster `LinkTag` (offsets pinned, state names 200/255/256), wording, §5 rewritten |

**PovertyCaster, `mbaacc/link-tag`** (`/mnt/c/dev/castergroup/pc-linktag-wt`, off main `5bf0bc46`; not pushed, not merged):

| Commit | What |
|---|---|
| `6d21726c` | `tag_tuning.sample.ini`: `assistFromBlockstun` listed, four help strings synced; `mbaacc_tag_tuning` sample drift test |
| `9d68513a` | `QueryTag` / `LinkTag` (Proto.hpp), `mbaacc/LinkTag.hpp` (pure build), `tagFillLinkTag` (read-only), `etm::lastGateInputs`, `tagKoRuleInForce`, `tests/mbaacc_link_tag` |
| `96d1772a` | a query never triggers the first ini load |

Build: the full `pchost.dll` builds (x86, `cmake --build -j4` under the shared lock). ctest: `mbaacc_link_tag`, `mbaacc_tag_tuning` (with the new drift test), `mbaacc_tag_table`, `mbaacc_etm_reload_gate`, `mbaacc_stage_link` and `ipc_link_roundtrip` all pass (6/6).

`pc_lint`: one ratchet reads 3 against a baseline of 2 (`adapter_syms_in_shared_libs`). The extra entry is `libpc_spectate2.a`'s `std::string::_M_replace`, a libstdc++ instantiation. This branch does not touch pc-spectate2, so the baseline was left as it is for hinokakera to judge.
