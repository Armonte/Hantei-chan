# Hantei-chan renderer: the godlike path (plan, 2026-10-18)

## Goal
Hantei-chan draws every sprite, .pat, effect and stage **the same way the game does, pixel for pixel**: same API, same shaders, same states. It does this on Windows and on native Linux from one codebase. Today we imitate the games in OpenGL and get "as close as measured". The target is "identical, with a test that proves it".

## Facts this plan rests on
- **Every game we edit renders with D3D9 + D3DX9**, checked from the exes' imports:

  | Game | D3D9 | D3DX9 |
  |---|---|---|
  | MBAACC (`MBAA.exe`) | `d3d9.dll` | `d3dx9_36` |
  | UNI2 (`uni2.exe`) | `d3d9.dll` | `d3dx9_42` |
  | MBTL (`MBTL.exe`) | `d3d9.dll` | `d3dx9_42` |
  | DFCI (`RingGame.exe`) | `d3d9.dll` | `d3dx9_42` |

- **UNI2, MBTL and DFCI ship their shaders as HLSL effect source** in `Shader/*.txt`, and compile them at load (`D3DXCreateEffect`, plus `D3DXCreateEffectFromFileA` in DFCI).
  - UNI2 and MBTL have the same 25 files (5,353 lines in UNI2): `sh_chara`, `sh_panimask`, `sh_paniblur`, `sh_blur`, `sh_mono`, `sh_fade`, `sh_noise`, `sh_normal`, `sh_gaugedraw`, `sh_talkmono`, `sh_bgobject`, `sh_bgspeculer`, `bg*.txt`, `fbxshader`.
  - DFCI ships a subset.
  - They use vs_3_0 / ps_3_0 techniques, with sampler and blend state inside the effect. For example, `TecCharaDrawBgColor_V` samples the palette with Point filtering.
  - MBAACC is fixed-function D3D9.
- **Hantei-chan today** is Win32 windowing (`imgui_impl_win32`) plus OpenGL. ImGui's GL backend is built in ES2 mode, which gives 16-bit indices and no vertex offset; that caused the big-Squirrel-file bug. The GL code is 673 call sites in 27 files:

  | File | GL calls | What it draws |
  |---|---|---|
  | `bg_renderer.cpp` | 223 | stage backgrounds |
  | `s3d_view.cpp` | 99 | 3D stage view |
  | `render.cpp` | 68 | sprites and boxes |
  | `render_target.cpp` | 63 | off-screen targets |
  | `vao.cpp`, `texture.cpp`, `shader.cpp` | about 75 | GL helpers |
  | everything else | small | |

  There are 4 GLSL shader files plus inline shaders in the stage renderers.

## The stack

| Layer | Now | Godlike |
|---|---|---|
| Window, input, DPI, dialogs, drag-drop | Win32 | **SDL3**: the same code on Windows and Linux |
| Graphics API | OpenGL (ImGui on ES2) | **D3D9Ex**, the games' own API. On Linux, **DXVK-native** (D3D9 to Vulkan, the layer Proton uses to run these same games) |
| ImGui backend | `imgui_impl_win32` + `imgui_impl_opengl3` (ES2) | `imgui_impl_sdl3` + `imgui_impl_dx9`, with 32-bit indices |
| Character / .pat / stage shaders | Our hand-written GLSL imitations | **The game's own `Shader/*.txt`**, loaded from the user's install at runtime (never shipped by us), with our ports as the fallback |
| MBAACC | GL imitation of fixed-function | D3D9 fixed-function states, the same ones the game sets |
| Parity proof | Shots compared with captures, "at floor" | A **pixel-exact diff harness**: same inputs through the same shader; any non-zero diff fails |

**Why D3D9 and not D3D11/12/Vulkan.** Matching the game means matching its rasterisation rules: the half-pixel offset, sampler addressing, how DXT textures decode, and fixed-function blending. On D3D9 those come free. On any other API they are emulation again, which is what we do now. DXVK already keeps these games pixel-correct on Linux, so the Linux build inherits that.

## Phases (each one ships, with a gate)

1. **P0: SDL3 platform** (2–3 days).
   - Replace Win32 windowing and input with SDL3.
   - Keep OpenGL, but switch ImGui to desktop GL 3.3: 32-bit indices, vertex offset.
   - Move file dialogs to SDL3's dialogs, with the native Windows dialogs as fallback.
   - *Gate:* every ui-script shot and ctest unchanged, plus a native Linux window that opens a character.
2. **P1: renderer interface** (about 1 week).
   - Pull textures, buffers, render targets, shaders and blend states behind a small RHI out of `render.cpp`, `texture.cpp`, `vao.cpp`, `shader.cpp` and `render_target.cpp`.
   - The GL backend implements it with no behaviour change.
   - Tool windows (CG manager, archive browser, PAT editor) stop calling `gl*` directly.
   - *Gate:* the game-vs-editor comparison shots are byte-identical before and after.
3. **P2: D3D9Ex backend on Windows** (1–2 weeks).
   - Sprites, boxes, CG and .pat drawing, the palette path, render targets, headless/offscreen capture, and ImGui on dx9.
   - It becomes the default on Windows; GL stays as a fallback switch.
   - *Gate:* the comparison set is at or below today's floor on every frame, and ImGui Test Engine runs on it.
4. **P3: the games' effects** (about 2 weeks, plus IDA).
   - A small effect runtime: parse the `.fx` techniques, passes and state blocks, compile with `D3DCompiler_47` (part of Windows 10+), and apply the states.
   - IDA work: trace which technique, parameters and textures UNI2/MBTL/DFCI set for each character, .pat, mask and blur draw (the `ID3DXEffect` calls in each exe). Rename everything and save the IDBs.
   - Draw characters and .pats through `sh_chara`, `sh_panimask` and `sh_paniblur` from the user's own install.
   - *Gate:* pixel-exact diffs against gamecap captures with matched inputs, per game. Every remaining difference has an IDA-backed reason.
5. **P4: stages and 3D** (about 2 weeks).
   - Port `bg_renderer` and `s3d_view` to the RHI.
   - Stage objects go through `bg*.txt`, `sh_bgobject` and `sh_bgspeculer`, and the FBX path through `fbxshader`.
   - *Gate:* stage shots match the game.
6. **P5: native Linux** (1–2 weeks, the riskiest step).
   - SDL3 plus DXVK-native (d3d9 to Vulkan) packaged with the app.
   - The game effects compile with vkd3d-shader (HLSL SM3 to d3dbc). Where it can't compile a shader, we use our own port of that shader.
   - CI: a Linux job runs the GPU tests on lavapipe, a CPU Vulkan driver, through DXVK.
   - *Gate:* the same pixel-exact set passes on Linux.
7. **P6: drop GL.** Once P2–P5 have run clean for one release.

Rough total: about 8–10 weeks of focused work. P0 alone is already worth shipping.

## Risks and how we handle them
- **Effect framework.** `ID3DXEffect` lives in D3DX9, which we don't want to depend on (redistribution, and Linux). Mitigation: our own small effect layer, covering only the states these 25 files use, plus `D3DCompiler_47` on Windows and vkd3d-shader on Linux. Check its coverage on all 25 files first.
- **vkd3d-shader SM3 HLSL coverage** might miss pieces. Fallback: per-shader ports, already what we have, with the diff harness telling us which ones.
- **GPU tests in CI.** D3D9 has no WARP software rasteriser on Windows runners. Windows CI keeps running non-GPU tests only (as now, `ctest -LE gpu`). The pixel-exact GPU set runs on Linux CI (DXVK + lavapipe) and locally.
- **Game shaders are copyrighted.** We never bundle them. Hantei-chan reads them from the user's game folder, the way it reads the game's data. Without a game install, it uses our own shaders.
- **Scope creep.** Each phase ships behind a switch and has a gate. Nothing is merged on "looks right".

## What this buys
- **Exact parity.** "The editor looks different from the game" stops being possible by construction, and every leftover difference is a test failure with a reason.
- **New effects preview free.** Mask, blur, mono, fade and BG colour filter work in the preview with no imitation code.
- **Native Linux** from the same renderer.
- **No more ES2 and GL-driver bugs**: index overflow, Intel/AMD GL quirks, opaque GL errors like #87.
- **Groundwork for the in-editor game view**: the same API and shaders as the game, which is what Play-in-game previews and fpan-im integration need.
