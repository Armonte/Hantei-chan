#include "var_refs.h"
#include "framedata.h"

#include <cstdlib>

namespace varrefs {

// Parameter slots follow the effect/condition panels (frame_disp_*), which
// follow the MBAA handlers (docs/HANTEI_PREVIEW_SIM.md for EF1/11/1000).
const std::vector<Rule>& Rules()
{
	static const std::vector<Rule> rules = {
		{true,  1,    -1,  8, Encoding::Direct, catProjectile, true , "EF1 spawn pattern: projectile var (p9)"},
		{true,  101,  -1,  8, Encoding::Direct, catProjectile, true , "EF101 spawn relative: projectile var (p9)"},
		{true,  1000, -1,  8, Encoding::Direct, catProjectile, true , "EF1000 spawn once: projectile var (p9)"},
		{true,  1000, -1,  5, Encoding::Direct, catOnceGuard, false, "EF1000 spawn once: once-guard var (p6)"},
		{true,  11,   -1, 10, Encoding::Direct, catProjectile, true , "EF11 random spawn: projectile var (p11)"},
		{true,  111,  -1, 10, Encoding::Direct, catProjectile, true , "EF111 random spawn: projectile var (p11)"},
		{true,  2,   100,  0, Encoding::Tens,   catProjectile, false, "EF2/100 increase projectile var (p1 tens)"},
		{true,  2,   101,  0, Encoding::Tens,   catProjectile, false, "EF2/101 decrease projectile var (p1 tens)"},
		{true,  2,   105,  0, Encoding::Direct, catVariable, false, "EF2/105 change variable (p1)"},
		{false, 2,    -1,  3, Encoding::Direct, catProjectile, true , "IF2 despawn: projectile var decrease (p4)"},
		{false, 3,    -1,  3, Encoding::Direct, catProjectile, true , "IF3 branch on hit: projectile var decrease (p4)"},
		{false, 24,   -1,  1, Encoding::Tens,   catProjectile, false, "IF24 projectile flag check (p2 tens)"},
		{false, 25,   -1,  1, Encoding::Direct, catVariable, false, "IF25 variable comparison (p2)"},
		{false, 31,   -1,  1, Encoding::Direct, catVariable, false, "IF31 change variable on command (p2)"},
		{false, 38,   -1,  3, Encoding::Direct, catVariable, false, "IF38 change variable on hit: extra var (p4)"},
	};
	return rules;
}

static int Decode(const Rule& r, int raw)
{
	return r.encoding == Encoding::Tens ? raw / 10 : raw;
}

static bool Encode(const Rule& r, int raw, int newId, int* out)
{
	if (r.zeroIsNone && newId == 0) return false; // would read as "no variable"
	if (r.encoding == Encoding::Direct) { *out = newId; return true; }
	if (newId < 0 || raw < 0) return false;
	*out = newId * 10 + raw % 10;
	return true;
}

template<class Fn>
static void ForEachSlot(FrameData& fd, unsigned categories, Fn&& fn)
{
	const auto& rules = Rules();
	const int count = fd.get_sequence_count();
	for (int p = 0; p < count; ++p) {
		Sequence* seq = fd.get_sequence(p);
		if (!seq) continue;
		for (int f = 0; f < (int)seq->frames.size(); ++f) {
			Frame& frame = seq->frames[f];
			for (int i = 0; i < (int)frame.EF.size(); ++i) {
				Frame_EF& ef = frame.EF[i];
				for (int ri = 0; ri < (int)rules.size(); ++ri) {
					const Rule& r = rules[ri];
					if (!r.isEffect || !(r.category & categories) || r.type != ef.type) continue;
					if (r.number >= 0 && r.number != ef.number) continue;
					fn(p, f, true, i, ri, ef.parameters[r.param]);
				}
			}
			for (int i = 0; i < (int)frame.IF.size(); ++i) {
				Frame_IF& cond = frame.IF[i];
				for (int ri = 0; ri < (int)rules.size(); ++ri) {
					const Rule& r = rules[ri];
					if (r.isEffect || !(r.category & categories) || r.type != cond.type) continue;
					fn(p, f, false, i, ri, cond.parameters[r.param]);
				}
			}
		}
	}
}

std::vector<Ref> Find(FrameData& fd, int varId, unsigned categories)
{
	std::vector<Ref> out;
	const auto& rules = Rules();
	ForEachSlot(fd, categories, [&](int p, int f, bool isEffect, int i, int ri, int& value) {
		const int id = Decode(rules[ri], value);
		if (rules[ri].zeroIsNone && value == 0) return;
		if (varId >= 0 && id != varId) return;
		Ref r;
		r.pattern = p; r.frame = f; r.isEffect = isEffect; r.index = i; r.rule = ri;
		r.raw = value; r.varId = id;
		out.push_back(r);
	});
	return out;
}

int Replace(FrameData& fd, int from, int to, unsigned categories, int* skipped)
{
	int changed = 0, skip = 0;
	const auto& rules = Rules();
	ForEachSlot(fd, categories, [&](int p, int, bool, int, int ri, int& value) {
		if (rules[ri].zeroIsNone && value == 0) return;
		if (Decode(rules[ri], value) != from) return;
		int nv;
		if (!Encode(rules[ri], value, to, &nv)) { ++skip; return; }
		if (nv == value) return;
		value = nv;
		fd.mark_modified(p);
		++changed;
	});
	if (skipped) *skipped = skip;
	return changed;
}

std::string Describe(const Ref& r)
{
	const auto& rules = Rules();
	std::string s = "Pattern " + std::to_string(r.pattern) + " frame " + std::to_string(r.frame) +
		(r.isEffect ? " EF#" : " IF#") + std::to_string(r.index);
	if (r.rule >= 0 && r.rule < (int)rules.size()) s += ": " + std::string(rules[r.rule].label);
	s += "  [var " + std::to_string(r.varId) + ", raw " + std::to_string(r.raw) + "]";
	return s;
}

} // namespace varrefs
