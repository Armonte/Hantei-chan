# render/d3d9 status

Plan: `docs/plans/render_d3d9/PLAN.md`. Branch `render/d3d9`, worktree
`/home/teo/dev/hantei-chan-wt/render-d3d9`. One checkbox per PLAN.md section 6
step, the latest gate result under each phase, a BLOCKED section and the
allowlist-change log at the bottom.

Naming the two shas, as PLAN.md 4.2/4.3 asks.

| what | sha / path |
|---|---|
| `BASELINE` = origin/main when the phase started | `45ea8ae59c1efd7bb6c1a9bc884c4205cef3174a` |
| plain baseline exe sha256 | `a347a3e7aa1d3d535f93c5857515bb8e6ebe532ee1224316b1ce5e724cda9c1b` |
| determinism commits (BASELINE + these four only is the golden binary) | `81a948a`, `f2e1cc0`, `a4eb48f`, `4f6ba36` |
| determinism exe sha256 | `cf92645f1c77f7bd9780631d1d6e46fdd2b445b96e16150f7f7842de1aad7113` |
| golden | `/mnt/c/dev/hantei-chan/render_parity/golden/45ea8ae59c1efd7bb6c1a9bc884c4205cef3174a` |
| smoke golden (60 cases, outside git) | `/mnt/c/dev/hantei-chan/render_parity/golden_smoke` |
| runs | `/mnt/c/dev/hantei-chan/render_parity/runs/<run-id>` |

## Phase H: parity harness (PLAN.md 4)

- [x] 1. Baseline worktree and binary (plain build at `BASELINE`; staged under
      `baseline_bin/<BASELINE>/`).
- [x] 2. `tools/render_parity/{make_corpus.py,run_corpus.sh,diff.py,BASELINE}`
      plus `test_diff.py` (ctest `render_parity_tools`, no GPU).
- [x] 3. Corpus generated (2635 cases); two plain-baseline runs rendered; the
      flakiness found (39/2757 cases) and fixed with `--deterministic`.
- [x] 4. Golden rendered with the determinism binary, twice.

**Gate H1: PASS.** Two runs of the determinism binary are byte-identical on
2847/2847 cases (0 differing).

**Gate H2: PASS.** `test_diff.py` 14/14 and `diff.py golden golden` = 2757/2757
PASS (the earlier corpus size; the current corpus is 2847).

**Corpus** (committed `tools/render_parity/corpus/`): `chars_<game>.batch` for
uni2 832, mbtl 463, unib 188, dfci 372, mbaacc 532; `stages_2d.batch` 9;
`ui/stage3d_*.txt` 230; `ui/mbac_chars.txt` 12; `ui/stages_mbac.txt` 2; and one
ui-script per committed shot set (39 files) covering areas D and E.

| area | where | cases |
|---|---|---|
| A character frames | `corpus/chars_*.batch` (every listed game; variants zoom 1/2/0.5, boxes 0/1, bg 0 and 255, one 1920x1080 and one 333x257 body) | 2387 |
| B onion skin / compare overlay | `corpus/ui/overlay_*_onion224.txt` and `..._cmp.txt` (5 + 5, one process each) | 10 |
| C stages | `corpus/stages_2d.batch` (MBAACC/MBAC 2D, 3 camera positions each) and `corpus/ui/stage3d_{uni2,mbtl,dfci,unib}.txt` (every BgList stage, centre/left/right) | 248 (+230 stage3d shots) |
| D UI | `corpus/ui/<set>__<script>.txt` for every `tools/ui/shots*/` script (39 files) | ~180 |
| E PNG export | `corpus/ui/export_{tr,ed,bx,ns}_*.txt`, 5 patterns each, scale 2 | 20 |
| smoke subset | `corpus/smoke.batch` (idle frame of every character, zoom 1/2 + the 333x257 body) | 60 |

MBAC ActCadenza characters (.DAT) open through the header-based File > Open route
(a batch `open` sends .DAT to the stage loader): `corpus/ui/mbac_chars.txt` and
`corpus/ui/stages_mbac.txt`.

**Detached windows (area D, the last item of PLAN.md 4.2 D) are not in the
byte-identical corpus.** `--detach` only creates an OS window when the app is
not headless (`MainFrame::ProcessStartupArgs` skips it under `--headless`, and
ImGui's viewports are off there), so the harness cannot capture them. They are
covered by the P0 gate's visible run instead ("run every `tools/ui/shots*/`
script visibly once with `AB_VISIBLE=1`"), where `--detach` +
`--capture-windows` and `--post-key` are exercised by hand.

## Flakiness (PLAN.md 4.3)

Two plain-baseline runs differed on 39 of 2757 cases. Two wall-clock sources,
both fixed by `--deterministic` (commit `81a948a`), which pins the frame step
to 1/60 s the way the game's own background tick works:

1. `bg::Renderer::Update` accumulated real time and ran up to 8 animation steps
   per frame, so a loaded machine ran the stage animation past the captured
   frame (MBAACC stage shots came out black in 1 of 12 runs). Now exactly one
   step per rendered frame under the flag.
2. ImGui's modal dim fade (`g.DimBgRatio += io.DeltaTime * 6.0f`) reached a
   different value per wall-clock frame. `io.DeltaTime` is pinned to 1/60 s.
3. The corpus ui-scripts write back the scratch fixtures they read
   (`shots__s2_pat_editor.txt` ends in `savechar`; the CG-manager scripts save
   banks, palettes and a rebuilt bank), so run 2 started from run 1's output.
   `run_corpus.sh` now calls `tools/render_parity/prepare_fixtures.py` before
   every run, which recreates `work/ab/s2/chr000.pat` from
   `gamedata/uni2/data/chr000/chr000.pat`, the `ab_s45/fx` tree from the MBTL
   chr000 + BaseData.HA6 + MBAACC akiha sources, `new_sprite.png` and the cgtool
   `plain` export exactly as `tools/ui/cgm_smoke.sh` builds them. --no-reset
   skips it. Fixing the cause, not the golden.
4. The welcome screen's install scan and the archive browser's preview and
   thumbnail jobs run on worker threads, so under load a thumbnail could still
   be missing at the frame a capture was taken (3 cases, 1-4 pixels, in one run
   of 24). `--deterministic` now runs the install scan
   (`abrowser::g_installScanSync`) and the browser jobs
   (`abrowser::SetDeterministic`) inline, and keeps the workers' no-GL-upload
   flag around the inline job.

Fixed entry in `tools/render_parity/flaky.tsv`. Verified by three repeated runs
of the affected areas, byte-identical.

## Gates

| gate | result | run | counts | date |
|---|---|---|---|---|
| H1 (two baseline runs byte-identical) | **PASS** | `golden/<BASELINE>` vs `..._repeat` | 2847 cases, 0 differing | 2026-10-18 |
| H2 (`test_diff.py` + `diff.py golden golden`) | **PASS** | local | test_diff 14/14; diff golden golden 2757/2757 PASS | 2026-10-18 |
| ctest | **PASS** | `build/ctest -j8` | 93/93, incl. `render_parity` 60/60 (45 s) and `render_parity_tools` | 2026-10-18 |

## BLOCKED

## Phase P0: SDL3 platform, GL kept (PLAN.md 6 P0)

- [x] 1. SDL3 vendored as a submodule `deps/SDL`, pinned at the
      `release-3.4.18` tag (`829a65d769d935c4852f8159e964312c0957260a`,
      published 2026-10-02, 16 days old). Built static from the submodule by
      `cmake/sdl3.cmake` (`SDL_STATIC ON`, `SDL_SHARED OFF`,
      `SDL_TEST_LIBRARY OFF`). Verified with `make SDL3-static`: SDL 3.4.18
      cross-compiles cleanly with x86_64-w64-mingw32 into `libSDL3.a`.
- [ ] 2. `src/platform/platform.{h,cpp}`.
- [ ] 3. `main.cpp` SDL main loop.
- [ ] 4. ImGui backends.

None.

## Allowlist changes

None yet. `tools/render_parity/allowlist.tsv` is empty (header only) and stays
empty until a P2 `EMU`/`AA`/`FLT` case needs an evidence-backed entry.
