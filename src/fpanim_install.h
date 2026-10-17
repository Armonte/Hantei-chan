#ifndef FPANIM_INSTALL_H_GUARD
#define FPANIM_INSTALL_H_GUARD

// Install / update / uninstall fpan-im into a UNI2 or MBTL game folder (docs/FPANIM_BUNDLE.md).
// Layout in the game folder:  <game>\winmm.dll (loader shim)  +  <game>\fpan-im\fpan-im.dll (core).
// The shim chainloads <game>\winmm.chain.dll when present, so a foreign winmm.dll found at install time is moved there
// (or, when winmm.chain.dll already exists, kept as winmm.dll.bak-<timestamp>). The install records that in
// <game>\fpan-im\install.txt; uninstall moves our backup back to winmm.dll.
// Only these names are ever written or removed: UNI2-IM / MBTL-IM (dinput8.dll, their folders and ini) and the
// fpan-im mods folder, ini and logs are never touched (PlanTouchesOnlyOurFiles checks every plan).
// The planning half is pure (unit tested, tests/fpanim_install_test.cpp); the Win32 half reads version resources
// (GetFileVersionInfo, the DLL is never loaded), finds a running game and runs a plan.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fpaninst {

// What we know about one file (from its VERSIONINFO, without loading it).
struct FileInfo {
	bool exists = false;
	bool hasVersion = false;     // a StringFileInfo block was read
	std::string productName;     // "fpan-im" for our builds
	std::string version;         // FileVersion ("0.1.0")
	std::string component;       // FpanimComponent: "shim" (winmm.dll) / "core" (fpan-im.dll)
	bool hasMarker = false;      // the bytes "fpan-im" / "fbmod\fbmod.dll" (ASCII or UTF-16) occur in the file
};

// A winmm.dll is ours when its resource says so, or (builds before the version resource) the fpan-im marker is in it.
bool IsOurShim(const FileInfo& winmm);
// "0.1.0", "unknown version" (ours without a resource), "" when the file is missing.
std::string VersionLabel(const FileInfo& f);

struct GameState {
	std::string dir;             // game folder, UTF-8
	FileInfo shim;               // <dir>\winmm.dll
	FileInfo core;               // <dir>\fpan-im\fpan-im.dll
	bool chainExists = false;    // <dir>\winmm.chain.dll
	std::string recordedBackup;  // from fpan-im\install.txt: the name our install moved a foreign winmm.dll to ("" none)
};

enum class Status {
	NotInstalled,       // no shim of ours and no core
	Installed,          // our shim + core
	Partial,            // one of the two is missing (or the shim is foreign while the core is there)
};
Status Classify(const GameState& g);

struct Bundle {
	bool found = false;
	std::string winmm, core;     // absolute source paths (UTF-8)
	std::string dir;             // where it was found
	std::string versionFile;     // contents of <dir>\VERSION (trimmed), "" if absent
};
// A bundle folder holds winmm.dll and either fpan-im\fpan-im.dll (the res/fpan-im package layout) or fpan-im.dll next
// to it (fpan-im's build/bin). exists() answers for a UTF-8 path.
Bundle ResolveBundle(const std::string& dir, const std::function<bool(const std::string&)>& exists);

struct Op {
	enum Type { MkDir, Copy, Move, Remove, WriteText } type;
	std::string src, dst;        // relative to the game folder for every game-side path ('/' separators); Copy src absolute
	std::string text;            // WriteText contents
};
struct Plan {
	std::vector<Op> ops;
	std::vector<std::string> notes;   // shown to the user (backup made, chain kept, ...)
	std::string refused;              // non-empty: the plan must not run
};
// Install or update: copy the bundle's two DLLs in. A foreign winmm.dll is backed up first (see above). timestamp is
// used only for the .bak name ("20261017-153000").
Plan PlanInstall(const GameState& g, const Bundle& b, const std::string& timestamp);
// Uninstall: our winmm.dll and fpan-im\fpan-im.dll (+ fpan-im\*.pdb listed in pdbs), install.txt; our backup moved back.
// A foreign winmm.dll is never removed.
Plan PlanUninstall(const GameState& g, const std::vector<std::string>& pdbs = {});
// Every game-side path the plan writes, moves or removes is one of ours (winmm.dll, winmm.chain.dll, winmm.dll.bak-*,
// fpan-im/fpan-im.dll, fpan-im/*.pdb, fpan-im/install.txt, the fpan-im folder itself).
bool PlanTouchesOnlyOurFiles(const Plan& p);

// install.txt: "backup=<name>" lines. Parse returns the recorded backup name ("" none).
std::string ParseInstallRecord(const std::string& text);
// True when imagePath (a process image, any slashes / case) is the given exe directly inside dir.
bool ImageIsInDir(const std::string& imagePath, const std::string& dir);
// The exe names of the games fpan-im runs on, lower case ("uni2.exe", "mbtl.exe").
const std::vector<std::string>& GameExeNames();

// ---- Win32 -----------------------------------------------------------------------------------------------------
FileInfo ReadFileInfo(const std::string& utf8Path);
GameState ReadGameState(const std::string& dir);
// pid of a uni2.exe / MBTL.exe running from dir, 0 if none.
uint32_t FindRunningGame(const std::string& dir);
// <exe dir>\res\fpan-im
std::string DefaultBundleDir();
// Runs the plan in order; stops at the first failure. Returns "" on success or the error.
std::string Execute(const std::string& dir, const Plan& p);
std::string NowStamp();

} // namespace fpaninst

#endif
