# Hantei-chan preview simulator (Phases B and E)

Status: 2026-09-23. Branch `feat/preview` (worktree `wt/preview`, based on `update/ex-mbac`, merged with `db83c66`).

This covers Phases B and E of the Gonptechan EX port plan (`GONPTECHAN_EX_INVENTORY.md` §5): preview correctness, spawn transforms and a cached tick simulator.

- **Code:** `src/preview_sim.{h,cpp}`, the headless driver `src/preview_sim_tool.cpp`, and `src/mbaacc_transform.h` (adopted from EX).
- **Integration:** `framestate.*`, `main_frame.cpp` (DrawBack), `ui/main_ui_impl.h` (RenderUpdate), `box_pane.cpp` (timeline), `ui/editor_tools_impl.h` (position tool, StepTick).
- **Evidence source:** IDA session `212fe94c` (MBAA.exe). It was used read-only, so the renames proposed in §7 are **not** applied in the IDB.

---

## 1. Results at a glance

| Item | Before | Now |
|---|---|---|
| EF 11/111 parse | p1 read as range, offsets from p2/p3, flags p4/p5 | p1/p2 base, p3/p4 random rect (p4=30000 circle), p5 range, p6 count, p7/p8 flagsets, p9 angle + rand(p10), p11 proj var |
| EF 8/108 | angle ignored, 108 unhandled | angle p8 (same record layout as EF1), 108 relative |
| EF 1000 | offsets/flags zeroed | EF1 layout, spawn-once guard on spawner var p6 |
| Nested spawn mirroring | accumulated offset negated when the child's own bit 11 was set | facing inherited, toggled by bit 11, only the child's own X mirrored, then the parent position added |
| EF offset scale | 1.0 (EX: empirical 0.667) | 0.5 when the spawner's current frame is a PAT frame (AFGP[0] != 0), else 1.0 |
| Positioning flags | TODO in `render.cpp` | camera-relative, screen-absolute, both screen edges, opponent distance, fixed, absolute Y, always-face-right |
| Inherit rotation (fs1 bit 8) | root layer-0 AF rotation added | parent **actor** angle added (engine `+0x2FC`) |
| Self-jump frames | EF never re-fired | every frame entry re-fires EFs, as the engine does |
| Spawn lifetime | summed authored durations; "loops" = last frame aniType 2 | simulated: engine loop counter, aniType 0 queue, IF2 p3 destroy, link 0x20/0x21 destruction |
| Spawn preview cost | EX: re-run from tick 0 per draw, O(T²·S), x17 with onion skin | ≤ 32 ticks per query from a checkpoint, 1 tick per sequential query |
| IF branches | ignored | runtime IFs resolve by a user assumption (false / true-once-per-visit, per-IF override) and are listed. Loop-counter and parent-pattern IFs are simulated. |
| PRSP additive colour | already fixed in our tree (`parts.cpp:750`) | unchanged |
| PAT colour slot 0 | tinted by palette entry 0 | slot 0 = no tint (§3.6) |
| Loop period (`FindLoopPeriod`) | 1-tick false loops; trusts last frame aniType | compare states at frame boundaries only; follows the reachable flow |

Commits (on `feat/preview`):

- `7417cbd` Preview: engine-verified spawn EF layouts, loop period, PAT colour slot 0
- `51ede9f` Add cached tick simulator for pattern/spawn preview
- `aeb178f` Merge `update/ex-mbac` (feat/undo)
- `6937af1` Drive viewport, playback, timeline and position tool from the tick simulator

---

## 2. Design

### 2.1 State and stepping

`PreviewSim` owns a list of `SimActor`s:

- one root actor (the edited pattern)
- every actor it spawns, recursively

`Step()` advances one game tick in the engine's order:

1. **Condition pass.** Before the character update (`Battle_RunSimulationLoop` 0x41DFE0 runs `Cond_RunFrameConditionsForPhase` before `Battle_UpdateAllCharacters` / `EffectPool_UpdateAll`):
   - Native IFs are evaluated every tick.
   - Runtime IFs go through the assumption rule (§4).
2. **Update pass** (`Character_RunFrameTick` / `Character_AdvanceFrameByAniFlag`):
   - Start a queued pattern, or
   - `++timer`, and once `timer >= duration`, apply the aniType rule.
   - Actors spawned during this tick are not updated this tick. The engine sets the "spawned during update" skip byte `0x563576`.
3. **Scheduled spawns.** MBTL move-script spawns, as a list of absolute-tick entries.
4. **Sweep** dead actors.

Every frame entry calls `EnterFrame()`, the equivalent of `Character_InitializeFrame` 0x45F450:

- Loads the loop counter if AF loopCount != 0.
- Sets priority only if AF priority != 0.
- Dispatches the frame's spawn EFs **immediately**. A child spawned at tick T enters its frame 0 at T and fires its own frame-0 EFs at T, recursively. The depth guard is 48 and the `maxActors` guard is 1024.

### 2.2 Cache

- **Frontier.**
  - The first query simulates forward from tick 0 and keeps a full `State` snapshot every `checkpointInterval` (32) ticks.
  - Spawn records (lifetimes), events and the root frame track are recorded only while the frontier advances. Ids are deterministic, so replays reproduce them.
- **Queries.**
  - `getStateAt(t)` starts from the best of:
    - the checkpoint `⌊t/32⌋`
    - four LRU cursors already at or before `t`
  - It then steps forward. The worst case is 31 ticks.
  - Sequential playback and onion-style bursts (±k around one tick) hit a cursor and cost about one tick each.
- **Invalidation.** Every query validates the cache in this order:
  1. The input identity (FrameData pointers, root pattern, options including the IF overrides, script schedule) must match (`setInputs`).
  2. `FrameData::dataVersion` must match for the main and effect data. The value is bumped to a process-unique number on load, free and `mark_modified`, so a recycled `FrameData` address cannot alias an old version.
  3. A 64-bit FNV fingerprint over every sequence the simulation actually read must match. Its inputs are the AF flow fields, layer-0 usePat, and the raw EF and IF records. This catches edits that bypass `mark_modified` (undo restores, direct widget writes).

  The measured cost is 0.002-0.012 ms per query (§6).
- **Determinism.**
  - Randomness (EF11 picks and positions) comes from a seeded xorshift32 held in the state.
  - Checkpoint replay, random-order queries and a fresh from-zero simulation agree bit for bit (§6.1).

### 2.3 Why not EX's approach

EX `SimulateSpawnsToTickInternal` (`framestate.cpp:~909`, drop 1abc27f9):

- It loops `tick = 0..T` on every draw.
- For every spawn on every tick, it re-derives the spawn's frame with `SimulateAnimationFlow(elapsed)` (twice), itself a walk from 0.

That is roughly O(T²·S) per draw, multiplied by up to 17 passes with onion skin. §6.2 has the measured numbers against our port of it.

---

## 3. Engine semantics with IDA evidence

Offsets are byte offsets into the MBAA actor (`#571`/`MBAA_Actor`) unless noted. EF record = `{int type; int no; int p[12]}`, so `ef+8` is p1 (our `parameters[0]`). IF record = `{int type; int p[9]}`, so `if+4` is p1.

### 3.1 Spawn EF layouts

| EF | Handler | Evidence |
|---|---|---|
| 1 / 101 / 1000 | `Effect1_SpawnPattern` 0x454D90 | Passes the EF record itself to `Effect_InitializeComplex`: `a1+8` X, `+12` Y, `+16` fs1, `+20` fs2, `+36` angle (p8), `+40` proj var (p9). Type 1000: `if (*(actor + p6 + 224)) return 0; ... = 1` (spawn once per var word `+0x1C0 + 2*p6`). |
| 8 / 108 | `Effect8_SpawnEffectHa6Actor` 0x4551B0 | Same record passed through (angle p8). Data pointer from the effect.ha6 global (`unk_562DA8`). |
| relative types | `Effect_InitializeComplex` 0x454900 | `if (*a1 >= 1000 \|\| *a1 <= 100) ... else v5 += *(actor+3)`: **100 < type < 1000 adds the spawner's pattern**, so 101, 108 and 111 are relative and 1000 is not. |
| 11 / 111 | `Effect11_SpawnRandomPatterns` 0x454E30 | See below. |

EF 11/111 (`Effect11_SpawnRandomPatterns` 0x454E30) builds a 14-int spawn block `v57` and calls `Effect_InitFromSpawnParams`:

- **Count and pattern.** `range = p5 ? p5 : 1` (`ef+6`), and `count = p6 ? p6 : range` (`ef+7`). The pattern is `no + i` when p6 == 0, else `no + rand % range`.
- **Position.** `v57[2] = p1 + rand % max(p3,1)` and `v57[3] = p2 + rand % max(p4,1)`.
- **Circle mode.** When `p4 == 30000`: radius `rand % max(p3,1)`, angle `rand % 360`, then `Math_PolarToCartesian`.
- **Flagsets and angle.** `v57[4] = p7` (fs1), `v57[5] = p8` (fs2), `v57[9] = p9 + (p10 ? rand % p10 : 0)` (angle), `v57[10] = p11` (proj var).

This matches the panel in `effect_spawn.h`, EX's layout for p1-p8, and `MBAA_NAME_AUDIT.md` §2.1.

### 3.2 Facing and the nested flip (#69)

From `Effect_InitializeComplex` 0x454900:

```
v4[785] = v4[784] = parent[784];                 // facing copied from the spawner
if (fs2 & 0x800) v4[784] = parent[752] & 1;      // "face according to player slot"
if (fs1 & 0x800) v4[784] = !v4[784];             // bit 11 toggles
x = p1 << s; y = p2 << s;                        // s = 7 or 8, see 3.3
if (v4[784]) x = -x;                             // only the child's OWN X
... v4[+0x104] = x + parent[+0x104]              // then add the parent position
if (fs1 & 2) facing = 0;                         // "always face right", applied last
```

Our old tree negated the **accumulated** parent+child offset when the child's own bit 11 was set. That was correct only at depth 0. The simulator now:

- propagates facing actor to actor
- mirrors only each EF's own X

`preview_sim_tool --selftest` cases 1 and 13 cover right- and left-facing roots, nested chains, and every flag combination the position tool can edit.

### 3.3 owner+796: the 0.5 vs 1.0 EF offset scale

- **The field.** `Effect_InitializeComplex` has `cmp word ptr [edx], 0` with `edx = [spawner+31Ch]`. `Actor_LoadPatternAndFramePointers` (0x45F650; decompiled at 0x45F6DA) stores `+0x31C` = **the current frame record** for (pattern `+0xC`, frame `+0x10`).
- **The flag.** `HA6_ParseAnimationFrameChunks` 0x409130 writes the frame record's first word from `AFGP` (`*frame = v20; *(frame+2) = v21`). Word 0 is `AFGP[0]`, which our loader calls `usePat` (`framedata_load.cpp:442`).
- **The rule.** When the spawner's current frame is a PAT frame, offsets are `<<7`; for a CG frame they are `<<8`. `Effect2_VariousEffects` 0x455450 uses the same test.
- **Units.** `Camera_GetScreenBoundary` 0x44C300 gives a screen half-width of 40960 and a height of 61440 (4:3). The world is therefore 128 units per 640x480 screen pixel, and CG sprites are drawn at 2x (EX's `mbaacc_transform.h` also notes "MBAA's separate 2x CG draw scale").
- **Conclusion.** In CG-pixel units, which are Hantei-chan's viewport units, the EF offset scale is **1.0 for CG-frame spawners and 0.5 for PAT-frame spawners**. EX's 0.667 is not an engine constant. The rule is MBAA's, so it is disabled for UNI/MBTL-format data (`Options::patOwnerHalfScale`, cleared when `usesUniFormat()`).

### 3.4 Positioning modes

All from `Effect_InitializeComplex`. `dx`/`dy` is the scaled own offset after the facing mirror. Camera X is the screen centre (`g_GlobalEffectVars[827]`); the edges are `camX ∓ 40960/zoom ± 2048` (= ∓160 ± 8 CG px).

| Flag | Effect | Simulator |
|---|---|---|
| fs2 `0x200` | fixed: `X = (p1-128)<<8`, `Y = p2<<8`, facing 0, early return | `x = p1-128, y = p2` |
| fs2 `0x100` | `dx += ±dist(target)`, sign by target side (`Character_GetTargetAndDistance`) | `dx += opponentX - parent.x` |
| fs1 `0x400` | X = left edge if facing left, else right edge, + dx; Y = parent + dy | same (`cameraX`, `cameraZoom`) |
| fs1 `0x200` | X = left edge if facing right, else right edge, + dx | same |
| fs1 `0x10` and fs2 ≥ 0 | camera-relative: `camX + dx`, `camY + dy` | `cameraX/Y` options |
| fs1 `0x10` and fs2 sign bit | screen-absolute: `dx`, `dy` | same |
| fs1 sign bit | `Y = p2 << 8` (absolute, full scale) | `y = p2` |
| fs1 `0x2` | facing forced right after positioning | same |
| fs1 `0x100` | `angle += spawner[+0x2FC]` (actor angle, **not** AF layer rotation) | `angle += parent.angle` |
| fs2 `0x400` | priority field 460 ("top Z") | initial `zPriority = 460` |

The preview context lives in `preview::Options`:

- root at (0,0), facing right
- camera at (0,0)
- zoom 1
- opponent at X = 160
- P1 slot

### 3.5 Frame advance, loops, pattern end

From `Character_AdvanceFrameByAniFlag` 0x461150, which runs after `++timer` (`+0x18`) with `if (timer < AF duration) return`:

| aniType | Engine |
|---|---|
| 0 | `timer = dur; +0x1F = 1; queuedPattern = AF jump (+ own pattern if flag 4)` |
| 1 | `++frame; timer = 0; Character_InitializeFrame` |
| 2 | `if (cnt) --cnt; if (!(flag&2) \|\| cnt) frame = jump (+cur if flag 4) else frame = loopEnd (+cur if flag 8); Character_InitializeFrame` |
| other | nothing (the frame holds) |

- **Loop counter.** The counter is the byte `+0x21`. `Character_InitializeFrame` loads it only when AF `+20` (AFCT) != 0. Note that this differs from Hantei's old rule (decrement to -1 before taking loopEnd): with AFCT = 3 the engine takes the loop 2 more times and exits on the 3rd pass.
- **Every entry dispatches EFs.** `Character_InitializeFrame` ends with `Effect_DispatchFrameEFs`, and it is called from:
  - `AdvanceFrameByAniFlag` (every branch, **including a jump to the same frame**)
  - `Character_JumpToFrame` 0x4616E0
  - `StartQueuedPattern`
  - the spawners

  So self-jumps re-fire (inventory 3.2.2, fixed).
- **Running off the end.** A frame index past the end makes `Actor_CheckFrameIndexValid` return -2, and the pattern and frame reset to 0. The simulator ends the root there (holding its last frame) and despawns a spawned actor, with an event.
- **Pattern end.** When `+0x1F` is set, `Character_RunFrameTick` runs `Actor_RunIf2DestroyOnHitPass` 0x469100 on the current frame's IFs. **An IF 2 with p3 != 0 destroys the actor.** Otherwise `Character_StartQueuedPattern` 0x460CA0 starts `AF jump`. In MBAACC data, about 3,300 of 3,500 surveyed spawn targets end `aniType 0, jump 0` with IF 2 p3 set (survey in the session log).
- **Queued pattern 0.** A spawned actor queuing pattern 0 despawns in the preview, with an event, because the engine would restart pattern 0 of its data. A root that queues any pattern "ends" and holds.
- **Pattern-start destruction.** `Actor_ResetStateOnPatternStart` 0x45F2D0 calls `Effect_ForceDestroyChildrenWithFlag20`, so children with link flag 0x20 die when their parent starts a new pattern, including the root's end. The link flags come from fs1 bits `1|4|8|0x20|0x40`, plus fs2 `0x2` setting 0x20.
- **Parent destroyed.** `Effect_DetachChildrenOnParentDestroy` 0x453A00 destroys children with link 0x21.
- **Priority.** `Character_InitializeFrame` applies `Actor_ApplyAFLayerPriority(AF+46)` **only if non-zero**, so AF priority 0 keeps the previous value (inventory 3.2.7, verified). The absolute mapping in `Actor_ApplyAFLayerPriority` is not modelled; raw AF priority values are sorted.

### 3.6 PAT palette colour slot 0

- **MBAA and hantei4.** MBAA's PAT loader `File_ParsePPTPChunks` 0x404220 handles only PPTP, PPCC, PPUV, PPSS, PPTX, PPJP, PPNM and PPED. **There is no PPCL/PPPA (colour slot) tag.** hantei4.exe has no PAT support at all. The colour slot is a UNI-era feature, and every MBAACC cutout keeps `colorSlot = -1`, so the choice of check is moot for MBAACC.
- **UNI.** No UNI binary is loaded in IDA. Palette entry 0 is the transparent key colour, EX reports white material turning blue with the `!= -1` check (Linne pattern 135), and old Hantei used `> 0`.
- **Decision.** Adopted `colorSlot > 0`. Confirm against a UNI binary when one is available.

### 3.7 Flip in the renderer

The actor matrix is `F · R(angle)` (EX `MbaaTransform::ActorMatrix`, runtime-verified by EX). DrawBack mirrors via `layer.scaleX *= -1` and adds `actor.angle/10000` to `rotZ`. The renderer applies `scale · Rz`, which equals `F·R` for uniform layer scale. `SimActor::transform()` exposes the exact matrix for consumers.

---

## 4. IF conditions

`Cond_Dispatch` 0x469599; the IF record's `p1` is `parameters[0]`.

**Native (always simulated):**

- IF 9: loop counter = p1.
- IF 10: jump p1 if the counter is 0.
- IF 37: queue pattern p1 (unconditional).
- IF 55: frame p1 or p2 by player slot parity.
- IF 18: jump p1 if the parent's pattern == p2 (p2 == 256 uses a runtime flag, so it is treated as runtime).
- IF 2 p1 bits 1/2: off-screen X/Y against the preview camera (±50 px margin, as the engine uses ±12800 units).
- IF 2 p3: destroy at pattern end (above).

**Runtime (user assumption):**

| IF | Branch when true |
|---|---|
| 1 | p2 ≥ 10000 → frame p2-10000, else queue pattern p2 |
| 3, 14, 25 | p1 ≥ 10000 → queue p1-10000, else frame p1 |
| 4, 5, 15, 17, 20, 21, 22, 24, 28, 29, 30, 33, 36, 39, 40, 41, 42, 50, 52, 54, 60, 150 | frame p1 |
| 6 | frame p3 (+ current frame if p5) |
| 7 | queue p3 |
| 8 | p3 == 0 → frame p1, else queue p1 |
| 12 | p3 & 2 → queue p1, else frame p1 |
| 13 | p1 != -1 → p2 ? queue p1 : frame p1 |
| 27 | p2 ? frame p1 : queue p1 |
| 2 (p2 landed), 53 | destroy |

- **Not modelled (no simple branch):** 11, 16, 19, 26, 31, 34, 35, 38, 70, 100, 151 (commands, velocity, reflection, variables).
- **Assumption semantics.**
  - `False` (default): the condition never holds, so the authored AF flow is followed.
  - `True`: the branch is taken on the first evaluation of each frame visit, which is the tick after entry, before the update.
  - The global default is in `Options::defaultIfAssumption`.
  - Per-IF overrides are in `Options::ifOverrides`, keyed by `IfKey{effectHa6, pattern, frame, index}`.
- **UI marking.** Every runtime IF the simulation meets is recorded once as an `IfUnmodeled` event (or an `IfAssumedTrue` event each time one fires). The Spawn Timeline's "Runtime conditions" list shows them with a Default/False/True override each.
- **Not modelled at all:** landing (AF landJump), hitstop, motion (AS velocities; spawned actors stay where they spawned), hits and EF 6 behaviours.

---

## 5. Consumer API (onion skin, PNG export, detached views)

```cpp
#include "preview_sim.h"

// Per view: FrameState owns a shared_ptr<preview::PreviewSim> and the options.
preview::PreviewSim& sim = state.BindPreviewSim(&character->frameData, effectFrameDataOrNull);

preview::TickState ts;                 // caller-owned; reuse across calls
sim.getStateAt(tick, ts);              // clamped to [0, horizon]; false if no root
for (const preview::SimActor& a : ts.actors) {
    // a.isRoot / a.isPreset / a.isScript
    // a.pattern, a.effectHa6, a.frame, a.frameTimer   -> which frame to draw
    // a.x, a.y (world, root at 0,0), a.facingLeft, a.angle (10000 = 360°)
    // a.transform()  = translate(x,y) * MbaaTransform::ActorMatrix(angle, facing)
    // a.zPriority, a.id, a.parentId, a.depth
    // a.srcPattern / srcFrame / srcEffectIndex / srcSubIndex: authored EF record
}

sim.settledTick();          // first tick with only an ended root left (-1: never)
sim.rootEndTick();          // -1 if the root never ends within the horizon
sim.rootFrameTrack();       // root frame per tick for [0, frontier]
sim.firstTickOfRootFrame(f);
sim.spawnRecords();         // every actor ever spawned: spawn/death tick, source
sim.events();               // spawn/despawn/pattern/IF/limit events with reasons
sim.stats();                // ticksStepped, lastQuerySteps, checkpoints, invalidations
```

Guidance for the next wave:

- **Onion skin.** Call `getStateAt(t ± k·spacing)` for each sample. The four LRU cursors serve a burst around one tick at about one simulated tick per sample (a 17-sample burst costs 0.05-0.21 ms, §6.2). Do **not** re-run anything per pass.
- **PNG export.** Iterate `t = 0..settledTick()` sequentially. Each call steps one tick from a cursor.
- **Detached views.**
  - Each view has its own `FrameState`, and therefore its own simulator and cache.
  - Two views of the same pattern each pay the frontier build once (at most ~3 ms, §6.1).
  - The simulator has no GL state, so render passes can share it freely.
- **Stable identity.** Actor `id`s are deterministic for a given (data, pattern, options). Use `(srcPattern, srcFrame, srcEffectIndex, srcSubIndex)` to map an actor back to its EF (the position tool, selection).
- **Position tools.**
  - Place handles with `preview::ResolveSpawnPlacement(options, spawner, spawnerFrameUsesPat, fs1, fs2)`, which returns `base + k·(X,Y)`.
  - Invert with `1/kx`, `1/ky`.
  - Get the parameter slots from `preview::SpawnOffsetSlots` / `SpawnFlagsets`.
  - `--selftest` case 13 asserts this equals the simulated spawn position for 48 flag/facing/PAT combinations.

Headless use is `preview_sim_tool` (no GL/ImGui/Windows dependencies beyond `framedata`):

```
preview_sim_tool --selftest
preview_sim_tool --data C:/games/mbaacc_tag/data --char aoko --pattern 156 --every 10 --events
preview_sim_tool --data C:/games/mbaacc_tag/data --char ciel --scan
preview_sim_tool --data C:/games/mbaacc_tag/data --char hisui --pattern 170 --bench
```

On this WSL host, wine is broken ("could not load kernel32.dll"), so run the MinGW exe through WSL interop with a Windows-style data path, as above.

---

## 6. Verification

### 6.1 Correctness

`--selftest` (synthetic FrameData, no game data needed), all 28 checks passing:

1. Nested facing for right and left roots.
2. Self-jump re-fire.
3. Engine loop counter.
4. PAT half scale.
5. Fixed / camera / edge / opponent / absolute-Y positioning.
6. IF2 p3 destroy vs AF-jump chaining.
7. Link-0x20 destruction on root end.
8. EF11 count/range.
9. Runtime IF false/true override and event.
10. Invalidation via `mark_modified` and via a silent edit (fingerprint).
11. EF1000 once.
12. EF8 into effect.ha6 with angle, and angle inheritance.
13. Position-tool placement == simulated position (48 combinations).

`--scan` over every pattern of MBAACC characters. Each pattern is simulated to settle (or 600 ticks). 64 random-order queries per pattern are compared field by field with sequential replay: ids, pattern, frame, timer, position, facing, angle.

| Char (moon 0) | Patterns | With spawns | Actors spawned | Max alive | Worst frontier build | Random query avg / worst | Mismatches |
|---|---|---|---|---|---|---|---|
| aoko | 315 | 104 | 3,515 | 24 | 3.0 ms | 0.0021 / 0.07 ms | 0 |
| ciel | 309 | 104 | 5,425 | 42 | 3.1 ms | 0.0025 / 0.33 ms | 0 |
| hisui | 380 | 127 | 6,629 | 24 | 2.6 ms | 0.0021 / 0.08 ms | 0 |
| ryougi | 507 | 200 | 3,777 | 25 | 1.8 ms | 0.0019 / 0.22 ms | 0 |
| warakia | 267 | 96 | 5,802 | 24 | 1.5 ms | 0.0019 / 0.09 ms | 0 |
| arc | 326 | 123 | 7,183 | 33 | 1.8 ms | 0.0019 / 0.25 ms | 0 |
| sion | 221 | 58 | 5,282 | 24 | 1.6 ms | 0.0016 / 0.04 ms | 0 |
| akiha | 270 | 94 | 5,906 | 24 | 1.4 ms | 0.0021 / 0.15 ms | 0 |

In single-pattern `--bench` mode, 8 fresh from-zero simulations per pattern are also compared, again with 0 mismatches.

Spot checks of actor lists (plausible and stable across runs):

- **Aoko 156.** Frames 1-3 loop 8x (AFCT 8 on frame 0), then frames 10-12 loop 8x. Each pass fires EF11 → 167 at a random point in the authored rectangle (x -57..+44, y -100..+4). There are 25 actors and the root ends at tick 318.
- **Arc 192.** Repeated EF1 → 193 at the hand offset (33,-77). Up to 33 alive, root end 190, settled 239.
- **Ryougi 331.** EF1 with bit 11 (fs1 0x805): the children face left at the root origin.

### 6.2 Performance

Measured with `--bench` on the Windows host (MinGW -O2 Release, WSL interop). "EX" is a port of EX 1abc27f9's `SimulateSpawnsToTickInternal` inside `preview_sim_tool.cpp`, used only for this comparison.

| Pattern (span) | Seq. playback / tick | Random seek | 17-sample onion burst | Invalidate + rebuild | Validate overhead | EX per query at the last tick | EX x17 (onion) |
|---|---|---|---|---|---|---|---|
| ciel 199 (194 t, 42 actors) | 0.012 ms | 0.016 ms | 0.21 ms | 0.13 ms | 0.012 ms | 1.15 ms | 19.5 ms |
| aoko 223 (600 t) | 0.003 ms | 0.022 ms | 0.09 ms | 0.88 ms | 0.002 ms | 1.54 ms | 26.1 ms |
| aoko 156 (318 t) | 0.002 ms | 0.006 ms | 0.05 ms | 0.13 ms | 0.002 ms | 0.55 ms | 9.4 ms |
| hisui 170 (600 t) | 0.007 ms | 0.016 ms | 0.14 ms | 0.46 ms | 0.006 ms | 5.72 ms | 97.2 ms |

- EX's cost grows quadratically with the tick. For Hisui 170 it goes from 0.10 ms at t150 to 0.97 ms at t300 and 5.7 ms at t600.
- The simulator's query cost is flat: at most one checkpoint interval.
- Our worst case is an edit (full rebuild), under 1 ms here and at most 3 ms over all 2,600 scanned patterns.

---

## 7. IDB naming corrections (not applied: the session is read-only)

| Address | Current name | Proposed | Why |
|---|---|---|---|
| 0x4690D0 | `Cond2_DestroyIfHitFlagSet` | `Cond2_DestroyIfPatternEnded` | `+0x1F` is the pattern-end flag set by `AdvanceFrameByAniFlag` aniType 0 (`actor->_pad_14[11] = 1`), not a hit flag |
| 0x469100 | `Actor_RunIf2DestroyOnHitPass` | `Actor_RunIf2DestroyOnPatternEnd` | same flag; called right after the frame advance |
| actor `+0x1F` | (`_pad_14[11]`) | `patternEndedFlag` | |
| actor `+0x31C` | (`frameData` / `a1[199]`) | `curFrameRecord` | set by `Actor_LoadPatternAndFramePointers`; word 0 = AFGP[0] |
| actor `+0x2FC` | — | `spawnAngle` | EF angle; inherited with fs1 bit 8 |
| 0x454900 | shows as `Effect_InitializeComplex` in the decompiler; callers show `Effect_InitFromSpawnParams` | keep `Effect_InitFromSpawnParams` everywhere | name mismatch in the IDB |

`MBAA_NAME_AUDIT.md` §3, IF 2: "p3: this actor was hit" should read "p3: destroy when the pattern ends (aniType 0)". Our UI label ("Despawn on pattern transition") is already close.

---

## 8. Inventory items closed or changed

| Item | Status |
|---|---|
| 3.2.1 per-draw re-simulation | closed (cached simulator) |
| 3.2.2 self-jump re-fire | closed |
| 3.2.3 lifetime from summed durations | closed |
| 3.2.4 IF-driven branches | partially closed (native IFs simulated, runtime IFs by assumption, listed in the UI); landing and hitstop are still open |
| 3.2.5 inherit rotation from root layer 0 | closed (parent actor angle) |
| 3.2.6 / 4.16 flip | closed; plus fs2 0x800, positioning modes and the owner scale |
| 3.2.7 priority 0 = keep | verified in `Character_InitializeFrame` |
| 3.2.9 `CalculateTickFromFrame` | now returns the first tick the simulated root flow enters the frame (the authored start tick if unreachable), not EX's "last tick" |
| 3.2.10 dead code, stale comment | removed with the old paths |
| B1, B2, B4, B5, B6, B6b, B8 (header adopted; transform used by `SimActor::transform()` and the render mirror), E1, E5 | done |
| B3 stable sort | was already `std::stable_sort` in our `render.cpp` |

---

## 9. Known limits and follow-ups

- **No motion.** AS velocities, gravity and EF6 movement are not integrated, so projectiles stay where they spawned. Off-screen IF2 therefore rarely triggers, and landing and IF 4 velocity checks are runtime assumptions. This is the largest remaining gap: a follow-up can integrate `Frame_AS` using `Character_ApplyStateVelocity`/`Character_IntegrateMotion`.
- **Priority mapping.** `Actor_ApplyAFLayerPriority` (parent-relative priority) is not modelled; spawns sort by raw sticky AF priority, and fs2 0x400 = 460.
- **EF3 presets.** EF3 preset markers use the EF1 placement rule; `Effect3_SpawnPresetEffect` 0x4553D0 was not re-checked.
- **MBTL move-script spawns.** These are placed with scale 1.0 and inherit facing like EF1; the script engine's own rules are not modelled.
- **Root pattern changes.** A root that queues another pattern (IF assumed true, IF 37, aniType 0) "ends" and holds its frame. It does not play the follow-up pattern, because the editor view shows a single pattern.
- **Root facing and camera options.** `Options::rootFacingLeft` and the camera/opponent options have no UI yet. DrawBack mirrors spawns relative to the root's facing, but the main sprite itself is not mirrored for a left-facing root.
- **Untested GUI path.** The GUI integration (DrawBack, RenderUpdate, timeline, position tool) builds, but was not exercised interactively in this session: the app has no command-line file open, and the host GUI was not driven. The headless tool covers the simulator and the placement math that those paths call.
