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
| determinism commit (BASELINE + this only is the golden binary) | `81a948ad6647dd520733ad7ee47558bc68889134` |
| determinism exe sha256 | `2e7039ee321db132f231d66d982f594e2558adfa5d7cdb3a62f4a18052f4e304` |
| golden | `/mnt/c/dev/hantei-chan/render_parity/golden/45ea8ae59c1efd7bb6c1a9bc884c4205cef3174a` |
| runs | `/mnt/c/dev/hantei-chan/render_parity/runs/<run-id>` |

## Phase H: parity harness (PLAN.md 4)

- [x] 1. Baseline worktree and binary (plain build at `BASELINE`; staged under
      `baseline_bin/<BASELINE>/`).
- [x] 2. `tools/render_parity/{make_corpus.py,run_corpus.sh,diff.py,BASELINE}`
      plus `test_diff.py` (ctest `render_parity_tools`, no GPU).
- [x] 3. Corpus generated (2635 cases); two plain-baseline runs rendered; the
      flakiness found (39/2757 cases) and fixed with `--deterministic`.
- [ ] 4. Render the golden with the determinism binary.

**Corpus** (committed `tools/render_parity/corpus/`): `chars_<game>.batch` for
uni2 832, mbtl 463, unib 188, dfci 372, mbaacc 532; `stages_2d.batch` 9;
`ui/stage3d_*.txt` 230; `ui/mbac_chars.txt` 12; `ui/stages_mbac.txt` 2; and one
ui-script per committed shot set (39 files) covering areas D and E.

| area | where |
|---|---|
| A character frames | `corpus/chars_*.batch` (every listed game; variants zoom 1/2/0.5, boxes 0/1, bg 0 and 255, one 1920x1080 and one 333x257 body) |
| B onion skin / compare overlay | not yet written (5 + 5 cases) |
| C stages | `corpus/stages_2d.batch` (MBAACC/MBAC 2D, 3 camera positions each) and `corpus/ui/stage3d_{uni2,mbtl,dfci,unib}.txt` (every BgList stage, centre/left/right) |
| D UI | `corpus/ui/<set>__<script>.txt` for every `tools/ui/shots*/` script |
| E PNG export | not yet written (5 patterns) |

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

Fixed entry in `tools/render_parity/flaky.tsv`. Verified by three repeated runs
of the affected areas, byte-identical.

## Gates

| gate | result | run | counts | date |
|---|---|---|---|---|
| H1 (two baseline runs byte-identical) | pending | | | 2026-10-18 |
| H2 (`test_diff.py` + `diff.py golden golden`) | `test_diff.py` 14/14 PASS | local | 14 ok | 2026-10-18 |

## BLOCKED

None.

## Allowlist changes

None yet. `tools/render_parity/allowlist.tsv` is empty (header only) and stays
empty until a P2 `EMU`/`AA`/`FLT` case needs an evidence-backed entry.
