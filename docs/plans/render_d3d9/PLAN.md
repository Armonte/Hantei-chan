# Hantei-chan renderer migration: SDL3 + D3D9 (execution plan for an autonomous agent)

Written 2026-10-18 for a Codex agent (DeepSeek) running unattended. Read this whole file before doing anything. Re-read the phase you are in before each work session. It is the single source of truth. `docs/plans/RENDERER_GODLIKE.md` is the short rationale.

---------------------------------------------------------------------------------------------------------------------------

## 0. Mission

Move Hantei-chan from **Win32 windowing + OpenGL** to **SDL3 windowing + a renderer interface (RHI) with a D3D9Ex backend**. Then draw characters, .pats and stages with **the games' own HLSL effect files**, loaded from the user's game install. Finally build **natively on Linux** with SDL3 + DXVK-native (D3D9 on Vulkan). Then delete the OpenGL backend.

Why D3D9: every game Hantei-chan edits renders with D3D9 + D3DX9 (checked from the exes' imports):

| Game | Exe | D3DX9 |
|---|---|---|
| MBAACC | `MBAA.exe` | `d3dx9_36` |
| UNI2 | `uni2.exe` | `d3dx9_42` |
| MBTL | `MBTL.exe` | `d3dx9_42` |
| DFCI | `RingGame.exe` | `d3dx9_42` |

UNI2, MBTL and DFCI ship their shaders as HLSL effect source in `Shader/*.txt` and compile them at load with `D3DXCreateEffect` (`...FromFileA` in DFCI). UNI2 and MBTL have the same 25 files (5,353 lines in UNI2), vs_3_0/ps_3_0. Drawing with the same API and the same shaders makes the editor match the game by construction.

**The method, in one sentence:** freeze today's renderer output as a golden image corpus. Change the code in small mechanical steps. After every step, re-render the corpus and diff it against the golden. Only move on when the diff is zero, or when every difference is explained and listed in an allowlist with evidence.

### Definition of done (all must be true)
1. `gonptechan.exe` (Windows, mingw build) runs on SDL3 + D3D9Ex by default. No `gl*` call and no `<glad/glad.h>` include remains anywhere in `src/`. GL is deleted in P6.
2. **Parity:** the full golden corpus (§4) renders through D3D9 with **0 failing cases**. Every non-identical case is in `tools/render_parity/allowlist.tsv`, with the reason class, the measured diff and evidence (§4.6).
3. **Game effects:** "Use game shaders" (on by default when a game install is found) draws UNI2/MBTL/DFCI characters, .pats and stages through the game's `Shader/*.txt` effects. The parity harness checks the game-shader path against the existing game captures (§8.6), and it is no worse than the ported path on any case.
4. **Linux:** a native Linux build (`build_linux.sh`, gcc, SDL3, DXVK-native) opens characters, renders the corpus headless on lavapipe, and matches the Windows D3D9 golden within the allowlist.
5. ctest passes fully on Windows (`./build.sh`, then ctest in `build/`), including the new `render_parity` test. ctest passes on Linux too.
6. Every ui-script shot set in `tools/ui/shots*/` runs to completion on the new build, and every one of its captures is in the corpus diff.
7. One draft PR `render/d3d9 → main` with a complete description, `docs/plans/render_d3d9/STATUS.md` fully ticked, and nothing merged by you.

---------------------------------------------------------------------------------------------------------------------------

## 1. Hard rules (breaking any of these is a failure, whatever else works)

**Git**
- Never put AI or tool attribution in commits or PR text: no "Co-Authored-By", no "Generated with". Messages describe the change only.
- Never `git commit -a` or `git add -A` / `git add .`. Add the exact files you changed.
- Never `git stash`. To set work aside, make a WIP commit (`git commit -m "WIP: <what>"`).
- Never merge into `main`, never force-push `main`, never delete branches you didn't create, and never edit shared git config. To init submodules, use `git -c protocol.file.allow=always submodule update --init`.
- WIP-commit at least every 20 minutes of work and before every build. Push the branch after every phase gate and at least every 2 hours.

**Builds and code**
- Windows build: `./build.sh` (MinGW cross-compile, ccache). Never `rm -rf build` unless a CMake cache is corrupt, and then write down why. Never use a Linux native build for the Windows app.
- Logging: never `printf`/`std::cout`/`fprintf` in shipped code. Use quill through `src/app_log.h` (`HC_LOG_INFO/WARN/ERROR`). Existing debug printfs in code you touch get converted.
- Code style: match the surrounding code (tabs, brace style, comment density, naming). No new dependencies beyond the ones this plan names.

**Files and data**
- Never write anything of lasting value to `/tmp` or a session scratch dir. Golden images, diffs, logs and scripts go to the persistent paths in §2.
- Never commit game data, game shaders or images rendered from game assets. They are copyrighted. Only manifests, hashes, tools, reports and allowlists are committed.
- Never ship (commit, bundle or embed) the games' `Shader/*.txt`. Read them at runtime from the user's install.

**Machine and safety**
- Never launch a game (`uni2.exe`, `MBTL.exe`, `MBAA.exe`, `RingGame.exe`), never use `tools/gamecap`, and never boot-loop anything.
- Never send global mouse/keyboard input and never take full-desktop screenshots. All UI testing goes through `--ui-script`, `--batch` and `--headless` (in-app capture).
- `pgrep -f`/`pkill -f`: never with a pattern that matches your own shell.
- IDA (only P3): for every function or global you understand, rename it to a behaviour name and comment it. Save the IDB after each session. Never leave `sub_`/`dword_` names on anything you touched.

**Process**
- **Never weaken a test to make it pass.** Don't raise a threshold, delete a case, regenerate the golden from the new renderer, or add an allowlist entry without evidence. If a gate can't pass after 3 honest attempts, write a `BLOCKED` entry (§10) and move to the next independent item.
- No sudo here (it needs a password). If a system package is missing, vendor it (submodule or a release tarball under `deps/`), or write it into the mailbox (§10) and work around it.

---------------------------------------------------------------------------------------------------------------------------

## 2. Environment

| What | Where |
|---|---|
| Your worktree (ext4, fast builds) | `/home/teo/dev/hantei-chan-wt/render-d3d9`, branch `render/d3d9` (created from `origin/main` by the coordinator) |
| This plan | `docs/plans/render_d3d9/PLAN.md` (in your branch) |
| Status, ticked as you go | `docs/plans/render_d3d9/STATUS.md` (you create and maintain it, §10) |
| Mailbox to the coordinator (Claude) | `/home/teo/dev/orders/ds_render.md`. Append-only, newest entry at the top |
| Golden corpus, outside git | `/mnt/c/dev/hantei-chan/render_parity/golden/<baseline-sha>/` |
| New-renderer runs, outside git | `/mnt/c/dev/hantei-chan/render_parity/runs/<run-id>/` (`run-id` = `<date>_<shortsha>_<backend>`) |
| Parity tools, in git | `tools/render_parity/` |
| Game data | `/mnt/c/dev/hantei-chan/gamedata/{uni2,mbtl,mbac,mbaacc_ref}`, `/mnt/c/games/mbaacc`, `/mnt/c/games/unib`, `/mnt/c/games/{rbo_run,gof1/run,gof2_run}`, DFCI `/mnt/c/dev/transmission/dfci+transmission` |
| Game shaders (read-only) | `/mnt/c/dev/hantei-chan/gamedata/{uni2,mbtl}/Shader/*.txt`, `/mnt/c/dev/transmission/dfci+transmission/Shader/` |
| Existing game captures (read-only) | `/mnt/c/dev/hantei-chan/tools/gamecap/captures/{s1_compare,v2}/`, and the index `docs/audit_2026_10_14/evidence/compare/{MISMATCHES.md,v2/INDEX.md}` |
| IDA databases (P3 only, via the IDA MCP) | `/mnt/c/dev/ida/server/{uni2.exe.i64,MBTL_25646393.exe.i64,MBAA.exe.i64}` and `RingGame.exe.i64.old-ai-names.i64` |

**Running Windows exes from WSL.** `build/*.exe` run directly (WSL interop). Paths given to the exe must be Windows paths: use `wslpath -w`. The exe's working directory matters for `res/` and the ini. Use a scratch folder per run under `/mnt/c/dev/hantei-chan/work/render_parity/` (persistent), and copy the exe plus `res/` there the way `tools/ui/ab_shot.sh` does.

**Tools available:** cmake, ninja, gcc/g++ 13, x86_64-w64-mingw32 (with `d3d9.h` and `d3dcompiler.h`), python3 with numpy 2.4 and Pillow, bison, flex, pkg-config, Vulkan loader, mesa lavapipe (`/usr/share/vulkan/icd.d/lvp_icd.json`). Not installed: meson, SDL3, DXVK, vkd3d. Vendor those (§9).

**How long things take:**
- Incremental `./build.sh`: about 1–3 minutes.
- Full ctest: about 3 minutes (`cd build && ctest -j8 --timeout 300`).
- Headless batch: about 0.2–1 s per shot.

---------------------------------------------------------------------------------------------------------------------------

## 3. The current renderer: exact inventory (what you are replacing)

Measured at `origin/main` 45ea8ae: **673 GL call sites in 27 files**.

### 3.1 Core (moves behind the RHI in P1)

| File | Lines | GL calls | Role | What it uses |
|---|---|---|---|---|
| `src/render.h` / `src/render.cpp` | 280 / 1365 | 95 | `class Render`: sprites, layers, boxes, grid, PAT drawing, sprite texture LRU cache. Inline GLSL: `simpleSrc*`, `texturedSrc*` (palette lookup modes 0–3, including the MBTL `CharaDrawBgColorBaseFilterBokashi` port), `partsSrc*` (diffuse * tex + PRSP add) | blend add/sub (`GL_FUNC_REVERSE_SUBTRACT`), depth mask, 256×1 palette texture on unit 1, `glVertexAttrib4fv` constant colour |
| `src/texture.h/.cpp` | 32 / 189 | 26 | `Texture` (id = GL name): RGBA, 8-bit `GL_R8` indexed, DXT via `glCompressedTexImage2D` | |
| `src/vao.h/.cpp` | 57 / 169 | 35 | `Vao` (VBO only; types F2F2, F3F3, F3F4); `Draw`, `DrawQuads` (`glMultiDrawArrays`), `glMapBuffer` | |
| `src/shader.h/.cpp` | 22 / 137 | 18 | `Shader` (program, `GetLoc`, attribs) | |
| `src/render_target.h/.cpp` | 88 / 157 | 65 | `RenderTarget` (FBO + RGBA8 colour + depth/stencil RB), `ScopedTargetBinding` (saves and restores GL state), `readRgba`, `blitToDefault`, headless default FBO | |
| `src/context_gl.h` | 59 | wgl | `ContextGl`: `wglCreateContext` on the main HWND | |
| `src/parts/parts.cpp` | 1154 | 21 | PAT parts draw: blend per part (add `GL_ONE`, subtract `REVERSE_SUBTRACT`, normal), depth off | |
| `src/parts/parts_texture.cpp` | | 3 | part textures | |
| `src/src/{simple,textured}.{vert,frag}` | | | **Dead copies** (GLSL 120). The live shaders are inline in `render.cpp`. Delete them in P1 after a grep confirms nothing loads them | |

### 3.2 Stage and 3D (moves in P4)

| File | GL calls | Role |
|---|---|---|
| `src/background/bg_renderer.{h,cpp}` (210 / 1510 lines) | 269 | Stage backgrounds: its own programs (inline GLSL 330 at lines ~43, 58, 935, 942, 1055, 1061, 1082), FBOs, `glCopyTexSubImage2D`, `glReadPixels`, DXT |
| `src/stage3d/s3d_view.cpp` (832 lines) | 102 | 3D stage view: VAOs, `glDrawElements`, cull and depth, a grade pass (`kVs`/`kFs`/`kGradeVs`/`kGradeFs`), shows `rt.texture()` in ImGui with flipped UVs |
| `src/bg_render.cpp` | 15 | bg offscreen render plus readback (FBO) |

### 3.3 Tool-window thumbnails: "upload RGBA → GL texture → ImGui::Image" (moves in P1)

Each of these hand-rolls `glGenTextures`/`glTexImage2D`/`glTexParameteri`/`glDeleteTextures`, then casts the GL name to `ImTextureID`:

| File | GL calls |
|---|---|
| `src/archive_browser_ui.cpp` | 9 |
| `src/background/bg_browser.cpp` | 8 |
| `src/background/stage_card_ui.cpp` | 6 |
| `src/cgm/cgm_window.cpp` | 8 (`Window::upload`, `Thumb`) |
| `src/cgm/fxr_panel.cpp` | 7 |
| `src/han2_inspector.cpp` | 5 |
| `src/han2_pac_window.cpp` | 6 |
| `src/han2_thumbs.cpp` | 7 |
| `src/hud_textures.cpp` | 8 |
| `src/authoring/game_view.cpp` | 14 (also `glBlendFuncSeparate`, `glTexSubImage2D` streaming) |

**27 `ImTextureID` sites in 13 files:**
- `han2_thumbs`, `archive_browser_ui`, `han2_inspector`, `han2_pac_window`, `cgm/fxr_panel`, `authoring/game_view`, `cgm/cgm_window`, `ui/workspace_hosts_impl.h`, `ui/tool_windows_impl.h`, `background/stage_card_ui`, `stage3d/s3d_view`, `authoring/hud_layout_ui`, `background/bg_browser`.
- Render-target textures are shown **with flipped UVs** `(0,1)-(1,0)`, because GL targets are bottom-up. Sites: `ui/workspace_hosts_impl.h:465`, `stage3d/s3d_view.cpp:680,706`, `ui/main_frame_gamelink.cpp:154`.

### 3.4 Small or stray GL

| File | What it does |
|---|---|
| `src/main_frame.cpp` | `glBindFramebuffer`, `glReadBuffer`; ImGui GL3 NewFrame/Render; `SwapBuffers` |
| `src/startup_args.cpp` | `--capture` readback: `glReadPixels` |
| `src/workspace_viewports.cpp` | detached-window readback, `SwapBuffers` |
| `src/ui/main_frame_gamelink.cpp` | clear |
| `src/ui/main_ui_impl.h` | viewport |
| `src/ui/view_render_impl.h` | clear, viewport |
| `src/game_frame_share.h` | `glBlendFunc` |
| `src/render.cpp` `Render::Draw`, `DrawGridLines`, `DrawLayers` | `glGetError` reporting. #102 turns this into `ReportPendingGlErrors`, logged and not modal |

### 3.5 Win32 platform code (moves to SDL3 in P0)

14 files use the Win32 API:
- `main.cpp`: `WinMain`, `WNDCLASSEX`, `CreateWindowEx` (with `--headless` as `WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE`), the `PeekMessage` loop with 60 fps pacing, `WndProc` → `ImGui_ImplWin32_WndProcHandler`, `gladLoadGL`, `ImGui_ImplWin32_InitForOpenGL`, `ImGui_ImplOpenGL3_Init("#version 330 core")`, `wglSwapIntervalEXT`, `DragAcceptFiles`, `GetCurrentDirectoryA` for the ini, `-r W H`.
- `main.h`, `context_gl.h`.
- `workspace_viewports.{h,cpp}`: a hand-written ImGui multi-viewport bridge for raw Win32 + one shared GL context, a per-HWND subclass for key routing, and `PrintWindow`-style capture.
- `filedialog.cpp`: `IFileDialog`/`GetOpenFileName`.
- `startup_args.{h,cpp}`: `--post-key` posts `WM_KEYDOWN`.
- `ui/workspace_hosts_impl.h`, `bg_render.cpp`, `background/bg_gametex.cpp`, `han2_pac_window.cpp`.

### 3.6 ImGui build (`cmake/imgui.cmake`)

`imgui_impl_opengl3.cpp` + `imgui_impl_win32.cpp`, compiled with `IMGUI_IMPL_OPENGL_LOADER_GLAD IMGUI_IMPL_OPENGL_ES2`, linking `glad imm32 dwmapi`. ES2 mode means no `glDrawElementsBaseVertex`, so ImGui can't use vertex offsets. PR #99 added 32-bit `ImDrawIdx` as a workaround. The imgui submodule is pinned at the 1.92.x docking tag the repo uses. Don't bump it unless a phase needs it, and if you do, record why.

### 3.7 Headless and capture paths (the parity harness depends on these)
- `--headless`: hidden window, the views render into their FBO, and `MainFrame::Draw` sends the frame to an offscreen default target (`RenderTarget::setDefaultFramebuffer`).
- `--batch <file>`: one shot per line, settings carry over. Commands: `open X; pattern N; frame N; tick N; zoom Z; center X,Y; bg R,G,B; size WxH; layer L; boxes 0|1; spawns 0|1; export out.png`. The exit code is 0 only when every PNG was written.
- `--capture-view`/`--export-png` read the view's own target. `--capture` reads the window. `--ui-script` runs `capture <png>` commands.

The world view emulates D3D9 rasterisation (see the comment at `render.cpp:549`: pixel centres at integer coordinates, and the origin snapped to `zoom * int(origin / zoom)`). In D3D9 this emulation becomes native. Treat each place GL emulates D3D9 as a parity hotspot (§4.6, class `EMU`).

---------------------------------------------------------------------------------------------------------------------------

## 4. Parity harness (Phase H: build it first; nothing else starts until it's green)

### 4.1 Freeze the baseline
1. `git rev-parse origin/main` at the moment you start becomes `BASELINE`. Build it in a **separate worktree** `/home/teo/dev/hantei-chan-wt/render-baseline` (detached at `BASELINE`, `./build.sh`).
2. Copy `build/gonptechan.exe` plus `res/` to `/mnt/c/dev/hantei-chan/render_parity/baseline_bin/<BASELINE>/`. Never rebuild or overwrite it. Every golden comes from this binary.
3. Write `tools/render_parity/BASELINE` containing the sha and the exe's sha256.

### 4.2 The corpus: what is rendered
`tools/render_parity/corpus/*.batch` (committed): `--batch` files, one shot per line, grouped by area. `tools/render_parity/corpus/ui/*.txt` holds ui-script files for UI shots. Write a generator, `tools/render_parity/make_corpus.py`, that picks the cases from the data, so coverage is systematic rather than hand-picked.

**A. Character frames** (`corpus/chars_<game>.batch`). For each game:

| Game | Data | Characters |
|---|---|---|
| MBAACC | `/mnt/c/games/mbaacc` | 4 |
| ActCadenza | `gamedata/mbac` | 2 |
| UNIB | `/mnt/c/games/unib` | 2 |
| UNI2 | `gamedata/uni2/data/chrNNN/chrNNN_0.txt` | 6, including chr000, chr016, chr019, chr020, chr024, chr025, chr027 (the ones in MISMATCHES.md) |
| MBTL | `gamedata/mbtl` | 4 |
| DFCI | DFCI `data/` | 3 |
| RBO / GOF1 / GOF2 | `C:\games\...` | 1 each |

Use `ha6_dump.exe` (built by `build.sh`, usage `ha6_dump <file.ha6> <pattern...>`) or `tools/uni/ha6walk.py` to find frames with each **feature**. Take at least 3 frames per feature per game where the game has it:
- the plain idle frame (pattern 0, frame 0);
- blend modes 1 (add), 2 (subtract), 3 (blur mask);
- AF flip H, V, and both;
- AFAZ/AFAX/AFAY rotation and AFZM scale;
- AFHK interpolation mid-frame: tick inside the frame;
- AFAL alpha < 255, and colour tint;
- multi-layer AFGX (UNI2: 5 layers, MBTL/UNIB: 3);
- AFPL buckets 1–4;
- PAT layers (usePat) with each PRAL mode, PRRV flip, PRSP specular, PRAS pivot, PPPA tint;
- PUPS ≠ 0 (palette bank);
- palette numbers 0, 1 and the last;
- spawned patterns (`spawns 1` vs `spawns 0`);
- MBTL CG at zoom > 1 (the `indexed == 3` filter path);
- the cases from `MISMATCHES.md` and `v2/INDEX.md` by name.

**Variants per frame:**
- zoom 1, 2 and 0.5;
- `boxes 1` and `boxes 0`;
- backgrounds `bg 0,0,0` and `bg 255,255,255` (black and white backdrops expose blending);
- one shot with `size 1920x1080`, one with `size 333x257` (odd sizes catch half-pixel and readback-stride bugs).

Target **about 1,500–2,500 shots** in total. Name each shot `<game>_<chr>_p<pat>_f<frame>_t<tick>_<feature>_z<zoom>_<bg>_<boxes>.png`.

**B. Onion skin and compare overlay** (`corpus/overlay.batch`): `--onion 2,2,4` on 5 cases, and `--compare N` on 5 cases.

**C. Stages** (`corpus/stages.batch` plus `corpus/ui/stage*.txt`):
- Every UNI2 and MBTL stage in `bg/BgList.txt`: 3 camera positions each (centre, far left, far right), zoom 1.
- DFCI and UNIB stages: 5 each. MBAACC: `stage3d.ini` alignment mode, 3 stages.
- 3D stage view (`s3d_view`): 3 stages, through ui-script `capture`.

**D. UI** (`corpus/ui/*.txt`): every script in `tools/ui/shots/`, `tools/ui/shots_cgm/`, `tools/ui/shots_layout/` and `tools/ui/s8/`, re-pointed at the run folder. These cover the archive browser, CG manager thumbnails and preview, PAT editor, HUD textures, han2 windows, stage card, BG browser, authoring game view, and detached windows (`--detach` + `--capture-windows`).

**E. PNG export** (`corpus/export.batch`): `--export-png <dir>` with `--export-scale 2`, `--export-bg transparent|editor`, `--export-boxes`, `--export-nospawns`, `--export-crop each`, on 5 patterns. Compare every exported file.

### 4.3 Determinism: run first
Render the whole corpus **twice** with the baseline binary into `golden/<BASELINE>/` and `golden/<BASELINE>_repeat/`. Diff the two runs.
- Any case that differs between two baseline runs is **flaky**. List it in `tools/render_parity/flaky.tsv` with the cause (animation timers, LRU eviction order, uninitialised memory, time-based pulses).
- Then **fix the cause in the app**, on your branch, before any port work: for example, a `--deterministic` flag that pins wall-clock-driven effects, used by every harness run. Rebuild the baseline binary from `BASELINE` + only that determinism commit, and record both shas in `BASELINE`.
- **Gate H1:** two baseline runs are byte-identical on 100% of the corpus.

### 4.4 Tools to write (`tools/render_parity/`, committed)

| File | What it does |
|---|---|
| `make_corpus.py` | Scans the game data, picks the cases in §4.2 and writes `corpus/*.batch` deterministically (sorted, seeded). |
| `run_corpus.sh <exe-dir> <out-dir> [--renderer gl\|d3d9] [--only <glob>]` | Copies the exe and `res/` to a scratch folder on C:, runs each `.batch` with `--headless --batch` and each ui-script with `--headless --ui-script` (one process per file; resume where it stopped; timeout per file). Writes `<out-dir>/manifest.tsv` (`case`, `png`, `sha256`, `w`, `h`, `exit`). |
| `diff.py <golden-dir> <run-dir> [--allow allowlist.tsv] [--report out.md] [--heat <dir>]` | Per case: identical (sha equal), or a numpy diff giving max |d|, mean |d| over pixels with any difference, count of differing pixels, % of image, and the bounding box of differences. Writes a heatmap PNG for each non-identical case. Classifies each case as `PASS` (identical), `ALLOWED` (matches an allowlist entry within its stated bounds) or `FAIL`. Exit code 1 on any FAIL. Also fails when a case is missing on either side. |
| `report.md` (generated per run) | Counts per class and per area, the 20 worst FAILs with heatmap links, and the allowlist entries that were used. |
| `ctest` entry `render_parity` (label `gpu`) | Runs a small (about 60-case) smoke subset against `golden_smoke/` for quick local checks. The full corpus is the phase gate, run by hand. |

### 4.5 Pass criteria
- **PASS:** byte-identical PNG. This is the default expectation for P0 and P1.
- **ALLOWED:** only with an allowlist entry (§4.6). Its bounds are hard limits: `max_abs ≤ N`, `pixels ≤ M`, and optionally `region = edges` (all differing pixels lie on alpha or geometry edges of the golden). Any value over a bound is a FAIL.
- **FAIL:** everything else.

### 4.6 Allowlist (`tools/render_parity/allowlist.tsv`, committed)
Columns: `case_glob`, `backend`, `class`, `max_abs`, `max_pixels`, `region`, `evidence`, `note`.

| Class | Meaning | Evidence required |
|---|---|---|
| `EMU` | GL emulated a D3D9 rule imperfectly, and D3D9 is now native. | (a) The exact GL emulation code (file:line) and the D3D9 rule it imitates. (b) The case compared against a game capture from §2, where one exists, shows D3D9 **closer to the game** (residual numbers from `tools/uni/s1_compare` models). Without a game capture, a written argument from the D3D9 documentation of the rule, plus the coordinator's review flag `needs-review`. |
| `AA` | Edge antialiasing or rasterisation tie-breaks on ImGui UI geometry, which can't be bit-exact across APIs. | `max_abs ≤ 64`, differences only on UI edges (`region=edges`), `≤ 0.5%` of pixels. A heatmap committed under `tools/render_parity/evidence/`, scaled down, UI only, never game imagery. |
| `FLT` | Bilinear filtering precision (GL vs D3D9 fixed-point texture filtering). | `max_abs ≤ 2`, linear-filtered cases only. |
| `GAME` | P3/P4 game-shader path: the new path differs from the golden by design. | Compared against the game captures. It must be closer to the game than the golden (lower mean |d| on the sprite footprint using the existing models). |

Never use a glob that covers a whole area ("`uni2_*`"). Each entry names one feature group at most.

---------------------------------------------------------------------------------------------------------------------------

## 5. Target architecture

```
src/platform/            SDL3 window, events, DPI, clipboard, drag-drop, message box, file dialogs (Windows: keep IFileDialog)
  platform.h/.cpp        Platform::Init/Shutdown, PollEvents(), main loop pacing, headless window, AppWindow handle
  file_dialog.h/.cpp     the existing FileDialog()/BrowseForFolderUtf8() signatures, unchanged for callers
src/rhi/                 render hardware interface: the only place that knows the graphics API
  rhi.h                  Device, Texture, RenderTarget, Buffer, Program, PipelineState, SamplerState, enums
  rhi_imgui.h            ImTextureID ToImTex(const Texture&), UvRect for render targets (handles GL bottom-up vs D3D top-down)
  gl/                    GL backend (P1: exact behaviour of today's code; deleted in P6)
  d3d9/                  D3D9Ex backend (P2)
  d3d9/fx/               effect runtime for the games' Shader/*.txt (P3)
src/render.cpp, parts/, background/, stage3d/, tool windows: call rhi only
```

### 5.1 RHI interface (`src/rhi/rhi.h`). Keep it this small; grow it only when a call site needs it.

```cpp
namespace rhi {
enum class Format { RGBA8, R8, DXT1, DXT3, DXT5, Depth24S8 };
enum class Filter { Point, Linear };
enum class Address { Clamp, Wrap };
enum class BlendOp { Add, RevSubtract };
enum class BlendFactor { Zero, One, SrcAlpha, InvSrcAlpha, DstColor, ... /* only the ones used */ };
enum class Prim { Triangles, TriangleFan, Lines, LineLoop };   // LineLoop/TriangleFan: emulated in d3d9 by index lists

struct BlendState { bool enable; BlendOp op; BlendFactor src, dst; BlendOp opA; BlendFactor srcA, dstA; uint8_t writeMask; };
struct DepthState { bool test; bool write; };
struct SamplerState { Filter min, mag; Address u, v; };

struct TextureDesc { int w, h; Format fmt; bool renderTarget; };
class Texture;        // opaque; owns the API object
class RenderTarget;   // colour Texture + depth; read back as top-down RGBA8
class Buffer;         // dynamic vertex buffer (ring); index buffer for quads
class Program;        // vertex + pixel shader with a named constant table

class Device {
public:
  static std::unique_ptr<Device> Create(Backend, const platform::AppWindow&, bool headless);
  // textures
  std::unique_ptr<Texture> CreateTexture(const TextureDesc&, const void* pixels, int rowPitch);
  void UpdateTexture(Texture&, int x, int y, int w, int h, const void* pixels, int rowPitch);
  // targets
  std::unique_ptr<RenderTarget> CreateRenderTarget(int w, int h, bool linear);
  void BeginPass(RenderTarget* rt /*nullptr = backbuffer/headless default*/, const float clear[4], bool opaqueAlpha);
  void EndPass();
  bool ReadRgba(RenderTarget*, std::vector<uint8_t>& topDown);
  // drawing
  void SetProgram(Program&); void SetConstant(Program&, const char* name, const float* v, int count4);
  void SetTexture(int unit, const Texture*, SamplerState);
  void SetBlend(const BlendState&); void SetDepth(const DepthState&);
  void Draw(Prim, const void* vertices, int vertexCount, const VertexLayout&);   // dynamic upload
  void DrawQuads(Prim, const void* vertices, int quadCount, const VertexLayout&); // replaces Vao::DrawQuads/glMultiDrawArrays
  // frame
  void BeginFrame(); void EndFrame(); void Present();
  // imgui
  ImTextureID ImTex(const Texture&); UvRect ImUv(const RenderTarget&);   // UV flip lives here, not at call sites
};
}
```

Rules:
- **No GL types or GL enums outside `src/rhi/gl/`.** Lint (§6, P1 gate): `grep -rnE "\bgl[A-Z]|GL_[A-Z]|<glad/" src | grep -v "^src/rhi/gl/"` must print nothing.
- **State is explicit.** Every draw sets its blend, depth and sampler state, and nothing carries over. Today's code relies on leftover GL state (for example `render.cpp:445/504/1202` reset the blend after each draw). The RHI makes the state part of the draw call.
- **Render targets read back top-down** through `ReadRgba`. The UV flip for showing a target in ImGui comes only from `Device::ImUv`. Remove the hard-coded `(0,1),(1,0)` at the 4 sites in §3.3.
- **One shared vertex layout per shader**, matching today's attribute layouts:
  - simple = `F3F3` (pos3, colour3);
  - textured = `F2F2` (pos2, uv2) plus a constant vertex colour (today `glVertexAttrib4fv(2, …)`); in the RHI the colour becomes a shader constant;
  - parts = `F3F4` (pos3, uv4) plus colour.

### 5.2 Shader sources
Today: GLSL 330 inline in `render.cpp` (6 programs), `bg_renderer.cpp` (about 4 programs), `s3d_view.cpp` (2 programs).

**Target layout:**
- `res/shaders/<name>.glsl.vert/.frag` for the GL backend, moved out of the C++ strings in P1 with **identical text**.
- `res/shaders/<name>.hlsl` for D3D9: vs_3_0/ps_3_0, written in P2.

**Rules:**
- The HLSL port is line-for-line equivalent: the same maths and the same constant names.
- Compile at runtime with `D3DCompile`, loaded from `d3dcompiler_47.dll` via `LoadLibrary`. The mingw import library is fine, but load it dynamically so a missing DLL gives a clear error.
- Cache bytecode under `%LOCALAPPDATA%\hantei-chan\shadercache\<sha>.bin`.

**Hotspots to port carefully:**
- **`texturedSrcFrag`:**
  - `textureSize`/`texelFetch` (mode 3) become constants `g_texSize` plus point samples at `(t + 0.5) / size`.
  - `palLookup`'s `idx * (255/256) + 0.5/256`: keep it exactly.
  - Manual bilinear after the palette lookup (mode 2).
- **`partsSrcFrag`:** `col *= Frag_Color; col += addColor`.
- **Blend modes** (`render.cpp:957–966`, `parts.cpp:489–504`):

  | Mode | GL today | D3D9 |
  |---|---|---|
  | Normal | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD` | `D3DBLEND_SRCALPHA` / `D3DBLEND_INVSRCALPHA`, `D3DBLENDOP_ADD` |
  | Additive | `SRC_ALPHA, ONE, ADD` | `D3DBLEND_SRCALPHA` / `D3DBLEND_ONE`, `D3DBLENDOP_ADD` |
  | Subtractive | `SRC_ALPHA, ONE, REVERSE_SUBTRACT` | `D3DBLEND_SRCALPHA` / `D3DBLEND_ONE`, `D3DBLENDOP_REVSUBTRACT` |

### 5.3 D3D9 specifics you must get right (P2)
- **Device:** `Direct3DCreate9Ex` → `CreateDeviceEx`.
  - HWND from SDL3: `SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)`.
  - `D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED`, windowed, `D3DSWAPEFFECT_FLIPEX`.
  - Fall back to `D3DSWAPEFFECT_DISCARD` if FLIPEX fails.
  - Handle `D3DERR_DEVICELOST`/`S_PRESENT_OCCLUDED`, and reset on resize.
- **Half-pixel offset:** D3D9 maps pixel centres to integer coordinates. Today's GL code emulates this (`render.cpp:549` and the origin snapping). In D3D9, remove the emulation, and keep the maths that produces the same screen positions. For orthographic 2D passes, apply the standard `-0.5` texel/pixel offset in the projection (or the equivalent `D3DXMatrixTranslation(-0.5,-0.5)`) exactly once. **This is the #1 source of off-by-one diffs.** Verify on the 333×257 odd-size cases.
- **Render targets:** `CreateTexture(…, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT)` plus a `D3DFMT_D24S8` surface. Readback: `GetRenderTargetData` into a `D3DPOOL_SYSTEMMEM` surface, `LockRect`, swizzle BGRA → RGBA, rows top-down.
- **Formats:**

  | GL | D3D9 |
  |---|---|
  | `GL_R8` index textures | `D3DFMT_L8` (sampled as `.r` in HLSL via `tex2D(...).r`; verify L8 replicates to rgb) |
  | DXT | `D3DFMT_DXT1/3/5` |
  | RGBA | `D3DFMT_A8R8G8B8`, swizzling RGBA → BGRA on upload |

  The palette texture is 256×1 `A8R8G8B8`, point-sampled, clamped.
- **Alpha write-protect:** `ScopedTargetBinding::clear(..., opaque=true)` maps to `D3DRS_COLORWRITEENABLE` without `D3DCOLORWRITEENABLE_ALPHA`.
- **Line loops and triangle fans:** D3D9 has `D3DPT_TRIANGLEFAN` but no line loop. Emit a closed `D3DPT_LINESTRIP` (repeat the first vertex). `DrawQuads` becomes one `DrawPrimitiveUP`, or a dynamic VB per quad with the same order. The draw order must match the GL order exactly, because overlapping translucent boxes show the order.
- **sRGB:** off everywhere (`D3DSAMP_SRGBTEXTURE = FALSE`, `D3DRS_SRGBWRITEENABLE = FALSE`), the same as today's GL.
- **ImGui:** `imgui_impl_dx9.cpp` plus `imgui_impl_sdl3.cpp`.
  - The DX9 backend supports multi-viewports, so `workspace_viewports.cpp` is deleted in P0/P2 and SDL3 + the backends handle detached windows.
  - Keep 32-bit indices (`ImDrawIdx = unsigned int`; dx9 backend: `D3DFMT_INDEX32`).
- **Headless:** SDL3 hidden window (`SDL_WINDOW_HIDDEN`), a device on it, and every pass into render targets. `MainFrame::Draw`'s headless default target becomes an RHI `RenderTarget`.

---------------------------------------------------------------------------------------------------------------------------

## 6. Phases: what to do, file by file

Each phase lists its **Steps**, **Test**, **Gate (success)**, **Failure handling** and **Deliverables**. Do the phases in order. Inside a phase, keep every commit building and passing ctest.

### Phase H: parity harness (§4)
- **Steps:**
  1. Create the baseline worktree and binary.
  2. Write `make_corpus.py`, `run_corpus.sh` and `diff.py`, plus unit tests for `diff.py` using synthetic PNGs (identical, 1-pixel diff, missing case, allowlist bound exceeded): `tools/render_parity/test_diff.py`, run by ctest as `render_parity_tools` (no GPU).
  3. Generate the corpus. Render twice. Fix flakiness (§4.3).
  4. Render the golden.
- **Gate H1:** the repeat run is identical on 100% of cases. **Gate H2:** `test_diff.py` passes, and `diff.py golden golden` gives 100% PASS.
- **Deliverables:** tools, corpus, `BASELINE`, `flaky.tsv` (fixed entries marked), `golden_smoke/` (the 60-case subset, outside git), and the STATUS entry.

### Phase P0: SDL3 platform, GL kept (renderer output must not change)
- **Steps:**
  1. **Vendor SDL3** as a submodule `deps/SDL` at the latest stable `release-3.x.y` tag at least 2 weeks old. Build it static with `add_subdirectory(deps/SDL)`, using `SDL_STATIC ON`, `SDL_SHARED OFF`, `SDL_TEST_LIBRARY OFF`. It must cross-compile with mingw (it does; check `SDL_DIRECTX`/`SDL_RENDER_D3D` defaults).
  2. **`src/platform/platform.{h,cpp}`:**
     - `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)`.
     - Main window: `SDL_CreateWindow(title, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (headless ? SDL_WINDOW_HIDDEN : 0))`.
     - GL context: `SDL_GL_CreateContext` with 3.3 core and attributes 24 depth / 8 stencil, as in `ContextGl`.
     - `SDL_GL_SetSwapInterval(1)` replaces `wglSwapIntervalEXT`.
     - `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)`.
  3. **`main.cpp`:**
     - Replace `WinMain`/`WNDCLASSEX`/`CreateWindowEx`/`PeekMessage`/`WndProc` with an SDL main loop.
     - Keep `wWinMain` argument parsing semantics: `CommandLineToArgvW` stays on Windows; on Linux, `argv`.
     - Keep the 60 fps pacing, and no pacing under `--headless`.
     - Map every `WndProc` message handled today to SDL events: `WM_SIZE` → `SDL_EVENT_WINDOW_RESIZED`; `WM_DROPFILES` → `SDL_EVENT_DROP_FILE`; `WM_CLOSE` → `SDL_EVENT_WINDOW_CLOSE_REQUESTED`; plus any `WM_KEYDOWN` shortcuts the app intercepts before ImGui (read `WndProc` fully and list them in STATUS first).
     - Keep the ini position/size/maximised behaviour.
  4. **ImGui:**
     - `cmake/imgui.cmake`: replace `imgui_impl_win32.cpp` with `imgui_impl_sdl3.cpp`, keep `imgui_impl_opengl3.cpp`, and **remove `IMGUI_IMPL_OPENGL_ES2`**, which enables `glDrawElementsBaseVertex`.
     - `ImGui_ImplSDL3_InitForOpenGL(window, glctx)`, `ImGui_ImplOpenGL3_Init("#version 330 core")`.
     - `main_frame.cpp:125-145`: NewFrame/Render via SDL3; `SDL_GL_SwapWindow` replaces `SwapBuffers`.
  5. **Detached windows:** enable ImGui multi-viewports through the SDL3 + GL3 backends (`ImGuiConfigFlags_ViewportsEnable`, `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`). **Delete `workspace_viewports.{h,cpp}`** and its key-routing subclass. Route shortcuts in detached windows by checking `SDL_GetKeyboardFocus()` against the viewport's `PlatformHandle` in the existing shortcut handler. `--capture-windows` reads each viewport's framebuffer after its render, via the renderer's per-viewport hook.
  6. **`--post-key <vk>`:** push an `SDL_EVENT_KEY_DOWN` with the mapped scancode into the queue (`SDL_PushEvent`). Keep the VK number argument and map VK to SDL scancode.
  7. **File dialogs:** keep `filedialog.cpp`'s Windows `IFileDialog` code under `#ifdef _WIN32`, with the same API. Add an SDL3 path for Linux (P5): `SDL_ShowOpenFileDialog`/`SDL_ShowSaveFileDialog`/`SDL_ShowOpenFolderDialog`, made synchronous by pumping `SDL_PumpEvents` until the callback fires. `MessageBox*` becomes `SDL_ShowSimpleMessageBox`, except in `--headless`, where it is logged.
  8. **Remove `context_gl.h`.** `MainFrame` gets the platform window.
- **Test:** full ctest; `run_corpus.sh` (renderer gl) over the whole corpus; `diff.py` against the golden. Also run every `tools/ui/shots*/` script visibly once with `AB_VISIBLE=1` and check that it finishes.
- **Gate P0:**
  - 100% PASS (byte-identical) on corpus areas A, B, C and E.
  - Area D (UI) 100% PASS, or `AA` allowlist entries only where ImGui backend batching changed (`imgui_impl_opengl3` without ES2 draws with base vertex). Each `AA` entry must be ≤ the bounds in §4.6.
  - Detached windows open, render, take shortcuts and capture.
  - Drag-drop opens files.
  - ctest green.
- **Failure:**
  - A non-UI diff means the window or target size changed (DPI, client-area size, a 1-pixel border) or the frame-count timing of a capture changed. Fix the cause; never the golden.
  - Common traps:
    - `SDL_WINDOW_HIGH_PIXEL_DENSITY` changes the framebuffer size vs the window size. Use `SDL_GetWindowSizeInPixels` wherever `clientRect` was used.
    - Headless `GetClientRect` semantics must give the same size.
- **Deliverables:** the commit series, STATUS, and the run report under `runs/`.

### Phase P1: RHI with the GL backend (output must not change)
- **Steps, in this order, each one a commit that keeps the corpus at 100% PASS:**
  1. Add `src/rhi/rhi.h` and `src/rhi/gl/rhi_gl.cpp`, implementing §5.1 with today's exact GL calls, moved from `texture.cpp`, `vao.cpp`, `shader.cpp` and `render_target.cpp`.
  2. Move shader text to `res/shaders/*.glsl.*` with identical text. Add `res/shaders` to the CI packaging and to `ab_shot.sh`'s copy step. Delete the dead `src/{simple,textured}.{vert,frag}` (grep first).
  3. **`texture.cpp`/`Texture`:** keep the class name for callers, but hold a `std::unique_ptr<rhi::Texture>`. `id` disappears. Fix every caller that read `.id` to use `device.ImTex()` or `SetTexture`.
  4. **`render_target.cpp`:** `RenderTarget` wraps `rhi::RenderTarget`, and `ScopedTargetBinding` becomes `device.BeginPass/EndPass`. Remove every GL state save/restore. The RHI now sets the full state per draw.
  5. **`vao.cpp`:** replace with `rhi::Device::Draw/DrawQuads` plus a small CPU-side vertex list. Keep the `Vao` API for callers to keep the diff small, then remove it once all callers are moved.
  6. **`render.cpp`:** replace every `gl*` with RHI calls, function by function:
     - `Render()` constructor (programs),
     - `UpdateProj`, `BeginPass`, `SetModelView/SetMatrix*`,
     - `SetBlendingMode`, `ApplySpriteTextureMode` (palette upload → `UpdateTexture(paletteTex, …)`),
     - `BindSpriteTexture`, sprite cache (`CachedSprite::tex` → `std::unique_ptr<rhi::Texture>`),
     - `DrawGridLines`, `DrawSpriteOnly`, `Draw`, `DrawLayers`, `DrawPatLayerItem`, `DrawCgLayerItem`,
     - `GenerateHitboxVertices`, `GeneratePartCenterVertices`, `GenerateUVRectangleVertices`,
     - `DrawBgPattern`, `SetupSpriteShader`, `SetSpriteTransform`.
     - `glVertexAttrib4fv(2, colour)` becomes a constant `g_color` in the textured and parts programs (GLSL: `uniform vec4`). This changes the GLSL. The output must stay identical, so check it.
  7. **`parts/parts.cpp`, `parts_texture.cpp`:** blend per part → `BlendState`; texture binds → `SetTexture`.
  8. **Tool windows (§3.3):** add `rhi::Texture* rhi::UploadRgba(…)` and one helper `ui::ImageTexture` (owns the texture, re-uploads on size change). Replace the hand-rolled upload in all 10 files. In `ImTextureID` sites, use `device.ImTex(tex)`. For render targets, use `device.ImUv(rt)` instead of hard-coded flipped UVs.
  9. **`main_frame.cpp`, `startup_args.cpp`, `ui/*`:** `glClear`/`glViewport`/`glReadPixels` → `BeginPass` clear and `ReadRgba`.
  10. `bg_renderer.cpp`, `bg_render.cpp`, `s3d_view.cpp`, `game_view.cpp`, `game_frame_share.h`: port to the RHI on GL too. Do these last within P1. They are the biggest (269 + 102 calls). Split `bg_renderer` by function (`Renderer::Init`, programs, `DrawLayer`, post passes, readback) and commit each.
- **Lint gate:** `grep -rnE "\bgl[A-Z][A-Za-z0-9]*\(|\bGL_[A-Z0-9_]+|<glad/glad.h>" src | grep -v "^src/rhi/gl/"` prints **nothing**. Add this as ctest `rhi_lint` (a shell or Python script, no GPU).
- **Test:** after every step, the smoke subset (`ctest -R render_parity`). After each file group, the full corpus.
- **Gate P1:** the full corpus is 100% byte-identical to the golden (UI included; `AA` entries from P0 may stay). `rhi_lint` and ctest are green.
- **Failure:**
  - The usual cause is state that leaked before and no longer does: a blend left on from a previous draw that some later draw depended on.
  - Find it with `--only <case>` plus a GL state dump. Add `RHI_TRACE=1` logging of every state change.
  - Reproduce the old behaviour **explicitly** at that call site, with a comment naming the old leak. Never reintroduce implicit state.
- **Deliverables:** the RHI, the GL backend, the shader files, the lint test.

### Phase P2: D3D9Ex backend
- **Steps:**
  1. `src/rhi/d3d9/rhi_d3d9.cpp` implementing §5.1 with §5.3. Link `d3d9`. Load `d3dcompiler_47` dynamically.
  2. Write `res/shaders/*.hlsl` from the GLSL, one by one, with the same constant names. The shader compile errors go to the log with the file and line.
  3. Add backend selection: `--renderer gl|d3d9`, an ini key `Renderer=` and a Preferences combo (with i18n Japanese strings, missing=0). Default: GL until Gate P2, then D3D9.
  4. Platform: create the SDL window without `SDL_WINDOW_OPENGL` for d3d9. Use ImGui `imgui_impl_dx9` + `imgui_impl_sdl3` (`ImGui_ImplSDL3_InitForD3D`). Detached viewports via the dx9 backend's swap chains.
  5. Headless on D3D9: hidden window + offscreen targets. `--capture` and `--capture-windows` via `ReadRgba`.
  6. Run the corpus with `--renderer d3d9` and diff it against the golden.
- **Gate P2:**
  - 0 FAIL on the whole corpus with `--renderer d3d9`. Every ALLOWED case carries an `EMU`, `AA` or `FLT` entry with evidence.
  - ctest green with both renderers: add a ctest `render_parity_d3d9` smoke run.
  - All ui-script shots complete.
  - D3D9 becomes the default.
- **Failure:**
  - Most diffs will be half-pixel offsets, the L8 sampling channel, BGRA swizzle, readback row order, line-loop closure, or blend factor mapping.
  - Work through them by class, worst first.
  - For each `EMU` candidate, find the matching game capture (MISMATCHES.md / v2 INDEX). Run the residual model (`tools/uni/s1_compare/v2_model.py`) on both the GL golden and the D3D9 output. D3D9 must be ≤ GL residual to be allowed.
- **Deliverables:** the D3D9 backend, HLSL shaders, the renderer switch, the run report.

### Phase P3: the games' own effects (UNI2, MBTL, DFCI)
- **Steps:**
  1. **Effect parser** (`src/rhi/d3d9/fx/fx_parse.{h,cpp}`) for the `.fx` text dialect in `Shader/*.txt`. Files are Shift-JIS (cp932) with Japanese comments; decode before parsing. It parses:
     - global uniforms with types, defaults and annotations;
     - `texture` / `sampler2D … = sampler_state { … }` blocks: `Texture`, `MinFilter`/`MagFilter`/`MipFilter`, `AddressU/V`;
     - functions (passed through as HLSL text);
     - `technique NAME { pass P0 { VertexShader = compile vs_3_0 F(); PixelShader = compile ps_3_0 G(); <render states> } }`.

     Record every render state each pass sets. Write a unit test that parses all 25 UNI2 files and all 25 MBTL files plus DFCI's, and lists every technique, pass and state. That output is the coverage table: commit it as `tools/render_parity/fx_inventory.tsv`, generated from the user's files at test time; it contains names only.
  2. **Compile:**
     - For each pass, strip the effect-only syntax (techniques, `sampler_state` blocks become plain `sampler2D` declarations).
     - `D3DCompile` the VS and PS entry points, with `D3DCOMPILE_OPTIMIZATION_LEVEL1` to match D3DX's default.
     - Read the constant table from the bytecode's CTAB comment block (parse it yourself; the format is in the D3D9 docs: `D3DXSHADER_CONSTANTTABLE`).
     - Map uniform names to registers.
  3. **IDA (one game at a time, starting with UNI2 1.42)** via the IDA MCP. Trace every `ID3DXEffect` call site:
     - `SetTechnique`, `SetMatrix`/`SetFloat`/`SetVector`/`SetTexture`/`SetValue`, `Begin`/`BeginPass`/`CommitChanges`;
     - for each, the technique name and the source of each parameter.

     Known starting points:
     - `CharaObj_DrawWithStageLight` 0x5CE3C0 → `PatPart_ComputeSpecular_PRSPPlusObjAdd` 0x413BA0;
     - `g_CharaAura` 0xC7A688;
     - MBTL `Han6Draw_SubmitCgSprite_ParentMtx` 0x4A0C40 (picks `TecCharaDrawBgColorFilter_V` at zoom > 1);
     - MBTL `Han6Object_Draw` 0x596FA0 / `Han6Draw_DrawLayer` 0x4A0070.

     Write `docs/render/<game>_effect_calls.md`: per draw kind (CG sprite, PAT part, mask, blur, mono, fade, BG object, BG specular, FBX), the technique, every parameter and its source, the textures, and the render states the game sets around it. Rename everything you touch in the IDB and save.
  4. **Effect draw path:**
     - `rhi::d3d9::Effect` (techniques → compiled passes + states) and an `EffectLibrary` loaded from `<game>/Shader/` when the character's game folder is known. The path comes from the existing game-folder detection (the save-to-mod-folder code finds UNI2/MBTL installs).
     - `Render` gets a `GameShaderMode { Off, On }`. With On and the game's library loaded, CG and PAT draws go through the traced technique, with parameters filled the way the game fills them.
     - Values the editor doesn't simulate (stage light colour, aura) default to the neutral values the game uses in training on stage 1, recorded in the doc, and are exposed in View > Game shaders.
  5. **Fallback:** no install or a compile failure means the ported shaders (P2 path), logged once.
- **Test:**
  - Corpus with game shaders **Off**: identical to the P2 result (no regressions).
  - Corpus with game shaders **On**: a separate run, diffed against the golden with class `GAME`.
  - For every case that has a game capture, run the residual models. **The On path must be ≤ the Off path's residual on every captured case, and better on the mean.**
  - Unit tests: the parser on all files; constant-table parsing on 5 compiled shaders; technique selection per draw kind.
- **Gate P3:**
  - The parser and compiler handle 100% of the techniques in UNI2, MBTL and DFCI (`fx_inventory.tsv` all `OK`).
  - The traced call docs exist for UNI2 and MBTL; DFCI at least for characters.
  - Game-capture residuals improve as above.
  - Game shaders are on by default when an install is present.
- **Failure:**
  - Compile errors in game files mean your preprocessing is wrong. D3DX accepts them, so fix the parser.
  - A residual that got worse means a parameter source is wrong. Go back to IDA for that draw kind.
  - Don't allowlist your way out of P3.

### Phase P4: stages and 3D on the RHI and game shaders
- **Steps:**
  1. Finish `bg_renderer.cpp`, `bg_render.cpp` and `s3d_view.cpp` on D3D9: HLSL ports of their GLSL, plus the depth, cull and front-face states of `s3d_view`.
  2. With game shaders On, stage objects use `sh_bgobject`, `sh_bgspeculer` and `bg*.txt`, and FBX uses `fbxshader`, traced in IDA the same way as P3 (`docs/render/<game>_effect_calls.md`, stage section).
- **Gate P4:**
  - Corpus area C: 0 FAIL against the golden with game shaders Off.
  - With game shaders On, `GAME` entries only, each better than or equal to the Off path on any stage frame that has a game capture.
  - The 3D stage view and the Blender round trip (`s3d_blender`) still work.

### Phase P5: native Linux
- **Steps:**
  1. **`build_linux.sh`:** CMake + Ninja, gcc, into `build-linux/`. SDL3 comes from the submodule (built for Linux).
  2. **DXVK-native:** vendor the official DXVK release's native build. Use the `dxvk-native` tarball from the DXVK GitHub release matching a tag at least 2 weeks old, under `deps/dxvk-native/`; if no prebuilt is usable, build it with meson in a venv (`python3 -m venv`, `pip install meson`). Link `libdxvk_d3d9.so`, and use DXVK's SDL3 WSI (`DXVK_WSI_DRIVER=SDL3`). Include DXVK's `d3d9.h` from its native headers.
  3. **HLSL compile on Linux:** vendor vkd3d (`vkd3d-shader`) from source (autotools; bison and flex are installed) under `deps/vkd3d`. Use `vkd3d_shader_compile` with source `VKD3D_SHADER_SOURCE_HLSL` and target `VKD3D_SHADER_TARGET_D3D_BYTECODE` for vs_3_0/ps_3_0. Wrap it so the RHI's `CompileHlsl()` has one signature on both platforms. Any shader vkd3d can't compile: log it and use the P2 port (ported shaders must compile; if one doesn't, simplify the HLSL so both compilers accept it, then re-check Windows parity).
  4. **Platform portability:**
     - `AnsiToUtf8`/wide-path helpers → UTF-8 `std::filesystem` paths everywhere on Linux.
     - `GetCurrentDirectoryA` → `SDL_GetCurrentDirectory`.
     - Registry and Steam-path lookups → `~/.steam/steam/steamapps/common` and libraryfolders.vdf.
     - Collect every `#include <windows.h>` site (`grep -rln "windows.h" src`) and either guard it `#ifdef _WIN32` with a portable alternative, or move it into `src/platform/`.
     - List them all in STATUS before starting.
  5. **Headless on Linux:** `SDL_VIDEO_DRIVER=offscreen` (or `dummy`) plus `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` (lavapipe) for deterministic CPU rendering.
- **Test:**
  - Linux ctest (non-GPU plus `render_parity` on lavapipe).
  - The full corpus on Linux, diffed against the **Windows D3D9 run from Gate P2/P3** (not the GL golden) and against the golden.
- **Gate P5:**
  - Builds and starts with no Windows-only code.
  - The corpus on lavapipe: 0 FAIL against the Windows D3D9 run, with `FLT`/`AA` allowed only with evidence (lavapipe vs hardware filtering precision).
  - Opens a character from each game folder layout.
- **Failure:** DXVK-native WSI or SDL3 issues: write a mailbox entry with the exact error, and continue with other items.

### Phase P6: remove GL
- **Steps:**
  1. Delete `src/rhi/gl/`, `res/shaders/*.glsl.*`, glad, `IMGUI_IMPL_OPENGL_*`, the `--renderer gl` option (keep accepting it with a warning that it is gone) and the Preferences entry.
  2. Update `.github/workflows/build.yml`: the Windows build plus the mingw cross build, and a new `linux-native` job that builds with `build_linux.sh` and runs `ctest` on lavapipe, including the `render_parity` smoke test against a golden **generated in CI from the same commit's Windows-equivalent rules**. If that's not possible without game data, run only non-GPU tests in CI and say so in the PR.
  3. Update the docs: `docs/plans/render_d3d9/STATUS.md` all ticked; `README` build instructions (Windows, Linux); the release notes draft.
- **Gate P6:**
  - The lint `grep -rnE "\bgl[A-Z]|GL_|glad" src CMakeLists.txt cmake` prints nothing.
  - Full corpus on Windows D3D9 and on Linux: 0 FAIL.
  - All ctest green.
  - The definition of done (§0) is fully true.

---------------------------------------------------------------------------------------------------------------------------

## 7. Order of work inside a session (the loop)
1. Read STATUS.md and the mailbox (top entries) for coordinator replies.
2. Pick the next unticked step of the current phase.
3. Make the change, keeping the commit small.
4. `./build.sh`. Fix warnings you introduced.
5. ctest (full, or `-R render_parity` while iterating).
6. Run the smoke corpus, then the full corpus at the end of a step group, and diff.
7. If everything passes, commit (specific files), tick STATUS, and append the run summary.
8. If not, investigate (§8), fix, and go back to 4. After 3 failed attempts on one item, write a BLOCKED entry and skip to the next independent item.

## 8. Investigating a diff
1. `diff.py --only <case> --heat` gives the heatmap, bounding box and max |d|.
2. Classify the diff by its shape:

   | Shape | Likely cause |
   |---|---|
   | Whole image shifted 1 px | half-pixel or origin snapping |
   | Colours swapped | RGBA/BGRA |
   | Image upside down | readback row order |
   | Thin lines missing a segment | line-loop closure |
   | Translucent overlaps different | blend factor/op or draw order |
   | Wrong palette colours | index sampling channel or palette UV maths |
   | Edges only | filtering/AA |
   | One tile black | a texture upload failed: check the log |

3. Reduce it with a one-line `.batch` reproducing just that case, and compare `RHI_TRACE=1` logs between gl and d3d9.
4. Fix it in code. Re-run that case, then its area, then the whole corpus.
5. Game captures (read-only, §2) are the judge only for `EMU`/`GAME` decisions, using the existing residual tools: `tools/uni/s1_compare/v2_model.py`, `v2_fullframe.py`, the floor numbers in `v2/INDEX.md`.
6. For P3/P4, compare against game captures with the sidecar JSON (frame, tick, camera) to set the editor's tick and camera exactly.

## 9. Dependencies

| Dependency | Pin | Build | Licence note |
|---|---|---|---|
| SDL3 | submodule `deps/SDL`, a stable `release-3.x.y` tag ≥ 2 weeks old | `add_subdirectory`, static | zlib |
| DXVK-native (P5) | release tarball or submodule at a tag ≥ 2 weeks old | prebuilt `.so`, or meson in a venv | zlib |
| vkd3d-shader (P5) | submodule at a release tag ≥ 2 weeks old | autotools, static | LGPL-2.1: link dynamically, or keep it a separate shared lib; note it in the PR |
| d3dcompiler_47 | system DLL (Windows 10+) | `LoadLibrary` | OS component |
| imgui backends | the existing imgui submodule | `imgui_impl_sdl3`, `imgui_impl_dx9` (+ `imgui_impl_opengl3` until P6) | MIT |

## 10. Reporting

**`docs/plans/render_d3d9/STATUS.md`:**
- one checkbox per step in §6;
- under each phase, the latest gate result: run id, PASS/ALLOWED/FAIL counts, the link to `runs/<id>/report.md`, and the date;
- a `BLOCKED` section;
- an `Allowlist changes` log.

**Mailbox `/home/teo/dev/orders/ds_render.md`** (append at the top, never edit old entries). Write an entry:
- at every gate pass or failure;
- for every `BLOCKED` item;
- for every allowlist entry flagged `needs-review`;
- when you need a decision;
- at least every 3 hours of work, as a short heartbeat: phase, step, last run counts.

Format:
```
## 2026-10-19 14:05 P1 step 6 (render.cpp)
state: in progress | gate passed | gate failed | blocked | needs decision
runs: <run-id> PASS n / ALLOWED n / FAIL n
done: ...
next: ...
need: ... (only if blocked / decision)
```

**PR:** open **one draft PR** `render/d3d9 → main` after Gate H (title: "Renderer: SDL3 + D3D9 (draft)"). Update its description at each gate: phases done, corpus counts per backend, allowlist summary, known issues. Never mark it ready and never merge it. The coordinator does that.

## 11. What failure looks like (stop and report instead of pushing on)
- You're about to change a golden, a threshold, or the corpus to make a FAIL go away.
- A phase gate has been failing for more than one working day with no new hypothesis.
- A step needs launching a game, sudo, or a dependency not listed in §9.
- The build or ctest is red at the end of a session. Revert to the last green commit on a new WIP branch name and report.
- You find user-visible behaviour (menus, shortcuts, file formats, saving) that changed because of the port. That's a regression. Fix it, or report it.
