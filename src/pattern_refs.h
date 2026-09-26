#ifndef PATTERN_REFS_H_GUARD
#define PATTERN_REFS_H_GUARD

// Pattern references and pattern moves/copies with reference remapping
// (issues #44, #42). A reference is any field that names another pattern:
// the AF end jump, spawn effects (absolute EF1/1000/11, relative EF101/111)
// and the condition fields that queue a pattern. Each rule knows its encoding
// (absolute, owner-relative, 10000+N), so moving patterns can rewrite callers.
#include <map>
#include <string>
#include <vector>

#include "framedata.h"

namespace patrefs {

enum class Kind { AFJump, Effect, Condition };

struct Ref {
	int pattern = -1;   // pattern holding the reference
	int frame = -1;
	Kind kind = Kind::AFJump;
	int index = -1;     // EF/IF index in the frame
	int target = -1;    // referenced pattern (absolute)
	const char* label = "";
};

// All references (to `target`, or to any pattern when target < 0).
std::vector<Ref> Find(FrameData& fd, int target = -1);

std::string Describe(const Ref& r);

// Rewrites references. `targetMap` maps old pattern index -> new index. Only
// records inside `scope` (current pattern indices; all patterns when empty)
// are touched. `ownerOld` gives, for a pattern in scope, the index it had
// when its relative references were written (EF101/111 are relative to their
// owner); patterns not in it are their own origin. Returns fields changed.
int Remap(FrameData& fd, const std::map<int, int>& targetMap,
          const std::vector<int>& scope = {}, const std::map<int, int>& ownerOld = {});

// Placement for paste/move (issue #42).
enum class Placement {
	NextEmpty,     // from `start`, each pattern goes into the next empty slot
	Consecutive,   // start, start+1, ... overwriting what is there
	OriginalIds,   // back into the ids they were copied from
};

// Destination slot for each of `count` patterns. `sourceIds` is used by
// OriginalIds. Slots past the end of the character are -1.
std::vector<int> PlanSlots(FrameData& fd, int count, int start, Placement how,
                           const std::vector<int>& sourceIds);

bool IsEmptySlot(FrameData& fd, int pattern);

// Copy `patterns` (taken from ids `sourceIds`, possibly of another character)
// into `slots` (-1 = skipped). With remapRefs, references inside the pasted
// patterns that point at another pasted pattern follow it to its new slot,
// and relative spawns keep their absolute target. Returns references changed.
int PastePatterns(FrameData& fd, const std::vector<Sequence_T<std::allocator>>& patterns,
                  const std::vector<int>& sourceIds, const std::vector<int>& slots, bool remapRefs);

// Move patterns `from[i]` -> `to[i]` inside one character. Source slots that
// are not also destinations are cleared. With remapRefs every reference in
// the character to a moved pattern follows it. Returns references changed.
int MovePatterns(FrameData& fd, const std::vector<int>& from, const std::vector<int>& to, bool remapRefs);

} // namespace patrefs

#endif /* PATTERN_REFS_H_GUARD */
