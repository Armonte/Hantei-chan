// See hmod_conflict.h.
#include "hmod_conflict.h"
#include "hmod_ha6.h"
#include "../pcmod/pcmod_package.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace hmod {

const char* ConflictKindName(ConflictKind k)
{
	switch (k) {
	case ConflictKind::File: return "same file";
	case ConflictKind::Ha6Slot: return "same HA6 slot";
	case ConflictKind::CharaCell: return "same charaselect cell";
	}
	return "?";
}

static std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return s; }
static bool EndsWith(const std::string& s, const char* suf) { const size_t n = std::strlen(suf); return s.size() >= n && s.compare(s.size() - n, n, suf) == 0; }

// ---------------------------------------------------------------- loaders
static void ScanTree(const fs::path& root, PackageView& v, bool skipTop)
{
	std::error_code ec;
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
		if (!it->is_regular_file(ec)) continue;
		std::string rel = it->path().lexically_relative(root).u8string();
		const std::string norm = NormKey(rel);
		if (skipTop && (norm == "mod.ini" || norm == "pcmod.json")) continue;
		PkgFile f;
		f.abs = it->path().u8string();
		f.size = (uint64_t)fs::file_size(it->path(), ec);
		v.files[norm] = f;   // hashed lazily (HashOf) so a big folder of installed mods is not read up front
	}
}

static const std::string& HashOf(PkgFile& f)
{
	if (f.sha256.empty()) { pcmod::FileEntry e; std::string why; if (pcmod::HashFile(f.abs, e, why)) f.sha256 = e.sha256; else f.sha256 = "unreadable"; }
	return f.sha256;
}

bool LoadPackageView(const std::string& dir, PackageView& out, std::string& err)
{
	std::error_code ec;
	const fs::path d = fs::u8path(dir);
	if (!fs::is_directory(d, ec)) { err = dir + " is not a folder"; return false; }
	PackageView v;
	v.dir = dir;
	v.id = d.filename().u8string();
	if (fs::is_regular_file(d / "pcmod.json", ec)) {
		std::vector<uint8_t> raw;
		pcmod::ReadAll((d / "pcmod.json").u8string(), raw);
		pcmod::Manifest m;
		std::string e;
		if (pcmod::ParseManifest(std::string(raw.begin(), raw.end()), m, e)) {
			v.id = m.id; v.version = m.version; v.cls = pcmod::ClassName(m.cls); v.origin = "installed pcmod: " + dir;
			v.conflicts.insert(m.conflicts.begin(), m.conflicts.end());
			v.overrides.insert(m.overrides.begin(), m.overrides.end());
			for (const pcmod::FileEntry& f : m.files) {
				std::string norm, why;
				if (!pcmod::NormalisePath(f.path, norm, why)) continue;
				PkgFile pf;
				pf.abs = (d / fs::u8path(f.path)).u8string();   // the author's case: the disk may be case-sensitive (WSL) pf.sha256 = f.sha256; pf.size = f.size;
				v.files[norm] = pf;
			}
			bool en = true; int pri = 0;
			if (pcmod::ReadModIni(dir, en, pri)) v.enabled = en;
			out = std::move(v);
			return true;
		}
		err = "pcmod.json: " + e;
		return false;
	}
	v.origin = "known: " + dir;
	bool en = true; int pri = 0;
	if (pcmod::ReadModIni(dir, en, pri)) v.enabled = en;
	ScanTree(d, v, true);
	out = std::move(v);
	return true;
}

std::vector<PackageView> LoadInstalledPcmod(const std::string& gameDir)
{
	std::vector<PackageView> out;
	std::error_code ec;
	const fs::path mods = fs::u8path(gameDir) / "mods";
	if (!fs::is_directory(mods, ec)) return out;
	std::vector<fs::path> dirs;
	for (const auto& e : fs::directory_iterator(mods, ec)) if (e.is_directory(ec)) dirs.push_back(e.path());
	std::sort(dirs.begin(), dirs.end());
	for (const fs::path& d : dirs) {
		PackageView v; std::string err;
		if (LoadPackageView(d.u8string(), v, err)) { v.origin = "installed mod: " + d.u8string(); out.push_back(std::move(v)); }
	}
	return out;
}

std::vector<PackageView> LoadImPackages(const std::string& imRoot, const std::string& skipPack)
{
	std::vector<PackageView> out;
	std::error_code ec;
	const fs::path root = fs::u8path(imRoot);
	if (fs::is_directory(root / "Mods", ec)) {
		PackageView v;
		v.id = "(Mods: your own files)"; v.dir = (root / "Mods").u8string(); v.origin = "IM Mods folder: " + v.dir;
		ScanTree(root / "Mods", v, false);
		if (!v.files.empty()) out.push_back(std::move(v));
	}
	if (fs::is_directory(root / "Packs", ec)) {
		std::vector<fs::path> dirs;
		for (const auto& e : fs::directory_iterator(root / "Packs", ec)) if (e.is_directory(ec)) dirs.push_back(e.path());
		std::sort(dirs.begin(), dirs.end());
		for (const fs::path& d : dirs) {
			if (!skipPack.empty() && Lower(d.filename().u8string()) == Lower(skipPack)) continue;
			PackageView v;
			v.id = d.filename().u8string(); v.dir = d.u8string(); v.origin = "IM pack: " + v.dir;
			ScanTree(d, v, true);
			out.push_back(std::move(v));
		}
	}
	return out;
}

PackageView ViewOfProjectPackage(const Project& p, const Package& pkg, const std::vector<TrackedFile>& tracked)
{
	PackageView v;
	v.id = pkg.id; v.version = pkg.version; v.origin = "this project"; v.dir = p.dir; v.cls = pkg.cls;
	v.conflicts.insert(pkg.conflicts.begin(), pkg.conflicts.end());
	v.overrides.insert(pkg.overrides.begin(), pkg.overrides.end());
	const std::vector<std::string> ids = ResolvePackageAssets(p, pkg);
	for (const TrackedFile& t : tracked) {
		if (!Ships(t.status) || std::find(ids.begin(), ids.end(), t.asset) == ids.end()) continue;
		PkgFile f; f.abs = t.abs; f.sha256 = t.sha256; f.size = t.size;
		v.files[t.norm] = f;
	}
	for (const std::string& id : ids) if (const Asset* a = p.FindAsset(id); a && a->chara.any()) v.declared.push_back(a->chara);
	return v;
}

// ---------------------------------------------------------------- charaselect
std::map<std::string, std::string> ParseCharaselect(const std::string& text)
{
	std::map<std::string, std::string> out;
	std::stringstream ss(text);
	std::string line, sect;
	while (std::getline(ss, line)) {
		const size_t c = line.find("//");
		if (c != std::string::npos) line.erase(c);
		size_t a = 0, b = line.size();
		while (a < b && (line[a] == ' ' || line[a] == '\t' || line[a] == '\r')) ++a;
		while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t' || line[b - 1] == '\r')) --b;
		line = line.substr(a, b - a);
		if (line.empty()) continue;
		if (line.front() == '[' && line.back() == ']') { sect = Lower(line.substr(1, line.size() - 2)); out[sect]; continue; }
		const size_t eq = line.find('=');
		if (eq == std::string::npos || sect.empty()) continue;
		auto trim = [](std::string s) { size_t x = 0, y = s.size(); while (x < y && (s[x] == ' ' || s[x] == '\t')) ++x; while (y > x && (s[y - 1] == ' ' || s[y - 1] == '\t')) --y; return s.substr(x, y - x); };
		out[sect] += Lower(trim(line.substr(0, eq))) + "=" + trim(line.substr(eq + 1)) + ";";
	}
	return out;
}

static std::string ReadText(const std::string& path) { std::vector<uint8_t> b; if (!pcmod::ReadAll(path, b)) return {}; return std::string(b.begin(), b.end()); }

// Sections of a charaselect.txt that differ from the vanilla file (changed or new): key -> normalised content.
static std::map<std::string, std::string> ClaimedSections(const std::map<std::string, std::string>& mine, const VanillaIndex* vanilla)
{
	std::map<std::string, std::string> base;
	if (vanilla) { std::vector<uint8_t> b; if (vanilla->ReadBytes("charaselect.txt", b)) base = ParseCharaselect(std::string(b.begin(), b.end())); }
	std::map<std::string, std::string> out;
	for (const auto& kv : mine) {
		if (kv.first == "system") continue;
		const auto it = base.find(kv.first);
		if (it == base.end() || it->second != kv.second) out[kv.first] = kv.second;
	}
	return out;
}

static std::string FieldOf(const std::string& content, const char* key)
{
	const std::string k = std::string(key) + "=";
	size_t p = 0;
	while (p < content.size()) {
		const size_t e = content.find(';', p);
		const std::string part = content.substr(p, e == std::string::npos ? std::string::npos : e - p);
		if (part.rfind(k, 0) == 0) return part.substr(k.size());
		if (e == std::string::npos) break;
		p = e + 1;
	}
	return {};
}

static bool SectionsEqual(const std::string& a, const std::string& b)
{
	if (a.rfind("decl=1;", 0) == 0 || b.rfind("decl=1;", 0) == 0)   // a declared claim only knows Name / File1 / File2
		return FieldOf(a, "name") == FieldOf(b, "name") && FieldOf(a, "file1") == FieldOf(b, "file1") && FieldOf(a, "file2") == FieldOf(b, "file2");
	return a == b;
}

// ---------------------------------------------------------------- the check
static std::string MoonKey(const std::string& norm)
{
	std::string s = norm;
	const size_t dot = s.find_last_of('.');
	if (dot != std::string::npos) s.erase(dot);
	if (EndsWith(s, "_r")) s.erase(s.size() - 2);
	return s;
}
static bool IsHa6Path(const std::string& norm) { return EndsWith(norm, ".ha6"); }

static std::vector<uint32_t> ClaimedSlots(const std::string& norm, const std::string& abs, const VanillaIndex* vanilla)
{
	std::vector<uint8_t> mine, base;
	if (!pcmod::ReadAll(abs, mine) || !IsHa6(mine)) return {};
	Ha6Slots sm, sb;
	std::string err;
	if (!SplitHa6(mine, sm, err)) return {};
	if (vanilla && vanilla->ReadBytes(norm, base) && IsHa6(base)) { std::string e2; if (!SplitHa6(base, sb, e2)) sb.clear(); }
	return DiffSlots(sb, sm).claimed;
}

static std::string SlotList(const std::vector<uint32_t>& s)
{
	std::string r;
	for (size_t i = 0; i < s.size() && i < 8; ++i) r += (i ? ", " : "") + std::to_string(s[i]);
	if (s.size() > 8) r += ", ... (" + std::to_string(s.size()) + " total)";
	return r;
}

std::vector<Conflict> CheckConflicts(const PackageView& mine, const std::vector<PackageView>& others, const VanillaIndex* vanilla)
{
	std::vector<Conflict> out;
	// mine's HA6 slot claims per moon key
	std::map<std::string, std::map<uint32_t, std::string>> myClaims;   // moonKey -> slot -> file
	for (const auto& kv : mine.files)
		if (IsHa6Path(kv.first))
			for (uint32_t s : ClaimedSlots(kv.first, kv.second.abs, vanilla)) myClaims[MoonKey(kv.first)][s] = kv.first;
	std::map<std::string, std::string> myChara;   // section -> content
	if (const auto it = mine.files.find("charaselect.txt"); it != mine.files.end())
		myChara = ClaimedSections(ParseCharaselect(ReadText(it->second.abs)), vanilla);
	for (const CharaClaim& d : mine.declared) {
		if (d.charaNo >= 0) {
			char b[24]; std::snprintf(b, sizeof b, "charano_%03d", d.charaNo);
			std::string c = "decl=1;name=" + d.name + ";file1=" + d.file1 + ";file2=" + d.file2 + ";";
			myChara.emplace(b, c);
		}
	}

	for (const PackageView& o : others) {
		if (o.id == mine.id && o.dir == mine.dir) continue;
		// resolution by declaration
		const bool declared = mine.conflicts.count(o.id) || o.conflicts.count(mine.id);
		const bool overrides = mine.overrides.count(o.id);
		auto add = [&](Conflict c) {
			c.mine = mine.id; c.other = o.id; c.otherOrigin = o.origin;
			if (c.blocks && declared) { c.blocks = false; c.why += " [resolved: declared in conflicts, the two are never enabled together]"; }
			else if (c.blocks && overrides) { c.blocks = false; c.why += " [resolved: this package declares overrides " + o.id + "]"; }
			if (!o.enabled && c.blocks) c.why += " (the other package is currently disabled)";
			out.push_back(std::move(c));
		};

		// 1. same file
		for (const auto& kv : mine.files) {
			const auto it = o.files.find(kv.first);
			if (it == o.files.end()) continue;
			PkgFile a = kv.second, b = it->second;
			Conflict c; c.kind = ConflictKind::File; c.what = kv.first;
			if (HashOf(a) == HashOf(b)) { c.blocks = false; c.why = "byte-identical in both: harmless"; }
			else c.why = "both packages ship " + kv.first + " with different bytes; the loser's edits would be silently lost";
			add(c);
		}
		// 2. same HA6 slot (any file of the same moon layer set)
		if (!myClaims.empty()) {
			std::map<std::string, std::map<uint32_t, std::string>> theirs;
			for (const auto& kv : o.files)
				if (IsHa6Path(kv.first) && myClaims.count(MoonKey(kv.first)))
					for (uint32_t s : ClaimedSlots(kv.first, kv.second.abs, vanilla)) theirs[MoonKey(kv.first)][s] = kv.first;
			for (const auto& mk : myClaims) {
				const auto t = theirs.find(mk.first);
				if (t == theirs.end()) continue;
				std::vector<uint32_t> both;
				for (const auto& s : mk.second) if (t->second.count(s.first)) both.push_back(s.first);
				if (both.empty()) continue;
				Conflict c; c.kind = ConflictKind::Ha6Slot;
				c.what = mk.first + " slot " + SlotList(both);
				c.why = "both change pattern slot(s) " + SlotList(both) + " of " + mk.first + " (" + mk.second.at(both[0]) + " vs " + t->second.at(both[0]) + ")";
				add(c);
			}
		}
		// 3. charaselect cell / section / name
		if (!myChara.empty() || !mine.declared.empty()) {
			std::map<std::string, std::string> theirChara;
			if (const auto it = o.files.find("charaselect.txt"); it != o.files.end())
				theirChara = ClaimedSections(ParseCharaselect(ReadText(it->second.abs)), vanilla);
			for (const CharaClaim& d : o.declared) {
				if (d.charaNo < 0) continue;
				char b[24]; std::snprintf(b, sizeof b, "charano_%03d", d.charaNo);
				theirChara.emplace(b, "decl=1;name=" + d.name + ";file1=" + d.file1 + ";file2=" + d.file2 + ";");
			}
			for (const auto& s : myChara) {
				const auto t = theirChara.find(s.first);
				if (t != theirChara.end() && !SectionsEqual(t->second, s.second)) {
					Conflict c; c.kind = ConflictKind::CharaCell; c.what = "[" + s.first + "]";
					c.why = "both define charaselect section [" + s.first + "] differently (" + FieldOf(s.second, "name") + " vs " + FieldOf(t->second, "name") + ")";
					add(c);
				}
			}
			// same Name in different sections
			for (const auto& s : myChara) {
				const std::string n = FieldOf(s.second, "name");
				if (n.empty()) continue;
				for (const auto& t : theirChara)
					if (t.first != s.first && FieldOf(t.second, "name") == n) {
						Conflict c; c.kind = ConflictKind::CharaCell; c.what = "Name = " + n;
						c.why = "both add a character named " + n + " in different cells ([" + s.first + "] vs [" + t.first + "])";
						add(c);
					}
			}
			// declared grid cell
			for (const CharaClaim& a : mine.declared)
				for (const CharaClaim& b : o.declared)
					if (a.cell >= 0 && a.cell == b.cell && !(a.name == b.name && a.file1 == b.file1)) {
						Conflict c; c.kind = ConflictKind::CharaCell; c.what = "grid cell " + std::to_string(a.cell);
						c.why = "both claim character-select grid cell " + std::to_string(a.cell) + " (" + a.name + " vs " + b.name + ")";
						add(c);
					}
		}
	}
	return out;
}

std::string DescribeConflict(const Conflict& c)
{
	return std::string(c.blocks ? "CONFLICT " : "note     ") + ConflictKindName(c.kind) + ": " + c.mine + " vs " + c.other + " [" + c.otherOrigin + "]: " + c.why;
}

} // namespace hmod
