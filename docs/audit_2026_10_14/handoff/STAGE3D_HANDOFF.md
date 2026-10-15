# STAGE3D handoff (feat/stage3d, worktree wt/stage3d, from update/parity-1014)

## State

Committed on `feat/stage3d`. Nothing is pushed or merged.

### Code

| Path | What it does |
|---|---|
| `src/stage3d/s3d_model.*` | fbxex reader and anime evaluation |
| `src/stage3d/s3d_dds.*` | DXT1/3/5 and RGB(A) decoder |
| `src/stage3d/s3d_bglist.*` | Squirrel BgList parser, plus `StageParams` with the game defaults |
| `src/stage3d/s3d_view.*` | dockable "3D Stage" window and the `stage3d` ui-script commands |

- The view renders off-screen at the stage's TargetW x TargetH (1280x720), then runs a composite grade pass.
- The camera is the UNI2 1.42 camera taken from IDA (`SCOPE.md` §3.2).
- Shading is DefaultShader_Easy with fog.
- Open it from Stage → "3D Stage view (UNI2 / UNI / MBTL)...".
- It is wired in through `src/ui/main_menu_impl.h`, `src/ui/main_ui_impl.h` and `src/startup_args.cpp`.

### Tests

- `ctest stage3d` (`tests/stage3d_test.cpp`) covers:
  - synthetic fbxex, including truncation checks;
  - DDS decoding;
  - BgList parsing;
  - all 29 shipped UNI2 stages and their 654 textures from `gamedata/uni2/bg`.
- The full ctest run passes: 41/41.

### Tools

| Path | What it does |
|---|---|
| `tools/stage3d/shot.sh` | wraps `ab_shot.sh` and puts the window at posX -2000, off-screen and never activated |
| `tools/stage3d/bg001_shot.txt` | ui-script that takes the bg001 shots |
| `tools/stage3d/cull_test.txt` | ui-script for the cull test shots |
| `tools/stage3d/sheet.py` | builds the side-by-side sheet |

### Docs

- `docs/stage3d/SCOPE.md`: formats, pc-stage3d compared with Zanaylo's tools, the IDA render model, MBAACC alignment, and the plan.
- `docs/stage3d/DFCI_ISSUES.md`.
- `docs/stage3d/evidence/` (see its README).

## Result

- UNI2 bg001 compared with the game capture: mean |diff| is 3.4/255 at round start and 3.2/255 at camera x 700.
- Residual differences come from bloom, the sky animation phase and MSAA.
- The game capture used gamecap case `tools/gamecap/cases/stage3d_uni2_bg001.gcs`, which lives in the main tree, not the worktree.

## IDA

- UNI2 1.42 IDB: about 60 functions and globals renamed (`Stage3D_*`, `BtlCamera_*`, `BgList_*`, `StageBloom_*`, `FbxMesh_*`, `CharaShadow_*`). Saved.
- DFCI `RingGame.exe.i64` (SDCS folder): 25 renames. Saved.
- MBTL has not been examined.

## Open decisions for the user

1. **Licence.** UNI2-IM is GPL-3.0, and MBTL-IM's stage files are copies of it. Hantei-chan currently has no licence. This slice copies no Zanaylo code. The recommendation is to keep reimplementing (`SCOPE.md` §2.4), and to port DFCI FBX support from PovertyCaster's own `FbxAscii.cpp`.
2. **pc-stage3d camera fix.** pc-stage3d's camera misreads FOV, puts the camera 10 units away and ignores VanishingPoint. That is the root cause of "DFCI looks wrong" and probably affects the UNI2 ports as well. The fix belongs to the PovertyCaster side; pass them `SCOPE.md` §3.2 and `DFCI_ISSUES.md`.

## Next steps

These follow `SCOPE.md` §6.

| Phase | Work |
|---|---|
| P1.1 | Trace the cull field; implement the Light and NT techniques. |
| P1.2 | Bloom and TinyFXAA. |
| P1.3 | Interactive camera (pan, zoom, StageW clamp), the object.txt 2D layer. |
| P2 | DFCI FBX, UNIB archive, UNIEL RC4. |
| P3 | MBAACC alignment mode and stage3d.ini export. |

## Notes

- The game capture shows that the stored texture V is bottom-up, so the view flips it (`flipV`).
- The live projection has zn 0.2, while the decompile says 0.15. It makes no visible difference.
- The loose `<UNI2>/bg/bg001` folder has the same model as the `d\` archive.
