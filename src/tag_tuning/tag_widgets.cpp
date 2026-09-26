// Shared TAG lever widgets — see tag_widgets.h.
#include "tag_widgets.h"
#include "tag_assist.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace tagui {

using namespace tagtune;

namespace {
const ImVec4 kWarn(1.0f, 0.62f, 0.25f, 1.0f);
} // namespace

std::string RangeText(const Lever& l)
{
	if (l.kind == LeverKind::Bool) return "0|1";
	if (l.kind == LeverKind::Enum) {
		std::string r;
		for (int k = l.lo; k <= l.hi; ++k) r += std::string(k == l.lo ? "" : "|") + l.names[k];
		return r;
	}
	if (l.kind == LeverKind::Motion) return "an input, e.g. 236C";
	return std::to_string(l.lo) + ".." + std::to_string(l.hi);
}

// Extra notes from TAG_TUNING_GUIDE.md for the levers whose one-line help is not enough.
const char* GuideNote(const char* key)
{
	struct N { const char* k; const char* n; };
	static const N notes[] = {
		{ "cancelWindowTicks", "-1 = the tag-in pattern's own flags (stock: movable from tick 12; Miyako never). N >= 0 opens earlier AND closes later than the data. Capped by the exit (~51-68 ticks, ~20 with an air tag-out). With N <= ~6 a buffered 22+button special can fire by itself: cancelSpecials=0 stops that." },
		{ "cancelSpecials", "The 22D is still in the command buffer: with an early window a character with a 22+button special fires it on the first open tick." },
		{ "airTagOut", "The outgoing skips its grounded wind-up and starts its tag-out at the lift-off frame, found from the loaded pattern." },
		{ "airTagIn", "Master switch for the airborne entries: entryStyle drop and arc need it." },
		{ "entryStyle", "edge = the TagIn pattern's dash-in (its speed is the pattern's X set: edit it in Hantei-chan). drop = the vertical jump p36 from its falling frame. arc = the forward jump p35 from just after take-off." },
		{ "entryAnchor", "point = the incoming appears behind the outgoing point, on the team's side." },
		{ "parkX", "Boot-time byte patch: restart the game to change it." },
		{ "koRule", "allDown = forced tag-in: the partner comes in automatically when the point is KO'd. Not part of any built-in style." },
		{ "tagIn", "0 = the built-in TeamChangeData table. A number not in the loaded HA6 is refused by the game (logged)." },
		{ "tagOut", "0 = the built-in TeamChangeData table. A number not in the loaded HA6 is refused by the game (logged)." },
		{ "assistEntry", "behind = runs in from assistOffsetX behind the point; edge = from the screen edge behind you; drop = falls from entryY; arc = jump-in from the edge. A slot's own entry wins." },
		{ "assistDamagePct", "The assist can be hit once; that hit deals damage x this %, then it is intangible and leaves (Marvel feel ~150)." },
		{ "regenPerTick", "A KO'd member never regenerates." },
	};
	for (const N& n : notes) if (!std::strcmp(n.k, key)) return n.n;
	return nullptr;
}

void LeverTooltip(const Lever& l, int32_t styleValue, const char* where)
{
	if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
	ImGui::Text("%s  [%s]", l.key, l.group);
	ImGui::TextUnformatted(l.help);
	if (const char* n = GuideNote(l.key)) { ImGui::Separator(); ImGui::TextUnformatted(n); }
	ImGui::Separator();
	ImGui::Text("default %s, range %s", FormatLeverValue(l, l.def).c_str(), RangeText(l).c_str());
	ImGui::Text("%s: %s", where, FormatLeverValue(l, styleValue).c_str());
	if (l.scope == LeverScope::SimBoot) ImGui::TextColored(kWarn, "boot-time: restart the game to change it");
	if (l.scope == LeverScope::Harness) ImGui::TextDisabled("stress harness only, never a netplay behaviour");
	if (l.perChar) ImGui::TextDisabled("can also be set per character");
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

// One value widget. Returns true when v changed (always clamped into range).
bool ValueWidget(const Lever& l, int32_t& v, float width)
{
	bool changed = false;
	ImGui::SetNextItemWidth(width);
	switch (l.kind) {
	case LeverKind::Bool: {
		bool b = v != 0;
		if (ImGui::Checkbox("##v", &b)) { v = b; changed = true; }
		break;
	}
	case LeverKind::Enum: {
		if (ImGui::BeginCombo("##v", FormatLeverValue(l, v).c_str())) {
			for (int k = l.lo; k <= l.hi; ++k)
				if (ImGui::Selectable(l.names[k], k == v)) { v = k; changed = true; }
			ImGui::EndCombo();
		}
		break;
	}
	case LeverKind::Motion: {
		char buf[16];
		std::snprintf(buf, sizeof buf, "%s", v ? UnpackMotion(v).c_str() : "");
		if (ImGui::InputText("##v", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
			int32_t p = 0;
			if (!buf[0]) { v = 0; changed = true; }
			else if (ValidateMotion(buf).empty() && PackMotion(buf, p)) { v = p; changed = true; }
		}
		break;
	}
	case LeverKind::Int: {
		const long long span = (long long)l.hi - l.lo;
		if (span <= 6001) changed = ImGui::SliderInt("##v", &v, l.lo, l.hi, "%d", ImGuiSliderFlags_AlwaysClamp);
		else changed = ImGui::InputInt("##v", &v, 100, 1000);
		break;
	}
	}
	if (v < l.lo) { v = l.lo; changed = true; }
	if (v > l.hi) { v = l.hi; changed = true; }
	return changed;
}

bool IsSlotAction(const Lever& l) { return !std::strncmp(l.key, "assist.", 7) && l.scope == LeverScope::CharOnly; }
bool IsSlotEntry(const Lever& l) { return !std::strncmp(l.key, "assist.", 7) && l.scope != LeverScope::CharOnly; }


} // namespace tagui
