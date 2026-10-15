#ifndef HMOD_HA6_H_GUARD
#define HMOD_HA6_H_GUARD
// Raw HA6 pattern-slot splitter + canonical slot hash for mod change tracking and conflict checks (AUTHORING_DESIGN.md
// section 4.2). Mirrors tools/uni/ha6walk.py and docs/modmanager/tools/ha6_semantic_diff.py (canonical body: names and
// PDS2 dropped, AF shorthands expanded, ASSM pool references resolved, tags sorted per frame) so a re-serialised editor
// file does not show as "everything changed". Pure C++, no framedata dependency: works on UNI2/MBTL .ha6 too (same tags).
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace hmod {

struct Ha6Slot {
	uint32_t id = 0;
	bool real = false;        // has at least one frame (FSTR); a frameless slot is a stub
	std::string name;         // PTT2/PTIT name bytes as stored (cp932 or utf-8, not decoded)
	std::string canon;        // lowercase hex SHA-256 of the canonical body ("" for a stub)
};
using Ha6Slots = std::map<uint32_t, Ha6Slot>;

bool IsHa6(const std::vector<uint8_t>& bytes);
// False + `err` on an unknown tag or a truncated file.
bool SplitHa6(const std::vector<uint8_t>& bytes, Ha6Slots& out, std::string& err);

enum class SlotDelta { Same, Filled, Cleared, Modified, NameOnly };
const char* SlotDeltaName(SlotDelta d);
// How slot `mod` differs from slot `base` (either may be null = absent). Two stubs / absent slots are Same.
SlotDelta CompareSlot(const Ha6Slot* base, const Ha6Slot* mod);

struct SlotDiffSummary { size_t filled = 0, cleared = 0, modified = 0, nameOnly = 0; std::vector<uint32_t> claimed; };
// Slots of `mod` that change behaviour against `base` (Filled / Cleared / Modified); name-only changes are counted, not claimed.
SlotDiffSummary DiffSlots(const Ha6Slots& base, const Ha6Slots& mod);

} // namespace hmod
#endif
