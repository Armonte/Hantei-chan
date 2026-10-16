# MBAACC effect recolour ("DGV Better Akiha" method): status 2026-10-16

Scope: the runtime effect-sprite recolour (PovertyCaster pixel shader at effect draw time) and its authoring side (Hantei-chan CG manager,
"Effect recolor" tab, writes `<game>\fxrecolor\<char>.ini`). Checked on Hantei-chan `feat/palette-preview` (ae88b80) and PovertyCaster
`origin/main` (49f98e13f, fetched today).

## What exists

### Hantei-chan side

| file | what it does |
|---|---|
| `src/cgm/fxr_spec.hpp` | vendored copy of the runtime header `FxRecolor.hpp` (ini grammar, rule match, Oklab/OkHSV/OkHSL maths, CPU reference) |
| `src/cgm/fxr_core.cpp/.h` | effect classification (image type != 0 = fixed colour), accent index suggestion, `RecolorImage` (runtime maths on original pixels) |
| `src/cgm/fxr_panel.cpp/.h` | the "Effect recolor" tab: rules list, effect-pattern table with assign/new-rule-from-ticked, mapping/ramp/accent/per-slot editors, original vs recoloured preview, Load/Save/Diff, "Import DGV Akiha ruleset"; env hooks `HANTEI_FXR_DEMO` (=1 DGV set, =accent adds a per-pattern accent rule) and `HANTEI_FXR_SELFTEST` |
| `src/cgm/fxr_dgv_akiha.inc` | DGV's 9 Akiha colour groups as runtime rules (16-stop Oklab ramps fitted to his indexed output) |
| `src/cgm/fxr_cli.cpp` | `cgmtool fxr classify / check / shade / apply / dgv` |
| `tools/fxrecolor/` | `fxr_sync.sh` (header sync), `dgv_groups.py` / `dgv_oracle.py` (fit + regression oracle), census/usage scripts, `palfree.py`, `cgparse.py` / `cgpatch.py` / `proof_run.sh` / `t5proof.py` (type-5 bake investigation) |
| docs | `docs/cg/effect_recolor_design.md` (analysis + bake/runtime design), `docs/cg/fxrecolor_config_spec.md`, `docs/cg/type5_report.md`, `docs/cg/effect_recolor_data/` (dgv_akiha.ini, oracle csv, census csvs), `docs/cg/fxr_shots/effect_recolor_tab.png` (2026-10-03 tab shot) |
| tests | ctest `fxr_ini` (`cgmtool fxr check docs/cg/effect_recolor_data/dgv_akiha.ini`), `fxr_dgv_oracle` (needs `/home/teo/dev/refs/dgv_better_akiha`, present here). Both PASS today (`ctest -R fxr`, exit 0). `--fxr-selftest` exists but is not a ctest entry. |

Commits (oldest first): 8c3d143 vendor header + DGV ruleset, 45d86f8 core/CLI/tab/oracle, 603feae spec doc + ctest, e093970 accent suggestion + demo hooks,
b44a31a header sync (ABGR), 0426af4 Oklab/OkHSV (dc197266), 72b77e1 OkHSL (c78ec288), then UI-only passes (f645ca0, 5f36570, 1e89e9f, b73c867 layout sweep).
Related: 0bc5c2a type-5 investigation + in-game proof, bb3c472 CG manager in-game proof shots.

### PovertyCaster side (origin/main, read only)

| file | what it does |
|---|---|
| `pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp` | authoritative grammar/parser/writer + colour maths (CPU reference) |
| `pc-adapters/mbaacc/include/mbaacc/FxRecolorDefaults.hpp` | compiled-in defaults: rules for `arc` and `akiha`; `aoko` is an empty set (orbs already follow the palette) |
| `pc-adapters/mbaacc/include/mbaacc/FxRecolorLive.hpp` | live state for the F3 tab / hot reload |
| `pc-adapters/mbaacc/src/MbaaccSim_FxRecolor.cpp` | draw-time hook + HLSL technique, ini hot reload (1 s), census logging, CANARY sim-state check (`PCHOST_MBAACC_FXRECOLOR_CANARY`), CPU-vs-CPU harness (`PCHOST_MBAACC_FXRECOLOR_CVC`) |
| `pc-adapters/mbaacc/ui/src/MbaaccUi_FxRecolor.cpp` | in-game F3 tab |
| `tests/mbaacc_fxrecolor/mbaacc_fxrecolor_test.cpp` | header-only unit test (ini grammar, match, accent, ramp maths, Oklab/OkHSL vs Ottosson references) |
| `tools/mbaacc_fxrecolor_shots.sh` | offline AI match with recolour armed, pc_shot backbuffer shots + `[fxrecolor]` log lines into an out dir |
| `docs/fxrecolor_format.md` | ini format v1 / colour model v1.2 |

Commits: 7c4b32d98 (2026-10-03, "spike"), bcb242c1a format doc, 7e1487449 ABGR palette, ccba554ff lint, ee6f499e5 CvC harness,
dc1972667 Oklab/OkHSV, c78ec2885 (2026-10-04) OkHSL. Since then only 2104dd376 / 3ca56fdcb (harness inject/PID ownership) touched the shots script.

## Spec sync

`tools/fxrecolor/fxr_sync.sh check` compares (`cmp`) `src/cgm/fxr_spec.hpp` with `pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp` in a PovertyCaster
WORKING TREE (default `/home/teo/dev/PovertyCaster`, or arg 2). Result today:

    $ tools/fxrecolor/fxr_sync.sh check
    no /home/teo/dev/PovertyCaster/pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp      (exit 2)
    $ tools/fxrecolor/fxr_sync.sh check /mnt/c/dev/castergroup/PovertyCaster
    no /mnt/c/dev/castergroup/PovertyCaster/pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp   (exit 2)

Both checkouts sit on `launcher/wire-online-spectate-rows` (2501 commits behind origin/main), which predates fxrecolor, so the script cannot run.
Compared directly against origin/main instead:

    $ git -C /mnt/c/dev/castergroup/PovertyCaster show origin/main:pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp | cmp - src/cgm/fxr_spec.hpp
    (no output, exit 0)  -> IDENTICAL; last runtime change to the header is c78ec2885

No drift: nothing pulled, no build. The script should read `origin/main` via `git show` instead of a working tree (next steps).

## In-game proof

None for the runtime recolour. Searched: PovertyCaster origin/main tree (no shots/logs committed; `docs/fxrecolor_format.md` cites none),
`/mnt/c/games/_pc_instances/fxrecolor` (the shots script's default runtime/instance dir: does not exist), `/mnt/c/games/mbaacc/fxrecolor`
(does not exist), `pchost_fxr_*` logs / `*fxr*.bmp` under `/mnt/c/dev/castergroup`, `/mnt/c/games`, `/home/teo/dev` (none), Hantei-chan `docs/` and
`artifacts/`, `/mnt/c/dev/hantei-chan/work/` (only editor shots). Commit 7e1487449 says "palette is ABGR (verified)", so a game run happened
on 2026-10-03, but its shots/logs were not kept.

What does exist in-game is unrelated to the shader: `docs/cg/evidence/ingame_*.png` (2026-10-03, palette recolour + replaced sprites via
CG data edit) and `docs/cg/type5_proof/*.png` (type-5 data path proof). Neither shows the runtime shader.

## Hantei-chan Effect recolor tab check

Shots (baseline exe `palprev_base.exe`, headless `--open akiha.HA6 --cg akiha.cg --ui-script`, 1900x1000, cropped to the CG manager):

* `docs/cg/fxr_status_2026-10-16/fxr_tab_akiha_dgv.png` (`HANTEI_FXR_DEMO=1`: DGV ruleset, pattern 64 selected)
* `docs/cg/fxr_status_2026-10-16/fxr_tab_akiha_dgv_accent.png` (`HANTEI_FXR_DEMO=accent`: plus a per-pattern accent rule)
* `docs/cg/fxr_status_2026-10-16/fxr_status_akiha_dgv_script.txt` (the ui-script)
* `docs/cg/fxr_status_2026-10-16/fxr_cli_apply_slots_0_3_7_12.png` (`cgmtool fxr apply` of the DGV ruleset, sprites 1311/1100/0018, slots 0/3/7/12)

Note: `tools/ui/ab_shot.sh` plus `openany akiha.HA6` is not enough: the tab then says "The active character has no CG bank." The CG must be
given with `--cg` (or loaded), so the shot was run by hand with the same setup as ab_shot.sh. There is no script command for the starter rules
(the "Import DGV Akiha ruleset" button is a plain `ImGui::Button`, not reachable by `cgm press`); `HANTEI_FXR_DEMO` via `WSLENV` is the way.

Layout: correct after the sweeps. Toolbar, tabs, path row with Load/Save/Import DGV/Diff, the counts line ("fixed-colour sprites: 675 | drawn: 469 |
shared by 2+ patterns: 344 | effect patterns: 77"), rules list, pattern table (pat/name/fx/all/%/spr/shr/rule) and the right-hand preview/editor split
all fit, nothing clipped or overlapping at 1900x1000. The right pane needs scrolling for the ramp stops and accent table (expected).

Preview honesty: with the DGV ruleset alone the recoloured slot is visually the same as the original (pattern 64, sprite 1311: mean abs
difference 0.4-0.7 per channel, 763 of 21316 pixels differ by more than 8). This is expected and not a bug: DGV's rules reproduce his red
remaster with fixed red ramps ("the accent tokens are not used"), and Akiha's effects are red already. The same holds for every slot:
`cgmtool fxr apply` gives identical output for slots 0/3/7/12 (sprite 1311 mean RGB 125,9,9 in all four). So the DGV starter set is a
slot-invariant red remap, not a slot-following recolour. With the accent demo rule the preview does visibly change (whiter highlights,
accent #be0000; mean abs diff 3.7/9.1/7.3), so the preview pipeline itself works. A shot that shows a strong recolour needs a non-red slot
selected, and slot selection has no script hook (only a click in the per-slot table).

Possible issue: in this session the save path showed `.\fxrecolor\akiha.ini` (relative to the exe's working dir), because
`FxPanel::defaultPath` falls back to "." when `cg.palettePath(0)` is empty and the path is computed once. With `--cg` and no .pal path the
Save button would write next to the exe instead of `C:\games\mbaacc\fxrecolor\akiha.ini`. Not verified in a normal File > Open session.

## Not done

1. No in-game proof of the runtime shader is kept anywhere (no shots, no `[fxrecolor]` logs), neither for the built-in defaults nor for a Hantei-chan-authored ini.
2. Type-0 palette-following effects (e.g. Aoko's orbs, many Akiha type-0 effect frames) are never touched by the runtime (format doc, by design for v1). Type-0 effect frames inside "mix" patterns stay unrecoloured.
3. The type-5 bake tool is not built (type5_report.md status: parked, "user wants no data changes"). Hantei-chan reads and keeps type 5 byte-exact but refuses to author it (`cgm_ops.cpp`).
4. Starter rulesets exist only for Akiha (DGV) in Hantei-chan and for arc/akiha in the runtime defaults; no rules for the other characters, no `bank = effect` (shared effect.cg) rules in either.
5. The DGV starter set does not follow palette slots (fixed red ramps); turning it into per-slot colours (accent ramps) is left to the user.
6. `fxr_sync.sh` cannot check against origin/main when the local PovertyCaster checkout is on an old branch (both are today); it silently needs a working tree.
7. Default save path can resolve relative to the exe (see above), unverified in normal use.
8. `--fxr-selftest` is not a ctest; the panel has no ctest coverage. The oracle test depends on an external folder (skipped when absent).
9. Runtime: first commit is labelled "spike"; netplay loopback/hash verification and the 5-character x 36-slot in-game pass from the design plan (design doc section 7, effort table) are not documented as done. The CANARY sim-state check exists but no recorded run.
10. Design doc section 5.1 still describes a JSON config; the shipped format is the ini (fxrecolor_format.md). The design doc is stale on that point.
11. No ui-script command for "Import DGV Akiha ruleset", rule/slot selection in the tab, so the tab can only be shot via the env hook.

## Next steps

* [cg2/PovertyCaster] Run `tools/mbaacc_fxrecolor_shots.sh` with `docs/cg/effect_recolor_data/` style rules (akiha.ini from Hantei-chan) and with `-` (defaults), pin a non-red Akiha palette, and commit the shots + `_log.txt` as the first kept in-game proof.
* [cg2/PovertyCaster] One CANARY run (`PCHOST_MBAACC_FXRECOLOR_CANARY=1` vs `=2` control) to record that the hook leaves sim state untouched.
* [Hantei-chan] `fxr_sync.sh`: compare against `git show origin/main:...` (fetch first) instead of a working tree; keep `pull` writing from the same source.
* [Hantei-chan] Fix `FxPanel::defaultPath` fallback (derive the game dir from the .cg / .HA6 path when no .pal path, recompute when the bank changes).
* [Hantei-chan] Add `cgm fxr ...` script commands (import DGV, select rule, select slot) and make `--fxr-selftest` a ctest, so the tab can be shot and tested without env hooks.
* [Hantei-chan] Offer an "accent-follow" variant of the DGV set (ramps through `accent.*` tokens) so the starter rules recolour per slot.
* [Hantei-chan] Update effect_recolor_design.md section 5.1 to point at the ini format.
