#ifndef PROJECT_ENTRIES_H_GUARD
#define PROJECT_ENTRIES_H_GUARD

// The "characters" entries of a .hproj, without the editor types behind them, so the
// save/restore rules can be tested headlessly (tests/project_entries_test.cpp).
//
// Entry kinds:
//   "txt" / "ha6"  a character file; optional "pat_path" = the .pat it draws (Load PAT on an
//                  HA6 tab, or one the txt pointed at), restored when the file did not load it.
//   "pat"          a standalone PAT editor tab (File > Open/New PAT): "path" = the .pat,
//                  "pat_format" = "mbaacc" | "uni" (UNI2 / MBTL layout).
// Older builds skip a "pat" entry (unknown type: reported, never misread), and a file without
// "pat_path" / "pat_format" loads as before.

#include "../third_party/json/json.hpp"
#include <string>

namespace projentry {

struct CharacterInfo {
	std::string name;
	std::string txtPath;
	std::string ha6Path;      // top HA6
	std::string patPath;      // its .pat, if any
	bool patDocument = false; // a standalone PAT editor tab
	bool patMbaacc = false;   // PAT layout (MBAACC vs UNI / MBTL)
};

// What a saved entry asks the loader to do. Paths are absolute.
struct LoadPlan {
	enum class Kind { None, Txt, Ha6, Pat } kind = Kind::None;
	std::string path;
	std::string patPath;      // Txt / Ha6: the .pat to attach when the file did not load one
	bool hasPatFormat = false;
	bool patMbaacc = false;
};

std::string MakeRelativePath(const std::string& filePath, const std::string& projectPath);
std::string MakeAbsolutePath(const std::string& filePath, const std::string& projectPath);

// The entry for a character, or a null json when it has nothing on disk to point at (unsaved).
nlohmann::json MakeCharacterEntry(const CharacterInfo& c, const std::string& projectPath);
LoadPlan PlanCharacterLoad(const nlohmann::json& entry, const std::string& projectPath);

// The text shown and logged for something in the project that did not load: its path, flagged
// "(file not found)" when the file is not there, so it reads differently from a parse failure.
std::string DescribeFailed(const nlohmann::json& entry, const std::string& absolutePath);

const char* PatFormatName(bool mbaacc);

} // namespace projentry

#endif /* PROJECT_ENTRIES_H_GUARD */
