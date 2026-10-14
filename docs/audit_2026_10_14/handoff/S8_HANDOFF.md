# S8 handoff: MBAC HA4 -> HA6 converter fidelity (feat/s8-ha4-converter)

Audit: `/mnt/c/dev/hantei-chan/docs/mbac_moon/CONVERTER_FIDELITY.md` (outside the repo). Its verdict table now has a
status column, and new sections §2 M10/M11 and §2b give the S8 results. The pre-S8 copy is at
`gamedata/mbac_to_cc/roster_v2/_audit/CONVERTER_FIDELITY.pre_s8.md`.

## Status: done (session 2, 2026-10-14)
- **Code** (session 1): M1, M2/M9, M3, M7, M8 and M12 fixes are in b5fa998..9aa9c26. Details are in
  `Hantei_Docs/HANTEI_MBAC_SUPPORT.md` §3, §4 rules 9-11 and §6.
- **Build**: `./build.sh` is clean, including gonptechan.exe with the usePat box display scale.
- **ctest**: 25/25 pass, including `ha4_convert` and `ha4_golden`. Log: `docs/audit_2026_10_14/evidence/s8/ctest.log`.
- **ha4_golden**: still 44/44 tag patterns and 0 behaviour failures. The goldens did not need updating because
  section 1 compares CG frames.
- **Roster**: `gamedata/mbac_to_cc/roster_v2/` converted 50/50 OK (`_audit/convert_roster_v2.sh`, `_audit/convert_log.txt`).
  The `_cc_c.txt` files were copied from roster/.
- **Bosses**: `g_chaos/cc_data_v2` and `g_akiha_mbac/cc_data_v2` are cc_data with the HA6 swapped. The pat, cg and pal
  files are byte-identical to the old ones. Diff: `ha6patdiff_v1_v2.txt`.
- **Audit**: the new script `gamedata/tools/scripts/mbac_moon_convverify.py roster roster_v2` checks the converted
  output against the expected value for each mismatch id and prints the before/after table
  (`roster_v2/_audit/convverify_before_after.md`). Every M row went from N to 0. The other `mbac_moon_*` scripts measure
  exposure in the raw `.DAT` files, so their output is unchanged (`roster_v2/_audit/convfidelity/`).
- **MBAC round trip**: `ha4tool roundtrip` gives **50/50 byte-identical** (`evidence/s8/ha4_roundtrip.log`).
- **uni_regress**: complete (`evidence/s8/uni_regress.log`, `uni=1`). Both FAILs are unrelated to S8:
  - PAT fresh: `mbtl/.../chr012_ed00.pat` (PGT2 texture type). The same failure is in parity's baseline and S1 logs.
  - PAL: 272 files, 1 differ (`uni2/chr004/chr004.pal`). `pal_roundtrip.exe` hung for 68 min on that file under machine
    load, and I killed it (taskkill on that PID only), so the suite counted it as a DIFF. Run on its own afterwards it gives
    rc 0 and byte-identical output (`cmp`), 3/3 retries. So this was a transient WSL-interop hang, not a palette bug.
  - Everything else passes: HA6 262/262 bytes, PAT bytes 180/180, MBAACC .pat 19/19, #71 edits 5/5, ha6_regress PASS
    (MBAACC 305, stacks ok), MBAC ha4tool roundtrip 50/50 byte-identical, bg 56/56, unit tests done.
- **Shots** (`docs/audit_2026_10_14/evidence/s8/`, scripts in `tools/ui/s8/`, `--ui-script` only):
  - `before_*` = parity build with the old roster; `after_*` = this build with roster_v2;
  - `control_newbuild_oldroster_gchaos_p100_f10.png` isolates the converter change: same build, Arc Drive box ½ vs full;
  - `after_sion_p65_f0_shield_if51.png` shows IF 51 (3, 5, 130). The before value (−1, 5, 11) is in `data_before_after.txt`.
- **Tooling**: the new ui-script command `framenodes MASK` holds the AT/EF/IF nodes open (src/right_pane.cpp,
  startup_args.cpp). `ab_shot.sh` takes `AB_EXE` (on top of 3d72074, which was cherry-picked).

## Observations for other streams
- **G_CHAOS p100 f10 (Arc Drive)**: this build draws the parts sprite about 1.6x larger than the parity build at the
  same zoom. A capture straight after load showed it at about 6x. The p0 frames render identically, and the .pat is
  byte-identical between rosters, so the cause is the merged S1 PAT render (AFHK/AFZM interpolation on PAT layers?),
  not S8. Compare `before_gchaos_p100_f10_arcdrive.png` with `control_newbuild_oldroster_gchaos_p100_f10.png`.

## Open (optional)
- M11 / F7: a "CC-tuned" mode for IF 26 p3, IF 13 p4 and ATHS. Reuse the `--gg-from-cc` loader.
- GAKIHA has no 17/23-26, so it gets no M7 reaction patterns. Decide what a hit on the boss should do in CC.
- Spawn leaks into CC-only slots (akaakiha 130, akiha 129/130, hisui 206, shiki 175-179) are not handled.
- `[Type_N] ExShieldTime_*` is printed in the convert log, not written to a moon `_type.txt`.
- No push or merge has been done. The branch is ready for review.
- Next tasks (fresh agent, per the coordinator): GAKIHA reactions from CC G_AKIHA, rewriting the spawn-leak slots as no-ops, and an ExShieldTime sidecar.
