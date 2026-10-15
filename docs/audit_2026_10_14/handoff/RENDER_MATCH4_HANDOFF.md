# Render match 4 handoff (2026-10-15, fix/render-match4 in wt/render-match4, from update/parity-1014 9e0fccc)

The table is in `docs/audit_2026_10_14/evidence/compare/v2/INDEX.md`, section "Round 5 / render-match4". Reproduce:

```
CASES='flipHV_chr019_p153_z1|ctrl_chr019_p0_z1|pups_chr027_p449f|mbtl_patctrl' tools/uni/s1_compare/shoot_batch.sh
tools/uni/s1_compare/v2_round5.sh
tools/gamecap/run.sh cases/v2r5_chr019.gcs   # also v2r5_chr027.gcs, v2r5_mbtl_chr001.gcs (copies in v2/gamecap_scripts)
```

No C++ changed, so uni_regress was not needed. ctest 46/46 (`v2/ctest_r5.log`). The editor shots came from the wt/parity build of the same commit
(9e0fccc), because the worktree build was still compiling.

## Done
1. **flipHV_chr019_p153: 9.23 / 27.4% -> 7.37 / 22.0%, band 3.85 / 10.3% -> 2.35 / 5.0%** (floor 3.08 / 7.3%,
   band 1.28 / 2.5%).
   - The residual left after the aura fix was the plate. The round-4 plate was grabbed after `unfreeze` with P1's
     body still drawn under the streaks.
   - `v2r5_chr019.gcs` takes a same-session plate (hidefighters + one UNI2-IM step).
2. **mbtl_flipV_chr001_p157f2: at its floor.**
   - New .pat-only MBTL controls for chr001: p238 f10 (set 275, PRAL 1 + translucent) and p277 f0 (set 211). Neither
     has PRRV, PRAL 2/3 or PRAS, so S1 did not touch their render path.
   - Controls: 4.55 / 11.1% (band 2.75 / 7.8%) and 4.83 / 13.7% (band 2.21 / 6.3%).
   - Case: 4.03 / 13.5% (band 2.20 / 4.7%).
3. **pups_chr027_p449: now measured.**
   - f0 draws nothing on either side.
   - f4 puts cutout 427 (PPPA 254, PRAL 1, set 839) on screen with CG 869: 4.17 / 14.5% (band 3.15 / 11.9%) against
     ctrl_chr027 r5 4.67 / 10.2%. The mean is at the floor.
   - f6 is 6.16 / 22.0%. It is dominated by an AFZM-zoomed alpha-128 CG copy.
4. IDA uni2 1.42 (saved): renamed `Matrix4_SetIdentity` 0x413090, `ShaderCmd_QueueSetMatrixParam` 0x4156F0,
   `ShaderCmd_QueueSetFloatArrayParam` 0x415790, `ShaderCmd_QueueSetTechnique` 0x4AF210, `Math_CosPrecise`
   0x4107B0 and `Math_SinPrecise` 0x4133C0. Added a comment on `PatPart_GetGlobalPerspective640x480` 0x4C1860.

## Open
- **rect_chr024_p76: one top row, still not fixed.** There is no C++ change and no unit check, because the cause
  is not proven. Ruled out this round:
  - VS_CharaPtDrawV (`Shader/sh_chara.txt`) is plain World·View·Proj with no offset.
  - `PatPart_BuildTransformMatrix` 0x4C10E0 adds only PRXY, then the part perspective.
  - The part perspective (0x4C1860) leaves a z = 0 vertex at w = cot(fov/2) = 1.3032 and z/w = 0.992 part px. That
    is a uniform scale toward the screen centre: a 1-row y move would need about a 1.15 px x move, and x matches
    (d 0.00).
  - D3D9 pixel centres are integers and the top edge is at y 639.99999, so the top-left rule covers row 640, not
    639.
  - So the game's top edge sits at about y <= 639. The remaining suspect is the object or layer matrix (`a5`,
    `obj+52`) or camera y snapping.
  - Next step: read `g_mWorld` (`ShaderCmd_QueueSetMatrixParam` arg, PatPart_SetupWorldMatrixAndShader 0x415230)
    for this part with a gamecap breakpoint/peek and project y = 0.
- **flipHV streak edges.** The plain mean is 2x the floor. The residual is on the edges of the PRFL 1 streak parts:
  the game is stair-stepped, the editor smooth.
  - A zoom-1 editor shot gives 9.20 / 29.5% against the z1 control's 1.17 / 2.0%, so it is not the resample.
  - Check the game's sampler state for these parts (PRFL 1 -> linear per the editor). The other possibility is the
    PS alpha-0 discard `if (outcolor.a == 0) return 0` on a magnified texture with hard alpha.
- **pups p449 f6** carries the same afhk_cg-type sub-pixel edge on the zoomed CG copy.

## Commits
Final commit only: INDEX round 5, this handoff, v2_round5.sh, cases_control rows, editor_bw shots, sheets, logs,
gcs copies, sidecars. No push or merge.
