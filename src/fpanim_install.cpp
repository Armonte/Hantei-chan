// fpan-im install / update / uninstall: see fpanim_install.h.
#include "fpanim_install.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include "app_log.h"
#endif

namespace fpaninst {

namespace {

std::string Lower(std::string s)
{
	for (char& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}
std::string Slashes(std::string s)
{
	std::replace(s.begin(), s.end(), '\\', '/');
	while (s.size() > 1 && s.back() == '/') s.pop_back();
	return s;
}
std::string Trim(const std::string& s)
{
	size_t a = 0, b = s.size();
	while (a < b && std::isspace((unsigned char)s[a])) ++a;
	while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
	return s.substr(a, b - a);
}

constexpr const char* kShim = "winmm.dll";
constexpr const char* kChain = "winmm.chain.dll";
constexpr const char* kCore = "fpan-im/fpan-im.dll";
constexpr const char* kRecord = "fpan-im/install.txt";

} // namespace

bool IsOurShim(const FileInfo& w)
{
	if (!w.exists) return false;
	if (w.hasVersion && w.productName == "fpan-im") return true;
	return w.hasMarker;
}

std::string VersionLabel(const FileInfo& f)
{
	if (!f.exists) return "";
	if (f.hasVersion && !f.version.empty()) return f.version;
	return "unknown version";
}

Status Classify(const GameState& g)
{
	const bool shim = IsOurShim(g.shim);
	if (shim && g.core.exists) return Status::Installed;
	if (!shim && !g.core.exists) return Status::NotInstalled;
	return Status::Partial;
}

Bundle ResolveBundle(const std::string& dir, const std::function<bool(const std::string&)>& exists)
{
	Bundle b;
	if (dir.empty()) return b;
	const std::string d = Slashes(dir);
	const std::string w = d + "/winmm.dll";
	if (!exists(w)) return b;
	for (const std::string& c : { d + "/fpan-im/fpan-im.dll", d + "/fpan-im.dll" }) {
		if (exists(c)) { b.found = true; b.winmm = w; b.core = c; b.dir = d; return b; }
	}
	return b;
}

Plan PlanInstall(const GameState& g, const Bundle& b, const std::string& timestamp)
{
	Plan p;
	if (!b.found) { p.refused = "fpan-im not bundled"; return p; }
	if (g.dir.empty()) { p.refused = "no game folder"; return p; }
	std::string backup = g.recordedBackup;
	if (g.shim.exists && !IsOurShim(g.shim)) {
		// A foreign winmm.dll (another loader or mod): keep it running behind our shim.
		if (!g.chainExists) {
			p.ops.push_back({ Op::Move, kShim, kChain, "" });
			backup = kChain;
			p.notes.push_back("existing winmm.dll is not fpan-im: moved to winmm.chain.dll (fpan-im's shim chainloads it)");
		} else {
			const std::string bak = std::string("winmm.dll.bak-") + timestamp;
			p.ops.push_back({ Op::Move, kShim, bak, "" });
			p.notes.push_back("existing winmm.dll is not fpan-im and winmm.chain.dll already exists (kept, still chainloaded): "
			                  "the old winmm.dll was saved as " + bak);
		}
	} else if (g.chainExists && backup.empty()) {
		p.notes.push_back("winmm.chain.dll is present: fpan-im's shim will chainload it");
	}
	p.ops.push_back({ Op::MkDir, "", "fpan-im", "" });
	p.ops.push_back({ Op::Copy, b.core, kCore, "" });
	p.ops.push_back({ Op::Copy, b.winmm, kShim, "" });
	p.ops.push_back({ Op::WriteText, "", kRecord, backup.empty() ? std::string("# fpan-im install record\n")
	                                                              : "# fpan-im install record\nbackup=" + backup + "\n" });
	return p;
}

Plan PlanUninstall(const GameState& g, const std::vector<std::string>& pdbs)
{
	Plan p;
	if (g.dir.empty()) { p.refused = "no game folder"; return p; }
	const bool shimOurs = IsOurShim(g.shim);
	if (!shimOurs && !g.core.exists) { p.refused = "fpan-im is not installed"; return p; }
	if (shimOurs) p.ops.push_back({ Op::Remove, "", kShim, "" });
	else if (g.shim.exists) p.notes.push_back("winmm.dll is not fpan-im's: left in place");
	if (g.core.exists) p.ops.push_back({ Op::Remove, "", kCore, "" });
	for (const std::string& pdb : pdbs) p.ops.push_back({ Op::Remove, "", "fpan-im/" + pdb, "" });
	p.ops.push_back({ Op::Remove, "", kRecord, "" });
	if (g.recordedBackup == kChain && g.chainExists && (shimOurs || !g.shim.exists)) {
		p.ops.push_back({ Op::Move, kChain, kShim, "" });
		p.notes.push_back("restored the original winmm.dll from winmm.chain.dll");
	} else if (g.chainExists) {
		p.notes.push_back("winmm.chain.dll left in place (not made by this install)");
	}
	p.notes.push_back("fpan-im\\mods, fpan-im.ini and logs were kept");
	return p;
}

bool PlanTouchesOnlyOurFiles(const Plan& p)
{
	auto ours = [](const std::string& raw) {
		const std::string s = Lower(Slashes(raw));
		if (s.empty() || s.find("..") != std::string::npos || s[0] == '/' || s.find(':') != std::string::npos) return false;
		if (s == "winmm.dll" || s == "winmm.chain.dll" || s == "fpan-im" || s == "fpan-im/fpan-im.dll" || s == "fpan-im/install.txt")
			return true;
		if (s.rfind("winmm.dll.bak-", 0) == 0 && s.find('/') == std::string::npos) return true;
		if (s.rfind("fpan-im/", 0) == 0 && s.size() > 12 && s.compare(s.size() - 4, 4, ".pdb") == 0 &&
		    s.find('/', 8) == std::string::npos) return true;
		return false;
	};
	for (const Op& o : p.ops) {
		if (!ours(o.dst)) return false;
		if (o.type == Op::Move && !ours(o.src)) return false;
	}
	return true;
}

std::string ParseInstallRecord(const std::string& text)
{
	std::string out;
	size_t i = 0;
	while (i <= text.size()) {
		size_t e = text.find('\n', i);
		if (e == std::string::npos) e = text.size();
		const std::string line = Trim(text.substr(i, e - i));
		if (line.rfind("backup=", 0) == 0) out = Trim(line.substr(7));
		i = e + 1;
	}
	return out;
}

bool ImageIsInDir(const std::string& imagePath, const std::string& dir)
{
	const std::string img = Lower(Slashes(imagePath)), d = Lower(Slashes(dir));
	const size_t s = img.find_last_of('/');
	if (s == std::string::npos || d.empty()) return false;
	const std::string name = img.substr(s + 1);
	const auto& names = GameExeNames();
	if (std::find(names.begin(), names.end(), name) == names.end()) return false;
	return img.substr(0, s) == d;
}

const std::vector<std::string>& GameExeNames()
{
	static const std::vector<std::string> n = { "uni2.exe", "mbtl.exe" };
	return n;
}

#ifdef _WIN32

namespace {

std::wstring W(const std::string& u8)
{
	if (u8.empty()) return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), w.data(), n);
	return w;
}
std::string U8(const std::wstring& w)
{
	if (w.empty()) return {};
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}
std::filesystem::path P(const std::string& dir, const std::string& rel = {})
{
	std::filesystem::path p(W(dir));
	if (!rel.empty()) p /= std::filesystem::path(W(rel));
	return p.make_preferred();
}

bool FileHasMarker(const std::wstring& path)
{
	std::ifstream f(std::filesystem::path(path), std::ios::binary);
	if (!f) return false;
	std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (data.size() > 64u << 20) return false;
	for (const char* m : { "fpan-im", "fbmod\\fbmod.dll" }) {
		if (data.find(m) != std::string::npos) return true;
		std::string w16;
		for (const char* c = m; *c; ++c) { w16 += *c; w16 += '\0'; }
		if (data.find(w16) != std::string::npos) return true;
	}
	return false;
}

std::string QueryString(const std::vector<char>& blk, const wchar_t* lang, const wchar_t* key)
{
	std::wstring q = std::wstring(L"\\StringFileInfo\\") + lang + L"\\" + key;
	wchar_t* v = nullptr;
	UINT len = 0;
	if (!VerQueryValueW(blk.data(), q.c_str(), (void**)&v, &len) || !v || !len) return {};
	return U8(std::wstring(v, wcsnlen(v, len)));
}

} // namespace

FileInfo ReadFileInfo(const std::string& utf8Path)
{
	FileInfo fi;
	const std::wstring p = W(utf8Path);
	const DWORD attr = GetFileAttributesW(p.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return fi;
	fi.exists = true;
	DWORD h = 0;
	const DWORD sz = GetFileVersionInfoSizeW(p.c_str(), &h);
	if (sz) {
		std::vector<char> blk(sz);
		if (GetFileVersionInfoW(p.c_str(), 0, sz, blk.data())) {
			// The first translation names the StringFileInfo block; fpan-im writes 040904b0.
			struct Tr { WORD lang, cp; }* tr = nullptr;
			UINT n = 0;
			wchar_t lang[16] = L"040904b0";
			if (VerQueryValueW(blk.data(), L"\\VarFileInfo\\Translation", (void**)&tr, &n) && tr && n >= sizeof(Tr))
				swprintf(lang, 16, L"%04x%04x", tr->lang, tr->cp);
			fi.productName = QueryString(blk, lang, L"ProductName");
			fi.version = QueryString(blk, lang, L"FileVersion");
			fi.component = QueryString(blk, lang, L"FpanimComponent");
			fi.hasVersion = !fi.productName.empty() || !fi.version.empty();
		}
	}
	if (!(fi.hasVersion && fi.productName == "fpan-im")) fi.hasMarker = FileHasMarker(p);
	else fi.hasMarker = true;
	return fi;
}

GameState ReadGameState(const std::string& dir)
{
	GameState g;
	g.dir = dir;
	if (dir.empty()) return g;
	g.shim = ReadFileInfo(U8(P(dir, kShim).wstring()));
	g.core = ReadFileInfo(U8(P(dir, kCore).wstring()));
	std::error_code ec;
	g.chainExists = std::filesystem::is_regular_file(P(dir, kChain), ec);
	std::ifstream f(P(dir, kRecord), std::ios::binary);
	if (f) {
		std::stringstream ss;
		ss << f.rdbuf();
		g.recordedBackup = ParseInstallRecord(ss.str());
	}
	return g;
}

uint32_t FindRunningGame(const std::string& dir)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return 0;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof pe;
	uint32_t found = 0;
	for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
		const std::string exe = Lower(U8(pe.szExeFile));
		const auto& names = GameExeNames();
		if (std::find(names.begin(), names.end(), exe) == names.end()) continue;
		HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
		if (!h) { found = pe.th32ProcessID; break; }   // can't read its path: assume it is this game (refuse, safe side)
		wchar_t buf[MAX_PATH * 2];
		DWORD n = MAX_PATH * 2;
		if (QueryFullProcessImageNameW(h, 0, buf, &n) && ImageIsInDir(U8(std::wstring(buf, n)), dir)) found = pe.th32ProcessID;
		CloseHandle(h);
	}
	CloseHandle(snap);
	return found;
}

std::string DefaultBundleDir()
{
	wchar_t buf[MAX_PATH * 2];
	const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
	std::filesystem::path exe(std::wstring(buf, n));
	return U8((exe.parent_path() / L"res" / L"fpan-im").wstring());
}

std::string Execute(const std::string& dir, const Plan& p)
{
	if (!p.refused.empty()) return p.refused;
	if (!PlanTouchesOnlyOurFiles(p)) return "internal: plan touches a file that is not fpan-im's";
	namespace fs = std::filesystem;
	for (const Op& o : p.ops) {
		std::error_code ec;
		const fs::path dst = P(dir, o.dst);
		switch (o.type) {
		case Op::MkDir:
			fs::create_directories(dst, ec);
			if (ec) return "create " + o.dst + ": " + ec.message();
			break;
		case Op::Copy: {
			// Copy beside the target, then replace it, so a failed copy never leaves half a DLL.
			fs::path tmp = dst;
			tmp += L".new";
			fs::copy_file(P(o.src), tmp, fs::copy_options::overwrite_existing, ec);
			if (ec) return "copy " + o.src + ": " + ec.message();
			if (!MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING)) {
				fs::remove(tmp, ec);
				return "replace " + o.dst + ": error " + std::to_string(GetLastError()) + " (is the game running?)";
			}
			break;
		}
		case Op::Move:
			if (!MoveFileExW(P(dir, o.src).c_str(), dst.c_str(), 0))
				return "move " + o.src + " -> " + o.dst + ": error " + std::to_string(GetLastError());
			break;
		case Op::Remove:
			fs::remove(dst, ec);
			if (ec) return "remove " + o.dst + ": " + ec.message();
			break;
		case Op::WriteText: {
			std::ofstream f(dst, std::ios::binary | std::ios::trunc);
			f << o.text;
			if (!f) return "write " + o.dst;
			break;
		}
		}
		HC_LOG_INFO("[fpan-im] {} {}{}{}", (int)o.type, o.src, o.src.empty() ? "" : " -> ", o.dst);
	}
	return "";
}

std::string NowStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);
	char b[32];
	std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", &tm);
	return b;
}

#endif

} // namespace fpaninst
