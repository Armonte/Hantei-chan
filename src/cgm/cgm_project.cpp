#include "cgm_project.h"
#include "cgm_usage.h"
#include "../framedata.h"
#include "../png_writer.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <cstring>

namespace cgm {
namespace fs = std::filesystem;

static std::string Cp932ToUtf8(const std::string &s) {
	if (s.empty()) return s;
	const int n = MultiByteToWideChar(932, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0'); if (n > 0) MultiByteToWideChar(932, 0, s.data(), (int)s.size(), w.data(), n);
	return WideToUtf8(w);
}
static std::string Utf8ToAnsi(const std::string &s) {
	const std::wstring w = Utf8ToWide(s);
	const int n = WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string o(n > 0 ? n : 0, '\0'); if (n > 0) WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), o.data(), n, nullptr, nullptr);
	return o;
}
static std::string Trim(std::string s) {
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
	size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
	return s.substr(i);
}
static std::string Lower(std::string s) { for (char &c : s) c = (char)tolower((unsigned char)c); return s; }

std::string NormPath(const std::string &p) {
	std::string s = p; std::replace(s.begin(), s.end(), '\\', '/');
	std::vector<std::string> parts; std::string cur, lead;
	if (s.size() >= 2 && s[1] == ':') { lead = Lower(s.substr(0, 2)); s = s.substr(2); }
	const bool abs = !s.empty() && s[0] == '/';
	for (size_t i = 0; i <= s.size(); i++) {
		if (i == s.size() || s[i] == '/') {
			if (cur == "..") { if (!parts.empty() && parts.back() != "..") parts.pop_back(); else if (!abs) parts.push_back(cur); }
			else if (!cur.empty() && cur != ".") parts.push_back(Lower(cur));
			cur.clear();
		} else cur += s[i];
	}
	std::string o = lead + (abs ? "/" : "");
	for (size_t i = 0; i < parts.size(); i++) { if (i) o += "/"; o += parts[i]; }
	return o;
}

static std::string DirOf(const std::string &p) { const size_t k = p.find_last_of("\\/"); return k == std::string::npos ? std::string(".") : p.substr(0, k); }
static std::string Join(const std::string &dir, const std::string &rel) {
	if (rel.size() >= 2 && (rel[1] == ':' || rel[0] == '\\' || rel[0] == '/')) return rel;
	return dir + "/" + rel;
}

bool ParseProjectTxt(const std::string &path, ProjectTxt &out) {
	out = ProjectTxt(); out.path = path;
	std::ifstream f(fs::path(Utf8ToWide(path)), std::ios::binary); if (!f) return false;
	std::map<std::string, std::map<std::string, std::string>> ini; std::string sec, line;
	int lines = 0;
	while (std::getline(f, line) && lines++ < 4000) {
		line = Trim(line);
		if (line.empty() || line[0] == ';') continue;
		if (line[0] == '[') { const size_t e = line.find(']'); if (e == std::string::npos) continue; sec = Lower(Trim(line.substr(1, e - 1))); continue; }
		const size_t eq = line.find('='); if (eq == std::string::npos || sec.empty()) continue;
		const std::string k = Lower(Trim(line.substr(0, eq))); if (!ini[sec].count(k)) ini[sec][k] = Trim(line.substr(eq + 1));   // first one wins, like GetPrivateProfileString
	}
	auto it = ini.find("datafile"); if (it == ini.end()) return false;
	const auto &df = it->second;
	const int num = df.count("filenum") ? atoi(df.at("filenum").c_str()) : 0;
	if (num > 0 && num < 1000) {
		const int width = num >= 100 ? 3 : 2;
		for (int i = 0; i < num; i++) { char k[16]; snprintf(k, sizeof(k), "file%0*d", width, i); auto e = df.find(k); out.dataFiles.push_back(e == df.end() ? std::string() : Cp932ToUtf8(e->second)); }
	} else if (num == 0 && df.count("file")) out.dataFiles.push_back(Cp932ToUtf8(df.at("file")));
	if (out.dataFiles.empty()) return false;
	auto bc = ini.find("bmpcutfile");
	if (bc != ini.end() && bc->second.count("file00") && atoi(bc->second.count("filenum") ? bc->second.at("filenum").c_str() : "0") >= 1) out.bank = Cp932ToUtf8(bc->second.at("file00"));
	auto pa = ini.find("panifile");
	if (pa != ini.end() && pa->second.count("file00") && atoi(pa->second.count("filenum") ? pa->second.at("filenum").c_str() : "0") >= 1) out.pat = Cp932ToUtf8(pa->second.at("file00"));
	const std::string dir = DirOf(path);
	if (!out.pat.empty()) out.patPath = Join(dir, out.pat);
	for (const std::string &d : out.dataFiles) out.dataPaths.push_back(d.empty() ? std::string() : Join(dir, d));
	if (!out.bank.empty()) out.bankPath = Join(dir, out.bank);
	return true;
}

int BankUsers::fileIndex(const std::string &p) const {
	const std::string n = NormPath(p);
	for (size_t i = 0; i < files.size(); i++) if (NormPath(files[i].path) == n) return (int)i;
	return -1;
}

BankUsers FindBankUsers(const std::string &bankPath) {
	BankUsers u; u.bankPath = bankPath;
	const std::string bankN = NormPath(bankPath), dir = DirOf(bankPath), dirN = NormPath(dir);
	std::vector<ProjectTxt> all;
	std::error_code ec;
	for (auto &e : fs::directory_iterator(fs::path(Utf8ToWide(dir)), ec)) {
		if (!e.is_regular_file(ec)) continue;
		std::string ext = Lower(e.path().extension().string()); if (ext != ".txt") continue;
		ProjectTxt t; if (ParseProjectTxt(WideToUtf8(e.path().wstring()), t) && !t.bankPath.empty()) all.push_back(std::move(t));
	}
	std::sort(all.begin(), all.end(), [](const ProjectTxt &a, const ProjectTxt &b) { return NormPath(a.path) < NormPath(b.path); });
	std::set<std::string> otherBankFiles;
	for (const ProjectTxt &t : all) {
		if (NormPath(t.bankPath) == bankN) continue;
		for (const std::string &p : t.dataPaths) if (!p.empty()) otherBankFiles.insert(NormPath(p));
	}
	for (const ProjectTxt &t : all) {
		if (NormPath(t.bankPath) != bankN) continue;
		const int pi = (int)u.projects.size(); u.projects.push_back(t);
		for (const std::string &p : t.dataPaths) {
			if (p.empty()) continue;
			int fi = u.fileIndex(p);
			if (fi < 0) {
				BankUserFile f; f.path = p; const std::string n = NormPath(p);
				const std::string fdir = n.substr(0, n.find_last_of('/'));
				f.shared = otherBankFiles.count(n) || fdir != dirN || n.find("basedata") != std::string::npos;
				fi = (int)u.files.size(); u.files.push_back(f);
			}
			if (std::find(u.files[fi].projects.begin(), u.files[fi].projects.end(), pi) == u.files[fi].projects.end()) u.files[fi].projects.push_back(pi);
		}
	}
	// HA6 files no project lists but named after the bank (UNIB <chr>_s.HA6): the game loads them with this bank
	{
		std::string stem = Lower(fs::path(Utf8ToWide(bankPath)).stem().string());
		for (auto &e : fs::directory_iterator(fs::path(Utf8ToWide(dir)), ec)) {
			if (!e.is_regular_file(ec) || Lower(e.path().extension().string()) != ".ha6") continue;
			const std::string p = WideToUtf8(e.path().wstring()), n = NormPath(p), fstem = Lower(e.path().stem().string());
			if (u.fileIndex(p) >= 0 || otherBankFiles.count(n) || u.projects.empty()) continue;
			if (fstem.compare(0, stem.size() + 1, stem + "_") != 0) continue;
			bool claimed = false;   // a longer bank name of the folder owns it (akiha_x vs akiha_something.cg)
			for (const ProjectTxt &t : all) { const std::string bs = Lower(fs::path(Utf8ToWide(t.bankPath)).stem().string()); if (bs.size() > stem.size() && fstem.compare(0, bs.size(), bs) == 0) claimed = true; }
			if (claimed) continue;
			BankUserFile f; f.path = p; f.unlisted = true; u.files.push_back(f);
		}
	}
	return u;
}

static void CountFile(FrameData &fd, std::vector<int> &v) {
	const int n = fd.get_sequence_count();
	for (int p = 0; p < n; p++) {
		Sequence *s = fd.get_sequence(p); if (!s) continue;
		for (auto &fr : s->frames) for (auto &L : fr.AF.layers) {
			if (L.usePat || L.spriteId < 0) continue;
			if (L.spriteId >= (int)v.size()) v.resize((size_t)L.spriteId + 1, 0);
			v[L.spriteId]++;
		}
	}
}

ProjectUsage CountProjectUsage(const BankUsers &users, int imageCount) {
	ProjectUsage u; u.total.assign((size_t)std::max(0, imageCount), 0);
	for (const BankUserFile &f : users.files) {
		std::vector<int> v((size_t)std::max(0, imageCount), 0);
		FrameData fd;
		if (!fs::exists(fs::path(Utf8ToWide(f.path)))) { u.perFile.push_back(v); continue; }   // a project may name a file that is not shipped
		if (!fd.load(Utf8ToAnsi(f.path).c_str())) { u.errors.push_back(f.path); u.perFile.push_back(v); continue; }
		CountFile(fd, v);
		if (v.size() > u.total.size()) u.total.resize(v.size(), 0);
		for (size_t i = 0; i < v.size(); i++) u.total[i] += v[i];
		u.perFile.push_back(std::move(v));
	}
	return u;
}

bool IsIdentityRemap(const std::vector<int> &r) { for (size_t i = 0; i < r.size(); i++) if (r[i] != (int)i) return false; return true; }

// The ids are rewritten in the file's own bytes (offsets recorded by the loader): every other byte stays as shipped.
bool RemapHa6File(const std::string &path, const std::vector<int> &remap, bool backup, int *changed, int *cleared, std::string *err) {
	if (changed) *changed = 0; if (cleared) *cleared = 0;
	const std::string ansi = Utf8ToAnsi(path);
	std::vector<uint8_t> bytes;
	{ std::ifstream f(fs::path(Utf8ToWide(path)), std::ios::binary); if (!f) { if (err) *err = "cannot read " + path; return false; } bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); }
	Ha6SpriteRefSink sink; FrameData fd;
	g_ha6SpriteRefSink = &sink; const bool ok = fd.load(ansi.c_str()); g_ha6SpriteRefSink = nullptr;
	if (!ok || !sink.base) { if (err) *err = "cannot read " + path + " as an HA6 file"; return false; }
	int n = 0, c = 0;
	for (const auto &r : sink.refs) {
		if (r.second || r.first + 4 > bytes.size()) continue;   // usePat layers draw PAT parts, not CG images
		int32_t id; memcpy(&id, &bytes[r.first], 4);
		if (id < 0 || id >= (int)remap.size() || remap[id] == id) continue;
		const int32_t now = remap[id] < 0 ? -1 : remap[id];
		memcpy(&bytes[r.first], &now, 4); n++; if (now < 0) c++;
	}
	if (changed) *changed = n; if (cleared) *cleared = c;
	if (!n) return true;
	std::error_code ec;
	const fs::path p(Utf8ToWide(path)); fs::path bak = p; bak += L".bak"; fs::path tmp = p; tmp += L".tmp";
	if (backup && !fs::exists(bak, ec)) fs::copy_file(p, bak, ec);
	{ std::ofstream f(tmp, std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); if (!f) { if (err) *err = "cannot write " + path; return false; } }
	fs::rename(tmp, p, ec);
	if (ec) { if (err) *err = "cannot replace " + path + ": " + ec.message(); return false; }
	return true;
}

bool RemapUserFiles(const BankUsers &users, const std::vector<int> &remap, bool backup, DiskRemapResult &res, const std::vector<std::string> &skip) {
	bool ok = true;
	std::set<std::string> sk; for (auto &s : skip) sk.insert(NormPath(s));
	for (const BankUserFile &f : users.files) {
		if (f.shared) { res.sharedSkipped.push_back(f.path); continue; }
		if (sk.count(NormPath(f.path))) continue;
		std::error_code ec; if (!fs::exists(fs::path(Utf8ToWide(f.path)), ec)) continue;
		int ch = 0, cl = 0; std::string e;
		if (!RemapHa6File(f.path, remap, backup, &ch, &cl, &e)) { res.errors.push_back(e); ok = false; continue; }
		res.refsChanged += ch; res.refsCleared += cl;
		(ch ? res.written : res.unchanged).push_back(f.path);
	}
	return ok;
}

} // namespace cgm
