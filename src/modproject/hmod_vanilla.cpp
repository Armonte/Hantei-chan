// See hmod_vanilla.h.
#include "hmod_vanilla.h"
#include "../fbarc/fb_archive.h"
#include "../pcmod/pcmod_sha256.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace hmod {

std::string NormKey(const std::string& rel)
{
	std::string s = rel;
	for (char& c : s) { if (c == '/') c = '\\'; else if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); }
	while (s.rfind(".\\", 0) == 0) s.erase(0, 2);
	while (!s.empty() && s[0] == '\\') s.erase(0, 1);
	return s;
}

static std::string FileLabel(const std::string& path)
{
	const fs::path p = fs::u8path(path);
	return p.filename().u8string();
}

void VanillaIndex::AddRow(const VanillaEntry& e)
{
	VanillaEntry r = e;
	r.path = NormKey(e.path);
	m_rows[r.path] = r;
}

void VanillaIndex::AddBytes(const std::string& path, const std::string& pack, std::vector<uint8_t> bytes)
{
	VanillaEntry r;
	r.path = NormKey(path); r.pack = pack; r.size = bytes.size();
	r.sha256 = pcmod::Sha256Hex(bytes.data(), bytes.size());
	m_rows[r.path] = r;
	m_mem[r.path] = std::move(bytes);
}

bool VanillaIndex::Build(const std::vector<std::string>& archives, const std::string& game, std::string& err,
                         const std::function<bool(size_t, size_t, const std::string&)>& progress)
{
	m_rows.clear();
	m_mem.clear();
	m_archives = archives;
	m_game = game;
	m_packset = ComputePackset8(archives);
	size_t total = 0, done = 0;
	std::vector<std::unique_ptr<fbarc::Archive>> opened;
	for (const std::string& a : archives) {
		std::string e;
		auto ar = fbarc::Open(a, &e);
		if (!ar) { err = a + ": " + e; return false; }
		total += ar->entries().size();
		opened.push_back(std::move(ar));
	}
	for (size_t k = 0; k < opened.size(); ++k) {
		const fbarc::Archive& ar = *opened[k];
		const std::string label = FileLabel(archives[k]);
		for (size_t i = 0; i < ar.entries().size(); ++i, ++done) {
			if (progress && (done % 64 == 0) && !progress(done, total, label)) { err = "cancelled"; m_rows.clear(); return false; }
			std::vector<uint8_t> bytes;
			std::string e;
			if (!ar.read(i, bytes, &e)) { err = label + " entry " + std::to_string(i) + ": " + e; return false; }
			VanillaEntry r;
			r.path = NormKey(fbarc::NameToUtf8(ar.relativePath(i)));
			r.pack = label;
			r.size = bytes.size();
			r.sha256 = pcmod::Sha256Hex(bytes.data(), bytes.size());
			m_rows[r.path] = std::move(r);   // a later archive overrides an earlier one
		}
	}
	return true;
}

bool VanillaIndex::Save(const std::string& tsvPath, std::string& err) const
{
	std::error_code ec;
	fs::create_directories(fs::u8path(tsvPath).parent_path(), ec);
	const std::string tmp = tsvPath + ".tmp";
	{
		std::ofstream f(fs::u8path(tmp), std::ios::binary);
		if (!f) { err = "cannot write " + tmp; return false; }
		f << "#hmod-vanilla-1\n#game\t" << m_game << "\n#packset\t" << m_packset << "\n";
		for (const std::string& a : m_archives) f << "#archive\t" << a << "\n";
		f << "path\tpack\tsize\tsha256\n";
		for (const auto& kv : m_rows) f << kv.second.path << '\t' << kv.second.pack << '\t' << kv.second.size << '\t' << kv.second.sha256 << '\n';
		if (!f) { err = "write failed: " + tmp; return false; }
	}
	fs::rename(fs::u8path(tmp), fs::u8path(tsvPath), ec);
	if (ec) { err = "rename failed: " + ec.message(); return false; }
	return true;
}

bool VanillaIndex::Load(const std::string& tsvPath, std::string& err)
{
	m_rows.clear();
	m_archives.clear();
	std::ifstream f(fs::u8path(tsvPath), std::ios::binary);
	if (!f) { err = "cannot read " + tsvPath; return false; }
	std::string line;
	if (!std::getline(f, line) || line.rfind("#hmod-vanilla-1", 0) != 0) { err = "not a hmod vanilla index"; return false; }
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;
		if (line[0] == '#') {
			const size_t t = line.find('\t');
			const std::string k = line.substr(0, t), v = t == std::string::npos ? "" : line.substr(t + 1);
			if (k == "#game") m_game = v; else if (k == "#packset") m_packset = v; else if (k == "#archive") m_archives.push_back(v);
			continue;
		}
		if (line.rfind("path\t", 0) == 0) continue;
		std::vector<std::string> c;
		std::stringstream ss(line);
		for (std::string x; std::getline(ss, x, '\t');) c.push_back(x);
		if (c.size() != 4) { err = "bad row: " + line; return false; }
		VanillaEntry r;
		r.path = c[0]; r.pack = c[1]; r.size = std::strtoull(c[2].c_str(), nullptr, 10); r.sha256 = c[3];
		m_rows[r.path] = r;
	}
	return true;
}

const VanillaEntry* VanillaIndex::Find(const std::string& normPath) const
{
	const auto it = m_rows.find(normPath);
	return it == m_rows.end() ? nullptr : &it->second;
}

struct VanillaIndex::OpenCache {
	struct One { std::unique_ptr<fbarc::Archive> ar; std::map<std::string, size_t> idx; };
	std::map<std::string, One> byLabel;
};

bool VanillaIndex::ReadBytes(const std::string& normPath, std::vector<uint8_t>& out, std::string* err) const
{
	const VanillaEntry* row = Find(normPath);
	if (!row) { if (err) *err = "not a vanilla file"; return false; }
	if (const auto mem = m_mem.find(normPath); mem != m_mem.end()) { out = mem->second; return true; }
	if (!m_cache) m_cache = std::make_shared<OpenCache>();
	auto it = m_cache->byLabel.find(row->pack);
	if (it == m_cache->byLabel.end()) {
		for (const std::string& a : m_archives) {
			if (FileLabel(a) != row->pack) continue;
			std::string e;
			auto ar = fbarc::Open(a, &e);
			if (!ar) { if (err) *err = e; return false; }
			OpenCache::One one;
			for (size_t i = 0; i < ar->entries().size(); ++i) one.idx[NormKey(fbarc::NameToUtf8(ar->relativePath(i)))] = i;   // later duplicate wins
			one.ar = std::move(ar);
			it = m_cache->byLabel.emplace(row->pack, std::move(one)).first;
			break;
		}
	}
	if (it == m_cache->byLabel.end()) { if (err) *err = "archive " + row->pack + " not available"; return false; }
	const auto k = it->second.idx.find(normPath);
	if (k == it->second.idx.end()) { if (err) *err = "entry missing in " + row->pack; return false; }
	return it->second.ar->read(k->second, out, err);
}

bool VanillaIndex::LoadCached(const std::string& game, const std::string& gameDir, std::string& err)
{
	const std::vector<std::string> arc = FindArchives(game, gameDir);
	if (arc.empty()) { err = "no " + game + " archives found in " + gameDir; return false; }
	const std::string path = CachePath(game, ComputePackset8(arc));
	std::error_code ec;
	if (!fs::exists(fs::u8path(path), ec)) { err = "vanilla index not built yet (" + path + ")"; return false; }
	if (!Load(path, err)) return false;
	SetArchives(arc);
	return true;
}

bool VanillaIndex::BuildCached(const std::string& game, const std::string& gameDir, std::string& cachePath, std::string& err,
                               const std::function<bool(size_t, size_t, const std::string&)>& progress)
{
	const std::vector<std::string> arc = FindArchives(game, gameDir);
	if (arc.empty()) { err = "no " + game + " archives found in " + gameDir; return false; }
	if (!Build(arc, game, err, progress)) return false;
	cachePath = CachePath(game, m_packset);
	return Save(cachePath, err);
}

std::string VanillaIndex::ComputePackset8(const std::vector<std::string>& archives)
{
	pcmod::Sha256 h;
	for (const std::string& a : archives) {
		std::error_code ec;
		const fs::path p = fs::u8path(a);
		const uint64_t sz = fs::is_regular_file(p, ec) ? (uint64_t)fs::file_size(p, ec) : 0;
		const std::string line = FileLabel(a) + "\t" + std::to_string(sz) + "\n";
		h.update(line.data(), line.size());
	}
	return pcmod::ToHex(h.finish()).substr(0, 8);
}

std::string VanillaIndex::CachePath(const std::string& game, const std::string& packset8)
{
	const char* la = std::getenv("LOCALAPPDATA");
	std::error_code ec;
	const fs::path root = la ? fs::path(la) / "Hantei-chan" / "vanilla" : fs::temp_directory_path(ec) / "hantei_vanilla";
	return (root / (game + "-" + packset8 + ".tsv")).u8string();
}

std::vector<std::string> VanillaIndex::FindArchives(const std::string& game, const std::string& dir)
{
	std::vector<std::string> out;
	std::error_code ec;
	const fs::path root = fs::u8path(dir);
	if (game == "uni2") {
		if (fs::is_directory(root / "d", ec)) out.push_back((root / "d").u8string());
	} else if (game == "mbtl") {
		if (fs::is_regular_file(root / "MBTL.exe", ec)) out.push_back((root / "MBTL.exe").u8string());
	} else {
		std::vector<std::string> names;
		std::set<std::string> have;
		for (const auto& it : fs::directory_iterator(root, ec)) {
			if (!it.is_regular_file(ec)) continue;
			std::string n = it.path().filename().u8string();
			std::string l = n;
			for (char& c : l) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
			if (l.size() > 2 && l.compare(l.size() - 2, 2, ".p") == 0) { names.push_back(n); have.insert(l); }
		}
		for (const auto& it : fs::directory_iterator(root, ec)) {
			if (!it.is_regular_file(ec)) continue;
			std::string n = it.path().filename().u8string(), l = n;
			for (char& c : l) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
			if (l.size() > 6 && l.compare(l.size() - 6, 6, ".p.bak") == 0 && !have.count(l.substr(0, l.size() - 4))) names.push_back(n);
		}
		// order by the 4-digit stem so "0002.p.bak" sits between 0001.p and 0003.p
		std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return a.substr(0, a.find('.')) < b.substr(0, b.find('.')); });
		for (const std::string& n : names) out.push_back((root / n).u8string());
	}
	return out;
}

} // namespace hmod
