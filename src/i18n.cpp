#include "i18n.h"
#include <unordered_map>
#include <string>
#include <windows.h>

namespace i18n {
int language = 0;

namespace {
struct Row { const char *en, *ja; };
const Row kRows[] = {
#include "i18n_ja_menu.inc"
#include "i18n_ja_windows.inc"
#include "i18n_ja_tools.inc"
#include "i18n_ja_han2.inc"
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
}
