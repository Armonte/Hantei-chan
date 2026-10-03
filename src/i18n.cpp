#include "i18n.h"
#include <unordered_map>
#include <string>
#include <windows.h>
#include <vector>
#include <cstring>
#include <cstdarg>

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
		"no original data kept for this character", "refusing to overwrite ", "short read of ", "name too long (max 59 bytes): ", "Saved. Previous version backed up to ", "Cannot open ", "Cannot read ", "Cannot create backup folder ", "Cannot create backup ", "Cannot write backup ", "No free backup name in ", "Could not replace ", "Notes file is not valid JSON: ", "Notes file could not be read: " };
	auto one = [&](const std::string &line) -> std::string {
		auto &t = Table();
		auto it = t.find(line);
		if (it != t.end()) return it->second;
		for (const char *p : kPrefixes) {
			const size_t n = strlen(p);
			if (line.compare(0, n, p) == 0) { auto q = t.find(p); if (q != t.end()) return std::string(q->second) + line.substr(n); }
		}
		static const char *const kInfixes[] = { " is not in " };   // "<name>" + infix + "<where>"
		for (const char *inf : kInfixes) {
			const size_t at = line.find(inf);
			if (at == std::string::npos) continue;
			auto q = t.find(inf);
			if (q != t.end()) return line.substr(0, at) + q->second + line.substr(at + strlen(inf));
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
}
