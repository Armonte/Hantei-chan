#include "steam.h"

#include "paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace portable {

namespace {

std::string Lower(std::string s)
{
	for (char& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

// game_table.cpp's QuotedStrings: every "..." on the line, with backslash
// escapes consumed. libraryfolders.vdf and the appmanifests both use it.
std::vector<std::string> QuotedStrings(const std::string& line)
{
	std::vector<std::string> out;
	size_t i = 0;
	while ((i = line.find('"', i)) != std::string::npos) {
		std::string cur;
		size_t j = i + 1;
		for (; j < line.size() && line[j] != '"'; ++j) {
			if (line[j] == '\\' && j + 1 < line.size()) ++j;
			cur += line[j];
		}
		out.push_back(cur);
		i = j + 1;
	}
	return out;
}

void AddUnique(std::vector<std::string>& v, const std::string& p)
{
	if (p.empty()) return;
	const std::string n = NormalizeSeparators(p);
	if (std::find(v.begin(), v.end(), n) == v.end()) v.push_back(n);
}

// A root is a Steam install when it has steamapps/ next to steam.exe; the
// library list only needs the steamapps folder to exist.
std::vector<std::string> Roots()
{
	std::vector<std::string> roots;
	if (const char* override_ = std::getenv("HANTEI_STEAM_ROOT")) AddUnique(roots, override_);
#ifdef _WIN32
	for (const char* e : { "ProgramFiles(x86)", "ProgramFiles" }) {
		const char* v = std::getenv(e);
		if (v) AddUnique(roots, std::string(v) + "\\Steam");
	}
	AddUnique(roots, "C:\\Program Files (x86)\\Steam");
#else
	if (const char* home = std::getenv("HOME")) {
		AddUnique(roots, std::string(home) + "/.steam/steam");
		AddUnique(roots, std::string(home) + "/.steam/root");
		AddUnique(roots, std::string(home) + "/.local/share/Steam");
	}
#endif
	return roots;
}

}  // namespace

std::vector<std::string> SteamLibraries()
{
	std::vector<std::string> libs;
	std::error_code ec;
	for (const std::string& r : Roots()) {
		if (!fs::is_directory(fs::u8path(r), ec)) continue;
		AddUnique(libs, r);
		std::ifstream f(fs::u8path(r) / "steamapps" / "libraryfolders.vdf");
		std::string line;
		while (std::getline(f, line)) {
			const std::vector<std::string> q = QuotedStrings(line);
			if (q.size() == 2 && q[0] == "path") AddUnique(libs, q[1]);
		}
	}
	return libs;
}

std::vector<SteamApp> SteamApps()
{
	std::vector<SteamApp> apps;
	std::error_code ec;
	for (const std::string& lib : SteamLibraries()) {
		const fs::path sa = fs::u8path(lib) / "steamapps";
		for (const fs::directory_entry& e : fs::directory_iterator(sa, fs::directory_options::skip_permission_denied, ec)) {
			const std::string fn = e.path().filename().string();
			if (fn.rfind("appmanifest_", 0) != 0) continue;
			std::ifstream f(e.path());
			std::string line;
			SteamApp a;
			a.library = lib;
			while (std::getline(f, line)) {
				const std::vector<std::string> q = QuotedStrings(line);
				if (q.size() != 2) continue;
				if (q[0] == "name") a.name = q[1];
				else if (q[0] == "installdir") a.dir = (sa / "common" / fs::u8path(q[1])).u8string();
			}
			if (!a.dir.empty()) apps.push_back(a);
		}
	}
	return apps;
}

std::string SteamAppDir(const std::string& nameNeedle)
{
	const std::string want = Lower(nameNeedle);
	for (const SteamApp& a : SteamApps())
		if (Lower(a.name).find(want) != std::string::npos) return a.dir;
	return std::string();
}

}  // namespace portable
