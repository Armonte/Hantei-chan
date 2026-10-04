#ifndef CGM_STRUCT_H_GUARD
#define CGM_STRUCT_H_GUARD
// Structural edits of a bank: add / clear / delete / move / permute images, make an image own its pixels, atlas page bookkeeping.
// Every op that changes image ids fills `remap` (old id -> new id, -1 = removed) so frame references can be fixed up.
#include "cgm_bank.h"
#include <string>
#include <vector>

namespace cgm {

struct NewImageSpec {
	std::string name = "new.bmp";   // stored name (<= 31 bytes)
	int type = -99;                 // -99 = auto: 2 (<=255 colours, binary alpha), 4 (<=255 colours, soft alpha) else 1; or force 0 / 1 / 2 / 4
	int canvasW = 256, canvasH = 256;
	int x1 = INT32_MIN, y1 = INT32_MIN;   // top-left of the sprite on the canvas (cell aligned); INT32_MIN = centred
};

// at = index the new image takes (later ids shift up); -1 = append. The PNG may have any size: it is padded with transparency to whole cells.
bool AddImage(Bank &bank, int at, const NewImageSpec &spec, const uint8_t *rgba, int w, int h, std::vector<int> &remap, int *newId, std::string *err);
// Replace image n by an empty type -1 placeholder (the engine's own "no sprite" entry), keeping every id. Refused while other images borrow its cells.
bool ClearImage(Bank &bank, int n, std::string *err);
// Remove image n and renumber the following ones. Dependants (images borrowing its cells) are made independent first when unshare is set, else refused.
bool DeleteImage(Bank &bank, int n, bool unshareDependants, std::vector<int> &remap, std::string *err);
// Image n takes position `to`; the ids in between shift by one.
bool MoveImage(Bank &bank, int from, int to, std::vector<int> &remap, std::string *err);
// newOrder[k] = old id that ends up at k (a permutation of 0..N-1).
bool Permute(Bank &bank, const std::vector<int> &newOrder, std::vector<int> &remap, std::string *err);
// Image n gets its own atlas cells for every block it borrows (pixels unchanged).
bool UnshareImage(Bank &bank, int n, std::string *err);
// Duplicate image n as a new, fully independent image at `at`.
bool DuplicateImage(Bank &bank, int n, int at, std::vector<int> &remap, int *newId, std::string *err);
// Drop atlas pages nobody uses at the end (H0 = used pages).
bool TrimPages(Bank &bank);
// Ids that no remap would change are the identity: helpers.
std::vector<int> IdentityRemap(size_t n);
std::vector<int> InvertRemap(const std::vector<int> &remap, size_t newCount);   // for undo of a permutation / insert (removed ids have no inverse)

} // namespace cgm
#endif
