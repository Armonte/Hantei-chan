#include "script_layers.h"
#include "fpanim_link.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>

namespace fs = std::filesystem;

namespace sqedit {

namespace {

fs::path U8(const std::string& s) { return fs::u8path(s); }
std::string ToU8(const fs::path& p) { return p.generic_u8string(); }

std::string Lower(std::string s)
{
	for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return s;
}
std::string Trim(const std::string& s)
{
	size_t a = 0, b = s.size();
	while (a < b && isspace(static_cast<unsigned char>(s[a]))) a++;
	while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) b--;
	return s.substr(a, b - a);
}
bool IsDir(const fs::path& p)
{
	std::error_code ec;
	return fs::is_directory(p, ec);
}
std::string NameLower(const fs::path& p) { return Lower(p.filename().u8string()); }

// [section] key=value, case-insensitive names (GetPrivateProfileString semantics for the files fpan-im reads).
std::string IniString(const fs::path& ini, const std::string& section, const std::string& key)
{
	std::ifstream f(ini, std::ios::binary);
	if (!f) return "";
	std::string line, cur;
	const std::string sec = Lower(section), k = Lower(key);
	while (std::getline(f, line)) {
		if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line.erase(0, 3);   // UTF-8 BOM
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line[0] == '[') { cur = Lower(Trim(line.substr(1, line.find(']') - 1))); continue; }
		const size_t eq = line.find('=');
		if (cur == sec && eq != std::string::npos && Lower(Trim(line.substr(0, eq))) == k) return Trim(line.substr(eq + 1));
	}
	return "";
}

std::vector<std::string> SplitBar(const std::string& s)
{
	std::vector<std::string> out;
	std::string cur;
	for (char c : s) {
		if (c == '|') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
		else cur.push_back(c);
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

// fpan-im core/vfs.cpp discoverPacks: every subfolder; ordered by `order`, unlisted after (by id); `off` disables.
struct Pack { std::string id; fs::path dir; bool enabled = true; };
std::vector<Pack> DiscoverPacks(const fs::path& packsDir, const std::string& order, const std::string& off)
{
	std::vector<Pack> packs;
	std::error_code ec;
	if (!IsDir(packsDir)) return packs;
	const auto ord = SplitBar(order), offl = SplitBar(off);
	for (auto it = fs::directory_iterator(packsDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
		std::error_code e2;
		if (!it->is_directory(e2)) continue;
		const std::string id = it->path().filename().u8string();
		if (id.empty() || id[0] == '.') continue;
		packs.push_back({id, it->path(), std::find(offl.begin(), offl.end(), id) == offl.end()});
	}
	std::sort(packs.begin(), packs.end(), [](const Pack& a, const Pack& b) { return a.id < b.id; });
	auto place = [&](const std::string& id) { auto i = std::find(ord.begin(), ord.end(), id); return static_cast<size_t>(i - ord.begin()); };
	std::stable_sort(packs.begin(), packs.end(), [&](const Pack& a, const Pack& b) { return place(a.id) < place(b.id); });
	return packs;
}

bool IsImName(const std::string& low) { return low == "uni2-im" || low == "mbtl-im"; }
std::string ImLabel(const std::string& low) { return low == "mbtl-im" ? "MBTL-IM" : "UNI2-IM"; }

bool LooksLikeGame(const fs::path& d)
{
	std::error_code ec;
	for (const char* s : {"script", "data", "d", "MBTL.exe", "uni2.exe", "UNI2-IM", "MBTL-IM", "fpan-im"})
		if (fs::exists(d / s, ec)) return true;
	return false;
}

int64_t MTime(const fs::path& p)
{
	std::error_code ec;
	const auto t = fs::last_write_time(p, ec);
	return ec ? 0 : static_cast<int64_t>(t.time_since_epoch().count());
}

}  // namespace

ScriptRootInfo ResolveScriptRoot(const std::string& chosenUtf8)
{
	ScriptRootInfo r;
	std::string s = chosenUtf8;
	for (char& c : s) if (c == '\\') c = '/';
	while (s.size() > 1 && s.back() == '/' && !(s.size() == 3 && s[1] == ':')) s.pop_back();
	fs::path p = U8(s).lexically_normal();
	if (!p.has_filename() && p.has_parent_path() && p != p.root_path()) p = p.parent_path();
	// <x>/data or <x>/script -> <x>
	const std::string last = NameLower(p);
	if ((last == "data" || last == "script") && p.has_parent_path()) p = p.parent_path();
	const std::string n0 = NameLower(p), n1 = NameLower(p.parent_path()), n2 = NameLower(p.parent_path().parent_path());
	fs::path game = p;
	if (n1 == "packs" && IsImName(n2)) {                 // <game>/UNI2-IM/Packs/<id>
		game = p.parent_path().parent_path().parent_path();
		r.editRoot = ToU8(p);
		r.editTag = "pack " + p.filename().u8string();
	} else if (n0 == "mods" && IsImName(n1)) {           // <game>/UNI2-IM/Mods
		game = p.parent_path().parent_path();
		r.editRoot = ToU8(p);
		r.editTag = ImLabel(n1) + " Mods";
	} else if (n1 == "packs" && n2 == "fpan-im") {        // <game>/fpan-im/packs/<id>
		game = p.parent_path().parent_path().parent_path();
		r.editRoot = ToU8(p);
		r.editTag = "fpan-im pack " + p.filename().u8string();
	} else if (n0 == "mods" && n1 == "fpan-im") {         // <game>/fpan-im/mods
		game = p.parent_path().parent_path();
		r.editRoot = ToU8(p);
		r.editTag = "fpan-im mods";
	} else if (IsImName(n0) || n0 == "fpan-im") {         // <game>/UNI2-IM itself
		game = p.parent_path();
	} else if ((n0 == "packs") && (IsImName(n1) || n1 == "fpan-im")) {
		game = p.parent_path().parent_path();
	}
	r.gameDir = ToU8(game);
	if (r.editRoot.empty() && !LooksLikeGame(game)) r.error = "no script/ or data/ folder (or game archive, UNI2-IM, MBTL-IM, fpan-im) in " + chosenUtf8;
	return r;
}

std::vector<ScriptLayer> ScriptLayersFor(const std::string& gameDirUtf8, const std::string& game)
{
	std::vector<ScriptLayer> L;
	const fs::path g = U8(gameDirUtf8);
	// fpan-im's own folders: 1000, then 999, 998 ... (core/main.cpp setupVfs)
	const fs::path fp = g / "fpan-im";
	if (IsDir(fp / "mods")) L.push_back({ScriptLayer::FpanimMods, "fpan-im mods", ToU8(fp / "mods"), "", 1000, true, true});
	int prio = 999;
	for (const Pack& p : DiscoverPacks(fp / "packs", IniString(fp / "fpan-im.ini", "packs", "order"), IniString(fp / "fpan-im.ini", "packs", "off")))
		L.push_back({ScriptLayer::FpanimPack, "fpan-im pack " + p.id + (p.enabled ? "" : " (off)"), ToU8(p.dir), p.id, p.enabled ? prio-- : -1000, p.enabled, p.enabled});
	// IM compat layers: Mods at 100, Packs at 99, 98 ... (imCompatLayers, basePriority 0)
	std::vector<std::pair<std::string, std::string>> ims;   // folder, ini
	if (game != "mbtl") ims.push_back({"UNI2-IM", "UNI2_IM.ini"});
	if (game != "uni2") ims.push_back({"MBTL-IM", "MBTL_IM.ini"});
	for (const auto& [im, iniName] : ims) {
		const fs::path root = g / im;
		if (!IsDir(root)) continue;
		if (IsDir(root / "Mods")) L.push_back({ScriptLayer::ImMods, im + " Mods", ToU8(root / "Mods"), "", 100, true, true});
		int pp = 99;
		const fs::path ini = root / iniName;
		for (const Pack& p : DiscoverPacks(root / "Packs", IniString(ini, "Mods", "Order"), IniString(ini, "Mods", "Off")))
			L.push_back({ScriptLayer::ImPack, "pack " + p.id + (p.enabled ? "" : " (off)"), ToU8(p.dir), p.id, p.enabled ? pp-- : -1000, p.enabled, p.enabled});
	}
	L.push_back({ScriptLayer::Loose, "loose", ToU8(g), "", -1, true, false});
	std::stable_sort(L.begin(), L.end(), [](const ScriptLayer& a, const ScriptLayer& b) { return a.priority > b.priority; });
	return L;
}

bool IsSquirrelScriptRel(const std::string& relIn, const std::string* head)
{
	std::string rel = relIn;
	for (char& c : rel) if (c == '\\') c = '/';
	if (rel.rfind("./", 0) == 0) rel.erase(0, 2);
	const std::string low = Lower(rel);
	if (low.size() < 4 || low.compare(low.size() - 4, 4, ".txt") != 0) return false;
	if (low.find("/.hantei-backups/") != std::string::npos) return false;
	if (low.rfind("script/", 0) == 0) return true;
	if (low.rfind("data/", 0) != 0) return false;
	if (FpanimIsSharedFile(rel)) return true;
	const std::string f = rel.substr(rel.find_last_of('/') + 1);
	// Same classification as sqtool (exe loader path formats, SCOPE.md §1.4).
	static const std::regex re(R"(^(chr|npc)\d+_(mv|sc|cmd|com|com_ranking)(_\d+)?\.txt$|^(chr|npc)\d+_se_list\.txt$)"
	                           R"(|^[A-Za-z]+(_s)?_\d+_(mv|sc|cmd|com)\.txt$)",
		std::regex::icase);
	if (std::regex_match(f, re)) return true;
	if (!head) return false;
	// A new file (a mod's own script under data/): Squirrel when it has a slot, a function or a table statement.
	static const std::regex sq(R"((<-|\bfunction\s*[A-Za-z_(]|\blocal\s+[A-Za-z_]|__dofile__))");
	return std::regex_search(*head, sq);
}

std::vector<ScriptFile> ListScriptFiles(std::vector<ScriptLayer>& layers, const std::vector<std::string>& archiveRels)
{
	std::map<std::string, ScriptFile> byKey;
	auto add = [&](const std::string& rel, int li, const std::string& path, int64_t mt) {
		ScriptFile& f = byKey[Lower(rel)];
		if (f.rel.empty()) f.rel = rel;
		f.copies.push_back({li, path, mt});
	};
	for (int li = 0; li < static_cast<int>(layers.size()); li++) {
		const fs::path root = U8(layers[li].root);
		for (const char* sub : {"script", "data"}) {
			std::error_code ec;
			fs::path dir;
			// "data" / "script" in any case (packs are often made on case-insensitive systems by hand)
			for (auto it = fs::directory_iterator(root, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
				std::error_code e2;
				if (it->is_directory(e2) && NameLower(it->path()) == sub) { dir = it->path(); break; }
			}
			if (dir.empty()) continue;
			for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				std::error_code e2;
				if (!it->is_regular_file(e2) || Lower(it->path().extension().u8string()) != ".txt") continue;
				std::string rel = it->path().lexically_relative(root).generic_u8string();
				rel = std::string(sub) + rel.substr(rel.find('/'));   // canonical lower-case top folder
				std::string head;
				if (rel.rfind("data/", 0) == 0) {
					std::ifstream f(it->path(), std::ios::binary);
					head.resize(8192);
					f.read(&head[0], static_cast<std::streamsize>(head.size()));
					head.resize(static_cast<size_t>(f.gcount()));
				}
				if (!IsSquirrelScriptRel(rel, &head)) continue;
				add(rel, li, ToU8(it->path()), MTime(it->path()));
			}
		}
	}
	std::vector<std::string> arch;
	for (std::string r : archiveRels) {
		for (char& c : r) if (c == '\\') c = '/';
		while (r.rfind("./", 0) == 0) r.erase(0, 2);
		const std::string low = Lower(r);
		if (low.rfind("data/", 0) == 0) r = "data" + r.substr(4);
		else if (low.rfind("script/", 0) == 0) r = "script" + r.substr(6);
		if (IsSquirrelScriptRel(r)) arch.push_back(r);
	}
	if (!arch.empty()) {
		layers.push_back({ScriptLayer::Archive, "archive", "", "", -2, true, false});
		const int li = static_cast<int>(layers.size()) - 1;
		for (const std::string& r : arch) add(r, li, "", 0);
	}
	std::vector<ScriptFile> out;
	out.reserve(byKey.size());
	for (auto& [k, f] : byKey) {
		std::stable_sort(f.copies.begin(), f.copies.end(), [&](const ScriptCopy& a, const ScriptCopy& b) { return layers[a.layer].priority > layers[b.layer].priority; });
		for (int i = 0; i < static_cast<int>(f.copies.size()); i++) {
			const ScriptLayer& l = layers[f.copies[i].layer];
			if (!l.enabled) continue;
			if (f.winner < 0) { f.winner = i; continue; }
			// fpan-im\mods vs <IM>\Mods: the newer file wins.
			const ScriptLayer& w = layers[f.copies[f.winner].layer];
			const bool looseL = l.kind == ScriptLayer::FpanimMods || l.kind == ScriptLayer::ImMods;
			const bool looseW = w.kind == ScriptLayer::FpanimMods || w.kind == ScriptLayer::ImMods;
			if (looseL && looseW && f.copies[i].mtime > f.copies[f.winner].mtime) f.winner = i;
		}
		if (f.winner >= 0) {   // spell the rel as the winning copy does
			const ScriptCopy& w = f.copies[f.winner];
			if (!w.path.empty()) {
				const std::string root = layers[w.layer].root;
				std::string rel = U8(w.path).lexically_relative(U8(root)).generic_u8string();
				const size_t sl = rel.find('/');
				if (sl != std::string::npos) f.rel = Lower(rel.substr(0, sl)) + rel.substr(sl);
			}
		}
		out.push_back(std::move(f));
	}
	std::sort(out.begin(), out.end(), [](const ScriptFile& a, const ScriptFile& b) { return a.rel < b.rel; });
	return out;
}

std::string ScriptCopyLabel(const ScriptFile& f, const ScriptCopy& c, const std::vector<ScriptLayer>& layers)
{
	return f.rel + "  [" + layers[c.layer].tag + "]";
}

}  // namespace sqedit
