#ifndef CHAR_DIFF_H_GUARD
#define CHAR_DIFF_H_GUARD

// Character diff (issue #131): compares two characters' pattern tables by value, pattern by pattern (same index on both
// sides). A pattern that exists on one side only (frames, a name or a code name) is Added / Removed; one that exists on
// both is Changed when its header (name, code name, PSTS/PLVL/PFLG/PUPS), frame count, or any frame's AF / AS / AT / EF /
// IF data or boxes differ. Editor state (the modified flag, encoding choices kept for byte-exact saving, notes) is not data
// and is ignored. Pure model code (no ImGui), so tests and the UI script can run it.
#include "framedata.h"
#include <cstdint>
#include <string>
#include <vector>

namespace chardiff {

enum class Kind : uint8_t { Same = 0, Changed = 1, Added = 2, Removed = 3 };
const char* KindName(Kind k);   // "same" / "changed" / "added" / "removed"

// What differs (bit set).
enum Area : uint32_t {
	aHeader = 1u << 0,      // name, code name, PSTS / PLVL / PFLG / PUPS
	aFrameCount = 1u << 1,  // frame count (frames past the shorter side are reported with this bit)
	aAF = 1u << 2,          // animation: layers (sprite, offset, colour, scale, rotation...), duration, jumps, loops
	aAS = 1u << 3,          // state: movement, cancels, invincibility, flags
	aAT = 1u << 4,          // attack: damage, guard, vectors, hit stop...
	aEF = 1u << 5,          // effects
	aIF = 1u << 6,          // conditions
	aBoxes = 1u << 7,       // hit / hurt / collision boxes
};
// Short English names of the bits ("header", "frame count", "AF", "AS", "AT", "EF", "IF", "boxes"), comma separated.
std::string AreaNames(uint32_t areas, const char* (*tr)(const char*) = nullptr);

struct FrameDiff {
	int frame = 0;
	uint32_t areas = 0;
};

struct PatternDiff {
	int pattern = 0;
	Kind kind = Kind::Same;
	uint32_t areas = 0;               // union of the header and frame areas
	int framesMine = 0, framesOther = 0;
	std::vector<FrameDiff> frames;    // the differing frames, ascending
};

struct Result {
	std::vector<PatternDiff> patterns;   // differing patterns only, ascending
	std::vector<Kind> kindOf;            // per pattern index (max of both sides' counts)
	int changed = 0, added = 0, removed = 0;
	int framesDiffering = 0;
	double ms = 0.0;
	bool valid = false;
	Kind kind(int pattern) const { return pattern >= 0 && pattern < (int)kindOf.size() ? kindOf[pattern] : Kind::Same; }
	const PatternDiff* find(int pattern) const;
	int total() const { return changed + added + removed; }
};

bool PatternExists(const Sequence* s);

// Compares `mine` against `other` (the reference side, e.g. vanilla).
Result Compare(const std::vector<Sequence>& mine, const std::vector<Sequence>& other);
// Fast check of one pattern (null = missing slot). Fills `out` when given.
bool ComparePattern(const Sequence* mine, const Sequence* other, int index, PatternDiff* out);

// "3 frames: AT, boxes; frame count 5 -> 6; header" (English, or through tr for the UI).
std::string Describe(const PatternDiff& d, const char* (*tr)(const char*) = nullptr);

// Row / text colour of a kind (RGBA 0..1): amber changed, green added, red removed; darker shades on a light background.
// False for Kind::Same (no colour).
bool KindColor(Kind k, bool lightBackground, float rgba[4]);

// Field-level differences, for the detail view (only computed on demand).
struct FieldDiff {
	uint32_t area = 0;
	std::string field, mine, other;
};
std::vector<FieldDiff> HeaderFields(const Sequence* mine, const Sequence* other);
// a or b may be null (frame missing on that side): every field of the other one is listed then.
std::vector<FieldDiff> FrameFields(const Frame* mine, const Frame* other);

// Shared with the pattern lists (MainPane): the differing patterns of the character the diff belongs to (and the reverse
// marks for the other side when it is open in a view). Set by the main frame; cleared when the diff is cleared.
struct ListMarks {
	const FrameData* mine = nullptr;    // marks apply to this pattern table
	const FrameData* other = nullptr;   // ... and to this one (the other side's tab), with added / removed swapped
	const std::vector<Kind>* kinds = nullptr;
	bool onlyChanged = false;           // pattern lists show only the differing patterns (and the selected one)
	Kind kindFor(const FrameData* fd, int pattern) const;
	bool active(const FrameData* fd) const { return kinds && fd && (fd == mine || fd == other); }
};
ListMarks& Marks();

} // namespace chardiff

#endif
