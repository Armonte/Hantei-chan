# S8 evidence (ActCadenza HA4 -> HA6 converter)

Shots come from the app's own `--ui-script` / `--capture` paths (scripts in `tools/ui/s8/`); handoff: `../../handoff/S8_HANDOFF.md`.

- `before_*`: old roster (`gamedata/mbac_to_cc/roster`), pre-S1 exe `builds/gonptechan_2026-10-04.exe`.
- `after_*`: roster_v2 with the S8 build. `control_newbuild_oldroster_gchaos_p100_f10.png`: S8 build with the old roster.
- `sheet_*.png`: before | after side by side (`tools/ui/s8/sheet.py`). The SION sheet uses `after_sion_p65_f0_shield_if51.png`.
- Zoom is now matched: the `before_*` shots were retaken at view zoom 1 (`tools/ui/s8/shoot_before_zoom1.sh`, `--zoom 1 --capture`), the same as the `zoomview 1` after/control shots. The first `before_*` set was at the default zoom 3.0. `after_sion_p65_f0_shield.png` is still at zoom 3 (its script has no `zoomview`). The old exe has no `framenodes`, so the right pane's AT/EF/IF nodes are collapsed in `before_*`.
- At matched zoom the G_CHAOS p100 f10 Arc Drive parts draw at the same size in both builds (see `sheet_gchaos_p100_f10_arcdrive.png`), so the ~1.6x difference noted in S8_HANDOFF.md came from the zoom mismatch.
