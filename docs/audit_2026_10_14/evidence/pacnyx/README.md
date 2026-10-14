# Evidence: PACNyx+ / icaroffa parity (stream S6), build c660f62+ (feat/pacnyx-parity after merging update/parity-1014)

| File | Result |
|---|---|
| build.log | `OMP_NUM_THREADS=4 ./build.sh` (mingw), EXIT 0 |
| han2_roundtrip.log | `tools/han2/run_roundtrip.sh` (copy of build/han2tool.exe): every SECTION fail 0; effectedit 436/0, dt2comp 95/0, pacwrite 18/0; suite exit 0 |
| fb_suite.log | `tools/fb/run_fb_suite.sh`: suite exit 0 |
| cgm_suite.log | `tools/cg/run_cgm_suite.sh`: cgm-bank 72/0, roundtrip 72 banks, refs pass, cgio 1/0, palette 44/0, no FAIL line; suite exit 0 |
| unit_tests.log | han2_parts_edit_test 71/0, undo_manager_test PASS, fb_part_transform_test 19/0, part_render_math_test 60/0. (A first run under load average ~40 failed only undo_manager_test's 16 ms timing bound, 56 ms/step; rerun at lower load: 11 ms, pass.) |
| ctest.log | full ctest after the S2/S3/S4+S5 merges: 36/36 passed |
| uni_regress.log | `tools/uni/uni_regress.sh` after the merges: HA6 262 bytes rc0 (--fresh: chr016 the expected rc5), PAT 180/180 bytes and --fresh, MBAACC .pat 19/0, PAL 272/0, stacks bad=0, regress exit 0 |
| save_check.log | the app's Save As (.DT2 + companion .DAT) read back: 6 sheets, pose 3 has 15 parts (was 14), pose 444 `MY_NEW_POSE` |
| export_icaroffa/ | sample of `han2tool export ... --icaroffa`; `EXPORT_INFO.txt` = result line + file counts of the full tree |
| shots/*.png, shots/*.log | app captures via `tools/ui/ab_shot.sh tools/ui/shots/{pn01,pn10,pn20,pn22}*.txt` (`--ui-script`, in-app GL frame read, no global input, no desktop capture), palette-quantized to 1400 px wide with `tools/ui/shrink_shot.py` |

Known gap: `pn02_sheet_selection.png` shows the imported 256x256 sheet but not the red selection rectangle (the view is not panned onto the
sheet), so the sheet-canvas selection row stays [~]. In-game check: pending, manual steps in `docs/formats/pacnyx_parity.md`.

Parity (docs/formats/pacnyx_parity.md, 47 graded rows): [x] 37, [~] 10, [ ] 0 -> strict 78.7 %, with partials at 0.5: 89.4 %.
