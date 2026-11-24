// pcmod package files on disk; see pcmod_package.h.
#include "pcmod_package.h"
#include "pcmod_zip.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace fs = std::filesystem;

namespace pcmod {

static fs::path U8(const std::string& s) { return fs::u8path(s); }
static std::string Str(const fs::path& p) { return p.u8string(); }
static std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return s; }
static fs::path RelToNative(const std::string& canon)
{
	fs::path p;
	size_t s = 0;
	for (;;) {
		const size_t e = std::min(canon.find('\\', s), canon.size());
		p /= U8(canon.substr(s, e - s));
		if (e == canon.size()) break;
		s = e + 1;
	}
	return p;
}

bool ReadAll(const std::string& path, std::vector<uint8_t>& out)
{
	std::ifstream f(U8(path), std::ios::binary);
	if (!f) return false;
	f.seekg(0, std::ios::end);
	const std::streamoff n = f.tellg();
	if (n < 0) return false;
	f.seekg(0);
	out.resize((size_t)n);
	if (n) f.read((char*)out.data(), n);
	return (bool)f;
}

static bool WriteAll(const fs::path& p, const void* data, size_t size, std::string& why)
{
	std::error_code ec;
	fs::create_directories(p.parent_path(), ec);
	std::ofstream f(p, std::ios::binary | std::ios::trunc);
	if (!f) { why = "cannot write " + Str(p); return false; }
	if (size) f.write((const char*)data, (std::streamsize)size);
	f.close();
	if (!f) { why = "write failed: " + Str(p); return false; }
	return true;
}

std::string Sha256OfFile(const std::string& path)
{
	std::vector<uint8_t> b;
	if (!ReadAll(path, b)) return {};
	return Sha256Hex(b.data(), b.size());
}

bool HashFile(const std::string& absPath, FileEntry& e, std::string& why)
{
	std::vector<uint8_t> b;
	if (!ReadAll(absPath, b)) { why = "cannot read " + absPath; return false; }
	e.sha256 = Sha256Hex(b.data(), b.size());
	e.size = b.size();
	return true;
}

bool ScanSourceDir(const std::string& dir, std::vector<SourceFile>& out, std::string& why)
{
	out.clear();
	std::error_code ec;
	const fs::path root = U8(dir);
	if (!fs::is_directory(root, ec)) { why = "not a directory: " + dir; return false; }
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
		if (it->is_symlink(ec)) { why = "reparse point / symlink refused: " + Str(it->path()); return false; }
		if (!it->is_regular_file(ec)) continue;
		std::string rel = Str(it->path().lexically_relative(root));
		std::replace(rel.begin(), rel.end(), '/', '\\');
		const std::string low = Lower(rel);
		if (low == "pcmod.json" || low == "mod.ini") continue;
		out.push_back({rel, Str(it->path())});
	}
	if (ec) { why = "cannot scan " + dir + ": " + ec.message(); return false; }
	std::sort(out.begin(), out.end(), [](const SourceFile& a, const SourceFile& b) { return Lower(a.rel) < Lower(b.rel); });
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
PackResult Pack(const std::vector<SourceFile>& sources, const Manifest& meta, const std::string& outDir, bool writeZip, const s2::Files* aux)
{
	PackResult r;
	Manifest m = meta;
	if (!m.HasAux()) m.schema = 1;
	else if (!aux) { r.error = "schema 2 package without its patches/features/rules texts"; return r; }
	struct AuxOut { const AuxFile* ref; const std::string* text; };
	const AuxOut auxOut[3] = { { &m.exePatches, aux ? &aux->patches : nullptr }, { &m.features, aux ? &aux->features : nullptr }, { &m.rules, aux ? &aux->rules : nullptr } };
	m.files.clear();
	std::vector<std::vector<uint8_t>> bytes;
	for (const SourceFile& s : sources) {
		FileEntry e;
		std::string why;
		if (!CanonicalPath(s.rel, e.path, why)) { r.error = "'" + s.rel + "': " + why; return r; }
		std::vector<uint8_t> b;
		if (!ReadAll(s.absPath, b)) { r.error = "cannot read " + s.absPath; return r; }
		e.sha256 = Sha256Hex(b.data(), b.size());
		e.size = b.size();
		m.files.push_back(e);
		bytes.push_back(std::move(b));
	}
	const std::string bad = Validate(m);
	if (!bad.empty()) { r.error = bad; return r; }

	std::error_code ec;
	const fs::path pkg = U8(outDir) / U8(m.id);
	if (fs::exists(pkg, ec)) {
		if (fs::exists(pkg / "pcmod.json", ec)) fs::remove_all(pkg, ec);
		else if (!fs::is_empty(pkg, ec)) { r.error = Str(pkg) + " exists and is not a pcmod package; refusing to overwrite it"; return r; }
		if (ec) { r.error = "cannot clear " + Str(pkg) + ": " + ec.message(); return r; }
	}
	fs::create_directories(pkg, ec);
	for (size_t i = 0; i < m.files.size(); ++i)
		if (!WriteAll(pkg / RelToNative(m.files[i].path), bytes[i].data(), bytes[i].size(), r.error)) return r;
	for (const AuxOut& a : auxOut)
		if (a.ref->present && !WriteAll(pkg / U8(a.ref->path), a.text->data(), a.text->size(), r.error)) return r;
	const std::string json = WriteManifest(m);
	if (!WriteAll(pkg / "pcmod.json", json.data(), json.size(), r.error)) return r;
	r.packageDir = Str(pkg);
	r.digestHex = PackageDigestHex(m);
	r.simDigestHex = PackageSimDigestHex(m);

	if (writeZip) {
		// <id>/pcmod.json first, then the files in digest order; one top-level directory entry.
		std::vector<ZipEntry> z;
		z.push_back({m.id + "/", {}});
		z.push_back({m.id + "/pcmod.json", std::vector<uint8_t>(json.begin(), json.end())});
		for (const AuxOut& a : auxOut)
			if (a.ref->present) z.push_back({m.id + "/" + a.ref->path, std::vector<uint8_t>(a.text->begin(), a.text->end())});
		std::vector<size_t> order(m.files.size());
		for (size_t i = 0; i < order.size(); ++i) order[i] = i;
		std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return m.files[a].key() < m.files[b].key(); });
		for (size_t i : order) {
			std::string name = m.id + "/" + m.files[i].path;
			std::replace(name.begin(), name.end(), '\\', '/');
			z.push_back({name, bytes[i]});
		}
		std::vector<uint8_t> zip;
		if (!BuildZip(z, zip, r.error)) return r;
		const fs::path zp = U8(outDir) / U8(m.id + "-" + m.version + ".zip");
		if (!WriteAll(zp, zip.data(), zip.size(), r.error)) return r;
		r.zipPath = Str(zp);
	}

	// Read back what was written: the package must verify exactly as a consumer will see it.
	const VerifyResult vd = VerifyDirectory(r.packageDir);
	if (!vd.ok) { r.error = "written package does not verify: " + (vd.errors.empty() ? std::string("?") : vd.errors[0]); return r; }
	if (vd.digestHex != r.digestHex) { r.error = "written package digest differs on read-back"; return r; }
	r.simDigestHex = vd.simDigestHex;   // schema 2: the sim digest needs the PARSED TOML (set by VerifyDirectory)
	if (writeZip) {
		const VerifyResult vz = VerifyZip(r.zipPath);
		if (!vz.ok) { r.error = "written archive does not verify: " + (vz.errors.empty() ? std::string("?") : vz.errors[0]); return r; }
		if (vz.digestHex != r.digestHex) { r.error = "archive digest differs from the folder's"; return r; }
	}
	r.manifest = m;
	r.ok = true;
	return r;
}

// ---------------------------------------------------------------------------------------------------------------------
// Schema 2: check the aux TOML texts against the manifest and parse them (MBAACC profile); sets the sim digest.
static void LoadAux(VerifyResult& v, const s2::Files& t)
{
	if (!v.manifest.HasAux()) return;
	const std::string e = s2::LoadFromTexts(v.manifest, t, s2::MbaaccProfile(), v.s2);
	if (!e.empty()) v.errors.push_back(e);
	v.simDigestHex = PackageSimDigestHex(v.manifest);
}
static bool IsAuxPath(const Manifest& m, const std::string& lowRel)
{
	return (m.exePatches.present && lowRel == m.exePatches.path) || (m.features.present && lowRel == m.features.path) || (m.rules.present && lowRel == m.rules.path);
}

static bool ParseInto(const std::string& text, VerifyResult& v)
{
	std::string err;
	if (!ParseManifest(text, v.manifest, err)) { v.errors.push_back("pcmod.json: " + err); return false; }
	v.digestHex = PackageDigestHex(v.manifest);
	v.simDigestHex = PackageSimDigestHex(v.manifest);
	return true;
}

VerifyResult VerifyDirectory(const std::string& dir, bool requireFolderName)
{
	VerifyResult v;
	std::error_code ec;
	const fs::path root = U8(dir);
	std::vector<uint8_t> text;
	if (!ReadAll(Str(root / "pcmod.json"), text)) { v.errors.push_back("no pcmod.json in " + dir + " (a legacy mod.ini-only folder is cosmetic, never sim)"); return v; }
	if (!ParseInto(std::string(text.begin(), text.end()), v)) return v;
	const Manifest& m = v.manifest;
	std::string folder = Str(fs::absolute(root, ec).lexically_normal().filename());
	if (folder.empty()) folder = Str(fs::absolute(root, ec).lexically_normal().parent_path().filename());
	if (requireFolderName && folder != m.id) v.errors.push_back("folder name '" + folder + "' differs from id '" + m.id + "'");

	// Index the files on disk by their normalised lowercase path first: the spec compares paths lowercase,
	// so a manifest's "DATA\\_type.txt" must find data/_type.txt on case-sensitive filesystems too.
	std::map<std::string, fs::path> onDisk;
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
		if (it->is_symlink(ec)) { v.warnings.push_back(Str(it->path()) + ": reparse point inside the package (never served)"); continue; }
		if (!it->is_regular_file(ec)) continue;
		std::string rel = Str(it->path().lexically_relative(root));
		std::replace(rel.begin(), rel.end(), '/', '\\');
		const std::string low = Lower(rel);
		if (low == "pcmod.json" || low == "mod.ini" || IsAuxPath(m, low)) continue;
		std::string norm, why;
		if (NormalisePath(rel, norm, why)) onDisk.emplace(norm, it->path());
		else v.warnings.push_back(rel + ": not listed in pcmod.json (never served)");
	}
	std::set<std::string> listed;
	for (const FileEntry& f : m.files) {
		listed.insert(f.key());
		const auto hit = onDisk.find(f.key());
		if (hit == onDisk.end()) { v.errors.push_back(f.path + ": missing"); continue; }
		std::vector<uint8_t> b;
		if (!ReadAll(Str(hit->second), b)) { v.errors.push_back(f.path + ": unreadable"); continue; }
		if (b.size() != f.size) { v.errors.push_back(f.path + ": size " + std::to_string(b.size()) + ", manifest says " + std::to_string(f.size)); continue; }
		if (Sha256Hex(b.data(), b.size()) != f.sha256) v.errors.push_back(f.path + ": sha256 differs from the manifest");
	}
	for (const auto& [norm, path] : onDisk)
		if (!listed.count(norm)) v.warnings.push_back(Str(path.lexically_relative(root)) + ": not listed in pcmod.json (never served)");
	if (m.HasAux()) {
		s2::Files t;
		auto rd = [&](const AuxFile& a, std::string& out) {
			std::vector<uint8_t> b;
			if (a.present && !ReadAll(Str(root / U8(a.path)), b)) v.errors.push_back(m.id + ": " + a.path + " is missing");
			out.assign(b.begin(), b.end());
		};
		rd(m.exePatches, t.patches); rd(m.features, t.features); rd(m.rules, t.rules);
		if (v.errors.empty()) LoadAux(v, t);
	}
	v.ok = v.errors.empty();
	return v;
}

VerifyResult VerifyZipBytes(const std::vector<uint8_t>& bytes)
{
	VerifyResult v;
	std::vector<ZipEntry> entries;
	std::string why;
	if (!ReadZip(bytes, entries, why)) { v.errors.push_back("zip: " + why); return v; }
	// Single top-level directory.
	std::string top;
	std::map<std::string, const ZipEntry*> files;   // name below <top>/ as stored
	for (const ZipEntry& e : entries) {
		std::string n = e.name;
		std::replace(n.begin(), n.end(), '\\', '/');
		const size_t slash = n.find('/');
		if (slash == std::string::npos || slash == 0) { v.errors.push_back("zip: '" + e.name + "' is not inside a top-level <id>/ directory"); continue; }
		const std::string t = n.substr(0, slash);
		if (top.empty()) top = t;
		else if (t != top) { v.errors.push_back("zip: more than one top-level directory ('" + top + "', '" + t + "')"); continue; }
		const std::string rest = n.substr(slash + 1);
		if (rest.empty() || rest.back() == '/') continue;   // directory entry
		if (!files.emplace(rest, &e).second) v.errors.push_back("zip: '" + e.name + "' appears twice");
	}
	if (!v.errors.empty()) return v;
	if (top.empty()) { v.errors.push_back("zip: empty archive"); return v; }
	auto mj = files.find("pcmod.json");
	if (mj == files.end()) { v.errors.push_back("zip: no " + top + "/pcmod.json"); return v; }
	if (!ParseInto(std::string(mj->second->data.begin(), mj->second->data.end()), v)) return v;
	const Manifest& m = v.manifest;
	if (top != m.id) v.errors.push_back("zip: top-level directory '" + top + "' differs from id '" + m.id + "'");

	std::map<std::string, const FileEntry*> listed;
	for (const FileEntry& f : m.files) listed[f.key()] = &f;
	std::set<std::string> found;
	s2::Files auxText;
	for (const auto& [rel, e] : files) {
		if (rel == "pcmod.json" || Lower(rel) == "mod.ini") continue;
		if (IsAuxPath(m, rel)) {
			const std::string t(e->data.begin(), e->data.end());
			(rel == m.exePatches.path ? auxText.patches : rel == m.features.path ? auxText.features : auxText.rules) = t;
			continue;
		}
		std::string norm, bad;
		if (!NormalisePath(rel, norm, bad)) { v.errors.push_back("zip: '" + rel + "' refused: " + bad); continue; }
		auto it = listed.find(norm);
		if (it == listed.end()) { v.errors.push_back("zip: '" + rel + "' is not listed in pcmod.json (refused, not extracted)"); continue; }
		if (!found.insert(norm).second) { v.errors.push_back("zip: '" + rel + "' appears twice under two spellings"); continue; }
		const FileEntry& f = *it->second;
		if (e->data.size() != f.size) v.errors.push_back("zip: '" + rel + "': size differs from the manifest");
		else if (Sha256Hex(e->data.data(), e->data.size()) != f.sha256) v.errors.push_back("zip: '" + rel + "': sha256 differs from the manifest");
	}
	for (const auto& [key, f] : listed) if (!found.count(key)) v.errors.push_back("zip: listed file '" + f->path + "' is missing");
	if (v.errors.empty()) LoadAux(v, auxText);
	v.ok = v.errors.empty();
	return v;
}

VerifyResult VerifyZip(const std::string& zipPath)
{
	std::vector<uint8_t> b;
	if (!ReadAll(zipPath, b)) { VerifyResult v; v.errors.push_back("cannot read " + zipPath); return v; }
	return VerifyZipBytes(b);
}

VerifyResult VerifyPath(const std::string& path)
{
	std::error_code ec;
	if (fs::is_directory(U8(path), ec)) return VerifyDirectory(path);
	return VerifyZip(path);
}

bool ReadModIni(const std::string& packageDir, bool& enabled, int& priority)
{
	std::vector<uint8_t> b;
	if (!ReadAll(Str(U8(packageDir) / "mod.ini"), b)) return false;
	const std::string t(b.begin(), b.end());
	bool inConfig = false;
	size_t s = 0;
	while (s < t.size()) {
		size_t e = t.find('\n', s);
		if (e == std::string::npos) e = t.size();
		std::string line = t.substr(s, e - s);
		s = e + 1;
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
		size_t a = line.find_first_not_of(" \t");
		if (a == std::string::npos) continue;
		line = line.substr(a);
		if (line[0] == ';' || line[0] == '#') continue;
		if (line[0] == '[') { inConfig = Lower(line) == "[config]"; continue; }
		if (!inConfig) continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = Lower(line.substr(0, eq)), val = line.substr(eq + 1);
		while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
		val.erase(0, std::min(val.find_first_not_of(" \t"), val.size()));
		if (k == "enabled") enabled = val == "1" || Lower(val) == "true";
		else if (k == "priority") { try { priority = std::stoi(val); } catch (...) {} }
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
SessionResult ResolveSession(const std::string& exeSha256Hex, const std::vector<std::string>& packageDirs, const std::string& mode)
{
	SessionResult s;
	s.mode = mode;
	struct Pkg { std::string dir; Manifest m; int priority = 0; std::string simDigest; bool broken = false; };
	std::vector<Pkg> enabled;
	for (const std::string& d : packageDirs) {
		bool on = true;
		const VerifyResult v = VerifyDirectory(d);
		Pkg p;
		p.dir = d;
		p.m = v.manifest;
		p.priority = v.manifest.priority;
		ReadModIni(d, on, p.priority);
		if (!on) { s.notes.push_back(d + ": disabled in mod.ini"); continue; }
		if (!v.ok) {
			const std::string why = d + ": BROKEN: " + (v.errors.empty() ? std::string("?") : v.errors[0]);
			// A broken cosmetic package is just not served; a broken (or unreadable) sim package blocks the digest.
			if (!v.manifest.id.empty() && v.manifest.cls == PackageClass::Cosmetic) s.notes.push_back(why);
			else s.errors.push_back(why);
			continue;
		}
		if (p.m.game != "mbaacc") { s.notes.push_back(p.m.id + ": for game '" + p.m.game + "', ignored"); continue; }
		p.simDigest = v.simDigestHex;
		if (p.m.cls == PackageClass::Sim && std::find(p.m.exe.begin(), p.m.exe.end(), exeSha256Hex) == p.m.exe.end()) {
			s.errors.push_back(p.m.id + " " + p.m.version + ": BROKEN: not valid on this exe");
			continue;
		}
		enabled.push_back(p);
	}
	std::map<std::string, const Pkg*> byId;
	for (const Pkg& p : enabled) {
		if (!byId.emplace(p.m.id, &p).second) s.errors.push_back(p.m.id + ": enabled twice");
	}
	for (const Pkg& p : enabled) {
		for (const Requirement& r : p.m.requires_) {
			auto it = byId.find(r.id);
			if (it == byId.end()) s.errors.push_back(p.m.id + ": BROKEN: requires " + r.id + " " + r.version + ", not enabled");
			else if (it->second->m.version != r.version) s.errors.push_back(p.m.id + ": BROKEN: requires " + r.id + " " + r.version + ", have " + it->second->m.version);
		}
		for (const std::string& c : p.m.conflicts)
			if (byId.count(c)) s.errors.push_back(p.m.id + ": BROKEN: conflicts with enabled " + c);
	}
	// Path ownership (§4): highest priority (then id) serves a path.
	std::vector<const Pkg*> order;
	for (const Pkg& p : enabled) order.push_back(&p);
	std::sort(order.begin(), order.end(), [](const Pkg* a, const Pkg* b) { return a->priority != b->priority ? a->priority > b->priority : a->m.id < b->m.id; });
	std::map<std::string, const Pkg*> owner;
	for (const Pkg* p : order) {
		for (const FileEntry& f : p->m.files) {
			const std::string k = f.key();
			auto it = owner.find(k);
			if (it == owner.end()) { owner[k] = p; continue; }
			const Pkg* w = it->second;
			const bool simInvolved = w->m.cls == PackageClass::Sim || p->m.cls == PackageClass::Sim;
			const bool allowed = std::find(w->m.overrides.begin(), w->m.overrides.end(), p->m.id) != w->m.overrides.end();
			if (simInvolved && !allowed) s.errors.push_back(k + ": shipped by " + w->m.id + " and " + p->m.id + " (sim involved; " + w->m.id + " does not list " + p->m.id + " in overrides)");
			else s.notes.push_back(k + ": " + p->m.id + " overridden by " + w->m.id);
		}
	}
	for (const Pkg* p : order) {
		if (p->m.cls != PackageClass::Sim) continue;
		if (!CountsForMode(p->m.modes, mode)) {
			s.notes.push_back(p->m.id + " " + p->m.version + ": not in the '" + mode + "' set (modes " + JoinModes(p->m.modes) + ")");
			continue;
		}
		s.simOrder.push_back({p->m.id, p->m.version, p->priority, p->simDigest, p->m.schema});
	}
	if (!s.errors.empty()) return s;
	s.digestHex = SessionDigestHex(exeSha256Hex, s.simOrder, s2::MbaaccProfile().abiId);
	s.ok = true;
	return s;
}

} // namespace pcmod
