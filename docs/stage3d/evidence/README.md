# Stage 3D evidence (first slice, 2026-10-15)

| File | What |
|---|---|
| `uni2_bg001_sheet.png` | game \| Hantei-chan \| abs diff x2, round-start camera and camera x = 700 px |
| `uni2_bg001_game.png`, `_side.png` | UNI2 1.42 backbuffer, fighters hidden (UNI2-IM `hidefighters on`), HUD opacity 0, via `tools/gamecap/run.sh cases/stage3d_uni2_bg001.gcs` (per-process, under game_session.lock) |
| `uni2_bg001_game*.json`, `uni2_bg001_game.txt` | camera object dump and notes (camera x 0 / 89600 fixed, zoom 1, view translation (0, -0.7778, 2.4142)) |
| `uni2_bg001_hantei.png`, `_side.png` | `stage3d capture` of the 1280x720 stage target (`tools/stage3d/bg001_shot.txt`, run with `tools/stage3d/shot.sh`, window off-screen) |

Result: mean |difference| 3.4/255 (centre) and 3.2/255 (side). Geometry, camera, parallax, fog and the 0.10 + 0.6708 composite
match. Residuals: bloom / lens-flare glow (BGBloomEnable = 1 on bg001; not implemented yet), sky cloud animation phase,
MSAA x4 edges. Two fixes found by the comparison: texture V is stored bottom-up (v' = 1 - v), and the node Z mirror is required.

Rebuild the sheet: `python3 tools/stage3d/sheet.py docs/stage3d/evidence/uni2_bg001_sheet.png "label" game.png hantei.png ...`
