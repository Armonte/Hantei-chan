# Authoring Mode: Hantei-chan + PovertyCaster (MBAACC)

**Status:** design, frozen for implementation (2026-09-25). No code exists yet.

**Audience:** the two implementation agents, one on PovertyCaster (PC) and one on Hantei-chan (HC). Each should be able to build its half from this document alone. Every wire struct is given byte for byte. When this document and a mirrored header disagree, fix the header to match the document, then update the document in the same commit.

**User goal:** *"I need one source of truth for the hantei live edit shit. Right now we have hantei and the ini/txt. We need this to be a proper dev authoring tool with good UX/UI, i.e. I need to be able to select the chars and moons etc, have them loaded up, and tweak everything."*

**Final user decisions (not up for discussion):**
1. **Per-character sidecar files plus one global/styles file are the single source of truth.** Hantei-chan edits them, the game hot-reloads them, and netplay carries them.
2. **Scope of this phase:**
   * Pick characters, moons, palettes and partners.
   * Pick the stage, the mode (1v1 / TAG / TEAM), the style and assists on/off.
   * Hantei-chan launches MBAA with PovertyCaster, or attaches to a running game.
   * The game loads exactly that setup, and Hantei-chan opens those characters' HA6 files.
   * Training/TAS controls come later. The protocol reserves room for them.
3. **Hantei-chan is the one editor.** The in-game F3 Tag panel becomes read-only, with a few quick toggles.
4. **Netplay:** the host sends its full resolved tuning, so the client does not need matching files. This replaces today's "tuning mismatch → refuse" rule.

Built on (read before starting):

| Piece | Where |
|---|---|
| PC `origin/main` | `6d0161d4` |
| PC `mbaacc/link-tag` | `89a56ace` in `~/wt/pc-linktag`. Adds QueryTag + the 180-byte LinkTag. Not merged. |
| Tuning, pure half | `pc-adapters/mbaacc/include/mbaacc/MbaaccTagTuning.hpp` |
| Tuning, live half | `src/MbaaccSim_TagTuning.cpp` |
| Ruleset | `src/MbaaccSim_Ruleset.cpp` |
| TAG CSS | `include/mbaacc/TagCss.hpp`, `src/MbaaccSim_TagCss.cpp` |
| Dev link | `src/MbaaccSim_EtmLink.cpp`, `pc-proto/include/pc/proto/Proto.hpp`, `pc-ipc` |
| HC | `feat/tagpanel` (`/mnt/c/dev/hantei-chan/wt/tagpanel`): `src/tag_tuning/*`, `src/tag_panel.cpp`, `src/game_link*.{h,cpp}`, `src/game_link_proto.h` |
| HC stage browser | `update/ex-mbac`: `src/background/bg_browser.{h,cpp}`, `bg_project.h` |
| Earlier docs | `docs/HANTEI_GAME_LINK.md`, `docs/HANTEI_STAGE_LINK.md`, `docs/HANTEI_TAG_PANEL.md`, `docs/tag_research/{TAG_TUNING_GUIDE,TAG_CSS,PC_TAG_SKELETON}.md` |

---

## 0. Decisions at a glance

| Question | Decision | Why (short) |
|---|---|---|
| File format | **INI**, in the exact grammar of the game's `parseTuningIni`, extended with `[char]` and `[moon.<m>]` sections. | The DLL already parses it. HC already has a byte-preserving editor for it (`tagtune::TagIni`). It allows comments and survives CP932. The migration can be proven equivalent by the same resolver. JSON would add a second parser to the DLL and lose comments. |
| Location | `<gamedir>\povertycaster\tag\global.ini` and `<gamedir>\povertycaster\tag\chars\<file>.ini` | `<gamedir>\povertycaster\` already exists (sidebars). Nothing new appears next to MBAA.exe. |
| Per-character key | The character's **data-file name (File1)**, lower-case: `sion`, `v_sion`, `warakia`, `shiki`. | This is the key the game already uses: `[char.<file>]`, TeamChangeData, `LinkActor.file`, H2. HA6 stems (`sion_0_r`) are per moon and per layer. The moon difference lives inside the file as `[moon.*]` sections. |
| Moon-specific values | **Yes.** `[moon.crescent\|full\|half]` sections in the character file, layered on top of `[char]`. | `_c.txt` command ids and HA6 pattern numbers can differ per moon. Assist commands, tagIn and tagOut need this. |
| Hot reload | **The file is the truth. The link only says "re-read now" (`ApplyTuning`) and never pushes values.** The DLL still polls once a second, so hand edits work. | One source of truth. What is on disk is what plays, and what the host serialises. |
| Who writes the files | **Only Hantei-chan** (or a human in a text editor). The DLL never writes tuning files. The F3 "Save to ini" is removed. | One writer, so there are no lost updates and no comment-dropping rewrites. |
| Setup op | `SetMatchSetup` (64-byte payload). The DLL chooses a **hot path** (ETM reload, ~0.5–1.2 s) or a **cold path** (rebuild through the CSS, ~3–10 s). | It reuses the proven ETM reload and the TAG CSS "viewer result" commit. |
| "Training" | 1v1 uses **native Training** (0x1010). TAG and TEAM use **Authoring VS**: a VS scene with an infinite timer, an endless round, and P2 neutral. | OR-ing 0x100 onto 0x1010 breaks every `== 0x1010` compare in the game. That is unaudited gap G3 (`PC_TAG_SKELETON.md` §154, `PC_2V2_AUDIT.md` G3). See §7 Q1. |
| Netplay carriage | The host sends a **session tuning payload**: the global block plus one fully resolved entry per (file, moon) in the match, at most 4 entries and ≤ 755 bytes. It travels on the adapter control byte **0xCE**, sub-kind `0xA7`. The ruleset blob's 17-byte tuning wire becomes **wire v3**, the digest of that payload. The client **adopts** the payload. | The user asked for the 4 selected characters only. The 32-byte ruleset blob cannot hold it (30 of 32 bytes are already used), and `pc-ipc`/control limits are ~1 KB. |
| Timing guarantee | The client's **first tuning read of a match (H2, on the loading thread) waits** until the host payload for this match's character set has been adopted, for up to 5 s. On timeout the session is refused. | The TAG CSS commit happens on the same lockstep frame on both peers, and H2 is the first reader of tuning. So both peers load with the same bytes, and no generic attach hold is needed. |
| Replay / spectate | A new tape chunk **`CH_SETUP_EXT = 16`** (adapter-opaque, per gen, right after `CH_GEN`) carries the same payload. Unknown-chunk readers skip it. | Replay and spectate need the tuning without the recorder's files. The setup blob is full (32/32). |
| What replaces the mismatch refusal | **Adoption.** A TAG session is refused only when the host's build cannot send the payload (wire version ≠ 3), the payload does not arrive in time, or it fails to decode. 1v1 is never refused over tuning. | Decision 4. |

---

## 1. Model

```
              Hantei-chan (the only editor)                          MBAA.exe + pchost.dll
 +--------------------------------------------------+      +-----------------------------------------+
 | Authoring window                                 |      |                                         |
 |  Setup -----------SetMatchSetup (pipe)-----------+----->| authoring state machine (hot/cold path) |
 |  Tuning editor --atomic write--> povertycaster\tag\*.ini <--poll 1 Hz / ApplyTuning-- tuning live |
 |  Live  <---------QueryState/Tag/Tuning/Setup-----+------| half: resolve -> per-slot Tuning        |
 |  HA6 tabs --save--> data\*.HA6 --auto Reload-----+----->| ETM reload                              |
 +--------------------------------------------------+      |   in a TAG session (host):              |
                                                           |   payload = global + <=4 entries        |
                                                           |   -> 0xCE/0xA7 to client (adopts)       |
                                                           |   -> CH_SETUP_EXT in .pcrep / spectate  |
                                                           +-----------------------------------------+
```

Rules the design rests on:

1. **One truth per mode.**
   * Offline: the sidecar files.
   * In a session, replay or spectate: the payload the host (or the tape) supplied. The local files are ignored there, as today's freeze already does.
2. **No values over the pipe into the game.**
   * The pipe carries **setup** (who fights where), **commands** ("re-read the files"), and **reads** (what the game resolved).
   * The setup is not tuning. It is the same kind of thing as a CSS pick.
3. **Everything the editor shows about the game comes from a read** (QueryTuning, QueryMatchSetup), never from the editor's assumption. HC resolves the files itself as well and flags any cell where its own resolution differs from the game's.
4. **Session gate unchanged.**
   * Every state-changing op goes through the existing `etm::gateVerdict` / `tag::tuningReloadVerdict` refusal set: netplay, rollback, compat, spectate, replay playback, synctest, an armed recording, or a non-CE build.
   * Reads are always answered.

---

## 2. Sidecar files

### 2.1 Layout

```
<gamedir>\
  MBAA.exe, pchost.dll, pc_inject.exe, data\ (loose), Bg\ ...
  tag_tuning.ini                 legacy; read only when no sidecar exists (2.6)
  povertycaster\
    sidebars\ ...                (existing)
    tag\
      global.ini                 [tuning] + [style.<name>] (+ tolerated legacy [char.<file>], 2.6)
      chars\
        shiki.ini                [char] + optional [moon.crescent] [moon.full] [moon.half]
        sion.ini
        ...
      *.bak                      previous bytes, written by Hantei-chan once per editing session per file
```

* **File names:**
  * `chars\<file>.ini`: `<file>` is the lower-case File1 name from the character's `g_CharaSelectDataTable` entry (+52), which is also the `kTeamChangeTable` key.
  * The allowed characters are `[a-z0-9_]`, 1–27 characters long (`LinkActor.file` is 28 bytes).
  * Any other file in `chars\` is skipped with a warning.
* **Encoding:**
  * Raw bytes. CP932 is allowed in comments. CRLF and LF are both accepted.
  * The DLL parses bytes. HC keeps every byte it does not edit.

### 2.2 `global.ini` grammar

This is today's `tag_tuning.ini` grammar (`parseTuningIni`), unchanged:

```ini
[tuning]
active_style=Classic
koRule=oneDown
cancelWindowTicks=4          ; a global lever override (on top of the style)

[style.MyStyle]
regen=1
cooldownTicks=60
```

* `[tuning]`:
  * Every instance counts, in file order, and a later key wins. This includes `active_style`.
  * Keys: every lever whose scope is not CharOnly (§2.5).
* `[style.<name>]`: only the **first** instance of a name counts (`findIniStyle`). Keys: the same set as `[tuning]`.
* **Comments:** `;` or `#` at the start of a line, or after a space or tab.
* Section names and keys are case-insensitive.
* `[char.<file>]` in `global.ini` is **tolerated for back-compat.** It is applied, but below the character file (§2.4), and it logs one warning per section: `per-character section in global.ini: move it to chars\<file>.ini`.

### 2.3 `chars\<file>.ini` grammar

```ini
; chars\shiki.ini — TAG tuning for data file "shiki" (every moon)
[char]
cancelWindowTicks=6
entryStyle=drop
tagIn=250                   ; an alternate entry authored in Hantei-chan
assist.5.command=45         ; 236A in shiki_0_c.txt

[moon.full]                 ; only when this slot plays Full Moon
assist.5.command=47         ; Full's _c.txt numbers differ
tagIn=252
```

* `[char]` holds the character's own values for every moon. The header `[char.<name>]` is accepted as a synonym (copy-paste from the legacy file). If `<name>` differs from the file name, the file name wins and a warning is logged.
* `[moon.<m>]` holds values for one moon only. `<m>` is `crescent|full|half`, or the aliases `c|f|h` and `0|1|2`, case-insensitive. The canonical written form, used by HC and the migrator, is `[moon.crescent]`, `[moon.full]`, `[moon.half]`. The moon numbers are the game's: 0 crescent, 1 full, 2 half, which is also the `<file>_<m>.txt` suffix.
* Allowed keys in both sections: every `perChar` lever, including the CharOnly ones (§2.5).
* These are warnings, and the offending line is skipped:
  * a team-wide key (`[char] … is team-wide, not per character`);
  * `[tuning]` or `[style.*]` inside a character file (`ignored in a character file`);
  * an unknown section or key;
  * a bad or out-of-range value.

  Nothing is ever half-applied, which is today's `applyKvs` rule.
* Repeated sections of the same kind in one file merge in file order, and a later key wins (the `[char.x]` rule today).

### 2.4 Resolution order

For the global values (what `tagTuning()` returns):

```
Tuning{} defaults
  <- the active style ([style.<active_style>] from global.ini first, else the built-in of that name)
  <- global.ini [tuning] keys
  <- PCHOST_MBAACC_TAG_* env (dev only; non-CharOnly levers)                  = G
```

For one slot playing data file `f` in moon `m` (what `tagTuningForSlot(slot)` returns):

```
G
  <- global.ini [char.f] sections (legacy-tolerated)             perChar levers only
  <- chars\f.ini [char]                                          perChar levers only
  <- chars\f.ini [moon.<m>]                                      perChar levers only
  <- the CSS assist choices (partner slots 2/3 only; TagCss.hpp applyAssistChoices)       = S(slot)
```

* This is today's `tuningForChar` with two layers added (the char file and the moon section). Env still sits below the character layers, exactly as today (`s_active` carries env and `[char]` lands on top of it).
* **The slot's moon.** It is the moon the slot actually loaded. The PC agent should take it from the load in `BattleScene_LoadCharacterData`, where H2 already knows the file (`tagTuningNoteSlotFile(slot, file)` gains a `moon` argument). A partner whose moon byte is 0xFF ("follow the primary") resolves to the primary's moon.

### 2.5 Every lever

Source: `MbaaccTagTuning.hpp` `kLevers` at `origin/main` (59 rows; HC's `tag_levers.h` matches it 59/59).

* **Index** is the table order. It is the wire order and the index into the `values[64]` arrays in §3.
* **Lives in:**
  * *global*: `global.ini` `[tuning]` / `[style.*]` only.
  * *global + char/moon*: may also be set in a character file.
  * *char file only*: CharOnly; valid only in `[char]` / `[moon.*]`.
* **Netplay:**
  * *block*: in the fixed global block (109 B).
  * *per-char*: in each payload entry (§4.1).
  * *—*: never on the wire (Harness).
* **Defaults** are `Tuning{}`. Motion levers are text of up to 6 characters from `0-9 A-F + V`, packed by `packMotion`.

| # | key | group | kind | range | default | lives in | netplay | meaning |
|---|---|---|---|---|---|---|---|---|
| 0 | `cooldownTicks` | Swap | int | 1..6000 | 120 | global | block | ticks in state 101 before the team can tag again (stock 120) |
| 1 | `guardSwapHit` | Guards | bool | 0/1 | 1 | global | block | S1: a hit in the 2-tick swap window cancels the swap (else tag-lock) |
| 2 | `exitTimeoutTicks` | Guards | int | 0..6000 | 180 | global | block | S2: force-park an exit still running this long after the swap (0 = off) |
| 3 | `regen` | Regen | bool | 0/1 | 0 | global | block | MBAC: the resting partner regains red health |
| 4 | `regenPerTick` | Regen | int | 0..11400 | 1 | global | block | health per tick while resting (MBAC 1) |
| 5 | `parkX` | Park | int | -2000000..2000000 | -102400 | global | block (boot patch) | resting partner x (byte patch at boot: restart to change) |
| 6 | `parkY` | Park | int | -2000000..2000000 | 0 | global | block | resting partner y |
| 7 | `pinTeamPoint` | Harness | bool | 0/1 | 1 | global (local only) | — | stress harness: pin each team's point's health (not netplay) |
| 8 | `cancelWindowTicks` | Tag-in cancel | int | -1..255 | -1 | global + char/moon | block + per-char | tag-in actionable from this tick (-1 = the pattern's own flags) |
| 9 | `cancelNormals` | Tag-in cancel | bool | 0/1 | 1 | global + char/moon | block + per-char | window allows normals / jumps / guard / walk |
| 10 | `cancelSpecials` | Tag-in cancel | bool | 0/1 | 1 | global + char/moon | block + per-char | window allows command-list specials / supers |
| 11 | `airTagOut` | Air tag | bool | 0/1 | 0 | global + char/moon | block + per-char | 22D while airborne; the outgoing starts at its lift-off frame |
| 12 | `airTagIn` | Air tag | bool | 0/1 | 0 | global + char/moon | block + per-char | the incoming enters airborne (entryStyle drop / arc) |
| 13 | `entryStyle` | Entry | enum | edge/drop/arc | edge | global + char/moon | block + per-char | edge = stock dash-in; drop / arc need airTagIn |
| 14 | `entryX` | Entry | int | 0..200000 | 40960 (0xA000) | global + char/moon | block + per-char | edge / arc start: distance from the camera centre (stock 40960) |
| 15 | `dropX` | Entry | int | -200000..200000 | 16384 | global + char/moon | block + per-char | drop start: distance from the camera centre, own side |
| 16 | `entryY` | Entry | int | -200000..0 | -40000 | global + char/moon | block + per-char | drop start height (negative = up) |
| 17 | `arcY` | Entry | int | -200000..0 | 0 | global + char/moon | block + per-char | arc start height |
| 18 | `arcVelX` | Entry | int | -10000..10000 | 900 | global + char/moon | block + per-char | arc forward speed |
| 19 | `arcVelY` | Entry | int | -10000..10000 | -2800 | global + char/moon | block + per-char | arc initial vertical speed (negative = up) |
| 20 | `dropVelY` | Entry | int | -10000..10000 | 0 | global + char/moon | block + per-char | drop initial vertical speed (positive = down) |
| 21 | `entryGravity` | Entry | int | 0..2000 | 150 | global + char/moon | block + per-char | air entries: y acceleration per tick |
| 22 | `entryAnchor` | Entry | enum | camera/point | camera | global + char/moon | block + per-char | x distances measured from: camera = screen edge, point = behind the point |
| 23 | `entryInvulnTicks` | Entry | int | 0..255 | 0 | global + char/moon | block + per-char | strike + throw invulnerability ticks on entry (0 = none, stock) |
| 24 | `tagIn` | Patterns | int | 0..999 | 0 | char file only | per-char | [char] tag-in pattern number (0 = the TeamChangeData table) |
| 25 | `tagOut` | Patterns | int | 0..999 | 0 | char file only | per-char | [char] tag-out pattern number (0 = the TeamChangeData table) |
| 26 | `koRule` | KO rule | enum | oneDown/allDown | oneDown | global | block | oneDown = the point's KO ends the round; allDown = forced tag-in |
| 27 | `forcedEntryDelayTicks` | KO rule | int | 0..600 | 60 | global | block | allDown: ticks from the point's KO to the partner's entry |
| 28 | `swapRedKeepPct` | Entry | int | 0..100 | 50 | global + char/moon | block + per-char | % of the incoming's red-health gap kept at the swap (stock 50) |
| 29 | `assistEnabled` | Assist | bool | 0/1 | 0 | global | block | the assist button (FN1) calls the partner in for one move |
| 30 | `assistEntry` | Assist | enum | behind/edge/drop/arc | behind | global + char/moon | block + per-char | default entry: behind the point / screen edge / drop from above / jump arc |
| 31 | `assistOffsetX` | Assist | int | 0..200000 | 36000 | global + char/moon | block + per-char | behind / drop: start distance behind the point |
| 32 | `assistEntryTicks` | Assist | int | 0..120 | 8 | global + char/moon | block + per-char | behind / edge: entry ticks before the action (drop / arc: on landing) |
| 33 | `assistCooldownTicks` | Assist | int | 0..6000 | 180 | global + char/moon | block + per-char | ticks after the assist parks before the next call |
| 34 | `assistMaxTicks` | Assist | int | 1..6000 | 150 | global + char/moon | block + per-char | the move is cut off after this many ticks |
| 35 | `assistDamagePct` | Assist | int | 0..1000 | 100 | global + char/moon | block + per-char | % of a hit's damage the assist takes (100 = stock) |
| 36 | `assistMeter` | Assist | bool | 0/1 | 1 | global | block | meter moves allowed as assists (paid from the point's meter) |
| 37 | `assistPromote` | Assist | bool | 0/1 | 1 | global | block | 22D during an assist promotes the assist to point |
| 38 | `assistFromBlockstun` | Assist | bool | 0/1 | 0 | global | block | the assist may be called from blockstun (alpha-counter style) |
| 39 | `assist.5.entry` | Assist slots | enum | default/behind/edge/drop/arc | default | global + char/moon | block + per-char | 5+FN1 entry (default = slot 5's, then assistEntry) |
| 40 | `assist.2.entry` | Assist slots | enum | default/behind/edge/drop/arc | default | global + char/moon | block + per-char | 2+FN1 entry (default = slot 5's, then assistEntry) |
| 41 | `assist.6.entry` | Assist slots | enum | default/behind/edge/drop/arc | default | global + char/moon | block + per-char | 6+FN1 entry (default = slot 5's, then assistEntry) |
| 42 | `assist.4.entry` | Assist slots | enum | default/behind/edge/drop/arc | default | global + char/moon | block + per-char | 4+FN1 entry (default = slot 5's, then assistEntry) |
| 43 | `assist.8.entry` | Assist slots | enum | default/behind/edge/drop/arc | default | global + char/moon | block + per-char | 8+FN1 entry (default = slot 5's, then assistEntry) |
| 44 | `assist.5.pattern` | Assist slots | int | 0..999 | 0 | char file only | per-char | [char] 5+FN1: play this hantei pattern directly (0 = unset) |
| 45 | `assist.5.command` | Assist slots | int | -1..999 | -1 | char file only | per-char | [char] 5+FN1: the _c.txt command id whose pattern plays (-1 = unset) |
| 46 | `assist.5.motion` | Assist slots | motion | text, ≤6 of 0-9 A-F + V | (unset) | char file only | per-char | [char] 5+FN1: feed this input through the command interpreter (e.g. 236C) |
| 47 | `assist.2.pattern` | Assist slots | int | 0..999 | 0 | char file only | per-char | [char] 2+FN1: play this hantei pattern directly (0 = unset) |
| 48 | `assist.2.command` | Assist slots | int | -1..999 | -1 | char file only | per-char | [char] 2+FN1: the _c.txt command id whose pattern plays (-1 = unset) |
| 49 | `assist.2.motion` | Assist slots | motion | text, ≤6 of 0-9 A-F + V | (unset) | char file only | per-char | [char] 2+FN1: feed this input through the command interpreter (e.g. 236C) |
| 50 | `assist.6.pattern` | Assist slots | int | 0..999 | 0 | char file only | per-char | [char] 6+FN1: play this hantei pattern directly (0 = unset) |
| 51 | `assist.6.command` | Assist slots | int | -1..999 | -1 | char file only | per-char | [char] 6+FN1: the _c.txt command id whose pattern plays (-1 = unset) |
| 52 | `assist.6.motion` | Assist slots | motion | text, ≤6 of 0-9 A-F + V | (unset) | char file only | per-char | [char] 6+FN1: feed this input through the command interpreter (e.g. 236C) |
| 53 | `assist.4.pattern` | Assist slots | int | 0..999 | 0 | char file only | per-char | [char] 4+FN1: play this hantei pattern directly (0 = unset) |
| 54 | `assist.4.command` | Assist slots | int | -1..999 | -1 | char file only | per-char | [char] 4+FN1: the _c.txt command id whose pattern plays (-1 = unset) |
| 55 | `assist.4.motion` | Assist slots | motion | text, ≤6 of 0-9 A-F + V | (unset) | char file only | per-char | [char] 4+FN1: feed this input through the command interpreter (e.g. 236C) |
| 56 | `assist.8.pattern` | Assist slots | int | 0..999 | 0 | char file only | per-char | [char] 8+FN1: play this hantei pattern directly (0 = unset) |
| 57 | `assist.8.command` | Assist slots | int | -1..999 | -1 | char file only | per-char | [char] 8+FN1: the _c.txt command id whose pattern plays (-1 = unset) |
| 58 | `assist.8.motion` | Assist slots | motion | text, ≤6 of 0-9 A-F + V | (unset) | char file only | per-char | [char] 8+FN1: feed this input through the command interpreter (e.g. 236C) |

**Counts:**
* 59 levers.
* 42 are `perChar`. That includes 17 CharOnly: tagIn, tagOut, and 15 assist slot actions.
* 41 fields are in the global block (Sim + SimBoot).
* The per-char entry values take **132 bytes**.

**Special cases:**
* `parkX` is **SimBoot**: a byte patch at hook install. A changed value offline means "restart the game". In a session see §4.3(e).
* `pinTeamPoint` is **Harness**: local only, never on the wire.
* `koRule` in a session is still decided by ruleset byte 4 (the host's), as today.
* `assistEnabled` stays a dev override that forces assists on. The match-level assists switch is the setup's `kSetupAssists` flag (§3.3), which is what the CSS "TAG Battle / no assists" preset sets today.

**The limit of 64 levers:**
* The link arrays hold 64 values.
* A 65th lever needs a new struct revision.
* PC adds `static_assert(kLeverCount <= 64)` next to the `kTuningBlockSize` assert.

### 2.6 Back-compat reading rules (DLL)

The DLL decides the tuning source at every (re)load:

1. **Sidecar mode.** Chosen when `povertycaster\tag\global.ini` exists, or `povertycaster\tag\chars\` holds at least one `*.ini`.
   * It resolves as in §2.4.
   * If `tag_tuning.ini` also exists, it is **ignored**. The DLL logs it once per process and sets `LinkTuningGlobal.flags.legacyIgnored` so HC can offer to finish the migration.
2. **Legacy mode.** Chosen when there is no sidecar but `<gamedir>\tag_tuning.ini` exists. It resolves exactly as today (byte-identical behaviour), and `source = Legacy`.
3. **Defaults.** Neither exists: `source = Defaults`.

Env overrides apply on top in every mode.

A sidecar tree with no `global.ini`, only character files, is valid: the global values are the defaults plus env, with no style.

### 2.7 Migration (one-shot converter, in Hantei-chan)

Code: `src/tag_tuning/tag_migrate.{h,cpp}`. It runs from the Authoring window (*Tuning → Migrate tag_tuning.ini…*) or from `game_link_cli tag-migrate <gamedir> [--dry-run]`.

1. **Refuse** if `povertycaster\tag\global.ini` or any `chars\*.ini` already exists. Merging is not supported; the message says so.
2. Load `tag_tuning.ini` into `TagIni`.
3. Build **`global.ini`**:
   * Take every line of the legacy file that is **not** inside a `[char.*]` section, in order. That covers `[tuning]`, `[style.*]`, unknown sections and all comments, including the commented-out `; [char.miyako]` examples.
   * Prefix one line: `; migrated from tag_tuning.ini by Hantei-chan <date>; per-character sections moved to chars\`.
4. Build **`chars\<name>.ini`** for each distinct `[char.<name>]` (case-insensitive merge, name lower-cased):
   * Write the header `[char]`.
   * Then write every line of every instance of that section in file order: key lines and comment lines, verbatim, including trailing comments.
   * A `<name>` that fails the §2.1 character rule is kept in `global.ini` as a legacy `[char.*]` section, with a warning.
5. **Verify before writing anything.**
   * Resolve the legacy text with the legacy rules, and the new set with the sidecar rules.
   * For every character name in either, and for each moon 0..2, the resolved per-slot values must be identical: global values, style name, and the per-character values.
   * The warning *messages* must be identical, ignoring line numbers and section labels.
   * If anything differs, abort and write nothing. The dialog shows the first difference.
6. **Write** with the atomic writer (§2.8): character files first, `global.ini` last.
7. **Rename** `tag_tuning.ini` → `tag_tuning.ini.migrated`, so the DLL does not report `legacyIgnored` forever. Then send `ApplyTuning` if linked.
8. `--dry-run` prints the planned files and the verification result, and writes nothing.

`tag_tuning.ini.bak` from the F3 panel stays where it is. Nothing is deleted.

### 2.8 Hot reload, atomic writes, file-watch vs push

**The decision is file-watch plus an explicit re-read command. Values are never pushed over the link.**

| | File-watch + `ApplyTuning` (chosen) | Push resolved values over the link |
|---|---|---|
| Source of truth | disk, always | the game's memory can diverge from disk |
| Hand edits (notepad) | work (1 s poll) | ignored until something pushes |
| Netplay host | serialises what it resolved from disk | must decide whether memory or disk is the truth |
| Latency | `ApplyTuning` makes it ~1 frame; the poll is ≤ 1 s | ~1 frame |
| Needs a 2nd serialiser in HC | no | yes, and one that must match the DLL byte for byte |

**Hantei-chan write protocol (per file):**
1. Write `<f>.tmp` in the same directory, then `FlushFileBuffers`.
2. `MoveFileExW(tmp, f, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
3. Before the **first** write to `<f>` in this HC session, copy the previous bytes to `<f>.bak`.
4. For several dirty files, write the character files first and `global.ini` last. Then send **one** `ApplyTuning` (§3.4.4). The poll may observe a half-saved set between files. That is harmless offline, and `ApplyTuning` gives the consistent read.
5. Refuse the save (with the message shown) when:
   * any file would gain a new warning (the panel's `NewWarnings` rule, kept); or
   * the game is in a session and the editor is locked (§5.6).

**DLL read protocol:**
* **Poll** once a second, offline only (same place and gate as today's `pollFile`):
  * stat `global.ini`;
  * enumerate `chars\*.ini` (name, mtime, size);
  * stat the legacy `tag_tuning.ini`.
  * Any difference triggers a full reload: parse all files, resolve, commit.
* **Opening files:** open with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` and read the whole file.
  * A **sharing violation or read error** keeps the previous values for the whole set and retries at the next poll. It never falls back to defaults. This fixes a latent issue in today's `readFile`, which treats "cannot open" as "absent".
  * "Removed" means `GetFileAttributesExW` reports not found.
* **Other re-read triggers** are unchanged: after every ETM character reload (the listener) and on `ApplyTuning`.
* **After a commit:** `afterCommit()` as today: `tagRefillSlotPatterns` re-points tagIn/tagOut, and `tagApplyKoRuleLive`. No character reload is needed for any lever except `parkX`, which needs a restart.
* **In any session, replay or recording:** nothing is polled or re-read. Today's freeze and `noteIgnoredEdit` apply unchanged to the sidecar directory.

---

## 3. Link protocol additions

### 3.1 Compatibility and discovery

* **Transport is unchanged:**
  * pc-ipc framing: `IpcHeader{u16 kind, u16 version, u32 size}` plus the payload.
  * Pipe: `\\.\pipe\povertycaster-link-<pid>`.
  * `IpcHeader.version` stays `kLinkVersion = 1` on every kind, old and new.
* **The size cap is 1024 bytes of payload per message, both ways.**
  * `pc-ipc/src/PipeChannel.cpp` has `kMaxPayload = 1024`, and HC's client closes the pipe on `h.size > 1024`.
  * Every new message is ≤ 1024 B. Nothing below needs pc-ipc changes.
* **Discovery.**
  * On connect, HC sends `LinkOp::QueryCaps` in an ordinary 24-byte `LinkCommand`.
  * A DLL built before this work answers `LinkReply{status = Unknown}`, and HC runs in **link rev 1 mode**: Game Link, Stage and QueryTag only (§5.1). A new DLL answers `LinkCaps`.
  * HC sends `LinkCommandEx` **only** after `LinkCaps` says `kLinkCapSetup`. An old DLL's `linkFrame` drops unknown kinds without a reply ("dropped a message of kind …").
* **Old editors against a new DLL** are unaffected: no existing struct, op, flag or reply changes.
* **Authoring revision:** `LinkCaps.revision = 1` for this document. Additive changes bump it. Removing or re-laying-out anything needs a new op or kind, never a changed struct.

### 3.2 New ops and kinds

| `LinkOp` | Name | Carried in | Answer | Gated |
|---:|---|---|---|---|
| 9 | `QueryCaps` | `LinkCommand` | `LinkCaps` (0x105) | never |
| 10 | `QueryRoster` | `LinkCommand` (`slot` = page) | `LinkRoster` (0x106) | never |
| 11 | `QueryMatchSetup` | `LinkCommand` | `LinkSetupState` (0x107) | never |
| 12 | `SetMatchSetup` | `LinkCommandEx` (0x110) + `LinkMatchSetup` | one `LinkReply` when it **finishes** (§3.4.2) | yes |
| 13 | `ApplyTuning` | `LinkCommand` | `LinkReply` (+ the QueryTuning answer when `kLinkFlagQueryAfter`) | yes (the tuning gate) |
| 14 | `QueryTuning` | `LinkCommand` (`slotMask`, 0 = all) | `LinkTuningGlobal` (0x108), then one `LinkTuningSlot` (0x109) per requested slot | never |
| 15 | `EndAuthoring` | `LinkCommand` | `LinkReply` | yes |
| 16–31 | reserved: authoring growth | | | |
| 32–63 | **reserved: Training / TAS** (§3.8) | | | |

| `IpcKind` | Name | Direction |
|---:|---|---|
| 0x105 | `LinkCaps` | DLL → editor |
| 0x106 | `LinkRoster` | DLL → editor |
| 0x107 | `LinkSetupState` | DLL → editor |
| 0x108 | `LinkTuningGlobal` | DLL → editor |
| 0x109 | `LinkTuningSlot` | DLL → editor |
| 0x10A–0x10F | reserved DLL → editor | |
| 0x110 | `LinkCommandEx` | editor → DLL |
| 0x111–0x11F | reserved editor → DLL | |
| 0x120–0x13F | **reserved: Training / TAS** (both directions; §3.8) | |

**New `LinkCommand.flags` bits:**

| Bit | Name | Meaning |
|---|---|---|
| `1<<5` | `kLinkFlagQueryAfter` | `ApplyTuning`: follow the reply with the QueryTuning answer (all slots) |

Bits 6 and 7 are reserved. Bits 0–4 keep their current meanings.

**New `LinkStatus` codes.** Old editors never see them, because only new ops return them.

| Code | Name | Meaning |
|---:|---|---|
| -9 | `NeedsRestart` | the request needs a process restart: TEAM without `PCHOST_MBAACC_2V2=1` at boot, or a `parkX` change |
| -10 | `Unsupported` | valid, but this build or scene can't do it (e.g. `scene = Training` with `mode = Tag`) |
| -11 | `Ineligible` | a pick is banned in this mode: a duo in TAG/TEAM, or no tag data (`TagCss.hpp tagEligibility`) |
| -12 | `Timeout` | `SetMatchSetup` did not reach `Ready` within 20 s |

**Answer rate.** At most **one** answer per frame for each of `QueryCaps`, `QueryRoster`, `QueryMatchSetup` and `QueryTuning`. A QueryTuning answer (1 global + up to 4 slots) counts as one. This is the same rule as QueryState, QueryStage and QueryTag in `linkFrame`.

### 3.3 Byte layouts

Every struct is pointer-free, little-endian, fixed-width, and uses **no 64-bit fields**: masks are `uint32_t[2]`, so i686 and x64 layouts are identical without alignment questions. Offsets are given for the static_asserts on **both** sides (PC `tests/mbaacc_link_authoring`, HC `game_link_proto.h`).

```cpp
// ---- editor -> DLL: the extended command (IpcKind 0x110). IpcHeader.size = 8 + size (<= 1024). ----
struct LinkCommandEx {            // 8 B header, then `size` payload bytes
    uint16_t op;                  // +0  LinkOp (only SetMatchSetup = 12 in revision 1)
    uint16_t seq;                 // +2  echoed in the LinkReply
    uint16_t size;                // +4  payload bytes that follow (SetMatchSetup: 64)
    uint16_t _pad;                // +6  0
};
static_assert(sizeof(LinkCommandEx) == 8);

// ---- a character pick ----
struct LinkPick {                 // 4 B
    int16_t  chara;               // +0  g_CharaSelectDataTable index (the charaselect id LinkCommand.SetChar uses);
                                  //     -1 = empty (a TAG partner: this side plays solo; a point: BadArgs)
    uint8_t  moon;                // +2  0 crescent, 1 full, 2 half
    uint8_t  palette;             // +3  0-based; must be < min(<file1>.pal count, 256) (the SetChar bound)
};
static_assert(sizeof(LinkPick) == 4);

// ---- the match setup (SetMatchSetup payload; also inside LinkSetupState) ----
constexpr uint8_t kMatchSetupVersion = 1;
enum class LinkMode  : uint8_t { Versus = 0, Tag = 1, Team = 2 };
enum class LinkScene : uint8_t { Auto = 0, Training = 1, AuthoringVs = 2 };  // Auto = Training for Versus, else AuthoringVs
constexpr uint8_t kSetupAssists     = 1u << 0;  // TAG: FN1 assists on for this match (the "TAG Battle" vs "no assists" preset)
constexpr uint8_t kSetupKeepBgm     = 1u << 1;  // stage change keeps the music (kLinkFlagKeepBgm semantics)
constexpr uint8_t kSetupForce       = 1u << 2;  // skip the loose-file preflight (kLinkFlagForce semantics)
constexpr uint8_t kSetupResetPos    = 1u << 3;  // hot path: do not restore point positions (kLinkFlagResetPos)
constexpr uint8_t kSetupHotOnly     = 1u << 4;  // fail (Unsupported) instead of taking the cold path
constexpr uint8_t kSetupTuningFirst = 1u << 5;  // re-read the tuning files before loading (== ApplyTuning first)
                                                // bits 6,7 reserved (0)
struct LinkMatchSetup {           // 64 B
    uint8_t  version;             // +0  kMatchSetupVersion (the DLL refuses others: BadArgs)
    uint8_t  mode;                // +1  LinkMode
    uint8_t  scene;               // +2  LinkScene
    uint8_t  flags;               // +3  kSetup*
    int16_t  stage;               // +4  BgList.ini id 1..99; 0 = keep the current stage; -1 = the game's random roll
    uint8_t  koRule;              // +6  0 oneDown, 1 allDown, 0xFF = the tuning's koRule
    uint8_t  timer;               // +7  0 infinite, 1/2/4 = ruleset timer speeds, 0xFF = the scene's default
                                  //     (AuthoringVs: infinite; Training: native)
    LinkPick slot[4];             // +8  ENGINE slots: 0 = P1 point, 1 = P2 point, 2 = P1 partner, 3 = P2 partner
                                  //     (TAG: side s owns s and s+2; TEAM: seat i = slot i; Versus: 2/3 must be empty)
    uint8_t  assist[2][5];        // +24 TAG CSS assist choice per side per direction (5,2,6,4,8): 0 = the tuning's action
                                  //     (sidecar / character default), k = kAssistMotionChoices[k-1] (TagCss.hpp)
    uint8_t  dummy[2];            // +34 RESERVED for Training (P2/P4 dummy mode). Must be 0 in revision 1.
    uint8_t  _reserved[28];       // +36 0
};
static_assert(sizeof(LinkMatchSetup) == 64);

// ---- QueryCaps answer ----
constexpr uint32_t kLinkCapStage     = 1u << 0;   // SetStage / ReloadStage / QueryStage
constexpr uint32_t kLinkCapTag       = 1u << 1;   // QueryTag
constexpr uint32_t kLinkCapRoster    = 1u << 2;   // QueryRoster
constexpr uint32_t kLinkCapSetup     = 1u << 3;   // SetMatchSetup / QueryMatchSetup / EndAuthoring / LinkCommandEx
constexpr uint32_t kLinkCapTuning    = 1u << 4;   // ApplyTuning / QueryTuning
constexpr uint32_t kLinkCapSidecars  = 1u << 5;   // this DLL reads povertycaster\tag\ (2.6)
constexpr uint32_t kLinkCapTeam4P    = 1u << 6;   // the process booted with 4 input slots (PCHOST_MBAACC_2V2=1): TEAM possible
constexpr uint32_t kLinkCapTrainingScene = 1u << 7; // LinkScene::Training reachable (CE build)
                                                  // bits 16..23 RESERVED: training ops; 24..31 RESERVED: TAS ops
struct LinkCaps {                 // 48 B
    uint16_t revision;            // +0  authoring revision (1 = this document)
    uint16_t _pad;                // +2
    uint32_t caps;                // +4  kLinkCap*
    uint32_t leverTableHash;      // +8  FNV-1a of the lever table (3.5); the editor refuses to interpret values on mismatch
    uint8_t  leverCount;          // +12 kLeverCount (59)
    uint8_t  perCharLeverCount;   // +13 42
    uint8_t  matchSetupVersion;   // +14 kMatchSetupVersion
    uint8_t  tuningWireVersion;   // +15 3 (4.2)
    char     build[16];           // +16 pchost build id (git short hash), NUL-terminated
    uint8_t  tuningSource;        // +32 0 defaults, 1 legacy tag_tuning.ini, 2 sidecars, 3 host/tape (adopted)
    uint8_t  tuningFlags;         // +33 same bits as LinkTuningGlobal.flags
    uint16_t charFiles;           // +34 chars\*.ini loaded
    uint8_t  _reserved[12];       // +36
};
static_assert(sizeof(LinkCaps) == 48);

// ---- QueryRoster answer (page = LinkCommand.slot, 0-based; 16 entries per page) ----
constexpr uint8_t kRosterDuo        = 1u << 0;  // File2 or TagType != 0 (banned in TAG and TEAM)
constexpr uint8_t kRosterTagOk      = 1u << 1;  // tagEligibility(..., Tag) == Allowed with the data the game loads
constexpr uint8_t kRosterTeamOk     = 1u << 2;  // tagEligibility(..., Team4P) == Allowed
constexpr uint8_t kRosterHasTcRow   = 1u << 3;  // has a kTeamChangeTable row
constexpr uint8_t kRosterNeedsMod   = 1u << 4;  // its tag patterns exist only in tag_mod data
struct LinkRosterEntry {          // 56 B
    int16_t  chara;               // +0  g_CharaSelectDataTable index
    uint8_t  selector;            // +2  CSS grid cell (kCssRoster)
    uint8_t  flags;               // +3  kRoster*
    char     file1[20];           // +4  lower-case File1 ("shiki")
    char     file2[20];           // +24 lower-case File2 ("" if none)
    char     name[12];            // +44 kCssRoster short name ("Tohno", "V.Sion")
};
static_assert(sizeof(LinkRosterEntry) == 56);
struct LinkRoster {               // 904 B
    uint8_t  page;                // +0
    uint8_t  pageCount;           // +1
    uint8_t  count;               // +2  valid entries on this page
    uint8_t  total;               // +3  entries over all pages (31 today)
    uint8_t  modDataLoaded;       // +4  teamcss::tagModDataLoaded()
    uint8_t  _pad[3];             // +5
    LinkRosterEntry e[16];        // +8
};
static_assert(sizeof(LinkRoster) == 904);

// ---- QueryMatchSetup answer ----
enum class LinkPhase : uint8_t { Unknown = 0, Boot = 1, Title = 2, MainMenu = 3, CharaSelect = 4, Loading = 5,
                                 Battle = 6, RoundEnd = 7 /* result / retry menu */, Other = 8 /* arcade/story/options */ };
enum class LinkAuthState : uint8_t { Idle = 0, ApplyingHot = 1, Rebuilding = 2, Ready = 3, Failed = 4 };
struct LinkSetupState {           // 192 B
    LinkMatchSetup requested;     // +0   the last SetMatchSetup (or the launch env's), zero if none
    LinkMatchSetup inForce;       // +64  READ BACK from the game: mode from g_GameMode, picks from the battle slot records
                                  //      + session config, stage = g_BGLoadedStageIndex, flags.assists from the session
                                  //      config (valid in Battle; zero elsewhere)
    uint8_t  phase;               // +128 LinkPhase
    uint8_t  authState;           // +129 LinkAuthState
    uint8_t  sessionFlags;        // +130 kLinkTagSess* (the reload gate's inputs; nonzero = no edits)
    uint8_t  editsAllowed;        // +131 1 = SetMatchSetup / ApplyTuning would pass the gate right now
    uint16_t lastSeq;             // +132 seq of the SetMatchSetup this state describes
    int16_t  lastStatus;          // +134 LinkStatus of the last finished SetMatchSetup (0 while running)
    uint32_t setupCount;          // +136 SetMatchSetup runs finished (ok or not)
    uint32_t gameModeKind;        // +140 g_GameModeKind raw
    uint32_t lastDurationMs;      // +144 wall time of the last finished SetMatchSetup
    uint8_t  lastPath;            // +148 0 none, 1 hot, 2 cold
    uint8_t  _pad[3];             // +149
    char     message[40];         // +152 what Rebuilding waits for / why Failed, NUL-terminated
};
static_assert(sizeof(LinkSetupState) == 192);

// ---- QueryTuning answer: one global message, then one message per requested slot ----
constexpr uint8_t kTunFlagLegacyIgnored   = 1u << 0;  // tag_tuning.ini present but the sidecars win (2.6)
constexpr uint8_t kTunFlagHotReloadPaused = 1u << 1;  // the F3 quick toggle "pause hot reload" is on
constexpr uint8_t kTunFlagParkXRestart    = 1u << 2;  // the files ask for a parkX other than the installed one
constexpr uint8_t kTunFlagGlobalCharSecs  = 1u << 3;  // global.ini still holds legacy [char.*] sections
constexpr uint8_t kTunFlagReadError       = 1u << 4;  // the last poll could not read a file (previous values kept)
struct LinkTuningGlobal {         // 344 B
    uint8_t  revision;            // +0   1
    uint8_t  source;              // +1   0 defaults, 1 legacy, 2 sidecars, 3 host/tape (adopted)
    uint8_t  frozen;              // +2   1 = session / replay / recording (tuningReloadVerdict != Allowed)
    uint8_t  leverCount;          // +3   kLeverCount
    uint32_t leverTableHash;      // +4
    uint32_t tuningLoads;         // +8   resolutions in this process
    uint16_t warnings;            // +12  warnings of the last resolution (all files)
    uint8_t  flags;               // +14  kTunFlag*
    uint8_t  sessionFlags;        // +15  kLinkTagSess*
    uint16_t charFiles;           // +16  chars\*.ini loaded
    uint16_t _pad;                // +18
    char     activeStyle[24];     // +20  resolved active style ("" = defaults), NUL-terminated
    char     sha[16];             // +44  first 15 hex digits: offline = the sidecar set's sha256; in a session / playback =
                                  //      the adopted payload's (4.2). NUL-terminated.
    uint8_t  _pad2[4];            // +60
    uint32_t tuningMask[2];       // +64  bit i (mask[i>>5] >> (i&31)): lever i set by global.ini [tuning] (or legacy [tuning])
    uint32_t styleMask[2];        // +72  lever i differs from Tuning{} because of the active style
    uint32_t envMask[2];          // +80  lever i overridden by PCHOST_MBAACC_TAG_* (edits to it have no effect)
    int32_t  values[64];          // +88  resolved GLOBAL values G (2.4), kLevers order, leverGet encoding
                                  //      (bool 0/1, enum index, motion packMotion); CharOnly = its default; [leverCount..63] = 0
};
static_assert(sizeof(LinkTuningGlobal) == 344);
constexpr uint8_t kTunSlotHasCharFile = 1u << 0;  // chars\<file>.ini (or a legacy [char.<file>]) contributed
constexpr uint8_t kTunSlotHasMoonSec  = 1u << 1;  // a [moon.<m>] section contributed
constexpr uint8_t kTunSlotCssAssists  = 1u << 2;  // CSS assist choices applied (partner slots)
constexpr uint8_t kTunSlotFromHost    = 1u << 3;  // values come from an adopted host/tape payload
struct LinkTuningSlot {           // 312 B
    uint8_t  slot;                // +0   0..3
    uint8_t  exists;              // +1   1 = a character is loaded in this slot (battle), else values = G
    uint8_t  moon;                // +2   0..2, 0xFF unknown
    uint8_t  flags;               // +3   kTunSlot*
    char     file[28];            // +4   lower-case data-file name, NUL-terminated ("" = none)
    uint32_t charMask[2];         // +32  lever i comes from the character layer ([char] / legacy [char.<file>])
    uint32_t moonMask[2];         // +40  lever i comes from [moon.<m>]
    uint32_t cssMask[2];          // +48  lever i comes from the CSS assist choices
    int32_t  values[64];          // +56  S(slot) (2.4), kLevers order, leverGet encoding
};
static_assert(sizeof(LinkTuningSlot) == 312);
```

### 3.4 Semantics

#### 3.4.1 QueryCaps and QueryRoster

* **`QueryCaps`** is answered at any time, including in a session. HC sends it on every (re)connect.
* **`QueryRoster`:**
  * `LinkCommand.slot` is the page (0, 1, …).
  * The entries come from the game's own `g_CharaSelectDataTable`, walked in `kCssRoster` order. The flags come from `teamcss::tagEligibility` with the data the game actually loads (`tagModDataLoaded()`).
  * A page number out of range gives `LinkReply BadArgs`.
  * HC also carries a static mirror (§5.3) for when no game is linked.

#### 3.4.2 SetMatchSetup

**Validation.** It runs on receipt and refuses immediately (a `LinkReply` with the status):
1. `version != kMatchSetupVersion`, or reserved bytes nonzero: **BadArgs**.
2. The gate (`gateVerdict`, session first): **RefusedSession / RefusedRecording / RefusedVariant**. `RefusedScene` does not apply, because any scene is acceptable (the cold path).
3. Mode and picks:
   * Versus: slots 0 and 1 set, 2 and 3 empty.
   * Tag: slots 0 and 1 set; 2 and 3 set or empty.
   * Team: all four set.

   Otherwise **BadArgs**. The chara id must exist, the moon must be 0..2, and the palette must be within the `.pal` bound, or **BadArgs**.
4. A duo or no-tag-data pick in Tag/Team: **Ineligible** (`tagEligibility`).
5. `mode = Team` without `kLinkCapTeam4P`: **NeedsRestart**.
6. `scene = Training` with `mode != Versus`: **Unsupported** (§7 Q1).
7. `stage` outside {-1, 0, 1..99}, or an id with no BgList entry: **BadArgs**. A missing or non-bgmake `.dat` is **MissingFile**.
8. Loose-file preflight (`.\data\<File1>_<moon>.txt` for every pick) fails without `kSetupForce`: **MissingFile**.
9. Another SetMatchSetup still running: **Busy**. The editor disables *Load in game* while `authState ∈ {1,2}`.

**Plan.** A pure function, `authoring::planSetup(current, requested) -> Plan` in the new `mbaacc/AuthoringSetup.hpp`, unit-tested.

* **HOT path.** Taken when all of these hold:
  * the phase is `Battle`;
  * `authState == Ready` (we built this battle);
  * `requested.mode == inForce.mode`;
  * the effective scene is equal.

  Steps, all on the game thread:
  1. With `kSetupTuningFirst`, reload the tuning files.
  2. Write the changed picks: slots 0/1 through the battle slot records, exactly as `requestSetChar` does; slots 2/3 through the session config partners (`tagSetSessionConfig` / `SessionPick`); the assists flag and choices through `tagSetCssAssists`.
  3. If the stage differs, run `stage::requestSetStage` (with `kLinkFlagKeepBgm` from `kSetupKeepBgm`).
  4. If any pick changed, run **one** ETM reload (`requestReload`, with `kSetupResetPos` → `kLinkFlagResetPos`).
  5. If nothing changed, reply `Ok "no change"` at once.

  Expect ~0.5–1.2 s.
* **COLD path.** Everything else. The steps:
  1. **Leave to the character select.**
     * From Battle, RoundEnd, MainMenu or Other: request scene 20 the way `driveBootTo(ModeTraining)` does (`SCENE_TRANSITION_CODE = MODE_CHARA_SELECT`, `NEW_SCENE_FLAG = 1`).
     * Before that, arm the game mode:
       * **Training**: `CharaSelect_InitVsTrainingMode(0)` (`addr::INIT_VS_TRAINING_MODE`).
       * **AuthoringVs / TAG / TEAM**: the VS setup (`GameMode_SetupVsPlayer`). The four-panel scene already runs it on a direct scene-200 entry (`2008fc8f`).
     * From Boot or Title: wait (`message = "waiting for the main menu"`). The authoring boot (§3.7) reaches the main menu quickly.
  2. **Arm the mode:**
     * TAG: `tagSetPresetMode(true, assists)` plus the session config partners.
     * TEAM: the TEAM arm.
     * Versus: neither. Clear any TAG preset.
  3. **Commit the picks without human input.**
     * TAG/TEAM: build a `TagCssResult` and hand it to the scene the way a viewer's is handed (`teamcss::setViewerTagResult` → `teamCssCommitViewer`). This path already exists for spectate/replay: "a viewer never picks".
     * Versus/Training: force selector, character, moon and palette every CSS frame (the `ReplayCodec::applySetup` / `driveCssSelection` model), and confirm.
     * Local pad input to the CSS is ignored until the commit.
  4. **Stage:** force `STAGE_SELECTOR` each frame until Loading (the `PCHOST_STAGE` mechanism). `-1` leaves the game's roll alone.
  5. **Loading → Battle.** On the first battle frame, **read back** everything below. Any mismatch is `Failed` with a message naming the field:
     * mode (`g_GameMode`);
     * all four slots' chara/moon/palette (battle slot records + session config);
     * the stage (`g_BGLoadedStageIndex`);
     * assists.
  6. `authState = Ready`, and reply `Ok`.

  Expect ~3–10 s. A 20 s budget ends in **Timeout**, and the game is left wherever it got to.
* **The reply.** Exactly one `LinkReply` per SetMatchSetup: an immediate refusal, or the final `Ok / Failed-status / Timeout`. `message` names the path and the time, e.g. `hot: 2 picks, stage 28 -> 16, reload 742 ms`. While it runs, `QueryMatchSetup` shows the progress.

**Authoring VS pins.** These hold while `authState ∈ {Ready}` and the scene is AuthoringVs. They are all offline-only, so they are inert in sessions by the existing gate.
* **Timer:** `timer` (default infinite): the ruleset's `_timerSpeed = 0` pin.
* **Endless round:**
  * A KO'd point is restored instead of ending the round. This reuses the stress harness's endless round (`MbaaccSim_Advance.cpp`, `pinTeamPoint`), generalised to "authoring".
  * It also keeps the known TAG round-end fault `0x448672 Match_IsLeaderCharDifferentFromStored` (HANTEI_GAME_LINK §6) out of the authoring loop.
* **P2 / P4 input:** whatever the binder maps; neutral when unbound. Real dummy control is the Training/TAS phase.

**EndAuthoring** clears the pins (the timer comes back from the ruleset, the endless round is off) and sets `authState = Idle`. The match continues as a normal offline match.

**Launch-time setup.** `PCHOST_MBAACC_AUTHORING_SETUP=<128 hex digits>` (the 64 bytes of `LinkMatchSetup`, lower-case hex) is queued as a SetMatchSetup with `seq = 0` on the first frame at which the main menu is reached. Its result is visible through `QueryMatchSetup`. An unparseable value is logged and ignored.

#### 3.4.3 QueryMatchSetup

This is answered at any time. How to derive `phase` (PC maps `addr::GAME_MODE`):

| Phase | Game state |
|---|---|
| Boot | splash/logo |
| Title | `MODE_TITLE` |
| MainMenu | `MODE_MAIN` |
| CharaSelect | scene 20 or the four-panel scene 200 |
| Loading | the loading scene |
| Battle | `MODE_IN_GAME` with fighters live |
| RoundEnd | the result or retry menu |
| Other | anything else |

`inForce` is valid only in Battle. `editsAllowed` is the gate verdict without the scene condition.

#### 3.4.4 ApplyTuning

* **Gate:** `tuningReloadVerdict`. A session gives `RefusedSession`, playback `RefusedSession`, and recording `RefusedRecording`. Any scene is fine.
* It re-reads all tuning files **now**, on the game thread at the next frame, resolves them, commits, and runs `afterCommit`.
* `flags & kLinkFlagReload` also runs one ETM character reload afterwards, and only in Battle; otherwise that part is skipped and the message says so. This is rarely needed, because tagIn/tagOut re-point without a reload.
* **Reply:** `Ok` with a message such as `sidecars: global + 4 chars, style 'Classic', 0 warnings, sha 1a2b3c4d5e6f708`. With `kLinkFlagQueryAfter`, the QueryTuning answer for all four slots follows.
* If a file could not be read, the reply is `Ok` with `kTunFlagReadError` set and the message `kept previous values: <file> busy`.

#### 3.4.5 QueryTuning

* It is answered at any time, including in a session. In a session it shows the **adopted** values (`source = 3`, `kTunSlotFromHost`).
* It sends `LinkTuningGlobal`, then `LinkTuningSlot` for each bit in `slotMask` (0 = all four), in slot order. Empty slots are sent with `exists = 0` and `values = G`.
* **Provenance masks:** a bit is set in the **highest** layer that set the lever. Layers from lowest to highest: style < `[tuning]` < env < char < moon < CSS. HC uses the masks for the "(style)", "(global)", "(env)", "(char)", "(moon)" and "(CSS)" badges.

#### 3.4.6 Gate summary

| Op | Offline | Session / replay / spectate / synctest | Recording armed | Non-CE |
|---|---|---|---|---|
| Query* | answered | answered | answered | answered |
| SetMatchSetup, EndAuthoring | runs | RefusedSession | RefusedRecording | RefusedVariant |
| ApplyTuning | runs | RefusedSession | RefusedRecording | runs (file IO only) |

### 3.5 Lever table hash (drift check at runtime)

FNV-1a 32 (offset basis `2166136261`, prime `16777619`). For each lever in `kLevers` order, feed in:
1. the key bytes and one `0x00`;
2. `kind` as a u8 (Bool 0, Int 1, Enum 2, Motion 3);
3. `scope` as a u8 (Sim 0, SimBoot 1, Harness 2, CharOnly 3);
4. `lo`, `hi` and `leverGet(Tuning{}, l)`, each as i32 LE;
5. `perChar` as a u8.

Both sides compute it from their own table:
* PC pins the value in `tests/mbaacc_tag_table`.
* HC's `tag_tuning_test` checks its mirror against PC's header when `PC_TAG_TUNING_HPP` exists, and against the pinned constant otherwise.
* At runtime HC compares with `LinkCaps.leverTableHash`. On a mismatch it shows the values as raw numbers only, with the banner `pchost.dll lever table differs from this Hantei-chan build (pc <hash> vs hc <hash>) — update one of them`, and never writes a lever it cannot name.

### 3.6 How Hantei-chan learns the game's state

| What | How often |
|---|---|
| `QueryState` | 10 Hz (existing) |
| `QueryMatchSetup` | 2 Hz while the Authoring window is open; 10 Hz while `authState ∈ {1,2}` |
| `QueryTag` | 5 Hz while the Live tab is visible (existing poll) |
| `QueryTuning` | after every `ApplyTuning` (`kLinkFlagQueryAfter`), after `authState` becomes `Ready`, and at 0.5 Hz while the Tuning or Live tab is visible |
| `QueryCaps`, `QueryRoster` | once per connection |

**Session detection** comes from `LinkSetupState.sessionFlags`, or `LinkTag.sessionFlags` on a rev-1 DLL. Any bit set means:
* the editor goes into **session lock** (§5.6);
* Load in game is disabled;
* the header says `session live — the game plays the host's tuning (sha …)`.

### 3.7 Launch environment (Hantei-chan → pc_inject)

`pc_inject.exe MBAA.exe pchost.dll` runs with cwd `<gamedir>` and the parent environment plus these variables:

| Var | Value | Why |
|---|---|---|
| `PCHOST_GAME` | `mbaacc` | |
| `PCHOST_MBAACC_LINK` | `1` | the dev link |
| `PCHOST_MBAACC_AUTHORING` | `1` | **new.** Authoring boot: mash straight to the main menu (`BootStop::MainMenu`), then run the launch setup. Implies the link. |
| `PCHOST_MBAACC_AUTHORING_SETUP` | 128 hex digits | **new.** The initial `LinkMatchSetup` (§3.4.2). |
| `PCHOST_MBAACC_2V2` | `1` **only when** the setup's mode is Team | 4 input slots are decided at boot (`wants2v2FromEnv`) |
| `PCHOST_LOG_TAG` | `authoring` | log file name |

* Do not pass `--training`. It hands control to the native menus. Authoring reaches Training itself, via the cold path.
* pc_inject loads `pchost.dll` from `<gamedir>` (the folder's `winmm.dll` shim trap from HANTEI_STAGE_LINK §6). That is the folder's own DLL, which is what the user runs.

### 3.8 Reserved room: Training / TAS (not in this phase)

Nothing below is implemented. It is listed so that revision 1 leaves the room and the names are not taken.

| Op | Name (reserved) | Sketch |
|---:|---|---|
| 32 | `ResetPositions` | position preset (training's own reset, or x/y per point) |
| 33 | `SetDummy` | P2/P4 dummy: stand / crouch / jump / CPU / record slot / playback slot, guard and tech options (uses `LinkMatchSetup.dummy[]` for the initial value) |
| 34 | `RecordSlot` / 35 `PlaybackSlot` | ETM-style record/playback slots |
| 36 | `Pause` / 37 `FrameStep(n)` / 38 `SetSpeed` | frame control (Training only; never in a session) |
| 39 | `SaveState(slot)` / 40 `LoadState(slot)` | via `IGameSim::save/load` (HANTEI_GAME_LINK §2, ETM piece 4) |
| 41 | `InputScript` (Ex) | TAS / moviemaker: a chunked input script (frames × pads), `LinkCommandEx` pages of ≤ 1016 B |
| 42 | `QueryTraining` | → `LinkTraining` (0x120) |
| — | kinds 0x120–0x13F | `LinkTraining`, `LinkInputStream` (paged), `LinkFrameInfo` (framebar) |

`LinkCaps.caps` bits 16–23 and 24–31 announce them. All of them are gated like SetMatchSetup and additionally need `authState == Ready`.

---

## 4. Netplay, replay and spectate carriage

### 4.1 The session tuning payload

This goes in the pure half: `MbaaccTagTuning.hpp` gets `serializeSessionTuning`, `deserializeSessionTuning` (which fails closed) and `sessionTuningDigest`.

```
off  size  field
0    1     kSessionTuningVersion = 1
1    1     entryCount (0..4)
2    109   the global block: serializeTuningBlock(G) — its own version byte (2) + field count (41) + 41 Sim/SimBoot levers
111  ...   entryCount entries, sorted by (name bytes, memcmp, shorter first on a common prefix; then moon):
             1        nameLen (1..27)
             nameLen  name: lower-case data-file name, [a-z0-9_] only
             1        moon (0..2)
             132      every perChar lever in kLevers order, value of S_nocss(f,m): Int/Motion 4 B LE, Bool/Enum 1 B
```

* **Which entries.** One per distinct (file, moon) among the match's occupied engine slots 0..3:
  * the file is the slot character's File1, lower-cased;
  * the moon is the slot's moon, with a partner's 0xFF resolved to its primary's moon.

  Before the characters are known (before the CSS commit), `entryCount = 0`.
* **Entry values** are `G ← [char.f] ← chars\f.ini [char] ← [moon.m]`: all 42 perChar levers, **fully resolved**, with no presence bits. That makes the payload independent of how the files were written: comments, order and redundant keys hash the same.
  * The CSS assist choices are **not** in the payload. They are in the setup blob (`MbaaccReplaySetup.tagAssist`) and in the lockstep CSS, as today.
  * The receiver applies them on top, exactly like the sender.
* **Decoding fails closed.** The payload is rejected whole (nothing adopted) when:
  * the length is wrong or bytes are left over;
  * a version is unknown;
  * the block fails `deserializeTuningBlock`;
  * `entryCount > 4`;
  * a name is empty, longer than 27 characters, or has a character outside `[a-z0-9_]`;
  * a moon is above 2;
  * any value is out of its lever's range;
  * the entries are not strictly sorted.

**Size budget:**

| Part | Bytes |
|---|---|
| header | 2 |
| global block | 109 |
| 4 × (1 + 27 + 1 + 132) | 644 |
| **max payload** | **755** |
| 0xCE control message (4-byte header + payload) | ≤ 759 (limit `kMaxControlPayload` = 1023) |
| `CH_SETUP_EXT` chunk (`ChunkHeader` 8 + ext header 4 + payload) | ≤ 767 (u16 len) |

`static_assert(kSessionTuningMaxBytes == 755)` is pinned in the test, and a lever change moves it.

### 4.2 The digest and the ruleset blob (tuning wire v3)

* Ruleset blob bytes 5..21 (the 17-byte `TuningWire`) change meaning:
  * `[5] = kTuningWireVersion = 3`;
  * `[6..21] = SHA-256(payload)[0..15]`.
* The host re-serialises the blob on every pre-attach send (existing #226 behaviour), so the digest follows the payload as the CSS result becomes known.
* **The tail layout does not change**, and `kSessionLayoutVersion` stays 3. Only the tuning wire's version byte moves, from 2 to 3.
* **The digest is the payload's integrity and identity check.** `LinkTag.sha` / `LinkTuningGlobal.sha` show it in a session, and the session-start log prints the full sha256 plus the payload hex, as the log prints the set today.
* **The per-frame syncHash does not change.** Tuning is adapter state, constant within a match and not savestated. That is exactly why it must be agreed before the first reader, which is §4.3's job.

### 4.3 Transport and timing (netplay)

**(a) Channel.**
* The payload rides the session control channel on **`0xCE`**, the adapter-addressed byte. The runtime already forwards every `0xCE` to `IGameAdapter::onControlMsg` in both receive paths (`RuntimeDriver_Netplay.cpp`, around lines 489 and 2523).
* MBAACC's CSS is lockstep (`lobbyCss() == false`), so MBAACC sends no other `0xCE` today. The PC agent must confirm this before claiming it.
* Layout: `[0] 0xCE, [1] 0xA7 (sub-kind: MBAACC TAG tuning), [2] proto = 1, [3] send counter (diagnostic), [4..] payload`.
* A receiver that sees another sub-kind or proto drops the message.
* This needs `MbaaccSim::onControlMsg` overridden. That is a one-line addition to `MbaaccSim.hpp`, a **hinokakera review item**. `onControlMsg` runs on the **network thread**, so the adapter only copies the bytes into a mutex-guarded slot (§(d)).

**(b) Host send.**
* In a live netplay session in which the host's session config says TAG, the host sends the current payload on **every frame** from the moment its match's (file, moon) set is known until the round attaches. Sending pauses while Attached and resumes before the next round, the same cadence as the ruleset chunk.
* "Known" means the four-panel CSS commit (`tagEffectiveConfig` points + partners) or the battle slot records, whichever comes first.
* Cost: ≤ 759 B/frame for the few seconds of CSS-commit → Loading → intro.
* 1v1 sessions send nothing.

**(c) Host self-adoption.**
* The host plays from `deserializeSessionTuning(serializeSessionTuning(x))`, never from its raw in-memory resolution. That guarantees both peers run the identical representation.
* The host's frozen session values (today's freeze at the session edge) are the input.

**(d) Client adoption.**
* The network thread stores the newest valid-looking message.
* The game thread (or the loading thread in §(e)) decodes it, and adopts it when **the payload's entry set equals the client's own match set**. The client knows its own set from the lockstep CSS result and the ruleset tail.
* Adopting means:
  * `s_active = G`;
  * the per-(file, moon) entries replace the character layer;
  * recompute the slots;
  * `tagRefillSlotPatterns`;
  * log `adopted host TAG tuning sha256=…`;
  * `source = 3`.
* The local sidecars and legacy ini are untouched and unread until the session ends. Today's unfreeze then triggers a normal offline reload.

**(e) The load-time wait: the timing guarantee.**
* H2 is the TagIn/TagOut fill inside `BattleScene_LoadCharacterData`. It runs on the **loading thread** and is the first code in a match that reads per-slot tuning. H4 (mode) and H3 (partner) read the session config, not tuning.
* In a TAG netplay session on the client, H2's first call per match **waits on a condition variable** until the adopted entry set equals the match set, for up to **5000 ms**.
* The game thread keeps running during Loading and keeps receiving control messages, so the payload lands. The PC agent must verify this with a log line (the ruleset chunk already relies on pre-attach reception).
* **On timeout:**
  * H2 proceeds with whatever has been adopted (the global block only if the entries never arrived, else the local values);
  * the adapter sets a refusal that `sessionRefusal()` returns on the next frame: `the host's TAG tuning did not arrive before loading (waited 5 s) — reconnect`.

  The session then ends on both sides instead of risking a desync.
* The CSS commit is on the same lockstep frame on both peers, and Loading takes hundreds of milliseconds at minimum. In practice the wait is 0 ms.
* A **rematch** with the same characters needs no new payload: it is the same digest, already adopted.

**(f) Defensive check.**
* After the round attaches, the client compares its adopted digest with the host blob's `[6..21]`.
* On a difference, `sessionRefusal()` returns `TAG tuning digest differs from the host's after adoption (bug) — refusing rather than desyncing`.

**(g) parkX (SimBoot).**
* The block carries `parkX`, and a host's value can differ from the client's installed byte patch.
* When the adopted `parkX` differs from `s_installedParkX`, the client **re-applies the parkX patch** before the attach, on the game thread, while no battle actor runs the park code (Loading/intro). The patch sites are the two `-65536` immediates in `TagMode_UpdateOffscreenMember` cases 0/101 (MbaaccTagTuning.hpp comment).
* If the PC agent finds that it cannot be re-patched safely, the fallback is to refuse the session with `host parkX X differs from this game's installed Y; parkX is boot-only — restart with the host's value`. That fallback is §7 Q3.

**(h) Old and new peers.**

| Host | Client | 1v1 | TAG |
|---|---|---|---|
| new (wire v3) | new | plays | client adopts; plays |
| new (wire v3) | old (expects v2) | plays | old client refuses with its existing `VersionUnknown` message ("update PovertyCaster") |
| old (wire v2) | new | plays | new client refuses: `the host's PovertyCaster predates embedded TAG tuning (wire v2) — both players need a build with Authoring Mode` |
| any | any, other `kSessionLayoutVersion` | refused (unchanged) | refused (unchanged) |

### 4.4 Replay and spectate: `CH_SETUP_EXT`

These are generic, **additive** changes (hinokakera review items). No existing chunk or struct changes.

* **`pc-replay/include/pc/replay/ReplayFormat.hpp`:** `CH_SETUP_EXT = 16`, with the payload `struct SetupExtChunk { uint8_t gen; uint8_t kind; uint16_t len; }` followed by `len` bytes. `kind 1` = the MBAACC TAG session tuning (§4.1). Other kinds are reserved per adapter. A reader that does not know 16 skips it, as it does 15 (`CH_GEN_STATE_Z`), so there is no file version bump.
* **`pc-core/include/pc/adapter/IReplayCodec.hpp`:**

  ```cpp
  // Optional per-gen setup EXTENSION, adapter-opaque, carried beside the 32-byte setup blob (CH_SETUP_EXT).
  // readSetupExt returns bytes written (0 = none); kind identifies the schema. applySetupExt is called wherever the same
  // gen's setup blob is applied (playback's CSS / Mode::Enter, spectate join), BEFORE that setup's loading starts.
  virtual size_t readSetupExt(uint8_t* out, size_t cap, uint8_t& kind) const { (void)out; (void)cap; kind = 0; return 0; }
  virtual void   applySetupExt(uint8_t kind, const uint8_t* in, size_t n) { (void)kind; (void)in; (void)n; }
  ```

* **Recorder** (`ReplayRecorder.cpp`): immediately after each `CH_GEN`, if `readSetupExt` returns more than 0, emit `CH_SETUP_EXT` **through the `IChunkSink` fan-out**, so the file and the spectate broadcast carry the same bytes.
* **Reader / player** (`ReplayReader.cpp`, `ReplayPlayer`): store the ext per gen, and call `applySetupExt` where that gen's setup is offered (the "Mash next-gen setup offer", `offerRoundSetupToRequestCodec`), before the loading.
* **Spectate** (`pc-spectate/src/LiveFeed.cpp`, `SpectatePlayer`): the same. Store per gen and apply with the gen's setup. The PC agent must confirm which spectate stack MBAACC uses (pc-spectate vs pc-spectate2) and plumb that one.
* **MBAACC** (`MbaaccSim_Replay.cpp`):
  * `readSetupExt` returns the current match's payload **only when the match is TAG**. That is the adopted one in a session, and the locally resolved one offline, so an offline `.pcrep` also carries it.
  * `applySetupExt(1, …)` adopts it exactly as §4.3(d) does (`source = 3`).
  * On a tape **without** the chunk (every older TAG tape), playback uses the local files, as today, and logs `tape predates embedded TAG tuning; playing with local files (may diverge)`.

### 4.5 What replaces the mismatch refusal

**In `MbaaccSim_Ruleset.cpp refuseBlob`:**
* **Removed:**
  * the `checkTuningAgreement(host, local)` digest comparison against the local files;
  * the `Mismatch` message "Copy the host's tag_tuning.ini…".
* **Kept:** the layout version check, and `tagSessionArmRefusal()`.
* **New:** when the host's session config says TAG, refuse if `in[5] != 3`, with the §4.3(h) message.

**`sessionRefusal()`:** keeps its existing checks and adds the §4.3(e) timeout and the §4.3(f) digest check.

**`MbaaccTagTuning.hpp`:**
* `checkTuningAgreement` and `tuningMismatchMessage` are reduced to the version logic.
* The pure test cases for them are rewritten to the new rules, not deleted.

**In one line:** two players with different sidecars (or none) can play TAG together, and they play the host's tuning.

---

## 5. Hantei-chan UX

### 5.1 One Authoring window

* It lives under **Windows → Authoring (MBAACC)**, also reachable with `--tool authoring`.
* It uses the shared link client (`gamelink::SharedClient`), so the editor still opens one pipe.
* The old **Tag / Team (experimental)** menu item now opens Authoring on the Tuning tab. `--tool tag` keeps working and maps to the same place.
* The Game Link window stays as the low-level console (follow, raw reload, stage, log).

```
+-- Authoring (MBAACC) ------------------------------------------------------------------------------------+
| Game  C:\games\mbaacc_tag                                   [...]   loose data ✓  pchost ✓  pc_inject ✓   |
| [Launch] [Attach] [Detach]     ● linked pid 41900  pchost 89a56ace  authoring rev 1  lever table ✓        |
| Phase BATTLE · Authoring VS · TAG      Setup: READY (hot, 742 ms)      Session: offline — edits live      |
+---------------------------------------------------------------------------------------------------------+
| [ Setup ]  [ Tuning ]  [ Live ]                                                                          |
+---------------------------------------------------------------------------------------------------------+
```

**States of the header line:**
* Not configured: the game dir is red, and Launch is disabled with the reason.
* Not linked: Launch and Attach are enabled.
* A rev-1 DLL:
  * `pchost.dll predates Authoring Mode — Setup/Load disabled; tuning edits go to <mode> files, game reads them if it supports them`;
  * the Setup tab is read-only;
  * the Tuning tab works on files;
  * if the DLL cannot read sidecars (no `kLinkCapSidecars`), the Tuning tab switches to **legacy document mode**: it edits `tag_tuning.ini`, which is today's panel behaviour.
* In a session: red `SESSION — host tuning adopted (sha …); editing locked` (§5.6).

### 5.2 Game dir, launch and attach

* **Game dir:**
  * Stored in `hanteichan.ini` as `[Authoring] gameDir=`.
  * Default: the linked process's folder (`gamelink::GameDirOf(pid)`), else the last used.
  * Checks: `MBAA.exe`, `pchost.dll` and `pc_inject.exe` present, and loose `data\`. A missing `data\` gives the warning `0002.p present: live reload needs extracted data (HANTEI_GAME_LINK §5)`. The last check is whether `povertycaster\tag\` exists; if it does not, the Tuning tab offers *Create* or *Migrate* (§2.7).
* **Launch** (`src/authoring/game_launcher.{h,cpp}`, on a worker thread):
  1. Snapshot the running `MBAA.exe` pids (`gamelink::FindGamePids`, plus each one's image path).
  2. `CreateProcessW` `<gamedir>\pc_inject.exe MBAA.exe pchost.dll` with cwd `<gamedir>` and the §3.7 environment. Its stdout goes to the Authoring log. It returns once pchost has attached (≤ 15 s).
  3. The new pid is the `MBAA.exe` whose image path is `<gamedir>\MBAA.exe` and which was not in the snapshot. Pin it (`Client::SetTargetPid`) and connect. The pipe appears within ~1 s of the first frame.
  4. The initial setup is already in the environment, so *Load in game* is not sent again. HC watches `QueryMatchSetup` until `Ready`, then opens the character tabs (§5.4).
* **Attach:**
  * Pick from the running `MBAA.exe` list (pid plus path). Choosing one in another folder asks for confirmation.
  * Connect, then `QueryCaps`, `QueryRoster`, `QueryMatchSetup`.
  * If the game is in an authoring battle, its `inForce` setup is loaded into the Setup tab.
* **Detach** closes the pipe and leaves the game running. *EndAuthoring* is a separate button in the Live tab.

### 5.3 The Setup tab

```
+-- Setup ------------------------------------------------------------------------------------------------+
| Mode  ( ) 1v1   (•) TAG   ( ) TEAM            Scene [Authoring VS ▼]  (Training: 1v1 only — see tooltip)   |
| Style [Classic ▼] (edits global.ini)  KO rule [tuning: oneDown ▼]  Timer [∞ ▼]  [x] Assists this match     |
| Stage [16 ▼ Tohno Mansion - Courtyard ] [Browse…]  [ ] keep BGM                                           |
|                                                                                                          |
|            P1 TEAM                                        P2 TEAM                                        |
|  Point   [ Tohno     ] (C)(F)(H)  pal [03 ▼ ■■■]          Point   [ V.Sion   ] (C)(F)(H)  pal [00 ▼ ■■■]   |
|  Partner [ Sion      ] (C)(F)(H)  pal [01 ▼ ■■■]          Partner [ Miyako   ] (C)(F)(H)  pal [07 ▼ ■■■]   |
|  Assists 5[tuning▼] 2[236A▼] 6[tuning▼] 4[tuning▼] 8[tuning▼]   Assists 5[..] 2[..] 6[..] 4[..] 8[..]    |
|                                                                                                          |
|  Characters (click = assign to the highlighted cell; right-click = assign to P2 cell)                     |
|  +--------+--------+--------+--------+--------+                                                           |
|  | Aoko   | Tohno  | Hime ⊘ | Nanaya | Kouma  |      ⊘ = duo (not in TAG/TEAM)                           |
|  +--------+--------+--------+--------+--------+      ! = no tag patterns without tag_mod data            |
|  | Miyako | Ciel   | Sion   | Ries   | V.Sion | Wara | Roa |   * = has chars\<file>.ini                   |
|  | Maids⊘ | Akiha* | Arc    | P.Ciel | Warc   | V.Akiha | M.Hisui |                                     |
|  | S.Akiha| Satsuki| Len    | Ryougi | W.Len  | Nero   | NAC |                                          |
|  | KohaMech⊘| Hisui | Neko  | Kohaku | NekoMech⊘ |                                                      |
|  +--------+--------+--------+--------+--------+                                                           |
|                                                                                                          |
|  [ Load in game ]   [x] open characters in the editor   [x] re-read tuning first                          |
|  last: READY — hot: P1 partner Shiki -> Sion, reload 742 ms                     requested ≠ in game: —     |
+---------------------------------------------------------------------------------------------------------+
```

* **The roster:**
  * Taken from `LinkRoster` when linked.
  * Otherwise from a static mirror `src/authoring/roster_mirror.h` of `kCssRoster` (selector, chara, name), with File1 from `kTeamChangeTable` (already mirrored in `tag_levers.h`). The drift test runs against PC's `MbaaccRoster.hpp` when it is on disk.
  * The grid is laid out in the CSS's own grid order.
* **Eligibility:** banned cells are greyed for the current mode, with the reason as a tooltip (`kRosterDuo`, `!kRosterTagOk`). In 1v1 everything is allowed. A duo point opens both File1 and File2 (§5.4).
* **Moon** is three toggle buttons.
* **Palette:**
  * The count is `<gamedir>\data\<file1>.pal`, first dword, capped at 256.
  * The swatch is drawn from that `.pal` with HC's `pal_file` loader: the first 3 colours of the chosen palette.
  * Out-of-range values cannot be chosen.
* **Stage:**
  * A combo filled from `bg::StageProject` on `<gamedir>\Bg\BgList.ini`: id plus name, with the entries training excludes {55, 57, 58, 99} marked.
  * *Browse…* opens the existing Stage Browser with a new `BrowserHooks::pickForSetup(int id)` callback, which is set only while the Setup tab asked.
  * Also offered: "keep current" (0) and "random" (-1).
* **Style:** built-ins plus `global.ini` styles. Changing it edits `global.ini` `active_style`, with the panel's "drop [tuning] overrides" option. That is a tuning edit (undoable, live-applied), not a setup field.
* **KO rule:** "tuning" (0xFF) or an explicit override for this match. **Timer:** ∞ / slow / normal / fast.
* **Assists:**
  * The *Assists this match* checkbox is `kSetupAssists`.
  * The per-side direction row holds the CSS choices (`kAssistMotionChoices`), where "tuning" = 0.
  * These are **match settings**, like a CSS pick. They are never written to the sidecars, and the tooltip says so.
* **Load in game:**
  1. Validate locally, with the same rules as §3.4.2, so the user sees the reason before sending.
  2. With *re-read tuning first*, save the dirty tuning files first.
  3. Send `SetMatchSetup` (with `kSetupTuningFirst`).
  4. Open the characters (§5.4) **right away**, in parallel with the game's load.
  5. Show progress from `QueryMatchSetup` (`message`), then the result line.
  6. If `inForce` later differs from what was requested (e.g. someone used F3 → Reload), show `requested ≠ in game: P2 partner` with *Adopt game's* and *Re-send*.

### 5.4 Opening the characters

For each distinct (file, moon) in the setup, plus File2 for a duo point in 1v1:
* The path is `<gamedir>\data\<file>_<moon>.txt`.
* If a tab already shows that `.txt`, focus it. Otherwise `CharacterInstance::loadFromTxt(path)`, then `createViewForCharacter`, the same code as `--open`.
* The first tab gets focus: P1 point.
* The existing Game Link auto-reload-on-save picks them up with no extra wiring. `SlotMaskForFile` maps `shiki_1` → the slot that loaded `shiki`.
* A missing `.txt` gives a warning row in the Setup tab (`data\shiki_1.txt missing (packed data?)`). The game-side MissingFile refusal says the same.

### 5.5 The Tuning tab

```
+-- Tuning --------------------------------------------------------------------------------------------------+
| povertycaster\tag\   global.ini ●   chars\shiki.ini   chars\sion.ini ●   [+ character file…]   [Migrate…]   |
| [Save all] [Revert file] [Revert to checkpoint ▼] [Undo] [Redo]   [x] Live apply (0.4 s)   game: sha 1a2b3c4 ✓ |
| warnings: 0 new · 1 existing (▸)                                        env overrides in game: none          |
+-----------------------------------------------------------------------------------------------------------+
| [ Global & styles ] [ Shiki · P1 ] [ Sion · P1 partner ] [ V.Sion · P2 ] [ Miyako · P2 partner ] [ Other ▼ ] |
|                                                                                                           |
|  Global & styles:  Style [Classic ▼]  [Save as style… ]  [x] picking a style drops [tuning] overrides       |
|    Swap        cooldownTicks        [====|======] 120      (style)                                          |
|    Guards      guardSwapHit         [x]                                                                     |
|    ...                                                                                                    |
|    Park        parkX                -102400  (boot-time: restart the game to apply)  ⚠ game has -65536       |
|                                                                                                           |
|  Shiki · P1 (shiki_0.txt open ✓)            Layer: (•) All moons [char]  ( ) Crescent ( ) Full ( ) Half     |
|    ☑ cancelWindowTicks   [==|========] 6        (char)          game ✓                                      |
|    ☐ entryStyle          edge            (inherits global)                                                |
|    ☑ tagIn               [241 交代登場 ▼] [jump]    table default: 241                                     |
|    Assists  slot 5: entry[default▼] mode (•)command  [45  236A -> 105 百裂 ▼] [jump]  -> command 45 -> p105 |
|             slot 2: entry[drop▼]    mode (•)motion   [236C ] -> command 47 -> p107 $10000                     |
|             ...                                                                                            |
+-----------------------------------------------------------------------------------------------------------+
```

* **The document model** is `src/tag_tuning/tag_sidecar.{h,cpp}`: a `SidecarWorkspace` holding one `TagIni` for `global.ini` and one per character file.
  * `TagIni` gains `SecKind::Moon` and the bare `[char]` header.
  * Each document has its own saved bytes and dirty state.
  * Resolution mirrors §2.4, including the provenance.
* **Tabs:**
  * *Global & styles*: today's Global section, driven by the lever table.
  * **One tab per character in the current setup** (point, partner, P2 point, P2 partner), labelled with the role.
  * *Other ▼*: any other `chars\*.ini`, or a new one (from the roster).
* **The character tab:**
  * **Layer selector:** "All moons" edits `[char]`; Crescent/Full/Half edit `[moon.*]`. The default is the moon in the setup.
  * Every perChar lever has a row. The tick box means "this layer overrides". When it is off, the inherited value is shown greyed, with its source (global / style / char).
  * `tagIn` / `tagOut` use the HA6 pattern picker from the **open tab of that (file, moon)**, which Load in game guarantees is open, plus *jump*. It falls back to a number box.
  * **Assists:** today's slot editor. The command list comes from `<gamedir>\data\<file>_<moon>_c.txt` for the layer's moon, and "All moons" uses the setup's moon, with a note that ids may differ per moon.
* **Live apply** is on by default in Authoring:
  1. 0.4 s after the last edit gesture ends (`IsItemDeactivatedAfterEdit`, or 0.4 s of no change), save the dirty files (§2.8);
  2. then send `ApplyTuning | kLinkFlagQueryAfter`.

  With it off, **Save all** does the same. The header shows `game: sha … ✓` when every visible slot's `LinkTuningSlot.values` equal HC's own resolution for that slot. Otherwise it shows `game differs (3 levers) ▸`, which lists them with the likely cause: env override (`envMask`), hot reload paused, a read error, `parkX` boot-time, or a stale DLL.
* **Dirty, save and revert:**
  * `●` marks a dirty file.
  * *Revert file* reloads that file from disk.
  * **Checkpoints:** HC records the bytes of every sidecar file at each *Load in game* and at window open.
    * *Revert to checkpoint ▼* restores all files to a chosen checkpoint (then live-applies).
    * This is the "undo my whole experiment" button.
    * A revert is itself one undo step.
* **External changes:** the panel's existing 1 s stat, per file.
  * A clean document reloads silently.
  * A dirty one asks *Load theirs / Keep mine*.
  * A save is refused while that question is open.
* **Undo** (`src/tag_tuning/tag_history.{h,cpp}`):
  * One history for the whole workspace. Each entry is `{seq, label, file, before bytes, after bytes}`.
  * A slider drag is one entry, closed on deactivation. A style pick that also drops `[tuning]` keys is one entry.
  * Routing in `MainFrame` works like `stageUndoRedo`. While the Authoring window (or a child) has focus, **Ctrl+Z / Ctrl+Y** go to the tuning history. Otherwise they go to the active character's `UndoManager`. The Edit menu labels show which one ("Undo cancelWindowTicks (shiki)").
  * Undoing a change that was already applied **writes the file back and live-applies it**, so the game follows the undo. The files stay the truth.
* **Validation** is kept from the panel:
  * An edit may not add a warning.
  * Widgets clamp to the range.
  * Motions are validated as typed.
  * Env overrides (`envMask`) show a lock icon: `PCHOST_MBAACC_TAG_X overrides this in the game`.

### 5.6 The Live tab and the session lock

* **The Live tab** has today's Live content (QueryTag: points/reserves, tag state, cooldown, assist phase, slot table, *Follow point*). It adds:
  * a **resolved-values grid**: 59 levers × {global, slot 0..3}, from QueryTuning. Cells that differ from HC's resolution are marked, and each cell's tooltip shows its provenance badges;
  * *EndAuthoring* and *Re-read tuning* buttons.
* **The session lock.** It turns on when `sessionFlags != 0`.
  * The Tuning tab goes read-only and shows the **adopted** values (source 3) as the reference.
  * The banner reads `The game is in a session and plays the host's tuning. Your files are not used until the session ends.`
  * An **Unlock (edits apply after the session)** button makes the files editable again. Saves are allowed; `ApplyTuning` and live apply stay off. The game's freeze logs such edits as IGNORED, and today's unfreeze picks them up.
  * Setup: *Load in game* is disabled.

### 5.7 The in-game F3 Tag tab (PC side)

It becomes read-only, plus three quick toggles:

```
+-- F3 · Tag -----------------------------------------------------------------------+
| Tuning: SIDECARS  povertycaster\tag\  global + 4 chars   style 'Classic'   0 warn   |
| sha 1a2b3c4d5e6f708   loads 37   [ Re-read files ]  [ ] Pause hot reload            |
| Assists this match: [x]                         Edit in Hantei-chan: Windows > Authoring |
| (in a session: "HOST'S TUNING (adopted) sha … — frozen")                           |
|------------------------------------------------------------------------------------|
| lever              global   P1 pt(shiki C)  P2 pt(v_sion C)  P1 pa(sion C)  P2 pa  |
| cooldownTicks      120 s    120             120              120            120    |
| cancelWindowTicks  -1       6 c             -1               -1             -1     |
| tagIn              0        250 c           0                0              0      |
| ... (source letters: s style, g global, e env, c char, m moon, x CSS, h host)       |
|------------------------------------------------------------------------------------|
| live tag state / assist state (unchanged readout)                                   |
+------------------------------------------------------------------------------------+
```

* **Removed:**
  * the style picker;
  * the lever widgets;
  * *Save to ini*, *Save as style*, *Reset to defaults*;
  * `tagui::requestLive/requestStyle/requestSave/requestSaveAsStyle/requestChar`;
  * the DLL's `writeIni`.
* **Kept:** `requestReload`, now meaning *Re-read files*.
* **New:** `requestPauseHotReload(bool)`. While it is on, the poll does not reload, and `kTunFlagHotReloadPaused` is reported. `ApplyTuning` still reloads, because it is an explicit request.
* **"Assists this match"** flips the session config's assists for the current offline match. It does the same thing as `kSetupAssists`, and QueryMatchSetup's `inForce` shows it.
* The facade rule is unchanged: the UI TU names no address and only queues requests.

---

## 6. Work split and order

### 6.1 Branches

| Repo | Base | New branch / worktree |
|---|---|---|
| PovertyCaster | `mbaacc/link-tag` (`89a56ace`) **rebased onto `origin/main 6d0161d4`**. One known conflict: `src/MbaaccSim_TagTuning.cpp`, where main's session-edge freeze (`eabf2170`) meets link-tag's `tagFillLinkTag` / first-load change. Keep both. | `mbaacc/authoring` in `~/wt/pc-authoring` (AGENT_RULES: `~/wt`, build dir symlinked into `~/pcbuild`) |
| Hantei-chan | `feat/tagpanel` (`4277a05`), then merge `update/ex-mbac` for the stage browser. Checked: it merges cleanly (`git merge-tree`). | `feat/authoring` in `/mnt/c/dev/hantei-chan/wt/authoring` |

**House rules apply** (AGENT_RULES.md and the PC owner's rules):
* WIP-commit every 15–20 minutes and before builds. Never `commit -a` or `add -A`.
* No AI attribution in commits.
* Heavy jobs go through `~/pcbuild/pc_slot.sh` with `-j4`. Run one game at a time, and only on the pid you launched.
* Never touch `/mnt/c/games/mbaacc_tag`, and never run `pc_deploy_runtime`.
* Log with quill; never printf.
* **Shared files are review items for hinokakera:**
  * `MbaaccSim.hpp` (the `onControlMsg` override);
  * `pc-core` (`IReplayCodec`);
  * `pc-replay`;
  * `pc-spectate`;
  * `Proto.hpp`;
  * `MbaaccAddrs.hpp`, if an address is needed.

  Put them in separate, clearly titled commits and list them in the final report. If a change inside `RuntimeDriver*.cpp` turns out to be needed, **write it up** instead of editing (the design avoids needing one).

### 6.2 PovertyCaster tasks (in order)

| # | Task | Files | Tests |
|---|---|---|---|
| P0 | Rebase link-tag onto 6d0161d4 and resolve the conflict | `MbaaccSim_TagTuning.cpp` | the existing `mbaacc_link_tag`, `mbaacc_tag_tuning`, `mbaacc_tag_table`, `mbaacc_etm_reload_gate`, `mbaacc_stage_link`, `ipc_link_roundtrip` |
| P1 | **Proto:** §3.2/§3.3 ops, kinds, flags, status codes and structs, verbatim | `pc-proto/include/pc/proto/Proto.hpp` (review item) | **new** `tests/mbaacc_link_authoring`: every size and offset in §3.3; `ipc_link_roundtrip` sends a `LinkCommandEx` of 72 B and each new reply kind; every new message is ≤ 1024 |
| P2 | **Sidecar model (pure):** the char file grammar (`[char]`, `[char.<n>]`, `[moon.<m>]` and aliases), `resolveSidecars(globalText, {file → text})` with the §2.4 layers and provenance, legacy resolution untouched, `leverTableHash`, `static_assert(kLeverCount <= 64)` | `MbaaccTagTuning.hpp` | `mbaacc_tag_tuning`: grammar and warnings table; moon aliases; the legacy file split by hand into sidecars resolves identically for every name × moon; pinned `leverTableHash` |
| P3 | **Session payload (pure):** §4.1 serialize, deserialize and digest; `kTuningWireVersion = 3`; the agreement logic reduced to versions | `MbaaccTagTuning.hpp` | round trip; every fail-closed case in §4.1; sorting; digest independent of comments and key order; max size 755 with four 27-character names; host self-adoption idempotent (`serialize(deserialize(p)) == p`) |
| P4 | **Live tuning:** sidecar dir load + poll + read-error keep; legacy fallback + `legacyIgnored`; the per-slot moon (`tagTuningNoteSlotFile(slot, file, moon)`); provenance masks; `requestPauseHotReload`; the adoption API (`tagAdoptSessionTuning(payload, why)`, `tagSessionTuningPayload()`); the fill functions for `LinkCaps` / `LinkTuningGlobal` / `LinkTuningSlot` / `LinkRoster` | `MbaaccSim_TagTuning.cpp`, `MbaaccSim_internal.hpp` | pure pieces in P2/P3; live in P10 |
| P5 | **Link dispatch:** QueryCaps, QueryRoster, QueryMatchSetup, ApplyTuning, QueryTuning, EndAuthoring; accept `LinkCommandEx` in `linkFrame` (size checks; unknown ex op → `Unknown` reply); one answer per frame per query | `MbaaccSim_EtmLink.cpp` | `ipc_link_roundtrip` extension |
| P6 | **Authoring state machine:** validation (§3.4.2 1–9), `planSetup` (pure), the hot path, the cold path (scene 20 request, mode arm, viewer-result commit / forced 1v1 picks, stage force, readback), the AuthoringVs pins (timer, endless round), `EndAuthoring`, `PCHOST_MBAACC_AUTHORING(_SETUP)`, the phase mapping | **new** `include/mbaacc/AuthoringSetup.hpp` (pure), **new** `src/MbaaccSim_Authoring.cpp`, hook-up in `MbaaccSim_EtmReload.cpp`'s frame pump | **new** `tests/mbaacc_authoring`: validation table (every status); plan table (phase × mode × scene → hot/cold/refuse); hex env parse |
| P7 | **Netplay:** ruleset wire v3 in `serialize`; `refuseBlob` / `sessionRefusal` per §4.5; the host send (0xCE/0xA7) with its cadence; the `onControlMsg` override (review item) with a network-thread copy; client adoption; the **H2 load-time wait** (CV, 5 s) and timeout refusal; the post-attach digest check; the parkX re-patch (or the documented refusal); dev env `PCHOST_MBAACC_TUNING_WIRE=<n>` (send wire version n, for the mixed-build refusal test, like `PCHOST_MBAACC_RULESET_LAYOUT`) | `MbaaccSim_Ruleset.cpp`, `MbaaccSim_TagTuning.cpp`, `MbaaccSim_Tag.cpp` (H2), `MbaaccSim.hpp` (1 line) | pure: the refusal table of §4.3(h) through `refuseBlob` with synthetic blobs; the adoption set-match predicate |
| P8 | **Replay/spectate:** `CH_SETUP_EXT` (format, reader, recorder through the sink), `IReplayCodec::readSetupExt/applySetupExt`, the player and spectate apply points, MBAACC `readSetupExt/applySetupExt` | `pc-replay/*`, `pc-core/.../IReplayCodec.hpp`, `pc-spectate/*` (review items), `MbaaccSim_Replay.cpp` | `replay_roundtrip`: ext carried and applied before the gen's setup; a reader that does not know 16 skips it (a tape with the chunk still plays in an old-reader path); `spectate_stream`: ext fanned out byte-identical |
| P9 | **F3 read-only** + the three toggles (§5.7) | `ui/src/MbaaccUi_Tag.cpp`, `include/mbaacc/TagTuningPanel.hpp` | `tests/imgui_ui` build |
| P10 | **Samples:** `data/tag_sidecar/global.sample.ini`, `data/tag_sidecar/chars/shiki.sample.ini`; the note in `tag_tuning.sample.ini` that sidecars supersede it; a drift test like the existing sample test | `pc-adapters/mbaacc/data/` | `mbaacc_tag_tuning` sample drift |
| P11 | **Proof runs**, in the game dir the user approves (§7 Q2) | evidence goes to `/mnt/c/dev/hantei-chan/docs/tag_research/authoring/` | see §6.4 |
| P12 | `tools/pc_precommit.sh` clean (under pc_slot) | | |

`game_link_cli` (Hantei-chan's) is the test driver for P11, so PC does not need its own client. For early smoke before HC lands, `tests/ipc_link_roundtrip` can carry a tiny scripted client.

### 6.3 Hantei-chan tasks (in order)

| # | Task | Files | Tests |
|---|---|---|---|
| H0 | Branch; merge `update/ex-mbac` | | `./build.sh` |
| H1 | **Proto mirror:** everything in §3.2/§3.3 with static_asserts (sizes **and** offsets); `leverTableHash()` over the `tag_levers.h` mirror | `src/game_link_proto.h`, `src/tag_tuning/tag_levers.h` | `tag_tuning_test`: hash equals PC's (header compiled in when `PC_TAG_TUNING_HPP` exists) and equals the pinned constant |
| H2 | **Client:** `QueryCaps` on connect (rev-1 fallback on `Unknown`); `QueryRoster`, `QueryMatchSetup`, `SetMatchSetup` (Ex; plus `SetMatchSetupWhenConnected`), `ApplyTuning`, `QueryTuning`, `EndAuthoring`; decoding `LinkCaps` / `LinkRoster` / `LinkSetupState` / `LinkTuningGlobal` / `LinkTuningSlot` into `Snapshot`; the §3.6 poll cadence; keep the `h.size > 1024` guard | `src/game_link.{h,cpp}`, `src/game_link_api.h` (`GameLink_LoadSetupInGame`, `GameLink_GetSetupInfo`) | `game_link_test` |
| H3 | **Sidecar workspace:** `TagIni` + `SecKind::Moon` + bare `[char]`; `SidecarWorkspace` (load dir, per-file dirty, atomic write per §2.8, `.bak` once, external change per file); resolution with provenance (§2.4); `NewWarnings` across files | `src/tag_tuning/tag_ini.{h,cpp}`, **new** `tag_sidecar.{h,cpp}` | **new** `tests/tag_sidecar_test.cpp` |
| H4 | **Migration** (§2.7) including the verification and `--dry-run` | **new** `src/tag_tuning/tag_migrate.{h,cpp}` | `tag_sidecar_test`: the sample, the capture ini (`docs/tag_panel/capture_tag_tuning.ini`), and a synthetic file (CRLF, duplicate `[char.X]`/`[char.x]`, unknown keys, CP932 comments, commented-out `; [char.*]`) all migrate with identical resolution; refusal when sidecars exist; the rename only after verify |
| H5 | **Undo history** (§5.5) + MainFrame routing | **new** `src/tag_tuning/tag_history.{h,cpp}`, `src/main_frame.cpp` (routing) | `tag_sidecar_test` (history apply/revert writes the right bytes); `shortcut_router_test` still passes |
| H6 | **Authoring model (pure):** roster mirror + `LinkRoster` override; eligibility; setup validation (the same rules as §3.4.2 1–8, where decidable offline); setup ⇄ `LinkMatchSetup` bytes; files to open (duo → File2); palette count from `.pal`; env hex encoding | **new** `src/authoring/authoring_model.{h,cpp}`, `roster_mirror.h` | **new** `tests/authoring_model_test.cpp`: golden bytes for 3 setups (1v1 training, TAG with assists + choices, TEAM) that PC's `mbaacc_authoring` test decodes too (the same hex literals in both repos); validation table; files-to-open |
| H7 | **Launcher:** game-dir checks, `CreateProcessW` of pc_inject with the §3.7 env, stdout capture, the pid snapshot diff, pinning | **new** `src/authoring/game_launcher.{h,cpp}` | `authoring_model_test` covers the env block builder; the process part is manual |
| H8 | **UI:** the Authoring window (header, Setup, Tuning, Live), Load in game + tab opening, live apply, checkpoints, session lock, the `pickForSetup` hook on the stage browser, the menu entry, and `--tool authoring` / `--authoring-setup <hex>` / `--authoring-tab` startup options for captures; the old Tag / Team entry redirected; `tag_panel.cpp` widgets refactored into reusable functions over a `TagIni&` + `Values` | **new** `src/authoring/authoring_window.{h,cpp}`, `src/tag_panel.cpp` (refactor), `src/ui/main_frame_gamelink.cpp`, `src/background/bg_browser.{h,cpp}` (hook), `src/startup_args.cpp`, `src/main.cpp` | headless captures of every tab against the mock (§6.4) |
| H9 | **Mock DLL:** the new ops; a setup state machine (hot 300 ms, cold 2 s, `Ready` with `inForce = requested`; honours the validation errors); tuning answered by **HC's own resolver** over a fixture sidecar dir (with `envMask` / `hotReloadPaused` knobs); modes `rev1` (answers `Unknown` to QueryCaps), `session` (sessionFlags, adopted values, refusals) | `src/game_link_mock.{h,cpp}` | `game_link_test` |
| H10 | **CLI:** `caps`, `roster`, `setup-get`, `setup-set <mode> <p1:chara/moon/pal> <p2:…> [p3 p4] [stage N] [scene …] [assists on\|off] [hot]`, `tuning-apply [reload]`, `tuning-get [slot]`, `tag-migrate <gamedir> [--dry-run]`, `launch <gamedir> <setup args>` | `src/game_link_cli.cpp` | used by P11 |
| H11 | **Docs:** an "as built" section in this document, a pointer in `HANTEI_TAG_PANEL.md`, the copy in `Hantei_Docs/update_2026_09/` | | |

### 6.4 Acceptance criteria

**PovertyCaster half (P):**
1. All §3.3 struct sizes and offsets are pinned by `tests/mbaacc_link_authoring`. Every new message is ≤ 1024 B. `ipc_link_roundtrip` covers `LinkCommandEx` and every new reply kind.
2. Pure tests pass:
   * sidecar grammar and resolution, including moon layers and provenance;
   * **legacy ≡ sidecar** resolution on the sample and on at least 3 edited files;
   * payload round trip, the fail-closed table, 755 B max, digest stability;
   * the `refuseBlob` / `sessionRefusal` table of §4.3(h) / §4.5;
   * the `planSetup` and validation tables;
   * the pinned `leverTableHash`;
   * `CH_SETUP_EXT` carried, skipped and applied.
3. The golden setup hex from H6 decodes to the same fields in PC's test.
4. **Live proof (P11):** evidence in `docs/tag_research/authoring/`, with pchost log excerpts and screenshots:
   * a. A launch with `PCHOST_MBAACC_AUTHORING_SETUP` = TAG (Tohno C/pal 3 + Sion C vs V.Sion C + Miyako F, stage 16, assists on) reaches `Ready`. The readback matches every field. It takes ≤ 20 s from launch.
   * b. `setup-set` changing the P1 partner only takes the **hot** path in ≤ 2 s. `inForce` updates and positions are kept (training) or the round restarts (Authoring VS).
   * c. `setup-set` to 1v1 Training takes the **cold** path in ≤ 10 s. `g_GameMode == 0x1010`.
   * d. Edit `chars\shiki.ini` `cancelWindowTicks`, then `tuning-apply`. `tuning-get 0` shows the new value with `charMask` set, within one frame of the reply. Deleting the file restores the global value.
   * e. A `[moon.full]` override applies only when Shiki is Full (b and c with moon changes).
   * f. The F3 Tag tab has no edit widgets. *Re-read files* and *Pause hot reload* work (`kTunFlagHotReloadPaused` observed).
   * g. **Netplay loopback TAG** (AGENT_RULES standard loopback, with `PCHOST_MBAACC_TAG=1 …`) with **different sidecars** on host and client (e.g. host `cooldownTicks=60`, client none):
     * the session plays with no refusal;
     * both logs print the same payload sha256;
     * the client logs `adopted host TAG tuning`;
     * H2 wait ≤ a few ms;
     * **zero desyncs** over the standard 300 s run;
     * `tuning-get` on the client shows `source = 3` and cooldown 60.
   * h. The same loopback with `PCHOST_MBAACC_RULESET_LAYOUT` unchanged but a host built without P7 (or a simulated wire v2 via a dev env `PCHOST_MBAACC_TUNING_WIRE=2`) is refused in TAG with the §4.3(h) text. 1v1 plays.
   * i. The tape recorded in (g) plays back **with the client's sidecars removed**, and the replay checks compare clean. A pre-change TAG tape still plays, with the "predates embedded tuning" log line.
   * j. `pc_precommit.sh` is clean.

**Hantei-chan half (H):**
1. `./build.sh` succeeds, and every existing test suite passes (tag_tuning, game_link, cmdfile, undo_manager, shortcut_router, workspace_session, …).
2. `tag_sidecar_test`:
   * migration equivalence on the fixtures;
   * byte-preserving edits in `[char]` and `[moon.*]`;
   * atomic write leaves no `.tmp` on success;
   * `.bak` is written once;
   * external change handling;
   * undo/redo writes the exact previous bytes.
3. `authoring_model_test`: golden bytes, validation, files to open, env hex.
4. `game_link_test` against the mock:
   * caps negotiation, including the rev-1 fallback, where Setup is disabled and there is no Ex send;
   * the setup state machine and the reply;
   * ApplyTuning + QueryAfter decoding;
   * the session lock and the refusals;
   * a lever-hash mismatch banner, with no writes.
5. Headless captures (mock, no game), with the sidecar fixture's md5 unchanged by opening, drawing and switching tabs:
   * Setup (TAG, one banned duo cell, palette swatches, the stage combo);
   * Tuning (global, a char tab with the Full layer selected, the assists);
   * Live (the resolved grid with one mismatch highlighted);
   * the session lock.

**End to end (with the user's approved game dir):**
1. In Hantei-chan: set the game dir, pick TAG, click Tohno (C, pal 3), Sion, V.Sion, Miyako (F), stage 16, then **Launch**. The game boots into that TAG match (Authoring VS, infinite timer), and the four characters are open in tabs. ≤ 25 s.
2. Change P1's partner to Akiha F, then **Load in game**: hot path, and the Akiha F tab opens.
3. Edit Akiha's `tagIn` (pattern picker, Full layer), with live apply. The next tag-in plays the new pattern with no reload. Undo restores it in the game.
4. Edit the tag-in pattern's frames in the HA6 and save. The character auto-reloads.
5. Switch to 1v1 **Training** and Load: cold path. The HA6 tabs follow.
6. F3 shows the same sha and values as the Live tab.
7. Start a netplay TAG session with a peer whose files differ: no refusal, the editor is locked with "host tuning adopted", and no desync.
8. A game dir with only `tag_tuning.ini`: **Migrate**, and `tuning-get` for all four slots shows identical values before and after.

---

## 7. Risks and open questions

Each item carries a recommended answer. **Q2 blocks only the live proof runs**; everything else can start now.

| # | Question | Recommendation |
|---|---|---|
| Q1 | **TAG/TEAM in native Training (0x1110)?** OR-ing 0x100 onto 0x1010 fails every `== 0x1010` compare, so training's dummy, reset and no-round-end features switch off. FN1 is also training's dummy-control button (`Training_IsDummyControlHeld` 0x477D90), which collides with the assist button. This is unaudited (gap G3). | Phase 1 uses **Authoring VS** for TAG/TEAM (infinite timer, endless round, P2 neutral), and native Training for 1v1 only. The G3 audit and a TAG-aware dummy belong to the Training/TAS phase. The UI says why Training is greyed for TAG. |
| Q2 | **Which game dir for PC's live proof runs?** AGENT_RULES forbid `mbaacc_tag` (the user's) and forbid new copies without the user's OK. `/mnt/c/games/mbaacc` is packed (`0002.p`), so live reload and the loose-file preflight cannot work there. | Ask the user for one approved agent copy, e.g. `mbaacc_authoring` with extracted `data\`, the same way as `mbaacc_tag`. Until then, the PC agent does everything except P11. |
| Q3 | **parkX (SimBoot) in an adopted session.** Can the two park immediates be re-patched safely before attach? | Yes, most likely: they are code bytes, written on the game thread during Loading when no park code runs. Implement the re-patch, log it, and test it in the loopback (P11g with differing `parkX`). If that proves unsafe, refuse the session with the §4.3(g) message. Never desync. |
| Q4 | **Moon layers now or later?** | Now. It is cheap (one more section kind and the slot's moon), and assist command ids really do differ per `_c.txt`. Leaving it out would force a second file-format revision. |
| Q5 | **0xCE for the tuning message.** It is the adapter-owned byte, but not reserved per adapter. | Use `0xCE` + sub-kind `0xA7`. PC confirms that MBAACC sends no other 0xCE, and adds the sub-kind to the `PeerHello.hpp` tag map comment. The fallback, a new top-level byte, needs a `RuntimeDriver` routing line, which would be a write-up for hinokakera. |
| Q6 | **The H2 wait on the loading thread.** Is it safe to block it for up to 5 s? | Yes, if the game thread keeps pumping the control channel during Loading. The ruleset chunk already relies on pre-attach reception, but the PC agent must log it once to prove it. If Loading turns out to starve the pump, move the wait to the CSS → Loading transition on the game thread (`simTickBlocked()` on the client until adopted). That is still deterministic, because the commit frame is lockstep. |
| Q7 | **TEAM mode needs 4 input slots from boot.** | `NeedsRestart`. HC offers **Relaunch with 4 players**: the same setup, launched with `PCHOST_MBAACC_2V2=1`. |
| Q8 | **Old TAG tapes** (before `CH_SETUP_EXT`) replay with the viewer's local files. | Accept it and log it. Those tapes were recorded under the old hash-must-match rule, so a viewer with the recording-era files reproduces them. |
| Q9 | **Hand edits while HC holds unsaved changes.** | The per-file *Load theirs / Keep mine* prompt (kept from the panel). With live apply on, HC rarely holds unsaved bytes for more than 0.4 s. |
| Q10 | **Should the session lock be strict (no saves at all) or unlockable?** | Unlockable, off by default. Saved edits cannot affect the running session (the freeze), and preparing the next set's tuning mid-session is a real use. |
| Q11 | **Roster names:** CCCaster short names ("Tohno", "Wara") vs full names. | Show the short name in the grid and the full name plus the file in the tooltip (from `LinkRoster.file1` / the TeamChangeData key). Names are labels only; ids travel on the wire. |
| Q12 | **Generic shared-file changes** (Proto.hpp, IReplayCodec, pc-replay, pc-spectate, the MbaaccSim.hpp line) need hinokakera's review before merge. | Land them as separate, additive commits on `mbaacc/authoring`, listed in the final report, and merge them together with the adapter work. The design needs **no** `RuntimeDriver*.cpp` edits. |
| Q13 | **A known pre-existing TAG netplay stall on the RetryMenu after the match** (memory note 2026-09-24) could mask P11g results. | Run P11g with `PCHOST_EXIT_AFTER_MATCHES=1`, or judge it on the first match. Record any stall separately; it is not this feature's defect. |

---

## 8. User decisions, round 2 (2026-09-25). These OVERRIDE anything above that conflicts

1. **Moons are separate characters.** Use one file per character+moon: `chars/<file>_crescent.ini`, `chars/<file>_full.ini`, `chars/<file>_half.ini`, each fully independent. An optional `chars/<file>.ini` holds values shared by all moons (the moon file wins). This replaces the `[moon.*]` sections.
2. **Apply is instant on every edit:** save → hot reload, with undo/revert still available.
3. **Styles are kept** as a named layer in global.ini. The setup picker chooses the style; per-char/moon values override it.
4. **Shipped defaults + local overrides.** The repo ships the default global and per-char/moon files. The user's edits live in a local overlay folder. A "Promote to defaults" action copies a value into the shipped set.
5. **_c.txt, HA6 and other base game files are READ-ONLY.** The tool reads them for pickers only. Everything tag/assist-specific lives in the sidecars. Editing base data is the future mod manager's job (a separate project, not now).
6. **Scope this phase:**
   - tag + assist tuning;
   - _c.txt read-only pickers;
   - HUD / tag HUD layout, edited by **dragging in a preview** in Hantei-chan (rendered with the game's art) and saved to a sidecar; the live game follows;
   - stage params through the existing stage browser + stage link.
7. **Netplay:**
   - the host sends its resolved tuning at session start;
   - the **host may edit between rounds**: changes are sent to the peer and applied at the next round start in lockstep;
   - the client sees a brief overlay note ("Host updated tuning: Shiki (F) tagIn, assist.6") and it is logged;
   - no mid-round edits.
8. **Training vs Authoring VS:**
   - 1v1 uses real training mode.
   - TAG/TEAM also get **real training mode via the four-panel CSS** routed into training (the user thinks the only missing piece is routing the 4P CSS into training). Audit what TAG in training needs, and keep **Authoring VS as the fallback** where training isn't safe yet.
   - The other side is simply the second controller/player (no dummy logic this phase).
9. **Hantei-chan UX:**
   - a **dedicated Authoring workspace**;
   - the character picker uses **dropdowns** (character, moon, palette per slot);
   - "Load in game" opens **all 2–4 picked characters as tabs**, with point highlighting and Follow point;
   - **named saved setups** plus a recent list;
   - **inline frame data** for the picked tag-in/out/assist moves (startup/active/recovery, hitboxes);
   - **MBAC reference values** for the 22 shared characters, with a "use MBAC value" button;
   - **A/B tuning snapshots** with a quick swap from Hantei-chan (no in-game hotkeys this phase);
   - a **filtered game-log pane**;
   - on a crash: **offer relaunch with the same setup** (show the exit code + last log lines).
10. **Launch:** Hantei-chan runs pc_inject + PovertyCaster on the shared dev folder and auto-attaches; it also attaches to an already-running game. The dev folder is **`C:\games\mbaacc_dev`** (extracted data\, the user's display settings copied from mbaacc_tag), shared by all agents with an owner lock (`C:\games\mbaacc_dev\.agent_lock`: write your agent name + PID before a run and remove it after; wait if another agent holds it).
11. **Public build:** Authoring ships in the normal Hantei-chan build (update/ex-mbac → PR #83) behind an **"Experimental: Authoring"** menu.
12. **Generic seams:** MBAACC now, but keep the link ops and the workspace game-agnostic where it's free (a game id in caps, per-game lever tables).

---

## 9. §8 details the two halves must agree on (written by the HC agent, 2026-09-25)

§8 changed the file layout and added features, but left some wire and file details open. Hantei-chan (`feat/authoring`) is built against the answers below. **PC agent:** if you choose differently, edit the line and mark it `CHANGED by PC agent`; HC follows.

### 9.1 Sidecar tree: shipped defaults + local overlay, one file per character+moon (§8.1, §8.4)

```
<gamedir>\povertycaster\tag\               SHIPPED DEFAULTS (deployed from PC pc-adapters/mbaacc/data/tag_sidecar/)
  global.ini                               [tuning] + [style.<name>]            (grammar §2.2)
  chars\<file>.ini                         [char]  values shared by every moon  (optional)
  chars\<file>_crescent.ini                [char]  Crescent only
  chars\<file>_full.ini                    [char]  Full only
  chars\<file>_half.ini                    [char]  Half only
  hud.ini                                  [taghud] layout (9.4)
<gamedir>\povertycaster\tag\local\         LOCAL OVERLAY: the user's edits. Hantei-chan writes ONLY here
  (the same names)                         (except "Promote to defaults", which writes the shipped file)
```

* A character file (shared or moon) holds `[char]` (synonym `[char.<name>]`) with any `perChar` lever (§2.3 rules). `[moon.*]` sections are **gone**: in a character file they are an unknown section (warning, skipped).
* `<file>` is §2.1's File1 rule. The moon suffix is exactly `_crescent`, `_full`, `_half` (lower case). A name that ends in one of these suffixes is a moon file of the stem before it; everything else in `chars\` that matches `[a-z0-9_]{1,27}\.ini` is a shared file.
* **Sidecar mode** (§2.6) is chosen when either tree holds `global.ini` or any `chars\*.ini`.
* **Resolution** (replaces §2.4; "<-" = later wins):

```
G      = Tuning{} <- style <- shipped global [tuning] <- local global [tuning] <- env
         active_style = the last active_style in (shipped [tuning] ..., then local [tuning] ...)
         style body   = local [style.<n>] (first instance) else shipped [style.<n>] (first instance) else built-in
S(f,m) = G <- shipped global [char.f] <- local global [char.f]         (legacy-tolerated, perChar only)
           <- shipped chars\f.ini <- local chars\f.ini                 (perChar only)
           <- shipped chars\f_<moon>.ini <- local chars\f_<moon>.ini   (perChar only)
           <- the CSS assist choices (partner slots)
```

* **Provenance masks** keep their §3.3 meaning: `charMask` = set by a shared character layer (`chars\f.ini` or a legacy `[char.f]`), `moonMask` = set by a moon file. `tuningMask` covers both `[tuning]` layers.
* **Migration (§2.7)** writes into `local\`. It refuses when `local\` already holds `global.ini` or `chars\*.ini`. It verifies that the legacy file resolves exactly like the **local tree alone**, and it lists every lever the shipped defaults will change on top.
* **Promote to defaults** (HC): moves a key from the local file to the same-named shipped file, then removes it from the local one. It is an undoable edit, like any other.

### 9.2 `LinkCaps` game id (§8.12)

`LinkCaps._reserved[12]` at +36 becomes `char gameId[8]` (+36, NUL-terminated, `"mbaacc"`) + `_reserved[4]` (+44). The size stays 48. An all-zero `gameId` means "mbaacc" (older revision-1 DLLs). HC picks its lever table by `gameId` and refuses to interpret values for a game it has no table for.

### 9.3 Host edits between rounds (§8.7) in `LinkSetupState`

`LinkSetupState._pad[3]` at +149 becomes:

| Off | Field | Meaning |
|---|---|---|
| +149 | `uint8_t sessionRole` | 0 offline, 1 netplay host, 2 netplay client, 3 viewer (replay / spectate / synctest) |
| +150 | `uint8_t betweenRounds` | 1 = a session is live and no round is running (result / retry / CSS / loading before attach) |
| +151 | `uint8_t _pad` | 0 |

* `editsAllowed` = 1 also for **host + betweenRounds**.
* `ApplyTuning` from the host between rounds is accepted. The reply is `Ok`, with the message `queued for the next round`. The DLL re-reads the files, re-serialises the payload, and sends it at the next round start (§8.7). A client or viewer gets `RefusedSession`.
* HC's session lock: locked when `sessionFlags != 0`, **unless** `sessionRole == 1 && betweenRounds == 1`. In that case edits save and `ApplyTuning` is sent, and HC shows `host: edits apply at the next round`.

### 9.4 `hud.ini`: the TAG HUD layout sidecar (§8.6)

The same two trees (a shipped file, then a local file on top, key by key). One section, `[taghud]`. The keys are the `TagHudSettings` / `TagHudLayout` fields (`pc-adapters/mbaacc/include/mbaacc/TagHud.hpp`), in side-0 640×480 coordinates. A rect is `x,y,w,h`:

```ini
[taghud]
enabled=1
nativeRedirect=1
reserveFace=1
partnerBar=1
teamName=1
banners=1
assist=1
swapCooldown=1
tweenMs=260
mainPortrait=0,0,256,96
mainSrc=0,0,256,96
reservePortrait=72,0,66,37
reserveSrc=0,0,96,54
partnerThumb=112,119,21,12
partnerBar.rect=134,122,139,8
swapSliver=134,131,139,2
assistLabel=134,136           ; x,y
assistPip=158,137,48,6
name=134,148,1                ; x,y,px
banner=134,147,176,11
```

* `partnerBar` is the bool, and `partnerBar.rect` is the rect, because the struct uses the same name for both.
* The DLL reads `hud.ini` at the same poll and on `ApplyTuning`, and applies it to `TagHudSettings` (presentation only, never on the wire).
* A bad value is a warning, and that key keeps its default.

### 9.5 Launch and log

* The Hantei-chan launch uses `PCHOST_LOG_TAG=authoring`, so the log is `<gamedir>\pchost_authoring.log`. HC tails that file for its log pane and for the crash report.
* The game dir for agents is `C:\games\mbaacc_dev`, with the `.agent_lock` rule (§8.10).

---

## 10. PC agent decisions and corrections (written by the PC agent, 2026-09-25)

PovertyCaster (`mbaacc/authoring` in `~/wt/pc-authoring`) is built against §9 as written. The items below are either corrections of numbers above or details the PC side had to decide. Each is marked **CHANGED by PC agent** where it changes something stated earlier.

### 10.1 CHANGED by PC agent: per-character lever counts and the payload budget

§2.5 and §4.1 miscounted the lever table. The code (and HC's `tag_levers.h`, same table, same hash `0x37BDB3FB`) has:

| Count | Doc said | Actual |
|---|---|---|
| `perChar` levers | 42 | **45** (16 tag-in/entry levers #8–23, `tagIn`, `tagOut`, `swapRedKeepPct`, 6 assist levers #30–35, 5 slot entries #39–43, 15 slot actions #44–58) |
| CharOnly levers | 17 | 17 |
| one payload entry's values | 132 B | **144 B** (33 levers × 4 B + 12 levers × 1 B) |
| max payload (4 entries, 27-char names) | 755 B | **803 B** = 2 + 109 + 4 × (1 + 27 + 1 + 144) |
| 0xCE message (8-byte header, §10.3) | ≤ 759 B | **≤ 811 B** (limit 1023) |
| `CH_SETUP_EXT` chunk | ≤ 767 B | **≤ 815 B** |

`LinkCaps.perCharLeverCount` is therefore **45**. PC pins 144 / 803 with a `static_assert` in `TagSidecar.hpp` and in `tests/mbaacc_tag_sidecar`.

### 10.2 Golden `LinkMatchSetup` bytes (§6.4 P3 / H6)

The same three literals are pinned in PC `tests/mbaacc_link_authoring/mbaacc_link_authoring_test.cpp`. HC's `authoring_model_test` should decode (and encode) them byte for byte. Chara ids are `g_CharaSelectDataTable` indices: Tohno 7, Sion 0, V.Sion 11, Miyako 8.

| Name | Setup | 64 bytes, hex |
|---|---|---|
| G1 TAG | TAG, Authoring VS, flags `Assists\|TuningFirst`, stage 16, koRule/timer default; Tohno C pal 3 + Sion C pal 1 vs V.Sion C pal 0 + Miyako F pal 7; P1's 2+FN1 = choice 1 (236A) | `010102211000ffff070000030b0000000000000108000107000100000000000000000000` + 28 × `00` |
| G2 Training | 1v1, Training, flags `TuningFirst`, stage 16; Tohno C pal 3 vs V.Sion C pal 0; partners empty | `010001201000ffff070000030b000000ffff0000ffff0000000000000000000000000000` + 28 × `00` |
| G3 TEAM | TEAM, Authoring VS, flags `TuningFirst`, stage −1, koRule allDown, timer infinite; Tohno C3 / V.Sion C0 / Sion C1 / Miyako F7 | `01020220ffff0100070000030b0000000000000108000107000000000000000000000000` + 28 × `00` |

### 10.3 CHANGED by PC agent: between-rounds host edits (§8.7) on the wire

§4.3 assumed one payload per session. §8.7 lets the host change it between rounds, so the payload now travels with an **epoch**, and both peers switch at the same round attach.

* **Message (0xCE / 0xA7), proto 2:** `[0] 0xCE [1] 0xA7 [2] proto = 2 [3] send counter [4..7] epoch (u32 LE) [8..] payload (§4.1)`. A receiver drops any other sub-kind or proto (the proto-1 layout in §4.3(a) is never sent).
* **Epoch** `e` = "the tuning in force from the e-th round attach of this session on". Epoch 0 = session start.
* **Host:**
  * At session start it freezes its resolved files as the *committed* state (epoch 0) and plays `deserialize(serialize(x))` of it, like any client (§4.3(c)).
  * Between rounds, `ApplyTuning` (host only, §9.3) re-reads the files into a *proposal*. The committed state does not change.
  * At its round-barrier entry for attach `e + 1` it freezes the proposal for the current match set as epoch `e + 1` and sends it every frame until that attach.
  * While not attached it also sends the committed state for the current match set as epoch `e` (this is what a client's H2 wait needs at a new match).
* **Client:** keeps the last few messages (network thread, mutex).
  * H2's first call of a match waits (≤ 5 s) for the epoch-`e` payload whose entry set equals the match set (§4.3(e)).
  * At the round attach it adopts the epoch-`e + 1` payload. This happens in the attach frame's `advance`, which is idempotent under rollback.
  * If that payload is missing at the attach, the session is refused instead of guessing: `the host's TAG tuning for round N did not arrive before the round started — reconnect`.
* **Both peers** switch exactly at the attach frame. That is the one point where both are held at the same state and no tuning reader has run. The between-rounds note (`Host updated tuning: shiki (F) tagIn, assist.6.command`) comes from diffing the two payloads (`describePayloadChange`), and is logged plus shown for 5 s on the client's overlay.
* **Defensive check (§4.3(f)), moved to the host:** the client sends an ack every frame after it adopts: `[0] 0xCE [1] 0xA8 [2] 2 [3] 0 [4..7] epoch [8..23] SHA-256(payload)[0..15]`. The host compares it with its own digest for that epoch. On a difference its `sessionRefusal()` ends the session: `TAG tuning digest differs from the host's after adoption (bug) — refusing rather than desyncing`. The ruleset blob's `[6..21]` carries the digest of the host's **committed** payload. It identifies the tuning; it is not a per-round gate.
* **Replay / spectate:** `CH_SETUP_EXT` is written per gen with the payload that plays in that gen, so a between-rounds change is on the tape at the gen where it applied.

### 9.6 Golden `LinkMatchSetup` bytes (H6 ⇄ PC `tests/mbaacc_authoring`)

These are the 64 bytes as lower-case hex, the `PCHOST_MBAACC_AUTHORING_SETUP` form. HC `tests/authoring_model_test.cpp` pins them, and the PC test should decode the same literals to the same fields.

| Setup | Hex |
|---|---|
| 1v1 Training: Tohno(7) C pal 0 vs Sion(0) H pal 5, stage 28, `kSetupTuningFirst` | `010001201c00ffff0700000000000205ffff0000ffff000000000000000000000000000000000000000000000000000000000000000000000000000000000000` |
| TAG (P11a): Tohno C pal 3 + Sion C vs V.Sion C + Miyako F, stage 16, assists + tuning-first; P1 2+FN1 = 1 (236A), P2 5+FN1 = 13 (236C) | `010100211000ffff070000030b000000000000000800010000010000000d00000000000000000000000000000000000000000000000000000000000000000000` |
| TEAM, Authoring VS: Arc(1) F pal 1, Ciel(2) C pal 2, Akiha(3) H pal 3, Nanaya(15) C pal 4 (slots 0..3), stage −1, koRule 1 (allDown), timer 0 | `01020200ffff01000100010102000002030002030f00000400000000000000000000000000000000000000000000000000000000000000000000000000000000` |

### 9.7 Counts to reconcile (HC agent finding)

HC's lever mirror (59 rows, identical to `MbaaccTagTuning.hpp` at PC main `236b8e30`) has **45** `perChar` levers, not 42. That is 28 perChar Sim levers plus the 17 CharOnly ones. The per-char entry is then **144 B**, not 132, and the §4.1 max payload is 2 + 109 + 4 × (1 + 27 + 1 + 144) = **803 B**. That is still under 1023. HC reports the counts it computes (`LinkCaps.perCharLeverCount` expectation = 45). The pinned lever table hash is **`0x37BDB3FB`**.

### 10.4 As built on the PC side (`mbaacc/authoring`)

| Piece | Where |
|---|---|
| Link structs, ops 9–15, kinds 0x105–0x110, statuses −9..−12, `kLinkFlagQueryAfter`; `LinkCaps.gameId` (§9.2); `LinkSetupState.sessionRole` / `betweenRounds` (§9.3) | `pc-proto/include/pc/proto/Proto.hpp` |
| Sidecar grammar, shipped ← local resolution with provenance, legacy path, the payload and its fail-closed decode, the wire v3, the epoch message | `pc-adapters/mbaacc/include/mbaacc/TagSidecar.hpp` |
| `leverTableHash()` = `0x37BDB3FB`, `static_assert(kLeverCount <= 64)`, the version-only agreement | `MbaaccTagTuning.hpp` |
| The carriage rules (host freeze/switch, client match-wait/switch), tested under loss | `TagCarriage.hpp` |
| `hud.ini` (§9.4) | `TagHudIni.hpp` + `MbaaccSim_TagHud.cpp applyHudIni` |
| SetMatchSetup validation / plan / hex / phase (pure) | `AuthoringSetup.hpp` |
| Live: files + poll + adoption; netplay carriage; link answers + the read-only F3 tab | `MbaaccSim_TagTuning.cpp`, `MbaaccSim_TagTuningNet.cpp`, `MbaaccSim_TagTuningLink.cpp` |
| SetMatchSetup hot/cold, QueryMatchSetup/Roster/Caps, EndAuthoring, Authoring VS pins, launch env | `MbaaccSim_Authoring.cpp` |
| TAG in real training (§8.8) | `MbaaccSim_TagTraining.cpp` (docs/authoring/pc/TRAINING_TAG_AUDIT.md) |
| Shipped defaults | `pc-adapters/mbaacc/data/tag_sidecar/{global.ini,hud.ini,chars/shiki.ini}` |
| Link CLI driver | `tests/mbaacc_link_cli` (`mbaacc_link_cli <pid> caps|roster|setup-get|setup-set <hex>|tuning-apply [query]|tuning-get [mask]|end|tag`) |

Behaviour the HC side can rely on:
* **`SetMatchSetup` scene Training + mode TAG is accepted** (§8.8): the game runs with `g_GameMode = 0x1110`, and 41 training compares are retargeted to a shadow word holding 0x1010. `QueryMatchSetup.inForce.scene` reads Training for it. **TEAM returns `Unsupported`** in this build ("TEAM setups are not implemented"): PC's TEAM is the fork 2v2, and its CSS commit is not wired to a setup.
* **`ApplyTuning` from the host between rounds** replies `Ok "queued for the next round"`, or `"queued for the round after next"` when this round's tuning is already frozen (the host is past its barrier entry).
* **Roles:** `sessionRole` 1 = netplay host, 2 = netplay client, 3 = viewer (replay / spectate / compat).
* **`LinkTuningGlobal.sha`:** offline it is the digest of the payload this machine would send for the loaded slots; in a session it is the adopted payload's.
* **Launch:** `PCHOST_MBAACC_AUTHORING=1` implies the link and sets `PCHOST_BOOT_STOP=main` (unless it is already set). `PCHOST_MBAACC_AUTHORING_SETUP` runs at the first main-menu frame.
* **Dev env:**
  * `PCHOST_MBAACC_TUNING_DIR=<dir>` replaces `<gamedir>\povertycaster\tag` (both trees move with it). This is how two loopback peers in one folder get different files.
  * `PCHOST_MBAACC_TUNING_WIRE=<n>` sends wire version *n*.
* **Deploy:** the shipped defaults are **not** copied by the build. Copy `pc-adapters/mbaacc/data/tag_sidecar/*` to `<gamedir>\povertycaster\tag\` by hand (deploy tooling is the release owner's call).

---

## 11. Hantei-chan as built (HC agent, 2026-09-25)

**Branch:** `feat/authoring` in `/mnt/c/dev/hantei-chan/wt/authoring`. It is `feat/tagpanel` with `update/ex-mbac` merged in (a clean merge) and 28 commits on top. It is built with the repo's mingw build (`build.sh` / `make -j4` under `pc_slot.sh`). It has not been pushed or merged.

### 11.1 Map of the tasks

| Task | What | Files |
|---|---|---|
| H1 | Protocol mirror: every §3.2/§3.3 op, kind, flag, status and struct, plus §9.2 `gameId` and §9.3 `sessionRole` / `betweenRounds`. Sizes and offsets are `static_assert`ed. `LeverTableHash()` is pinned at `0x37BDB3FB`. | `src/game_link_proto.h`, `src/tag_tuning/tag_levers.h` |
| H2 | Client: `QueryCaps` on every connect, with rev-1 fallback (no `LinkCommandEx` is ever sent to an old DLL). Roster paging, setup / tuning ops, `SetMatchSetupWhenConnected`, the §3.6 poll cadence, and waits for the CLI. | `src/game_link.{h,cpp}` |
| H3 | Sidecar workspace: two layers, per-file documents, atomic writes (tmp → flush → `MoveFileExW`), `.bak` once per session, external-change handling, the resolver with provenance, and the `NewWarnings` refusal across files. | `src/tag_tuning/tag_sidecar.{h,cpp}`, `tag_ini.{h,cpp}` (character-file mode) |
| H4 | Migration into `local\` (§9.1), verified before anything is written; `--dry-run`. | `src/tag_tuning/tag_migrate.{h,cpp}` |
| H5 | One undo history for the whole tree. Ctrl+Z / Ctrl+Y reach it while the Authoring window has focus (new `ShortcutContext::authoring`). Edit menu: "Undo tuning: …". | `src/tag_tuning/tag_history.{h,cpp}`, `src/ui/main_frame_gamelink.cpp`, `src/shortcut_router.h` |
| H6 | Pure model: roster mirror (checked against `MbaaccRoster.hpp`), eligibility, setup ⇄ wire ⇄ hex, validation, files to open, `.pal` count, launch env, saved setups, per-game lever tables. | `src/authoring/authoring_model.{h,cpp}`, `roster_mirror.h`, `authoring_policy.{h,cpp}` |
| H7 | Launcher: game-dir checks, `pc_inject` with the §3.7 env, stdout capture, the pid snapshot diff, exit / crash watch (exit code + the last `pchost_authoring.log` lines). It only ever kills its own pid. | `src/authoring/game_launcher.{h,cpp}` |
| H8 | The Authoring workspace (below) behind **Experimental: Authoring**. The old Tag / Team item opens it on Tuning; the lever widgets moved into `tag_widgets`. Adds the stage-browser `pickForSetup` hook and tab badges. | `src/authoring/authoring_window.{h,cpp}`, `authoring_tuning_ui.cpp`, `authoring_live_ui.cpp`, `authoring_state.h`, `edit_sink.h`, `hud_layout*.{h,cpp}`, `frame_summary.{h,cpp}`, `mbac_reference.{h,cpp}`, `src/tag_tuning/tag_widgets.{h,cpp}`, `src/background/bg_browser.*`, `src/ui/*` |
| H9 | Mock DLL: every new op. It has the setup state machine (validation → hot/cold → one reply), and answers tuning with HC's own resolver over a fixture tree. Knobs: rev1, session, host + between rounds, env mask, paused, skewed lever, foreign hash, team4p. | `src/game_link_mock.{h,cpp}` |
| H10 | CLI: `caps`, `roster`, `setup-get`, `setup-set`, `setup-hex`, `tuning-apply [reload]`, `tuning-get [slot]`, `end-authoring`, `tag-migrate <dir> [--dry-run]`, `launch <dir> <setup>`, and `mock-dll` with the knobs. | `src/game_link_cli.cpp` |
| H11 | This section; a pointer in `HANTEI_TAG_PANEL.md`; the copy in `Hantei_Docs/update_2026_09/`. | |

### 11.2 The workspace (§8.9)

* **Header:**
  * Game folder, with checks for MBAA, pchost, pc_inject and loose data.
  * **Launch**: pc_inject + pchost with the setup in `PCHOST_MBAACC_AUTHORING_SETUP`. It auto-attaches to the new pid, and opens the characters when the launch setup reaches Ready.
  * **Attach…** (a list of running MBAA.exe; another folder needs a second pick), **Detach**, and **Close game** (own pid only).
  * Link, caps, lever-table state, phase, and the setup state.
  * The policy banner: rev 1, lever-table mismatch, session lock (with an Unlock that only allows saves), host between rounds.
  * **Crash / exit panel**: exit code, the last log lines, and **Relaunch with the same setup**.
* **Setup:**
  * Mode (1v1 / TAG / TEAM), scene (Training is greyed for TAG/TEAM, with the reason), style (a global.ini edit), KO rule, timer, assists.
  * Stage combo from `Bg\BgList.ini` (training exclusions marked), plus **Browse…** → Stage Browser → "Use for the Authoring setup". Keep BGM.
  * Per slot: **dropdowns** for character (banned cells disabled, with the reason), moon (C/F/H), and palette (`.pal` count, colour swatches).
  * The per-side assist choices (match settings, never written to files).
  * Validation messages; **Load in game** (hot/cold via `SetMatchSetup`); "Relaunch with 4 players" for TEAM. Characters open as tabs right away.
  * The result line, `requested ≠ in game` with *Adopt game's* / *Re-send*, and **Save setup**.
* **Setups:** named setups (Use / Delete), the recent list (every Load / Launch), and the setup as hex. Stored in `hanteichan_authoring.ini` next to `hanteichan.ini`.
* **Tabs:** the picked characters' editor tabs are labelled `[P1 point]` etc. A team's current **point** is highlighted (`*`, amber tab). **Follow point** (Live tab) drives that team's point tab: pattern and frame every frame, and the tab itself on a tag.
* **Tuning:**
  * **Tree bar:** root and mode, *edit local overlay / SHIPPED defaults*, Save all, Revert file, **Revert to checkpoint** (window open + every Load / Launch), Undo / Redo, Live apply.
  * **A/B:** take A, take B, load A or B, **Swap A ↔ B**. Each is one undo step; the files are written and re-read by the game.
  * The game's sha ("matches these files" / "game differs (n)"), the warnings, *Load theirs / Keep mine* on external changes, and **Migrate** when a legacy `tag_tuning.ini` exists without a local tree.
  * **Global & styles:** the style picker (optionally dropping `[tuning]` overrides), Save as style, and every non-CharOnly lever. Each lever row has:
    * an override tick box;
    * a provenance badge (default / style / global, local or shipped);
    * a game cell (ok, a differing value with its likely cause, `env`, or `CSS`);
    * the **MBAC** value with **use MBAC**;
    * **promote**.
  * **Per character + moon** (one tab per pick, plus *Other…*):
    * Layer: All moons (`chars\<f>.ini`) or Crescent / Full / Half (`chars\<f>_<moon>.ini`).
    * **tagIn / tagOut:** a pattern picker over the open HA6 (read-only), *jump*, the table default, **inline frame data** (startup / active / recovery and a tick strip with active and invulnerable frames), and the **MBAC reference** (the MBAC number and the MBAC pattern's frame data) with **use MBAC value**.
    * **Assists 5/2/6/4/8:** entry, and the mode (inherit / pattern / command / motion) in this layer. Commands come from `_c.txt` (read-only). The resolved action is shown as command → pattern, with frame data and *jump*.
    * Then every other perChar lever row.
* **HUD:**
  * A 640×480 preview with the game's face art from `GRP\Gauge_AA\face`, both sides.
  * Drag to move, drag the corner to resize, Shift snaps to 8 px.
  * The key list shows each key's layer, reset and promote.
  * Everything is written to `local\hud.ini`. Its keys are identical to PC `TagHudIni.hpp` (20 keys).
* **Live:**
  * The teams (point / reserve / pattern / tag and assist state), Follow point, Re-read tuning, End authoring.
  * **The resolved grid:** 59 levers × {global, slots 0–3}. A red cell is where the game differs from HC's own resolution of the same files. Hover shows the provenance badges.
* **Log:** a tail of `<gamedir>\pchost_authoring.log`. Filters: tag/authoring lines only, problems only, a substring, follow the end, copy. It also shows the link and launcher logs.
* **Instant apply (§8.2).** Every edit is one undo step (`EditSink` Begin / Edited / End). It is saved at once (a drag saves about 6 times a second) and then `ApplyTuning | QueryAfter` is sent. Undo and redo write the files back and re-apply, so the game follows. In a client session nothing is applied, and saves happen only after Unlock. The host between rounds applies at the next round (§9.3 / §10.3).
* **Startup options:** `--tool authoring`, `--authoring-tab`, `--authoring-setup <hex>`, `--authoring-game`, `--authoring-tag-root`, `--authoring-char`, `--authoring-layer all|c|f|h`, `--authoring-view global`, `--authoring-link`, `--authoring-pid`, `--authoring-load`, `--authoring-unlock`, `--authoring-ab 1`. `--tool tag` without `--tag-ini` opens Authoring › Tuning.

### 11.3 Proof

| Suite | Result |
|---|---|
| `ctest` (all 17: cmdfile, cmdfile_ui_smoke, pattern_search, shared_clipboard, refs, bgm, hud, shortcut_router, undo_manager, workspace_session, package_tools, game_link, tag_tuning, tag_sidecar, authoring_model, frame_data, hud_layout) | **100% passed** |
| `tag_sidecar_test` | 4904 checks. Covers: names and the character grammar; resolution layering and provenance; byte-preserving edits; atomic write (no `.tmp`); `.bak` once; a save refused on a new warning; external change (clean / dirty / keep / theirs); undo and redo give the exact bytes; A/B; promote; migration of the sample, the capture ini and a synthetic file (CRLF, duplicate `[char.X]`/`[char.x]`, unknown keys, CP932, commented `; [char.*]`, a bad name, no final newline); refusal when `local\` exists; the rename only after writing. **The cross-check against PC `TagSidecar.hpp` `resolveSidecars` / `resolveSlot`:** identical values and masks for G + 24 character×moon slots on 3 trees. |
| `authoring_model_test` | 170 checks. Covers: HC's golden set (§9.6) and **PC's golden set G1–G3 (§10.2)**, encoded and decoded; struct sizes; a golden `LinkCaps` image; **every size, offset and constant compiled against PC `Proto.hpp`** (at `27b357cf`); the roster mirror against `MbaaccRoster.hpp`; the validation table; files to open; the env and env block; the `.pal` count; saved setups. |
| `game_link_test` | Against the mock: the rev-1 fallback (no Ex sent); caps, roster and setup; refusals (Ineligible, NeedsRestart, Busy, Unsupported); hot and cold paths with exactly one reply; `ApplyTuning + QueryAfter`, where an edited file shows up in slot 0; `QueryTuning` with a slot mask; EndAuthoring; client session lock; host between rounds; the foreign lever-table banner with no writes. |
| `frame_data_test` | 77 checks: synthetic walks (startup / active / recovery, loops, jumps) and the MBAC data for all 22 characters. |
| `hud_layout_test` | 75 checks. |
| `tag_tuning_test` | 3459 checks, with the pinned lever hash. |

**Captures:** `/mnt/c/dev/hantei-chan/docs/authoring/hantei/` (`tools/authoring/capture_authoring.sh`, against the mock, with the game folder read-only). The fixture tree's md5 was unchanged after all 15 (`fixture_md5.txt`). In the repo, `Hantei_Docs/update_2026_09/authoring/authoring_sheet.png` is a contact sheet of all 15.

The views:
* Setup: TAG, and with a banned duo;
* Setups;
* Tuning: global, Shiki Full layer, Shiki all moons, A/B;
* HUD;
* Live with a skewed lever and an env override;
* Log;
* session lock, and host between rounds;
* rev-1 DLL, lever-table mismatch, not linked.

**Two caveats:**
* The captures were taken before two small fixes: the HUD key list row layout (`9e716e6`), and the `styleMask` rule. So `08_hud_layout.png` shows the list rows run together.
* Captures were then **stopped at the coordinator's request**, because every capture opens a visible editor window. The script's header says so.

### 11.4 Deferred / open

* **The end-to-end run in `mbaacc_dev`** (§6.4 "End to end") was not done: no game was launched, per the coordinator's stop. Everything it needs exists: Launch from the header, `game_link_cli launch <dir> tag tohno/c/3 v.sion/c/0 sion/c/0 miyako/f/0 stage 16`, A/B, and undo.
* The Authoring window is a large floating window (with *Workspace › Fill the main window*). It is not a separate docking layout.
* The palette swatch shows 6 sampled colours of the palette, not "the first 3".
* The HUD preview draws the native gauge frame, bar and moon icon as outlines only, because the atlas sub-rects are not known. There is no tween or slide preview.
* The inline frame data is the plain pattern walk: no IF branches or landing (see `frame_summary.h`).
* HC's own warnings (used for the `NewWarnings` save refusal) are HC's texts, not the DLL's. The *values* are proven identical to PC's resolver; the warning *messages* are not compared.

## 12. Embedded game view (approved by the user 2026-09-25: "this sounds godlike")

The game renders inside a Hantei-chan panel.

- **Game side (pchost):**
  - At Present, copy the final backbuffer into a shared-memory frame ring: a named file mapping, current-user DACL, header {magic, version, width, height, pitch, format, frameSeq, gameFrame}, and 2–3 slots. Readers take the newest complete slot, detected by the seqlock pattern.
  - The name is announced over the dev link: a new capability bit plus a QueryFrameShare op.
  - "Embedded mode" hides/minimizes the real game window, but it keeps rendering at native res.
  - Input injection from the link feeds the normal controller input path, never key polling. It is offline/authoring only, and refused in netplay.
  - The copy costs ≤1 ms at 640x480; GPU readback is via GetRenderTargetData into a lockable system-memory surface.
- **Hantei-chan side:**
  - A dockable "Game" panel in the Authoring workspace. It uploads the newest frame to a GL texture every UI frame and keeps the aspect ratio (including sidebars).
  - Keyboard/pad input captured while the panel is focused is forwarded over the link.
  - Overlay layer on top: hitboxes and the selected box from the link state, pattern/frame numbers.
  - "Undock to real window" as a fallback.
- **Later:** zero-copy GPU sharing (D3D9Ex shared handle + WGL_NV_DX_interop2), and training/TAS controls (pause, frame step) in the same panel.
- **Constraint:** the frame ring's size and struct layout get static_asserts on both sides and golden values, like the other link structs. Rule: no visible test windows or screenshots without the user's OK.

### 12.1 Layered capture: stage rendered by Hantei-chan (user idea, 2026-09-25)

The goal is to edit stages live behind the real game's characters with zero game reloads. Hantei-chan's stage renderer matches the game at 99.4–100%.

- **Game side:**
  - In "layered" embedded mode, pchost skips the stage draw (Background_DrawInstance 0x4B7060, split into back layers and front layers).
  - It clears the capture targets to transparent and captures separate RGBA layers:
    - (L1) characters + effects + shadows;
    - (L2) HUD.
  - Additive/effect blends into a transparent target use D3DRS_SEPARATEALPHABLENDENABLE so alpha accumulates correctly; the output is premultiplied alpha.
  - Each frame record also carries the camera (x, y, zoom, the per-frame shake offset) and the stage-effect state the game applied: stage light/StageColorVal, the super-flash darkening amount, and the Heat blur.
  - The game still has a stage loaded for gameplay logic, so stage light values for custom stages come from Hantei-chan over the link (SetStageLighting).
- **Hantei-chan side, composite order:** back stage layers (its own renderer, at the game's camera and parallax) → L1 → front stage layers → L2 (HUD), with premultiplied blending. Stage edits apply instantly.
- **Frame ring format (§12) must support this from day one:** a layer count, a per-layer {offset, w, h, pitch, format, premultiplied flag}, and a per-frame camera/stage-effect block. The first version may ship with only layer 0 = the full composited frame.
- **Later:** the scratch stage slot, so a custom stage can be checked inside the real game; HUD authoring on layer L2, then HUD mods.

### 10.5 Verified live, and what that changed (2026-09-25)

The evidence is in `docs/authoring/pc/README.md`. Additions to §10.3 / §10.4:
* **Role detection:** a netplay client's `.pcrep` recorder calls `serialize()` before the host's ruleset chunk arrives. So "serialize ran" does not mark a host. The **client mark (refuseBlob) wins**, and a later serialize never takes the host role back.
* **Send redundancy:** the host sends the next round's payload **three times a frame** between its barrier entry and the attach. In the loss simulation this took attach refusals from 4% to under 0.1% at 15% loss.
* **Cold path route:** the cold path always passes through the **main menu** before the character select. A stock select entered straight from a TAG battle ignored the forced confirm; the four panels' port-input relocation, now undone for 1v1 setups, was the cause.
* **Readback moon:** `inForce` reads the slot's **loaded** moon (H2's load descriptor). The actor's moon field reads 0 for Full/Half picks.
* **Timing:** cold 1v1 Training takes about 14.5 s on this machine (≈ 4 s CSS + ≈ 10 s of the game's own load). TAG cold ≈ 10.6–10.9 s. Hot ≈ 0.4–0.6 s.

### 12.2 ADDED by Hantei agent: the frame ring and the link ops for the embedded game view (2026-09-25)

Hantei-chan `feat/game-view` (`/mnt/c/dev/hantei-chan/wt/gameview`) is built against this section. **PC agent:** copy `src/game_frame_share.h` verbatim next to `Proto.hpp` (it is pure: no Win32, no pointers, no 64-bit fields). Mirror the structs below in `Proto.hpp`. If you change anything, mark the line `CHANGED by PC agent`.

**The frame ring** (`src/game_frame_share.h`; Win32 open/create in `src/game_frame_ring.{h,cpp}`):

* **Mapping:** `Local\povertycaster-frames-<producer pid>`, current-user DACL (`D:P(A;;GA;;;<user SID>)`), read-only for readers. Its name is announced by `LinkFrameShare.name`.
* **Layout:**
  * `FrameRingHeader` (256 B);
  * then `slotCount` (2–3) slots, each a `FrameSlotHeader` (384 B) followed by `layerCapacity` (1–4) layer regions of `layerStride = PitchFor(maxW) * maxH` bytes;
  * `PitchFor(w)` = w·4 rounded up to 64 B.
* **Per frame (`FrameSlotHeader`):** seqlock `seq`, `frameSeq`, `gameFrame`, `presentMs` (latency), width/height, flags, `layerCount`, then:
  * **`FrameCamera` (64 B, §12.1):** camera x/y (1/128 px), zoom ×1000, shake x/y (px ×1000), stage id, StageColorVal ×1000, stage light ARGB, super-flash darkening ×1000, heat blur ×1000, and the 640×480 view rect inside the frame (sidebars);
  * **`FrameLayer[4]` (32 B each):** offset, w, h, pitch, format (1 BGRX8, 2 BGRA8), kind (0 FULL, 1 CHARS, 2 HUD), flags (bit 0 premultiplied), x/y placement, and an optional FNV-1a checksum;
  * **`FrameActor[4]`:** the fighters as drawn (x/y, pattern, frame, facing, team), so the overlay matches the picture.
* **Seqlock:**
  * The writer takes the slot after `latest`. It makes `seq` odd, writes, makes `seq` even, then stores `latest` and then `frameSeq`, all with release stores.
  * The reader acquires `latest` and `seq`, copies, fences, and re-checks `seq`, retrying a bounded number of times.
* **The first pchost version may fill only layer 0 = FULL.** Layered mode adds CHARS and HUD (premultiplied, stage not drawn) and should keep FULL too, so the panel can toggle without a round trip.
* **Golden sizes** (`static_assert`s, pinned in HC `tests/game_view_test.cpp`):

| Ring | Bytes |
|---|---|
| 640×480, 1 layer, 3 slots | 3687808 |
| 640×480, 3 layers, 3 slots | 11060608 |
| 854×480, 3 layers, 3 slots | 14931328 |
| max: 1920×1200, 4 layers, 3 slots | 110593408 |

  The golden test frame `DrawTestFull(640×480, frameSeq 1)` has FNV-1a **`0x1AF20C3D`**.

**Link additions:**

| What | Value | Payload | Gated |
|---|---|---|---|
| cap `kLinkCapFrameShare` | bit 8 | `QueryFrameShare` / `SetEmbedded` / `SetStageLighting` exist | |
| cap `kLinkCapInputInject` | bit 9 | `InputInject` exists | |
| op `QueryFrameShare` = 16 | `LinkCommand` → `LinkFrameShare` (kind **0x10A**, 96 B) | version (0 = no ring), slotCount, layerCapacity, maxW/H, ringBytes, flags, current w/h, framesPublished, `name[64]` | never |
| op `SetEmbedded` = 17 | `LinkCommand.slot`: 0 show the real window (export stays on), 1 embedded full frame, 2 embedded layered | → `LinkReply` | offline |
| op `InputInject` = 18 | `LinkCommandEx` + `LinkInputInject` (16 B): version 1, player 0–3, flags (bit 0 release), direction (numpad 1–9), buttons (A 1, B 2, C 4, D 8, E/FN1 16, FN2 32, Start 64), holdFrames 1–600, serial | → `LinkReply` | like `SetMatchSetup`: refused in netplay / replay / spectate / recording |
| op `SetStageLighting` = 19 | `LinkCommandEx` + `LinkStageLighting` (16 B): stageId (−1 = the stage on screen), flags (bit 0 reset), light ARGB, StageColorVal ×1000 | → `LinkReply` | offline |

* The injected state feeds the game's **normal controller input path**, where the binder writes; it never polls keys.
* The state holds `holdFrames` frames and then goes neutral. HC re-sends every 100 ms with hold 12, and sends a release on focus loss, so a stalled editor never leaves a button held.

**Hantei-chan side (as built):**
* The **Game panel** (`src/authoring/game_view.{h,cpp}`) is dockable, opened from *Experimental: Authoring › Game view* or the Authoring header.
* It opens the ring the link announces and uploads the newest frame's layers to GL textures once per UI frame. The picture keeps its aspect ratio, sidebars included.
* **Readouts:** fps, latency, frame and game frame, skipped frames, the producer's copy cost, and the embed state.
* **View › Full frame / Layered.**
  * Layered draws: Hantei-chan's stage **back** pass (a dedicated `bg::Renderer` into an offscreen target at the frame's camera and shake, using `bg_render`'s game-exact mapping) → super-flash darkening → CHARS → the stage **front** pass → HUD. Blending is premultiplied (`glBlendFuncSeparate(ONE, ONE_MINUS_SRC_ALPHA)` via an ImGui draw callback).
  * The stage source: the open stage tab, a test pattern (for the mock), or black.
* **Overlay:** from the frame's actors plus the link state's file/moon, it draws the open characters' hit/hurt/other boxes (mirrored by facing, with the box selected in the active tab highlighted), a cross at each origin, and slot / pattern / frame labels.
* **Input:** forwarded while the panel is focused, as player 1–4. Keyboard: arrows/WASD, J K L U = A B C D, I = FN1, O = FN2, Enter = Start. Pad through ImGui's XInput. While capturing, the editor's shortcuts are kept away from those keys.
* **Game menu:** Embed full / Embed layered / **Undock to real window** (`SetEmbedded 0`), and *This panel in its own OS window* (ImGui NoAutoMerge viewport).
* **Mock:** `game_link_cli mock-dll <s> frames [layered] [fps n] [size w h]` runs a fake producer. It draws an animated test stage + CHARS + HUD with a moving camera, shake and super-flash; injected input moves slot 0; stage lighting reaches the camera block.
* **CLI:** `frame-check <n> [layered]`, `embed <0|1|2>`, `input <player> <dir> <buttons> <frames>`.

**Proof (headless: no window was opened):**
* `game_view_test`: 125 checks. They cover:
  * the layout and goldens;
  * seqlock write/read, Unchanged, NoFrame, a stuck odd `seq` (→ Torn, never half a frame), a corrupt layer table, a wrong version;
  * a concurrent writer (20,000 frames) against a polling reader: 19,689 frames read, **0 bad**, and every layered one rebuilt FULL;
  * the SOCD input mapping;
  * **against the mock:** QueryFrameShare → open by name → 60 frames decoded with checksums and recomputed from `frameSeq`; `SetEmbedded 2` → 40 layered frames that rebuild FULL from stage@camera + CHARS + HUD while the camera moves; InputInject 6 for 30 frames moves slot 0 by exactly 30 × 4 px; SetStageLighting reaches the frames; a session refuses input; a DLL without the export answers Unknown and gets no inject.
* `game_link_cli frame-check` against `mock-dll frames`: 120 full frames (0 bad checksums, ~50 fps, 16 ms latency) and 90 layered frames (all 90 rebuilt FULL).

**Open / for the PC side:**
* The stage **front** pass is rendered over a transparent clear with straight-alpha blending, which gives alpha², so it is only approximately premultiplied. An exact fix needs the stage renderer to write premultiplied alpha (`glBlendFuncSeparate`).
* HC box coordinates assume 1 HA6 unit = 1 game pixel at zoom 1, with the origin at the actor's world position.
* Stage light / StageColorVal are carried but not applied by HC (the CHARS layer already has the game's lighting). The heat blur is passed to HC's renderer for the back pass.

### 12.3 PovertyCaster side as built (PC agent, 2026-09-26)

* **Branch:** `mbaacc/game-view` (`~/wt/pc-gameview`), on `mbaacc/authoring`, which is rebased onto origin/main `d3dc8af6`.
* **Evidence and numbers:** `docs/authoring/pc/gameview/README.md`.
* **Layout:** `game_frame_share.h` is copied verbatim to `pc-proto/include/pc/proto/` (same md5). The link structs, ops, kind and caps are mirrored in `Proto.hpp` exactly as in §12.2.

**Against the real game:**

* **Frames:**
  * 0 bad checksums, about 60 fps, HC-measured latency 32–47 ms.
  * The producer's copy cost is **~0.2 ms** a present, with a p95 under 0.3 ms at 624×351.
* **InputInject:** P1 walks for exactly the hold, then stops. It is refused on both netplay peers.
* **Synctest:** a TAG synctest with the export on has 0 desync.

**Changes against §12.2:**

* **CHANGED by PC agent: layered mode is not in this build.**
  * `SetEmbedded 2` answers `Unsupported` ("use mode 1"). Only layer 0 = FULL is published.
  * MBAA queues every draw into one priority-bucketed list: 0x5550A8, walked in ascending bucket order by `DrawCommandList_ProcessCommands` 0x4C0380. The stage uses buckets 600 (200 during a snapshot) and 10.
  * So a split is feasible by switching render targets at bucket boundaries. The character, effect and HUD bucket ranges still have to be mapped.
  * HC's `frame-check … layered` rebuilds FULL from the synthetic `DrawTestStage`, so it can never pass against the real game. A layered build needs its own check.
* **CHANGED by PC agent: ring name on regrow.**
  * A ring outgrown by the backbuffer is replaced by `Local\povertycaster-frames-<pid>-g<n>`, still announced by `LinkFrameShare.name`.
  * The old ring's `kFlagProducerAlive` is cleared. **HC: re-send QueryFrameShare when that flag clears.**
  * To make a regrow rare, the first ring is sized for the largest monitor, capped at 1920×1200. At 1920×1080 that is 24.9 MB.
* **CHANGED by PC agent: shake.**
  * `shakeX/Y` are always 0. MBAA's `Camera_ApplyShakeEffect` (0x44B8C0) already subtracts the shake from the working camera (0x55DEC4 / 0x55DEC8), so `cameraX/Y` is the camera as drawn.
  * HC must not add shake again.
* **CHANGED by PC agent: stage light.**
  * `stageLightArgb` is stored and reported, not applied. MBAA's stage "light" is a shadow position and power, not a colour.
  * StageColorVal *is* applied: it is written into the BgList entry `*(float*)(g_StageListEntries[id] + 44)` (0x74FC08 is an array of pointers), and restored on reset or on a stage change. It is not restored when the editor disconnects.
  * `stageColorValX1000` is unsigned, so a negative StageColorVal (stage 16 ships −0.05) reads as 0 and cannot be set.
* **CHANGED by PC agent: InputInject players.**
  * `player` 2–3 answer `Unsupported`: the offline writer only owns the P1/P2 pad words.
  * P2 answers `Unsupported` while the game's Training dummy drives it.
  * An injection is also cleared when the link client disconnects, so HC must keep its connection open. It does, re-sending every 100 ms.
* **CHANGED by PC agent: latency.**
  * The default deep readback publishes a frame two presents after it was drawn. Short mode (one present, `PCHOST_MBAACC_FRAMES_PIPE=2`) cost 1.6 ms a present.
  * The metadata (camera, actors, gameFrame, presentMs) is the metadata captured with the frame.
* **The frame can be smaller than 640×480,** for example 624×351 when the window is small. `FrameCamera.viewX/Y/W/H` is the scaler's picture rect: for example, `78,0 468×351` for a 4:3 picture between sidebars. It is 0 when the picture fills the frame.

### 12.4 Hantei-chan adapted to §12.3 (HC agent, 2026-09-26)

`feat/game-view` follows every §12.3 change:

* **Regrow.** When the open ring's `kFlagProducerAlive` clears, the panel re-sends `QueryFrameShare` (1 Hz) and re-opens the newly announced name (`…-g<n>`).
* **Shake is not added.** `cameraX/Y` is taken as drawn: the overlay, the layered stage camera, and `WorldToFrame` in `authoring/game_view_input.h` all ignore `shakeX/Y`.
* **The view rect frames the picture.**
  * The overlay maps world → frame through `FrameCamera.view`: the 640×480 picture is scaled to `viewW × viewH` at `viewX, viewY`, and all-zero means the picture fills the frame.
  * *View › Crop the sidebars* shows only that rect.
* **Stage light / StageColorVal:**
  * The light ARGB is shown as report-only.
  * *Game › StageColorVal override* sends `SetStageLighting`, with a note that it persists until *Reset* or a stage change, even after a disconnect, and that a negative StageColorVal cannot be sent (it reads 0).
* **Players.**
  * P3 and P4 are greyed out, with the reason.
  * P2 is greyed out while `g_GameModeKind == 0x1010` (the Training dummy). Input is not forwarded as a blocked player, and the panel says why.
* **Layered Unsupported.** When `SetEmbedded 2` answers `Unsupported`, the panel falls back to the full frame (it sends `SetEmbedded 1`) and shows why.
* **`frame-check … layered`** reports the refusal and checks full frames only. It rebuilds FULL from the synthetic stage only when the ring's producer is the mock.
* **Duplicate marker.** `src/game_frame_share.h` starts with `// DUPLICATE OF PovertyCaster pc-proto/include/pc/proto/game_frame_share.h — keep byte-identical; layout hash pinned in tests`.
  * `game_view_test` pins a layout hash (`0xF297FDB8`: every size, offset and constant of the ring).
  * It compares the file byte for byte with PC's copy after that first line (`PC_FRAME_SHARE_H`, default `~/wt/pc-gameview/...`): identical.
  * **PC agent:** add the matching `DUPLICATE OF` line on your side. The comparison skips the first line only when it starts with `// DUPLICATE OF`.
* **The mock mirrors §12.3** (`mock-dll … nolayered`, `regrow <n>`): layered is refused; P3/P4 and the Training P2 are refused; the ring regrows to `-g1`; a lighting override persists until reset.
* **Not changed:** the shared header's `FrameCamera.shakeX/Y` comment still says the shake is "added after the camera". It is left as is so the two copies stay byte-identical. The HC code follows §12.3, and the fix belongs in a joint edit of both copies.

### 12.5 Layered mode shipped; frames are the game's own 640×480 scene (PC agent, 2026-09-26)

`mbaacc/game-view` (rebased onto origin/main `cfa52cbc`) now answers `SetEmbedded 2`. This section supersedes these §12.3 bullets: "layered mode is not in this build", "ring name on regrow", and "the frame can be smaller than 640×480".

**CHANGED by PC agent: the frame is the game's scene target, not the back buffer.**
* Every frame, in modes 1 and 2, is the top-left 640×480 of the game's own scene render target, the surface both draw passes render into. It is 1024×512 X8R8G8B8 on the CE build: `*(*(*(0x5542D0)) + 12)`, bound by `Render_ProcessFrame` 0x433150.
* That is the picture before `Scene_DrawHudWithPostProcessing` scales it (with sidebars / fit modes) onto the back buffer.
* So:
  * the frame is always 640×480, pixel-exact;
  * there are no sidebars and no PovertyCaster overlay in it;
  * `FrameCamera.viewX/Y/W/H` is always 0;
  * HC's `screen = (world − cam) / 128 · zoom + (320, 432)` applies directly.
* **No regrow any more.** The ring is fixed at 640×480 × 3 layers × 3 slots = **11060608 B**, the golden size. Switching between modes 1 and 2 never regrows it. HC's regrow handling (§12.4) stays correct but idle.

**Layered (§12.1) as built.**
* **What goes in each frame.**
  * Every frame keeps FULL.
  * With `SetEmbedded 2`, frames also carry **CHARS** (kind 1) and **HUD** (kind 2): BGRA8, `kLayerPremultiplied`, 640×480 at 0,0.
  * The slot sets `kSlotLayered`, and the ring sets `kFlagLayered` while mode 2 is on.
* **How the layers are drawn.**
  * MBAA queues every draw as a node in one priority-bucketed list at 0x5550A8. There are 1600 bucket sentinels, walked in ascending order by `DrawCommandList_ProcessCommands` 0x4C0380, and a higher bucket draws on top.
  * Midhooks sit on the two `call DrawCommandList_ExecuteScene` sites in `Render_ProcessFrame`: the world pass at 0x4331D4 and the status pass at 0x4332DC.
  * Before each pass, the hook copies the pass's nodes into private per-layer lists (the technique the interpolation canary proved). It then runs the game's own `ExecuteScene` on each list into the layer target, so batching, samplers, shaders and colour blends are the game's.
* **Bucket table** (IDA plus an in-game survey; `mbaacc/FrameLayers.hpp`):

| Buckets | What | Layer |
|---|---|---|
| 0–10 | Stage band 0 | stage back (not drawn) |
| 11 | The HEAT `BgPointBlur` post pass. It samples the scene target, so it blurs the stage. | stage back (not drawn) |
| 266 | `Background_DrawScreenEffectOverlay`: the super-flash backdrop that replaces the stage | stage back (not drawn) |
| 256+p (306 … 456) | Fighters (`params+36 + 256`), shadows 366, effects | **CHARS** |
| 522 | DropObject | stage front (not drawn) |
| 600 | Stage band 1 (in front of the fighters) | stage front (not drawn) |
| ≥ 700, plus the whole status pass | HUD 704–716, menus | **HUD** |

* **CHANGED by PC agent: the premultiplied blend.**
  * Only the ALPHA columns of the game's blend-preset table (0x54CC50) are swapped while a layer draws: normal → `ONE / INVSRCALPHA`; additive → `ZERO / ONE`.
  * So an additive effect adds colour with **alpha unchanged**. That is the exact premultiplied form of light: `L over X` equals the game's additive draw on X.
  * "Additive accumulates alpha" would also darken the stage behind it.
  * The subtract and multiply presets (3 / 4) can only be approximated in a layer. The survey saw none in battle.
* **Verdict** (`mbaacc_link_cli layercheck`, with the producer run under `PCHOST_MBAACC_FRAMES_VERIFY=1`, which adds a second ring `…-verify` holding the same frames' stage-back and stage-front passes):
  * The check composites black → stage back → CHARS → stage front → HUD and compares the result with FULL per pixel.
  * Tolerance is 8. A frame passes with ≤ 5‰ of its pixels off, and CHARS must cover < 85% of the screen.
  * Results on the final build `4bcba320`: **every frame passes** across 9 stages (16, 3, 8, 12, 5, 9, 2, 1, 7) and 3 pairings (Tohno Shiki vs V.Sion, Aoko vs V.Akiha, V.Akiha vs Aoko). Moves covered: idle, 236C EX, 41236C Arc Drive (super-flash frames checked), ABC Heat, and V.Akiha's Heat (full-screen heat blur).
  * The typical maximum difference is 2/255.
  * Evidence: `docs/authoring/pc/gameview/runs/layers-*`.
* **Cost.** Full-mode copies take ~0.2 ms a present (median). Layered takes a median of 0.6–0.7 ms (p95 1.0–1.2 ms), plus ~0.2–0.3 ms to draw the split inside the game's render. The layered cost is mostly the 3 × 1.2 MB copy into the ring.
* **Netplay safety.** Layered forced on (`PCHOST_MBAACC_FRAMES_LAYERED=1`) gives a TAG synctest of 0 desync over 15,462 forced loads. A netplay loopback under NETSIM also gives 0 desync (chk 15028 each), and InputInject is refused on both peers.
* **For HC.**
  * `frame-check … layered` now receives real layered frames (3 layers, 0 bad checksums).
  * But it exits 1 against the real game: `layeredFrames` counts only mock rings, and `(!layered || layeredFrames > 0)` then fails.
  * Suggest: count a frame as layered when FULL + CHARS + HUD are present, and keep the synthetic-stage rebuild for the mock only.

### 12.6 Hantei-chan against layered mode (HC agent, 2026-09-26)

**Headless run against the real game** in `C:\games\mbaacc_dev`:
* pchost is `mbaacc/game-view 4bcba320`, launched by `game_link_cli launch` with `PCHOST_MBAACC_FRAMES=1 PCHOST_MBAACC_FRAMES_CHECKSUM=1` as 1v1 Tohno C vs V.Sion C, stage 16, Training.
* The window was on the second monitor via `System\_App.ini` PosX −640.
* `.agent_lock` was held for the run. FreeVirtualMemory was 4.27 GB before the run.
* Evidence: `docs/authoring/hantei/gameview_run/`.

| Check | Result |
|---|---|
| `frame-check 90` | 90 frames, 60.6 fps, **0 bad** of 90 checksummed layers, 0 skipped, 0 torn, latency 47 ms, exit 0 |
| `frame-check 90 layered` | 90 frames, all **layered (FULL + CHARS + HUD)**, all premultiplied BGRA, **0 bad** of 270 checksummed layers, 60.6 fps, exit 0 |
| `input 0 6 00 60` | Ok ("P1 dir 6 … for 60 frames") |
| `input 2 …` | Unsupported ("player 3: only P1/P2 are injectable"), as §12.3 says |
| `embed 0` | Ok: the real window is shown |

**`frame-check … layered` fix (§12.5):** a frame counts as layered when FULL, CHARS and HUD are all present, and the layers must be premultiplied BGRA. The FULL rebuild from the synthetic stage runs only when the ring's producer is the mock.

**What the panel does in layered mode (§12.5):**
* The frame is always 640×480 with view = 0, so the overlay and the stage camera use `(world − cam) / 128 · zoom + (320, 432)` directly.
* **Composite order:** Hantei-chan's stage back pass at the frame's camera, which gets the frame's heat blur (the game's bucket-11 blur also samples only the stage) → the frame's super-flash darkening → CHARS (premultiplied) → the stage front pass → HUD (premultiplied).
* **Remaining approximations:**
  * The super-flash is shown as darkening. The game's bucket-266 backdrop replaces the stage.
  * The stage front pass is rendered with straight alpha over a transparent clear, so its alpha is a² (see §12.2).
