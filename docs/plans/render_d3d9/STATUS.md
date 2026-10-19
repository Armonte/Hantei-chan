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
- [x] 3. Corpus generated (2847 cases); two plain-baseline runs rendered; the
      flakiness found (39 of the then-2757 cases) and fixed with
      `--deterministic`.
- [x] 4. Golden rendered with the determinism binary, twice.
- [x] 5. `run_corpus.sh -j N` sharding (default 8): round robin over the batch
      lines and the ui scripts, one scratch folder and one exe copy per shard,
      the scripts that write the shared scratch fixtures (the four
      `shots_cgm__cgm_*`, `shots__s2_pat_editor`, `export_*`) serialised on
      shard 0. `han2_settings.ini` is reset per file, not per shard, so a case
      does not depend on what ran before it. The split is a pure function of the
      sorted corpus list and N, so `-j 8` covers the same cases as `-j 1`.
      Checked: `-j 1` vs `-j 8` byte-identical on the smoke subset,
      `stage3d_dfci` and the cgm palettes; the full corpus at `-j 8` is 3m28
      (was 6-8 min). Golden and new runs both use `-j 8`.

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
| H1 (two baseline runs byte-identical) | **PASS** | `golden/<BASELINE>` vs `..._repeat`, report `runs/h1_report.md` | 2847 cases, 0 differing | 2026-10-18 |
| H2 (`test_diff.py` + `diff.py golden golden`) | **PASS** | `runs/h2_report.md` | test_diff 14/14; diff golden golden 2847/2847 PASS | 2026-10-18 |
| ctest | **PASS** | `build/ctest -j6` | 100/100 (93 before the HLSL/FX/LINUX merges), incl. `render_parity` 60/60, `render_parity_tools`, `glsl_identity`, `hlsl_compile`, `hlsl_ref`, `fx_*`, `portable_*` | 2026-10-19 |

## BLOCKED

## Phase P0: SDL3 platform, GL kept (PLAN.md 6 P0)

- [x] 1. SDL3 vendored as a submodule `deps/SDL`, pinned at the
      `release-3.4.18` tag (`829a65d769d935c4852f8159e964312c0957260a`,
      published 2026-10-02, 16 days old). Built static from the submodule by
      `cmake/sdl3.cmake` (`SDL_STATIC ON`, `SDL_SHARED OFF`,
      `SDL_TEST_LIBRARY OFF`). Verified with `make SDL3-static`: SDL 3.4.18
      cross-compiles cleanly with x86_64-w64-mingw32 into `libSDL3.a`.
- [x] 2. `src/platform/platform.{h,cpp}`: SDL3 window (OPENGL | RESIZABLE |
      HIGH_PIXEL_DENSITY | HIDDEN/UTILITY when headless), a GL 3.3
      compatibility context with 24/8 depth+stencil, `SDL_GL_SetSwapInterval`,
      `gladLoadGLLoader(SDL_GL_GetProcAddress)`, framebuffer/window size
      helpers, the native HWND/HDC, the scancode<->VK table
      (`platform_keys.cpp`) and the event pump.
- [x] 3. `main.cpp` SDL main loop (entry point `int main(int, char**)`; SDL
      supplies `wWinMain` under UNICODE). `PostQuitMessage` became `gAppQuit`.
- [x] 4. ImGui: `imgui_impl_sdl3` replaces `imgui_impl_win32`, and
      `IMGUI_IMPL_OPENGL_ES2` is gone, which enables `glDrawElementsBaseVertex`.
      One consequence had to be handled: without ES2 the backend binds its own
      GL sampler (`glBindSampler`), which overrides the GL_NEAREST filter the
      app sets on its pixel-art textures. `ui::ImageWithOwnSampler`
      (src/imgui_utils.h) asks the backend to keep each texture's own
      parameters, and every app-texture draw site uses it.
- [x] 7. `MessageBox*` routes through `platform::MessageBox`
      (src/platform/platform_message.h): a real box when visible, a log line
      under `--headless`. Not cosmetic: the GL error paths called
      `MessageBoxA`, and under `--headless` a modal box blocked the batch
      renderer forever.
- [x] 5. Detached windows through the SDL3 + GL3 backends;
      `workspace_viewports.{h,cpp}` and its per-HWND key subclass deleted
      (commits `345e264`, `c21d517`, `f041e10` and the step commit).
      `ImGuiConfigFlags_ViewportsEnable` is set once both backends registered
      their viewport hooks (`src/platform/viewports.cpp`); imgui_impl_sdl3
      creates each detached window with `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`
      from the main context, so the views' FBO textures draw there. Keys from
      a detached window reach `MainFrame::HandleKeys` from the app's event
      switch when the event's window (or, for a key pushed without one,
      `SDL_GetKeyboardFocus()`) is the main window or a viewport's
      `PlatformHandle`. `--capture-windows` reads each viewport's back buffer
      in a wrapped `Renderer_RenderWindow` (after its draw, before its swap).
      Fixed on the way, all user-visible regressions of P0 steps 2-4:
      ImGui got no SDL input at all (`ImGui_ImplSDL3_ProcessEvent` was never
      called) and the main view had no pan/draw drags; the ini was read
      after the window was created, so its size, position, maximised flag and
      DetachableWindows preference were ignored; the ini kept the client
      rectangle instead of the outer one (the window would shrink and drift on
      every restart); a detached window's close button quit the app and its
      resize/move overwrote the main window's ini values; the WM_CHAR swallow
      rule ran after ImGui had typed the character; drop points were window-
      relative instead of desktop + main-client coordinates.
      **Proof** (visible, in-app only): `tools/render_parity/detach_check.sh`
      runs `AB_VISIBLE=1` `--ui-script` twice on MBAACC akiha with
      `--detach --capture-windows --stats`, once with `--post-key 0x4F`
      ('O', Toggle onion skin) and once without. Result PASS: the view lives
      in detached host 1 which has focus, `view_onion_enabled` 1 with the key
      and 0 without, and the detached window's capture is 1130x672 and
      **byte-identical to the golden binary's** detached window from the same
      command (`render_parity/detach_check_baseline/`, run of
      `baseline_bin/<BASELINE>/gonptechan.exe`). The control runs differ by
      one hovered resize grip (78 px), which is the real desktop cursor's
      position, not the app.
- [x] 6. `--post-key` pushes an `SDL_EVENT_KEY_DOWN` (+ the key up, so ImGui
      does not hold the key) with the scancode mapped from the VK, aimed like
      the old `PostMessage` at the first detached window (else the main one).
      Landed with step 5; exercised by `detach_check.sh` above. Closed out on
      2026-10-19 (commit after `1010cc0`): the old `PostMessage` passed any VK
      through, including the generic modifiers `VK_SHIFT`/`VK_CONTROL`/`VK_MENU`
      (0x10-0x12) that are not in the scancode table, so `ScancodeFromVk` now
      maps them to the left key (one way; `VkFromScancode` is unchanged, so
      the event loop's VKs are not affected). Differences from the old path,
      both deliberate: the key up (the old WM_KEYDOWN with no WM_KEYUP left the
      key held in ImGui) and the main-window target when nothing is detached
      (the old code posted nothing). The only caller of `--post-key` in the
      repo is `detach_check.sh`, which always detaches, so it behaves as before.
      Evidence: `./build.sh` clean; ctest 100/100 at `-j8` (incl.
      `cgm_folder_mbaacc` 256 s); smoke corpus run `runs/smoke_20261019_145606`
      PASS 60 / ALLOWED 0 / FAIL 0 (no rendering code touched, so no full
      corpus run); `detach_check.sh` PASS (key run `view_onion_enabled` 1,
      control 0, both detached captures 1130x672); a headless run with
      `--post-key 0x11` logs no warning, and `--post-key 0xFF` (no scancode)
      still logs `--post-key 255: no SDL scancode for this virtual key`.
- [ ] 8. Remove `context_gl.h`.

**Gate P0: parity and ctest PASS.**

| what | result | evidence |
|---|---|---|
| corpus A, B, C, E byte-identical | **PASS** | `runs/p0j` vs the golden, 0 FAIL (steps 1-4, 7; numbers in `runs/p0_report.md`; the run was deleted after step 5 superseded it) |
| corpus D (UI) byte-identical | **PASS** | the whole 2847-case corpus is 2847 PASS / 0 ALLOWED / 0 FAIL (`runs/p0_report.md`) |
| ctest | **PASS** | 100/100, including `render_parity` (60-case smoke). After step 5: 99/100 at `-j8` with `cgm_folder_mbaacc` timing out at 300 s (load average 55-81 from other agents; cgmtool shares no code with this step); it passes alone in 700 s |
| corpus timing | **PASS** | full corpus at `-j 8` in 3m28, `-j 1` vs `-j 8` byte-identical on the smoke subset, `stage3d_dfci` and the cgm palettes |
| detached windows | **PASS** | `tools/render_parity/detach_check.sh` (step 5): open, render (byte-identical to the baseline exe), take a shortcut through `--post-key`, capture |
| drag-drop | pending | not exercised by a scripted run yet (drop points now go through `platform::PollEvents`) |
| full corpus after step 5 | **2828 PASS / 0 ALLOWED / 19 FAIL**, all 19 environment drift | run `runs/20261019_f041e10_gl`, report `runs/p0_step5_report.md`. The golden binary itself (`baseline_bin/<BASELINE>_det`) re-run today on the same 39 cases renders all 19 differently from the golden and byte-identically to the step-5 build (det vs new 39/39 PASS, `runs/p0_step5_env_check.md`). 16 UI shots: the archive browser's "Files in games (38956)" is now 38939 (C:/games changes under other agents; e.g. PovertyCaster logs in C:/games/mbaacc). 3 `stage_mbaacc_bg01_*`: the mist layer differs, `bg01.dat` unchanged (2026-07-03); the cause is outside the app binary. Golden NOT changed: needs the coordinator's decision (re-render the golden from the determinism binary, as on 2026-10-18, or pin the browsed folder). |

`tools/render_parity/allowlist.tsv` is still header-only: no case needed an
entry, because the UI differences this phase hit were a real bug (the sampler
override), not backend rasterisation ties.

**P0 step 3, first: the `WndProc` message list (PLAN.md 6 P0 step 3 asks for it
before the loop is rewritten). Read from `src/main.cpp` `WndProc`, which is the
complete list: there is no `WM_SETFOCUS`, `WM_KILLFOCUS`, `WM_DPICHANGED`,
`WM_SETTINGCHANGE`, `WM_ENTERSIZEMOVE`, `WM_NCHITTEST` or `WM_GETMINMAXINFO`
handler today.**

| message | what happens now | SDL mapping (P0) |
|---|---|---|
| `WM_CHAR` (before ImGui's handler) | `MainFrame::s_swallowChar` guard: a key that already ran a shortcut while a text field was focused must not also type its character | `SDL_EVENT_TEXT_INPUT` (drop the first char when it is the swallowed one) |
| (ImGui first) | `ImGui_ImplWin32_WndProcHandler` gets every message before the app | `ImGui_ImplSDL3_ProcessEvent` before the app's own switch |
| `WM_CREATE` | `ContextGl` (wgl), `gladLoadGL`, `ImGui::CreateContext`, `ImSearch::CreateContext`, `IniFilename` (null when headless), `InitIni`, viewports flag, `new MainFrame(context)` into `GWLP_USERDATA`, `MoveWindow`, `LoadJapaneseFonts`, `ImGui_ImplWin32_InitForOpenGL`, `ImGui_ImplOpenGL3_Init("#version 330 core")`, `WorkspaceViewports::Initialize`, `wglSwapIntervalEXT(0)` (vsync off) | `Platform::Init` + `MainFrame::CreateUi` |
| `WM_DROPFILES` | `setDropPoint` then `queueDroppedFile` per file (`DragAcceptFiles` registered at window create) | `SDL_EVENT_DROP_FILE` / `SDL_EVENT_DROP_POSITION` |
| `WM_MOVE` | save `gSettings.posX/posY` | `SDL_EVENT_WINDOW_MOVED` |
| `WM_SIZE` | maximized/restored flag, save pos+size, `clientRect`, `MainFrame::UpdateBackProj` | `SDL_EVENT_WINDOW_RESIZED` (+ `PIXEL_SIZE_CHANGED` for the framebuffer size) |
| `WM_KEYDOWN` | `MainFrame::HandleKeys(wParam, autoRepeat, WantCaptureKeyboard, WantTextInput)`; returning true suppresses the key. Bit 30 is OS auto-repeat | `SDL_EVENT_KEY_DOWN` |
| `WM_CANCELMODE` | `CancelViewportGestures`, clear the drag flags | `SDL_EVENT_WINDOW_FOCUS_LOST` / `SDL_EVENT_KEY_UP` (Escape) |
| `WM_LBUTTONDOWN`/`RBUTTON`/`MBUTTON` | pan/draw button preference (`gSettings.mousePanButton`/`mouseDrawButton`), `justActivated` guard, `SetCapture`, `RightClick`/`LeftClick`/`HandleMouseDown`, `SetMouseDragActive` | `SDL_EVENT_MOUSE_BUTTON_DOWN` (`SDL_CaptureMouse`) |
| `WM_LBUTTONUP`/`RBUTTON`/`MBUTTON` | release the drag for the matching button, `ReleaseCapture` when neither is held | `SDL_EVENT_MOUSE_BUTTON_UP` |
| `WM_MOUSEMOVE` | while dragging: `HandleMouseDrag(dx, dy, dragRight, dragLeft)` | `SDL_EVENT_MOUSE_MOTION` |
| `WM_MOUSEWHEEL` | only when ImGui does not capture the mouse and nothing is being dragged: `HandleMouseWheel(up != invertWheelZoom, x, y)` with the point converted to client space | `SDL_EVENT_MOUSE_WHEEL` |
| `WM_ACTIVATE` | `WA_INACTIVE`: `CancelViewportGestures`, `ReleaseCapture`, clear drags, `SetMouseDragActive(false)`; activating sets `justActivated` | `SDL_EVENT_WINDOW_FOCUS_GAINED` / `LOST` |
| `WM_SYSCOMMAND` `SC_KEYMENU` | swallowed (no Alt menu) | n/a (SDL has no window menu) |
| `WM_DESTROY` | `delete MainFrame`, `WorkspaceViewports::Shutdown`, ImGui GL3/Win32 shutdown, `ImSearch::DestroyContext`, `ImGui::DestroyContext`, `delete context`, quit | `Platform::Shutdown` after the loop |

Mouse handling that lives in `WndProc` (the pan/draw button split, `justActivated`,
the drag capture) is the part of `main.cpp` that needs the most care in P0; it is
not ImGui's, so the SDL port has to reproduce it message for message.

None.

## Allowlist changes

None yet. `tools/render_parity/allowlist.tsv` is empty (header only) and stays
empty until a P2 `EMU`/`AA`/`FLT` case needs an evidence-backed entry.
