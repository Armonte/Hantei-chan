#include "pattern_refs.h"
#include "framedata.h"

#include <algorithm>
#include <set>

namespace patrefs {

namespace {

// A slot holding a pattern number, and how to read/write it.
struct Slot {
	int* value;         // the stored field
	int base;           // stored = target - base (owner index for relative refs, else 0)
	int offset;         // stored = target + offset (10000 for 10000+N jumps)
	const char* label;
	bool relative = false;
};

int Decode(const Slot& s) { return *s.value - s.offset + s.base; }

// Visit every pattern-reference slot of a frame. `owner` is the pattern index
// relative references are measured from.
template<class Fn>
void ForEachSlot(Frame& frame, int owner, Fn&& fn)
{
	// AF: aniType 0 (end) jumps to pattern `jump`.
	if (frame.AF.aniType == 0) fn(Kind::AFJump, -1, Slot{&frame.AF.jump, 0, 0, "AF end: next pattern"});

	for (int i = 0; i < (int)frame.EF.size(); ++i) {
		Frame_EF& ef = frame.EF[i];
		switch (ef.type) {
		case 1:    fn(Kind::Effect, i, Slot{&ef.number, 0, 0, "EF1 spawn pattern"}); break;
		case 1000: fn(Kind::Effect, i, Slot{&ef.number, 0, 0, "EF1000 spawn pattern once"}); break;
		case 11:   fn(Kind::Effect, i, Slot{&ef.number, 0, 0, "EF11 random spawn (base)"}); break;
		case 101:  fn(Kind::Effect, i, Slot{&ef.number, owner, 0, "EF101 spawn relative pattern", true}); break;
		case 111:  fn(Kind::Effect, i, Slot{&ef.number, owner, 0, "EF111 random relative spawn (base)", true}); break;
		default: break;
		}
	}

	for (int i = 0; i < (int)frame.IF.size(); ++i) {
		Frame_IF& c = frame.IF[i];
		int* p = c.parameters;
		switch (c.type) {
		case 1:  if (p[1] >= 0 && p[1] < 10000) fn(Kind::Condition, i, Slot{&p[1], 0, 0, "IF1 lever input: queue pattern"}); break;
		case 3:  if (p[0] >= 10000) fn(Kind::Condition, i, Slot{&p[0], 0, 10000, "IF3 branch on hit: queue 10000+N"}); break;
		case 14: if (p[0] >= 10000) fn(Kind::Condition, i, Slot{&p[0], 0, 10000, "IF14 box collision: queue 10000+N"}); break;
		case 25: if (p[0] >= 10000) fn(Kind::Condition, i, Slot{&p[0], 0, 10000, "IF25 variable: queue 10000+N"}); break;
		case 7:  if (p[2] >= 0) fn(Kind::Condition, i, Slot{&p[2], 0, 0, "IF7 lever & trigger: queue pattern"}); break;
		case 8:  if (p[2] && p[0] >= 0) fn(Kind::Condition, i, Slot{&p[0], 0, 0, "IF8 random: queue pattern"}); break;
		case 12: if ((p[2] & 2) && p[0] >= 0) fn(Kind::Condition, i, Slot{&p[0], 0, 0, "IF12 distance: queue pattern"}); break;
		case 13: if (p[0] >= 0) fn(Kind::Condition, i, Slot{&p[0], 0, 0, "IF13 screen edge: queue pattern"}); break;
		case 18: if (p[1] >= 0 && p[1] != 256) fn(Kind::Condition, i, Slot{&p[1], 0, 0, "IF18 owner pattern check"}); break;
		case 27: if (p[1] == 0 && p[0] >= 0) fn(Kind::Condition, i, Slot{&p[0], 0, 0, "IF27 owner thrown/hurt: queue pattern"}); break;
		case 37: if (p[0] >= 0) fn(Kind::Condition, i, Slot{&p[0], 0, 0, "IF37 queue pattern"}); break;
		default: break;
		}
	}
}

} // namespace

std::vector<Ref> Find(FrameData& fd, int target)
{
	std::vector<Ref> out;
	const int count = fd.get_sequence_count();
	for (int p = 0; p < count; ++p) {
		Sequence* seq = fd.get_sequence(p);
		if (!seq) continue;
		for (int f = 0; f < (int)seq->frames.size(); ++f) {
			ForEachSlot(seq->frames[f], p, [&](Kind kind, int index, const Slot& s) {
				const int t = Decode(s);
				if (target >= 0 && t != target) return;
				Ref r;
				r.pattern = p; r.frame = f; r.kind = kind; r.index = index; r.target = t; r.label = s.label;
				out.push_back(r);
			});
		}
	}
	return out;
}

std::string Describe(const Ref& r)
{
	std::string s = "Pattern " + std::to_string(r.pattern) + " frame " + std::to_string(r.frame);
	if (r.kind == Kind::Effect) s += " EF#" + std::to_string(r.index);
	if (r.kind == Kind::Condition) s += " IF#" + std::to_string(r.index);
	s += ": " + std::string(r.label) + " -> " + std::to_string(r.target);
	return s;
}

int Remap(FrameData& fd, const std::map<int, int>& targetMap,
          const std::vector<int>& scope, const std::map<int, int>& ownerOld)
{
	std::vector<int> patterns = scope;
	if (patterns.empty())
		for (int p = 0; p < fd.get_sequence_count(); ++p) patterns.push_back(p);

	int changed = 0;
	for (int p : patterns) {
		Sequence* seq = fd.get_sequence(p);
		if (!seq) continue;
		auto oldIt = ownerOld.find(p);
		const int origin = oldIt != ownerOld.end() ? oldIt->second : p;
		bool touched = false;
		for (auto& frame : seq->frames) {
			ForEachSlot(frame, origin, [&](Kind, int, const Slot& s) {
				const int oldTarget = Decode(s);
				auto m = targetMap.find(oldTarget);
				const int newTarget = m != targetMap.end() ? m->second : oldTarget;
				// Relative slots are rewritten against the owner's current index.
				const int newValue = newTarget + s.offset - (s.relative ? p : 0);
				if (newValue != *s.value) { *s.value = newValue; touched = true; ++changed; }
			});
		}
		if (touched) fd.mark_modified(p);
	}
	return changed;
}

bool IsEmptySlot(FrameData& fd, int pattern)
{
	Sequence* seq = fd.get_sequence(pattern);
	return seq && seq->frames.empty() && seq->name.empty() && seq->codeName.empty();
}

std::vector<int> PlanSlots(FrameData& fd, int count, int start, Placement how,
                           const std::vector<int>& sourceIds)
{
	std::vector<int> out(count, -1);
	const int n = fd.get_sequence_count();
	if (how == Placement::OriginalIds) {
		for (int i = 0; i < count && i < (int)sourceIds.size(); ++i)
			out[i] = sourceIds[i] >= 0 && sourceIds[i] < n ? sourceIds[i] : -1;
		return out;
	}
	int slot = start;
	for (int i = 0; i < count; ++i) {
		if (how == Placement::NextEmpty)
			while (slot < n && !IsEmptySlot(fd, slot)) ++slot;
		out[i] = slot < n && slot >= 0 ? slot : -1;
		++slot;
	}
	return out;
}

int PastePatterns(FrameData& fd, const std::vector<Sequence>& patterns,
                  const std::vector<int>& sourceIds, const std::vector<int>& slots, bool remapRefs)
{
	std::map<int, int> targetMap, ownerOld;
	std::vector<int> scope;
	for (size_t i = 0; i < patterns.size() && i < slots.size(); ++i) {
		Sequence* dst = fd.get_sequence(slots[i]);
		if (!dst) continue;
		*dst = patterns[i];
		dst->empty = false;
		fd.mark_modified(slots[i]);
		scope.push_back(slots[i]);
		if (i < sourceIds.size() && sourceIds[i] >= 0) {
			targetMap[sourceIds[i]] = slots[i];
			ownerOld[slots[i]] = sourceIds[i];
		}
	}
	if (!remapRefs || scope.empty()) return 0;
	return Remap(fd, targetMap, scope, ownerOld);
}

int MovePatterns(FrameData& fd, const std::vector<int>& from, const std::vector<int>& to, bool remapRefs)
{
	std::vector<Sequence> moving;
	std::vector<int> src, dst;
	for (size_t i = 0; i < from.size() && i < to.size(); ++i) {
		Sequence* s = fd.get_sequence(from[i]);
		if (!s || !fd.get_sequence(to[i])) continue;
		moving.push_back(*s);
		src.push_back(from[i]);
		dst.push_back(to[i]);
	}
	const std::set<int> dstSet(dst.begin(), dst.end());
	for (int s : src) {
		if (dstSet.count(s)) continue;
		*fd.get_sequence(s) = Sequence{};
		fd.mark_modified(s);
	}
	std::map<int, int> targetMap, ownerOld;
	for (size_t i = 0; i < moving.size(); ++i) {
		*fd.get_sequence(dst[i]) = moving[i];
		fd.mark_modified(dst[i]);
		targetMap[src[i]] = dst[i];
		ownerOld[dst[i]] = src[i];
	}
	if (!remapRefs) return 0;
	return Remap(fd, targetMap, {}, ownerOld);
}

} // namespace patrefs
