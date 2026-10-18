# Native Linux: portability inventory and the P5 plan

Written 2026-10-18 by lane LINUX (deliverable 1 of the lane's brief; PLAN.md P5
steps 1-4). This file is the list of everything in `src/` that assumes Windows,
what each site becomes on POSIX, and the order the main line applies the
switch-over in P5. It is generated data plus hand-written prose: the tables
between the `auto:` markers come from `tools/linux/winapi_inventory.py`, so run

    python3 tools/linux/winapi_inventory.py --write-doc

after a source change instead of editing them by hand. Run `--check` in CI to
keep the P0-owned files out of the tables.

## 1. How to read this

The scope is `src/`, minus the files P0 replaces with SDL3: `main.cpp`,
`main.h`, `context_gl.h`, `workspace_viewports.{h,cpp}`, `filedialog.{h,cpp}`.
Those are the main line's, and the inventory tool fails its `--check` mode if
any of them appears.

Current size: <!-- auto:counts -->76 `#include <windows.h>`-family sites, 896 Win32 call sites<!-- /auto:counts -->

Two things are being separated here. The **portable layer** is what P5 actually
switches the callers to: `src/platform/portable/{paths,process,pipe,steam}.{h,cpp}`
(deliverable 2) give one signature per operation on both platforms, so a call
site changes once and then compiles everywhere. The **inventory** below is the
work list that drives those headers: every call, where it is, and which helper
covers it. Where a Win32 facility has no POSIX equivalent at all (the version
resource, the SDDL security descriptor, `/select,` Explorer highlighting) the
row says so and P5 keeps that path behind `#ifdef _WIN32`.

## 2. Remove the OS from the call sites, not from the code

The P5 goal is that `src/` above `src/platform` contains no Win32 call. That is
not the same as deleting every `#ifdef _WIN32`: a handful of behaviours are
genuinely Windows features the editor should keep when it runs on Windows.

- **Version resources** (`GetFileVersionInfoW`, `VerQueryValueW`,
  `VS_FIXEDFILEINFO` in `fpanim_install.cpp`) read the fpanim shim's DLL
  metadata. POSIX has no equivalent; the row stays in a `_WIN32` branch and the
  POSIX side reports "unknown version".
- **The current-user DACL** on the shared frame ring (`game_frame_ring.cpp`,
  SDDL + `OpenProcessToken` + `GetTokenInformation`) exists so another local
  user cannot read the producer's frames. POSIX gets the same guarantee from
  `shm_open` at mode 0600 in `$XDG_RUNTIME_DIR`, so the whole descriptor path is
  replaced, not branched.
- **Explorer integration** (`archive_browser_ui.cpp`'s `ShellExecuteW` with
  `/select,` and `SW_SHOWNORMAL`) has no Linux equivalent; the POSIX side opens
  the containing folder with `xdg-open` and does not select the file.
- **launching the games** (`authoring/game_launcher.cpp`, `bg_gametex.cpp`,
  `s3d_view.cpp`'s Blender launch) spawns Windows exes. On Linux the games run
  their own native binaries with a different install layout, which is outside
  this lane; the spawn helper is written portable (it is the same
  `posix_spawnp`/`CreateProcess` shape) but the callers that name a `.exe` stay
  Windows-only until P5 decides the Linux game paths.

Everything else in the tables is a mechanical swap onto a portable helper.

## 3. Paths and encoding

Paths are the largest group after the window types, and the one with the most
quiet bugs. Today there are three string types in flight: UTF-8 `std::string`
for UI and file-dialog results, the ANSI code page (`CP_ACP`, 932 on a Japanese
machine) for editor file paths and the ini, and UTF-16 `std::wstring` for the
Win32 `W` calls.

The conversion helpers already live in `png_writer.{h,cpp}` (`Utf8ToWide`,
`WideToUtf8`, `AnsiToUtf8`) and `misc.cpp` (`sj2utf8`, `utf82sj`). On Linux the
code page goes away: every path is UTF-8, so `AnsiToUtf8`/`Utf8ToAnsi` become
identity (or, at most, a UTF-8 pass-through for a file whose bytes were cp932 on
disk), and `Utf8ToWide`/`WideToUtf8` become the MBCS conversion `std::filesystem`
wants. `portable::CpToUtf8(s, cp)` keeps the code-page argument for the cases
that really read cp932 bytes (the games' text files), so the Linux build can
still decode a Japanese name from game data.

`std::filesystem::u8path` is used in a few places and is the right idea, but
note the C++17 `u8string` traps: `path::u8string()` returns `std::string` in
C++17 and `std::u8string` in C++20. The project is on C++17 (`CMAKE_CXX_STANDARD
17`), so a straight port keeps compiling, but helpers should take and return
`std::string` explicitly rather than relying on `auto`.

The inventory rows for this group are in section 6 below under `paths` and
`encoding`.

## 4. Registry, Steam and the remembered installs

`game_table.cpp` is the only real install scanner: it walks the Steam roots,
parses `libraryfolders.vdf` for extra library paths, reads every `appmanifest_*`
for `name` and `installdir`, and probes a candidate with `DetectGame`. It also
remembers the last folder per game in `han2_settings.ini` under `[browser]`
`Dir_<n>`, written with `WritePrivateProfileStringA` and read with
`GetCurrentDirectoryA` + `GetPrivateProfileStringA`.

There is no registry use in `src/` at all - the only `HKEY_` string in the tree
is in this doc's replacement table. The Windows install discovery is therefore
all filesystem and ini, which ports cleanly:

- Steam roots on Linux: `~/.steam/steam`, `~/.local/share/Steam`,
  `~/.steam/root`, plus any `$STEAM_ROOT`. Each root's `steamapps` is scanned
  the same way; `libraryfolders.vdf` has the same format, so
  `portable::SteamLibraries()` returns the same list on both platforms.
- The remembered folder is per-user state, not ini-per-directory: on Linux
  `GetCurrentDirectoryA` + `GetPrivateProfileStringA` becomes
  `portable::ini::GetString(portable::UserDataDir() / "hantei-chan.ini", ...)`.
  The Windows side keeps reading `han2_settings.ini` next to the exe, so the two
  builds do not fight over one file.
- `game_table.cpp` already guards its scan with `fs::is_directory` and a
  `skip_permission_denied` iterator, so a missing `steamapps` is a no-op. That
  behaviour is what makes the Linux scan safe on a machine with no Steam.

Several other files hard-code the two Steam install paths
(`fpanim_install_panel.cpp:53,55`, `cgm/cgm_window.cpp:1372-1373`,
`squirrel/script_panel.cpp:418-419`, `ui/main_frame_hotsave.cpp:146-147`). Those
are convenience defaults for the "choose folder" dialogs. P5 should route them
through the same `game_table` scan instead of growing a second copy of the
lookup; the table lists them so none is missed.

## 5. Named pipes, shared memory and the clipboard

Two features move bytes between the editor and another process, and both are
built on Win32 objects:

- **The fpan-im / MBAA link** talks over named pipes
  (`\\.\pipe\fpan-im-<game>`, `\\.\pipe\povertycaster-link-<pid>`). The client
  side is `game_link.cpp` and `squirrel/fpanim_link.cpp`; the mock server the
  tests exercise is `game_link_mock.cpp`. On Linux these become `AF_UNIX`
  sockets in `$XDG_RUNTIME_DIR` with a trivial name mapping
  (`portable::pipe::EndpointFor(name)`), a byte-stream read with `poll()` for the
  deadline instead of `OVERLAPPED` + `WaitForSingleObject`, and `ioctl(FIONREAD)`
  instead of `PeekNamedPipe`. The protocol on the wire is unchanged, so a Linux
  editor can talk to the same game-side code once the game side grows a Unix
  socket.
- **The cross-instance clipboard** (`copy_data.cpp`, `framestate.cpp`) is a
  named file mapping holding raw C++ objects, plus a tinyalloc heap. On Linux it
  is `shm_open` + `mmap`; the object-layout hash that already guards the mapping
  name (`DefaultMappingName`) keeps the two builds from sharing an incompatible
  layout. `MapViewOfFileEx` at a fixed address has a POSIX equivalent
  (`mmap` with `MAP_FIXED_NOREPLACE`) and the same fallback to a private heap
  when the address is taken.
- **The frame ring** (`game_frame_ring.cpp`, `game_frame_share.h`) is the same
  mapping pattern with a security descriptor instead of a fixup address.

## 6. Message boxes, process launch, timers, audio

These are small and mostly mechanical, with two exceptions worth naming.

`platform::MessageBox` (in `platform_message.h`) already exists and is the P0
interface; the remaining raw `MessageBox` uses are the ones that were not
converted with the window in P0 and should fold into the same helper. Note the
deliberate `#undef MessageBox` there: `windows.h` defines `MessageBox` as a
macro, and the helper is a function of the same name.

Timing splits into two meanings that must not be merged: `GetTickCount64` is a
monotonic clock (`portable::NowMs()`), and `GetLocalTime` is wall-clock
formatting for a log line (`portable::LocalTimeString()`). `Sleep` is
pacing/throttling in every case here and becomes
`std::this_thread::sleep_for`.

Audio is `bgm_player.cpp` (`waveOut*`) and `se_browser.cpp` (`PlaySoundW`). Both
are SDL3 on Linux (`SDL_OpenAudioDeviceStream`/`SDL_PutAudioStreamData` and
`SDL_LoadWAV`), which is the same dependency the window already pulls in, so no
new library is added. The vorbis decode is already done by the bundled
`stb_vorbis`, so only the output path changes.

## 7. What the main line applies in P5 (switch-over TODOs)

The helper headers land in this lane with the Windows body copied from the code
in the tables, so P5 only has to point the callers at them. Each line below is a
`TODO(P5)` the main line resolves; the file column is where the replacement is
written, not where the caller is.

| portable helper | replaces | callers to switch |
|---|---|---|
| `portable::ExecutablePath()` / `ExecutableDir()` | `GetModuleFileNameA/W` | `fpanim_install.cpp`, `bg_gametex.cpp`, `squirrel/script_panel.cpp`, `main.cpp` (P0) |
| `portable::UserDataDir()` | `GetCurrentDirectoryA` + ini next to the exe | `i18n.cpp`, `game_table.cpp`, `ini.cpp`, `archive_browser*.cpp`, `han2_browser.cpp`, `character_instance.cpp` |
| `portable::ini::Get*` / `Set*` | `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `WritePrivateProfileStringA` | all 44 profile rows in section 8 |
| `portable::SteamLibraries()` / `SteamApps()` | the hard-coded `steamapps` paths and `libraryfolders.vdf` parse | `game_table.cpp` plus the four default-location copies in section 4 |
| `portable::Utf8ToWide` / `WideToUtf8` / `CpToUtf8` / `Utf8ToCp` | `MultiByteToWideChar`, `WideCharToMultiByte`, `AnsiToUtf8`, `Utf8ToWide`, `WideToUtf8`, `Utf8ToAnsi` | `png_writer.cpp` (definitions) and the ~30 callers in section 8 |
| `portable::ReplaceFileAtomic()` | `MoveFileExA/W` + `WriteFileAtomic` retry loop | `misc.cpp:130`, `mbaacc_package.cpp`, `tag_sidecar.cpp`, `ui/png_export_impl.h`, `fpanim_install.cpp`, `script_panel.cpp`, `tag_migrate.cpp` |
| `portable::OpenFileExternally()` | `ShellExecuteW` | `archive_browser_ui.cpp`, `modproject/hmod_ui.cpp` |
| `portable::Spawn()` / `WaitExit()` / `Kill()` / `ProcessAlive()` / `FindProcesses()` | `CreateProcess*`, `GetExitCodeProcess`, `TerminateProcess`, `OpenProcess`, `CreateToolhelp32Snapshot` | `authoring/game_launcher.cpp`, `bg_gametex.cpp`, `s3d_view.cpp`, `fpanim_install.cpp`, `game_link.cpp`, `game_link_panel.cpp` |
| `portable::pipe::*` | `CreateFile*` on `\\.\pipe\`, `PeekNamedPipe`, `WaitNamedPipeW`, `CreateNamedPipeA`, `ConnectNamedPipe` | `game_link.cpp`, `game_link_mock.cpp`, `squirrel/fpanim_link.cpp` |
| `portable::SharedMemory::*` | `CreateFileMapping*`, `OpenFileMappingW`, `MapViewOfFile*` | `copy_data.cpp`, `game_frame_ring.cpp`, `game_frame_share.h` |
| `portable::NowMs()` / `LocalTimeString()` / `SleepMs()` | `GetTickCount*`, `GetLocalTime`, `Sleep` | 38 timer rows in section 8 |
| `platform::MessageBox()` | raw `MessageBox` | `render.cpp`, `shader.cpp`, `vao.cpp` (the three small-tool callers) |
| `platform::SetTitle()` | `SetWindowTextW` | `main_frame.cpp` (P0) |

Two of the rows above (`ExecutablePath`, `UserDataDir`) also need a **Windows**
body; both are written in this lane only if the main line wants them, and the
lane's `portable` headers are POSIX-plus-shared-signature until then. The point
of the header is that P5's diff is a rename, not a redesign.

## 8. Inventory (generated)

### 8.1 `#include <windows.h>` and friends, outside P0

<!-- BEGIN auto:includes -->

| file | line | header |
|---|---|---|
| `src/archive_browser.cpp` | 23 | `windows.h` |
| `src/archive_browser_ui.cpp` | 13 | `windows.h` |
| `src/bg_dxt32.c` | 15 | `windows.h` |
| `src/bg_render.cpp` | 16 | `windows.h` |
| `src/bgm_player.cpp` | 4 | `windows.h` |
| `src/bgm_player.cpp` | 5 | `mmsystem.h` |
| `src/character_instance.cpp` | 15 | `windows.h` |
| `src/copy_data.cpp` | 4 | `windows.h` |
| `src/file_backup.cpp` | 4 | `windows.h` |
| `src/fpanim_install.cpp` | 9 | `windows.h` |
| `src/fpanim_install.cpp` | 10 | `tlhelp32.h` |
| `src/fpanim_install_panel.cpp` | 14 | `windows.h` |
| `src/framedata_load.cpp` | 7 | `windows.h` |
| `src/framestate.cpp` | 8 | `windows.h` |
| `src/game_frame_ring.cpp` | 4 | `windows.h` |
| `src/game_link.cpp` | 4 | `windows.h` |
| `src/game_link.cpp` | 5 | `tlhelp32.h` |
| `src/game_link_cli.cpp` | 64 | `windows.h` |
| `src/game_link_mock.cpp` | 8 | `windows.h` |
| `src/game_link_panel.cpp` | 9 | `windows.h` |
| `src/game_table.cpp` | 4 | `windows.h` |
| `src/han2_browser.cpp` | 13 | `windows.h` |
| `src/han2_pac_window.cpp` | 24 | `windows.h` |
| `src/i18n.cpp` | 4 | `windows.h` |
| `src/ini.cpp` | 14 | `windows.h` |
| `src/main_frame.cpp` | 50 | `windows.h` |
| `src/mbaacc_pack.cpp` | 3 | `windows.h` |
| `src/mbaacc_package.cpp` | 7 | `windows.h` |
| `src/mbaacc_package_cli.cpp` | 15 | `windows.h` |
| `src/misc.cpp` | 2 | `windows.h` |
| `src/pattex.cpp` | 7 | `windows.h` |
| `src/pattool.cpp` | 27 | `windows.h` |
| `src/png_writer.cpp` | 3 | `windows.h` |
| `src/png_writer.cpp` | 4 | `wincodec.h` |
| `src/png_writer.cpp` | 5 | `shobjidl.h` |
| `src/render.cpp` | 12 | `windows.h` |
| `src/shader.cpp` | 7 | `windows.h` |
| `src/startup_args.cpp` | 25 | `windows.h` |
| `src/tag_panel.cpp` | 14 | `windows.h` |
| `src/vao.cpp` | 4 | `windows.h` |
| `src/vectors.cpp` | 8 | `windows.h` |
| `src/PatEditor/pat_partset_renumber.cpp` | 12 | `windows.h` |
| `src/authoring/authoring_live_ui.cpp` | 8 | `windows.h` |
| `src/authoring/authoring_tuning_ui.cpp` | 19 | `windows.h` |
| `src/authoring/authoring_window.cpp` | 13 | `windows.h` |
| `src/authoring/game_launcher.cpp` | 5 | `windows.h` |
| `src/authoring/game_launcher.cpp` | 6 | `tlhelp32.h` |
| `src/authoring/game_view.cpp` | 10 | `windows.h` |
| `src/authoring/se_browser.cpp` | 12 | `windows.h` |
| `src/authoring/se_browser.cpp` | 13 | `mmsystem.h` |
| `src/background/bg_browser.cpp` | 9 | `windows.h` |
| `src/background/bg_gametex.cpp` | 14 | `windows.h` |
| `src/background/bg_project.cpp` | 12 | `windows.h` |
| `src/cgm/cgm_export.cpp` | 5 | `windows.h` |
| `src/cgm/cgm_project.cpp` | 5 | `windows.h` |
| `src/cgm/cgmtex.cpp` | 17 | `windows.h` |
| `src/cgm/fxr_cli.cpp` | 19 | `io.h` |
| `src/cmdfile/cmd_io.cpp` | 3 | `windows.h` |
| `src/fbarc/fb_archive.cpp` | 249 | `windows.h` |
| `src/fbarc/fbarctool.cpp` | 252 | `windows.h` |
| `src/fbarc/fbchartool.cpp` | 36 | `windows.h` |
| `src/han2/han2tool.cpp` | 33 | `windows.h` |
| `src/modproject/hmod_ui.cpp` | 16 | `windows.h` |
| `src/modproject/hmodtool.cpp` | 13 | `windows.h` |
| `src/parts/pat_renumber.cpp` | 7 | `windows.h` |
| `src/pcmod/pcmod_export_ui.cpp` | 17 | `windows.h` |
| `src/pcmod/pcmodtool.cpp` | 22 | `windows.h` |
| `src/platform/platform.cpp` | 24 | `windows.h` |
| `src/platform/platform_message.h` | 17 | `windows.h` |
| `src/squirrel/fpanim_link.cpp` | 9 | `windows.h` |
| `src/squirrel/ptcn_stack.cpp` | 9 | `windows.h` |
| `src/squirrel/script_panel.cpp` | 37 | `windows.h` |
| `src/squirrel/sqtool.cpp` | 26 | `windows.h` |
| `src/stage3d/s3d_view.cpp` | 42 | `windows.h` |
| `src/tag_tuning/tag_migrate.cpp` | 5 | `windows.h` |
| `src/tag_tuning/tag_sidecar.cpp` | 4 | `windows.h` |

<!-- END auto:includes -->

### 8.2 Paths

<!-- BEGIN auto:paths -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/archive_browser.cpp` | 190 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/archive_browser.cpp` | 1365 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/archive_browser_ui.cpp` | 636 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/archive_browser_ui.cpp` | 641 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/authoring/authoring_window.cpp` | 78 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/authoring/game_launcher.cpp` | 194 | `GetEnvironmentStringsW` | `environ` |
| `src/background/bg_gametex.cpp` | 141 | `GetModuleHandleA` | Windows-only branch (`GetModuleHandle` for resource lookup) |
| `src/background/bg_gametex.cpp` | 251 | `GetModuleFileNameA` | `portable::ExecutablePath()` (readlink /proc/self/exe) |
| `src/background/bg_gametex.cpp` | 255 | `GetFileAttributesA` | `std::filesystem::status()` |
| `src/background/bg_gametex.cpp` | 257 | `GetTempPathA` | `std::filesystem::temp_directory_path()` |
| `src/background/bg_gametex.cpp` | 287 | `CreateDirectoryA` | `std::filesystem::create_directories()` |
| `src/background/bg_gametex.cpp` | 328 | `DeleteFileA` | `std::filesystem::remove()` |
| `src/background/bg_gametex.cpp` | 329 | `DeleteFileA` | `std::filesystem::remove()` |
| `src/bg_dxt32.c` | 40 | `GetModuleHandleA` | Windows-only branch (`GetModuleHandle` for resource lookup) |
| `src/bg_render.cpp` | 81 | `GetModuleHandleW` | Windows-only branch |
| `src/cgm/cgmtex.cpp` | 49 | `GetTempPathA` | `std::filesystem::temp_directory_path()` |
| `src/cmdfile/cmd_io.cpp` | 55 | `GetFileAttributesA` | `std::filesystem::status()` |
| `src/cmdfile/cmd_io.cpp` | 65 | `CreateDirectoryA` | `std::filesystem::create_directories()` |
| `src/cmdfile/cmd_io.cpp` | 91 | `DeleteFileA` | `std::filesystem::remove()` |
| `src/file_backup.cpp` | 30 | `GetFileAttributesA` | `std::filesystem::status()` |
| `src/file_backup.cpp` | 38 | `GetFileAttributesA` | `std::filesystem::status()` |
| `src/file_backup.cpp` | 44 | `CreateDirectoryA` | `std::filesystem::create_directories()` |
| `src/file_backup.cpp` | 79 | `FindFirstFileA` | `std::filesystem::directory_iterator()` |
| `src/file_backup.cpp` | 84 | `FindNextFileA` | `std::filesystem::directory_iterator()` |
| `src/file_backup.cpp` | 85 | `FindClose` | `std::filesystem` iterator scope |
| `src/file_backup.cpp` | 120 | `CopyFileA` | `std::filesystem::copy_file()` |
| `src/file_backup.cpp` | 130 | `DeleteFileA` | `std::filesystem::remove()` |
| `src/fpanim_install.cpp` | 240 | `GetFileAttributesW` | `std::filesystem::status()` |
| `src/fpanim_install.cpp` | 308 | `GetModuleFileNameW` | `portable::ExecutablePath()` |
| `src/fpanim_install.cpp` | 332 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/fpanim_install.cpp` | 339 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/game_link.cpp` | 770 | `GetFileAttributesExA` | `std::filesystem::status()` |
| `src/game_link.cpp` | 829 | `GetFileAttributesExA` | `std::filesystem::status()` |
| `src/game_table.cpp` | 113 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/han2_browser.cpp` | 31 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/i18n.cpp` | 71 | `GetCurrentDirectoryA` | `std::filesystem::current_path()` |
| `src/mbaacc_package.cpp` | 71 | `GetShortPathNameW` | no equivalent; only used for a Windows-only display path |
| `src/mbaacc_package.cpp` | 115 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/mbaacc_package.cpp` | 116 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/misc.cpp` | 181 | `MoveFileExA` | `portable::ReplaceFileAtomic()` |
| `src/misc.cpp` | 194 | `DeleteFileA` | `std::filesystem::remove()` |
| `src/png_writer.cpp` | 146 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/png_writer.cpp` | 147 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/png_writer.cpp` | 147 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/png_writer.cpp` | 225 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/png_writer.cpp` | 231 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/png_writer.cpp` | 232 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/squirrel/script_panel.cpp` | 159 | `GetModuleFileNameW` | `portable::ExecutablePath()` |
| `src/tag_tuning/tag_migrate.cpp` | 242 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/tag_tuning/tag_sidecar.cpp` | 308 | `GetFileAttributesExW` | `std::filesystem::status()` |
| `src/tag_tuning/tag_sidecar.cpp` | 367 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/tag_tuning/tag_sidecar.cpp` | 368 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/tag_tuning/tag_sidecar.cpp` | 369 | `DeleteFileW` | `std::filesystem::remove()` |
| `src/ui/png_export_impl.h` | 293 | `MoveFileExW` | `portable::ReplaceFileAtomic()` |
| `src/ui/png_export_impl.h` | 294 | `DeleteFileW` | `std::filesystem::remove()` |

<!-- END auto:paths -->

### 8.3 Encoding

<!-- BEGIN auto:encoding -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/PatEditor/pat_partset_fb.cpp` | 134 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/PatEditor/pat_partset_renumber.cpp` | 19 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/PatEditor/pat_partset_renumber.cpp` | 20 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/PatEditor/pat_partset_renumber.cpp` | 21 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/PatEditor/pat_texture_pane.cpp` | 154 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/PatEditor/pat_texture_pane.cpp` | 161 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/PatEditor/pat_texture_pane.cpp` | 206 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/archive_browser_ui.cpp` | 623 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/background/bg_browser.cpp` | 31 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/background/bg_browser.cpp` | 33 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/background/bg_browser.cpp` | 34 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/background/bg_browser.cpp` | 36 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/background/bg_project.cpp` | 22 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/background/bg_project.cpp` | 24 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/background/bg_project.cpp` | 25 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/background/bg_project.cpp` | 27 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cgm/cgm_ccpal.cpp` | 44 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_export.cpp` | 28 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cgm/cgm_export.cpp` | 29 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cgm/cgm_export.cpp` | 30 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_export.cpp` | 32 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_export.cpp` | 130 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_export.cpp` | 130 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_folder.cpp` | 65 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_folder.cpp` | 69 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_folder.cpp` | 173 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_folder.cpp` | 192 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_folder.cpp` | 192 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_folder.cpp` | 302 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_palette.cpp` | 13 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 18 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cgm/cgm_project.cpp` | 19 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cgm/cgm_project.cpp` | 20 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_project.cpp` | 22 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/cgm/cgm_project.cpp` | 23 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 24 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cgm/cgm_project.cpp` | 25 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cgm/cgm_project.cpp` | 60 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 100 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 103 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_project.cpp` | 128 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 129 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 131 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_project.cpp` | 135 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 160 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 161 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/cgm/cgm_project.cpp` | 175 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/cgm/cgm_project.cpp` | 177 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 192 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_project.cpp` | 206 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 109 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 114 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 141 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 142 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 155 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 236 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 329 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 666 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 699 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 756 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgm_window.cpp` | 756 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 761 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_window.cpp` | 762 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_window.cpp` | 765 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_window.cpp` | 767 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_window.cpp` | 773 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgm_window.cpp` | 812 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 1008 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 1117 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgm_window.cpp` | 1519 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/cgm/cgmtool.cpp` | 83 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 41 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 59 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 92 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 191 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 203 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 204 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 205 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 208 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 211 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgmtool_ops.cpp` | 212 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/cgm/cgmtool_ops.cpp` | 281 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 295 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 305 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 308 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 329 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cgm/cgmtool_ops.cpp` | 372 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/cmdfile/cmd_io.cpp` | 14 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cmdfile/cmd_io.cpp` | 17 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cmdfile/cmd_io.cpp` | 18 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cmdfile/cmd_io.cpp` | 21 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cmdfile/cmd_io.cpp` | 28 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cmdfile/cmd_io.cpp` | 31 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/cmdfile/cmd_io.cpp` | 33 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/cmdfile/cmd_io.cpp` | 37 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fb_archive.cpp` | 253 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/fbarc/fb_archive.cpp` | 255 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/fbarc/fb_archive.cpp` | 256 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fb_archive.cpp` | 258 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fbarctool.cpp` | 259 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fbarctool.cpp` | 261 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fbchartool.cpp` | 726 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fbarc/fbchartool.cpp` | 727 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fpanim_install.cpp` | 190 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/fpanim_install.cpp` | 192 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/fpanim_install.cpp` | 198 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fpanim_install.cpp` | 200 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/fpanim_install_panel.cpp` | 76 | `_wfopen` | `std::fopen` on the UTF-8 path |
| `src/han2_export.cpp` | 188 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/han2_export.cpp` | 188 | `_wfopen` | `std::fopen` on the UTF-8 path |
| `src/han2_inspector.cpp` | 380 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/han2_pac_window.cpp` | 375 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/main_frame.cpp` | 563 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/mbaacc_pack.cpp` | 35 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_pack.cpp` | 37 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_pack.cpp` | 38 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_pack.cpp` | 40 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 29 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package.cpp` | 31 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package.cpp` | 38 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 40 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 51 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package.cpp` | 53 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package.cpp` | 63 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 66 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 73 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 571 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package.cpp` | 573 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package_cli.cpp` | 24 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package_cli.cpp` | 26 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/mbaacc_package_cli.cpp` | 93 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package_cli.cpp` | 95 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package_cli.cpp` | 115 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/mbaacc_package_cli.cpp` | 117 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 62 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 68 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 80 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 87 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/misc.cpp` | 92 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/misc.cpp` | 109 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 117 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/misc.cpp` | 123 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/misc.cpp` | 128 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/modproject/hmod_ui.cpp` | 176 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/modproject/hmodtool.cpp` | 24 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/modproject/hmodtool.cpp` | 26 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/parts/pat_renumber.cpp` | 127 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/parts/pat_renumber.cpp` | 129 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/parts/pat_renumber.cpp` | 130 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/parts/pat_renumber.cpp` | 131 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/parts/pat_renumber.cpp` | 136 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/parts/pat_renumber.cpp` | 144 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/parts/pat_renumber.cpp` | 148 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/parts/pat_renumber.cpp` | 163 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/parts/pat_renumber.cpp` | 165 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/parts/pat_renumber.cpp` | 235 | `Utf8ToAnsi` | `portable::Utf8ToCp` |
| `src/pattool.cpp` | 40 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 41 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/pattool.cpp` | 42 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/pattool.cpp` | 47 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 143 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 148 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 149 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 150 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 150 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/pattool.cpp` | 161 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 184 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 267 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 269 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 300 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 301 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 301 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/pattool.cpp` | 302 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/pattool.cpp` | 302 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/pcmod/pcmod_export_ui.cpp` | 327 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/pcmod/pcmodtool.cpp` | 31 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/pcmod/pcmodtool.cpp` | 33 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/png_writer.cpp` | 27 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 30 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/png_writer.cpp` | 32 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/png_writer.cpp` | 36 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/png_writer.cpp` | 39 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/png_writer.cpp` | 41 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/png_writer.cpp` | 45 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/png_writer.cpp` | 48 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/png_writer.cpp` | 50 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/png_writer.cpp` | 51 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/png_writer.cpp` | 57 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 75 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 98 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 120 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 185 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 253 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.cpp` | 264 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/png_writer.h` | 28 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/png_writer.h` | 29 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/png_writer.h` | 30 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/squirrel/fpanim_link.cpp` | 165 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/squirrel/fpanim_link.cpp` | 167 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/stage3d/s3d_view.cpp` | 528 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/stage3d/s3d_view.cpp` | 529 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/startup_args.cpp` | 49 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/startup_args.cpp` | 51 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/startup_args.cpp` | 561 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/startup_args.cpp` | 561 | `_wfopen` | `std::fopen` on the UTF-8 path |
| `src/startup_args.cpp` | 581 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/startup_args.cpp` | 581 | `_wfopen` | `std::fopen` on the UTF-8 path |
| `src/ui/main_menu_impl.h` | 542 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/main_menu_impl.h` | 625 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/package_tools_impl.h` | 34 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/pat_editor_impl.h` | 40 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/ui/pat_editor_impl.h` | 42 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/ui/pat_editor_impl.h` | 43 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/ui/pat_editor_impl.h` | 45 | `WideCharToMultiByte` | `portable::WideToUtf8` / no-op |
| `src/ui/pat_editor_impl.h` | 52 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/ui/pat_editor_impl.h` | 53 | `MultiByteToWideChar` | `portable::CpToUtf8` / no-op (UTF-8 everywhere) |
| `src/ui/pat_editor_impl.h` | 54 | `WideToUtf8` | `portable::WideToUtf8` |
| `src/ui/pat_editor_impl.h` | 437 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/png_export_impl.h` | 213 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/png_export_impl.h` | 285 | `Utf8ToWide` | `portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`) |
| `src/ui/png_export_impl.h` | 316 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/png_export_impl.h` | 334 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/png_export_impl.h` | 335 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |
| `src/ui/png_export_impl.h` | 337 | `AnsiToUtf8` | `portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through) |

<!-- END auto:encoding -->

### 8.4 Registry and profile (the ini)

<!-- BEGIN auto:registry -->
<!-- END auto:registry -->

<!-- BEGIN auto:profile -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/archive_browser.cpp` | 200 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser.cpp` | 201 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/archive_browser.cpp` | 204 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser.cpp` | 212 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/archive_browser.cpp` | 213 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/archive_browser.cpp` | 216 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/archive_browser.cpp` | 902 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser.cpp` | 906 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser.cpp` | 930 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser.cpp` | 972 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser_ui.cpp` | 636 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/archive_browser_ui.cpp` | 641 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/character_instance.cpp` | 70 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/character_instance.cpp` | 79 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/character_instance.cpp` | 88 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/character_instance.cpp` | 124 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/character_instance.cpp` | 130 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/character_instance.cpp` | 140 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/game_link_cli.cpp` | 117 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/game_link_cli.cpp` | 123 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/game_table.cpp` | 117 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/game_table.cpp` | 123 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/han2_browser.cpp` | 52 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/han2_browser.cpp` | 53 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/han2_browser.cpp` | 55 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/han2_browser.cpp` | 61 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/han2_browser.cpp` | 62 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/han2_browser.cpp` | 63 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/i18n.cpp` | 74 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/i18n.cpp` | 75 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/ini.cpp` | 219 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/ini.cpp` | 228 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ini.cpp` | 235 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ini.cpp` | 244 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/ini.cpp` | 248 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ini.cpp` | 261 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/ini.cpp` | 265 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ini.cpp` | 346 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ini.cpp` | 390 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/ini.cpp` | 400 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/ini.cpp` | 407 | `WritePrivateProfileStringA` | `portable::ini::SetString()` |
| `src/squirrel/ptcn_stack.cpp` | 19 | `GetPrivateProfileIntA` | `portable::ini::GetInt()` (same parser as `portable::ini`) |
| `src/squirrel/ptcn_stack.cpp` | 24 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |
| `src/ui/han2_ui_impl.h` | 111 | `GetPrivateProfileStringA` | `portable::ini::GetString()` |

<!-- END auto:profile -->

### 8.5 Steam installs

<!-- BEGIN auto:steam -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/cgm/cgm_window.cpp` | 1372 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/cgm/cgm_window.cpp` | 1373 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/fbarc/fb_modern.cpp` | 222 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/fpanim_install_panel.cpp` | 53 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/fpanim_install_panel.cpp` | 55 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 43 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 48 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 66 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 146 | `SteamApps` | `portable::SteamLibraries()` |
| `src/game_table.cpp` | 156 | `libraryfolders.vdf` | `portable::SteamLibraries()` parses it identically |
| `src/game_table.cpp` | 156 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 162 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/game_table.cpp` | 165 | `appmanifest_` | `portable::SteamApps()` (same parsing) |
| `src/game_table.cpp` | 183 | `SteamApps` | `portable::SteamLibraries()` |
| `src/game_table.h` | 46 | `libraryfolders.vdf` | `portable::SteamLibraries()` parses it identically |
| `src/squirrel/script_panel.cpp` | 418 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/squirrel/script_panel.cpp` | 419 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/ui/main_frame_hotsave.cpp` | 146 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |
| `src/ui/main_frame_hotsave.cpp` | 147 | `steamapps` | `portable::SteamLibraries()` also returns `steamapps` |

<!-- END auto:steam -->

### 8.6 Message boxes

<!-- BEGIN auto:message -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/game_frame_ring.cpp` | 35 | `LocalFree` | `std::free` (only around SDDL/psid Win32 allocations) |
| `src/game_frame_ring.cpp` | 53 | `LocalFree` | `std::free` (only around SDDL/psid Win32 allocations) |
| `src/platform/platform_message.h` | 18 | `MessageBox` | `platform::MessageBox()` |
| `src/platform/platform_message.h` | 27 | `MessageBox` | `platform::MessageBox()` |
| `src/platform/platform_message.h` | 37 | `MessageBoxA` | `platform::MessageBox()` (SDL_ShowSimpleMessageBox, logged when headless) |
| `src/render.cpp` | 316 | `MessageBox` | `platform::MessageBox()` |
| `src/render.cpp` | 339 | `MessageBox` | `platform::MessageBox()` |
| `src/render.cpp` | 464 | `MessageBox` | `platform::MessageBox()` |
| `src/shader.cpp` | 53 | `MessageBox` | `platform::MessageBox()` |
| `src/shader.cpp` | 62 | `MessageBox` | `platform::MessageBox()` |
| `src/shader.cpp` | 130 | `MessageBox` | `platform::MessageBox()` |
| `src/vao.cpp` | 37 | `MessageBox` | `platform::MessageBox()` |

<!-- END auto:message -->

### 8.7 Process launch and discovery

<!-- BEGIN auto:process -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/archive_browser.cpp` | 1428 | `GetCurrentProcessId` | `getpid()` |
| `src/archive_browser_ui.cpp` | 623 | `ShellExecuteW` | `portable::OpenFileExternally()` (`xdg-open`) |
| `src/authoring/game_launcher.cpp` | 29 | `OpenProcess` | `portable::ProcessAlive(pid)` (`kill(pid, 0)`) |
| `src/authoring/game_launcher.cpp` | 77 | `CreateToolhelp32Snapshot` | `/proc/<pid>/cmdline` scan (`portable::FindProcess`) |
| `src/authoring/game_launcher.cpp` | 81 | `Process32FirstW` | `/proc/<pid>/cmdline` scan |
| `src/authoring/game_launcher.cpp` | 81 | `Process32NextW` | `/proc/<pid>/cmdline` scan |
| `src/authoring/game_launcher.cpp` | 202 | `CreateProcessW` | `portable::Spawn()` (`posix_spawnp`) or Windows-only game launcher |
| `src/authoring/game_launcher.cpp` | 261 | `GetExitCodeProcess` | `portable::WaitExit()` |
| `src/authoring/game_launcher.cpp` | 305 | `OpenProcess` | `portable::ProcessAlive(pid)` (`kill(pid, 0)`) |
| `src/authoring/game_launcher.cpp` | 310 | `GetExitCodeProcess` | `portable::WaitExit()` |
| `src/authoring/game_launcher.cpp` | 334 | `OpenProcess` | `portable::ProcessAlive(pid)` (`kill(pid, 0)`) |
| `src/authoring/game_launcher.cpp` | 337 | `TerminateProcess` | `portable::Kill()` |
| `src/background/bg_gametex.cpp` | 259 | `GetCurrentProcessId` | `getpid()` |
| `src/background/bg_gametex.cpp` | 303 | `CreateProcessA` | `portable::Spawn()` (`posix_spawnp`) or Windows-only game launcher |
| `src/background/bg_gametex.cpp` | 305 | `GetExitCodeProcess` | `portable::WaitExit()` |
| `src/cgm/cgmtex.cpp` | 50 | `GetCurrentProcessId` | `getpid()` |
| `src/fpanim_install.cpp` | 285 | `CreateToolhelp32Snapshot` | `/proc/<pid>/cmdline` scan (`portable::FindProcess`) |
| `src/fpanim_install.cpp` | 290 | `Process32FirstW` | `/proc/<pid>/cmdline` scan |
| `src/fpanim_install.cpp` | 290 | `Process32NextW` | `/proc/<pid>/cmdline` scan |
| `src/fpanim_install.cpp` | 294 | `OpenProcess` | `portable::ProcessAlive(pid)` (`kill(pid, 0)`) |
| `src/game_frame_ring.cpp` | 25 | `GetCurrentProcess` | `getpid()` (only in Win32 DACL code) |
| `src/game_frame_ring.cpp` | 25 | `OpenProcessToken` | not needed (no DACL on POSIX mappings) |
| `src/game_link.cpp` | 105 | `CreateToolhelp32Snapshot` | `/proc/<pid>/cmdline` scan (`portable::FindProcess`) |
| `src/game_link.cpp` | 109 | `Process32FirstW` | `/proc/<pid>/cmdline` scan |
| `src/game_link.cpp` | 109 | `Process32NextW` | `/proc/<pid>/cmdline` scan |
| `src/game_link_mock.cpp` | 72 | `GetCurrentProcessId` | `getpid()` |
| `src/game_link_panel.cpp` | 48 | `OpenProcess` | `portable::ProcessAlive(pid)` (`kill(pid, 0)`) |
| `src/misc.cpp` | 149 | `GetCurrentProcessId` | `getpid()` |
| `src/modproject/hmod_ui.cpp` | 320 | `ShellExecuteW` | `portable::OpenFileExternally()` (`xdg-open`) |
| `src/pattex.cpp` | 29 | `GetCurrentProcessId` | `getpid()` |
| `src/stage3d/s3d_view.cpp` | 537 | `CreateProcessW` | `portable::Spawn()` (`posix_spawnp`) or Windows-only game launcher |

<!-- END auto:process -->

### 8.8 Named pipes

<!-- BEGIN auto:pipe -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/authoring/game_launcher.cpp` | 185 | `CreatePipe` | `pipe(2)` |
| `src/authoring/game_launcher.cpp` | 228 | `PeekNamedPipe` | `portable::pipe::BytesAvailable()` (`ioctl FIONREAD`) |
| `src/game_link.cpp` | 702 | `PeekNamedPipe` | `portable::pipe::BytesAvailable()` (`ioctl FIONREAD`) |
| `src/game_link_mock.cpp` | 256 | `CreateNamedPipeA` | `portable::pipe::CreateServer()` (AF_UNIX in `$XDG_RUNTIME_DIR`) |
| `src/game_link_mock.cpp` | 282 | `DisconnectNamedPipe` | `portable::pipe::CloseClient()` |
| `src/game_link_mock.cpp` | 295 | `ConnectNamedPipe` | `portable::pipe::Accept()` |
| `src/game_link_mock.cpp` | 301 | `DisconnectNamedPipe` | `portable::pipe::CloseClient()` |
| `src/game_link_mock.cpp` | 437 | `PeekNamedPipe` | `portable::pipe::BytesAvailable()` (`ioctl FIONREAD`) |
| `src/squirrel/fpanim_link.cpp` | 202 | `WaitNamedPipeW` | `portable::pipe::WaitConnect()` |

<!-- END auto:pipe -->

### 8.9 Shared memory and the clipboard

<!-- BEGIN auto:clipboard -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/copy_data.cpp` | 42 | `GetSystemInfo` | `sysconf(_SC_PAGESIZE)` |
| `src/copy_data.cpp` | 49 | `CreateFileMappingW` | `portable::SharedMemory::Create()` (`shm_open`) |
| `src/copy_data.cpp` | 56 | `MapViewOfFileEx` | `mmap(MAP_FIXED_NOREPLACE)` at the same address |
| `src/copy_data.cpp` | 79 | `VirtualAlloc` | `mmap(MAP_ANONYMOUS)` |
| `src/game_frame_ring.cpp` | 52 | `CreateFileMappingW` | `portable::SharedMemory::Create()` (`shm_open`) |
| `src/game_frame_ring.cpp` | 55 | `MapViewOfFile` | `mmap()` |
| `src/game_frame_ring.cpp` | 58 | `UnmapViewOfFile` | `munmap()` |
| `src/game_frame_ring.cpp` | 68 | `UnmapViewOfFile` | `munmap()` |
| `src/game_frame_ring.cpp` | 79 | `OpenFileMappingW` | `portable::SharedMemory::Open()` |
| `src/game_frame_ring.cpp` | 81 | `MapViewOfFile` | `mmap()` |
| `src/game_frame_ring.cpp` | 86 | `UnmapViewOfFile` | `munmap()` |
| `src/game_frame_ring.cpp` | 99 | `UnmapViewOfFile` | `munmap()` |

<!-- END auto:clipboard -->

### 8.10 Timers and waits

<!-- BEGIN auto:timer -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/authoring/authoring_live_ui.cpp` | 24 | `GetTickCount64` | `portable::NowMs()` |
| `src/authoring/authoring_tuning_ui.cpp` | 272 | `GetLocalTime` | `std::localtime` (`localtime_r`) |
| `src/authoring/authoring_window.cpp` | 68 | `GetTickCount64` | `portable::NowMs()` |
| `src/authoring/authoring_window.cpp` | 809 | `GetLocalTime` | `std::localtime` (`localtime_r`) |
| `src/authoring/game_launcher.cpp` | 17 | `GetTickCount64` | `portable::NowMs()` |
| `src/authoring/game_launcher.cpp` | 266 | `Sleep` | `std::this_thread::sleep_for` |
| `src/authoring/game_launcher.cpp` | 279 | `Sleep` | `std::this_thread::sleep_for` |
| `src/authoring/game_view.cpp` | 28 | `GetTickCount64` | `portable::NowMs()` |
| `src/game_frame_ring.cpp` | 93 | `GetTickCount64` | `portable::NowMs()` |
| `src/game_frame_ring.cpp` | 109 | `GetTickCount64` | `portable::NowMs()` |
| `src/game_link.cpp` | 17 | `GetTickCount64` | `portable::NowMs()` |
| `src/game_link_cli.cpp` | 269 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 291 | `GetLocalTime` | `std::localtime` (`localtime_r`) |
| `src/game_link_cli.cpp` | 322 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 330 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 333 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 340 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 346 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 351 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 384 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 397 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 564 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 570 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 580 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 584 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 614 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 622 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 624 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_cli.cpp` | 625 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_cli.cpp` | 660 | `GetTickCount` | `portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`) |
| `src/game_link_mock.cpp` | 18 | `GetTickCount64` | `portable::NowMs()` |
| `src/game_link_mock.cpp` | 248 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_mock.cpp` | 297 | `Sleep` | `std::this_thread::sleep_for` |
| `src/game_link_mock.cpp` | 444 | `Sleep` | `std::this_thread::sleep_for` |
| `src/han2_pac_window.cpp` | 89 | `GetTickCount64` | `portable::NowMs()` |
| `src/misc.cpp` | 150 | `GetTickCount64` | `portable::NowMs()` |
| `src/misc.cpp` | 189 | `Sleep` | `std::this_thread::sleep_for` |
| `src/tag_panel.cpp` | 82 | `GetTickCount64` | `portable::NowMs()` |

<!-- END auto:timer -->

### 8.11 Sync objects and handles

<!-- BEGIN auto:sync -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/authoring/game_launcher.cpp` | 35 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 89 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 109 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 204 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 206 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/authoring/game_launcher.cpp` | 207 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 259 | `WaitForSingleObject` | `poll()` on the pipe fd / `waitpid()` on the child |
| `src/authoring/game_launcher.cpp` | 281 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 282 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 283 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 307 | `WaitForSingleObject` | `poll()` on the pipe fd / `waitpid()` on the child |
| `src/authoring/game_launcher.cpp` | 308 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 311 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/authoring/game_launcher.cpp` | 338 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/background/bg_gametex.cpp` | 304 | `WaitForSingleObject` | `poll()` on the pipe fd / `waitpid()` on the child |
| `src/background/bg_gametex.cpp` | 306 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/cmdfile/cmd_io.cpp` | 65 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/cmdfile/cmd_io.cpp` | 80 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/cmdfile/cmd_io.cpp` | 89 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/copy_data.cpp` | 51 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/copy_data.cpp` | 78 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/file_backup.cpp` | 44 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/fpanim_install.cpp` | 299 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/fpanim_install.cpp` | 301 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/fpanim_install.cpp` | 334 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/fpanim_install.cpp` | 340 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/game_frame_ring.cpp` | 38 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 54 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/game_frame_ring.cpp` | 56 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 58 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 69 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 80 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/game_frame_ring.cpp` | 82 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 86 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_frame_ring.cpp` | 100 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_link.cpp` | 111 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_link.cpp` | 477 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/game_link.cpp` | 518 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_link_mock.cpp` | 265 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_link_mock.cpp` | 283 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/game_link_mock.cpp` | 295 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/game_link_panel.cpp` | 58 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/mbaacc_package.cpp` | 114 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/misc.cpp` | 22 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/misc.cpp` | 29 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/misc.cpp` | 34 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/misc.cpp` | 64 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/misc.cpp` | 111 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/misc.cpp` | 171 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/misc.cpp` | 186 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 173 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 179 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 180 | `WaitForSingleObject` | `poll()` on the pipe fd / `waitpid()` on the child |
| `src/squirrel/fpanim_link.cpp` | 187 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 201 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 215 | `CreateEventW` | `portable::pipe` wait via `poll()` |
| `src/squirrel/fpanim_link.cpp` | 221 | `ResetEvent` | `portable::pipe` (no manual-reset object needed) |
| `src/squirrel/fpanim_link.cpp` | 224 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/squirrel/fpanim_link.cpp` | 231 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/squirrel/fpanim_link.cpp` | 232 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/stage3d/s3d_view.cpp` | 539 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/stage3d/s3d_view.cpp` | 542 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/stage3d/s3d_view.cpp` | 543 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/tag_tuning/tag_sidecar.cpp` | 323 | `GetLastError` | `errno` (`portable::LastError()`) |
| `src/tag_tuning/tag_sidecar.cpp` | 337 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/tag_tuning/tag_sidecar.cpp` | 355 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/tag_tuning/tag_sidecar.cpp` | 366 | `CloseHandle` | `close(fd)` / `munmap` |
| `src/ui/png_export_impl.h` | 292 | `CloseHandle` | `close(fd)` / `munmap` |

<!-- END auto:sync -->

### 8.12 File I/O

<!-- BEGIN auto:files -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/authoring/game_launcher.cpp` | 96 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/authoring/game_launcher.cpp` | 100 | `GetFileSizeEx` | `std::filesystem::file_size()` |
| `src/authoring/game_launcher.cpp` | 105 | `SetFilePointerEx` | `std::fseek` |
| `src/authoring/game_launcher.cpp` | 108 | `ReadFile` | `std::fread` / `read(2)` |
| `src/authoring/game_launcher.cpp` | 231 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 24 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 34 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 72 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 82 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool.cpp` | 87 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 123 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool.cpp` | 192 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool.cpp` | 252 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool.cpp` | 264 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool_ops.cpp` | 25 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool_ops.cpp` | 26 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool_ops.cpp` | 43 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool_ops.cpp` | 195 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cgm/cgmtool_ops.cpp` | 220 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool_ops.cpp` | 272 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool_ops.cpp` | 378 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cgm/cgmtool_ops.cpp` | 420 | `ReadFile` | `std::fread` / `read(2)` |
| `src/cmdfile/cmd_io.cpp` | 78 | `CreateFileA` | `std::fstream` / `open(2)` |
| `src/cmdfile/cmd_io.cpp` | 87 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/cmdfile/cmd_io.cpp` | 88 | `FlushFileBuffers` | `fsync(2)` |
| `src/fbarc/fbarctool.cpp` | 64 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/fbarc/fbarctool.cpp` | 84 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/game_link.cpp` | 475 | `CreateFileA` | `std::fstream` / `open(2)` |
| `src/game_link.cpp` | 694 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/game_link.cpp` | 706 | `ReadFile` | `std::fread` / `read(2)` |
| `src/game_link_mock.cpp` | 25 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/game_link_mock.cpp` | 441 | `ReadFile` | `std::fread` / `read(2)` |
| `src/han2/han2tool.cpp` | 65 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/han2/han2tool.cpp` | 85 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/mbaacc_package.cpp` | 109 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/mbaacc_package.cpp` | 112 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/mbaacc_package.cpp` | 113 | `FlushFileBuffers` | `fsync(2)` |
| `src/misc.cpp` | 14 | `CreateFileA` | `std::fstream` / `open(2)` |
| `src/misc.cpp` | 20 | `GetFileSizeEx` | `std::filesystem::file_size()` |
| `src/misc.cpp` | 33 | `ReadFile` | `std::fread` / `read(2)` |
| `src/misc.cpp` | 152 | `CreateFileA` | `std::fstream` / `open(2)` |
| `src/misc.cpp` | 164 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/misc.cpp` | 169 | `FlushFileBuffers` | `fsync(2)` |
| `src/shader.cpp` | 10 | `ReadFile` | `std::fread` / `read(2)` |
| `src/shader.cpp` | 104 | `ReadFile` | `std::fread` / `read(2)` |
| `src/shader.cpp` | 105 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/fpanim_link.cpp` | 199 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/squirrel/fpanim_link.cpp` | 217 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/squirrel/fpanim_link.cpp` | 223 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/script_vm.cpp` | 53 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/script_vm.cpp` | 164 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/script_vm.cpp` | 199 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/script_vm.cpp` | 369 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 76 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 155 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 194 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 370 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 429 | `ReadFile` | `std::fread` / `read(2)` |
| `src/squirrel/sqtool.cpp` | 503 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_panel.cpp` | 116 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_panel.cpp` | 131 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_panel.cpp` | 308 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_panel.cpp` | 575 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 320 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 333 | `ReadFile` | `std::fread` / `read(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 351 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 354 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 361 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 364 | `WriteFile` | `std::fwrite` / `write(2)` |
| `src/tag_tuning/tag_sidecar.cpp` | 365 | `FlushFileBuffers` | `fsync(2)` |
| `src/ui/png_export_impl.h` | 288 | `CreateFileW` | `std::fstream` / `open(2)` |
| `src/ui/png_export_impl.h` | 291 | `WriteFile` | `std::fwrite` / `write(2)` |

<!-- END auto:files -->

### 8.13 Window types and calls

Some of these are the P0 bridge (`platform.cpp` is allowed to know `HWND`),
some are stray.

<!-- BEGIN auto:window -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/background/bg_gametex.cpp` | 120 | `HMODULE` | `dlopen` handle (version resource only) |
| `src/background/bg_gametex.cpp` | 123 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/background/bg_gametex.cpp` | 140 | `DefWindowProcA` | `SDL_ShowSimpleMessageBox` / no-op |
| `src/background/bg_gametex.cpp` | 183 | `SUCCEEDED` | removed with WIC |
| `src/bg_dxt32.c` | 31 | `HMODULE` | `dlopen` handle (version resource only) |
| `src/bg_dxt32.c` | 40 | `DefWindowProcA` | `SDL_ShowSimpleMessageBox` / no-op |
| `src/bg_dxt32.c` | 42 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/bg_dxt32.c` | 68 | `SUCCEEDED` | removed with WIC |
| `src/bg_dxt32.c` | 78 | `SUCCEEDED` | removed with WIC |
| `src/bg_render.cpp` | 38 | `DefWindowProcW` | `SDL_ShowSimpleMessageBox` / no-op |
| `src/bg_render.cpp` | 38 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/bg_render.cpp` | 84 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/fbarc/fbchartool.cpp` | 709 | `CoInitializeEx` | not needed on POSIX |
| `src/han2/han2tool.cpp` | 1216 | `CoInitializeEx` | not needed on POSIX |
| `src/main_frame.cpp` | 848 | `SetWindowTextW` | `platform::SetTitle()` (P0) |
| `src/png_writer.cpp` | 21 | `CoInitializeEx` | not needed on POSIX |
| `src/png_writer.cpp` | 21 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 22 | `CoUninitialize` | not needed on POSIX |
| `src/png_writer.cpp` | 71 | `CoCreateInstance` | removed with WIC (use stb/libpng or SDL_image) |
| `src/png_writer.cpp` | 71 | `IID_PPV_ARGS` | removed with WIC |
| `src/png_writer.cpp` | 94 | `CoCreateInstance` | removed with WIC (use stb/libpng or SDL_image) |
| `src/png_writer.cpp` | 94 | `IID_PPV_ARGS` | removed with WIC |
| `src/png_writer.cpp` | 125 | `CoCreateInstance` | removed with WIC (use stb/libpng or SDL_image) |
| `src/png_writer.cpp` | 127 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 128 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 129 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 130 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 131 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 132 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 133 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 135 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 136 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 139 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 140 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 141 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 142 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 143 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 144 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 194 | `CoCreateInstance` | removed with WIC (use stb/libpng or SDL_image) |
| `src/png_writer.cpp` | 200 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 201 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 202 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 203 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 204 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 205 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 206 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 208 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 209 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 214 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 215 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 217 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 218 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 244 | `CoCreateInstance` | removed with WIC (use stb/libpng or SDL_image) |
| `src/png_writer.cpp` | 254 | `SHCreateItemFromParsingName` | `platform::BrowseForFolderUtf8()` (P0) |
| `src/png_writer.cpp` | 254 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 259 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 261 | `SUCCEEDED` | removed with WIC |
| `src/png_writer.cpp` | 263 | `SUCCEEDED` | removed with WIC |
| `src/startup_args.cpp` | 568 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/startup_args.cpp` | 570 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |
| `src/ui/editor_tools_impl.h` | 28 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 29 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 30 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 44 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 45 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 46 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 661 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/editor_tools_impl.h` | 665 | `GetKeyState` | `SDL_GetModState` / `SDL_GetKeyboardState` |
| `src/ui/workspace_hosts_impl.h` | 853 | `HWND` | `SDL_Window*` (P0 `platform::Window()`) |

<!-- END auto:window -->

The scalar aliases (`DWORD`, `BOOL`, `HANDLE`, `wchar_t`, the `STRUCT` types and
the sentinel constants) are the rest of the Win32 surface, counted per file
below. One `src/platform/portable/win_types.h` header defines them on both
platforms, so nothing here needs a per-site decision; the table exists so an
audit can see nothing was dropped.

<!-- BEGIN auto:types -->

| file | aliases used | portable replacement |
|---|---|---|
| `src/archive_browser.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/archive_browser_ui.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/authoring/authoring_model.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/authoring/authoring_model.h` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/authoring/game_launcher.cpp` | `BOOL`, `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE`, `PROCESSENTRY32W`, `PROCESS_INFORMATION`, `SECURITY_ATTRIBUTES`, `STARTUPINFOW`, `WAIT_OBJECT_0`, `WAIT_TIMEOUT`, `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/authoring/se_browser.cpp` | `LPCWSTR` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/background/bg_gametex.cpp` | `DWORD`, `PROCESS_INFORMATION` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/bg_dxt32.c` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/bgm_player.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/cmdfile/cmd_io.cpp` | `BOOL`, `DWORD`, `ERROR_ALREADY_EXISTS`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/copy_data.cpp` | `DWORD`, `ERROR_ALREADY_EXISTS`, `FILE_MAP_ALL_ACCESS`, `HANDLE`, `INVALID_HANDLE_VALUE`, `PAGE_READWRITE`, `SYSTEM_INFO`, `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/copy_data.h` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/fbarc/fbarctool.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/fbarc/fbchartool.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/file_backup.cpp` | `DWORD`, `ERROR_ALREADY_EXISTS`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/fpanim_install.cpp` | `BOOL`, `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE`, `PROCESSENTRY32W`, `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_frame_ring.cpp` | `DWORD`, `FILE_MAP_ALL_ACCESS`, `FILE_MAP_READ`, `HANDLE`, `INVALID_HANDLE_VALUE`, `LPWSTR`, `MEMORY_BASIC_INFORMATION`, `PAGE_READWRITE`, `SECURITY_ATTRIBUTES` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_link.cpp` | `BOOL`, `DWORD`, `ERROR_PIPE_BUSY`, `HANDLE`, `INVALID_HANDLE_VALUE`, `PROCESSENTRY32W` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_link_cli.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_link_mock.cpp` | `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_link_panel.cpp` | `DWORD`, `HANDLE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/game_table.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/han2_browser.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/i18n.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/main_frame.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/main_frame.h` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/mbaacc_pack.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/mbaacc_package.cpp` | `BOOL`, `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE`, `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/mbaacc_package_cli.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/misc.cpp` | `BOOL`, `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/modproject/hmodtool.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/pcmod/pcmodtool.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/png_writer.cpp` | `DWORD` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/squirrel/fpanim_link.cpp` | `BOOL`, `DWORD`, `ERROR_PIPE_BUSY`, `HANDLE`, `INVALID_HANDLE_VALUE`, `OVERLAPPED`, `WAIT_OBJECT_0`, `WAIT_TIMEOUT` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/squirrel/script_panel.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/stage3d/s3d_view.cpp` | `PROCESS_INFORMATION`, `STARTUPINFOW` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/startup_args.cpp` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/tag_tuning/tag_sidecar.cpp` | `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/ui/editor_tools_impl.h` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/ui/png_export_impl.h` | `BOOL`, `DWORD`, `HANDLE`, `INVALID_HANDLE_VALUE` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |
| `src/ui/workspace_hosts_impl.h` | `wchar_t` | `portable::win_types.h`: `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `SharedMemory`, `wchar_t` -> UTF-8 `std::string` |

<!-- END auto:types -->

### 8.14 Audio

<!-- BEGIN auto:audio -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/authoring/se_browser.cpp` | 178 | `PlaySoundW` | `SDL_LoadWAV` + `SDL_PlayAudioStream` |
| `src/authoring/se_browser.cpp` | 181 | `PlaySoundW` | `SDL_LoadWAV` + `SDL_PlayAudioStream` |
| `src/authoring/se_browser.cpp` | 187 | `PlaySoundW` | `SDL_LoadWAV` + `SDL_PlayAudioStream` |
| `src/bgm_player.cpp` | 159 | `WAVEFORMATEX` | `SDL_AudioSpec` |
| `src/bgm_player.cpp` | 167 | `waveOutOpen` | `SDL_OpenAudioDeviceStream` |
| `src/bgm_player.cpp` | 174 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 186 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 189 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 189 | `waveOutPrepareHeader` | no-op (SDL owns the buffer) |
| `src/bgm_player.cpp` | 190 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 190 | `waveOutWrite` | `SDL_PutAudioStreamData` |
| `src/bgm_player.cpp` | 197 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 197 | `waveOutUnprepareHeader` | no-op |
| `src/bgm_player.cpp` | 205 | `waveOutReset` | `SDL_ClearAudioStream` |
| `src/bgm_player.cpp` | 207 | `WAVEHDR` | `SDL_AudioStream` buffer |
| `src/bgm_player.cpp` | 207 | `waveOutUnprepareHeader` | no-op |
| `src/bgm_player.cpp` | 208 | `waveOutClose` | `SDL_DestroyAudioStream` |

<!-- END auto:audio -->

### 8.15 Version resources

<!-- BEGIN auto:version -->

| file | line | win32 call | portable replacement |
|---|---|---|---|
| `src/fpanim_install.cpp` | 230 | `VerQueryValueW` | Windows-only branch |
| `src/fpanim_install.cpp` | 247 | `GetFileVersionInfoW` | Windows-only branch (version resource) |
| `src/fpanim_install.cpp` | 252 | `VerQueryValueW` | Windows-only branch |

<!-- END auto:version -->
