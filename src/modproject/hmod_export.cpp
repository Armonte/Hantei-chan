// See hmod_export.h.
#include "hmod_export.h"
#include "../pcmod/pcmod_package.h"
#include "../pcmod/pcmod_vanilla.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace hmod {

std::string ImFolderName(const std::string& game) { return game == "uni2" ? "UNI2-IM" : game == "mbtl" ? "MBTL-IM" : ""; }

std::string ReportText(const ExportReport& r)
{
	std::string s;
	for (const std::string& e : r.errors) s += "ERROR " + e + "\n";
	for (const Conflict& c : r.conflicts) s += DescribeConflict(c) + "\n";
	for (const std::string& w : r.warnings) s += "WARN  " + w + "\n";
	for (const std::string& n : r.notes) s += "note  " + n + "\n";
	for (const Output& o : r.outputs) s += "wrote " + o.target + " " + o.package + " " + o.version + " -> " + o.path + (o.digest.empty() ? "" : " digest " + o.digest) + "\n";
	s += r.ok ? "EXPORT OK\n" : "EXPORT REFUSED\n";
	return s;
}

namespace {

struct Assembled {
	const Package* pkg = nullptr;
	std::vector<std::string> assetIds;
	std::vector<TrackedFile> files;      // shipping files
};

// Shared first half: which files a package ships, with the per-package structural checks.
bool Assemble(const Project& p, const Package& pkg, const std::vector<TrackedFile>& tracked, Assembled& a, ExportReport& r)
{
	a.pkg = &pkg;
	a.assetIds = ResolvePackageAssets(p, pkg);
	std::set<std::string> seen;
	bool ok = true;
	size_t skippedUnchanged = 0, skippedNoise = 0;
	for (const TrackedFile& t : tracked) {
		if (std::find(a.assetIds.begin(), a.assetIds.end(), t.asset) == a.assetIds.end()) continue;
		if (t.status == Status::Unchanged) { ++skippedUnchanged; continue; }
		if (t.status == Status::Reencoded || t.status == Status::Clutter) { ++skippedNoise; continue; }
		if (t.status == Status::Orphan) { r.warnings.push_back(pkg.id + ": " + t.rel + " is an orphan (" + t.note + "), not shipped"); continue; }
		if (!seen.insert(t.norm).second) { r.errors.push_back(pkg.id + ": " + t.rel + " is provided by two assets"); ok = false; continue; }
		a.files.push_back(t);
	}
	if (skippedUnchanged) r.notes.push_back(pkg.id + ": " + std::to_string(skippedUnchanged) + " unchanged (vanilla) file(s) not shipped");
	if (skippedNoise) r.notes.push_back(pkg.id + ": " + std::to_string(skippedNoise) + " re-encoded / clutter file(s) not shipped");
	for (const std::string& id : a.assetIds) {
		const Asset* as = p.FindAsset(id);
		if (as && as->exportMode != "whole-file")
			r.notes.push_back(pkg.id + ": asset " + id + " export '" + as->exportMode + "' needs pcmod schema 2; shipping whole files (schema 1)");
	}
	if (a.files.empty()) { r.errors.push_back(pkg.id + ": nothing to ship (every file is unchanged, noise or an orphan)"); ok = false; }
	return ok;
}

// requires derived from asset depends across package boundaries (design 6.2), merged with the explicit list.
bool DeriveRequires(const Project& p, const Assembled& a, std::vector<pcmod::Requirement>& req, ExportReport& r)
{
	req = a.pkg->requires_;
	bool ok = true;
	for (const std::string& id : a.assetIds) {
		const Asset* as = p.FindAsset(id);
		if (!as) continue;
		for (const std::string& d : as->depends) {
			if (std::find(a.assetIds.begin(), a.assetIds.end(), d) != a.assetIds.end()) continue;
			const Package* owner = nullptr;
			for (const Package& k : p.packages) {
				const auto ids = ResolvePackageAssets(p, k);
				if (k.id != a.pkg->id && std::find(ids.begin(), ids.end(), d) != ids.end()) { owner = &k; break; }
			}
			if (!owner) { r.errors.push_back(a.pkg->id + ": asset " + id + " depends on " + d + ", which is in no package of this project"); ok = false; continue; }
			if (std::none_of(req.begin(), req.end(), [&](const pcmod::Requirement& q) { return q.id == owner->id; })) {
				req.push_back({owner->id, owner->version});
				r.notes.push_back(a.pkg->id + ": requires " + owner->id + "@" + owner->version + " (asset " + id + " depends on " + d + ")");
			}
		}
	}
	return ok;
}

std::vector<const Package*> Selected(const Project& p, const std::string& id, ExportReport& r)
{
	std::vector<const Package*> v;
	for (const Package& k : p.packages) if (id.empty() || k.id == id) v.push_back(&k);
	if (v.empty()) r.errors.push_back(id.empty() ? "the project has no packages" : "no package '" + id + "' in the project");
	return v;
}

std::vector<PackageView> ProjectViews(const Project& p, const std::vector<TrackedFile>& tracked)
{
	std::vector<PackageView> v;
	for (const Package& k : p.packages) v.push_back(ViewOfProjectPackage(p, k, tracked));
	return v;
}

void Record(Project& p, const Output& o)
{
	for (Output& x : p.outputs) if (x.target == o.target && x.package == o.package) { x = o; return; }
	p.outputs.push_back(o);
}

std::string DigestOfFiles(const std::string& id, const std::string& version, const std::vector<OutputFile>& files)
{
	std::vector<OutputFile> f = files;
	std::sort(f.begin(), f.end(), [](const OutputFile& a, const OutputFile& b) { return a.path < b.path; });
	std::string s = "hmod-pack-1\n" + id + "\n" + version + "\n";
	for (const OutputFile& x : f) s += NormKey(x.path) + std::string(1, '\0') + x.sha256 + "\n";
	return pcmod::Sha256Hex(s);
}

} // namespace

ExportReport Export(Project& p, const ExportOptions& o, const VanillaIndex* v)
{
	return p.game == "mbaacc" ? ExportPcmod(p, o, v) : ExportIm(p, o, v);
}

// ---------------------------------------------------------------- pcmod
ExportReport ExportPcmod(Project& p, const ExportOptions& o, const VanillaIndex* vanilla)
{
	ExportReport r;
	if (p.game != "mbaacc") { r.errors.push_back("pcmod export is for MBAACC projects (this one is " + p.game + ")"); return r; }
	if (const std::string bad = Validate(p); !bad.empty()) { r.errors.push_back("hmod.json: " + bad); return r; }
	const auto pkgs = Selected(p, o.package, r);
	if (pkgs.empty()) return r;

	const std::vector<TrackedFile> tracked = TrackProject(p, vanilla);
	std::string exe = p.base.exeSha256;
	if (exe.empty()) exe = pcmod::DefaultExeSha256(o.gameDir.empty() ? pcmod::DefaultGameDir() : o.gameDir);

	std::vector<PackageView> others;
	if (!o.gameDir.empty()) for (PackageView& v : LoadInstalledPcmod(o.gameDir)) others.push_back(std::move(v));
	for (const std::string& k : o.knownDirs) {
		PackageView v; std::string err;
		if (LoadPackageView(k, v, err)) others.push_back(std::move(v)); else r.warnings.push_back("known package " + k + ": " + err);
	}
	const std::vector<PackageView> own = ProjectViews(p, tracked);

	struct Plan { Assembled a; pcmod::Manifest m; std::vector<pcmod::SourceFile> src; };
	std::vector<Plan> plans;
	bool ok = true;
	for (const Package* pk : pkgs) {
		Plan pl;
		if (!Assemble(p, *pk, tracked, pl.a, r)) { ok = false; continue; }
		pcmod::Manifest& m = pl.m;
		m.id = pk->id; m.version = pk->version; m.name = pk->name.empty() ? (p.name.empty() ? pk->id : p.name + ": " + pk->id) : pk->name;
		m.author = p.author; m.description = pk->description; m.game = "mbaacc"; m.priority = 0;
		m.conflicts = pk->conflicts; m.overrides = pk->overrides; m.modes = pk->modes;
		if (!DeriveRequires(p, pl.a, m.requires_, r)) ok = false;
		size_t sim = 0;
		for (const TrackedFile& t : pl.a.files) {
			std::string norm, why;
			if (!pcmod::NormalisePath(t.rel, norm, why)) { r.errors.push_back(pk->id + ": " + t.rel + ": " + why); ok = false; continue; }
			if (pcmod::IsGameWrittenPath(norm)) { r.warnings.push_back(pk->id + ": skipped " + t.rel + ": " + pcmod::RefusedPathReason(norm)); continue; }
			const pcmod::PathClass pc = pcmod::ClassifyPath(norm);
			if (pc == pcmod::PathClass::Refused) { r.errors.push_back(pk->id + ": " + t.rel + ": " + pcmod::RefusedPathReason(norm)); ok = false; continue; }
			if (pc == pcmod::PathClass::Sim) ++sim;
			pl.src.push_back({t.rel, t.abs});
		}
		if (pk->cls == "sim") m.cls = pcmod::PackageClass::Sim;
		else if (pk->cls == "cosmetic") m.cls = pcmod::PackageClass::Cosmetic;
		else m.cls = sim ? pcmod::PackageClass::Sim : pcmod::PackageClass::Cosmetic;
		if (!exe.empty()) m.exe.push_back(exe);
		if (m.cls == pcmod::PackageClass::Sim && m.exe.empty()) { r.errors.push_back(pk->id + ": a sim package needs an exe hash: set base.exe_sha256 or give --game <install with MBAA.exe>"); ok = false; }
		if (pl.src.empty()) { r.errors.push_back(pk->id + ": no shippable file"); ok = false; }
		if (const std::string bad = pcmod::Validate([&] { pcmod::Manifest t = m; for (const auto& s : pl.src) { std::string n, w; pcmod::NormalisePath(s.rel, n, w); t.files.push_back({s.rel, std::string(64, '0'), 0}); } return t; }()); !bad.empty()) {
			r.errors.push_back(pk->id + ": " + bad); ok = false;
		}
		plans.push_back(std::move(pl));
	}

	// conflict check: against installed / known packages and the project's other packages
	for (const Plan& pl : plans) {
		PackageView mine;
		for (const PackageView& v : own) if (v.id == pl.m.id) mine = v;
		std::vector<PackageView> against;
		for (const PackageView& v : others) if (v.id != pl.m.id) against.push_back(v);   // an installed copy of this very package is its previous version
		for (const PackageView& v : own) if (v.id != pl.m.id) against.push_back(v);
		for (Conflict& c : CheckConflicts(mine, against, vanilla)) r.conflicts.push_back(std::move(c));
	}
	if (AnyBlocking(r.conflicts)) { r.errors.push_back("conflicts with other packages: nothing was written (resolve them, or declare conflicts / overrides in the package)"); ok = false; }
	if (!ok || o.dryRun) { r.ok = ok && o.dryRun; if (o.dryRun && ok) r.notes.push_back("dry run: nothing written"); return r; }

	std::string out = o.outDir;
	if (out.empty()) out = (fs::u8path(p.dir) / "build" / "pkg").u8string();
	for (Plan& pl : plans) {
		const pcmod::PackResult pr = pcmod::Pack(pl.src, pl.m, out, o.writeZip);
		if (!pr.ok) { r.errors.push_back(pl.m.id + ": pack refused: " + pr.error); ok = false; continue; }
		for (const std::string& w : pr.warnings) r.warnings.push_back(pl.m.id + ": " + w);
		Output op;
		op.target = "pcmod"; op.package = pl.m.id; op.version = pl.m.version; op.path = pr.zipPath.empty() ? pr.packageDir : pr.zipPath;
		op.digest = pr.digestHex; op.simDigest = pr.simDigestHex; op.when = NowIso();
		for (const pcmod::FileEntry& f : pr.manifest.files) op.files.push_back({f.path, f.sha256});
		r.outputs.push_back(op);
		Record(p, op);
	}
	r.ok = ok;
	if (ok && o.saveProject) { std::string err; if (!Save(p, err)) r.warnings.push_back("could not record outputs in hmod.json: " + err); }
	return r;
}

// ---------------------------------------------------------------- IM layout
ExportReport ExportIm(Project& p, const ExportOptions& o, const VanillaIndex* vanilla)
{
	ExportReport r;
	const std::string imName = ImFolderName(p.game);
	if (imName.empty()) { r.errors.push_back("mod-folder export is for UNI2 / MBTL projects (this one is " + p.game + ")"); return r; }
	if (const std::string bad = Validate(p); !bad.empty()) { r.errors.push_back("hmod.json: " + bad); return r; }
	if (o.gameDir.empty()) { r.errors.push_back("give the " + p.game + " install folder (the one holding " + imName + ")"); return r; }
	const fs::path imRoot = fs::u8path(o.gameDir) / imName;
	std::error_code ec;
	if (!fs::is_directory(imRoot, ec)) { r.errors.push_back(imName + " is not installed in " + o.gameDir + " (no " + imRoot.u8string() + ")"); return r; }
	const auto pkgs = Selected(p, o.package, r);
	if (pkgs.empty()) return r;

	const std::vector<TrackedFile> tracked = TrackProject(p, vanilla);
	const std::vector<PackageView> own = ProjectViews(p, tracked);
	bool ok = true;
	struct Plan { Assembled a; fs::path dest; std::vector<OutputFile> files; std::string digest; };
	std::vector<Plan> plans;
	for (const Package* pk : pkgs) {
		Plan pl;
		if (!Assemble(p, *pk, tracked, pl.a, r)) { ok = false; continue; }
		std::vector<pcmod::Requirement> req;
		if (!DeriveRequires(p, pl.a, req, r)) ok = false;
		if (!req.empty()) r.notes.push_back(pk->id + ": IM packs have no 'requires'; enable " + req[0].id + " too (IM orders packs by list position)");
		pl.dest = o.imUsePacks ? imRoot / "Packs" / fs::u8path(pk->id) : imRoot / "Mods";
		size_t simish = 0;
		for (const TrackedFile& t : pl.a.files) {
			std::string canon, why;
			if (!pcmod::CanonicalPath(t.rel, canon, why)) { r.errors.push_back(pk->id + ": " + t.rel + ": " + why); ok = false; continue; }
			const std::string n = NormKey(canon);
			if (n == "mod.ini") { r.errors.push_back(pk->id + ": a game file may not be called mod.ini (the IM reads it as the pack description)"); ok = false; continue; }
			if (n.rfind("data\\", 0) == 0 || n.rfind("script\\", 0) == 0) ++simish;
			pl.files.push_back({canon, t.sha256});
		}
		if (simish) r.warnings.push_back(pk->id + ": " + std::to_string(simish) + " file(s) under data\\ / script\\ change the simulation: online, both players need the same files (the IM does not turn them off)");
		pl.digest = DigestOfFiles(pk->id, pk->version, pl.files);

		// overwrite safety: never replace what this project did not write
		const Output* last = nullptr;
		for (const Output& x : p.outputs) if (x.target == (o.imUsePacks ? "im-packs" : "im-mods") && x.package == pk->id) last = &x;
		if (o.imUsePacks && fs::exists(pl.dest, ec)) {
			PackageView old; std::string e;
			bool ours = false;
			std::vector<uint8_t> ini;
			if (pcmod::ReadAll((pl.dest / "mod.ini").u8string(), ini)) { const std::string t(ini.begin(), ini.end()); ours = t.find("Project = " + p.id) != std::string::npos || t.find("Project=" + p.id) != std::string::npos; }
			if (!ours) { r.errors.push_back(pk->id + ": " + pl.dest.u8string() + " already exists and was not written by this project; refusing to touch it (rename the package or remove the folder)"); ok = false; }
		} else if (!o.imUsePacks) {
			for (const OutputFile& f : pl.files) {
				const fs::path d = pl.dest / fs::u8path(f.path);
				if (!fs::is_regular_file(d, ec)) continue;
				const std::string cur = pcmod::Sha256OfFile(d.u8string());
				bool mine = cur == f.sha256;
				if (!mine && last) for (const OutputFile& lf : last->files) if (NormKey(lf.path) == NormKey(f.path) && lf.sha256 == cur) mine = true;
				if (!mine) { r.errors.push_back(pk->id + ": " + d.u8string() + " exists in Mods and was not written by this project; refusing to overwrite it"); ok = false; }
			}
		}
		plans.push_back(std::move(pl));
	}

	for (const Plan& pl : plans) {
		PackageView mine;
		for (const PackageView& v : own) if (v.id == pl.a.pkg->id) mine = v;
		std::vector<PackageView> against = LoadImPackages(imRoot.u8string(), o.imUsePacks ? pl.a.pkg->id : std::string());
		if (!o.imUsePacks) against.erase(std::remove_if(against.begin(), against.end(), [](const PackageView& v) { return v.id.rfind("(Mods", 0) == 0; }), against.end());
		for (const std::string& k : o.knownDirs) { PackageView v; std::string err; if (LoadPackageView(k, v, err)) against.push_back(std::move(v)); }
		for (const PackageView& v : own) if (v.id != pl.a.pkg->id) against.push_back(v);
		for (Conflict& c : CheckConflicts(mine, against, vanilla)) r.conflicts.push_back(std::move(c));
	}
	if (AnyBlocking(r.conflicts)) { r.errors.push_back("conflicts with other packs / mods: nothing was written (resolve them, or declare conflicts in the package)"); ok = false; }
	if (!ok || o.dryRun) { r.ok = ok && o.dryRun; if (o.dryRun && ok) r.notes.push_back("dry run: nothing written"); return r; }

	for (Plan& pl : plans) {
		const Package& pk = *pl.a.pkg;
		const std::string target = o.imUsePacks ? "im-packs" : "im-mods";
		// stale files of this project's previous export of the package
		for (const Output& x : p.outputs)
			if (x.target == target && x.package == pk.id)
				for (const OutputFile& f : x.files)
					if (std::none_of(pl.files.begin(), pl.files.end(), [&](const OutputFile& n) { return NormKey(n.path) == NormKey(f.path); })) {
						const fs::path d = pl.dest / fs::u8path(f.path);
						if (fs::is_regular_file(d, ec) && pcmod::Sha256OfFile(d.u8string()) == f.sha256) fs::remove(d, ec);
					}
		bool wrote = true;
		for (size_t i = 0; i < pl.files.size(); ++i) {
			const TrackedFile* src = nullptr;
			for (const TrackedFile& t : pl.a.files) if (NormKey(t.rel) == NormKey(pl.files[i].path)) { src = &t; break; }
			const fs::path d = pl.dest / fs::u8path(pl.files[i].path);
			fs::create_directories(d.parent_path(), ec);
			fs::remove(d, ec);   // copy_file(overwrite_existing) fails with "File exists" on some mingw runtimes
			fs::copy_file(fs::u8path(src->abs), d, ec);
			if (ec) { r.errors.push_back(pk.id + ": cannot write " + d.u8string() + ": " + ec.message()); wrote = false; break; }
		}
		if (!wrote) { ok = false; continue; }
		if (o.imUsePacks) {
			std::ofstream ini(pl.dest / "mod.ini", std::ios::binary);
			ini << "[Mod]\r\nName = " << (pk.name.empty() ? pk.id : pk.name) << "\r\nAuthor = " << p.author << "\r\nVersion = " << pk.version
			    << "\r\nNote = " << (pk.description.empty() ? "Exported from a Hantei-chan mod project." : pk.description) << "\r\n\r\n[Hmod]\r\nProject = " << p.id
			    << "\r\nPackage = " << pk.id << "\r\nDigest = " << pl.digest << "\r\n";
		}
		Output op;
		op.target = target; op.package = pk.id; op.version = pk.version; op.path = pl.dest.u8string(); op.digest = pl.digest; op.when = NowIso(); op.files = pl.files;
		r.outputs.push_back(op);
		Record(p, op);
	}
	r.ok = ok;
	if (ok && o.saveProject) { std::string err; if (!Save(p, err)) r.warnings.push_back("could not record outputs in hmod.json: " + err); }
	return r;
}

} // namespace hmod
