#ifndef VAR_REFS_H_GUARD
#define VAR_REFS_H_GUARD

// Variable references in effects and conditions, for the batch replace tool
// (issue #75). Each rule names one parameter that holds a variable ID. Some
// parameters pack the ID in the tens place and a value in the ones place
// (EF 2/100-101, IF 24); the rule knows its encoding, so a replace keeps the
// value digit.
#include <string>
#include <vector>

class FrameData;

namespace varrefs {

enum class Encoding { Direct, Tens };

enum Category : unsigned {
	catVariable   = 1u << 0, // general per-actor variables (EF2/105, IF25, IF31, IF38)
	catProjectile = 1u << 1, // projectile variables (EF1/11 proj var, IF2/3 decrease, EF2/100-101, IF24)
	catOnceGuard  = 1u << 2, // EF1000 spawn-once guard
	catAll        = 0xffffffffu,
};

struct Rule {
	bool isEffect;     // EF record (else IF)
	int type;          // record type
	int number;        // EF "number" (subtype) or -1 for any
	int param;         // parameter index (0-based)
	Encoding encoding;
	Category category;
	bool zeroIsNone;   // 0 means "no variable" here, so it is never a match
	const char* label;
};

const std::vector<Rule>& Rules();

struct Ref {
	int pattern = -1;
	int frame = -1;
	bool isEffect = false;
	int index = -1;    // index in the frame's EF or IF list
	int rule = -1;     // index into Rules()
	int raw = 0;       // stored parameter value
	int varId = 0;     // decoded variable ID
};

// Every reference to `varId` (or to any ID when varId < 0) in the rules
// selected by `categories`.
std::vector<Ref> Find(FrameData& fd, int varId, unsigned categories = catAll);

// Rewrites every reference to `from` as `to` (encoding-aware). Marks the
// touched patterns modified and returns how many parameters changed. Values
// that cannot hold `to` (a tens place with to < 0, or 0 in a slot where 0
// means "none") are skipped and counted in *skipped.
int Replace(FrameData& fd, int from, int to, unsigned categories = catAll, int* skipped = nullptr);

std::string Describe(const Ref& r);

} // namespace varrefs

#endif /* VAR_REFS_H_GUARD */
