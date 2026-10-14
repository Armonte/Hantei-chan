# fix/pat-scale: G_CHAOS Arc Drive "1.6x / 6x larger parts" after S1

Branch `fix/pat-scale`, worktree `wt/fix-patscale`, based on `update/parity-1014` (b3af38c). Nothing was pushed or merged.

## Verdict: there is no S1 scale regression. The S8 shots compared different view zooms.
- The S8 `before_*` script (tools/ui/s8/shots_before.txt in the s8 worktree) captures p100 f10 without a `zoomview`,
  so the view is at the default zoom of 3.0 (`gSettings.zoomLevel`, src/ini.h; the "1.00x" in the timeline is
  not the view zoom). The `after_*` and `control_*` scripts run `zoomview 1`. Sprite and boxes are both
  about 6x larger in `before`, which is that zoom ratio. The "first capture after load is ~6x" note is the same
  thing: it is the default zoom, not a mid-load render state.
- In a controlled A/B run (same script, same zoom, pre-S1 `builds/gonptechan_2026-10-04.exe` vs the S1 build),
  every editor frame view is pixel-identical: p100 f10 at zoom 1 and 3, p0 f0, the first capture after load
  and after 30/60 frames (`--ui-script` and `--capture-view`). Only mid-frame timeline ticks (t28, t30) differ.
  That is S1's AFHK lerp of the PAT layer: p100 has AFHK 1 on every frame, and pre-S1 did not interpolate.
  At those ticks the sprite size is unchanged.
- Data check: part sets 88-91 hold the same parts. PRZM is 2.0 or 2.06 and there is no PRAS, so slot-paired
  interpolation cannot scale anything by 1.6x. The weight is clamped to [0, 1], so a lerp never extrapolates.

## What MBAA.exe actually does (MBAA.exe.i64, CC build)
- The .pat parser `File_ParsePPTPChunks` 0x404220 has no PRAS and no PRA3 tag (the tag strings appear only for
  PRST/PRXY/PRZM/PRAN/...). The part matrix `PatPart_BuildTransformMatrix` 0x405C70 is scale * rotZ * translate.
  It lerps scale, rotZ and x/y toward the next part set's same slot (`Animation_RenderSortedParts` 0x4061B0
  pairs by slot) with weight `Easing_ApplyCurve(AFHK, x)`, and skips the lerp when the weight is 0.
- `AnimState_ProcessTransition` 0x419C00 sets x = 1 - timer/duration, which matches S1's CurrentWeight. It
  picks the loop-end target when the loop counter is **0**; uni2 does that at 1.
- `SpriteAnimation_Render` 0x407420 passes the next sprite to the part renderer only if the next frame's
  layer is also a PAT layer, as S1 does.

## Changes (commits on fix/pat-scale)
- `ecf454a` src/parts/part_render_math.h: `PartUsesPras(uniDialect, mbtl)` returns true only for
  UNI2/UNI-family, not MBTL or MBAACC. `LoopEndCounter(uniDialect)` is 1 for UNI and 0 for MBAA.
  src/ui/view_render_impl.h and the pattern-comparison overlay (src/ui/tool_windows_impl.h) use them.
  Before this, MBAACC characters applied PRAS (`!mbtl`), so a converted or UNI-made .pat with PRAS drew
  differently from the game. This was already true before S1 and does not affect G_CHAOS, which has no PRAS.
  tests/part_render_math_test.cpp adds `TestFirstFrameAndMbaaDialect`. It checks that the first tick never
  blends (any curve or duration), that stale or garbage tick counts keep the weight in [0, 1], the PRAS
  dialect rule, and the MBAA loop-end counter.
  tools/ui/patscale/ holds the repro (`shoot.sh` for the pre-S1 vs current `--capture-view` cases, and
  `shots*.txt` for `--ui-script`).
- `d4080fd` CMakeLists.txt: pattex links src/app_log.cpp. It failed to link on parity-1014 because parts.cpp
  logs through quill.
- `2b86913` (cherry-pick of f3c9f5f): ab_shot.sh `AB_EXE`.
- tools/uni/s1_compare/shoot.sh: `W` and `OUT` env overrides, and the capture path follows `W`.

## Verification
- `ctest`: 35/35 passed. part_render_math_test: 152 checks.
- S1 cases (tools/uni/s1_compare, 14 UNI2/MBTL cases including the PRAS case chr020 p5): the S1 build and
  this build are pixel-identical (the scratch run was in work/patscale_s1cmp/out).
- G_CHAOS: this build and the S1 build are pixel-identical in all 6 cases. Evidence is in
  docs/audit_2026_10_14/evidence/patscale/: `*_old` = pre-S1 and `*_new` = this fix; pre_s1_vs_s1/ = pre-S1
  vs the S1 build, plus the default-zoom `--ui-script` captures.

## Open
- tell S8: redo `before_*` with `zoomview 1` (or `--zoom 1`) to get a fair comparison.
  The 2026-10-04 exe has no `zoomview`; use `--zoom` with `--capture-view` as tools/ui/patscale/shoot.sh does.
- `ab_shot.sh` always starts at zoom 3. Scripts that compare sizes should set `zoomview` before the first capture.
- MBAA skips the part lerp when the weight is 0 (only possible with AFD 0). Our renderer goes fully to the
  next frame there. This is an edge case and was left as it is.
