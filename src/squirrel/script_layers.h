#ifndef SQUIRREL_SCRIPT_LAYERS_H_GUARD
#define SQUIRREL_SCRIPT_LAYERS_H_GUARD

// The Script panel's merged file list for UNI2 / MBTL (and the loose-only older games): every Squirrel script the game
// can read, from every place fpan-im's VFS serves files, with the copy that wins.
//
// Layer order, highest priority first (fpan-im core/vfs.h; same rule as UNI2-IM / MBTL-IM):
//   1. fpan-im\mods\                       loose, writable
//   2. fpan-im\packs\<id>\                 [packs] order / off from fpan-im\fpan-im.ini
//   3. <IM>\Mods\                          UNI2-IM\Mods or MBTL-IM\Mods
//   4. <IM>\Packs\<id>\                    [Mods] Order / Off from <IM>\UNI2_IM.ini / MBTL_IM.ini
//   5. loose script\ and data\ under the game folder
//   6. the game archive (UNI2 d\ index, MBTL data*.bin through MBTL.exe), read-only
// Between fpan-im\mods and <IM>\Mods (both "loose" edit folders) the newer file wins, as in fpan-im.
// A disabled pack (listed in Off) is listed with "(off)" and never wins.
//
// Pure std::filesystem code (no UI, no Win32), unit tested by tests/script_layers_test.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace sqedit {

struct ScriptLayer {
	enum Kind { FpanimMods, FpanimPack, ImMods, ImPack, Loose, Archive };
	Kind kind = Loose;
	std::string tag;     // "fpan-im mods", "fpan-im pack foo", "UNI2-IM Mods", "pack ___UniTM", "loose", "archive"
	std::string root;    // folder (UTF-8, '/' separators); "" for the archive
	std::string id;      // pack id
	int priority = 0;    // higher wins
	bool enabled = true; // false: a pack listed in Off (never wins)
	bool served = true;  // fpan-im's VFS reads it (mods / packs / IM layers); false for loose and the archive
};

// A game folder chosen in the panel, resolved: the chosen folder may be a pack, a mod folder or their data / script
// subfolder; the game install above it is found by the folder names.
struct ScriptRootInfo {
	std::string gameDir;    // the install (holds script/, data/, d/, MBTL.exe, UNI2-IM, fpan-im)
	std::string editRoot;   // the pack / mod folder every save goes into ("" = save to the copy that was opened)
	std::string editTag;    // the tag of that layer ("pack ___UniTM", "UNI2-IM Mods", ...)
	std::string error;      // non-empty: not a game folder
};
ScriptRootInfo ResolveScriptRoot(const std::string& chosenUtf8);

// The folder layers of a game install, highest priority first (no archive layer: the caller adds the archive's names).
// game: "uni2" / "mbtl" picks the IM folder (UNI2-IM / MBTL-IM); anything else probes both.
std::vector<ScriptLayer> ScriptLayersFor(const std::string& gameDirUtf8, const std::string& game);

// Which game-relative .txt files are Squirrel scripts: everything under script/, the shared data-root tables
// (data/_combase, VectorTable, BattleInfo, BoundEff: FpanimIsSharedFile), the per-character name patterns, and,
// when `head` (the first bytes of the file) is given, any other data/**/*.txt that reads as Squirrel (a brand-new file).
bool IsSquirrelScriptRel(const std::string& relGeneric, const std::string* head = nullptr);

struct ScriptCopy {
	int layer = -1;          // index into the layer list
	std::string path;        // absolute file (UTF-8); for the archive: "" (extracted on demand)
	int64_t mtime = 0;       // last write (loose-vs-loose rule)
};
struct ScriptFile {
	std::string rel;                // game-relative, '/' separators, as spelled by the winning copy
	std::vector<ScriptCopy> copies; // highest priority first
	int winner = -1;                // index into copies (-1: only disabled copies)
};

// Merges the layers. archiveRels: the archive's entry paths ("data/chr001/chr001_mv_0.txt"), filtered here; the archive
// layer is appended to `layers` when the list is non-empty. Keys compare case-insensitively (Windows file names).
std::vector<ScriptFile> ListScriptFiles(std::vector<ScriptLayer>& layers, const std::vector<std::string>& archiveRels);

// The row label of a copy: "<rel>  [<tag>]".
std::string ScriptCopyLabel(const ScriptFile& f, const ScriptCopy& c, const std::vector<ScriptLayer>& layers);

}  // namespace sqedit

#endif
