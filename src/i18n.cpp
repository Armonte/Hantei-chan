#include "i18n.h"
#include <unordered_map>
#include <string>
#include <windows.h>
#include <vector>
#include <cstring>
#include <cstdarg>
#include <imgui.h>

namespace i18n {
int language = 0;

namespace {
struct Row { const char *en, *ja; };
const Row kRows[] = {
#include "i18n_ja_menu.inc"
#include "i18n_ja_windows.inc"
#include "i18n_ja_tools.inc"
#include "i18n_ja_han2.inc"
#include "i18n_ja_ui2.inc"
};
std::unordered_map<std::string, const char *> &Table()
{
	static std::unordered_map<std::string, const char *> t = [] {
		std::unordered_map<std::string, const char *> m;
		for (const Row &r : kRows) m[r.en] = r.ja;
		return m;
	}();
	return t;
}
std::unordered_map<std::string, std::string> g_labelCache;
}

const char *Tr(const char *en)
{
	if (language == 0 || !en) return en;
	auto &t = Table();
	auto it = t.find(en);
	return it == t.end() ? en : it->second;
}

const char *Label(const char *en)
{
	if (language == 0 || !en) return en;
	std::string key = en;
	// a label that already carries an explicit id ("Text##id" / "Text###id") keeps it: translate only the visible part
	std::string visible = key, id;
	size_t p = key.find("##");
	if (p != std::string::npos) { visible = key.substr(0, p); id = key.substr(p); }
	auto it = Table().find(visible);
	if (it == Table().end()) return en;
	auto c = g_labelCache.find(key);
	if (c != g_labelCache.end()) return c->second.c_str();
	std::string out = std::string(it->second) + (id.empty() ? "###" + visible : id);
	return g_labelCache.emplace(key, out).first->second.c_str();
}

static std::string SettingsPath()
{
	char buf[512]; DWORD n = GetCurrentDirectoryA(512, buf);
	return std::string(buf, n) + "\\han2_settings.ini";
}
void Load() { language = (int)GetPrivateProfileIntA("han2", "Language", 0, SettingsPath().c_str()); }
void Save() { WritePrivateProfileStringA("han2", "Language", std::to_string(language).c_str(), SettingsPath().c_str()); }

std::string TrDetail(const std::string &s)
{
	if (language == 0 || s.empty()) return s;
	static const char *const kPrefixes[] = { "Could not open ", "cannot open ", "could not write ", "cannot read ", "cannot create ",
		"no original data kept for this character", "refusing to overwrite ", "short read of ", "name too long (max 59 bytes): " };
	auto one = [&](const std::string &line) -> std::string {
		auto &t = Table();
		auto it = t.find(line);
		if (it != t.end()) return it->second;
		for (const char *p : kPrefixes) {
			const size_t n = strlen(p);
			if (line.compare(0, n, p) == 0) { auto q = t.find(p); if (q != t.end()) return std::string(q->second) + line.substr(n); }
		}
		const size_t c = line.find(": ");
		if (c != std::string::npos) {
			auto q = t.find(line.substr(c + 2));
			if (q != t.end()) return line.substr(0, c) + ": " + q->second;
		}
		return line;
	};
	std::string out, line;
	for (size_t i = 0; i <= s.size(); ++i) {
		if (i == s.size() || s[i] == '\n') { out += one(line); if (i < s.size()) out += '\n'; line.clear(); }
		else line += s[i];
	}
	return out;
}
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
