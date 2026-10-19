#!/usr/bin/env python3
"""Scan src/ for Win32/WinAPI usage and print a deterministic inventory.

Deliverable 1 of the LINUX lane (docs/render/linux.md) needs every
`#include <windows.h>` and every Win32 call outside the files the main agent
rewrites in P0. This script produces that list so the doc is reproducible and
not hand-picked. It only reads files and writes to stdout; run it after a
checkout to refresh the table.

    python3 tools/linux/winapi_inventory.py            # markdown
    python3 tools/linux/winapi_inventory.py --tsv      # file<TAB>line<TAB>call
    python3 tools/linux/winapi_inventory.py --check     # exit 1 if any P0 file is listed
    python3 tools/linux/winapi_inventory.py --write-doc # regenerate the auto
                                                        # tables inside docs/render/linux.md

P0 owns main.cpp, context_gl.h, workspace_viewports.* and filedialog.cpp: those
are excluded here because the main line replaces them with SDL3.
"""

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src")
DOC = os.path.join(ROOT, "docs", "render", "linux.md")

# Files the P0 phase rewrites, so they are out of scope for this inventory.
# src/platform is out of scope for the same reason as the P0 files: it is the
# layer whose *job* is to know the OS. platform.cpp already holds the SDL/Win32
# window bridge and portable/* holds the two bodies of each portable helper.
INCLUDE_DIRS = ("src/platform",)
P0_FILES = {
    "src/main.cpp",
    "src/main.h",
    "src/context_gl.h",
    "src/workspace_viewports.cpp",
    "src/workspace_viewports.h",
    "src/filedialog.cpp",
    "src/filedialog.h",
}

EXT = (".cpp", ".h", ".c", ".inc", ".hpp")

# Sources whose `ReadFile`/`WriteFile` are local helpers over an ifstream, not
# the Win32 calls. Checked: cgm/cgm_acmoon.cpp:23 and hud_colors.cpp:31.
LOCAL_FILE_HELPERS = {"src/cgm/cgm_acmoon.cpp", "src/hud_colors.cpp"}

# Grouped Win32 symbols. A hit is any identifier occurrence; the report keeps
# the first hit per (file, line, symbol) so a line that uses two APIs lists both.
GROUPS = [
    ("paths", [
        "GetCurrentDirectoryA", "GetCurrentDirectoryW", "SetCurrentDirectoryA", "SetCurrentDirectoryW",
        "GetModuleFileNameA", "GetModuleFileNameW", "GetModuleHandleA", "GetModuleHandleW", "GetModuleHandle",
        "GetTempPathA", "GetTempPathW", "GetTempFileNameA", "GetTempFileNameW",
        "CreateDirectoryA", "CreateDirectoryW", "CreateDirectory",
        "GetFileAttributesA", "GetFileAttributesW", "GetFileAttributesExA", "GetFileAttributesExW",
        "FindFirstFileA", "FindFirstFileW", "FindFirstFileExA", "FindFirstFileExW",
        "FindNextFileA", "FindNextFileW", "FindClose",
        "CopyFileA", "CopyFileW", "MoveFileExA", "MoveFileExW", "MoveFileA", "MoveFileW",
        "DeleteFileA", "DeleteFileW", "RemoveDirectoryA", "RemoveDirectoryW",
        "GetFullPathNameA", "GetFullPathNameW", "GetShortPathNameW", "GetLongPathNameW",
        "PathFileExistsA", "PathFileExistsW", "PathAppendW", "PathCombineW", "PathRemoveFileSpecW",
        "SHGetFolderPathA", "SHGetFolderPathW", "SHGetKnownFolderPath", "SHGetSpecialFolderPathA", "SHGetSpecialFolderPathW",
        "GetEnvironmentVariableA", "GetEnvironmentVariableW", "GetEnvironmentStringsW",
    ]),
    ("encoding", [
        "MultiByteToWideChar", "WideCharToMultiByte", "AnsiToUtf8", "Utf8ToWide", "WideToUtf8", "Utf8ToAnsi",
        "AnsiToWide", "WideToAnsi", "CharUpperA", "CharUpperW", "lstrlenA", "lstrlenW",
        "_wfopen", "_wstat", "_waccess", "wsprintfA", "wsprintfW",
    ]),
    ("registry", [
        "RegOpenKeyExA", "RegOpenKeyExW", "RegQueryValueExA", "RegQueryValueExW", "RegGetValueA", "RegGetValueW",
        "RegCloseKey", "RegEnumKeyExA", "RegEnumKeyExW", "HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER",
    ]),
    ("profile", [
        "GetPrivateProfileIntA", "GetPrivateProfileStringA", "GetPrivateProfileStringW",
        "WritePrivateProfileStringA", "WritePrivateProfileStringW",
    ]),
    ("steam", [
        "libraryfolders.vdf", "steamapps", "appmanifest_", "SteamApps",
    ]),
    ("message", [
        "MessageBoxA", "MessageBoxW", "MessageBox", "FormatMessageA", "FormatMessageW", "LocalFree",
    ]),
    ("process", [
        "ShellExecuteA", "ShellExecuteW", "ShellExecuteExA", "ShellExecuteExW", "WinExec",
        "CreateProcessA", "CreateProcessW", "CreateProcessAsUserW", "GetExitCodeProcess", "TerminateProcess",
        "OpenProcess", "OpenProcessToken", "CreateToolhelp32Snapshot", "Process32FirstW", "Process32NextW",
        "CreateThread", "_beginthreadex", "ExitProcess", "GetCurrentProcess", "GetCurrentProcessId",
    ]),
    ("pipe", [
        "CreateNamedPipeA", "CreateNamedPipeW", "ConnectNamedPipe", "DisconnectNamedPipe", "PeekNamedPipe",
        "CreatePipe", "CallNamedPipeA", "CallNamedPipeW", "WaitNamedPipeA", "WaitNamedPipeW",
    ]),
    ("clipboard", [
        "OpenClipboard", "CloseClipboard", "EmptyClipboard", "GetClipboardData", "SetClipboardData",
        "IsClipboardFormatAvailable", "RegisterClipboardFormatA", "RegisterClipboardFormatW",
        "GlobalAlloc", "GlobalFree", "GlobalLock", "GlobalUnlock", "GlobalSize",
        "CreateFileMappingA", "CreateFileMappingW", "CreateFileMapping", "OpenFileMappingA", "OpenFileMappingW",
        "MapViewOfFile", "MapViewOfFileEx", "UnmapViewOfFile", "VirtualAlloc", "VirtualFree",
        "GetSystemInfo", "FILE_MAP_ALL_ACCESS", "FILE_MAP_READ", "PAGE_READWRITE", "ERROR_ALREADY_EXISTS",
    ]),
    ("timer", [
        "SetTimer", "KillTimer", "GetTickCount", "GetTickCount64", "QueryPerformanceCounter",
        "QueryPerformanceFrequency", "timeGetTime", "Sleep", "CreateWaitableTimerW", "SetWaitableTimer",
        "GetSystemTimeAsFileTime", "GetLocalTime", "GetSystemTime",
    ]),
    ("sync", [
        "CreateEventA", "CreateEventW", "SetEvent", "ResetEvent", "WaitForSingleObject", "WaitForMultipleObjects",
        "CreateMutexA", "CreateMutexW", "CreateSemaphoreW", "InitializeCriticalSection", "EnterCriticalSection",
        "LeaveCriticalSection", "DeleteCriticalSection", "CloseHandle", "INVALID_HANDLE_VALUE", "GetLastError",
    ]),
    ("files", [
        "CreateFileA", "CreateFileW", "ReadFile", "WriteFile", "SetFilePointer", "SetFilePointerEx",
        "GetFileSize", "GetFileSizeEx", "FlushFileBuffers", "DeleteFile",
    ]),
    ("window", [
        "CreateWindowExA", "CreateWindowExW", "RegisterClassExA", "RegisterClassExW", "DefWindowProcA", "DefWindowProcW",
        "GetClientRect", "GetWindowRect", "ScreenToClient", "ClientToScreen", "GetForegroundWindow", "SetForegroundWindow",
        "SetWindowTextW", "SetWindowLongPtrW", "GetWindowLongPtrW", "CallWindowProcW", "GetDC", "ReleaseDC",
        "GetSystemMetrics", "GetDpiForWindow", "ShowWindow", "UpdateWindow", "InvalidateRect", "GetKeyState",
        "GetAsyncKeyState", "LoadImageW", "LoadCursorW", "LoadIconW", "DragAcceptFiles", "DragQueryFileW", "DragFinish",
        "GetOpenFileNameA", "GetOpenFileNameW", "GetSaveFileNameA", "GetSaveFileNameW", "SHCreateItemFromParsingName",
        "CoInitializeEx", "CoCreateInstance", "CoUninitialize", "IID_PPV_ARGS", "SUCCEEDED",
        "HWND", "HINSTANCE", "HDC", "HGLRC", "HMODULE",
    ]),
    # Scalar and handle-shaped aliases. These are compatibility shims (typedef to
    # a fixed-width integer or an opaque pointer), not calls, so the doc counts
    # them per file instead of listing every line: a single `portable::win_types`
    # header covers all of them and there is nothing to decide per site.
    ("types", [
        "DWORD", "BOOL", "HANDLE", "LPWSTR", "LPCWSTR", "wchar_t", "PROCESSENTRY32W",
        "SYSTEM_INFO", "OVERLAPPED", "SECURITY_ATTRIBUTES", "STARTUPINFOW", "PROCESS_INFORMATION",
        "INVALID_HANDLE_VALUE", "ERROR_ALREADY_EXISTS", "ERROR_PIPE_BUSY", "WAIT_OBJECT_0", "WAIT_TIMEOUT",
        "FILE_MAP_ALL_ACCESS", "FILE_MAP_READ", "PAGE_READWRITE", "VS_FIXEDFILEINFO", "MEMORY_BASIC_INFORMATION",
    ]),
    ("audio", [
        "PlaySoundA", "PlaySoundW", "PlaySound", "waveOutOpen", "waveOutClose", "waveOutWrite", "waveOutPrepareHeader",
        "waveOutUnprepareHeader", "waveOutReset", "WAVEHDR", "WAVEFORMATEX",
    ]),
    ("version", [
        "GetFileVersionInfoA", "GetFileVersionInfoW", "VerQueryValueA", "VerQueryValueW", "VS_FIXEDFILEINFO",
    ]),
]

INCLUDE_RE = re.compile(r"#\s*include\s*[<\"](windows\.h|windef\.h|winbase\.h|winsock2?\.h|shlobj\.h|shobjidl\.h|wincodec\.h|tlhelp32\.h|mmsystem\.h|process\.h|io\.h)")

# Double-quoted literals, so a metadata string such as "+0x04 T: MapViewOfFile"
# in the generated type tables is not mistaken for a call.
STRING_LITERAL_RE = re.compile(r'"(?:\\.|[^"\\])*"')

# What each symbol becomes on POSIX. This is the P5 switch-over plan, one row per
# call site.  Anything not listed falls back to the group's default below; the
# doc explains the ones that need a decision rather than a mechanical rename.
REPLACEMENT = {
    # paths
    "GetCurrentDirectoryA": "`std::filesystem::current_path()`",
    "GetCurrentDirectoryW": "`std::filesystem::current_path()`",
    "SetCurrentDirectoryA": "`std::filesystem::current_path(p)`",
    "SetCurrentDirectoryW": "`std::filesystem::current_path(p)`",
    "GetModuleFileNameA": "`portable::ExecutablePath()` (readlink /proc/self/exe)",
    "GetModuleFileNameW": "`portable::ExecutablePath()`",
    "GetModuleHandleA": "Windows-only branch (`GetModuleHandle` for resource lookup)",
    "GetModuleHandleW": "Windows-only branch",
    "GetModuleHandle": "Windows-only branch",
    "GetTempPathA": "`std::filesystem::temp_directory_path()`",
    "GetTempPathW": "`std::filesystem::temp_directory_path()`",
    "GetTempFileNameA": "`portable::UniqueTempFile()`",
    "GetTempFileNameW": "`portable::UniqueTempFile()`",
    "CreateDirectoryA": "`std::filesystem::create_directories()`",
    "CreateDirectoryW": "`std::filesystem::create_directories()`",
    "CreateDirectory": "`std::filesystem::create_directories()`",
    "GetFileAttributesA": "`std::filesystem::status()`",
    "GetFileAttributesW": "`std::filesystem::status()`",
    "GetFileAttributesExA": "`std::filesystem::status()`",
    "GetFileAttributesExW": "`std::filesystem::status()`",
    "FindFirstFileA": "`std::filesystem::directory_iterator()`",
    "FindFirstFileW": "`std::filesystem::directory_iterator()`",
    "FindFirstFileExA": "`std::filesystem::directory_iterator()`",
    "FindFirstFileExW": "`std::filesystem::directory_iterator()`",
    "FindNextFileA": "`std::filesystem::directory_iterator()`",
    "FindNextFileW": "`std::filesystem::directory_iterator()`",
    "FindClose": "`std::filesystem` iterator scope",
    "CopyFileA": "`std::filesystem::copy_file()`",
    "CopyFileW": "`std::filesystem::copy_file()`",
    "MoveFileExA": "`portable::ReplaceFileAtomic()`",
    "MoveFileExW": "`portable::ReplaceFileAtomic()`",
    "MoveFileA": "`std::filesystem::rename()`",
    "MoveFileW": "`std::filesystem::rename()`",
    "DeleteFileA": "`std::filesystem::remove()`",
    "DeleteFileW": "`std::filesystem::remove()`",
    "DeleteFile": "`std::filesystem::remove()`",
    "RemoveDirectoryA": "`std::filesystem::remove()`",
    "RemoveDirectoryW": "`std::filesystem::remove()`",
    "GetFullPathNameA": "`std::filesystem::weakly_canonical()`",
    "GetFullPathNameW": "`std::filesystem::weakly_canonical()`",
    "GetShortPathNameW": "no equivalent; only used for a Windows-only display path",
    "GetLongPathNameW": "`std::filesystem::canonical()`",
    "PathFileExistsA": "`std::filesystem::exists()`",
    "PathFileExistsW": "`std::filesystem::exists()`",
    "PathAppendW": "`operator/` on `std::filesystem::path`",
    "PathCombineW": "`operator/` on `std::filesystem::path`",
    "PathRemoveFileSpecW": "`std::filesystem::path::parent_path()`",
    "SHGetFolderPathA": "`portable::UserDataDir()` (`$XDG_CONFIG_HOME`)",
    "SHGetFolderPathW": "`portable::UserDataDir()`",
    "SHGetKnownFolderPath": "`portable::UserDataDir()`",
    "SHGetSpecialFolderPathA": "`portable::UserDataDir()`",
    "SHGetSpecialFolderPathW": "`portable::UserDataDir()`",
    "GetEnvironmentVariableA": "`std::getenv()`",
    "GetEnvironmentVariableW": "`std::getenv()`",
    "GetEnvironmentStringsW": "`environ`",
    # encoding
    "MultiByteToWideChar": "`portable::CpToUtf8` / no-op (UTF-8 everywhere)",
    "WideCharToMultiByte": "`portable::WideToUtf8` / no-op",
    "AnsiToUtf8": "`portable::CpToUtf8(s, 932|0)` (`iconv` or UTF-8 pass-through)",
    "Utf8ToWide": "`portable::Utf8ToWide` (`wstring_convert`/`mbrtoc32`)",
    "WideToUtf8": "`portable::WideToUtf8`",
    "Utf8ToAnsi": "`portable::Utf8ToCp`",
    "AnsiToWide": "`portable::CpToWide`",
    "WideToAnsi": "`portable::WideToCp`",
    "CharUpperA": "`std::toupper`",
    "CharUpperW": "`std::towupper`",
    "lstrlenA": "`std::strlen`",
    "lstrlenW": "`std::wcslen`",
    "_wfopen": "`std::fopen` on the UTF-8 path",
    "_wstat": "`std::filesystem::status()`",
    "_waccess": "`std::filesystem::exists()`",
    "wsprintfA": "`std::snprintf`",
    "wsprintfW": "`std::swprintf`",
    # registry
    "RegOpenKeyExA": "`portable::ReadRegistryString()` (`~/.config/...` ini)",
    "RegOpenKeyExW": "`portable::ReadRegistryString()`",
    "RegQueryValueExA": "`portable::ReadRegistryString()`",
    "RegQueryValueExW": "`portable::ReadRegistryString()`",
    "RegGetValueA": "`portable::ReadRegistryString()`",
    "RegGetValueW": "`portable::ReadRegistryString()`",
    "RegCloseKey": "`portable::ReadRegistryString()` closes internally",
    "RegEnumKeyExA": "`portable::ReadRegistryString()`",
    "RegEnumKeyExW": "`portable::ReadRegistryString()`",
    "HKEY_LOCAL_MACHINE": "`portable::SteamLibraries()`",
    "HKEY_CURRENT_USER": "`portable::UserDataDir()`",
    # profile (GetPrivateProfile* family)
    "GetPrivateProfileIntA": "`portable::ini::GetInt()` (same parser as `portable::ini`)",
    "GetPrivateProfileStringA": "`portable::ini::GetString()`",
    "GetPrivateProfileStringW": "`portable::ini::GetString()`",
    "WritePrivateProfileStringA": "`portable::ini::SetString()`",
    "WritePrivateProfileStringW": "`portable::ini::SetString()`",
    # steam
    "steamapps": "`portable::SteamLibraries()` also returns `steamapps`",
    "SteamApps": "`portable::SteamLibraries()`",
    "libraryfolders.vdf": "`portable::SteamLibraries()` parses it identically",
    "appmanifest_": "`portable::SteamApps()` (same parsing)",
    # message
    "MessageBoxA": "`platform::MessageBox()` (SDL_ShowSimpleMessageBox, logged when headless)",
    "MessageBoxW": "`platform::MessageBox()`",
    "MessageBox": "`platform::MessageBox()`",
    "FormatMessageA": "`std::strerror` / portable error string",
    "FormatMessageW": "`std::strerror` / portable error string",
    "LocalFree": "`std::free` (only around SDDL/psid Win32 allocations)",
    # process
    "ShellExecuteA": "`portable::OpenFileExternally()` (`xdg-open`)",
    "ShellExecuteW": "`portable::OpenFileExternally()` (`xdg-open`)",
    "CreateProcessA": "`portable::Spawn()` (`posix_spawnp`) or Windows-only game launcher",
    "CreateProcessW": "`portable::Spawn()` (`posix_spawnp`) or Windows-only game launcher",
    "CreateProcess": "`portable::Spawn()`",
    "GetExitCodeProcess": "`portable::WaitExit()`",
    "TerminateProcess": "`portable::Kill()`",
    "OpenProcess": "`portable::ProcessAlive(pid)` (`kill(pid, 0)`)",
    "OpenProcessToken": "not needed (no DACL on POSIX mappings)",
    "CreateToolhelp32Snapshot": "`/proc/<pid>/cmdline` scan (`portable::FindProcess`)",
    "Process32FirstW": "`/proc/<pid>/cmdline` scan",
    "Process32NextW": "`/proc/<pid>/cmdline` scan",
    "GetCurrentProcess": "`getpid()` (only in Win32 DACL code)",
    "GetCurrentProcessId": "`getpid()`",
    "ExitProcess": "`std::exit`",
    # pipe
    "CreateNamedPipeA": "`portable::pipe::CreateServer()` (AF_UNIX in `$XDG_RUNTIME_DIR`)",
    "CreateNamedPipeW": "`portable::pipe::CreateServer()`",
    "ConnectNamedPipe": "`portable::pipe::Accept()`",
    "DisconnectNamedPipe": "`portable::pipe::CloseClient()`",
    "PeekNamedPipe": "`portable::pipe::BytesAvailable()` (`ioctl FIONREAD`)",
    "CreatePipe": "`pipe(2)`",
    "WaitNamedPipeW": "`portable::pipe::WaitConnect()`",
    # clipboard
    "OpenClipboard": "in-app clipboard is shared memory, not the OS clipboard",
    "CreateFileMappingA": "`portable::SharedMemory::Create()` (`shm_open`)",
    "CreateFileMappingW": "`portable::SharedMemory::Create()` (`shm_open`)",
    "CreateFileMapping": "`portable::SharedMemory::Create()`",
    "OpenFileMappingA": "`portable::SharedMemory::Open()`",
    "OpenFileMappingW": "`portable::SharedMemory::Open()`",
    "MapViewOfFile": "`mmap()`",
    "MapViewOfFileEx": "`mmap(MAP_FIXED_NOREPLACE)` at the same address",
    "UnmapViewOfFile": "`munmap()`",
    "VirtualAlloc": "`mmap(MAP_ANONYMOUS)`",
    "VirtualFree": "`munmap()`",
    "VirtualQuery": "`mmap` size is known from the header; no query needed",
    "GetSystemInfo": "`sysconf(_SC_PAGESIZE)`",
    "GlobalAlloc": "`malloc`",
    "GlobalFree": "`free`",
    "GlobalLock": "no-op on POSIX",
    "GlobalUnlock": "no-op on POSIX",
    "GlobalSize": "allocator size query",
    "FILE_MAP_ALL_ACCESS": "`PROT_READ|PROT_WRITE`",
    "FILE_MAP_READ": "`PROT_READ`",
    "PAGE_READWRITE": "`PROT_READ|PROT_WRITE`",
    "ERROR_ALREADY_EXISTS": "`O_EXCL` on the name file",
    # timer
    "SetTimer": "`SDL_AddTimer` (SDL3)",
    "KillTimer": "`SDL_RemoveTimer`",
    "GetTickCount": "`portable::NowMs()` (`clock_gettime(CLOCK_MONOTONIC)`)",
    "GetTickCount64": "`portable::NowMs()`",
    "QueryPerformanceCounter": "`clock_gettime(CLOCK_MONOTONIC)`",
    "QueryPerformanceFrequency": "`CLOCKS_PER_SEC` equivalent",
    "timeGetTime": "`portable::NowMs()`",
    "Sleep": "`std::this_thread::sleep_for`",
    "GetLocalTime": "`std::localtime` (`localtime_r`)",
    "GetSystemTime": "`std::gmtime` (`gmtime_r`)",
    "GetSystemTimeAsFileTime": "`clock_gettime(CLOCK_REALTIME)`",
    # sync
    "CreateEventA": "`portable::pipe` wait via `poll()`",
    "CreateEventW": "`portable::pipe` wait via `poll()`",
    "SetEvent": "`portable::pipe` wakeup via `pipe(2)`",
    "ResetEvent": "`portable::pipe` (no manual-reset object needed)",
    "WaitForSingleObject": "`poll()` on the pipe fd / `waitpid()` on the child",
    "WaitForMultipleObjects": "`poll()`",
    "CreateMutexA": "`std::mutex` (unused cross-process?)",
    "CreateMutexW": "`std::mutex`",
    "CreateSemaphoreW": "`std::counting_semaphore`",
    "InitializeCriticalSection": "`std::mutex`",
    "EnterCriticalSection": "`std::lock_guard`",
    "LeaveCriticalSection": "`std::lock_guard` scope",
    "DeleteCriticalSection": "`std::mutex` destructor",
    "CloseHandle": "`close(fd)` / `munmap`",
    "INVALID_HANDLE_VALUE": "`-1` fd sentinel",
    "GetLastError": "`errno` (`portable::LastError()`)",
    # files
    "CreateFileA": "`std::fstream` / `open(2)`",
    "CreateFileW": "`std::fstream` / `open(2)`",
    "ReadFile": "`std::fread` / `read(2)`",
    "WriteFile": "`std::fwrite` / `write(2)`",
    "SetFilePointer": "`std::fseek`",
    "SetFilePointerEx": "`std::fseek`",
    "GetFileSize": "`std::filesystem::file_size()`",
    "GetFileSizeEx": "`std::filesystem::file_size()`",
    "FlushFileBuffers": "`fsync(2)`",
    # window
    "HWND": "`SDL_Window*` (P0 `platform::Window()`)",
    "HINSTANCE": "`SDL_Window*`",
    "HDC": "removed with the GL context",
    "HGLRC": "removed with the GL context",
    "HMODULE": "`dlopen` handle (version resource only)",
    "DWORD": "`uint32_t`",
    "BOOL": "`bool`",
    "HANDLE": "`int` fd / `portable::SharedMemory`",
    "LPWSTR": "`std::wstring`",
    "LPCWSTR": "`std::wstring_view`",
    "wchar_t": "`std::u8string` / `std::string` (UTF-8)",
    "PROCESSENTRY32W": "`portable::ProcessEntry`",
    "SYSTEM_INFO": "`sysconf` results",
    "OVERLAPPED": "`poll()` + nonblocking fd",
    "SECURITY_ATTRIBUTES": "not needed on POSIX",
    "STARTUPINFOW": "`posix_spawn` attributes",
    "PROCESS_INFORMATION": "`pid_t`",
    "CreateWindowExA": "`SDL_CreateWindow` (P0, already done)",
    "CreateWindowExW": "`SDL_CreateWindow` (P0, already done)",
    "RegisterClassExA": "`SDL_CreateWindow` (P0, already done)",
    "RegisterClassExW": "`SDL_CreateWindow` (P0, already done)",
    "DefWindowProcA": "`SDL_ShowSimpleMessageBox` / no-op",
    "DefWindowProcW": "`SDL_ShowSimpleMessageBox` / no-op",
    "GetClientRect": "`platform::ClientSize()` (P0)",
    "GetWindowRect": "`platform::WindowSize()` (P0)",
    "ScreenToClient": "`SDL_GetWindowPosition` arithmetic",
    "ClientToScreen": "`SDL_GetWindowPosition` arithmetic",
    "GetForegroundWindow": "`SDL_GetKeyboardFocus`",
    "SetForegroundWindow": "`SDL_RaiseWindow`",
    "SetWindowTextW": "`platform::SetTitle()` (P0)",
    "SetWindowLongPtrW": "deleted with `workspace_viewports` (P2)",
    "GetWindowLongPtrW": "deleted with `workspace_viewports` (P2)",
    "CallWindowProcW": "deleted with `workspace_viewports` (P2)",
    "GetDC": "deleted with the GL context",
    "ReleaseDC": "deleted with the GL context",
    "GetSystemMetrics": "`SDL_GetDisplayBounds`",
    "GetDpiForWindow": "`platform::DisplayScale()` (P0)",
    "ShowWindow": "`SDL_ShowWindow`",
    "UpdateWindow": "`SDL_PumpEvents`",
    "InvalidateRect": "`SDL_PumpEvents`",
    "GetKeyState": "`SDL_GetModState` / `SDL_GetKeyboardState`",
    "GetAsyncKeyState": "`SDL_GetKeyboardState`",
    "LoadImageW": "`SDL_LoadBMP` / embedded resource",
    "LoadCursorW": "`SDL_CreateSystemCursor`",
    "LoadIconW": "`SDL_Surface` icon",
    "DragAcceptFiles": "`SDL_EventDrag` (P0)",
    "DragQueryFileW": "`SDL_EVENT_DROP_FILE` (P0)",
    "DragFinish": "`SDL_free` on the dropped path",
    "GetOpenFileNameA": "`platform::FileDialog()` (P0 SDL dialog)",
    "GetOpenFileNameW": "`platform::FileDialog()`",
    "GetSaveFileNameA": "`platform::FileDialog()`",
    "GetSaveFileNameW": "`platform::FileDialog()`",
    "SHCreateItemFromParsingName": "`platform::BrowseForFolderUtf8()` (P0)",
    "CoInitializeEx": "not needed on POSIX",
    "CoCreateInstance": "removed with WIC (use stb/libpng or SDL_image)",
    "CoUninitialize": "not needed on POSIX",
    "IID_PPV_ARGS": "removed with WIC",
    "SUCCEEDED": "removed with WIC",
    # audio
    "PlaySoundA": "`SDL_LoadWAV` + `SDL_PlayAudioStream`",
    "PlaySoundW": "`SDL_LoadWAV` + `SDL_PlayAudioStream`",
    "PlaySound": "`SDL_LoadWAV` + `SDL_PlayAudioStream`",
    "waveOutOpen": "`SDL_OpenAudioDeviceStream`",
    "waveOutClose": "`SDL_DestroyAudioStream`",
    "waveOutWrite": "`SDL_PutAudioStreamData`",
    "waveOutPrepareHeader": "no-op (SDL owns the buffer)",
    "waveOutUnprepareHeader": "no-op",
    "waveOutReset": "`SDL_ClearAudioStream`",
    "WAVEHDR": "`SDL_AudioStream` buffer",
    "WAVEFORMATEX": "`SDL_AudioSpec`",
    # version
    "GetFileVersionInfoW": "Windows-only branch (version resource)",
    "GetFileVersionInfoA": "Windows-only branch",
    "VerQueryValueW": "Windows-only branch",
    "VerQueryValueA": "Windows-only branch",
    "VS_FIXEDFILEINFO": "Windows-only branch",
}

GROUP_DEFAULT = {
    "paths": "`std::filesystem`",
    "encoding": "`portable::Utf8ToWide` / identity",
    "registry": "`portable::ReadRegistryString()`",
    "profile": "`portable::ini::*`",
    "steam": "`portable::SteamLibraries()`",
    "message": "`platform::MessageBox()`",
    "process": "`portable::Spawn()` / `/proc` scan",
    "pipe": "`portable::pipe::*` (AF_UNIX)",
    "clipboard": "`portable::SharedMemory::*` (`shm_open`)",
    "timer": "`portable::NowMs()` / `std::this_thread::sleep_for`",
    "sync": "`poll()` / `std::mutex`",
    "files": "`std::fstream` / `open(2)`",
    "window": "`platform::*` (SDL3, P0)",
    "types": "standard C++ type (`uint32_t`, `bool`, opaque handle, `std::string`)",
    "audio": "SDL3 audio stream",
    "version": "Windows-only branch",
}

# A few names above are also constants that mean a value, not a call; the
# `types` group's replacement is the alias, so the value cases are spelled out.
REPLACEMENT.update({
    "INVALID_HANDLE_VALUE": "`-1` (POSIX fd sentinel); `portable::kInvalidHandle`",
    "ERROR_ALREADY_EXISTS": "`errno == EEXIST` (or the `O_EXCL` create race)",
    "ERROR_PIPE_BUSY": "`errno == EAGAIN` on the connect path",
    "WAIT_OBJECT_0": "`poll()`/`waitpid()` success",
    "WAIT_TIMEOUT": "`poll()` timeout",
    "FILE_MAP_ALL_ACCESS": "`PROT_READ | PROT_WRITE`",
    "FILE_MAP_READ": "`PROT_READ`",
    "PAGE_READWRITE": "`PROT_READ | PROT_WRITE`",
    "VS_FIXEDFILEINFO": "Windows-only branch (version resource)",
    "MEMORY_BASIC_INFORMATION": "not needed: the mapping size is in the header",
})


def rel(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def excluded(relpath):
    """True for files this inventory deliberately does not list: the P0-owned
    files (the main line replaces them with SDL3) and src/platform (the layer
    that is allowed to know the OS; see section 1 of docs/render/linux.md)."""
    return relpath in P0_FILES or relpath.startswith(INCLUDE_DIRS)


# One compiled alternation over every symbol, so a source line is tested once
# instead of once per symbol. Longest symbols first: the regex alternation is
# ordered, so without this "MessageBox" would match before "MessageBoxA".
_SYMBOL_GROUP = {sym: group for group, syms in GROUPS for sym in syms}
# The steam group is made of file names and install paths, which live inside
# string literals, so it is matched against the raw line.
_STEAM_SYMBOLS = sorted((s for s, g in _SYMBOL_GROUP.items() if g == "steam"), key=len, reverse=True)
_ALL_SYMBOLS = sorted((s for s, g in _SYMBOL_GROUP.items() if g != "steam"), key=len, reverse=True)
_CALL_RE = re.compile("|".join(r"\b" + re.escape(s) + r"\b" for s in _ALL_SYMBOLS))
_STEAM_RE = re.compile("|".join(re.escape(s) for s in _STEAM_SYMBOLS))


def iter_files():
    for base, dirs, files in os.walk(SRC):
        dirs[:] = sorted(d for d in dirs if d not in ("third_party",))
        for name in sorted(files):
            if name.endswith(EXT):
                yield os.path.join(base, name)


def scan():
    includes = []   # (file, line, header)
    calls = []      # (file, line, group, symbol)
    for path in iter_files():
        r = rel(path)
        if excluded(r):
            continue
        # Files that declare their own ReadFile()/WriteFile() helpers. Matching
        # the Win32 names there would be a false positive (cgm_acmoon.cpp:23,
        # hud_colors.cpp:31).
        local = {"ReadFile", "WriteFile"} if r in LOCAL_FILE_HELPERS else set()
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f, 1):
                m = INCLUDE_RE.search(line)
                if m:
                    includes.append((r, i, m.group(1)))
                # Only real code counts: a Win32 name in a comment (the generated
                # type headers quote IDA annotations such as "+0x04 T: MapViewOfFile")
                # is documentation, not a call site.
                code = line.split("//", 1)[0]
                code = STRING_LITERAL_RE.sub('""', code)
                s = code.lstrip()
                if s.startswith("*") or s.startswith("/*"):
                    continue
                seen_line = set()
                for mm in _CALL_RE.finditer(code):
                    sym = mm.group(0)
                    if sym in seen_line or sym in local:
                        continue
                    seen_line.add(sym)
                    calls.append((r, i, _SYMBOL_GROUP[sym], sym))
                for mm in _STEAM_RE.finditer(line):
                    sym = mm.group(0)
                    if sym in seen_line:
                        continue
                    seen_line.add(sym)
                    calls.append((r, i, "steam", sym))
    return includes, calls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tsv", action="store_true", help="file<TAB>line<TAB>call, grouped")
    ap.add_argument("--check", action="store_true", help="fail when a P0-owned file appears")
    ap.add_argument("--write-doc", action="store_true", help="regenerate the auto tables in docs/render/linux.md")
    args = ap.parse_args()

    includes, calls = scan()
    if args.write_doc:
        return write_doc(includes, calls)
    if args.check:
        bad = [c for c in calls if excluded(c[0])]
        for b in bad:
            print("P0-owned file in inventory:", b)
        return 1 if bad else 0

    if args.tsv:
        for r, i, h in includes:
            print(f"{r}\t{i}\tinclude\t{h}")
        seen = set()
        for r, i, group, sym in calls:
            key = (r, i, sym)
            if key in seen:
                continue
            seen.add(key)
            print(f"{r}\t{i}\t{group}\t{sym}")
        return 0

    print(f"# Win32 inventory ({len(includes)} includes, {len(calls)} call sites)\n")
    print("## `#include <windows.h>` outside P0\n")
    print("| file | line | header |")
    print("|---|---|---|")
    for r, i, h in includes:
        print(f"| {r} | {i} | {h} |")
    print()
    by_group = {}
    for r, i, group, sym in calls:
        by_group.setdefault(group, []).append((r, i, sym))
    for group in sorted(by_group):
        print(f"## {group}\n")
        print("| file | line | symbol | portable replacement |")
        print("|---|---|---|---|")
        for r, i, sym in sorted(set(by_group[group])):
            print(f"| {r} | {i} | `{sym}` | {REPLACEMENT.get(sym, GROUP_DEFAULT[group])} |")
        print()
    return 0


# docs/render/linux.md keeps its prose and embeds the tables between markers, so
# `--write-doc` refreshes only the tables and a reviewer sees the diff of the
# inventory itself. One marker pair per group plus the include list.
def _marker(group, which):
    return f"<!-- {which} auto:{group} -->"


def _table_for(group, includes, calls):
    out = []
    if group == "includes":
        out.append("| file | line | header |")
        out.append("|---|---|---|")
        for r, i, h in includes:
            out.append(f"| `{r}` | {i} | `{h}` |")
        return "\n".join(out)
    if group == "types":
        # Counted per file: the alias header covers every line, so a line-level
        # list would be 200 rows of the same replacement.
        rows = sorted({(r, sym) for r, _, g, sym in calls if g == "types"})
        per_file = {}
        for r, sym in rows:
            per_file.setdefault(r, set()).add(sym)
        out.append("| file | aliases used | portable replacement |")
        out.append("|---|---|---|")
        for r in sorted(per_file):
            syms = ", ".join(f"`{s}`" for s in sorted(per_file[r]))
            out.append(f"| `{r}` | {syms} | `DWORD`/`BOOL` -> `uint32_t`/`bool`, `HANDLE` -> fd or `portable::SharedMemory`, `wchar_t` -> UTF-8 `std::string` |")
        return "\n".join(out)
    rows = sorted({(r, i, sym) for r, i, g, sym in calls if g == group})
    out.append("| file | line | win32 call | portable replacement |")
    out.append("|---|---|---|---|")
    for r, i, sym in rows:
        out.append(f"| `{r}` | {i} | `{sym}` | {REPLACEMENT.get(sym, GROUP_DEFAULT[group])} |")
    return "\n".join(out)


def write_doc(includes, calls):
    if not os.path.exists(DOC):
        print("no docs/render/linux.md to update (create the skeleton first)", file=sys.stderr)
        return 2
    with open(DOC, "r", encoding="utf-8") as f:
        text = f.read()
    groups = ["includes"] + sorted({g for _, _, g, _ in calls})
    for group in groups:
        begin, end = _marker(group, "BEGIN"), _marker(group, "END")
        if begin not in text or end not in text:
            print(f"missing marker for group {group}", file=sys.stderr)
            return 2
        head, rest = text.split(begin, 1)
        _, tail = rest.split(end, 1)
        text = head + begin + "\n\n" + _table_for(group, includes, calls) + "\n\n" + end + tail
    # The counts in the summary line come from the scan too.
    n_sites = len(calls)
    text = re.sub(r"<!-- auto:counts -->.*?<!-- /auto:counts -->",
                  f"<!-- auto:counts -->{len(includes)} `#include <windows.h>`-family sites, {n_sites} Win32 call sites<!-- /auto:counts -->",
                  text, flags=re.S)
    with open(DOC, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"updated {os.path.relpath(DOC, ROOT)} ({len(includes)} includes, {n_sites} calls)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
