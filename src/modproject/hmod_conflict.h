#ifndef HMOD_CONFLICT_H_GUARD
#define HMOD_CONFLICT_H_GUARD
// Conflict check before export: a package about to be written is compared with the other installed or known packages of the
// same game, and with the project's own other packages. Three kinds (the user's brief):
//   File       the same game path with different bytes;
//   Ha6Slot    the same HA6 pattern slot of the same character layer set claimed (changed against vanilla) by both, even
//              when they sit in different files (akiha_1.HA6 vs akiha_1_r.HA6);
//   CharaCell  the same charaselect.txt [CharaNo_NNN] section (or grid cell, or Name) defined differently.
// Exports refuse on any unresolved conflict and never overwrite silently. A conflict is resolved only by the author's own
// declaration (`conflicts` = never enabled together, `overrides` = deliberate replacement) or by byte-identical content.
#include "hmod_project.h"
#include "hmod_track.h"
#include "hmod_vanilla.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace hmod {

struct PkgFile { std::string abs, sha256; uint64_t size = 0; };

struct PackageView {
	std::string id, version, origin;      // origin: "this project", "installed pcmod: <dir>", "IM pack: <dir>", "known: <dir>"
	std::string dir;
	std::string cls;                      // sim | cosmetic | "" (unknown / IM)
	bool enabled = true;
	std::map<std::string, PkgFile> files; // normalised game path -> file
	std::set<std::string> conflicts, overrides;   // ids this package declares
	std::vector<CharaClaim> declared;     // charaselect claims declared in hmod.json (not read from a file)
};

enum class ConflictKind { File, Ha6Slot, CharaCell };
const char* ConflictKindName(ConflictKind k);

struct Conflict {
	ConflictKind kind = ConflictKind::File;
	std::string mine, other;              // package ids
	std::string otherOrigin;
	std::string what;                     // path / "akiha_1 slots 300, 301" / "[CharaNo_010]"
	bool blocks = true;                   // false = informational (identical bytes, declared conflicts / overrides)
	std::string why;                      // human detail, includes the resolution when blocks == false
};

// ---- loaders ----
// A package folder: pcmod.json (read as the manifest; mod.ini enabled=) or a loose IM-style tree (every file; mod.ini [Mod] ignored).
bool LoadPackageView(const std::string& dir, PackageView& out, std::string& err);
// <gameDir>\mods\<id>\ for every folder; legacy (mod.ini only) folders are listed by their files.
std::vector<PackageView> LoadInstalledPcmod(const std::string& gameDir);
// <imRoot>\Mods ("your own files") and every <imRoot>\Packs\<name>\ . `skipPack` leaves one pack out (the one being rewritten).
std::vector<PackageView> LoadImPackages(const std::string& imRoot, const std::string& skipPack = {});
// A project package from its tracked, shipping files.
PackageView ViewOfProjectPackage(const Project& p, const Package& pkg, const std::vector<TrackedFile>& tracked);

// The check. `vanilla` (may be null) supplies the base for slot / section comparison. `game`: mbaacc | uni2 | mbtl.
std::vector<Conflict> CheckConflicts(const PackageView& mine, const std::vector<PackageView>& others, const VanillaIndex* vanilla);
inline bool AnyBlocking(const std::vector<Conflict>& c) { for (const Conflict& x : c) if (x.blocks) return true; return false; }
std::string DescribeConflict(const Conflict& c);

// charaselect.txt: section key ("charano_010") -> normalised "key=value;..." text. Comments (// ...) and blank lines dropped.
std::map<std::string, std::string> ParseCharaselect(const std::string& text);

} // namespace hmod
#endif
