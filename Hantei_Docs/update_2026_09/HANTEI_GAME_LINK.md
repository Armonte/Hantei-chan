# Hantei-chan ↔ MBAACC Game Link (ETM hot reload, W3.C piece 1)

Edit in Hantei-chan, save, and the running game reloads the character in under a second. The editor can also follow what a game slot is playing, and pick characters per slot. It is built as the **first piece of the ETM port into PovertyCaster (W3.C)**. It is not a standalone re-implementation.

| | Repo / branch | Commits |
|---|---|---|
| PovertyCaster | `/mnt/c/dev/castergroup/pc-link-wt`, `mbaacc/link`, rebased onto local main `1d733714` (includes mbaacc/tag H7 + P3/P4 seed) | `22b15df3` pc-proto/pc-ipc dev link · `248e475f` pc-overlay Shift+N latch · `dcd58256` mbaacc ETM hot reload + link + F3 tab · `a21111f0` recording gate via `setOfflineRecording`, ETM rows folded into `MbaaccAddrs.hpp` · `f8c70641` local-only/current-user pipes, palette bound (pre-rebase ids d84bdee6, e9a55bad, ac799f41, fd301ef1, 2434f8d3) |
| Hantei-chan | `/mnt/c/dev/hantei-chan/wt/link`, `feat/link` (base `c015c09`) | `6f820c8` Game Link window, client, CLI, test |

Evidence (screenshots, log excerpts, launcher script, backup hashes): `docs/link/`.

---

## 1. What it does

* **Hot reload (in PovertyCaster).** This is gonp's ETM Shift+N `FullCharacterReload`, extended to four slots and to TAG. It has three triggers: **Shift+N** (pc-overlay message latch), the **F3 → "Reload" tab** (ETM's reload submenu: per-slot character/moon/palette + Apply), and the **dev link**. It runs **offline only**. Every session kind is refused, as are replay playback and an armed `.pcrep` recording, and each refusal writes a log line.
* **Dev link.** When `PCHOST_MBAACC_LINK=1` is set, the injected DLL serves `\\.\pipe\povertycaster-link-<MBAA pid>` on **pc-ipc**. Messages: reload, set character, state query.
* **Game Link window (in Hantei-chan).** Windows → *Game Link (MBAACC)*:
  * connect/disconnect
  * **auto-reload on save**: HA6 / .pat / .txt / _c.txt, mapped to the slots that loaded that file
  * a live per-slot table
  * **follow** a slot, optionally **jumping the editor** to its pattern/frame
  * **Push to game**: save the modified characters, then reload
  * reload all
  * set character per slot

  A background worker thread and a queue carry all of this, so the UI never blocks.

## 2. How it fits the ETM port (W3.C)

The catalog (`PovertyCaster/docs/2V2_PORT_CATALOG.md` §11, §13) says ETM goes into PovertyCaster as follows:
* game addresses stay confined to `pc-adapters/mbaacc`
* in-game UI goes through **pc-overlay**
* one Present chain
* ETM's CE↔Steam table is consumed, not re-hardcoded

Here is how this piece follows each rule:

| Rule | This piece |
|---|---|
| Addresses only in the adapter, from ETM's dual-target table | One table per game: the rows live in `MbaaccAddrs.hpp` (the "ETM W3.C" block), with ETM's Steam RVAs noted per row, and `static_assert`s pin them to ETM's `GameAddrs.gen.inc` values (CE RVA + 0x400000). ETM's `CodeMap.gen.inc` has no row for the code addresses, so they have no Steam mapping and the reload refuses on Steam. It does not guess. |
| In-game UI via pc-overlay | The F3 tab is registered with `Overlay::addSettingsTab`, like `HinokakeraUiLighting`. It follows the facade rule: the UI TU names no game address. There is no new hook and no new Present callback. |
| Hotkeys message-driven, no key polling | Shift+N is a latched edge in `OverlayInput`. It is armed by the adapter's offline gate, the same way as the #329 transport keys. |
| Control plane over pc-ipc | The existing framing and PipeCore are reused, on a second named endpoint (see §4). |
| Cite the ETM source | ExtendedTrainingMode `6f33c08`, `5129d5a`, `382e0ea`, `320c7b3`, `559a73a`, `dc55ac2` (banner of `MbaaccSim_EtmReload.cpp`). |

**Next ETM pieces, in order of dependency:**
1. **ETM renderer core into pc-overlay** (§11b): VertexData / EtmFrame / font. This is partly lifted already: EtmFrame, EtmFontAtlas and EtmRenderer exist.
2. **Hitbox viewer parity**: F5 exists; next is ETM's box colours, throw boxes and effect boxes.
3. **Framebar / frame data display**: this reads the same per-actor fields that `fillLinkState` already snapshots. It is a natural consumer of the dev-link state as well.
4. **Save/load state in training**: ETM `FullSave`. Note that PovertyCaster's rollback savestate already exists, so this should reuse `IGameSim::save/load` rather than port ETM's region list.
5. **Training dummy options** (recording/reversal slots, `XS_*`), after which the ETM reload submenu's remaining pieces become meaningful.
6. **Steam column**: once ETM's CodeMap maps 0x448FB0/0x45F650/0x550014, add them to the future `addr_steam` table (MbaaccAddrs.hpp scaffold).

## 3. IDA evidence (MBAA CE, session `212fe94c`)

Comments were added at 0x55DEC3, 0x550014, 0x74D838 and 0x41C6E0. No renames were made (the session is read-only).

| Address | Name | What matters |
|---|---|---|
| 0x448FB0 | `Scene_LoadInitialAssets(eax = loader)` | This is ETM's `MBAA_FullCharacterReload`, the match load run by `MatchingThread_Main`. It calls `Match_BuildCharacterLoadRequest` → `BattleScene_LoadCharacterData` → `Background_LoadStageData`. Prologue `55 8B EC 83 E4 F8 81 EC BC 00` is checked before every call. |
| 0x550014 | `g_CharacterDataLoader` | The EAX argument. `+4` is the progress word (0x80000000 at start). |
| 0x449030 / 0x449080 / 0x449100 | `Match_BuildCharacterLoadRequest`, `CharaLoadReq_FillPrimaryMember`, `CharaLoadReq_FillPartnerFromFile2` | These copy battle-slot records `0x74D838 + 0x2C*side`: `[1]` palette, `[2]` chara, `[5]` moon, `[9]==2 → [10]` moon override. These are exactly ETM's `adLoadP1Pal/Char/Moon`. The duo's File2 partner is filled natively, so the maid case needs no special-casing. |
| 0x4489E0 | `BattleScene_LoadCharacterData` | Per slot: `ResourceBundle_ReloadFromFile(".\data\<name>_<moon>.txt")`, which **destroys the old bundle** first, then .pal, textures, `LoadCharacterCommandFile(<name>_<moon>_c.txt)`, `Player_InitBattleData`, `InitResourceParser`, and `g_TeamAux[team].isActive`. PovertyCaster's H2 TagIn/TagOut fill hook at 0x448D4F sits inside this function. |
| 0x41C6E0 | `Player_InitBattleData` | This memsets the actor (0x338) and keeps only health/red, the pattern pointers and the fighter record. As a result the partner (+0x328) becomes 0, tagFlag (+0x174) becomes 0, and x is set from the spawn table. |
| 0x45F650 | `Actor_LoadPatternAndFramePointers(ebx = actor)` | This is ETM's `UpdateCharPointers`. It re-points +0x318/+0x31C/+0x320 into the new bundle. Signature `8B 53 0C 56 57 8B BB 2C 03 00`. |
| 0x55DEC3 | `g_NewSceneFlag` (MbaaccAddrs `NEW_SCENE_FLAG`) | This is ETM's `QueueTrainingReset`. When it is nonzero, `BattleMode` (0x42357A) → `ResetBattleMode` (0x423380) → `Battle_InitializeRound` (0x426590) runs. That sequence relinks partners (`slot ^ 2` when `isActive`), parks slots 2/3 through `Player_InitReserveMemberForRound` (0x41C870, which sets tagFlag = 1), resets `g_TeamAux` activeSlot/tagRequest, and **memsets the effect pool**. The effect pool still points into the destroyed bundle, so this last step is load-bearing. |
| 0x557DB8 | `g_TeamAux[t]` | ETM `adP1DataBase` / `activeCharacter`. gonp's 559a73a (the Koha-maid fix) writes activeSlot back to the primaries. |
| 0x4266C8 / 0x423459 | `g_GameMode == 0x1010` | Training skips the round intro. This is why point positions are restored in training only (in VS the round restarts from its intro). |

The ETM sequence and its 4-slot/TAG form are implemented in `pc-adapters/mbaacc/src/MbaaccSim_EtmReload.cpp`:
1. Apply the picks to the battle-slot records.
2. Loose-file preflight (`.\data\<File1>_<moon>.txt`, the duo's File2, and the TAG partners).
3. Save the points' x/y/facing.
4. Set `exists = 0` ×4.
5. Call `Scene_LoadInitialAssets`.
6. Call `Actor_LoadPatternAndFramePointers` for each live slot.
7. Point `TeamAux` activeSlot back to the team, with tagRequest = 0.
8. Set `0x55DEC3 = 0xFF`.
9. Run the reload listeners.
10. After `ResetBattleMode` clears the flag, and in training only, restore the positions.

**TAG** needs no special code, because steps 5 and 8 go through the game's own match build. The TAG hooks fire inside the reload:
* H4 `0x449044` (mode)
* H3 `0x449063` (partner request)
* H2 `0x448D4F` (TagIn/TagOut, keyed by the file just loaded)

`Battle_InitializeRound` then does the relinking. The logs show all three hooks firing within the reload (`docs/link/logs/pchost_link_tag2.excerpt.log`).

## 4. Protocol

Defined in PovertyCaster `pc-proto/include/pc/proto/Proto.hpp` and mirrored in Hantei-chan `src/game_link_proto.h`, with sizes static-asserted on both sides.

**Transport.** pc-ipc framing: `IpcHeader{u16 kind, u16 version, u32 size}` + one fixed-width payload, over the byte-mode named pipe `\\.\pipe\povertycaster-link-<MBAA pid>` (`pc::ipc::linkName`). The DLL serves the pipe (`ServerChannel::createEndpoint`) and the editor connects. There is one editor at a time; after it leaves, the DLL serves a fresh channel. The editor finds the pid by process name (MBAA.exe). Access is local only, with no auth.

**Why a second endpoint, not the launcher's session pipe.**
* The session pipe is created by the launcher before `ResumeThread` and accepts exactly one peer: the DLL.
* A game started without the launcher has no session pipe at all.
* The editor attaches and detaches whenever it likes.

Only the endpoint and the roles differ. The framing, the reader thread and the queue are the same code, so this is not a second IPC stack.

| kind | dir | payload |
|---|---|---|
| `0x100 LinkCommand` | editor → DLL | `{u16 op, u16 seq, u8 slotMask, u8 flags, u16 _, i32 slot, i32 chara, i32 moon, i32 palette}` (24 B) |
| `0x101 LinkReply` | DLL → editor | `{u16 op, u16 seq, i16 status, u16 _, u32 reloadCount, char message[116]}` (128 B) |
| `0x102 LinkState` | DLL → editor | `{u32 worldTimer, u32 gameModeKind, u32 reloadCount, u16 scene, u8 reloadAllowed, u8 tagLive, i8 teamActive[2], i8 teamTagRequest[2], LinkActor actors[4]}` (276 B) |

`LinkActor` (64 B) fields:
* `exists`, `team`, `tagFlag`, `partnerSlot` (0xFF = none)
* `chara`, `moon`, `palette`
* `pattern`, `frame` (the sequence element, i.e. Hantei-chan's "frame")
* `frameTicks`, `patternTicks`
* `x`, `y`
* `file[28]` (the data-file name the slot loaded: File1, or File2 for a duo partner)

Ops:

| op | meaning |
|---|---|
| `1 Ping` | Replies `ok "pong"`. |
| `2 Reload` | `slotMask` is informational (the native load is all-slot). |
| `3 SetChar` | Sets `slot`, `chara`, `moon`, `palette`; -1 keeps a field. With `flags & 1` the reload follows. |
| `4 QueryState` | Answered with a `LinkState`, at most one per game frame. |

Flags: `1` reload after SetChar, `2` force (skip the file preflight), `4` do not restore positions.

Status codes:

| code | meaning |
|---|---|
| `0` | ok |
| `1` | queued (never sent; a queued op's reply is sent when it has **run**) |
| `-1` | refused: session (netplay / rollback / CCCaster compat / spectate / stress synctest / replay playback) |
| `-2` | refused: `PCHOST_RECORD` |
| `-3` | not in battle |
| `-4` | not the CE build, or the code signature does not match |
| `-5` | bad args |
| `-6` | missing data file |
| `-7` | busy |
| `-8` | unknown op |

## 5. Using it

1. The game folder needs **loose data**: `0002.p` moved aside and `data\` extracted, as in `/mnt/c/games/mbaacc_tag`. With the pack in place the reload re-reads the pack, and the preflight refuses because it finds no loose `.txt`.
2. Launch with `PCHOST_MBAACC_LINK=1`. An offline VS or training match is fine; TAG with `PCHOST_MBAACC_TAG=1 PCHOST_MBAACC_TAG_P3=<id> PCHOST_MBAACC_TAG_P4=<id>` also works. The F3 → **Reload** tab shows whether a reload is allowed right now, and why not if it isn't.
3. In Hantei-chan: Windows → **Game Link (MBAACC)** → Connect. Open the character from the game's `data\<name>_<moon>.txt` (the save target is the top HA6, e.g. `sion_0_r.HA6`).
   * With *Auto-reload on save* on, **Ctrl+S / Save Character** reloads the slots that use that character about 0.5 s after the file settles.
   * Other triggers: **Push to game** (save + reload), **Shift+N** in the game window, or the F3 tab.
4. Follow mode: tick *Follow game slot* and *Jump editor to it*. This only jumps when the active character is what that slot loaded.
5. Scripting: `build/game_link_cli.exe state|ping|reload|setchar|sample|watch|ha6-get|ha6-set-duration` (banner of `src/game_link_cli.cpp`). You can also launch Hantei-chan with `--open <txt> --game-link <slot> [--capture <png>]`.
6. A TAG tuning loader (`tag_tuning.ini`, branch `mbaacc/tag`) hooks in with `mbaacc::etm::addReloadListener(fn, ctx)`. Listeners run on the game thread right after every reload, and the reload logs when `tag_tuning.ini` is present.

## 6. Verification (2026-09-23, `/mnt/c/games/mbaacc_tag`)

**Environment:**
* pchost.dll built from `mbaacc/link` and copied in. The tag agent's DLL was backed up first and restored afterwards; it was byte-compared as unchanged before the restore.
* The edits were made to `data\sion_0_r.HA6` and `data\shiki_0_r.HA6` only. Both were backed up in place (`*.linkbak`) and restored byte-identical; the sha256 values are in `docs/link/backup_sha256.txt` and `docs/link/restore_sha256.txt`.
* Edits were made with Hantei-chan's own loader and saver: `FrameData`, the `.txt` stack loaded as `LoadFromIni` does it, saved to the same top HA6 as *Save Character*, via `game_link_cli ha6-set-duration`.
* The reload was triggered by the **auto-reload-on-save watcher**, which is the same `gamelink::Client` code the window runs.
* Launcher: `docs/link/run_link_game.sh`. Log excerpts: `docs/link/logs/`.

| # | Test | Result |
|---|---|---|
| 1 | Offline VS (Sion vs V.Sion, test AI). `state` at CSS, then `reload` at CSS | State streams. `refused: not in battle`. |
| 2 | Plain reload in battle | `reload #1 done in 609 ms (2 slots)`. H2 TagIn/TagOut refilled inside the reload. The game kept running. |
| 3 | **Edit + auto-reload, VS.** Sion `pattern 10 (前進) frame 0` duration 2 → 40, saved; the watcher sent `reload (slot mask 0x1)` | Before: max 1 tick in that frame (4/288 samples). **After: max 35 ticks (108/430 samples)**, with no restart. Screenshot: `docs/link/vs_after_reload_sion_walk40.png`. |
| 4 | **Shift+N** (real WM_KEYDOWN/UP posted to the window, `docs/link/post_shift_n.ps1`) | `ETM-RELOAD #3 by Shift+N`, 531 ms. |
| 5 | Refusals | `PCHOST_STRESS=1` (GekkoNet synctest): `refused: session`. `PCHOST_RECORD=auto`: `refused: recording`. Both logged. The test tape was deleted. |
| 6 | **TAG** (P1 Sion + P3 Shiki vs P2 V.Sion + P4 Miyako). Partner edit: Shiki `pattern 0 (立ち) frame 1` duration 5 → 60; the watcher mapped `shiki_0` → **slot mask 0x4 (P3)** | Inside the reload: `H4 load`, `H3 side 0/1 partner`, `H2 slot 0..3`. At the next GO: `LAYOUT s0 partner=s2, s2 tf=1 partner=s0 …` (relinked, reserves parked). The reserve Shiki went from max 4 ticks in the frame to **58**. |
| 7 | **TAG, tagging after reload** (scripted 22D) | After the reload the round restarted. The pre-GO 22D was refused (the tag rule). The after-GO 22D **swapped both teams**: `act 2/3`, Shiki/Miyako on point, Sion/V.Sion `tf=1`. Screenshot: `docs/link/tag_swapped_after_reload.png`. |
| 8 | SetChar P1 → chara 3 (Akiha), moon 1, palette 2 | `pick slot 0 -> chara 3 moon 1 palette 2`, reload in 1078 ms. The state shows `P1 AKIHA chara=3 moon=1 pal=2`, and the partner link was kept. |
| 9 | **Hantei-chan GUI**: `--open sion_0.txt --game-link 1 --capture` | The window is connected, the live table is filled, and follow + jump has moved the editor to **Sion pattern 58 (6B) frame 24** as the game played it. Screenshot: `docs/link/hantei_game_link_follow.png`. |
| 10 | Unit tests | PovertyCaster `pc_precommit.sh`: build, lint and **264/264** ctest pass, including the new `mbaacc_etm_reload_gate` and `ipc_link_roundtrip`; `ipc_link_roundtrip` also passes on x64. Hantei-chan: `game_link_test`, `shortcut_router_test` and `undo_manager_test` pass. |

**A crash in the TAG run that is not caused by the reload.** In run #7 the game faulted at the next round end, at `0x448672 Match_IsLeaderCharDifferentFromStored`:
* It reads `[0+1]` when the winning team's member-0 slot has `exists == 0`. It is called from `MatchState_InitPlayerSlots` ← `BattleScene_SelectOfflineResult`.
* **The same fault is in the tag agent's own offline TAG runs** `pchost_offtag.log` (12:35) and `pchost_offtag3.log` (12:43), which had zero reloads.
* So it is a pre-existing TAG round-end issue, and I'm handing it to the `mbaacc/tag` owner. The team index comes from `0x55D200`, and `Team_GetMemberSlotIndex(team, 0)` is used without checking `exists`.
* The dump was armed for my run, but no `.dmp` was written (the first-chance handler logged the fault and stack, which is in `docs/link/logs/pchost_link_tag3.excerpt.log`).

## 7. Known limits

* **All-slot reload.** `Scene_LoadInitialAssets` rebuilds every slot and the stage, so `slotMask` only says which slots the editor thinks changed. It takes about 0.4 to 1.1 s. Old bundles are destroyed by the game's own `ResourceBundle_ReloadFromFile`, so repeated reloads do not accumulate them.
* **VS restarts the round** (its intro replays, and positions come from the spawn table). Health carries over after round 1. Training keeps the points' positions. The round re-init is required either way: it clears effects that point into freed data and relinks the TAG partners.
* **Loose data only.** With `0002.p` present the reload re-reads the pack, and the preflight refuses unless *force* is set.
* **CE only.** The code addresses have no Steam mapping yet (`MbaaccAddrs.hpp`, ETM block).
* **Where the P3/P4 picks come from.** Since `mbaacc/tag` 285c3cb7, a `PCHOST_MBAACC_TAG_P3/_P4` env pick is only a one-time seed into battle slots 2/3, which the TAG partner hook (H3, `MbaaccSim_Tag.cpp`) reads. An explicit `SetChar` on slot 2/3 overwrites those slots and wins from then on, for the rest of the run. Reloading the P3/P4 files always works. Before that change the env pick overrode every `SetChar`; my verification runs predate it. In those runs H3 also copied the primary's moon onto an env partner that had none, which is why Shiki became moon 1 after P1 went to moon 1.
* **One editor at a time** per game process. If the editor stops reading while it keeps writing, the DLL's `send` can block once the 64 KiB pipe buffer fills. Hantei-chan reads on its own thread, and the DLL answers at most one QueryState per frame.
* **Auto-reload watches what the editor loaded.** It watches the HA6 stack, .pat, .txt and the loaded `_c.txt`, mapped by file name. A file whose stem does not start with a slot's data-file name (e.g. `ex_akiha.ha6` loaded outside the character's .txt) maps to no slot. Use *Push to game* or *Reload all* for those.

## 7b. Review fixes (hinokakera, before merge)

* **Who may open the pipes.** Both pc-ipc pipes (the dev link and the launcher's session pipe) are now created with `PIPE_REJECT_REMOTE_CLIENTS` and an explicit DACL, `D:P(A;;GA;;;<the creating process's user SID>)`, built from the process token. They no longer use the default security descriptor. If the token can't be read, the pipe is not created (fail closed).
  * `ipc_link_roundtrip` checks the DACL through a raw client handle: exactly one ACCESS_ALLOWED ACE, and it is the current user.
  * It also drives the launcher's `create(pid)` / `open(pid)` path through a Hello handshake. No other ctest exercises the launcher↔pchost pipe.
  * Not checked: `PIPE_REJECT_REMOTE_CLIENTS` under Wine/Proton. If Wine rejects the flag there, the pipe creation fails loudly; it does not silently drop the check.
* **Palette bound.** `SetChar` now checks the palette against the game's own rule. `BattleScene_LoadCharacterData` loads `.\data\<File1>.pal` and copies from it only when `palette < count`, where `count` is the file's first dword (64 in the stock data). That compare is **signed** (`jge` at 0x448BBA; IDA comment at 0x448BB8), so a negative palette would read 1 KiB before the buffer. The bound is `0 <= palette < min(count, 256)`, since actor+6 stores it as a byte. Values outside it are rejected with a log line; -1 still means "keep".
  * Live results: palette 64 → `REFUSED — palette 64 out of range 0..63`; -5 → refused; 63 → applied (`docs/link/logs/pchost_link_smoke3.excerpt.log`).

**Final check on the rebased stack (`f8c70641` on `1d733714`):** `pc_precommit.sh` is clean, 265/265 plus the x64 launcher build. The first run failed only on `launcher_handshake`, which CLAUDE.md lists as load-sensitive (load average ~12 at the time); it does not use pc-ipc, passed twice in isolation, and passed in the full re-run.

A TAG smoke test in `mbaacc_tag` (`docs/link/logs/pchost_link_final.excerpt.log`):
* Reload: 718 ms.
* `SetChar` P3 with palette 99: refused.
* `SetChar` P3 → Hisui, palette 1: H3 logged `explicit pick`, P3 is now Hisui and still linked to P1.

## 8. hinokakera's files

Both write-ups were approved and are implemented in `fd301ef1` (reviewed after the spectate batch; not merged):

1. **`MbaaccSim.hpp` overrides `IGameAdapter::setOfflineRecording`.** The driver calls it every offline frame of a run that records a `.pcrep`, from frame 0, and the flag goes to the reload gate (`GateInputs::recordArmed`). The reload no longer reads `PCHOST_RECORD` itself. Re-verified live: `PCHOST_RECORD=auto` gives `refused: recording`.
2. **`EtmAddrs.hpp` is folded into `MbaaccAddrs.hpp` and removed.** Added: `CHARA_SELECT_TABLE`, `SCENE_LOAD_INITIAL_ASSETS`, `CHARACTER_DATA_LOADER`, `ACTOR_LOAD_PATTERN_POINTERS` and the two code signatures. Every other row already existed (`NEW_SCENE_FLAG`, `PLAYER_STRUCT_BASE`, `FIGHTER_STATES_BASE` = g_TeamAux, `BATTLE_SLOTS_BASE` + fields). The compile-time cross-checks now pin these rows to ETM's own table values.

The reload is called from `MbaaccSim::onFramePrologue` (`MbaaccSim_Advance.cpp`), not `installMenuHooks`, because it installs no hook: it is a per-frame pump that must run before the game's frame.

After `a21111f0` (then `fd301ef1`): `pc_precommit.sh` is clean (264/264 x86 plus the x64 launcher build), and a VS smoke test in `mbaacc_tag` reloaded in 437 ms (`docs/link/logs/pchost_link_smoke2.excerpt.log`).

**Shared-file edits to review (additive):**
* `pc-overlay/src/OverlayInput.cpp` / `.hpp`: `setHotkeyReloadArmed` / `consumeHotkeyReload`, a `g_shiftHeld` tracked from messages, and one latch call beside `latchReplayTransportKey` in each of the two WM_KEYDOWN paths.
* `pc-proto/Proto.hpp`: the `Link*` kinds and structs.
* `pc-ipc`: `linkName`, `createEndpoint`, `openEndpoint`.
* `MbaaccUi.cpp`: one `installEtmReloadTab()` line.
* `tests/CMakeLists.txt`: two tests.

For the TAG owner (`mbaacc/tag`): the round-end fault at 0x448672 above. The P3/P4 pick source is settled by 285c3cb7 (§7).
