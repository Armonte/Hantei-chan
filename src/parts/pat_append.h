#ifndef PAT_APPEND_H_GUARD
#define PAT_APPEND_H_GUARD
// Appending part sets of one .pat to another (pattool append-from). Used for the ActCadenza moon-3 set of MBAACC: its
// HA6 frames draw ActCadenza part set ids, while <c>_3.txt loads MBAACC's <c>.pat, so the ActCadenza part sets are
// appended to the MBAACC .pat and the frames' ids are remapped through the returned map.
//
// Every requested source part set is copied with the cut-outs (ppId), shapes (cut-out shapeIndex) and textures (cut-out
// texture) it draws, each at a new id at the end of the destination's list, with every inner reference rewritten. The
// destination keeps every one of its ids and its own records unchanged (Parts::Save writes their original bytes), the
// new part sets get ids >= the destination's part set count, in ascending source order.
// A source cut-out / shape / texture identical to one already in the destination (pixels for a texture) is reused
// instead of copied.
// Textures are re-encoded for the destination's format: an MBAACC destination gets 32-bit PGTX (with PGTP = width when
// the destination's textures carry it, like the shipped MBAACC files; ActCadenza files from ha4tool have none), a
// UNI2 / MBTL destination gets lossless A8R8G8B8 PGT2. MBAACC has no shapes (no VEST): appending to it a cut-out drawn
// with a non-plane shape is reported in `notes` (it draws as a plane there).
#include <string>
#include <utility>
#include <vector>

class Parts;
namespace patrn {

struct AppendResult {
	std::vector<std::pair<int, int>> setMap;   // (source part set id, new id in the destination)
	std::vector<int> skippedEmpty;             // requested source ids that are empty slots (draw nothing): not appended
	int cutOutsAdded = 0, cutOutsReused = 0;
	int shapesAdded = 0, shapesReused = 0;
	int texturesAdded = 0, texturesReused = 0;
	std::vector<std::string> notes;
};

// "1,5,7-9" -> ids. False on a malformed list.
bool ParseIdList(const std::string &s, std::vector<int> &ids, std::string *err);
// srcIds empty = every non-empty part set of the source.
bool AppendPartSets(Parts &dst, const Parts &src, const std::vector<int> &srcIds, AppendResult &res, std::string *err);

} // namespace patrn
#endif
