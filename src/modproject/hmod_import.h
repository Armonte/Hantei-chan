#ifndef HMOD_IMPORT_H_GUARD
#define HMOD_IMPORT_H_GUARD

// "Import an install as a mod project" (AUTHORING_DESIGN.md, install-import): walk a modded MBAACC install's loose files,
// keep those that differ from the vanilla index (or are new), and file them into assets by what they are: one asset per
// character (data\<stem>*), system constants, roster, story / win quotes, sound effects, music, stages, graphics. Every
// asset has origin install-import with a sources[] entry for the install. The install is only read.
// After copying, the project is tracked and files that are only re-encoded (same HA6 slots, EOL-only text) are dropped.

#include "hmod_project.h"
#include "hmod_vanilla.h"

#include <map>
#include <string>
#include <vector>

namespace hmod {

struct ImportInstallResult {
	size_t scanned = 0, unchanged = 0, imported = 0, reencoded = 0, clutter = 0, skipped = 0;
	std::map<std::string, size_t> perAsset; // asset id -> files kept
	std::vector<std::string> notes;
};

// Asset id and category for a game-relative MBAACC path ("" id = not imported; `why` says why).
void ClassifyInstallPath(const std::string& norm, std::string& assetId, std::string& category, std::string& why);

bool ImportInstall(Project& p, const std::string& installDir, const VanillaIndex& vanilla, ImportInstallResult& r, std::string& err);

} // namespace hmod

#endif
