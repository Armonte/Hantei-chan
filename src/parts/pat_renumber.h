#ifndef PAT_RENUMBER_H_GUARD
#define PAT_RENUMBER_H_GUARD
// Part set renumbering for UNI2 / MBTL / MBAACC .pat files, the way French-Bread reorganises them between patches
// (whole blocks moved or inserted, empty "〇" ids left as gaps), with every reference rewritten so the game draws the
// same thing.
//
// References: an HA6 frame's AF layer draws a PAT part set when its usePat flag is set (AFGP / AFGX), with the part set
// id in the sprite id field. The users of a .pat are the HA6 files of every project .txt of the .pat's folder whose
// [PAniFile] File00 names it (UNI2 chrNNN_0.txt, csel_chrNNN.txt, effect.txt). Only the id bytes of those layers are
// rewritten; every other byte stays as shipped (cgm_project.h does the same for CG ids).
//
// A user file shared with another .pat (../BaseData.ha6 of UNI2, listed by every character) draws a different .pat per
// character, so its references are never rewritten: an operation that would change an id it uses is refused. An
// operation that deletes a part set some frame still draws is refused too. Nothing else references part set ids in the
// data: effects spawn HA6 patterns, and the Squirrel scripts (script/*.txt, chrNNN_*.txt) name patterns, not parts.
// Ids the game code draws directly (exe-side UI) cannot be seen from the data.
#include <cstdint>
#include <string>
#include <vector>

class Parts;
namespace patrn {

struct Op {
	enum Kind { Move, Place, Insert, Delete, Clear, Swap } kind = Move;
	int a = 0, b = 0;   // block a..b (Swap: the two ids)
	int to = 0;         // Move / Place: first id of the block afterwards; Insert: where
	int n = 0;          // Insert: how many empty slots
};
// "a-b:c" / "a:c" (move, place), "at:n" (insert), "a-b" / "a" (delete, clear), "a:b" (swap).
bool ParseOp(Op::Kind kind, const std::string &arg, Op &op, std::string *err);
std::string Describe(const Op &op);

// remap[old] = new id, -1 = gone. `count` slots before; *newCount after (an insert or a move past the end grows it).
// Move: the block leaves its place (later ids close up) and is inserted so that its first id becomes `to`.
// Place: the block goes to `to`..`to`+len-1, which must be empty (other than the block itself); it leaves empty ids.
// Insert: n empty ids at `to`; later ids shift up. Delete: the block goes, later ids close up. Clear: the block's ids
// stay as empty gaps. Swap: two ids trade places. `empty[id]` tells Place which slots are empty.
bool BuildRemap(int count, const Op &op, const std::vector<bool> &empty, std::vector<int> &remap, int *newCount, std::string *err);

// Moves the slots of a part set list (anything with a partId) through `remap`; slots no old id lands on are new
// empty slots. Trailing empty slots that hold no loaded record are trimmed.
template<class PS> void ApplyRemap(std::vector<PS> &sets, const std::vector<int> &remap, int newCount)
{
	std::vector<PS> out((size_t)newCount);
	for (int i = 0; i < newCount; i++) out[i].partId = i;
	for (size_t i = 0; i < sets.size() && i < remap.size(); i++)
		if (remap[i] >= 0 && remap[i] < newCount) { out[remap[i]] = std::move(sets[i]); out[remap[i]].partId = remap[i]; }
	while (!out.empty() && out.back().origId < 0 && out.back().groups.empty() && out.back().name.empty()) out.pop_back();
	sets.swap(out);
}

// --- users of a .pat on disk ---
struct UserFile {
	std::string path;                    // UTF-8
	bool shared = false;                 // also used with another .pat or outside the folder: never rewritten
	bool missing = false;                // listed but not shipped
	std::string error;                   // could not be read as an HA6
	std::vector<std::string> projects;   // project .txt files listing it
	std::vector<std::pair<uint32_t, int>> refs;   // (file offset of the id, part set id) of every usePat layer
};
struct Users {
	std::string patPath;
	std::vector<UserFile> files;
	int refCount() const { int n = 0; for (auto &f : files) n += (int)f.refs.size(); return n; }
};
Users FindUsers(const std::string &utf8PatPath);

// What a remap does to the users (disk ids -> new ids).
struct Plan {
	struct File { std::string path; int refs = 0, changed = 0; bool shared = false; };
	std::vector<File> files;            // every user file, with its references and how many change
	std::vector<std::string> refusals;  // non-empty: the operation must not be applied
	int changed = 0;
	bool ok() const { return refusals.empty(); }
};
// diskRemap[diskId] = new id (-1 = deleted); ids past its end keep their number. Build the remap over
// max(slot count, MaxRef + 1) ids so references past the end of the .pat (they draw nothing) move like empty slots.
Plan MakePlan(const Users &users, const std::vector<int> &diskRemap);
int MaxRef(const Users &users);   // highest part set id any user draws, -1 = none

// Rewrites the id bytes of every non-shared user file through diskRemap (atomic write; `backup` is called with the path
// before the first write of each file). Files with no change are not touched.
struct WriteResult { std::vector<std::string> written, errors; int refs = 0; };
WriteResult WriteUsers(const Users &users, const std::vector<int> &diskRemap, void (*backup)(const std::string &utf8Path, void *ctx), void *ctx);

// --- the Parts model ---
// Applies `op` to the part sets (one step: callers commit one undo step around it). `extent`: ids the operation must
// cover (MaxRef + 1 of the users), the model is padded with empty slots up to it so references past the end move too.
bool ApplyOp(Parts &parts, const Op &op, int extent, std::string *err);
// Disk id -> current id of every part set id since the last load / save (Parts::diskIdOfOrig and the slots' origId),
// over max(extent, ids on disk). Identity when nothing was renumbered.
std::vector<int> PendingDiskRemap(const Parts &parts, int extent);
bool IsIdentity(const std::vector<int> &remap);

} // namespace patrn
#endif
