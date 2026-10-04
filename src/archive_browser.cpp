// Archive browser: model, worker thread, decoders, write-back. The ImGui side is archive_browser_ui.cpp.
#include "archive_browser_state.h"
#include "game_table.h"
#include "framedata_pb2k1.h"
#include "han2/qoh_dat.h"
#include "cgm/cgm_bank.h"
#include "character_instance.h"
#include "framedata_ha4.h"
#include "ha4_character.h"
#include "han2/han2_container.h"
#include "han2/img_file.h"
#include "han2/misc_formats.h"
#include "han2/qoh_img.h"
#include "han2/rosa_img.h"
#include "framedata_gof1.h"
#include "han2_export.h"
#include "han2_pat.h"
#include "framedata_han2.h"
#include "misc.h"

#include <windows.h>
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#define STBI_ONLY_PNG
#include "../third_party/stb_image/stb_image.h"

namespace fs = std::filesystem;

namespace abrowser {

bool show = false;

// ---- tiny helpers -------------------------------------------------------------------------------------------------------------------------------------
static std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
static std::string ExtOf(const std::string &name)
{
	size_t sl = name.find_last_of("/\\"), d = name.find_last_of('.');
	if (d == std::string::npos || (sl != std::string::npos && d < sl)) return {};
	return Lower(name.substr(d));
}
static std::string BaseName(const std::string &p) { size_t s = p.find_last_of("/\\"); return s == std::string::npos ? p : p.substr(s + 1); }
static std::string StemOf(const std::string &raw) { std::string b = BaseName(raw); size_t d = b.find_last_of('.'); return d == std::string::npos ? b : b.substr(0, d); }
static fs::path P8(const std::string &u8) { return fs::u8path(u8); }

const char *GameName(Game g)
{
	if (g == Game::Generic) return "Folder";
	const GameDef *d = FindGame(g);
	return d ? d->name : "";
}
const char *TypeName(Type t)
{
	switch (t) {
	case Type::Character: return "Character";
	case Type::CharData: return "Character data";
	case Type::Image: return "Image";
	case Type::CgBank: return "Sprite bank";
	case Type::Parts: return "Parts";
	case Type::Script: return "Script / data";
	case Type::Audio: return "Audio";
	case Type::Text: return "Text";
	case Type::Palette: return "Palette";
	case Type::Model: return "3D model";
	case Type::Archive: return "Archive";
	default: return "File";
	}
}
Type TypeOfName(const std::string &name, Game ctx, bool inArchive)
{
	const std::string e = ExtOf(name);
	if (e == ".ha6" && (ctx == Game::UNI2 || ctx == Game::MBTL || ctx == Game::DFCI || ctx == Game::UNIST || ctx == Game::UNI)) {
		// modern layout: <Name>/<Name>.HA6 is the character; _temp / BaseData / effect are shared pattern libraries, _csel / _coloredit are menu-only sets
		const std::string l = Lower(name), b = Lower(StemOf(name));
		if (b[0] == '_' || b == "basedata" || b.rfind("effect", 0) == 0 || l.find("/_csel/") != std::string::npos || l.find("/_coloredit/") != std::string::npos || l.rfind("_csel/", 0) == 0) return Type::CharData;
		return Type::Character;
	}
	if (e == ".dt2" || e == ".ha6" || e == ".chr") return Type::Character;
	if (e == ".dat") return ctx == Game::MBAACC ? Type::Other : Type::Character;   // MBAACC .DAT files are stages / backgrounds; its characters are .HA6
	if (e == ".img" || e == ".ex3" || e == ".bmp" || e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".gif" || e == ".dds") return Type::Image;
	if (e == ".cg" || e == ".chp") return Type::CgBank;
	if (e == ".pat") return Type::Parts;
	if (e == ".fob" || e == ".ct" || e == ".wmt" || e == ".cpf" || e == ".ai" || e == ".lst") return Type::Script;
	if (e == ".wav" || e == ".mp3" || e == ".ogg" || e == ".mpg" || e == ".mid") return Type::Audio;
	if (e == ".txt" || e == ".ini" || e == ".h" || e == ".csv" || e == ".json" || e == ".cfg") return Type::Text;
	if (e == ".pal") return Type::Palette;
	if (e == ".b") return Type::Model;
	if (e == ".pac" || e == ".p") return Type::Archive;
	return Type::Other;
}

// ---- worker thread ----------------------------------------------------------------------------------------------------------------------------------
namespace {
struct Worker {
	std::mutex mx;
	std::condition_variable cv;
	std::deque<std::function<void()>> hi, lo;
	std::deque<std::function<void()>> results;
	std::thread th;
	bool stop = false;
	std::atomic<int> active{0};
	void start()
	{
		if (th.joinable()) return;
		th = std::thread([this] {
			han2::SetNoGlUpload(true);   // thumbnails load characters without creating GL textures
			for (;;) {
				std::function<void()> f;
				{
					std::unique_lock<std::mutex> lk(mx);
					cv.wait(lk, [this] { return stop || !hi.empty() || !lo.empty(); });
					if (stop) return;
					if (!hi.empty()) { f = std::move(hi.front()); hi.pop_front(); }
					else { f = std::move(lo.back()); lo.pop_back(); }   // newest thumbnail request first: what is on screen now
					active++;
				}
				try { f(); } catch (...) {}
				active--;
			}
		});
		th.detach();
	}
};
Worker &W() { static Worker *w = new Worker(); return *w; }   // leaked on purpose: the detached worker may outlive static destruction
}

void PostJob(std::function<void()> f, bool low)
{
	Worker &w = W(); w.start();
	{ std::lock_guard<std::mutex> lk(w.mx); (low ? w.lo : w.hi).push_back(std::move(f)); }
	w.cv.notify_one();
}
void PostMain(std::function<void()> f) { Worker &w = W(); std::lock_guard<std::mutex> lk(w.mx); w.results.push_back(std::move(f)); }
void PumpMain()
{
	Worker &w = W();
	for (int n = 0; n < 64; n++) {
		std::function<void()> f;
		{ std::lock_guard<std::mutex> lk(w.mx); if (w.results.empty()) return; f = std::move(w.results.front()); w.results.pop_front(); }
		f();
	}
}
bool WorkerBusy()
{
	Worker &w = W();
	std::lock_guard<std::mutex> lk(w.mx);
	return w.active > 0 || !w.hi.empty() || !w.results.empty();
}
bool Busy()
{
	if (WorkerBusy()) return true;
	for (auto &s : S().sources) if (s->state == Source::Loading) return true;
	return S().prev.kind == Preview::Loading || S().thumbsPending > 0;
}

State &S() { static State *s = new State(); return *s; }

// ---- settings ------------------------------------------------------------------------------------------------------------------------------------------
static std::string IniPath()
{
	char buf[512]; DWORD n = GetCurrentDirectoryA(512, buf);
	return std::string(buf, n) + "\\han2_settings.ini";
}
void LoadSettings()
{
	State &st = S();
	if (st.restored) return;
	st.restored = true;
	const std::string p = IniPath();
	char b[2048];
	GetPrivateProfileStringA("browser", "Last", "", b, sizeof b, p.c_str()); st.lastBrowsed = b;
	st.thumbs = GetPrivateProfileIntA("browser", "Thumbs", 1, p.c_str()) != 0;
	for (int i = 0; i < 10; i++) {
		char k[16]; snprintf(k, sizeof k, "Recent%d", i);
		GetPrivateProfileStringA("browser", k, "", b, sizeof b, p.c_str());
		if (b[0]) st.recent.push_back(b);
	}
}
void SaveSettings()
{
	State &st = S();
	const std::string p = IniPath();
	WritePrivateProfileStringA("browser", "Last", st.lastBrowsed.c_str(), p.c_str());
	WritePrivateProfileStringA("browser", "Thumbs", st.thumbs ? "1" : "0", p.c_str());
	for (int i = 0; i < 10; i++) {
		char k[16]; snprintf(k, sizeof k, "Recent%d", i);
		WritePrivateProfileStringA("browser", k, i < (int)st.recent.size() ? st.recent[i].c_str() : nullptr, p.c_str());
	}
}
void RememberBrowsed(const std::string &path)
{
	State &st = S();
	st.lastBrowsed = path;
	auto it = std::find(st.recent.begin(), st.recent.end(), path);
	if (it != st.recent.end()) st.recent.erase(it);
	st.recent.insert(st.recent.begin(), path);
	if (st.recent.size() > 10) st.recent.resize(10);
	SaveSettings();
}
const std::string &LastBrowsed() { LoadSettings(); return S().lastBrowsed; }
const std::vector<std::string> &RecentBrowsed() { LoadSettings(); return S().recent; }

// ---- game detection ------------------------------------------------------------------------------------------------------------------------------
static bool SniffArchive(const fs::path &p, const std::string &ext)
{
	std::ifstream f(p, std::ios::binary);
	uint8_t head[64]{}; f.read((char *)head, sizeof head); const size_t n = (size_t)f.gcount();
	if (n < 8) return false;
	if (pac::LooksLikePac(head, n)) return true;
	if (ext == ".p" && gof1::LooksLikeArchive(head, n)) return true;
	return fbarc::Detect(head, n, ext) != fbarc::Kind::Unknown;
}

static int ArchiveRank(const std::string &lowerName)
{
	if (lowerName.rfind("data0", 0) == 0 || lowerName.rfind("data1", 0) == 0) return 0;
	if (lowerName.rfind("update", 0) == 0) return 5;
	if (lowerName.rfind("ex", 0) == 0) return 6;
	return 3;
}

static bool IsDigits2P(const std::string &n) { return n.size() == 4 && isdigit((unsigned char)n[0]) && isdigit((unsigned char)n[1]) && n.compare(2, 2, ".p") == 0; }

GameInfo DetectGame(const std::string &dir)
{
	GameInfo gi; gi.root = dir;
	std::error_code ec;
	if (!fs::is_directory(P8(dir), ec)) return gi;
	std::vector<fs::path> cands{ P8(dir) };
	for (auto &e : fs::directory_iterator(P8(dir), ec)) {
		std::error_code e2;
		if (!e.is_directory(e2)) continue;
		const std::string n = Lower(e.path().filename().string());
		if (n == "data" || n == "run" || n == "install") cands.push_back(e.path());
	}
	bool exeGof1 = false, exeGof2 = false, exeMbaa = false, haveHa6 = false, havePac = false, haveData0N = false, haveGofP = false, haveMbP = false, havePbDat = false;
	bool exeMbtl = false, exeUni2 = false, exeRing = false, exeUnist = false, exeUni = false, exeLilian = false, exeQoh99 = false, exeQoh98 = false, exeDmp = false, exeRosa = false;
	bool havePk = false, haveDigitP = false, haveChr = false, haveGamedataPac = false, haveBaseData = false;
	fs::path ha6Dir, dataDirCand, mbtlExe;
	std::vector<std::pair<std::string, fs::path>> allArchives;   // lower name, path
	for (auto &c : cands) {
		for (auto &e : fs::directory_iterator(c, ec)) {
			std::error_code e2;
			if (!e.is_regular_file(e2)) continue;
			const std::string n = Lower(e.path().filename().string()), x = Lower(e.path().extension().string());
			if (n == "gof.exe") exeGof1 = true;
			if (n == "gof2.exe") exeGof2 = true;
			if (n == "mbaa.exe") exeMbaa = true;
			if (n == "mbtl.exe") { exeMbtl = true; mbtlExe = e.path(); }
			if (n == "uni2.exe") exeUni2 = true;
			if (n == "ringgame.exe") exeRing = true;
			if (n.rfind("unist", 0) == 0 && x == ".exe") exeUnist = true;
			if ((n == "uni.exe" || n == "unib.exe" || n == "uniclr.exe" || n == "uni_clr.exe") && x == ".exe") exeUni = true;
			if (n == "lilianfourhand.exe") exeLilian = true;
			if (n.rfind("qoh99", 0) == 0 && x == ".exe") exeQoh99 = true;
			if (n == "qoh98.exe") exeQoh98 = true;
			if (n == "dmp.exe") exeDmp = true;
			if (n == "rosa_fh.exe") exeRosa = true;
			if (n == "gamedata.pac") haveGamedataPac = true;
			if (n == "basedata.ha6") haveBaseData = true;
			if (x == ".ha6") { haveHa6 = true; if (ha6Dir.empty()) ha6Dir = c; }
			if (x == ".pac" && SniffArchive(e.path(), x)) { havePac = true; allArchives.emplace_back(n, e.path()); if (dataDirCand.empty()) dataDirCand = c; }
			else if (x == ".dat" && n.size() == 10 && n.rfind("data0", 0) == 0 && SniffArchive(e.path(), x)) { haveData0N = true; allArchives.emplace_back(n, e.path()); if (dataDirCand.empty()) dataDirCand = c; }
			else if (x == ".p" && SniffArchive(e.path(), x)) {
				allArchives.emplace_back(n, e.path());
				if (n.rfind("gof_0", 0) == 0) { haveGofP = true; dataDirCand = c; }
				else if (n.rfind("data0", 0) == 0) haveMbP = true;
				else if (IsDigits2P(n)) {
					haveDigitP = true;
					std::ifstream f(e.path(), std::ios::binary); uint8_t h[16]{}; f.read((char *)h, 16);
					if (!memcmp(h, "PKFileInfo", 10)) havePk = true;
				}
			} else if (x == ".dat" && n.size() >= 6 && isdigit((unsigned char)n[0]) && SniffArchive(e.path(), x)) { havePbDat = true; allArchives.emplace_back(n, e.path()); }
		}
	}
	// loose per-character folders (Queen of Heart): <name>\<name>.chr one level down
	if (!exeQoh99 && !exeQoh98) {
		int n = 0;
		for (auto &e : fs::directory_iterator(P8(dir), ec)) {
			std::error_code e2; if (!e.is_directory(e2)) continue;
			for (auto &f : fs::directory_iterator(e.path(), ec)) if (Lower(f.path().extension().string()) == ".chr") { n++; break; }
			if (n >= 2) break;
		}
		haveChr = n >= 2;
	}
	const bool uni2D = fs::is_directory(P8(dir) / "d", ec) && (exeUni2 || fs::exists(P8(dir) / "d" / "hexeojmpimrjs", ec));
	const bool modernLoose = haveBaseData && fs::is_directory(P8(dir) / "data", ec) && !exeMbaa && !exeMbtl && !uni2D;
	Game g = Game::None;
	if (exeMbtl) g = Game::MBTL;
	else if (uni2D) g = Game::UNI2;
	else if (exeRing) g = Game::DFCI;
	else if (exeUnist && modernLoose) g = Game::UNIST;
	else if (modernLoose && !haveGofP) g = Game::UNI;
	else if (haveGofP || exeGof1) g = Game::GOF1;
	else if (haveData0N || exeGof2) g = Game::GOF2;
	else if (exeLilian) g = Game::LILIAN;
	else if (exeRosa) g = Game::ROSA;
	else if (exeDmp || haveGamedataPac) g = Game::DMP;
	else if (havePac) g = Game::RBO;
	else if (exeQoh98) g = Game::QOH98;
	else if (exeQoh99 || haveChr) g = Game::QOH99;
	else if (exeMbaa || haveHa6) g = Game::MBAACC;
	else if (havePk) g = Game::MBAC;
	else if (haveDigitP) g = Game::REACT;
	else if (haveMbP) g = Game::MB;
	else if (havePbDat) g = Game::PB2K1;
	else if (!allArchives.empty()) g = Game::Generic;
	if (g == Game::None) return gi;
	gi.game = g;
	std::sort(allArchives.begin(), allArchives.end(), [](const auto &a, const auto &b) { int ra = ArchiveRank(a.first), rb = ArchiveRank(b.first); return ra != rb ? ra < rb : a.first < b.first; });
	std::set<std::string> seen;
	for (auto &a : allArchives) if (seen.insert(Lower(a.second.generic_u8string())).second) gi.archives.push_back(a.second.u8string());
	if (g == Game::MBTL) gi.archives = { mbtlExe.u8string() };
	if (g == Game::UNI2) gi.archives = { (P8(dir) / "d").u8string() };
	if (g == Game::MBAACC) gi.dataDir = !ha6Dir.empty() ? ha6Dir.u8string() : (fs::is_directory(P8(dir) / "data", ec) ? (P8(dir) / "data").u8string() : dir);
	else if (g == Game::DFCI || g == Game::UNI || g == Game::UNIST) gi.dataDir = (P8(dir) / "data").u8string();
	else gi.dataDir = !dataDirCand.empty() ? dataDirCand.u8string() : dir;
	return gi;
}

// ---- reading -------------------------------------------------------------------------------------------------------------------------------------------
const Source &Owner(const Source &s, const Item &it, uint32_t &ownerIndex)
{
	ownerIndex = it.index;
	if (s.kind == Source::Merged && it.member < s.members.size()) return *s.members[it.member];
	return s;
}

bool ReadItem(const Source &s, const Item &it, std::vector<uint8_t> &out, std::string *err, size_t maxBytes)
{
	uint32_t idx; const Source &o = Owner(s, it, idx);
	auto cap = [&](bool ok) { if (ok && out.size() > maxBytes) out.resize(maxBytes); return ok; };
	switch (o.kind) {
	case Source::Pac: {
		if (!o.pac || idx >= o.pac->entries.size()) { if (err) *err = "entry out of range"; return false; }
		const pac::Entry &e = o.pac->entries[idx];
		std::ifstream f(P8(o.pac->path), std::ios::binary);
		if (!f) { if (err) *err = "cannot open " + o.pac->path; return false; }
		const size_t n = (size_t)std::min<uint64_t>(e.size, maxBytes);
		f.seekg((std::streamoff)e.offset); out.resize(n);
		if (n) f.read((char *)out.data(), (std::streamsize)n);
		if (!f) { if (err) *err = "short read of " + it.name; return false; }
		return true;
	}
	case Source::Gof1: return cap(o.g1 && gof1::ReadEntry(*o.g1, idx, out, err));
	case Source::Fb: return o.fb && o.fb->read(idx, out, err, maxBytes);
	case Source::Folder: {
		std::ifstream f(P8(it.key), std::ios::binary);
		if (!f) { if (err) *err = "cannot open " + it.key; return false; }
		f.seekg(0, std::ios::end); const uint64_t sz = (uint64_t)f.tellg(); f.seekg(0);
		out.resize((size_t)std::min<uint64_t>(sz, maxBytes));
		if (!out.empty()) f.read((char *)out.data(), (std::streamsize)out.size());
		return true;
	}
	default: return false;
	}
}

std::string ItemPathText(const Source &s, const Item &it)
{
	uint32_t idx; const Source &o = Owner(s, it, idx);
	if (o.kind == Source::Folder) return it.key;
	return o.path + " : " + it.name;
}

// ---- building sources ----------------------------------------------------------------------------------------------------------------------------
static void FillArchiveSource(Source &s)
{
	std::string err, e2, e3;
	{   // UNI2's d\ folder and MBTL.exe (its table) are archives without a PAC header
		std::error_code dec;
		const std::string fn = Lower(BaseName(s.path));
		if (fs::is_directory(P8(s.path), dec) || fn == "mbtl.exe") {
			std::shared_ptr<fbarc::Archive> fa = fbarc::Open(s.path, &e3);
			if (!fa) { s.error = e3; s.state = Source::Failed; return; }
			s.kind = Source::Fb; s.fb = fa;
			s.describe = std::string(fbarc::KindName(fa->kind())) + ", " + std::to_string(fa->entries().size()) + " entries; " + fa->describe();
			s.items.reserve(fa->entries().size());
			for (size_t i = 0; i < fa->entries().size(); i++) {
				Item it; it.key = fa->relativePath(i); it.name = it.key; it.size = fa->entries()[i].size; it.index = (uint32_t)i;
				it.type = TypeOfName(it.name, s.game, true); s.items.push_back(std::move(it));
			}
			return;
		}
	}
	auto pa = std::make_shared<pac::Archive>();
	if (pac::Open(s.path, *pa, &err)) {
		s.kind = Source::Pac; s.pac = pa;
		char d[160]; snprintf(d, sizeof d, "PAC archive (RBO / GOF2), %zu entries", pa->entries.size()); s.describe = d;
		s.items.reserve(pa->entries.size());
		for (size_t i = 0; i < pa->entries.size(); i++) {
			Item it; it.key = pa->entries[i].name; it.name = sj2utf8(it.key); it.size = pa->entries[i].size; it.index = (uint32_t)i;
			it.type = TypeOfName(it.name, s.game, true); s.items.push_back(std::move(it));
		}
		return;
	}
	auto ga = std::make_shared<gof1::Archive>();
	if (gof1::Open(s.path, *ga, &e2)) {
		s.kind = Source::Gof1; s.g1 = ga;
		char d[160]; snprintf(d, sizeof d, "GOF1 archive, %zu entries", ga->entries.size()); s.describe = d;
		for (size_t i = 0; i < ga->entries.size(); i++) {
			Item it; it.key = ga->entries[i].name; it.name = sj2utf8(it.key); it.size = ga->entries[i].size; it.index = (uint32_t)i;
			it.type = TypeOfName(it.name, s.game, true); s.items.push_back(std::move(it));
		}
		return;
	}
	std::shared_ptr<fbarc::Archive> fa = fbarc::Open(s.path, &e3);
	if (fa) {
		s.kind = Source::Fb; s.fb = fa;
		s.describe = std::string(fbarc::KindName(fa->kind())) + ", " + std::to_string(fa->entries().size()) + " entries; " + fa->describe();
		for (size_t i = 0; i < fa->entries().size(); i++) {
			Item it; it.key = fa->relativePath(i); it.name = fbarc::NameToUtf8(it.key); it.size = fa->entries()[i].size; it.index = (uint32_t)i;
			it.type = TypeOfName(it.name, s.game, true); s.items.push_back(std::move(it));
		}
		return;
	}
	s.error = err.empty() ? s.path + ": not a French Bread archive" : err;
	s.state = Source::Failed;
}

static void ScanFolderInto(Source &s, const std::string &root, bool recursive)
{
	std::error_code ec;
	const fs::path base = P8(root);
	size_t guard = 0;
	std::function<void(const fs::path &, int)> walk = [&](const fs::path &dir, int depth) {
		for (auto &e : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
			if (s.closed || ++guard > 40000) return;
			std::error_code e2;
			if (e.is_directory(e2)) { if (recursive && depth < 3) walk(e.path(), depth + 1); continue; }
			if (!e.is_regular_file(e2)) continue;
			Item it; it.key = e.path().u8string();
			it.name = fs::relative(e.path(), base, e2).generic_u8string();
			it.size = (uint64_t)e.file_size(e2);
			it.type = TypeOfName(it.name, s.game, false);
			const std::string x = ExtOf(it.name);
			if ((x == ".dat" || x == ".p" || x == ".pac") && it.size >= 8 && SniffArchive(e.path(), x)) it.type = Type::Archive;
			it.index = (uint32_t)s.items.size();
			s.items.push_back(std::move(it));
		}
	};
	walk(base, 0);
	std::sort(s.items.begin(), s.items.end(), [](const Item &a, const Item &b) { return Lower(a.name) < Lower(b.name); });
	{   // <stem>_0.HA6 / _1 / _r ... are the version / mirror files of <stem>.HA6: they are character data, not characters of their own
		std::set<std::string> stems;
		for (auto &it : s.items) if (ExtOf(it.name) == ".ha6") stems.insert(Lower(it.name.substr(0, it.name.size() - 4)));
		for (auto &it : s.items) {
			if (ExtOf(it.name) != ".ha6") continue;
			std::string st = Lower(it.name.substr(0, it.name.size() - 4));
			bool variant = false;
			for (;;) {
				if (st.size() > 2 && st.compare(st.size() - 2, 2, "_r") == 0) st.resize(st.size() - 2);
				else if (st.size() > 2 && st[st.size() - 2] == '_' && isdigit((unsigned char)st.back())) st.resize(st.size() - 2);
				else break;
				variant = true;
			}
			if (variant && stems.count(st)) it.type = Type::CharData;
		}
	}
	for (size_t i = 0; i < s.items.size(); i++) s.items[i].index = (uint32_t)i;
	char d[200]; snprintf(d, sizeof d, "Folder, %zu files", s.items.size()); s.describe = d;
}

static std::shared_ptr<Group> GroupById(int id) { for (auto &g : S().groups) if (g->id == id) return g; return nullptr; }

void RebuildMerged(Group &g)
{
	if (!g.merged) return;
	Source &m = *g.merged;
	m.members.clear();
	std::unordered_map<std::string, std::pair<uint16_t, uint32_t>> win;
	std::vector<std::string> order;
	for (auto &a : g.archives) {
		if (a->state != Source::Ready) continue;
		const uint16_t mi = (uint16_t)m.members.size();
		m.members.push_back(a);
		for (size_t i = 0; i < a->items.size(); i++) {
			const std::string k = Lower(a->items[i].name);
			auto it = win.find(k);
			if (it == win.end()) { order.push_back(k); win[k] = { mi, (uint32_t)i }; }
			else it->second = { mi, (uint32_t)i };
		}
	}
	std::vector<Item> items; items.reserve(order.size());
	std::set<std::string> dt2Stems;
	for (auto &k : order) {
		auto &w = win[k];
		Item it = m.members[w.first]->items[w.second];
		it.member = w.first; it.index = w.second;
		it.type = TypeOfName(it.name, g.info.game, true);
		if (ExtOf(it.name) == ".dt2") dt2Stems.insert(Lower(StemOf(it.name)));
		items.push_back(std::move(it));
	}
	for (auto &it : items) if (ExtOf(it.name) == ".dat" && dt2Stems.count(Lower(StemOf(it.name)))) it.type = Type::CharData;   // the DT2 is the character; its DAT holds the sprites
	std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return Lower(a.name) < Lower(b.name); });
	m.items = std::move(items);
	m.describe = std::to_string(m.items.size()) + " files from " + std::to_string(m.members.size()) + " archives (a later archive overrides an earlier one, as the game does)";
	m.state = Source::Ready;
	S().viewDirty = true;
}

void MountArchive(const std::string &path, int group, bool select)
{
	State &st = S();
	for (size_t i = 0; i < st.sources.size(); i++) if (st.sources[i]->IsArchive() && Lower(st.sources[i]->path) == Lower(path)) { if (select) { st.selSource = (int)i; st.viewDirty = true; st.sel.clear(); } return; }
	auto s = std::make_shared<Source>();
	s->path = path; s->label = P8(path).filename().u8string(); s->group = group;
	if (auto g = GroupById(group)) { s->game = g->info.game; g->archives.push_back(s); }
	st.sources.push_back(s);
	if (select) { st.selSource = (int)st.sources.size() - 1; st.viewDirty = true; st.sel.clear(); st.cursor = st.anchor = -1; }
	std::weak_ptr<Source> ws = s;
	PostJob([ws, group] {
		auto s = ws.lock(); if (!s || s->closed) return;
		FillArchiveSource(*s);
		const bool ok = s->state != Source::Failed;
		PostMain([ws, group, ok] {
			auto s = ws.lock(); if (!s) return;
			if (ok) s->state = Source::Ready;
			if (auto g = GroupById(group)) RebuildMerged(*g);
			S().viewDirty = true;
		});
	});
}

void ReloadArchive(const std::string &path)
{
	State &st = S();
	for (auto &s : st.sources) {
		if (!s->IsArchive() || Lower(s->path) != Lower(path)) continue;
		// replace the contents in place on the UI thread after a fresh read on the worker
		auto fresh = std::make_shared<Source>();
		fresh->path = s->path; fresh->game = s->game; fresh->group = s->group;
		std::weak_ptr<Source> ws = s;
		PostJob([ws, fresh] {
			FillArchiveSource(*fresh);
			PostMain([ws, fresh] {
				auto s = ws.lock(); if (!s || fresh->state == Source::Failed) return;
				s->kind = fresh->kind; s->pac = fresh->pac; s->g1 = fresh->g1; s->fb = fresh->fb; s->items = std::move(fresh->items); s->describe = fresh->describe;
				if (auto g = GroupById(s->group)) RebuildMerged(*g);
				S().viewDirty = true; S().prev = Preview(); S().prevSource = S().prevItem = -1;
				S().thumbCache.clear();
			});
		});
	}
}

void CloseSource(int idx)
{
	State &st = S();
	if (idx < 0 || idx >= (int)st.sources.size()) return;
	SourceP s = st.sources[idx];
	s->closed = true;
	if (auto g = GroupById(s->group)) {
		if (s == g->merged || s == g->folder) {   // closing the game's merged view or folder closes the whole game
			std::vector<SourceP> all = g->archives; all.push_back(g->merged); all.push_back(g->folder);
			st.sources.erase(std::remove_if(st.sources.begin(), st.sources.end(), [&](const SourceP &x) { return std::find(all.begin(), all.end(), x) != all.end(); }), st.sources.end());
			st.groups.erase(std::remove(st.groups.begin(), st.groups.end(), g), st.groups.end());
		} else {
			g->archives.erase(std::remove(g->archives.begin(), g->archives.end(), s), g->archives.end());
			st.sources.erase(st.sources.begin() + idx);
			RebuildMerged(*g);
		}
	} else st.sources.erase(st.sources.begin() + idx);
	st.selSource = st.sources.empty() ? -1 : std::min(st.selSource, (int)st.sources.size() - 1);
	if (st.selSource < 0) st.selSource = st.sources.empty() ? -1 : 0;
	st.viewDirty = true; st.sel.clear(); st.cursor = st.anchor = -1;
	st.prev = Preview(); st.prevSource = st.prevItem = -1; st.thumbCache.clear();
}

std::string OpenPath(const std::string &path)
{
	State &st = S();
	LoadSettings();
	std::error_code ec;
	const fs::path p = P8(path);
	if (fs::is_directory(p, ec)) {
		GameInfo gi = DetectGame(path);
		auto g = std::make_shared<Group>(); g->id = st.nextGroup++; g->info = gi;
		st.groups.push_back(g);
		const std::string title = gi.game == Game::None || gi.game == Game::Generic ? p.filename().u8string() : std::string(GameName(gi.game));
		if (gi.game == Game::None) gi.game = Game::Generic;
		g->info.game = gi.game;
		g->merged = std::make_shared<Source>();
		g->merged->kind = Source::Merged; g->merged->label = "All archives (game view)"; g->merged->game = gi.game; g->merged->group = g->id; g->merged->state = Source::Ready;
		g->folder = std::make_shared<Source>();
		g->folder->kind = Source::Folder; g->folder->label = (gi.game == Game::MBAACC || gi.game == Game::DFCI || gi.game == Game::UNI || gi.game == Game::UNIST || gi.game == Game::QOH99 || gi.game == Game::QOH98 ? "Characters: " : "Files in ") + (gi.dataDir.empty() ? p.filename().u8string() : P8(gi.dataDir).filename().u8string());
		g->folder->path = gi.dataDir.empty() ? path : gi.dataDir; g->folder->game = gi.game; g->folder->group = g->id;
		const bool haveArchives = !gi.archives.empty();
		if (haveArchives) st.sources.push_back(g->merged);
		st.sources.push_back(g->folder);
		const int folderIdx = (int)st.sources.size() - 1, mergedIdx = haveArchives ? folderIdx - 1 : -1;
		std::weak_ptr<Source> wf = g->folder;
		const std::string scanRoot = g->folder->path;
		const bool recursive = gi.game == Game::Generic || gi.game == Game::DFCI || gi.game == Game::UNI || gi.game == Game::UNIST || gi.game == Game::QOH99 || gi.game == Game::QOH98 ||
		                       gi.game == Game::ROSA || gi.game == Game::LILIAN || gi.game == Game::DMP;
		PostJob([wf, scanRoot, recursive] {
			auto s = wf.lock(); if (!s) return;
			ScanFolderInto(*s, scanRoot, recursive);
			PostMain([wf] { auto s = wf.lock(); if (s) { s->state = Source::Ready; S().viewDirty = true; } });
		});
		for (auto &a : gi.archives) MountArchive(a, g->id, false);
		st.selSource = (haveArchives && gi.game != Game::MBAACC) ? mergedIdx : folderIdx;   // MBAACC characters are loose .HA6 files: start on the folder
		st.viewDirty = true; st.sel.clear(); st.cursor = st.anchor = -1;
		const bool charGame = gi.game == Game::DFCI || gi.game == Game::UNI || gi.game == Game::UNIST || gi.game == Game::QOH99 || gi.game == Game::QOH98;
		if ((haveArchives && gi.game != Game::Generic) || charGame) st.typeFilter = (int)Type::Character; else st.typeFilter = -1;
		st.filter[0] = 0;
		show = true;
		RememberBrowsed(path);
		char b[256]; snprintf(b, sizeof b, "%s: %zu archives", title.c_str(), gi.archives.size()); st.status = b;
		return {};
	}
	if (!fs::exists(p, ec)) return path + ": not found";
	// a file: an archive, or any file (its folder opens with the file selected)
	if (SniffArchive(p, ExtOf(path)) || ExtOf(path) == ".pac") {
		auto g = std::make_shared<Group>(); g->id = st.nextGroup++; g->info.game = Game::Generic; g->info.root = p.parent_path().u8string();
		st.groups.push_back(g);
		MountArchive(path, g->id, true);
		st.typeFilter = -1; st.filter[0] = 0;
		show = true;
		RememberBrowsed(path);
		st.status = "Opened " + P8(path).filename().u8string();
		return {};
	}
	return OpenPath(p.parent_path().u8string());
}

// ---- image decoding ---------------------------------------------------------------------------------------------------------------------------------
static bool BmpToRgba(const han2::Bmp &b, std::vector<uint8_t> &rgba, int &w, int &h, std::vector<uint32_t> *sw)
{
	w = b.width; h = b.height < 0 ? -b.height : b.height;
	if (w <= 0 || h <= 0 || (uint64_t)w * h > (1ull << 28)) return false;
	const bool topDown = b.height < 0;
	rgba.assign((size_t)w * h * 4, 255);
	const size_t rowBytes = (((size_t)w * b.bpp + 31) / 32) * 4;
	if (b.pixels.size() < rowBytes * (size_t)h) return false;
	std::vector<uint32_t> pal;
	if (b.bpp <= 8) {
		const size_t entries = b.colorsUsed ? b.colorsUsed : (size_t)1 << b.bpp;
		for (size_t i = 0; i < std::min<size_t>(entries, b.palette.size() / 4); i++) pal.push_back(0xFF000000u | (b.palette[i * 4 + 0] << 16) | (b.palette[i * 4 + 1] << 8) | b.palette[i * 4 + 2]);   // B,G,R,x -> 0xAABBGGRR
		if (sw) *sw = pal;
	}
	for (int y = 0; y < h; y++) {
		const uint8_t *row = &b.pixels[rowBytes * (size_t)(topDown ? y : h - 1 - y)];
		uint8_t *o = &rgba[(size_t)y * w * 4];
		for (int x = 0; x < w; x++) {
			uint8_t r = 0, g = 0, bl = 0, a = 255;
			switch (b.bpp) {
			case 1: { uint32_t i = (row[x >> 3] >> (7 - (x & 7))) & 1; if (i < pal.size()) { uint32_t c = pal[i]; r = c & 255; g = (c >> 8) & 255; bl = (c >> 16) & 255; } break; }
			case 4: { uint32_t i = (x & 1) ? (row[x >> 1] & 15) : (row[x >> 1] >> 4); if (i < pal.size()) { uint32_t c = pal[i]; r = c & 255; g = (c >> 8) & 255; bl = (c >> 16) & 255; } break; }
			case 8: { uint32_t i = row[x]; if (i < pal.size()) { uint32_t c = pal[i]; r = c & 255; g = (c >> 8) & 255; bl = (c >> 16) & 255; } break; }
			case 16: { uint16_t v = row[x * 2] | (row[x * 2 + 1] << 8); r = (uint8_t)(((v >> 10) & 31) * 255 / 31); g = (uint8_t)(((v >> 5) & 31) * 255 / 31); bl = (uint8_t)((v & 31) * 255 / 31); break; }
			case 24: bl = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2]; break;
			case 32: bl = row[x * 4]; g = row[x * 4 + 1]; r = row[x * 4 + 2]; break;
			default: return false;
			}
			o[x * 4] = r; o[x * 4 + 1] = g; o[x * 4 + 2] = bl; o[x * 4 + 3] = a;
		}
	}
	return true;
}

static std::string Num(uint64_t v) { return std::to_string(v); }
static std::string Bytes(uint64_t n)
{
	char b[48];
	if (n >= (1u << 20)) snprintf(b, sizeof b, "%.1f MB", n / 1048576.0);
	else if (n >= 1024) snprintf(b, sizeof b, "%.1f KB", n / 1024.0);
	else snprintf(b, sizeof b, "%llu bytes", (unsigned long long)n);
	return b;
}

// Decodes any image-like file to RGBA. `how` receives a short description.
static bool DecodeImageBytes(const std::string &name, const std::vector<uint8_t> &d, std::vector<uint8_t> &rgba, int &w, int &h, std::string &how, std::vector<uint32_t> *sw)
{
	const std::string ext = ExtOf(name);
	std::string e;
	if (han2::IsImg(d.data(), d.size())) {
		han2::ImgFile im;
		if (han2::ParseImg(d.data(), d.size(), im, &e)) {
			rgba = im.rgba; w = im.width; h = im.height;
			const char *fm = im.format == 0 ? "ARGB1555" : im.format == 1 ? "ARGB4444" : im.format == 3 ? "RGB24" : "RGBA 8888";
			how = "French Bread IMG v" + std::to_string(im.version) + ", " + fm; return true;
		}
	}
	if (ext == ".img") {
		han2::QohImg q;
		if (han2::ParseQohImg(d.data(), d.size(), q, nullptr)) { rgba = q.rgba; w = (int)q.width; h = (int)q.height; how = "Queen of Heart image, " + std::to_string(q.bpp) + "-bit"; return true; }
		han2::RosaImg r;
		if (han2::ParseRosaImg(d.data(), d.size(), han2::RosaStemOfName(name), r, nullptr)) { rgba = r.rgba; w = (int)r.width; h = (int)r.height; how = "Rosa Chinensis image, " + std::to_string(r.bpp) + "-bit"; return true; }
	}
	if (d.size() > 8 && memcmp(d.data(), "LLIF", 4) == 0) {
		han2::Ex3 ex;
		if (han2::ParseEx3Auto(d.data(), d.size(), ex, &e)) {
			std::vector<uint8_t> bmp;
			if (han2::DecodeEx3(ex, bmp, &e)) {
				han2::Bmp b;
				if (han2::ParseBmp(bmp.data(), bmp.size(), b, &e) && BmpToRgba(b, rgba, w, h, sw)) { how = "EX3 (byte-pair compressed " + std::to_string(b.bpp) + "-bit BMP)"; return true; }
			}
		}
	}
	if (d.size() > 2 && d[0] == 'B' && d[1] == 'M') {
		han2::Bmp b;
		if (han2::ParseBmp(d.data(), d.size(), b, &e) && BmpToRgba(b, rgba, w, h, sw)) { how = "Windows BMP, " + std::to_string(b.bpp) + "-bit"; return true; }
	}
	if (d.size() > 8 && d[0] == 0x89 && d[1] == 'P') {
		int n = 0; unsigned char *px = stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &n, 4);
		if (px) { rgba.assign(px, px + (size_t)w * h * 4); stbi_image_free(px); how = "PNG"; return true; }
	}
	return false;
}

static void Downscale(const std::vector<uint8_t> &src, int sw, int sh, int maxSide, std::vector<uint8_t> &out, int &ow, int &oh)
{
	const float sc = std::min(1.f, (float)maxSide / (float)std::max(sw, sh));
	ow = std::max(1, (int)(sw * sc)); oh = std::max(1, (int)(sh * sc));
	out.assign((size_t)ow * oh * 4, 0);
	for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) {
		const int x0 = (int)(x / sc), x1 = std::max(x0 + 1, (int)((x + 1) / sc)), y0 = (int)(y / sc), y1 = std::max(y0 + 1, (int)((y + 1) / sc));
		float r = 0, g = 0, b = 0, a = 0; int n = 0;
		for (int yy = y0; yy < y1 && yy < sh; yy++) for (int xx = x0; xx < x1 && xx < sw; xx++) { const uint8_t *s = &src[((size_t)yy * sw + xx) * 4]; float al = s[3] / 255.f; r += s[0] * al; g += s[1] * al; b += s[2] * al; a += al; n++; }
		uint8_t *d = &out[((size_t)y * ow + x) * 4];
		if (n && a > 0.f) { d[0] = (uint8_t)(r / a); d[1] = (uint8_t)(g / a); d[2] = (uint8_t)(b / a); d[3] = (uint8_t)std::min(255.f, a / n * 255.f); }
	}
}

// ---- character preview ------------------------------------------------------------------------------------------------------------------------------
struct CharJob {
	enum Kind { Han2Stem, Gof1, Loose, Stack } kind = Loose;
	han2::ReadFn read; std::string stem, archivePath, entry, path, origin;
	SourceP owner; Item item;   // Stack: an entry of a (non-PAC) archive that is first unpacked as a working copy
};

static void AddCharFacts(CharacterInstance &ch, Preview &pv)
{
	FrameData &fd = ch.frameData;
	size_t patterns = 0, frames = 0;
	for (auto &sq : fd.m_sequences) if (!sq.frames.empty()) { patterns++; frames += sq.frames.size(); }
	pv.facts.push_back({ "Patterns", Num(patterns) + " (" + Num(frames) + " frames)" });
	if (ch.cg.m_loaded) pv.facts.push_back({ "Sprites (CG images)", Num((uint64_t)ch.cg.get_image_count()) });
	if (ch.parts.loaded) pv.facts.push_back({ "Poses (part sets)", Num(ch.parts.partSets.size()) });
	std::string names;
	int shown = 0;
	for (auto &sq : fd.m_sequences) { if (sq.frames.empty() || sq.name.empty()) continue; if (shown) names += ", "; names += sq.name; if (++shown >= 6) break; }
	if (!names.empty()) pv.facts.push_back({ "First patterns", names });
}

// ---- unpacking one character of a non-PAC archive (UNI2 / MBTL / MBAACC .p / MBAC / ...) as a working copy -------------------------------------------------
// A modern character is a STACK of files (txt project -> _temp.ha6, chrNNN.ha6, ../BaseData.HA6, .cg, .pal, .pat): copy the entries of the character's folder that
// belong to it plus the shared ones the txt names, into %TEMP%\hantei_archive\<game>_<archive>\<same relative paths>. Returns the file to open (the txt, else the .ha6).
static std::string DirOf(const std::string &rel) { size_t s = rel.find_last_of('/'); return s == std::string::npos ? std::string() : rel.substr(0, s); }
static std::string NormRel(std::string p)
{
	for (auto &c : p) if (c == '\\') c = '/';
	std::vector<std::string> parts; size_t i = 0;
	while (i <= p.size()) { size_t j = p.find('/', i); if (j == std::string::npos) j = p.size(); std::string seg = p.substr(i, j - i); if (seg == "..") { if (!parts.empty()) parts.pop_back(); } else if (!seg.empty() && seg != ".") parts.push_back(seg); i = j + 1; }
	std::string o; for (size_t k = 0; k < parts.size(); k++) o += (k ? "/" : "") + parts[k];
	return o;
}
std::string Materialize(const SourceP &o, const Item &it, std::string *err)
{
	if (!o || o->kind != Source::Fb || !o->fb) { if (err) *err = "not an archive entry"; return {}; }
	std::unordered_map<std::string, size_t> byName;
	for (size_t i = 0; i < o->items.size(); i++) byName[Lower(o->items[i].name)] = i;
	const std::string dir = DirOf(it.name), stem = Lower(StemOf(it.name));
	std::set<size_t> want;
	auto addNamed = [&](const std::string &rel) { auto f = byName.find(Lower(NormRel(rel))); if (f != byName.end()) want.insert(f->second); };
	for (size_t i = 0; i < o->items.size(); i++) {   // everything of this character in its folder, the shared _temp set, and the parent's BaseData / effect libraries
		const std::string n = Lower(o->items[i].name), d = DirOf(n), b = Lower(BaseName(n));
		if (d == Lower(dir)) {
			if (b.rfind(stem, 0) == 0 && b.size() > stem.size() && (b[stem.size()] == '.' || b[stem.size()] == '_')) want.insert(i);
			else if (b.rfind("_temp", 0) == 0 || b.rfind("basedata", 0) == 0) want.insert(i);
		} else if (d == Lower(DirOf(dir)) && (b.rfind("basedata", 0) == 0 || b.rfind("effect", 0) == 0 || b.rfind("sys_effect", 0) == 0)) want.insert(i);
	}
	addNamed(it.name);
	// the txt project names the rest (File%02d / BmpcutFile / PAniFile), relative to its own folder
	std::string txtName;
	for (const char *cand : { "_0.txt", ".txt" }) { std::string t = (dir.empty() ? "" : dir + "/") + StemOf(it.name) + cand; if (byName.count(Lower(t))) { txtName = t; break; } }
	std::error_code ec;
	const std::string game = std::string(GameName(o->game)) + "_" + Lower(o->label);
	fs::path root = fs::temp_directory_path(ec) / "hantei_archive";
	{ std::string g2; for (char c : game) g2 += (isalnum((unsigned char)c) ? c : '_'); root /= g2; }
	auto copyOne = [&](size_t i) -> bool {
		const Item &e = o->items[i];
		const fs::path out = root / P8(e.name);
		fs::create_directories(out.parent_path(), ec);
		if (fs::exists(out, ec) && fs::file_size(out, ec) == e.size) return true;
		std::vector<uint8_t> b; std::string er;
		if (!ReadItem(*o, e, b, &er)) { if (err) *err = er; return false; }
		std::ofstream f(out, std::ios::binary);
		if (!b.empty()) f.write((const char *)b.data(), (std::streamsize)b.size());
		return (bool)f;
	};
	if (!txtName.empty()) {   // parse the txt first (copy it, then read its [DataFile] etc. with the INI API)
		auto f = byName.find(Lower(txtName)); want.insert(f->second);
		if (!copyOne(f->second)) return {};
		const std::string tp = (root / P8(txtName)).u8string();
		char b[512];
		auto ref = [&](const char *sec, const char *key) { GetPrivateProfileStringA(sec, key, "", b, sizeof b, tp.c_str()); if (b[0]) addNamed((DirOf(txtName).empty() ? "" : DirOf(txtName) + "/") + b); };
		for (int i = 0; i < 40; i++) { char k[16]; snprintf(k, sizeof k, "File%02d", i); ref("DataFile", k); snprintf(k, sizeof k, "File%03d", i); ref("DataFile", k); }
		ref("DataFile", "File");
		for (int i = 0; i < 8; i++) { char k[16]; snprintf(k, sizeof k, "File%02d", i); ref("BmpcutFile", k); ref("PAniFile", k); }
	}
	for (size_t i : want) if (!copyOne(i)) return {};
	const auto mi = byName.find(Lower(it.name));
	return (root / P8(!txtName.empty() ? txtName : o->items[mi->second].name)).u8string();
}

// Loads one character file of any family the editor reads, by content (the same routing File > Open uses). `fmt` = what it was recognised as.
bool LoadCharacterFile(CharacterInstance &ch, const std::string &path, std::string *fmt, std::string *err)
{
	std::vector<uint8_t> head(4096);
	{ std::ifstream f(P8(path), std::ios::binary); f.read((char *)head.data(), (std::streamsize)head.size()); head.resize((size_t)f.gcount()); }
	if (head.empty()) { *err = "cannot read " + path; return false; }
	const std::string ext = ExtOf(path);
	auto starts = [&](const char *m, size_t n) { return head.size() >= n && memcmp(head.data(), m, n) == 0; };
	std::error_code ec;
	if (ext == ".ha6" || ext == ".dat" || ext == ".txt") {
		fs::path txt = P8(path);
		if (ext == ".ha6") {   // the sprites / palettes live in the character's txt stack: <stem>_0.txt or <stem>.txt
			const fs::path base = P8(path); const std::string stem = base.stem().u8string();
			for (const char *c : { "_0.txt", ".txt" }) {
				fs::path t = base.parent_path() / P8(stem + c);
				char probe[32]{};
				if (fs::exists(t, ec)) { GetPrivateProfileStringA("DataFile", "FileNum", "", probe, sizeof probe, t.u8string().c_str()); if (probe[0]) { if (ch.loadFromTxt(t.u8string())) { if (fmt) *fmt = std::string("Hantei6 character, txt stack (") + t.filename().u8string() + ")"; return true; } } }
			}
		}
		if (ext == ".txt") { if (ch.loadFromTxt(path)) { if (fmt) *fmt = "Hantei6 character, txt stack"; return true; } *err = "not a character project .txt"; return false; }
	}
	if (starts("Hantei6DataFile", 15) || starts("Hantei4\0", 8)) {
		if (!ch.loadHA6(path, false)) { *err = "Not a Hantei4 / HA6 character file"; return false; }
		if (fmt) *fmt = ch.frameData.isHA4() ? "MBAC / ReAct (Hantei4) character .DAT" : "Hantei6 character (.HA6)";
		const fs::path pp = P8(path);
		for (auto &e : fs::directory_iterator(pp.parent_path(), ec))
			if (Lower(e.path().stem().string()) == Lower(pp.stem().string()) && Lower(e.path().extension().string()) == ".cg") { ch.loadCG(e.path().u8string()); break; }
		return true;
	}
	if (starts("\xd9\x93\xfe\x3d", 4)) { if (!ch.loadGof1File(path, *err)) return false; if (fmt) *fmt = "Melty Blood / GOF1-family character .DAT"; return true; }
	std::error_code fe; const uintmax_t fsz = fs::file_size(P8(path), fe);
	if (!fe && ext == ".dat" && head.size() >= 0x41C && pb2k1::LooksLikeCharacter(head.data(), (size_t)fsz)) { if (!ch.loadPb2k1File(path, *err)) return false; if (fmt) *fmt = "Party Breakers character .DAT"; return true; }
	if (!fe && ext == ".chr") { if (!ch.loadQohFile(path, han2::qoh::V99, *err)) return false; if (fmt) *fmt = "Queen of Heart '99 character (.chr)"; return true; }
	if (!fe && ext == ".dat" && fsz > 28 + 1024 && fsz < (200u << 20)) {
		std::ifstream qf(P8(path), std::ios::binary); std::vector<uint8_t> all((std::istreambuf_iterator<char>(qf)), std::istreambuf_iterator<char>());
		if (han2::qoh::LooksLike98(all.data(), all.size())) { if (!ch.loadQohFile(path, han2::qoh::V98, *err)) return false; if (fmt) *fmt = "Queen of Heart '98 character (.dat)"; return true; }
	}
	*err = "This file is not a character this editor can load.";
	return false;
}

static bool BuildCharacterPreview(const CharJob &j, int maxSide, Preview &pv)
{
	CharacterInstance ch;
	std::string err, summary;
	switch (j.kind) {
	case CharJob::Han2Stem:
		if (!han2::LoadCharacter(ch, j.stem, j.read, j.origin, &summary, &err)) { pv.text = err; return false; }
		pv.facts.push_back({ "Format", ch.frameData.m_han2 && ch.frameData.m_han2->sub == 2 ? "Glove on Fight 2 (HAN2RBO, PAT + CHP)" : "Ragnarok Battle Offline (HAN2RBO)" });
		break;
	case CharJob::Gof1:
		if (!han2::LoadGof1Character(ch, j.archivePath, j.entry, &err)) { pv.text = err; return false; }
		pv.facts.push_back({ "Format", "Glove on Fight (GOF1 character .DAT)" });
		break;
	case CharJob::Stack:
	case CharJob::Loose: {
		std::string path = j.path;
		if (j.kind == CharJob::Stack) { path = Materialize(j.owner, j.item, &err); if (path.empty()) { pv.text = err; return false; } }
		std::string fmt;
		if (!LoadCharacterFile(ch, path, &fmt, &err)) { pv.text = err; return false; }
		pv.facts.push_back({ "Format", fmt });
		break;
	}
	}
	AddCharFacts(ch, pv);
	int pat = 0, fr = 0;
	std::vector<uint8_t> px; int w = 0, h = 0;
	if (han2::FindIdleFrame(ch.frameData, pat, fr) && han2::RenderFrameThumb(ch.frameData, ch.cg, ch.parts, pat, fr, maxSide, px, w, h)) {
		pv.rgba = std::move(px); pv.w = w; pv.h = h; pv.isCharacter = true;
		pv.facts.push_back({ "Thumbnail", "pattern " + Num(pat) + ", frame " + Num(fr) });
	} else pv.facts.push_back({ "Thumbnail", "no drawable first frame" });
	return true;
}

// Builds the CharJob for an item (UI thread: touches S()).
static bool MakeCharJob(const SourceP &sp, const Item &it, CharJob &j)
{
	const Source &s = *sp;
	uint32_t idx; const Source &o = Owner(s, it, idx);
	const std::string ext = ExtOf(it.name);
	State &st = S();
	if (o.kind == Source::Fb) {
		if (!(ext == ".ha6" || ext == ".dat" || ext == ".chr")) return false;
		SourceP own = sp->kind == Source::Merged && it.member < sp->members.size() ? sp->members[it.member] : sp;
		Item copy = it; copy.index = idx;
		j.kind = CharJob::Stack; j.owner = own; j.item = copy; j.origin = o.label;
		return true;
	}
	if (o.kind == Source::Pac) {
		std::vector<std::shared_ptr<pac::Archive>> order{ o.pac };
		if (auto g = GroupById(o.group)) for (int k = (int)g->archives.size() - 1; k >= 0; k--) if (g->archives[k].get() != &o && g->archives[k]->pac) order.push_back(g->archives[k]->pac);
		j.kind = CharJob::Han2Stem; j.read = han2::PacReader(order); j.stem = StemOf(it.key); j.origin = o.label;
		(void)st; return ext == ".dt2" || ext == ".dat";
	}
	if (o.kind == Source::Gof1) { j.kind = CharJob::Gof1; j.archivePath = o.g1->path; j.entry = it.key; return ext == ".dat"; }
	if (o.kind == Source::Folder) { j.kind = CharJob::Loose; j.path = it.key; return ext == ".ha6" || ext == ".dat" || ext == ".chr"; }
	return false;
}

// ---- preview ---------------------------------------------------------------------------------------------------------------------------------------
static void HexFacts(const std::vector<uint8_t> &d, Preview &pv)
{
	pv.kind = Preview::Hex; pv.bytes.assign(d.begin(), d.begin() + std::min<size_t>(d.size(), 8192));
}

static bool LooksUtf8(const std::vector<uint8_t> &d)
{
	size_t i = 0;
	while (i < d.size()) {
		uint8_t c = d[i];
		int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
		if (n < 0 || i + n >= d.size() + (n == 0 ? 1 : 0)) { if (n < 0) return false; }
		for (int k = 1; k <= n; k++) if (i + k >= d.size() || (d[i + k] & 0xC0) != 0x80) return false;
		i += 1 + n;
	}
	return true;
}

static void DecodePreviewJob(SourceP src, Item item, CharJob cj, bool charOk, uint64_t serial)
{
	auto pv = std::make_shared<Preview>();
	pv->serial = serial; pv->title = item.name; pv->totalSize = item.size;
	pv->facts.push_back({ "Size", Bytes(item.size) });
	const std::string ext = ExtOf(item.name);
	const Type type = item.type;
	std::vector<uint8_t> d; std::string err;
	auto finish = [&] { PostMain([pv] { ApplyPreview(pv); }); };

	if (type == Type::Archive) {
		pv->kind = Preview::Message; pv->text = "Archive. Double-click (or Enter) to mount it and browse its contents."; pv->canOpen = true; finish(); return;
	}
	if ((type == Type::Character || type == Type::CharData) && charOk) {
		pv->canOpen = true;
		if (BuildCharacterPreview(cj, 256, *pv)) {
			if (pv->w > 0) pv->kind = Preview::Image;
			else { pv->kind = Preview::Message; pv->text = "Character loaded, but its first frame draws nothing here."; }
			finish(); return;
		}
		pv->kind = Preview::Message; pv->text = pv->text.empty() ? "This file is not a character this editor can load." : pv->text;
		// fall through to a hex dump below so the user still sees something
		std::string why = pv->text;
		if (!ReadItem(*src, item, d, &err, 8192)) { pv->kind = Preview::Message; pv->text = why + "\n" + err; finish(); return; }
		HexFacts(d, *pv); pv->facts.push_back({ "Character load", why }); finish(); return;
	}
	const size_t kMaxImage = 96u << 20;
	if (type == Type::CgBank) {
		if (item.size > kMaxImage) { pv->kind = Preview::Message; pv->text = "Sprite bank too large to preview."; finish(); return; }
		if (!ReadItem(*src, item, d, &err)) { pv->kind = Preview::Message; pv->text = err; finish(); return; }
		auto bank = std::make_shared<cgm::Bank>();
		if (cgm::Bank::parse(d.data(), d.size(), *bank, &err)) {
			size_t present = 0; for (auto &im : bank->images) if (im.present) present++;
			pv->kind = Preview::Bank; pv->bank = bank; pv->canOpen = true;
			pv->facts.push_back({ "Format", "BMP Cutter sprite bank" });
			pv->facts.push_back({ "Images", Num(present) + " of " + Num(bank->images.size()) });
			pv->facts.push_back({ "Atlas pages", Num((uint64_t)bank->pages()) });
			finish(); return;
		}
		HexFacts(d, *pv); pv->facts.push_back({ "Not a BMP Cutter bank", err }); finish(); return;
	}
	if (type == Type::Image) {
		if (item.size > kMaxImage) { pv->kind = Preview::Message; pv->text = "Image too large to preview (" + Bytes(item.size) + ")."; finish(); return; }
		if (!ReadItem(*src, item, d, &err)) { pv->kind = Preview::Message; pv->text = err; finish(); return; }
		std::string how; std::vector<uint8_t> rgba; int w = 0, h = 0; std::vector<uint32_t> sw;
		if (DecodeImageBytes(item.name, d, rgba, w, h, how, &sw)) {
			pv->kind = Preview::Image; pv->w = w; pv->h = h; pv->rgba = std::move(rgba); pv->swatches = std::move(sw); pv->canOpen = true;
			pv->facts.push_back({ "Format", how }); pv->facts.push_back({ "Dimensions", Num(w) + " x " + Num(h) });
			finish(); return;
		}
		HexFacts(d, *pv); pv->facts.push_back({ "Image", "unrecognised image layout (hex shown)" }); finish(); return;
	}
	// everything else: read a prefix and pick text / info / hex
	const size_t cap = type == Type::Text ? (512u << 10) : 65536;
	if (!ReadItem(*src, item, d, &err, cap)) { pv->kind = Preview::Message; pv->text = err; finish(); return; }
	pv->canOpen = true;
	if (type == Type::Parts && han2::IsPat(d.data(), d.size())) pv->facts.push_back({ "Format", "French Bread PAT parts file (poses made of cut-out parts)" });
	if (type == Type::Audio) {
		if (d.size() > 12 && memcmp(d.data(), "RIFF", 4) == 0) {
			han2::Riff r; if (han2::ParseRiff(d.data(), d.size(), r, nullptr)) { char b[96]; snprintf(b, sizeof b, "WAVE, %u Hz, %u ch, %u-bit", r.sampleRate, r.channels, r.bits); pv->facts.push_back({ "Format", b }); }
		}
	}
	if (d.size() >= 8 && memcmp(d.data(), "HAN2RBO ", 8) == 0) pv->facts.push_back({ "Format", "HAN2RBO container (character data)" });
	if (type == Type::Text || (han2::LooksLikeText(d.data(), d.size()) && ext != ".dat" && ext != ".ct")) {
		pv->kind = Preview::Text;
		const std::string raw(d.begin(), d.end());
		pv->text = LooksUtf8(d) ? raw : sj2utf8(raw);
		pv->facts.push_back({ "Encoding", LooksUtf8(d) ? "UTF-8" : "Shift-JIS (CP932)" });
		size_t lines = std::count(raw.begin(), raw.end(), '\n') + 1; pv->facts.push_back({ "Lines", Num(lines) + (item.size > cap ? "+ (first part shown)" : "") });
		finish(); return;
	}
	HexFacts(d, *pv);
	finish();
}

void RequestPreview(int sourceIdx, int itemIdx)
{
	State &st = S();
	SourceP s = FindSource(sourceIdx);
	if (!s || s->state != Source::Ready || itemIdx < 0 || itemIdx >= (int)s->items.size()) return;
	const Item item = s->items[itemIdx];
	st.prevSource = sourceIdx; st.prevItem = itemIdx;
	st.prev = Preview(); st.prev.kind = Preview::Loading; st.prev.title = item.name; st.prev.serial = ++st.prevSerial;
	st.bankFrame = -1; st.bankTexFrame = -2; st.view2.fit = true;
	CharJob cj; bool charOk = false;
	if (item.type == Type::Character || item.type == Type::CharData) charOk = MakeCharJob(s, item, cj);
	const uint64_t serial = st.prevSerial;
	PostJob([s, item, cj, charOk, serial] { DecodePreviewJob(s, item, cj, charOk, serial); });
}

SourceP FindSource(int idx) { State &st = S(); return idx >= 0 && idx < (int)st.sources.size() ? st.sources[idx] : nullptr; }

// ---- thumbnails -------------------------------------------------------------------------------------------------------------------------------------
std::string ThumbKey(const Source &s, const Item &it)
{
	uint32_t idx; const Source &o = Owner(s, it, idx);
	return o.path + "|" + std::to_string(idx) + "|" + it.name;
}

static void ThumbJob(SourceP src, Item item, CharJob cj, bool charOk, std::string key)
{
	auto fail = [&] { PostMain([key] { auto &c = S().thumbCache[key]; c.state = 3; S().thumbsPending = std::max(0, S().thumbsPending - 1); }); };
	if (src->closed) { fail(); return; }
	std::vector<uint8_t> px; int w = 0, h = 0;
	std::string err;
	if ((item.type == Type::Character || item.type == Type::CharData) && charOk) {
		Preview pv;
		if (!BuildCharacterPreview(cj, 96, pv) || pv.w <= 0) { fail(); return; }
		px = std::move(pv.rgba); w = pv.w; h = pv.h;
	} else if (item.type == Type::Image) {
		if (item.size > (48u << 20)) { fail(); return; }
		std::vector<uint8_t> d; std::vector<uint8_t> rgba; int sw = 0, sh = 0; std::string how;
		if (!ReadItem(*src, item, d, &err) || !DecodeImageBytes(item.name, d, rgba, sw, sh, how, nullptr)) { fail(); return; }
		Downscale(rgba, sw, sh, 96, px, w, h);
	} else if (item.type == Type::CgBank) {
		if (item.size > (48u << 20)) { fail(); return; }
		std::vector<uint8_t> d; cgm::Bank bank;
		if (!ReadItem(*src, item, d, &err) || !cgm::Bank::parse(d.data(), d.size(), bank, &err)) { fail(); return; }
		bool done = false;
		for (size_t n = 0; n < bank.images.size() && !done; n++) {
			if (!bank.images[n].drawable()) continue;
			cgm::Rgba r;
			if (bank.decode((int)n, r) && r.w > 0) { Downscale(r.px, r.w, r.h, 96, px, w, h); done = true; }
		}
		if (!done) { fail(); return; }
	} else { fail(); return; }
	auto data = std::make_shared<std::vector<uint8_t>>(std::move(px));
	PostMain([key, data, w, h] { ApplyThumb(key, data, w, h); });
}

void RequestThumb(int sourceIdx, int itemIdx)
{
	State &st = S();
	SourceP s = FindSource(sourceIdx);
	if (!s || itemIdx < 0 || itemIdx >= (int)s->items.size()) return;
	const Item item = s->items[itemIdx];
	const std::string key = ThumbKey(*s, item);
	Thumb &t = st.thumbCache[key];
	if (t.state != 0) return;
	if (!(item.type == Type::Character || item.type == Type::CharData || item.type == Type::Image || item.type == Type::CgBank)) { t.state = 3; return; }
	t.state = 1; st.thumbsPending++;
	CharJob cj; bool charOk = false;
	if (item.type == Type::Character || item.type == Type::CharData) charOk = MakeCharJob(s, item, cj);
	PostJob([s, item, cj, charOk, key] { ThumbJob(s, item, cj, charOk, key); }, true);
}

// ---- opening ----------------------------------------------------------------------------------------------------------------------------------------
static std::string ManagedCopy(const Source &o, const Item &it, std::string *err)
{
	std::vector<uint8_t> b;
	if (!ReadItem(o, it, b, err)) return {};
	std::error_code ec;
	const fs::path dir = fs::temp_directory_path(ec) / "hantei_archive" / P8(o.label);
	fs::create_directories(dir, ec);
	const fs::path out = dir / P8(it.name);
	fs::create_directories(out.parent_path(), ec);
	std::ofstream f(out, std::ios::binary);
	if (!b.empty()) f.write((const char *)b.data(), (std::streamsize)b.size());
	if (!f) { if (err) *err = "could not write " + out.u8string(); return {}; }
	f.close();
	// remember the origin so Save writes back into the archive (fbarc understands every archive kind of this browser)
	if (!(o.fb && (o.fb->kind() == fbarc::Kind::Uni2D || o.fb->kind() == fbarc::Kind::MbtlBin))) fbarc::SetOrigin(out.u8string(), fbarc::Origin{ o.path, it.name });   // those archives are read-only: Save keeps the copy
	return out.u8string();
}

bool BuildOpenRequest(int sourceIdx, int itemIdx, OpenRequest &req, std::string &msg)
{
	State &st = S();
	SourceP s = FindSource(sourceIdx);
	if (!s || itemIdx < 0 || itemIdx >= (int)s->items.size()) return false;
	const Item &it = s->items[itemIdx];
	uint32_t idx; const Source &o = Owner(*s, it, idx);
	if (it.type == Type::Archive) {
		if (o.kind == Source::Folder) { MountArchive(it.key, o.group, true); }
		return false;
	}
	if (o.kind == Source::Folder) { req.kind = OpenRequest::LooseFile; req.path = it.key; return true; }
	const std::string ext = ExtOf(it.name);
	if ((it.type == Type::Character || it.type == Type::CharData) && o.kind == Source::Pac && (ext == ".dt2" || ext == ".dat")) {
		CharJob cj; MakeCharJob(s, it, cj);
		req.kind = OpenRequest::Han2Stem; req.stem = cj.stem; req.read = cj.read; req.origin = o.label; req.archivePath = o.path;
		return true;
	}
	if (it.type == Type::Character && o.kind == Source::Gof1 && ext == ".dat") {
		req.kind = OpenRequest::Gof1Entry; req.archivePath = o.path; req.entryName = it.key; req.origin = o.label;
		return true;
	}
	(void)st;
	std::string err;
	if (o.kind == Source::Fb && o.fb && it.type == Type::Character && ExtOf(it.name) == ".ha6") {   // a character stack: unpack it as a working copy and open its txt
		SourceP own = s->kind == Source::Merged && it.member < s->members.size() ? s->members[it.member] : s;
		Item copy = it; copy.index = idx;
		const std::string tp = Materialize(own, copy, &err);
		if (tp.empty()) { msg = err.empty() ? "could not unpack the character" : err; return false; }
		req.kind = OpenRequest::LooseFile; req.path = tp; req.origin = o.label;
		msg = "Working copy of " + it.name + " (the archive is read-only here)";
		return true;
	}
	const std::string p = ManagedCopy(o, it, &err);
	if (p.empty()) { msg = err.empty() ? "could not read the entry" : err; return false; }
	req.kind = OpenRequest::LooseFile; req.path = p; req.archivePath = o.path; req.entryName = it.key; req.origin = o.label;
	return true;
}

// ---- extraction -------------------------------------------------------------------------------------------------------------------------------------
void StartExtract(int sourceIdx, std::vector<uint32_t> items, const std::string &dir)
{
	State &st = S();
	SourceP s = FindSource(sourceIdx);
	if (!s || st.prog.running) return;
	if (items.empty()) for (uint32_t i = 0; i < s->items.size(); i++) items.push_back(i);
	st.prog.done = 0; st.prog.failed = 0; st.prog.total = (int)items.size(); st.prog.running = true; st.prog.cancel = false; st.prog.dest = dir; st.prog.what = s->label;
	PostJob([s, items, dir] {
		State &st = S();
		for (uint32_t i : items) {
			if (st.prog.cancel || s->closed) break;
			if (i >= s->items.size()) { st.prog.failed++; st.prog.done++; continue; }
			const Item &it = s->items[i];
			std::vector<uint8_t> b; std::string err;
			bool ok = ReadItem(*s, it, b, &err);
			if (ok) {
				std::error_code ec;
				// flatten ".." and absolute prefixes: an entry name never escapes the chosen folder
				std::string rel = it.name; for (auto &c : rel) if (c == '\\') c = '/';
				while (rel.find("../") != std::string::npos) rel.erase(rel.find("../"), 3);
				while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
				const fs::path out = P8(dir) / P8(rel);
				fs::create_directories(out.parent_path(), ec);
				std::ofstream f(out, std::ios::binary);
				if (!b.empty()) f.write((const char *)b.data(), (std::streamsize)b.size());
				ok = (bool)f;
			}
			if (!ok) st.prog.failed++;
			st.prog.done++;
		}
		PostMain([] { S().prog.running = false; });
	});
}

// ---- write-back -------------------------------------------------------------------------------------------------------------------------------------
static bool SwapIn(const std::string &archive, const std::string &rebuilt, std::string *err)
{
	std::error_code ec;
	const fs::path a = P8(archive);
	fs::path bak = a; bak += ".bak";
	if (!fs::exists(bak, ec)) {   // the first .bak is the pristine archive: later saves never overwrite it
		fs::copy_file(a, bak, ec);
		if (ec) { if (err) *err = "could not write " + bak.u8string() + ": " + ec.message(); return false; }
	}
	if (!MoveFileExW(P8(rebuilt).wstring().c_str(), a.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		if (err) *err = "could not replace " + archive + " (is the game running?)";
		return false;
	}
	return true;
}

bool ReplaceEntries(const std::string &archivePath, const std::vector<EntryEdit> &edits, std::string *err)
{
	std::string e;
	auto fa = fbarc::Open(archivePath, &e);
	if (!fa) { if (err) *err = e.empty() ? "cannot open " + archivePath : e; return false; }
	fbarc::Edit ed;
	for (auto &x : edits) {
		const int i = fa->find(x.name);
		if (i >= 0) ed.replace[(size_t)i] = x.data; else ed.add.emplace_back(x.name, x.data);
	}
	const std::string tmp = archivePath + ".hantei-new";
	if (!fa->rebuild(tmp, ed, &e)) { if (err) *err = e; std::error_code ec; fs::remove(P8(tmp), ec); return false; }
	fa.reset();
	if (!SwapIn(archivePath, tmp, err)) { std::error_code ec; fs::remove(P8(tmp), ec); return false; }
	ReloadArchive(archivePath);
	return true;
}

bool SwapRebuiltArchive(const std::string &archivePath, const std::string &rebuiltPath, std::string *err)
{
	if (!SwapIn(archivePath, rebuiltPath, err)) return false;
	ReloadArchive(archivePath);
	return true;
}

bool SaveCharacterIntoArchive(CharacterInstance &ch, const std::string &archivePath, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (!ch.frameData.isHan2() || !ch.frameData.m_han2) return fail("not a French Bread character");
	ch.undoManager.flush();
	std::string perr;
	if (!han2::SyncPartsToContainer(ch, nullptr, &perr)) return fail(perr);
	auto cont = ch.frameData.m_han2;
	// GOF1: the source archive is the one the character came from; its own writer produces a new archive with the entry replaced
	if (!cont->gof1Name.empty()) {
		const std::string tmp = archivePath + ".hantei-new.p";
		if (!ch.frameData.save(tmp.c_str())) { std::error_code ec; fs::remove(P8(tmp), ec); return fail(gof1::LastSaveError().empty() ? "could not write the GOF1 archive" : gof1::LastSaveError()); }
		if (!SwapIn(archivePath, tmp, err)) { std::error_code ec; fs::remove(P8(tmp), ec); return false; }
		ReloadArchive(archivePath);
		return true;
	}
	std::error_code ec;
	const fs::path dir = fs::temp_directory_path(ec) / ("hantei_save_" + std::to_string(GetCurrentProcessId()));
	fs::remove_all(dir, ec); fs::create_directories(dir, ec);
	const std::string srcExt = ExtOf(cont->sourcePath);
	const std::string stem = StemOf(cont->sourcePath);
	const fs::path out = dir / P8(stem + (srcExt.empty() ? ".DT2" : std::string(srcExt == ".dt2" ? ".DT2" : ".DAT")));
	if (!ch.frameData.save(out.u8string().c_str())) return fail(han2::LastSaveError().empty() ? "could not serialise the character" : han2::LastSaveError());
	std::string cerr2;
	han2::SaveGof2Companions(ch, out.u8string(), &cerr2);
	std::vector<EntryEdit> edits;
	for (auto &e : fs::directory_iterator(dir, ec)) {
		if (!e.is_regular_file(ec)) continue;
		if (Lower(e.path().extension().string()) == ".bak") continue;
		std::ifstream f(e.path(), std::ios::binary);
		EntryEdit x; x.name = utf82sj(e.path().filename().u8string());
		x.data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		edits.push_back(std::move(x));
	}
	fs::remove_all(dir, ec);
	if (edits.empty()) return fail("nothing to write");
	if (!ReplaceEntries(archivePath, edits, err)) return false;
	return true;
}

bool SaveEntryBytesIntoArchive(const std::string &loosePath, std::string *err, bool *hadOrigin)
{
	fbarc::Origin o;
	if (hadOrigin) *hadOrigin = false;
	if (!fbarc::GetOrigin(loosePath, &o)) return true;
	if (hadOrigin) *hadOrigin = true;
	std::ifstream f(P8(loosePath), std::ios::binary);
	if (!f) { if (err) *err = "cannot read " + loosePath; return false; }
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return ReplaceEntries(o.archive, { EntryEdit{ fbarc::NameFromUtf8(o.entry), std::move(b) } }, err);
}

} // namespace abrowser
