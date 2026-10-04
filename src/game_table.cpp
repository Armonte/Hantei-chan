#include "game_table.h"
#include "archive_browser_state.h"

#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

namespace fs = std::filesystem;

namespace abrowser {

const char *FamilyName(Family f, bool ja)
{
	switch (f) {
	case Family::Hantei6Modern: return ja ? "Hantei6 (UNI / MBTL / DFCI)" : "Hantei6 modern (UNI / UNI2 / DFCI / MBTL)";
	case Family::Hantei6: return ja ? "Hantei6 (MBAACC)" : "Hantei6 (MBAACC)";
	case Family::Hantei4: return ja ? "Hantei4 (MB / MBAC / ReAct)" : "Hantei4 (Melty Blood line)";
	case Family::Han2: return ja ? "HAN2 (GOF / RBO)" : "HAN2 (Glove on Fight / Ragnarok Battle Offline)";
	case Family::Gof1Pb: return ja ? "GOF1 / PB 系" : "GOF1 / Party Breakers family";
	case Family::QoH: return ja ? "Queen of Heart / Rosa / dMp" : "Queen of Heart, Lilian, Rosa, dMp";
	default: return "";
	}
}

const std::vector<GameDef> &Games()
{
	static const std::vector<GameDef> g = {
		// ---- Hantei6 modern (HA6 with AFGX layers + .cg + .pat + .pal) ----
		{ Game::MBTL, "MBTL", "Melty Blood: Type Lumina", "メルティブラッド タイプルミナ", Family::Hantei6Modern,
		  "txt project stack: _temp.ha6 + chrNNN.ha6 + BaseData.HA6 + .cg + .pal + .pat (loadFromTxt, 3 AFGX layers)",
		  "data000..019.bin, no table inside: the file table is read from MBTL.exe; per-entry XOR cipher; read-only here",
		  { "TYPE LUMINA" }, { "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA", "D:\\SteamLibrary\\steamapps\\common\\MELTY BLOOD TYPE LUMINA" },
		  Support::Characters, "Archives are never written: a character opens as a working copy (Save writes the copy; copy it into the game's __Mods / fu folder). Needs MBTL.exe next to data*.bin." },
		{ Game::UNI2, "UNI2", "Under Night In-Birth II Sys:Celes", "UNDER NIGHT IN-BIRTH II Sys:Celes", Family::Hantei6Modern,
		  "txt project stack (loadFromTxt, 5 AFGX layers)",
		  "<install>\\d\\ : obfuscated index files + data files, entries stored raw; read-only here",
		  { "UNDER NIGHT IN-BIRTH II" }, { "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes", "D:\\SteamLibrary\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes" },
		  Support::Characters, "Working copy semantics as MBTL. The nuuni mod folder (mod\\) is not mounted over the archive." },
		{ Game::DFCI, "DFCI", "Dengeki Bunko Fighting Climax Ignition", "電撃文庫 FIGHTING CLIMAX IGNITION", Family::Hantei6Modern,
		  "txt project stack (data\\<Name>_0\\<Name>_0.txt), loadFromTxt",
		  "loose files (data\\, grpdat\\, BattleRes\\): no archive",
		  {}, { "C:\\games\\unib", "C:\\games\\dfci", "C:\\games\\dfci_run" },
		  Support::CharactersLoose, "Arcade / TeknoParrot build (RingGame.exe). Characters edit in place (loose files)." },
		{ Game::UNIST, "UNI[st]", "Under Night In-Birth Exe:Late[st]", "UNDER NIGHT IN-BIRTH Exe:Late[st]", Family::Hantei6Modern,
		  "txt project stack, loadFromTxt", "loose data\\ folder (same layout as DFCI); recognised by UNIST*.exe",
		  { "Exe:Late[st]" }, { "C:\\games\\unist", "C:\\games\\uni_st" },
		  Support::CharactersLoose, "Not installed on the development machine: detection by exe name + data\\BaseData.HA6 is unverified against a real install." },
		{ Game::UNI, "UNI", "Under Night In-Birth Exe:Late[cl-r]", "UNDER NIGHT IN-BIRTH Exe:Late[cl-r]", Family::Hantei6Modern,
		  "txt project stack, loadFromTxt", "loose data\\ folder or the game's own archive (loose layout only here)",
		  { "Exe:Late[cl-r]", "UNDER NIGHT IN-BIRTH Exe" }, { "C:\\games\\uni", "C:\\games\\uniclr" },
		  Support::CharactersLoose, "Not installed on the development machine; a packed Steam build would need its own archive reader (not written, no sample)." },
		// ---- Hantei6 (MBAACC) ----
		{ Game::MBAACC, "MBAACC", "Melty Blood Actress Again Current Code", "メルティブラッド アクトレスアゲイン カレントコード", Family::Hantei6,
		  "HA6 + .cg + .pal via the character .txt stack (loadFromTxt)", "FilePacHeaderA .p (0000.p..0008.p) and/or loose data\\*.HA6",
		  { "Actress Again Current Code" }, { "C:\\games\\mbaacc", "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD Actress Again Current Code" },
		  Support::Characters, "" },
		// ---- Hantei4 (Melty Blood line) ----
		{ Game::MBAC, "MBAC", "Melty Blood Act Cadenza (PC)", "メルティブラッド Act Cadenza", Family::Hantei4,
		  "Hantei4 .DAT (framedata_ha4, embedded parts + CG)", "PKFileInfo .p (00.p..10.p)",
		  {}, { "C:\\games\\MB\\AC", "C:\\games\\MB\\AC\\install\\MBACPC" }, Support::Characters, "" },
		{ Game::REACT, "ReAct", "Melty Blood ReACT", "メルティブラッド re・ACT", Family::Hantei4,
		  "Hantei4 .DAT (framedata_ha4)", "PAC v0 / v1 .p (00.p..10.p, key E3DF59AC)",
		  {}, { "C:\\games\\MB\\R" }, Support::Characters, "" },
		{ Game::MB, "MB", "Melty Blood (2002)", "メルティブラッド (2002)", Family::Gof1Pb,
		  "GOF1-family .DAT + embedded MB strip bank (framedata_gof1)", "PAC v0 .p (data00.p..data03.p)",
		  {}, { "C:\\games\\MB\\MeltyBlood" }, Support::Characters, "" },
		// ---- HAN2 ----
		{ Game::GOF2, "GOF2", "Glove on Fight 2", "Glove on Fight 2", Family::Han2,
		  "HAN2RBO .DT2 + .DAT + .PAT + .CHP (loadHan2)", "PAC v1 data00..data05.dat (+ cg.pac), later archives override",
		  {}, { "C:\\games\\gof2_run", "C:\\games\\gof" }, Support::Characters, "" },
		{ Game::GOF1, "GOF1", "Glove on Fight", "Glove on Fight", Family::Gof1Pb,
		  "GOF1 character .DAT in gof_0N.p (loadGof1)", "PB / GOF1 archive gof_00.p..gof_03.p (key FA261EFB)",
		  {}, { "C:\\games\\gof1", "C:\\games\\gof1_run", "C:\\games\\gof1_ex" }, Support::Characters, "Not installed on the development machine (no gof_0N.p found)." },
		{ Game::RBO, "RBO", "Ragnarok Battle Offline", "Ragnarok Battle Offline", Family::Han2,
		  "HAN2RBO .DT2 + .DAT (loadHan2)", "PAC v1 *.PAC (data01, data02, update01, ex1..ex3disc, cg, bg, bgm, se)",
		  {}, { "C:\\games\\rbo", "C:\\games\\rbo_run", "C:\\dev\\frenchbread\\rbo" }, Support::Characters, "" },
		// ---- GOF1 / PB family and the rest ----
		{ Game::PB2K1, "PB2K1", "Party Breakers (PB2K1)", "Party Breakers", Family::Gof1Pb,
		  "PB2K1 character .DAT (three-section cipher, framedata_pb2k1)", "PB archive 00.dat, 01.dat ... (key FA261EFB)",
		  {}, { "C:\\games\\pb" }, Support::Characters, "" },
		{ Game::QOH99, "QoH99", "Queen of Heart '99", "Queen of Heart '99", Family::QoH,
		  "name-keyed .chr character (framedata_qoh)", "loose <name>\\<name>.chr + .Img + .Fob folders",
		  {}, { "C:\\games\\qoh", "C:\\games\\qoh_dec", "C:\\games\\qoh_dec_b" }, Support::CharactersLoose, "" },
		{ Game::QOH98, "QoH98", "Queen of Heart '98", "Queen of Heart '98", Family::QoH,
		  "'98 character .dat (framedata_qoh)", "loose per-character folders",
		  {}, { "C:\\games\\qoh98", "C:\\games\\qoh98\\Queen of Heart '98 [Himeya Soft][1998]" }, Support::CharactersLoose, "" },
		{ Game::LILIAN, "Lilian", "Lilian Fourhand", "Lilian Fourhand", Family::QoH,
		  "(no character editor yet: Lilian characters are not parsed)", "PAC v0 .p (00b.p, 00bg.p, 00dm.p ...) + .EX3 images",
		  {}, { "C:\\dev\\frenchbread\\yamayuri_rendan\\files\\data" }, Support::FilesOnly, "Archives mount and list; .EX3 images do not decode yet (docs/formats/lilian.md). No character editor." },
		{ Game::ROSA, "Rosa", "Rosa Chinensis Four hand", "Rosa Chinensis Four hand", Family::QoH,
		  "(no character editor: IMG v4 images and FOB scripts only)", "PB archive pac.pac; .IMG v4 / v1, .FOB",
		  {}, { "C:\\dev\\frenchbread\\benibara_rendan\\files\\Rosa Chinensis Four hand" }, Support::FilesOnly, "Images preview inline; FOB scripts open in the viewer; no character editor (docs/formats/rosa.md)." },
		{ Game::DMP, "dMp", "Drill Milky Punch (dMp)", "ドリルミルキーパンチ", Family::QoH,
		  "(no character editor: FOB viewer, 16-bit IMG)", "PB archive GAMEDATA.PAC / SOUNDDATA.PAC",
		  {}, { "C:\\dev\\frenchbread\\dmp_1020", "C:\\dev\\frenchbread\\aquat1c_dmp\\Drill Milky Punch" }, Support::FilesOnly, "Images, FOB scripts and replays preview; no character editor (docs/formats/dmp.md)." },
	};
	return g;
}
const GameDef *FindGame(Game id) { for (auto &d : Games()) if (d.id == id) return &d; return nullptr; }

// ---- remembered folders -------------------------------------------------------------------------------------------------------------------------------
static std::string Ini() { char c[512]; DWORD n = GetCurrentDirectoryA(512, c); return std::string(c, n) + "\\han2_settings.ini"; }
std::string RememberedDir(Game g)
{
	char key[48]; snprintf(key, sizeof key, "Dir_%d", (int)g);
	char b[1024]{}; GetPrivateProfileStringA("browser", key, "", b, sizeof b, Ini().c_str());
	return b;
}
void RememberDir(Game g, const std::string &dir)
{
	char key[48]; snprintf(key, sizeof key, "Dir_%d", (int)g);
	WritePrivateProfileStringA("browser", key, dir.c_str(), Ini().c_str());
}

// ---- install scan ----------------------------------------------------------------------------------------------------------------------------------
namespace {
std::mutex g_mx;
std::map<Game, Install> g_found;
bool g_done = false, g_started = false;

std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

std::vector<std::string> QuotedStrings(const std::string &line)
{
	std::vector<std::string> o; size_t i = 0;
	while ((i = line.find('"', i)) != std::string::npos) {
		std::string cur; size_t j = i + 1;
		for (; j < line.size() && line[j] != '"'; j++) { if (line[j] == '\\' && j + 1 < line.size()) j++; cur += line[j]; }
		o.push_back(cur); i = j + 1;
	}
	return o;
}

struct SteamApp { std::string name, dir; };
std::vector<SteamApp> SteamApps()
{
	std::vector<std::string> libs;
	std::vector<std::string> roots;
	for (const char *e : { "ProgramFiles(x86)", "ProgramFiles" }) { const char *v = getenv(e); if (v) roots.push_back(std::string(v) + "\\Steam"); }
	roots.push_back("C:\\Program Files (x86)\\Steam");
	std::error_code ec;
	for (auto &r : roots) {
		if (!fs::is_directory(fs::u8path(r), ec)) continue;
		if (std::find(libs.begin(), libs.end(), r) == libs.end()) libs.push_back(r);
		std::ifstream f(fs::u8path(r) / "steamapps" / "libraryfolders.vdf");
		std::string line;
		while (std::getline(f, line)) { auto q = QuotedStrings(line); if (q.size() == 2 && q[0] == "path" && std::find(libs.begin(), libs.end(), q[1]) == libs.end()) libs.push_back(q[1]); }
	}
	std::vector<SteamApp> apps;
	for (auto &l : libs) {
		const fs::path sa = fs::u8path(l) / "steamapps";
		for (auto &e : fs::directory_iterator(sa, fs::directory_options::skip_permission_denied, ec)) {
			const std::string fn = e.path().filename().string();
			if (fn.rfind("appmanifest_", 0) != 0) continue;
			std::ifstream f(e.path()); std::string line; SteamApp a;
			while (std::getline(f, line)) { auto q = QuotedStrings(line); if (q.size() == 2) { if (q[0] == "name") a.name = q[1]; else if (q[0] == "installdir") a.dir = (sa / "common" / fs::u8path(q[1])).u8string(); } }
			if (!a.dir.empty()) apps.push_back(a);
		}
	}
	return apps;
}
} // namespace

const Install &InstallOf(Game g) { static Install none; std::lock_guard<std::mutex> lk(g_mx); auto it = g_found.find(g); return it == g_found.end() ? none : it->second; }
bool ScanDone() { std::lock_guard<std::mutex> lk(g_mx); return g_done; }

void RescanInstalls()
{
	{ std::lock_guard<std::mutex> lk(g_mx); if (g_started) return; g_started = true; }
	PostJob([] {
		std::map<Game, Install> found;
		const auto apps = SteamApps();
		std::error_code ec;
		for (auto &d : Games()) {
			auto tryDir = [&](const std::string &p, const char *how) {
				if (!found[d.id].path.empty() || p.empty() || !fs::is_directory(fs::u8path(p), ec)) return;
				const GameInfo gi = DetectGame(p);
				if (gi.game == d.id) found[d.id] = Install{ p, how };
			};
			tryDir(RememberedDir(d.id), "remembered");
			for (auto &a : apps) for (auto *n : d.steamNames) if (Lower(a.name).find(Lower(n)) != std::string::npos) tryDir(a.dir, "Steam");
			for (auto *c : d.commonPaths) tryDir(c, "common path");
		}
		PostMain([found] { std::lock_guard<std::mutex> lk(g_mx); g_found = found; g_done = true; });
	});
}

} // namespace abrowser
