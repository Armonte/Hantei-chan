# Hantei-chan ↔ MBAACC Stage Link (live stage switch + stage hot reload)

You can change the stage of a running offline match, or save a stage edit in Hantei-chan and see it in the game about half a second later. You don't restart the match: the fighters, their positions, health, meter, the round and the timer all carry on. This is the second piece of the Game Link, after the character hot reload (`docs/HANTEI_GAME_LINK.md`), and it uses the same pipe, the same offline-only gate and the same F3 "Reload" tab.

| | Repo / branch | Commits |
|---|---|---|
| PovertyCaster | `/mnt/c/dev/castergroup/pc-stagelink-wt`, `mbaacc/stage-link`, rebased onto origin/main `1f1569ef` | `1aaf2c2e` pc-proto/pc-overlay: stage ops + Shift+B latch (shared, for review) · `0f5a23c6` mbaacc stage link, F3 picker, test · `fa496860` 0000.p latched at boot |
| Hantei-chan | `/mnt/c/dev/hantei-chan/wt/stage-link`, `feat/stage-link` (base `9d751c6`, update/ex-mbac) | `6139157` client stage ops + stage auto-reload + pinned pid · `9cbdd2b` Game Link window Stage section + `GameLink_ShowStageInGame` API · `f392004` CLI |

Evidence: `Hantei_Docs/update_2026_09/stage_link/` holds the launcher script, the Shift+B poster, the pchost logs, the sha files and the two contact sheets. The full-size screenshots named below are kept outside the repo, in `/mnt/c/dev/hantei-chan/docs/stage_link/`.

---

## 1. What it does

**In the game (PovertyCaster, `PCHOST_MBAACC_LINK=1` for the link; the F3 tab and the hotkey always work):**
* **SetStage(id):** switches the live stage to BgList.ini entry `id` (1..99) and restarts that stage's BGM.
* **ReloadStage:** re-reads the stage on screen from disk: `bgNN.dat` and `bgNNInfo.txt` (lights and weather), plus `BgList.ini` with a flag.
* **F3 → Reload tab → Stage section:** a picker over the game's own list, *Switch stage*, *Reload stage (Shift+B)*, *Also re-read BgList.ini* and *Keep BGM*.
* **Shift+B:** reloads the stage. It is latched by pc-overlay's `WM_KEYDOWN` message latch under the same offline-only arm as Shift+N, and nothing polls the keyboard.
* **Offline only.** It is refused in exactly the cases where the character reload is refused (netplay, rollback, CCCaster compat, spectate, stress synctest, replay playback, an armed `.pcrep` recording, a non-CE build, or not in battle). Each refusal is logged.

**In Hantei-chan (Windows → Game Link (MBAACC) → Stage section):**
* It shows the stage on screen, the selected stage, the BGM id and the op count.
* It has a stage picker (names come from the game folder's own `Bg\BgList.ini`, found from the connected pid) and the *Set stage*, *Reload stage* and *Show open stage (N)* buttons. The last one switches the game to the stage open in the active editor tab.
* **Auto-reload stage on save** (on by default). Every open stage tab's `.dat`, `Info.txt`, `light.txt` and `BgList.ini` are watched. When a save settles (400 ms), one `ReloadStage` is sent, but only if the saved file belongs to the stage the game shows (`gamelink::StageFileMatches`). A saved `BgList.ini` sends it with `kFlagStageList`.
* **API for other windows** (`src/game_link_api.h`; the stage browser on `feat/stage` calls this):
  ```cpp
  uint16_t GameLink_ShowStageInGame(int stageId);          // connects first if needed, then switches
  uint16_t GameLink_ReloadStageInGame(bool rereadList = false);
  GameLinkStageInfo GameLink_GetStageInfo();               // connected / allowed / loaded / bgm / dataFile / lastReply
  ```
  Both use the one shared client, so there is still only one pipe per editor.
* **Pinned target:** `Client::SetTargetPid(pid)`, the env var `HANTEI_GAME_LINK_PID`, or `game_link_cli --pid N` makes the link open only that MBAA.exe, with no discovery by name.

## 2. How the game changes a stage (IDA session `mbaa`, 2026-09-24)

The game already has a live mid-scene stage change. The story/arcade script's `GB n` op does it (`StoryScript_RunStageCommands` 0x4C46A0, at 0x4C4837..0x4C4858):

```
if (Background_LoadStageData(n, 0)) { Background_InitializeAndLoad(); battleBgmId = n; g_SelectedStageId = n; }
```

Its `GI n` form is the same load with `force = 1`. That re-reads the file even when the path is unchanged, which is exactly a hot reload. The stage link runs those calls and nothing else:

| Address | Name (renamed here unless noted) | Role |
|---|---|---|
| 0x4B6BF0 | `Background_LoadStageData(esi=stage, force)` (existing) | Reads `.\Bg\<entry>.dat` into the raw buffer 0x767198, sets `g_BGLoadedStageIndex`, clears the bg-ready flag 0x76E7B4. **It does not check that the read succeeded**, so the link preflights the file first (it must exist and start with `bgmake`). |
| 0x4B6B50 | `Background_InitializeAndLoad` (existing, commented) | Does nothing while 0x76E7B4 is set. Otherwise it frees the old stage, reads `<entry>Info.txt`, expands and uploads CG/PAT, spawns the instances and weather particles, then frees the raw buffer. |
| 0x4B68B0 | `Background_FreeStageResources` (was `ResultMenu_DestroyOverlayResources`) | Frees the CG, the PAT, the DropObject bitmap, the object block and the layer buckets. |
| 0x418390 | `BgPat_ReleaseTextures` (was `ResultMenu_ClearOverlayQueue`) | Releases every PAT D3D texture, so a stage swap does not leak textures. |
| 0x4B6970 | `Background_UnloadStage` (was `ResultMenu_ResetOverlayState`) | The full unload. |
| 0x4C46A0 | `StoryScript_RunStageCommands` (was `ConfigFile_ParseResultMenuCommands`) | The script ops `GB`/`GI`/`GF`/`ED`/`XH`. |
| 0x4B4A80 | `StageSelect_LoadStageData` (existing) | Re-parses BgList.ini. The old 48-byte entries are never freed by the game, so the `kFlagStageList` option leaks about 3 KiB per use. |
| 0x76E653 | battle BGM byte (`dword_76E64C+7`, commented) | `BattleLoop_StartFightAndRestoreBGM` 0x4728D0 plays it at round start. Every stage writer sets it to the stage id. |
| 0x4DDF40 / 0x4DDC40 / 0x4DDEC0 | `BGM_StopCurrent` (was `AudioDeviceManager_UpdateBackgroundAudio`) / `BGM_PlayById` / `BGM_StartLoaded` (was `AudioSystem_UpdateBackgroundMusic`) | The restart sequence from 0x472921..0x472949, replayed on a switch. It is skipped when the id has no `bgm.txt` entry (the table is at 0x768B90, 150×48 B), when that id is already playing, or with *Keep BGM*. |
| 0x41F7C0 / 0x41E471.. | `LoadVectorFile` / `LoadAllPackFiles` | The game looks in the packs **before** the loose files, and it registers the packs once, at boot. `.\bg` lives in **0000.p**. |

All the addresses and 24-byte code signatures are in `MbaaccAddrs.hpp` (the `[stage-link]` block), with `static_assert`s against the `dword_76E64C`/`dword_7676E0` array rows. The signatures are re-checked before every call.

**Where it runs.** `mbaacc::stage::stageFrame` is called from `etm::etmFrame`, which runs in `MbaaccSim::onFramePrologue`: on the game thread, before the game's frame, and after the character reload (whose `Scene_LoadInitialAssets` asks for `g_SelectedStageId` without `force`, so it stays a no-op once the selector matches the stage on screen). The F3 tab and the link only queue requests.

## 3. Determinism: the stage stays out of netplay

* **The gate.** The stage ops use the character reload's `gateVerdict`, and it is unit-tested (`mbaacc_etm_reload_gate`). Every session kind refuses, and so do replay playback and recording.
* **Even without the gate, the sim would not see it.** The stage state is outside every savestate region and the syncHash (MbaaccAddrs stage-anim block). The stage RNG is **stream 0** of `g_RngStreamBank` 0x563780. Its inline reader `Rng_Stream0_NextInt` 0x4B4F70 has exactly three callers, all cosmetic: `DropObject_UpdateParticles`, `BgCmd_SpawnRandomObject` and `BgCmd_RandomizeVelocity`. Every sim caller of `Rng_UpdateState` 0x421C10 passes `ecx = 1`. I re-checked this in IDA and left a comment at 0x4B4F70. This is the same fact `MbaaccSim_internal.hpp` records for its "not patched on purpose" list.
* **The render-only patch still holds.** `BG_ADVANCE_ARG_BYTE` (0x4238BF `push 1` → `push 0`) still makes the game's own pass draw-only, and `MbaaccSim::tickStageBackground` still advances the six updaters once per displayed frame from the Present hook. A switch only replaces the instance set those updaters walk; it adds no writer on the sim side. (The one sim-path caller of stage code, `Battle_InitializeRound` → `Background_SpawnInitialInstances`, draws only stream 0.)
* **Why it is refused anyway:** two peers would silently watch different stages, and a replay records the stage it was captured on.

## 4. Protocol (additive; `kLinkVersion` stays 1)

These are added to PovertyCaster `pc-proto/include/pc/proto/Proto.hpp` and mirrored in Hantei-chan `src/game_link_proto.h`.

| | |
|---|---|
| `LinkOp::SetStage = 5` | `LinkCommand.slot` = stage id. Flags: `kLinkFlagStageList (8)` re-reads BgList.ini first, `kLinkFlagKeepBgm (16)` keeps the music. |
| `LinkOp::ReloadStage = 6` | The stage on screen. `kLinkFlagStageList` is optional. |
| `LinkOp::QueryStage = 7` | Answered with `IpcKind::LinkStage = 0x103`. |
| `LinkStage` (64 B) | `{i32 selected, i32 loaded, u32 stageLoads, i16 bgmId, u8 allowed, u8 _, u8 valid[16] (bit per list entry), char dataFile[32]}` |

`LinkCommand` and `LinkState` are unchanged, so an old editor still works against a new DLL. A new editor against an old DLL gets `Unknown` for QueryStage and shows "pchost.dll predates the stage ops". Status codes are reused: `BadArgs` for an id outside 1..99 or with no list entry, and `MissingFile` for a missing or non-bgmake `.dat`.

## 5. Using it

1. **Loose `Bg\` needs 0000.p out of the way at boot**, the same as `0002.p` for characters. A switch works either way. With 0000.p registered, though, a reload re-reads the *packed* stage, and the DLL says so once at boot and in each op's reply (`[0000.p at boot: packed copy wins]`).
2. Launch with `PCHOST_MBAACC_LINK=1` into an offline battle. The F3 → Reload tab shows the Stage section.
3. In Hantei-chan, open the stage (`Bg\bgNN.dat`) and open Game Link → Connect. Edit and save with Ctrl+S or the inspector's Save, and the game reloads it. *Show open stage* switches the game to the stage you are editing.
4. Script: `game_link_cli [--pid N] stage | setstage <id> [keepbgm] [list] | reloadstage [list] | stage-watch <file> <ms> | bg-get <dat> <obj> <frame> | bg-set <dat> <obj> <frame> duration|offsetX|offsetY|opacity <v>`. `bg-set` edits the file with the stage editor's own `bg::File` loader and saver.

## 6. Verification (2026-09-24, `/mnt/c/games/mbaacc_winaspect` only)

The environment rules were followed:
* Only my own instance was driven, always by explicit pid (`game_link_cli --pid`).
* The window was placed borderless on DISPLAY2 at (-640,0), 640×480 (`GetWindowRect` returned `-640,0 0,480`).
* The log tag was `stglink`, and nothing in the folder was left changed.

`stage_link/run_stage_game.sh` does all of this. Two traps in this folder, both measured:
* **The folder's `winmm.dll` shim is an old build.** It has no `PCHOST_DLL_PATH` support and always loads `pchost.dll` next to the game. Worse, pc-launch deliberately stages an *unset* `PCHOST_DLL_PATH` for a DLL name without a directory. So `pc_inject MBAA.exe pchost_xxx.dll` **runs the folder's pchost.dll, not yours**; my first run loaded build 5ccfb3dd. The script swaps our DLL in as `pchost.dll` for our boot only, and puts the original back (sha1 `504a4b8c…`, checked) once the game is InGame. **The stage-audit agent's `pchost_stage.dll` launches hit the same trap.**
* **0000.p** is parked for our boot only, for the same reason. Its 70 files are byte-identical to the loose `Bg\` copies, which was checked before and after.

**Two of my instances were killed from outside** (no BYE line, even though ExitProcess is hooked, and no crash dump). This is consistent with the stage audit's `killours`, which stops every MBAA.exe under this folder. The runs below are from a later launch, pid 41900 (build `0f5a23c6` + the `fa496860` fix), and pid 3256 for the switch series.

| # | Test | Result |
|---|---|---|
| 1 | **Six live switches in one VS round** (Sion vs V.Sion): 28 → 1 → 16 → 35 → 9 → 44 → 28 | Every op was `ok`, 47–297 ms. `stage` followed each switch (`loaded=selected=bgm`). The round timer kept running (71→66→62→57→52), and the fighters and HUD were untouched. Contact sheet: `setstage_sheet.png`; shots `02..06_set_stageNN.png`. |
| 2 | **Edit + auto-reload on save.** `bg28.dat` object 0 (the backdrop), frame 0 offsetY -610 → -410, saved through `bg::File` (`bg-set`) while the Hantei-chan stage watcher (`stage-watch`, same `gamelink::Client` code as the window) was watching | The watcher logged `saved bg28: sent reloadstage #1`; the reply was `reloaded stage 28 (.\Bg\bg28.dat) in 31 ms`. The backdrop moved down 200 px live (`11_bg28_after_reload.png` against `10_…before_edit.png`). |
| 3 | **Restore** | `bg28.dat` was restored from the copy backup, the stage reloaded again (`12_bg28_restored.png`), and the sha256 `eab05ddc…` matches the backup and the packed original (`backup_sha256.txt`, `restore_sha256.txt`). |
| 4 | Bad args | `setstage 0` / `100`: `bad args`. `setstage 77`: `stage 77 has no BgList.ini entry`. Nothing was loaded. |
| 5 | Keep BGM | `setstage 9 keepbgm`: loaded=9, bgm stayed 28. |
| 6 | Hantei-chan GUI | `gonptechan --open Bg\bg09.dat --game-link 1` with `HANTEI_GAME_LINK_PID`: the window connected and the Stage section showed *Show open stage (9)* (`20_hantei_game_link_stage.png`; the Background Inspector covers part of it). |
| 7 | **Shift+B** (real `WM_KEYDOWN`/`WM_KEYUP` posted to my pid, `post_shift_b.ps1`) | `STAGE-LINK #1 by Shift+B: ReloadStage 16 -> 16 … in 31 ms` (`logs/pchost_stglink_shiftb.log`). |
| 8 | Unit tests + gate | PovertyCaster `mbaacc_stage_link` (header check, id range, wire values) and `mbaacc_etm_reload_gate` pass. **`tools/pc_precommit.sh` is clean on `fa496860`**: x86 build, pc_lint, **278/278 ctest** (loopback probe first: 127.0.0.1 clean, 0.026 ms). Its x64 leg was not built because no x64-relevant path changed. Hantei-chan `game_link_test` (stage file matching: case, `Info`/`light`/`_s`, prefix, BgList, no-stage) passes, and `./build.sh` succeeds. |

Not exercised live: the refusal path in an actual session. That is the character reload's gate, unit-tested and verified live there with `PCHOST_STRESS` / `PCHOST_RECORD` (HANTEI_GAME_LINK §6).

## 7. Known limits

* **The BGM follows the stage id.** A randomly rolled stage may have been paired with a different BGM by the game. After a SetStage, the music is the new stage's own.
* **`kLinkFlagStageList` leaks** the old list entries (about 3 KiB per use). It is off by default and only sent automatically when BgList.ini itself is saved.
* **CE only.** There is no Steam mapping for the code rows.
* **MBAC `_s` variants:** a saved `bgNN_s.dat` counts as the stage bgNN for matching, but MBAACC only loads `bgNN.dat`.

## 8. Review items for hinokakera (shared files, additive)

* `pc-proto/Proto.hpp`: LinkOp 5–7, `IpcKind::LinkStage`, `LinkStage`, two flags.
* `pc-overlay/OverlayInput.{hpp,cpp}`: `consumeHotkeyStageReload`. Shift+B is latched in the existing `latchHotkeyReload` under the Shift+N arm, and cleared on disarm.
* `MbaaccAddrs.hpp`: the `[stage-link]` block.
* `MbaaccSim_EtmReload.cpp`: one `stage::stageFrame(gate, why)` call. `MbaaccSim_EtmLink.cpp`: three op cases.
* `tests/CMakeLists.txt`: `mbaacc_stage_link`.
* No new PeerHello tag: the stage ops travel on the link pipe only.
