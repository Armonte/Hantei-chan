#include "i18n.h"
#include <cstdarg>
#include <string>
#include <vector>
#include <imgui.h>

// Layout helpers for translated (longer) text; split from i18n.cpp so non-GUI test tools can link the string tables alone.
namespace i18n {
void SameLineFit(float nextWidth, float spacing)
{
	const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	if (spacing < 0.f) spacing = ImGui::GetStyle().ItemSpacing.x;
	if (ImGui::GetItemRectMax().x + spacing + nextWidth <= right) ImGui::SameLine(0.f, spacing);
}
void TextDisabledWrapped(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrappedV(fmt, ap);
	ImGui::PopStyleColor();
	va_end(ap);
}
float RightPairX(const char *a, const char *b)
{
	const ImGuiStyle &st = ImGui::GetStyle();
	const float w = ButtonWidth(a) + st.ItemSpacing.x + ButtonWidth(b) + st.WindowPadding.x + st.ScrollbarSize;
	const float x = ImGui::GetWindowWidth() - w;
	return x < 0.f ? 0.f : x;
}
float ButtonWidth(const char *label)
{
	return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.f;
}
float FieldWidth(float itemWidth, const char *label)
{
	return itemWidth + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

bool Combo(const char *label, int *current, const char *const *items, int count, int heightInItems)
{
	if (language == 0) return ImGui::Combo(label, current, items, count, heightInItems);
	std::vector<const char *> tr((size_t)(count > 0 ? count : 0));
	for (int i = 0; i < count; i++) tr[(size_t)i] = Tr(items[i]);
	return ImGui::Combo(label, current, tr.data(), count, heightInItems);
}

bool Combo(const char *label, int *current, const char *itemsSeparatedByZeros, int heightInItems)
{
	if (language == 0) return ImGui::Combo(label, current, itemsSeparatedByZeros, heightInItems);
	std::string out;
	for (const char *p = itemsSeparatedByZeros; *p; ) {
		out += Tr(p);
		out.push_back('\0');
		p += strlen(p) + 1;
	}
	return ImGui::Combo(label, current, out.c_str(), heightInItems);   // c_str() keeps the final \0 pair
}
}
