#ifndef HMOD_TRACK_H_GUARD
#define HMOD_TRACK_H_GUARD
// Automatic change tracking against vanilla (AUTHORING_DESIGN.md section 4.1): every working file of an asset is classified
// from the vanilla hash index, with HA6 compared per pattern slot (so editor re-serialisation is "re-encoded", not "changed")
// and text compared with line-ending / trailing-space normalisation.
#include "hmod_project.h"
#include "hmod_vanilla.h"

#include <string>
#include <vector>

namespace hmod {

enum class Status { Unchanged, Reencoded, Changed, Added, Orphan, Clutter };
const char* StatusName(Status s);
inline bool Ships(Status s) { return s == Status::Changed || s == Status::Added; }

struct TrackedFile {
	std::string asset;
	std::string rel;        // game-relative, '\' separators, the author's case
	std::string norm;       // NormKey(rel)
	std::string abs;        // UTF-8 path of the working file
	std::string sha256;
	uint64_t size = 0;
	Status status = Status::Added;
	std::string note;       // why (clutter reason, "3 slots: 1 fill, 2 modify", "line endings only", ...)
	std::string vanillaPack;   // pack holding the vanilla file (when there is one)
	std::string vanillaSha;
	size_t slotsClaimed = 0, slotsNameOnly = 0;   // HA6 only
};

// Why `norm` is tool / user-state clutter ("" when it is not): editor caches, backups, dumps, user state, executables.
std::string ClutterReason(const std::string& norm);

// Every file under <project>\assets\<asset id>\ . `vanilla` may be null or empty (then files are Changed/unknown).
std::vector<TrackedFile> TrackAsset(const Project& p, const Asset& a, const VanillaIndex* vanilla);
std::vector<TrackedFile> TrackProject(const Project& p, const VanillaIndex* vanilla);

// Records the vanilla base (path, pack, sha256) of every Changed / Reencoded file on vanilla-derived assets, replacing the
// asset's origin.base. Returns how many assets changed.
size_t FillBases(Project& p, const std::vector<TrackedFile>& tracked);

struct StatusCounts { size_t unchanged = 0, reencoded = 0, changed = 0, added = 0, orphan = 0, clutter = 0; };
StatusCounts Count(const std::vector<TrackedFile>& t);

} // namespace hmod
#endif
