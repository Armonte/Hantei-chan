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
#include "effects/effect_form.h"
#include "../extension_profile.h"

#include <vector>
#include "../i18n.h"

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
		v.push_back("158: Gameplay predicate (BOF)");
		v.push_back("159: Runtime throw-rule modifier (BOF)");
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
		at = out.begin();
		while (at != out.end() && atoi(*at) < 160) ++at;
		at = out.insert(at, "160: Scoped interaction (BOF)");
		at = out.insert(at + 1, "161: Set target pose (BOF)");
		out.insert(at + 1, "162: Release target + HitVector (BOF)");
	}
	return out;
}

namespace bof {

// Label-first rows (effects/effect_form.h EfRow): label column, then the field filling the pane. The callers close any
// open label-form table first (frame_disp_if.h FormBreak), so these rows sit outside a table like the EF smart UIs.

static const char* const kBanks[] = { "0: Current owner", "1: Opponent", "2: Explicit P1", "3: Explicit P2" };
static const char* const kCompare[] = { "0: Equal", "1: Not equal", "2: Less", "3: Less or equal", "4: Greater", "5: Greater or equal" };
static const char* const kWriteOps[] = { "0: Set", "1: Add (signed 16-bit wraparound)" };

static inline void Colored(const ImVec4& col, const char* text)
{
	im::PushStyleColor(ImGuiCol_Text, col);
	im::PushTextWrapPos(0.f);
	im::TextUnformatted(text);
	im::PopTextWrapPos();
	im::PopStyleColor();
}
static inline void Warn(const char* text) { Colored(ImVec4(1.f, .35f, .25f, 1.f), TXT(text)); }
static inline void Caution(const char* text) { Colored(ImVec4(1.f, .75f, .2f, 1.f), TXT(text)); }
static inline void Hint(const char* text) { im::PushTextWrapPos(0.f); im::TextDisabled("%s", TXT(text)); im::PopTextWrapPos(); }

static inline void Int(const char* label, int* v, float, const std::function<void()>& markModified)
{
	if (im::InputInt(EfRow(LBL(label)), v, 0, 0)) markModified();
}

static inline bool Combo(const char* label, int* v, const char* const* items, int count, float width)
{
	return EfComboManual(label, v, items, count, 0.f, width);
}

// A row whose value column holds a group of bit checkboxes. `key` is the existing translated "Name:" text; the colon is
// dropped for the label column. Returns the value column x for MaskBox.
static inline float MaskRow(const char* key)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", TXT(key));
	size_t n = strlen(buf);
	while (n > 0 && (buf[n - 1] == ':' || buf[n - 1] == ' ' || (n >= 3 && (unsigned char)buf[n - 3] == 0xEF && (unsigned char)buf[n - 2] == 0xBC && (unsigned char)buf[n - 1] == 0x9A))) {
		if (buf[n - 1] == ':' || buf[n - 1] == ' ') --n; else n -= 3; // ASCII or full-width colon
	}
	snprintf(buf + n, sizeof(buf) - n, "##%s", key);
	EfRow(buf);
	return im::GetCursorPosX();
}

// One checkbox of a MaskRow group: on the row's line while it fits, otherwise on the next line in the value column.
static inline bool MaskBox(const char* label, int* m, int bit, bool first, float x0)
{
	if (!first) {
		const float w = im::GetFrameHeight() + im::GetStyle().ItemInnerSpacing.x + im::CalcTextSize(label, nullptr, true).x;
		im::SameLine();
		if (im::GetContentRegionAvail().x < w + ef_form::HelpGutterW()) { im::NewLine(); im::SetCursorPosX(x0); }
	}
	bool on = (*m & bit) != 0;
	if (!im::Checkbox(label, &on)) return false;
	if (on) *m |= bit; else *m &= ~bit;
	return true;
}

static inline void ClearReserved(int* p, int from, int to, const std::function<void()>& markModified)
{
	bool clear = true;
	for (int n = from; n < to; ++n) clear &= p[n] == 0;
	if (clear) return;
	char msg[160];
	snprintf(msg, sizeof(msg), TXT("Param%d..Param%d are reserved and must be zero."), from + 1, to);
	Colored(ImVec4(1.f, .35f, .25f, 1.f), msg);
	if (im::Button(LBL("Clear reserved parameters"))) { for (int n = from; n < to; ++n) p[n] = 0; markModified(); }
}

static inline void JumpField(int* v, float width, FrameData* frameData, const std::function<void()>& markModified)
{
	Int("Jump to", v, width, markModified);
	if (frameData && *v >= 10000 && *v - 10000 < frameData->get_sequence_count()) {
		char note[256];
		snprintf(note, sizeof(note), TXT("[queue %s]"), frameData->GetDecoratedName(*v - 10000).c_str());
		EfNote(note);
	}
}

// IF 14 extension: collider stance / guard / source filters packed in Param6..Param8.
static inline void DrawIf14Filters(int* p, float width, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled()) return;
	im::Spacing();
	if (!im::TreeNode(LBL("Extended Melty / BOF collider filters"))) return;
	Hint("Needs a BOF executable; all zero keeps vanilla behaviour.");
	int guardMode = p[7] & 3;
	int sourceMask = (p[7] >> 3) & 7;
	auto store = [&]() { p[7] = (guardMode & 3) | ((sourceMask & 7) << 3); };
	float x0 = MaskRow("Collider stance (any selected):");
	if (MaskBox(LBL("Standing##If14Stance"), &p[5], 1, true, x0)) markModified();
	if (MaskBox(LBL("Crouching##If14Stance"), &p[5], 2, false, x0)) markModified();
	if (MaskBox(LBL("Airborne##If14Stance"), &p[5], 4, false, x0)) markModified();
	const char* const guardModes[] = { "0: Disabled", "1: Contains all selected", "2: Exactly selected - no extras" };
	if (Combo("Guard match", &guardMode, guardModes, IM_ARRAYSIZE(guardModes), width)) { store(); markModified(); }
	x0 = MaskRow("Attack is guardable as:");
	if (MaskBox(LBL("Stand##If14Guard"), &p[6], 1, true, x0)) markModified();
	if (MaskBox(LBL("Crouch##If14Guard"), &p[6], 2, false, x0)) markModified();
	if (MaskBox(LBL("Air##If14Guard"), &p[6], 4, false, x0)) markModified();
	int src = sourceMask;
	x0 = MaskRow("Collider source (any selected):");
	MaskBox(LBL("Main body##If14Source"), &src, 1, true, x0);
	MaskBox(LBL("Assist / partner##If14Source"), &src, 2, false, x0);
	MaskBox(LBL("Spawned effect actor##If14Source"), &src, 4, false, x0);
	if (src != sourceMask) { sourceMask = src; store(); markModified(); }
	if ((p[5] & ~7) || (p[6] & ~7) || (p[7] & ~0x3b)) Warn("Invalid extension value: the patched game rejects this condition.");
	else if ((guardMode == 0) != (p[6] == 0)) Caution("Guard mode and guard boxes must both be disabled or both enabled.");
	else if (p[8] != 0) Caution("Param9 is unused; clear legacy data there.");
	else Hint("Enabled filter groups combine as AND. Add another IF 14 row for OR.");
	im::TreePop();
}

// IF 154-157. Returns false (draw nothing) under the vanilla profile.
static inline bool DrawCondition(int type, int* p, float width, FrameData* frameData, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled() || type < 154 || type > 157) return false;
	switch (type) {
	case 154:
		EfSection(TXT("--- Custom variable compare + jump (BOF ABI v1) ---"));
		JumpField(&p[0], width, frameData, markModified);
		Int("Variable index", &p[1], width, markModified);
		if (p[1] < 0 || p[1] > 1023) Warn("Invalid index: use 0..1023.");
		Int("Signed compare value", &p[2], width, markModified);
		if (Combo("Comparison", &p[3], kCompare, IM_ARRAYSIZE(kCompare), width)) markModified();
		if (Combo("Bank", &p[4], kBanks, IM_ARRAYSIZE(kBanks), width)) markModified();
		if (p[4] < 0 || p[4] > 3) Warn("Invalid bank: use 0..3; the condition fails closed.");
		ClearReserved(p, 5, 9, markModified);
		break;
	case 155: {
		EfSection(TXT("--- Custom variable write on command (BOF ABI v1) ---"));
		Int("Move ID", &p[0], width, markModified);
		if (frameData) {
			if (Command* cmd = frameData->get_command(p[0])) {
				char note[160];
				snprintf(note, sizeof(note), "[%s]", cmd->input.c_str());
				EfNote(note);
			}
		}
		Int("Variable index", &p[1], width, markModified);
		if (p[1] < 0 || p[1] > 1023) Warn("Invalid index: use 0..1023.");
		Int("Signed value", &p[2], width, markModified);
		if (Combo("Operation", &p[3], kWriteOps, IM_ARRAYSIZE(kWriteOps), width)) markModified();
		if (Combo("Bank", &p[4], kBanks, IM_ARRAYSIZE(kBanks), width)) markModified();
		if (p[4] < 0 || p[4] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
		ClearReserved(p, 5, 9, markModified);
		break;
	}
	case 156: {
		EfSection(TXT("--- Custom variable write on hit (BOF ABI v1) ---"));
		Int("Signed value", &p[0], width, markModified);
		const char* const when[] = { "0: On hit", "1: On hit/block", "2: On hit/clash", "3: On hit/block/clash", "5: On block", "6: On clash", "7: On block/clash" };
		if (Combo("When", &p[1], when, IM_ARRAYSIZE(when), width)) markModified();
		if (i18n::Combo(EfRow(LBL("Opponent state")), &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList))) markModified();
		Int("Variable index", &p[3], width, markModified);
		if (p[3] < 0 || p[3] > 1023) Warn("Invalid index: use 0..1023.");
		if (Combo("Operation", &p[4], kWriteOps, IM_ARRAYSIZE(kWriteOps), width)) markModified();
		if (Combo("Bank", &p[5], kBanks, IM_ARRAYSIZE(kBanks), width)) markModified();
		if (p[5] < 0 || p[5] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
		ClearReserved(p, 6, 9, markModified);
		break;
	}
	case 157: {
		EfSection(TXT("--- Tagged Special Box collision (BOF extension) ---"));
		Hint("A BOF projectile-level mechanism, not 2v2 tag.");
		if (im::Button(LBL("Apply projectile-level preset (Var6)"))) {
			p[1] = 0; p[2] = 1018; p[3] = 6; p[4] = 6; p[5] = 5; p[6] = 4; p[7] = 0; p[8] = 0;
			markModified();
		}
		if (im::IsItemHovered()) Tooltip("Opposing-team Special Box 16, non-consuming; candidate Var6 >= self Var6; spawned effects only.\nLevels by convention: 10 / 20 / 30 (set with EF6 105).");
		JumpField(&p[0], width, frameData, markModified);
		const char* const targets[] = { "0: Enemies only", "1: Allies only", "2: Both" };
		if (Combo("Check target", &p[1], targets, IM_ARRAYSIZE(targets), width)) markModified();
		Int("Candidate box", &p[2], width, markModified);
		im::SameLine(); im::TextDisabled("(?)");
		if (im::IsItemHovered()) Tooltip("(IF 14 box codes, e.g. 1018 = Special Box 16, non-consuming)");
		Int("Self ExtraVar index", &p[3], width, markModified);
		Int("Candidate ExtraVar index", &p[4], width, markModified);
		if (Combo("Candidate compared with self", &p[5], kCompare, IM_ARRAYSIZE(kCompare), width)) markModified();
		const float x0 = MaskRow("Eligible candidate object types (any selected):");
		if (MaskBox(LBL("Main body##If157"), &p[6], 1, true, x0)) markModified();
		if (MaskBox(LBL("Assist / partner##If157"), &p[6], 2, false, x0)) markModified();
		if (MaskBox(LBL("Spawned effect actor##If157"), &p[6], 4, false, x0)) markModified();
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
	EfSection(TXT("--- Custom variable write (BOF ABI v1) ---"));
	Hint("Bank 0 keeps the original owner-relative behaviour.");
	Int("Variable index", &p[0], width, markModified);
	if (p[0] < 0 || p[0] > 1023) Warn("Invalid index: ABI v1 accepts 0..1023; nothing is written.");
	Int("Signed value", &p[1], width, markModified);
	if (p[1] < -32768 || p[1] > 32767) Colored(ImVec4(1.f, .7f, 0.f, 1.f), TXT("Outside int16; the stored result wraps."));
	if (Combo("Operation", &p[2], kWriteOps, IM_ARRAYSIZE(kWriteOps), width)) markModified();
	if (p[2] < 0 || p[2] > 1) Warn("Invalid operation: 0 Set or 1 Add only.");
	if (Combo("Bank", &p[3], kBanks, IM_ARRAYSIZE(kBanks), width)) markModified();
	if (p[3] < 0 || p[3] > 3) Warn("Invalid bank: use 0..3; nothing is written.");
	ClearReserved(p, 4, 12, markModified);
	return true;
}

// EF6 105 projectile-level presets (BOF convention: ExtraVar 6 on the spawned effect): a hint line, then the three
// preset buttons in the value column (wrapping there when the pane is narrow).
static inline void DrawVar6Presets(int* p, const std::function<void()>& markModified)
{
	if (!ExtendedProfileEnabled()) return;
	Caution("BOF projectile level (pairs with the IF 157 preset):");
	EfRow("##Var6LevelRow");
	const float x0 = im::GetCursorPosX();
	for (int level = 1; level <= 3; ++level) {
		char label[48];
		snprintf(label, sizeof(label), "Level %d (%d)##Var6Level", level, level * 10);
		if (level > 1) {
			im::SameLine();
			if (im::GetContentRegionAvail().x < im::CalcTextSize(label, nullptr, true).x + im::GetStyle().FramePadding.x * 2.f) { im::NewLine(); im::SetCursorPosX(x0); }
		}
		if (im::Button(label)) { p[0] = 6; p[1] = level * 10; p[2] = 10; markModified(); }
		if (im::IsItemHovered()) Tooltip("Sets variable 6 on the effect itself (mode 10 = self set).");
	}
}

} // namespace bof

#endif
