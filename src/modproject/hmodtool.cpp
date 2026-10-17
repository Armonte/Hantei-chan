// hmodtool: Hantei-chan mod-project CLI (docs/modmanager/AUTHORING_DESIGN.md). Output on stdout is the tool's result.
//   hmodtool index <mbaacc|uni2|mbtl> <gameDir>                 hash the vanilla archives into the shared cache
//   hmodtool new <dir> <id> <game>                              create an empty project
//   hmodtool asset <dir> <id> <category> [--origin <kind>] [--target <glob>]... [--depends <id>]...
//   hmodtool package <dir> <id> --assets <id|glob>[,...] [--version x.y.z] [--class sim|cosmetic]
//   hmodtool import <dir> <asset> <file> [<game-relative path>]   copy a working file into the asset
//   hmodtool import-install <dir> <id> <install> [--vanilla-dir <dir>]  a modded install's changed loose files as assets
//   hmodtool status <dir> --game-dir <install>                  change tracking against the vanilla index
//   hmodtool export <dir> --game-dir <install> [--package <id>] [--out <dir>] [--known <pkgdir>]... [--no-zip] [--dry-run] [--mods]
#include "hmod_export.h"
#include "hmod_import.h"

#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hmod;

static std::string Utf8(const wchar_t* w)
{
	const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 1, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
	s.pop_back();
	return s;
}
static void Out(const std::string& s) { std::fwrite(s.data(), 1, s.size(), stdout); }
static void Err(const std::string& s) { std::fwrite(s.data(), 1, s.size(), stderr); }

static int Usage()
{
	Err("usage:\n"
		"  hmodtool index <mbaacc|uni2|mbtl> <gameDir>\n"
		"  hmodtool new <dir> <id> <game>\n"
		"  hmodtool asset <dir> <id> <category> [--origin <kind>] [--target <glob>]... [--depends <id>]...\n"
		"  hmodtool package <dir> <id> --assets <id|glob>[,...] [--version x.y.z] [--class sim|cosmetic]\n"
		"  hmodtool import <dir> <asset> <file> [<game-relative path>]\n"
		"  hmodtool import-install <dir> <id> <install> [--vanilla-dir <unmodded install>]\n"
		"  hmodtool status <dir> --game-dir <install>\n"
		"  hmodtool export <dir> --game-dir <install> [--package <id>] [--out <dir>] [--known <pkgdir>]... [--no-zip] [--dry-run] [--mods]\n");
	return 2;
}

static bool Opt(const std::vector<std::string>& a, size_t& i, const char* name, std::string& v)
{
	if (a[i] != name) return false;
	if (i + 1 >= a.size()) { Err(std::string(name) + " needs a value\n"); std::exit(2); }
	v = a[++i];
	return true;
}

int wmain(int argc, wchar_t** argv)
{
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::string> a;
	for (int i = 1; i < argc; ++i) a.push_back(Utf8(argv[i]));
	if (a.empty()) return Usage();
	const std::string cmd = a[0];
	a.erase(a.begin());
	std::string err;

	if (cmd == "index" && a.size() == 2) {
		VanillaIndex v; std::string cache;
		auto prog = [](size_t d, size_t t, const std::string& l) { if (d % 2048 == 0) Err(l + " " + std::to_string(d) + "/" + std::to_string(t) + "\n"); return true; };
		if (!v.BuildCached(a[0], a[1], cache, err, prog)) { Err("index failed: " + err + "\n"); return 1; }
		Out("indexed " + std::to_string(v.size()) + " files, packset " + v.packset8() + "\nwrote " + cache + "\n");
		return 0;
	}
	if (cmd == "import-install" && a.size() >= 3) {
		// <dir> <id> <install> [--vanilla-dir <unmodded install>]: the index comes from the install's own archives
		// unless they were modified too.
		std::string vdir = a[2];
		for (size_t i = 3; i < a.size(); ++i) {
			std::string v;
			if (Opt(a, i, "--vanilla-dir", v)) vdir = v;
			else return Usage();
		}
		Project p;
		if (!Load(a[0], p, err) && !Create(a[0], a[1], "mbaacc", p, err)) { Err(err + "\n"); return 1; }
		VanillaIndex v;
		if (!v.LoadCached("mbaacc", vdir, err)) {
			std::string cache;
			Err("building the vanilla index of " + vdir + "\n");
			if (!v.BuildCached("mbaacc", vdir, cache, err)) { Err("index failed: " + err + "\n"); return 1; }
		}
		ImportInstallResult r;
		if (!ImportInstall(p, a[2], v, r, err)) { Err(err + "\n"); return 1; }
		if (!Save(p, err)) { Err(err + "\n"); return 1; }
		for (const auto& n : r.notes) Out("note  " + n + "\n");
		for (const auto& [id, n] : r.perAsset) Out("asset " + id + ": " + std::to_string(n) + " file(s)\n");
		Out("scanned " + std::to_string(r.scanned) + ", imported " + std::to_string(r.imported) + ", unchanged " + std::to_string(r.unchanged) +
			", re-encoded " + std::to_string(r.reencoded) + ", clutter " + std::to_string(r.clutter) + ", skipped " + std::to_string(r.skipped) + "\n");
		return 0;
	}
	if (cmd == "new" && a.size() == 3) {
		Project p;
		if (!Create(a[0], a[1], a[2], p, err)) { Err(err + "\n"); return 1; }
		Out("created " + a[0] + "\\hmod.json\n");
		return 0;
	}
	if (a.empty()) return Usage();
	Project p;
	if (!Load(a[0], p, err)) { Err(err + "\n"); return 1; }

	if (cmd == "asset" && a.size() >= 3) {
		Asset x; x.id = a[1]; x.category = a[2];
		for (size_t i = 3; i < a.size(); ++i) {
			std::string v;
			if (Opt(a, i, "--origin", v)) x.origin.kind = v;
			else if (Opt(a, i, "--target", v)) x.targets.push_back(v);
			else if (Opt(a, i, "--depends", v)) x.depends.push_back(v);
			else return Usage();
		}
		p.assets.push_back(x);
		if (!Save(p, err)) { Err(err + "\n"); return 1; }
		std::error_code ec; fs::create_directories(fs::u8path(p.AssetDir(x.id)), ec);
		Out("added asset " + x.id + "\n");
		return 0;
	}
	if (cmd == "package" && a.size() >= 2) {
		Package k; k.id = a[1];
		for (size_t i = 2; i < a.size(); ++i) {
			std::string v;
			if (Opt(a, i, "--assets", v)) { size_t s = 0; while (s <= v.size()) { const size_t e = v.find(',', s); k.assets.push_back(v.substr(s, e == std::string::npos ? std::string::npos : e - s)); if (e == std::string::npos) break; s = e + 1; } }
			else if (Opt(a, i, "--version", v)) k.version = v;
			else if (Opt(a, i, "--class", v)) k.cls = v;
			else return Usage();
		}
		p.packages.push_back(k);
		if (!Save(p, err)) { Err(err + "\n"); return 1; }
		Out("added package " + k.id + "\n");
		return 0;
	}
	if (cmd == "import" && a.size() >= 3) {
		const Asset* as = p.FindAsset(a[1]);
		if (!as) { Err("no asset " + a[1] + "\n"); return 1; }
		std::string rel = a.size() > 3 ? a[3] : fs::u8path(a[2]).filename().u8string();
		std::error_code ec;
		const fs::path d = fs::u8path(p.AssetDir(a[1])) / fs::u8path(rel);
		fs::create_directories(d.parent_path(), ec);
		fs::copy_file(fs::u8path(a[2]), d, fs::copy_options::overwrite_existing, ec);
		if (ec) { Err("copy failed: " + ec.message() + "\n"); return 1; }
		Out("imported " + rel + "\n");
		return 0;
	}
	if (cmd == "status" || cmd == "export") {
		std::string gameDir, outDir, pkg;
		ExportOptions o;
		for (size_t i = 1; i < a.size(); ++i) {
			std::string v;
			if (Opt(a, i, "--game-dir", v)) gameDir = v;
			else if (Opt(a, i, "--out", v)) o.outDir = v;
			else if (Opt(a, i, "--package", v)) o.package = v;
			else if (Opt(a, i, "--known", v)) o.knownDirs.push_back(v);
			else if (a[i] == "--no-zip") o.writeZip = false;
			else if (a[i] == "--dry-run") o.dryRun = true;
			else if (a[i] == "--mods") o.imUsePacks = false;
			else return Usage();
		}
		o.gameDir = gameDir;
		VanillaIndex v;
		bool have = !gameDir.empty() && v.LoadCached(p.game, gameDir, err);
		if (!have) Out("note  vanilla index unavailable (" + err + "): run 'hmodtool index'; files are treated as changed\n");
		if (cmd == "status") {
			const auto t = TrackProject(p, have ? &v : nullptr);
			for (const TrackedFile& f : t) Out(std::string(StatusName(f.status)) + "\t" + f.asset + "\t" + f.rel + "\t" + f.note + "\n");
			const StatusCounts c = Count(t);
			Out("unchanged " + std::to_string(c.unchanged) + ", re-encoded " + std::to_string(c.reencoded) + ", changed " + std::to_string(c.changed) + ", added " + std::to_string(c.added) +
				", orphan " + std::to_string(c.orphan) + ", clutter " + std::to_string(c.clutter) + "\n");
			return 0;
		}
		const ExportReport r = Export(p, o, have ? &v : nullptr);
		Out(ReportText(r));
		return r.ok ? 0 : 1;
	}
	return Usage();
}
