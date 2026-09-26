# Hantei-chan wave 2: rendering infrastructure and package tools

Status: 2026-09-23. Branch `feat/wave2` (worktree `wt/wave2`, based on `update/ex-mbac` `c015c09`). Not pushed.

This wave covers phases G and H of the Gonptechan EX port plan (`GONPTECHAN_EX_INVENTORY.md` §5):

- G1: per-view render targets
- G2: PNG export
- G3: onion skin
- G4: detachable multi-monitor windows (GitHub issue #3)
- H: MBAACC package tools

Every rendering feature reads spawned actors and ticks from the cached preview simulator (`preview_sim.h`, `getStateAt`). Nothing re-simulates from tick 0 the way EX does.

Captures, fixtures and test output are in `docs/wave2_captures/` (§7).

---

## 1. Commits

| Commit | Content |
|---|---|
| `d546a8f` | GLAD regenerated for GL 3.3 (compatibility profile) to get the FBO entry points |
| `851ee5c` | Per-view render targets, onion skin, PNG export, detachable windows, headless startup options, `tools/wave2_regress.sh` |
| `9d26aad` | MBAACC package tools (validator, CRLF repair, HA6 consolidation, `.p` archives), the `mbaaccpackage` CLI, the test, and the `ReadInMem` fix (A5) |
| `6e58eb5` | Sprite texture cache keyed by CG generation; the `--stats` timing option |
| `823e3be` | `--post-key` check for keyboard routing into detached windows |
| `973e32b` | PNG export speed-ups (half-scale bounds analysis, one WIC factory per thread); timings in the manifest |
| `8981179` | Fix a false failure in the regression script |

---

## 2. Per-view render targets (G1)

### 2.1 Design

- **`RenderTarget`** (`src/render_target.*`)
  - One FBO per view: an RGBA8 colour texture plus a DEPTH24_STENCIL8 renderbuffer.
  - Sized lazily with `ensure(w, h)`. The texture name is kept across resizes, so an `ImGui::Image` recorded earlier in the frame stays valid.
- **`ScopedTargetBinding`**
  - An RAII guard that binds a target for one pass.
  - On scope exit it restores the draw and read FBO, the viewport, the scissor, the colour mask and the clear colour.
  - `clear(..., opaque=true)` write-protects alpha for the rest of the pass, so translucent sprites cannot punch holes in an image that ImGui later draws with blending.
- **`Render::BeginPass(PassParams)`**
  - `PassParams` is `{width, height, originX, originY, zoom}`.
  - It derives the projection, `x`/`y` and `scale` for that pass.
  - Every pass sets its own camera: main view, detached view, onion sample, export tick and analysis. Nothing is swapped and then restored.
  - The integer truncation of `x`/`y` is the same as the old single-surface code.
- **`CharacterView`** now owns:
  - a `ViewCamera` (pan and zoom). Views of one character used to share `CharacterInstance::renderX/Y`. That field is still kept as "the last camera", for new views and old project files.
  - its `RenderTarget`
  - a stable `id`
  - its `OnionSkinSettings`
- **The main window** renders its active view into that view's target, then `glBlitFramebuffer`s it to the back buffer before ImGui draws.
- **Scene code** (`src/ui/view_render_impl.h`, moved out of `main_frame.cpp`):
  - `AddSimulatedActorLayers` builds layers from a `TickState`.
  - `DrawCharacterScene(view, pass, SceneOptions)` draws the grid, the onion skin, the layers and the boxes.
  - `DrawMainViewScene` adds the stage passes and the preset markers.
  - The viewport, detached views, onion skin and export all use these functions.
- **Input code** no longer reads `render.scale`, `clientRect` or `renderX`:
  - box drawing (`BoxPane::BoxDragWorld`)
  - panning
  - the position tool
  - stage zoom and the inspector

  It uses the view camera instead.
- **ImGui coordinates.** With viewports enabled, ImGui coordinates are desktop coordinates. Main-window overlays (preset markers, position tool handles, stage rects) are offset by `GetMainViewport()->Pos`.
- **Sprite texture cache** (`Render::SwitchImage`, commit `6e58eb5`):
  - Textures are keyed by `(CG*, CG::generation(), image)`, LRU within 256 MiB.
  - `CG` renews a process-unique generation on load, palette load, palette change and free. A reloaded or recoloured CG, or a new CG at a recycled address, therefore never hits a stale texture.
  - Before this, every layer switch decoded and uploaded the sprite again.

### 2.2 Behaviour change (bug fix)

`DrawLayers` used to multiply CG layers by the root frame's RGBA a second time. The layer tint and alpha already carried it, so:

- colour and alpha were squared on layer 0
- spawns were tinted by the root

The base colour is now white. The PAT path never had this bug. Frames with an AF colour or alpha other than 1 look different; every capture below uses frames with RGBA = 1.

---

## 3. PNG export (G2)

`src/ui/png_export_impl.h` and `src/png_writer.*`. Open it from View → Export PNG... (Ctrl+P), or press Shift+P to export the current frame (P alone toggles the spawn preview since the feat/issues merge). Detached windows also have an "Export PNG..." button.

### 3.1 Options

- **Range:**
  - the current frame (exactly what the view shows)
  - a tick range (with a step)
  - the whole pattern (tick 0 to `settledTick()`, or `rootEndTick()`, or the horizon; capped by "Max ticks")
  - optionally, only the ticks where the root frame changes
  - optionally, skip empty ticks
- **Content:** spawned actors, hitboxes, and a smooth filter can each be switched on or off. Scale is 1× to 8×.
- **Background:**
  - Transparent: a black/white two-render matte, which recovers straight alpha. Normal blending is exact; additive and subtractive layers get a screen-style approximation.
  - The editor colour.
  - A custom colour.
- **Canvas:**
  - Fit all ticks: one fixed size, the union of the alpha bounds.
  - Fit each tick.
  - A fixed W×H with an origin.

  The bounds come from a half-scale analysis render into 1024², which covers a 2048-world-pixel window around the root.
- **Output:**
  - `<prefix>_t0123.png` (for the current frame, `_fNNN_tNNNN`)
  - a JSON manifest with, for each file: tick, root frame, actor count, canvas size, and the pixel position of world (0,0). It also records the timings.
  - The default folder is `<character folder>\export`, never the exe folder.

### 3.2 Writer

- WIC PNG (`wincodec.h`, `-lwindowscodecs -lole32`), which works under MinGW.
- Paths are UTF-8 in the app and UTF-16 at every Win32 call.
- Each file is written to `<file>.tmp` and then moved into place with `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`.
- The folder picker is `IFileOpenDialog` with `FOS_PICKFOLDERS | FOS_NOCHANGEDIR`.
- `create_directories` uses `error_code`, so it never throws.

### 3.3 Changes vs EX (`frame_export.cpp` + `MainFrame` orchestration)

| EX | Ours |
|---|---|
| Re-simulates each exported tick from tick 0 | One `getStateAt` per tick (a sequential cursor, about 1 simulated tick each) |
| Swaps editor render state for an off-screen pass and restores it by hand | Private `RenderTarget` + `BeginPass`; the view state is only read |
| Alpha recovery by division, with a heuristic mode | Black/white matte |
| Writes beside the exe; `create_directories` can throw | Character folder or chosen folder; `error_code`; atomic temp + move |
| Narrow paths | UTF-8 → UTF-16 |
| 4096² analysis surface | 1024² at half scale (a quarter of the readback) |

Not ported: EX's "legacy Hantei screenshot" and "framebuffer pixels" appearance modes, and the smart-crop debug report.

---

## 4. Onion skin (G3)

- **Settings.** Per view (View → Onion skin, the O key, the "Onion" checkbox and "..." settings in detached windows):
  - before and after: 0-16 samples
  - spacing in ticks, or "keyframes": the tick each neighbouring root frame visit starts
  - include spawned actors
  - opacity and per-sample falloff
  - past and future tint
- **Saved** in the project, per view.
- **How it works.**
  - `DrawOnionSkin` asks `sim.getStateAt(t)` for each sample tick, farthest first.
  - Each sample is drawn behind the current frame with its tint and alpha, without hitboxes.
  - The four LRU cursors of the simulator serve the burst.
- **Cost** is measured with `--stats` (frames 10-19, MinGW Release, CPU ms per frame; the host was also running other builds):

| Scene | Samples | Simulation | Onion total | Scene total |
|---|---|---|---|---|
| aoko 156 t160 (11 actors) | 0 | - | - | 0.40 |
| aoko 156 t160 | 16 | 0.12 | 1.85 | 2.20 |
| ciel 199 t100 (42 actors) | 0 | - | - | 0.69 |
| ciel 199 t100 | 16 | 0.40 | 4.47 | 4.99 |
| hisui 170 t300 | 16 | 0.35 | 1.00 | 1.21 |

- **Before the sprite cache,** ciel 199 with 16 samples cost 20.8 ms, because every layer re-decoded its sprite.
- **The simulation share** (0.12-0.40 ms for 16 samples) is in line with `HANTEI_PREVIEW_SIM.md` §6.2. EX re-simulates each sample from tick 0, which is 9-97 ms for 17 samples on the same patterns.

---

## 5. Detachable multi-monitor windows (G4, issue #3)

`src/ui/workspace_hosts_impl.h`, `src/workspace_session.*`, `src/workspace_viewports.*`.

### 5.1 How it works

- **Model.**
  - `WorkspaceSession` (ported from EX, extended, with tests) owns, for each host, the tab list, the order and the active tab.
  - Host 0 is the main window; each other host is one detached window. Content ids are `CharacterView` ids.
  - `SyncWorkspaceSession()` keeps the model in step with `views`. Views are created and closed from many code paths.
  - `activeViewIndex` always means "the main window's tab". Keyboard shortcuts act on `getShortcutView()`: the active tab of the window that had focus in the last frame. This is the router's `focusedViewId`, which is now a view id rather than a pointer.
- **Windows.**
  - Each detached host is an ImGui window with `ImGuiViewportFlags_NoAutoMerge`, so it is always its own OS window and can be moved to another monitor.
  - Each has a tab bar, a toolbar (pattern/frame/tick, play, step, zoom, center, onion, panes, export PNG, back to main), and its own dockspace.
  - The dockspace holds that host's active-view panes. The pane windows are named `Left Pane###host<N>_Left Pane` etc. (`DrawWindow::setWindowNamespace`), so the arrangement belongs to the window.
- **The view image.**
  - The central node shows the view's own render target through `AddImage`.
  - Left or middle drag pans. The wheel zooms around the cursor. Right drag draws the selected box, as one undo step.
- **Tabs.**
  - A tab is a drag source.
  - Dropping it on a tab of any window inserts it there (left or right half). Dropping it on a bar, or on a window body, appends it.
  - Releasing it anywhere else, more than 40 px from where it started, opens a new window at the cursor.
  - The tab context menu and View menu offer: Move to New Window (Ctrl+Shift+D), Move to Main Window, Move to Window N, and "Return all tabs to the main window".
  - Closing a window returns its tabs to the main window. It never closes views.
  - Tab moves and closes queued while a tab bar is drawn are applied after it.
  - ImGui's one-frame lag on `SetSelected` is reconciled explicitly in `DrawHostTabs`.
  - Stage and PAT-editor tabs stay in the main window.
- **GL.**
  - `workspace_viewports.cpp` (from EX) creates no GL contexts. Each platform window gets a DC with the main window's exact pixel format, and all windows render through the one HGLRC. That is why FBO textures made in the main pass can be shown in any window.
  - No render state is swapped per detached view; each view is just another `DrawCharacterScene` pass into its own target.
- **Keys** reach detached windows through `SetWindowSubclass` on each platform HWND. It is installed by wrapping `platform_io.Platform_CreateWindow` in app code, and ImGui's own window procedure runs first. `imgui/` is untouched.
- **Persistence.** The project's `"workspace"` object holds:
  - the host ids
  - each host's tab list and active tab
  - desktop geometry and the pane toggle

  Per view, the file also stores `id`, `camera_x/y` and `onion`. All of it is written by the existing atomic `SaveProject`, in one pass. Pane split sizes persist in `imgui.ini` under the host's stable id.
- **Setting.** View → "Native detached windows (restart)", default on, stored as `DetachableWindows` in the INI. When it is off, detached tabs are floating windows inside the main window.
- **Background Inspector.** Viewports were disabled because this window opened at a fixed (80,80) desktop position, behind the main window. It is now placed relative to the main viewport. The stage view capture (§7) shows it inside the main window.

### 5.2 Changes vs EX

| EX (1abc27f9) | Ours |
|---|---|
| Swaps `clientRect`, `render.scale`, `renderX/Y` and `renderViewOverride` around each detached draw and restores them by hand, so an early exit breaks the main view | Per-view camera + `BeginPass`; `ScopedTargetBinding` restores GL state on every exit |
| Patches `imgui/backends/imgui_impl_win32.cpp` (`Gonptechan_HandlePlatformWindowMessage`) | HWND subclass from app code |
| 5 always-on JSONL trace writers | None |
| `SaveProjectExportSettings` rewrites the `.hproj` with a plain `ofstream` after `SaveProject` | One atomic write |
| `ViewHostRegistry` and `WorkspaceContentRegistry` beside the session | Not ported: the session plus `CharacterView` ids cover it |
| Per-monitor DPI scaling on | Not enabled |

---

## 6. MBAACC package tools (H)

`src/mbaacc_package.*`, `src/mbaacc_pack.*`, `src/mbaacc_package_cli.cpp` (the `mbaaccpackage.exe` CLI), `src/ui/package_tools_impl.h` (File → MBAACC package), `tests/package_tools_test.cpp`.

### 6.1 What they do

- **Validate.**
  - Reads the descriptor as raw bytes and counts CRLF, lone LF and lone CR.
  - Checks sections, FileNum, contiguous FileNN rows and undeclared rows.
  - Checks each HA6 (`Hantei6DataFile`), CG (`BMP Cutter`) and PAT (`PAniDataFile`) row for existence and magic.
  - Rows missing on disk are looked up in `<game>\*.p` and reported as `packed (0002.p: data/akiha.HA6)`.
- **Repair CRLF.** Makes a unique timestamped `.bak`, normalises line endings with an atomic write, then re-validates.
- **Consolidate layered HA6.** Keeps the top layer byte-for-byte, and only if it has every frameful pattern of the lower layers (checked with the real loader).
  - It backs up the descriptor, the layers and their `.notes` sidecars into a unique folder, then installs.
  - It restores the descriptor if the result fails validation.
  - The UI has a review window with a canonical name field, a merge-notes option and an acknowledgement checkbox.
- **Extract.** Writes packed-only rows beside the descriptor and never overwrites.
- **Archives** (`mbaacc_pack.*`, format from castergroup `montopak.py`):
  - The reader handles the FilePacHeaderA header, folder records, file records and the name and payload XOR.
  - **PC vs Steam:** both XOR the first 4 KiB, but Steam also XORs the last 4 KiB of files over 8 KiB. Header probes are therefore mode-independent, while extraction detects the mode per archive with montopak's tail-entropy test.
- **CLI:**

```
mbaaccpackage validate <chara.txt> [--no-packs] [--game <dir>]
mbaaccpackage repair-crlf <chara.txt>
mbaaccpackage consolidate <chara.txt> [main.HA6] [--no-notes]
mbaaccpackage extract <chara.txt> [--game <dir>]
mbaaccpackage pack-list <archive.p> [substring]
mbaaccpackage pack-extract <archive.p> <data/name> <out> [--mode pc|steam|auto]
```

### 6.2 Changes vs EX (`mbaacc_package_validator.*`, `ha6_consolidator.*`)

| EX | Ours |
|---|---|
| Requires FileNum 1..99 in every section, so a stock `[PAniFile] FileNum= 0` fails | PAniFile may be 0 |
| Narrow `path::string()`, `GetPrivateProfile*A`, `MoveFileExW` on narrow-derived paths | UTF-8/UTF-16 throughout; HA6 layers reach `FrameData::load` through a lossless ANSI or 8.3 short path |
| Backup `copy_file` without `error_code`; CRLF backup name has 1-second resolution | Unique names (`_2`, `_3`...), `error_code`, and an explicit incomplete-backup refusal |
| Plain `ofstream` staging | `CreateFileW` + `FlushFileBuffers` + `MoveFileExW(WRITE_THROUGH)` |
| No archive awareness | `.p` lookup, header probe and extraction |

`ReadInMem` (inventory A5) now opens with shared read and write access, uses `GetFileSizeEx`, checks for a short read, and does not leak when `new` fails.

### 6.3 Checked against game data

- Extracted `data/akaakiha.HA6`, `data/akaakiha.cg` (27 MB, well past the Steam-only region) and `data/akiha_0_c.txt` from `C:\games\mbaacc\0002.p`. All three are byte-identical to the loose PC 1.07 files. All 9 archives detect as PC.
- A copy of `akiha_0.txt` in an empty `data` folder validates as 5/5 packed. `extract` writes the five files, and the CG is identical to the loose one.
- Consolidating the real 4-layer `akiha_0` refuses: "Pattern 0 exists only in akiha.HA6". Every file is left unchanged (md5 checked).
- `akiha.txt` validates PASS with loose files.

---

## 7. Verification

Everything runs headless through WSL interop; wine is not used. There are new startup options (`src/startup_args.h`):

- `--tick N`
- `--onion B,A,S[,K]`
- `--detach`
- `--capture-view png`
- `--capture-windows prefix`: each detached window's back buffer, read just before it is presented
- `--export-png dir` with `--export-range current|pattern|A:B`, `--export-scale`, `--export-bg`, `--export-boxes`, `--export-nospawns`, `--export-crop`
- `--save-project hproj`
- `--stats file`
- `--post-key vk`
- `--quit`

### 7.1 Regressions

`tools/wave2_regress.sh`; output in `wave2_captures/regression_results.txt`.

| Check | Result |
|---|---|
| shortcut_router_test | PASS (new cases for O, P, Ctrl+P, Ctrl+Shift+D) |
| undo_manager_test | PASS |
| workspace_session_test (new) | PASS |
| package_tools_test (new; Japanese-named temp folder) | PASS |
| cmdfile_test `..\tests\fixtures\cmdfile` | 272 checks, 0 failed |
| preview_sim_tool --selftest | passed (0 failures) |
| HA6 round trip, 269 MBAACC `.HA6`, new vs the `update/ex-mbac` roundtrip.exe | 269/269 identical |
| ha4tool roundtrip, 50 MBAC `.DAT` | 50/50 byte-identical |
| tools/bg_regress.sh 600 | 124 round trips ok, 124 sims ok, 0 determinism/edit failures |

### 7.2 Render captures (`docs/wave2_captures/`)

- **`baseline/*.png` vs `after_*.png`.** Main window before (`c015c09`) and after, for aoko 156 f2, arc 192 f3 and MBAC ARC.DAT 195 f3. The viewport region differs by 0, 0 and 1 pixel (`viewport_diff_vs_baseline.txt`). The single pixel is a texel-edge sample inside a PAT part (FBO vs back buffer). The rest of the window differs only by the new "View" menu and font anti-aliasing.
- **Stage view** (bg10 via `HANTEI_STAGE_PREVIEW`) was compared against the baseline exe. It is identical apart from the inspector's text and animation tick.
- **`onion_ciel199_compare.png`, `onion_aoko156_compare.png`.** Left: no onion. Right: onion with red past and blue future ghosts, including spawned effects. Also `onion_arc192_t40_keyframes.png` (keyframe mode).
- **`export_aoko156_sheet.png`, `export_ciel199_sheet.png`.** Whole-pattern transparent exports on a checkerboard. The Ciel sheet shows the fit-all canvas sized by the super's emblem. `export_aoko156_samples/` holds raw PNGs and the manifest. `export_arc192_2x_boxes/` is ticks 28-32 at 2× with hitboxes.
  - Every exported frame keeps at least 3 px of transparent border (checked over 514 frames), so there is no cropping.
- **Export timing** (to a local NTFS folder): aoko 156, 319 ticks at 1× in about 3.4 s; ciel 199, 195 ticks at 1× in about 6.3 s. At 2× (1480×1688 canvas) the time is dominated by WIC deflate, about 50 ms per frame. Writing to a `\\wsl.localhost` folder is about 2× slower.
- **`detached/`:**
  - `one_window_detached_1.png`: a detached native window with its own tab bar, toolbar and docked Left/Right/Box panes, restored from `one_window.hproj`.
  - `two_windows_*`: three views of one character. The main window shows view 1 at zoom 3. The detached window holds views 2 and 3, and the active one is at zoom 2 with onion on. This shows per-view cameras and settings.
  - `resaved_workspace.json`: the project saved again reproduces the workspace, and the empty host is dropped.
  - `key_routing_stats.txt`: posting 'O' to the detached HWND toggled the detached view's onion skin (`view_onion_enabled 1`, `shortcut_view 1`, `focused_host 1`). This shows subclass key routing and focus-scoped shortcuts.

---

## 8. Known gaps

- **Interactive detached-window gestures were not exercised**: dragging tabs between windows, the drop-to-detach release, and panning, zooming and box drawing on the detached image. Nobody drove the GUI with a mouse. Detach, move, restore, save, capture and key routing were tested headless. These need a manual pass on a real multi-monitor setup, as does the Background Inspector with viewports on.
- **Detached windows have no position tool.** The handles are main-window only. Stage and PAT-editor tabs cannot be detached.
- **Per-monitor DPI scaling** is not enabled, so detached windows on a monitor with a different DPI render at 100%.
- **Export:**
  - It is synchronous and blocks the UI for the export's duration.
  - The analysis window is ±1024 × (-1536..+512) world pixels, so anything outside it is not counted in the bounds.
  - Additive and subtractive layers in transparent exports are an approximation, as any straight-alpha PNG must be.
  - EX's legacy/framebuffer appearance modes are not ported.
- **Onion skin** follows the simulated tick flow. When the user picks a frame that the runtime flow never reaches, the samples are those of the tick, not of neighbouring authored frames. Keyframe mode steps through runtime frame entries.
- **Phase H items not done:**
  - selective resource reload
  - the shared effect cache
  - the section-expansion mask
  - the notes feature itself: the consolidator only merges existing `.notes` sidecars

  The validator does not decide load precedence between a loose file and a packed copy. It reports the loose file when one exists.
- **Paths:** `mbaaccpackage` fails on `\\wsl.localhost\...` descriptor paths (libstdc++ filesystem on UNC); `C:` paths work.
- **Merge with `feat/issues`** will conflict in:
  - `shortcut_router.h/.cpp` and its test: both branches append `ShortcutAction`s and bindings
  - `editor_tools_impl.h`: `RunShortcut` now resolves `getShortcutView()`
  - `main_pane.cpp` / `right_pane.cpp`: only the `Begin(windowName(...))` line changed here
  - `project_manager.cpp` and `character_view.cpp`

  All of these are additive on this side.
