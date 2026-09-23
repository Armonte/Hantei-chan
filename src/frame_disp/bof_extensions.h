#ifndef FRAME_DISP_BOF_EXTENSIONS_H_GUARD
#define FRAME_DISP_BOF_EXTENSIONS_H_GUARD

// Extended Melty / BOF-only frame-data panels (ported from Gonptechan EX 1abc27f9,
// frame_disp_if.h / effect_misc.h). They appear only when Preferences > Extension profile
// is "Extended Melty / BOF"; the vanilla profile shows none of these IDs.
//
// Vanilla facts (MBAA.exe): the IF dispatcher ends at 151 (152-157 are no-ops) and EF6 154 is
// a no-op. EF6 105 modes 10/11 ("self" set/add) ARE vanilla (Effect6 case 105 at 0x45e8d4:
// p3 % 10 = set/add, p3 / 10 != 0 = write the actor itself instead of its owner), so they stay
// in the vanilla panel; only the Var6 projectile-level presets are BOF.

#include "frame_disp_common.h"
#include "../extension_profile.h"

#include <vector>

// Condition labels indexed by type. Extended adds 152-157.
static inline const std::vector<const char*>& ConditionTypeLabels()
{
	static std::vector<const char*> vanilla(conditionTypes, conditionTypes + IM_ARRAYSIZE(conditionTypes));
	static std::vector<const char*> extended = [] {
		std::vector<const char*> v(conditionTypes, conditionTypes + IM_ARRAYSIZE(conditionTypes));
		v.push_back("152: (No-op in MBAACC)");
		v.push_back("153: (No-op in MBAACC)");
		v.push_back("154: Custom variable compare + jump (BOF)");
		v.push_back("155: Custom variable write on command (BOF)");
		v.push_back("156: Custom variable write on hit (BOF)");
		v.push_back("157: Tagged Special Box collision (BOF)");
		return v;
	}();
	return ExtendedProfileEnabled() ? extended : vanilla;
}

// EF6 sub-type labels: adds "154" after 153 under the Extended profile.
static inline std::vector<const char*> Effect6TypeLabels(const char* const* base, int count)
{
	std::vector<const char*> out(base, base + count);
	if (ExtendedProfileEnabled()) {
		auto at = out.begin();
		while (at != out.end() && atoi(*at) < 154) ++at;
		out.insert(at, "154: Custom variable write (BOF ABI v1)");
	}
	return out;
}

namespace bof {

static const char* const kBanks[] = { "0: Current owner", "1: Opponent", "2: Explicit P1", "3: Explicit P2" };
static const char* const kCompare[] = { "0: Equal", "1: Not equal", "2: Less", "3: Less or equal", "4: Greater", "5: Greater or equal" };
static const char* const kWriteOps[] = { "0: Set", "1: Add (signed 16-bit wraparound)" };

static inline void Warn(const char* text) { im::TextColored(ImVec4(1.f, .35f, .25f, 1.f), "%s", text); }

static inline void Int(const char* label, int* v, float width, const std::function<void()>& markModified)
{
	im::SetNextItemWidth(width);
	if (im::InputInt(label, v, 0, 0)) markModified();
}

static inline void ClearReserved(int* p, int from, int to, const std::function<void()>& markModified)
{
	bool clear = true;
	for (int n = from; n < to; ++n) clear &= p[n] == 0;
	if (clear) return;
	im::TextColored(ImVec4(1.f, .35f, .25f, 1.f), "Param%d..Param%d are reserved and must be zero.", from + 1, to);
	if (im::Button("Clear reserved parameters")) { for (int n = from; n < to; ++n) p[n] = 0; markModified(); }
}

static inline void JumpField(int* v, float width, FrameData* frameData, const std::function<void()>& markModified)
{
	Int("Jump to", v, width, markModified);
	if (frameData && *v >= 10000 && *v - 10000 < frameData->get_sequence_count()) {
		im::SameLine();
		im::TextDisabled("[queue %s]", frameData->GetDecoratedName(*v - 10000).c_str());
	}
}

// IF 14 extension: collider stance / guard / source filters packed in Param6..Param8.
static inline void DrawIf14Filters(int* p, float width, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled()) return;
	im::Spacing();
	if (!im::TreeNode("Extended Melty / BOF collider filters")) return;
	im::TextDisabled("Needs a BOF executable; all zero keeps vanilla behaviour.");
	int guardMode = p[7] & 3;
	int sourceMask = (p[7] >> 3) & 7;
	auto store = [&]() { p[7] = (guardMode & 3) | ((sourceMask & 7) << 3); };
	auto mask = [&](const char* label, int* m, int bit) {
		bool on = (*m & bit) != 0;
		if (im::Checkbox(label, &on)) { if (on) *m |= bit; else *m &= ~bit; markModified(); }
	};
	im::TextUnformatted("Collider stance (any selected):");
	mask("Standing##If14Stance", &p[5], 1); im::SameLine();
	mask("Crouching##If14Stance", &p[5], 2); im::SameLine();
	mask("Airborne##If14Stance", &p[5], 4);
	const char* const guardModes[] = { "0: Disabled", "1: Contains all selected", "2: Exactly selected - no extras" };
	im::SetNextItemWidth(width * 2.5f);
	if (ShowComboWithManual("Guard match", &guardMode, guardModes, IM_ARRAYSIZE(guardModes), width * 2.5f, width)) { store(); markModified(); }
	im::TextUnformatted("Attack is guardable as:");
	mask("Stand##If14Guard", &p[6], 1); im::SameLine();
	mask("Crouch##If14Guard", &p[6], 2); im::SameLine();
	mask("Air##If14Guard", &p[6], 4);
	im::TextUnformatted("Collider source (any selected):");
	int src = sourceMask;
	mask("Main body##If14Source", &src, 1); im::SameLine();
	mask("Assist / partner##If14Source", &src, 2); im::SameLine();
	mask("Spawned effect actor##If14Source", &src, 4);
	if (src != sourceMask) { sourceMask = src; store(); markModified(); }
	if ((p[5] & ~7) || (p[6] & ~7) || (p[7] & ~0x3b)) Warn("Invalid extension value: the patched game rejects this condition.");
	else if ((guardMode == 0) != (p[6] == 0)) im::TextColored(ImVec4(1.f, .75f, .2f, 1.f), "Guard mode and guard boxes must both be disabled or both enabled.");
	else if (p[8] != 0) im::TextColored(ImVec4(1.f, .75f, .2f, 1.f), "Param9 is unused; clear legacy data there.");
	else im::TextDisabled("Enabled filter groups combine as AND. Add another IF 14 row for OR.");
	im::TreePop();
}

// IF 154-157. Returns false (draw nothing) under the vanilla profile.
static inline bool DrawCondition(int type, int* p, float width, FrameData* frameData, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled() || type < 154 || type > 157) return false;
	switch (type) {
	case 154:
		im::Text("--- Custom variable compare + jump (BOF ABI v1) ---");
		JumpField(&p[0], width, frameData, markModified);
		Int("Variable index", &p[1], width, markModified);
		if (p[1] < 0 || p[1] > 1023) Warn("Invalid index: use 0..1023.");
		Int("Signed compare value", &p[2], width, markModified);
		if (ShowComboWithManual("Comparison", &p[3], kCompare, IM_ARRAYSIZE(kCompare), width * 2, width)) markModified();
		if (ShowComboWithManual("Bank", &p[4], kBanks, IM_ARRAYSIZE(kBanks), width * 2, width)) markModified();
		if (p[4] < 0 || p[4] > 3) Warn("Invalid bank: use 0..3; the condition fails closed.");
		ClearReserved(p, 5, 9, markModified);
		break;
	case 155: {
		im::Text("--- Custom variable write on command (BOF ABI v1) ---");
		Int("Move ID", &p[0], width, markModified);
		if (frameData) {
			if (Command* cmd = frameData->get_command(p[0])) { im::SameLine(); im::TextDisabled("[%s]", cmd->input.c_str()); }
		}
		Int("Variable index", &p[1], width, markModified);
		if (p[1] < 0 || p[1] > 1023) Warn("Invalid index: use 0..1023.");
		Int("Signed value", &p[2], width, markModified);
		if (ShowComboWithManual("Operation", &p[3], kWriteOps, IM_ARRAYSIZE(kWriteOps), width * 2, width)) markModified();
		if (ShowComboWithManual("Bank", &p[4], kBanks, IM_ARRAYSIZE(kBanks), width * 2, width)) markModified();
		if (p[4] < 0 || p[4] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
		ClearReserved(p, 5, 9, markModified);
		break;
	}
	case 156: {
		im::Text("--- Custom variable write on hit (BOF ABI v1) ---");
		Int("Signed value", &p[0], width, markModified);
		const char* const when[] = { "0: On hit", "1: On hit/block", "2: On hit/clash", "3: On hit/block/clash", "5: On block", "6: On clash", "7: On block/clash" };
		if (ShowComboWithManual("When", &p[1], when, IM_ARRAYSIZE(when), width * 2, width)) markModified();
		im::SetNextItemWidth(width * 2.5f);
		if (im::Combo("Opponent state", &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList))) markModified();
		Int("Variable index", &p[3], width, markModified);
		if (p[3] < 0 || p[3] > 1023) Warn("Invalid index: use 0..1023.");
		if (ShowComboWithManual("Operation", &p[4], kWriteOps, IM_ARRAYSIZE(kWriteOps), width * 2, width)) markModified();
		if (ShowComboWithManual("Bank", &p[5], kBanks, IM_ARRAYSIZE(kBanks), width * 2, width)) markModified();
		if (p[5] < 0 || p[5] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
		ClearReserved(p, 6, 9, markModified);
		break;
	}
	case 157: {
		im::Text("--- Tagged Special Box collision (BOF extension) ---");
		im::TextDisabled("A BOF projectile-level mechanism, not 2v2 tag.");
		if (im::Button("Apply projectile-level preset (Var6)")) {
			p[1] = 0; p[2] = 1018; p[3] = 6; p[4] = 6; p[5] = 5; p[6] = 4; p[7] = 0; p[8] = 0;
			markModified();
		}
		if (im::IsItemHovered()) Tooltip("Opposing-team Special Box 16, non-consuming; candidate Var6 >= self Var6; spawned effects only.\nLevels by convention: 10 / 20 / 30 (set with EF6 105).");
		JumpField(&p[0], width, frameData, markModified);
		const char* const targets[] = { "0: Enemies only", "1: Allies only", "2: Both" };
		if (ShowComboWithManual("Check target", &p[1], targets, IM_ARRAYSIZE(targets), width * 2, width)) markModified();
		Int("Candidate box", &p[2], width, markModified);
		im::SameLine(); im::TextDisabled("(IF 14 box codes, e.g. 1018 = Special Box 16, non-consuming)");
		Int("Self ExtraVar index", &p[3], width, markModified);
		Int("Candidate ExtraVar index", &p[4], width, markModified);
		if (ShowComboWithManual("Candidate compared with self", &p[5], kCompare, IM_ARRAYSIZE(kCompare), width * 2.5f, width)) markModified();
		im::TextUnformatted("Eligible candidate object types (any selected):");
		auto src = [&](const char* label, int bit) {
			bool on = (p[6] & bit) != 0;
			if (im::Checkbox(label, &on)) { if (on) p[6] |= bit; else p[6] &= ~bit; markModified(); }
		};
		src("Main body##If157", 1); im::SameLine();
		src("Assist / partner##If157", 2); im::SameLine();
		src("Spawned effect actor##If157", 4);
		const bool valid = p[1] >= 0 && p[1] <= 2 && p[3] >= 0 && p[3] <= 9 && p[4] >= 0 && p[4] <= 9 && p[5] >= 0 && p[5] <= 5 && p[6] >= 1 && p[6] <= 7 && p[7] == 0 && p[8] == 0;
		if (!valid) Warn("Invalid field: indexes 0..9, comparison 0..5, source mask 1..7, Param8/9 zero. The patched game fails closed.");
		break;
	}
	}
	return true;
}

// EF6 154. Returns false under the vanilla profile.
static inline bool DrawEffect154(int* p, float width, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled()) return false;
	im::Text("--- Custom variable write (BOF ABI v1) ---");
	im::TextDisabled("Bank 0 keeps the original owner-relative behaviour.");
	Int("Variable index", &p[0], width, markModified);
	if (p[0] < 0 || p[0] > 1023) Warn("Invalid index: ABI v1 accepts 0..1023; nothing is written.");
	Int("Signed value", &p[1], width, markModified);
	if (p[1] < -32768 || p[1] > 32767) im::TextColored(ImVec4(1.f, .7f, 0.f, 1.f), "Outside int16; the stored result wraps.");
	if (ShowComboWithManual("Operation", &p[2], kWriteOps, IM_ARRAYSIZE(kWriteOps), width * 3, width)) markModified();
	if (p[2] < 0 || p[2] > 1) Warn("Invalid operation: 0 Set or 1 Add only.");
	if (ShowComboWithManual("Bank", &p[3], kBanks, IM_ARRAYSIZE(kBanks), width * 2, width)) markModified();
	if (p[3] < 0 || p[3] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
	ClearReserved(p, 4, 12, markModified);
	return true;
}

// EF6 105 projectile-level presets (BOF convention: ExtraVar 6 on the spawned effect).
static inline void DrawVar6Presets(int* p, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled()) return;
	im::TextColored(ImVec4(1.f, .75f, .2f, 1.f), "BOF projectile level (pairs with the IF 157 preset):");
	for (int level = 1; level <= 3; ++level) {
		char label[48];
		snprintf(label, sizeof(label), "Level %d (%d)##Var6Level", level, level * 10);
		if (level > 1) im::SameLine();
		if (im::Button(label)) { p[0] = 6; p[1] = level * 10; p[2] = 10; markModified(); }
	}
	if (im::IsItemHovered()) Tooltip("Sets variable 6 on the effect itself (mode 10 = self set).");
}

} // namespace bof

#endif
