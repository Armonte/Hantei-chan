#include "hmod_import.h"

#include "hmod_track.h"
#include "../pcmod/pcmod_package.h" // Sha256OfFile

#include <algorithm>
#include <filesystem>
#include <regex>
#include <set>

namespace fs = std::filesystem;

namespace hmod {

namespace {

// Game folders MBAACC reads loose files from.
const char* const kRoots[] = {"data", "se", "bgm", "bg", "grp", "story"};

// "akiha_0_r.ha6" -> "akiha", "m_hisui_m_0_c.txt" -> "m_hisui_m", "p_arc_d_selist.txt" -> "p_arc_d", "s_akiha_1p.ha6" -> "s_akiha".
std::string CharacterStem(std::string name)
{
	const size_t dot = name.rfind('.');
	if (dot != std::string::npos) name.resize(dot);
	static const std::regex tail(R"(_(c|r|selist|\d+[a-z]?)$)");
	for (std::smatch m; std::regex_search(name, m, tail);) name.resize(static_cast<size_t>(m.position(0)));
	return name;
}

} // namespace

void ClassifyInstallPath(const std::string& norm, std::string& assetId, std::string& category, std::string& why)
{
	assetId.clear();
	category.clear();
	why.clear();
	const size_t slash = norm.find('\\');
	const std::string root = slash == std::string::npos ? "" : norm.substr(0, slash);
	const std::string name = norm.substr(norm.rfind('\\') == std::string::npos ? 0 : norm.rfind('\\') + 1);
	if (root.empty()) { why = "outside the game data folders"; return; }
	if (root == "data") {
		if (name.size() > 4 && name.compare(name.size() - 4, 4, ".wmt") == 0) {
			why = "data\\*.wmt is write-only (the game regenerates it from story\\battle\\*_win.txt)";
			return;
		}
		if (name == "_type.txt" || name == "vector.txt" || name == "vectorlist.txt") { assetId = "system-constants"; category = "system.constants"; return; }
		if (name == "charaselect.txt") { assetId = "roster"; category = "character.descriptor"; return; }
		if (norm.find('\\', slash + 1) != std::string::npos) { assetId = "data-other"; category = "other"; return; }
		const std::string stem = CharacterStem(name);
		if (stem.empty()) { assetId = "data-other"; category = "other"; return; }
		assetId = "char-" + stem;
		category = "character.framedata";
		return;
	}
	if (root == "se") { assetId = "audio-se"; category = "audio.se"; return; }
	if (root == "bgm") { assetId = "music"; category = "music"; return; }
	if (root == "bg") { assetId = "stages"; category = "stage"; return; }
	if (root == "story") { assetId = "story"; category = "story"; return; }
	if (root == "grp") { assetId = "graphics"; category = "ui.text"; return; }
	why = "outside the game data folders";
}

bool ImportInstall(Project& p, const std::string& installDir, const VanillaIndex& vanilla, ImportInstallResult& r, std::string& err)
{
	if (p.game != "mbaacc") { err = "install import supports MBAACC installs (UNI2 / MBTL: import the IM Packs folder files one by one)"; return false; }
	const fs::path inst = fs::u8path(installDir);
	std::error_code ec;
	if (!fs::is_directory(inst / "data", ec)) { err = "no data folder in " + installDir; return false; }

	const std::string srcId = "install";
	if (std::none_of(p.sources.begin(), p.sources.end(), [&](const SourceRef& s) { return s.id == srcId; })) {
		SourceRef s;
		s.id = srcId;
		s.kind = "install-import";
		s.path = installDir;
		s.imported = NowIso();
		s.note = "loose files that differ from the vanilla index of this install's .p archives";
		p.sources.push_back(s);
	}

	std::set<std::string> touched;
	for (auto it = fs::directory_iterator(inst, ec); it != fs::directory_iterator(); it.increment(ec)) {
		if (ec) break;
		std::string top = it->path().filename().u8string();
		std::string low = top;
		std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const bool known = std::any_of(std::begin(kRoots), std::end(kRoots), [&](const char* k) { return low == k; });
		if (!it->is_directory(ec)) continue;
		if (!known) { r.notes.push_back("not scanned: " + top + "\\ (not a folder MBAACC reads loose files from)"); continue; }
		for (auto f = fs::recursive_directory_iterator(it->path(), ec); f != fs::recursive_directory_iterator(); f.increment(ec)) {
			if (ec) break;
			if (!f->is_regular_file(ec)) continue;
			const std::string rel = fs::relative(f->path(), inst, ec).generic_u8string();
			std::string relBs = rel;
			std::replace(relBs.begin(), relBs.end(), '/', '\\');
			const std::string norm = NormKey(relBs);
			r.scanned++;
			if (!ClutterReason(norm).empty()) { r.clutter++; continue; }
			std::string assetId, category, why;
			ClassifyInstallPath(norm, assetId, category, why);
			if (assetId.empty()) { r.skipped++; r.notes.push_back("skipped " + relBs + ": " + why); continue; }
			if (const VanillaEntry* v = vanilla.Find(norm)) {
				if (v->sha256 == pcmod::Sha256OfFile(f->path().u8string())) { r.unchanged++; continue; }
			}
			Asset* a = p.FindAsset(assetId);
			if (!a) {
				Asset na;
				na.id = assetId;
				na.category = category;
				na.origin.kind = "install-import";
				na.origin.source = srcId;
				p.assets.push_back(na);
				a = &p.assets.back();
			}
			if (std::find(a->targets.begin(), a->targets.end(), relBs) == a->targets.end()) a->targets.push_back(relBs);
			const fs::path dst = fs::u8path(p.AssetDir(assetId)) / fs::u8path(rel);
			fs::create_directories(dst.parent_path(), ec);
			fs::copy_file(f->path(), dst, fs::copy_options::overwrite_existing, ec);
			if (ec) { err = "copy failed for " + relBs + ": " + ec.message(); return false; }
			touched.insert(assetId);
		}
	}

	// Keep only what would ship: re-encoded files (canonically equal HA6, EOL-only text) go back out.
	for (const TrackedFile& t : TrackProject(p, &vanilla)) {
		if (!touched.count(t.asset)) continue;
		if (Ships(t.status)) { r.imported++; r.perAsset[t.asset]++; continue; }
		if (t.status == Status::Reencoded) { r.reencoded++; r.notes.push_back("dropped " + t.rel + ": " + (t.note.empty() ? "re-encoded only" : t.note)); }
		fs::remove(fs::u8path(t.abs), ec);
		if (Asset* a = p.FindAsset(t.asset)) a->targets.erase(std::remove(a->targets.begin(), a->targets.end(), t.rel), a->targets.end());
	}
	p.assets.erase(std::remove_if(p.assets.begin(), p.assets.end(), [&](const Asset& a) { return touched.count(a.id) && a.targets.empty(); }), p.assets.end());
	std::sort(p.assets.begin(), p.assets.end(), [](const Asset& x, const Asset& y) { return x.id < y.id; });
	for (Asset& a : p.assets) std::sort(a.targets.begin(), a.targets.end());
	return true;
}

} // namespace hmod
