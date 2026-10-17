#ifndef HMOD_PROJECT_H_GUARD
#define HMOD_PROJECT_H_GUARD
// hmod.json: the Hantei-chan mod PROJECT manifest (docs/modmanager/AUTHORING_DESIGN.md section 3). A project is a folder:
//   hmod.json  assets\<asset-id>\<game-relative path>  sources\  recipes\  build\ (generated)  .hmod\ (cache)
// This is a project file, not a package manifest: packages (pcmod, IM Packs) are generated from it. Pure C++, no GL / Win32.
#include "../pcmod/pcmod_manifest.h"

#include <string>
#include <vector>

namespace hmod {

constexpr int kHmodVersion = 1;

// Category ids (design section 5). The default class and export mode follow from them.
struct CategoryInfo { const char* id; const char* defaultClass; const char* summary; };
const std::vector<CategoryInfo>& Categories();
const CategoryInfo* FindCategory(const std::string& id);

struct BaseRef { std::string path, pack, sha256; };   // the vanilla file an asset file was derived from

struct Origin {
	std::string kind = "authored";   // vanilla-derived | clone | game-convert | install-import | authored
	std::vector<BaseRef> base;       // vanilla-derived: base hashes (filled by Refresh)
	std::string of, files, note;     // clone: source character / file glob
	std::string source;              // game-convert / install-import: id of a sources[] entry
};

// charaselect.txt entry (or grid cell) an asset claims; compared by the conflict check.
struct CharaClaim {
	int charaNo = -1;                // [CharaNo_NNN]
	std::string name, file1, file2;  // Name / File1 / File2
	int cell = -1;                   // character-select grid cell (-1 = none declared)
	bool any() const { return charaNo >= 0 || cell >= 0 || !name.empty(); }
};

struct Asset {
	std::string id, category, cls;               // cls "" = decided per file by its path
	std::vector<std::string> targets;            // game-relative globs (* ?); empty = every file in the asset folder
	Origin origin;
	std::string exportMode = "whole-file";       // whole-file | ha6-patch | ini-merge (the last two degrade to whole-file in schema 1)
	std::vector<std::string> depends;            // asset ids
	CharaClaim chara;
};

struct Package {
	std::string id, version = "1.0.0", name, description, cls;   // cls "" = auto
	std::vector<std::string> assets;             // asset ids or globs ("*-balance")
	std::vector<pcmod::Requirement> requires_;
	std::vector<std::string> conflicts, overrides, modes;
};

struct SourceRef { std::string id, kind, path, tool, toolVersion, note, credit, imported; };

struct OutputFile { std::string path, sha256; };
struct Output {                                   // what the last export wrote
	std::string target;                           // "pcmod" | "im-packs" | "im-mods"
	std::string package, version, path, digest, simDigest, when;
	std::vector<OutputFile> files;
};

struct Base { std::string exeSha256, packset; };

struct Project {
	int hmod = kHmodVersion;
	std::string id, name, author, game = "mbaacc";   // mbaacc | uni2 | mbtl
	Base base;
	std::vector<SourceRef> sources;
	std::vector<Asset> assets;
	std::vector<Package> packages;
	std::vector<Output> outputs;
	std::string dir;                                 // project folder (UTF-8), set by Load / Create, not written

	const Asset* FindAsset(const std::string& id) const;
	Asset* FindAsset(const std::string& id);
	const Package* FindPackage(const std::string& id) const;
	std::string AssetDir(const std::string& assetId) const;   // <dir>\assets\<id>
};

bool ValidGame(const std::string& g);
// Every rule of the model: ids, unique assets / packages, known categories / origin kinds / export modes, depends resolve,
// package assets match something. Returns the first problem, or "".
std::string Validate(const Project& p);

bool Parse(const std::string& text, Project& out, std::string& err);
std::string Write(const Project& p);   // deterministic: 2-space indent, fixed key order, '\n'

// `pathOrDir` is a folder holding hmod.json, or the hmod.json itself.
bool Load(const std::string& pathOrDir, Project& out, std::string& err);
bool Save(const Project& p, std::string& err);   // writes <dir>\hmod.json (atomic) and creates assets\ sources\ recipes\
// A fresh project folder with an empty hmod.json.
bool Create(const std::string& dir, const std::string& id, const std::string& game, Project& out, std::string& err);

// Does `pattern` (game-relative glob, '*' = any run, '?' = one char, case-insensitive, '\' or '/') match `normPath`?
bool GlobMatch(const std::string& pattern, const std::string& normPath);
// Packages' asset lists resolved to asset ids (globs expanded, order kept, duplicates dropped).
std::vector<std::string> ResolvePackageAssets(const Project& p, const Package& pkg);

std::string NowIso();   // UTC "2026-10-15T12:00:00Z"

} // namespace hmod
#endif
