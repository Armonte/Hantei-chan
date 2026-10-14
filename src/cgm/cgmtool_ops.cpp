// cgmtool: scriptable structural edits, cgtool folder workflows, project-wide references, palette layouts and CC palette export.
// Every edit writes a NEW bank with -o, or rewrites the bank in place with --in-place (the original kept once as <bank>.bak);
// --refs (with --in-place) rewrites the sprite references of EVERY project that draws the bank (cgm_project.h).
#include "cgm_bank.h"
#include "cgm_ccpal.h"
#include "cgm_export.h"
#include "cgm_folder.h"
#include "cgm_ops.h"
#include "cgm_palette.h"
#include "cgm_project.h"
#include "cgm_struct.h"
#include "cgm_usage.h"
#include "../framedata.h"
#include "../png_writer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

bool ReadFile(const char *p, std::vector<uint8_t> &b);
bool WriteFile(const std::string &p, const std::vector<uint8_t> &b);
bool ParseFile(const std::string &p, std::vector<uint8_t> &b, cgm::Bank &bk, std::string &err);
std::vector<uint8_t> Sprite(int w, int h, int mode);
bool SameDecoded(const cgm::Bank &a, int ia, const cgm::Bank &b, int ib);
bool AtlasUnique(const cgm::Bank &b);
bool EngineAgrees(const cgm::Bank &b, std::string &why);
bool ArgVal(int argc, char **argv, const char *k, std::string &v);

namespace fs = std::filesystem;

static bool Flag(int argc, char **argv, const char *k) { for (int i = 0; i < argc; i++) if (!strcmp(argv[i], k)) return true; return false; }
static std::string U(const std::string &p) { return p; }   // command line paths are taken as UTF-8 / ASCII

static bool WriteBankFile(const std::string &path, const cgm::Bank &bk, bool backup) {
	std::vector<uint8_t> o; bk.serialize(o);
	std::error_code ec; const fs::path t(Utf8ToWide(path)); fs::path bak = t; bak += L".bak";
	if (backup && fs::exists(t, ec) && !fs::exists(bak, ec)) fs::copy_file(t, bak, ec);
	return WriteFile(path, o);
}

// -o out | --in-place [--refs]
static int Finish(const std::string &bankPath, const cgm::Bank &bk, const std::vector<int> &remap, int argc, char **argv) {
	std::string out; const bool inPlace = Flag(argc, argv, "--in-place"), refs = Flag(argc, argv, "--refs");
	ArgVal(argc, argv, "-o", out);
	if (!inPlace && out.empty()) { puts("-o <out.cg> or --in-place is required"); return 2; }
	if (refs && !inPlace) { puts("--refs rewrites the projects' HA6 files to match the bank, so it needs --in-place (the bank they load)"); return 2; }
	const std::string target = inPlace ? bankPath : out;
	if (!WriteBankFile(target, bk, inPlace)) { printf("could not write %s\n", target.c_str()); return 1; }
	printf("wrote %s (%zu images)%s\n", target.c_str(), bk.images.size(), inPlace ? " (original kept as .bak)" : "");
	if (remap.empty() || cgm::IsIdentityRemap(remap)) return 0;
	const cgm::BankUsers users = cgm::FindBankUsers(bankPath);
	if (!refs) {
		printf("ids changed: %zu project file(s) draw this bank and still use the old ids:", users.files.size());
		for (auto &f : users.files) printf(" %s%s", fs::path(Utf8ToWide(f.path)).filename().string().c_str(), f.shared ? "(shared)" : "");
		printf("\n  (run with --in-place --refs to rewrite them)\n");
		return 0;
	}
	cgm::DiskRemapResult r; const bool ok = cgm::RemapUserFiles(users, remap, true, r);
	printf("references: %d rewritten, %d cleared (deleted images) in %zu file(s); %zu unchanged; %zu shared file(s) not touched\n", r.refsChanged, r.refsCleared, r.written.size(), r.unchanged.size(), r.sharedSkipped.size());
	for (auto &w : r.written) printf("  rewrote %s (original kept as .bak)\n", w.c_str());
	for (auto &s : r.sharedSkipped) printf("  shared, not rewritten: %s (its patterns are re-saved by the character that loads it)\n", s.c_str());
	for (auto &e : r.errors) printf("  error: %s\n", e.c_str());
	return ok ? 0 : 1;
}

static bool LoadBank(const char *p, cgm::Bank &bk) {
	std::vector<uint8_t> b; std::string e;
	if (!ParseFile(p, b, bk, e)) { printf("error: %s: %s\n", p, e.c_str()); return false; }
	return true;
}
static std::vector<int> IdList(const std::string &s) {
	std::vector<int> v; size_t i = 0;
	while (i < s.size()) {
		size_t j = s.find(',', i); if (j == std::string::npos) j = s.size();
		const std::string t = s.substr(i, j - i); const size_t d = t.find('-');
		if (d != std::string::npos && d > 0) { for (int k = atoi(t.c_str()), e = atoi(t.c_str() + d + 1); k <= e; k++) v.push_back(k); }
		else if (!t.empty()) v.push_back(atoi(t.c_str()));
		i = j + 1;
	}
	return v;
}

static int CmdUsers(int argc, char **argv) {
	if (argc < 1) return 2;
	const cgm::BankUsers u = cgm::FindBankUsers(U(argv[0]));
	printf("%zu project(s), %zu HA6 file(s) (%d rewritable, %zu shared)\n", u.projects.size(), u.files.size(), u.ownFileCount(), u.files.size() - u.ownFileCount());
	for (auto &p : u.projects) { printf("  project %s:", fs::path(Utf8ToWide(p.path)).filename().string().c_str()); for (auto &d : p.dataFiles) printf(" %s", d.c_str()); printf("\n"); }
	for (auto &f : u.files) printf("  file %s%s%s, used by %zu project(s)\n", f.path.c_str(), f.shared ? " [shared: never rewritten]" : "", f.unlisted ? " [in no .txt, named after the bank]" : "", f.projects.size());
	return 0;
}

// usage <bank> : layers per image over every project; with --stack <txt>, the images that stack alone would call unused
static int CmdUsage(int argc, char **argv) {
	if (argc < 1) return 2;
	cgm::Bank bk; if (!LoadBank(argv[0], bk)) return 1;
	const cgm::BankUsers u = cgm::FindBankUsers(U(argv[0]));
	const cgm::ProjectUsage pu = cgm::CountProjectUsage(u, (int)bk.images.size());
	int used = 0, unused = 0; long layers = 0;
	for (size_t i = 0; i < bk.images.size(); i++) { if (!bk.images[i].drawable()) continue; (pu.count((int)i) ? used : unused)++; layers += pu.count((int)i); }
	printf("project-wide: %d image(s) used by %ld layer(s) over %zu file(s); %d drawable image(s) unused\n", used, layers, u.files.size(), unused);
	std::string st;
	if (ArgVal(argc, argv, "--stack", st)) {
		cgm::ProjectTxt t; if (!cgm::ParseProjectTxt(st, t)) { printf("error: %s is not a project .txt\n", st.c_str()); return 1; }
		std::vector<int> stackCount(bk.images.size(), 0);
		for (auto &p : t.dataPaths) { const int fi = u.fileIndex(p); if (fi < 0) continue; for (size_t i = 0; i < stackCount.size() && i < pu.perFile[fi].size(); i++) stackCount[i] += pu.perFile[fi][i]; }
		int falseUnused = 0; for (size_t i = 0; i < bk.images.size(); i++) if (bk.images[i].drawable() && !stackCount[i] && pu.count((int)i)) falseUnused++;
		printf("stack %s alone: %d image(s) look unused but other projects draw them (a per-stack 'delete unused' would break them)\n", st.c_str(), falseUnused);
	}
	for (auto &e : pu.errors) printf("  error reading %s\n", e.c_str());
	return 0;
}

static bool ReadPng(const std::string &p, std::vector<uint8_t> &px, int &w, int &h, std::vector<uint8_t> &idx, uint32_t pal[256]) {
	std::string e; if (!cgm::ReadFolderPng(p, px, w, h, &idx, pal, &e)) { printf("error: %s\n", e.c_str()); return false; }
	return true;
}

static int CmdStruct(const std::string &c, int argc, char **argv) {
	if (argc < 2 && c != "delete-unused") { printf("usage: cgmtool %s <bank.cg> ... (-o out.cg | --in-place [--refs])\n", c.c_str()); return 2; }
	cgm::Bank bk; if (!LoadBank(argv[0], bk)) return 1;
	std::vector<int> remap; std::string e, v;
	if (c == "insert") {   // insert <bank> <png> [--at N] [--name x.bmp]: a canvas-sized PNG (content box = bounds) or any picture
		std::vector<uint8_t> px, idx; uint32_t pal[256]; int w, h; if (!ReadPng(argv[1], px, w, h, idx, pal)) return 1;
		int at = -1; if (ArgVal(argc, argv, "--at", v)) at = atoi(v.c_str());
		std::string name = fs::path(argv[1]).stem().string() + ".bmp"; ArgVal(argc, argv, "--name", name);
		int id = -1; if (!cgm::AddCanvasImage(bk, at, name, -99, px.data(), w, h, idx.empty() ? nullptr : &idx, idx.empty() ? nullptr : pal, remap, &id, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		printf("inserted image %d (type %d, %dx%d at %d,%d)\n", id, bk.images[id].type, bk.images[id].boundsW(), bk.images[id].boundsH(), bk.images[id].x1, bk.images[id].y1);
	} else if (c == "delete") {   // delete <bank> <ids: 3,5,10-12>
		std::vector<int> ids = IdList(argv[1]); std::sort(ids.begin(), ids.end()); ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
		std::vector<int> cur = cgm::IdentityRemap(bk.images.size());
		for (int k = (int)ids.size() - 1; k >= 0; k--) { std::vector<int> r; if (ids[k] < 0 || ids[k] >= (int)cur.size() || !cgm::DeleteImage(bk, cur[ids[k]], true, r, &e)) { printf("error: delete %d: %s\n", ids[k], e.c_str()); return 1; } for (int &x : cur) if (x >= 0) x = r[x]; }
		remap = cur; printf("deleted %zu image(s)\n", ids.size());
	} else if (c == "move") {   // move <bank> <from> <to>
		if (argc < 3) return 2;
		if (!cgm::MoveImage(bk, atoi(argv[1]), atoi(argv[2]), remap, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		printf("moved image %s to %s\n", argv[1], argv[2]);
	} else if (c == "delete-unused") {   // every drawable image no project draws (and no other image borrows cells of)
		const cgm::BankUsers u = cgm::FindBankUsers(U(argv[0]));
		if (u.files.empty() && !Flag(argc, argv, "--no-projects")) { puts("error: no project .txt next to the bank names it: usage cannot be counted (pass --no-projects to delete every image anyway)"); return 1; }
		const cgm::ProjectUsage pu = cgm::CountProjectUsage(u, (int)bk.images.size());
		if (!pu.errors.empty()) { printf("error: could not read %s: refusing to call anything unused\n", pu.errors[0].c_str()); return 1; }
		auto at = bk.buildAtlas(); std::vector<int> t;
		for (int i = 0; i < (int)bk.images.size(); i++) if (bk.images[i].drawable() && !pu.count(i) && bk.dependants(i, at).empty()) t.push_back(i);
		std::vector<int> cur = cgm::IdentityRemap(bk.images.size());
		for (int k = (int)t.size() - 1; k >= 0; k--) { std::vector<int> r; if (!cgm::DeleteImage(bk, cur[t[k]], true, r, &e)) { printf("error: %s\n", e.c_str()); return 1; } for (int &x : cur) if (x >= 0) x = r[x]; }
		remap = cur; printf("deleted %zu image(s) no project draws (usage counted over %zu file(s))\n", t.size(), u.files.size());
	} else if (c == "replace") {   // replace <bank> <id> <png> [--anchor bottom|topleft]: any size; a canvas-sized PNG keeps its own position
		if (argc < 3) return 2;
		const int id = atoi(argv[1]); std::vector<uint8_t> px, idx; uint32_t pal[256]; int w, h; if (!ReadPng(argv[2], px, w, h, idx, pal)) return 1;
		if (id < 0 || id >= (int)bk.images.size() || !bk.images[id].present) { puts("error: no such image"); return 1; }
		cgm::Image &im = bk.images[id];
		if (im.drawable() && w == im.boundsW() && h == im.boundsH()) { cgm::ReplaceReport rr; if (!cgm::ReplaceImage(bk, id, px.data(), w, h, &e, &rr)) { printf("error: %s\n", e.c_str()); return 1; } printf("replaced in place (%d pixels)\n", rr.changedPixels); }
		else {
			int x1 = im.x1, y1 = im.y1; std::string anchor = "bottom"; ArgVal(argc, argv, "--anchor", anchor);
			std::vector<uint8_t> use = px; int uw = w, uh = h;
			if (w == im.w && h == im.h) {   // canvas-sized: the content box is the new bounds
				int a, b, cc, d; if (!cgm::TrimToContent(px.data(), w, h, a, b, cc, d)) { puts("error: the PNG is empty"); return 1; }
				uw = cc - a + 1; uh = d - b + 1; use.assign((size_t)uw * uh * 4, 0);
				for (int y = 0; y < uh; y++) memcpy(&use[(size_t)y * uw * 4], &px[((size_t)(y + b) * w + a) * 4], (size_t)uw * 4);
				x1 = a; y1 = b;
			} else if (anchor == "bottom") { x1 = im.x1 + im.boundsW() / 2 - w / 2; y1 = im.y2 + 1 - h; }   // feet stay where they were
			if (x1 < 0 || y1 < 0) { printf("error: the new sprite would start at %d,%d, outside the canvas\n", x1, y1); return 1; }
			std::vector<int> un; if (!cgm::ReplaceImageResized(bk, id, use.data(), uw, uh, x1, y1, &un, &e)) { printf("error: %s\n", e.c_str()); return 1; }
			printf("replaced with a %dx%d sprite at %d,%d (canvas %dx%d); %zu image(s) made independent\n", uw, uh, x1, y1, bk.images[id].w, bk.images[id].h, un.size());
		}
	} else if (c == "sync") {   // sync <bank> <dir>: the folder decides the images and their order
		cgm::FolderSyncReport r; if (!cgm::SyncBankToFolder(bk, argv[1], remap, r)) { printf("error: %s\n", r.error.c_str()); return 1; }
		printf("folder sync: kept %d, added %d, removed %d, moved %d; pixels: %zu replaced, %zu re-cut, %d unchanged\n", r.kept, r.added, r.removed, r.moved, r.pixels.replaced.size(), r.pixels.resized.size(), r.pixels.unchanged);
		for (auto &w : r.pixels.warnings) printf("  warning: %s\n", w.c_str());
	} else if (c == "import-plain") {   // import-plain <bank> <dir>: a cgtool export without manifest, ids unchanged
		cgm::FolderImportReport r; if (!cgm::ImportPlainFolder(bk, argv[1], r)) { printf("error: %s\n", r.error.c_str()); return 1; }
		printf("plain import: %zu replaced in place, %zu re-cut, %d unchanged\n", r.replaced.size(), r.resized.size(), r.unchanged);
		for (auto &w : r.warnings) printf("  warning: %s\n", w.c_str());
	} else return -1;
	return Finish(U(argv[0]), bk, remap, argc, argv);
}

// build <dir | png...> -o out.cg [--pal file[:N]]
static int CmdBuild(int argc, char **argv) {
	std::string out; if (!ArgVal(argc, argv, "-o", out) || argc < 1) { puts("usage: cgmtool build <folder | a.png b.png ...> -o out.cg [--pal file.pal[:N]]"); return 2; }
	cgm::BuildOptions o; std::string pal;
	if (ArgVal(argc, argv, "--pal", pal)) { const size_t c = pal.rfind(':'); if (c != std::string::npos && c > 1 && isdigit((unsigned char)pal[c + 1])) { o.palIndex = atoi(pal.c_str() + c + 1); pal.resize(c); } o.palettePath = pal; }
	std::vector<std::string> files;
	for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--pal")) { i++; continue; } files.push_back(argv[i]); }
	cgm::Bank bk; cgm::BuildReport r;
	const bool ok = files.size() == 1 && fs::is_directory(fs::path(Utf8ToWide(files[0]))) ? cgm::BuildBankFromFolder(files[0], o, bk, r) : cgm::BuildBankFromFiles(files, o, bk, r);
	if (!ok) { printf("error: %s\n", r.error.c_str()); return 1; }
	std::vector<uint8_t> bytes; bk.serialize(bytes);
	cgm::Bank again; std::string e; if (!cgm::Bank::parse(bytes.data(), bytes.size(), again, &e)) { printf("error: the new bank does not re-parse: %s\n", e.c_str()); return 1; }
	if (!WriteFile(out, bytes)) { puts("could not write the output"); return 1; }
	printf("built %s: %d image(s) (%d 8-bit on the bank palette), %d empty placeholder(s), %d atlas page(s), %zu bytes\n", out.c_str(), r.images, r.indexed, r.placeholders, (int)bk.H[0] + 1, bytes.size());
	return 0;
}

// ---- proofs on real data ----
static bool CopyTree(const cgm::BankUsers &u, const std::string &bank, const std::string &tmp, std::map<std::string, std::string> &map, std::string &newBank) {
	// copies the bank, its projects and their HA6 files into tmp, keeping the relative layout (../BaseData.HA6 too)
	std::error_code ec; fs::remove_all(fs::path(Utf8ToWide(tmp)), ec);
	const fs::path bdir = fs::path(Utf8ToWide(bank)).parent_path();
	const fs::path root = fs::path(Utf8ToWide(tmp)) / "data" / bdir.filename();
	fs::create_directories(root, ec);
	auto cp = [&](const std::string &src) -> std::string {
		const fs::path s(Utf8ToWide(src)); const fs::path rel = s.lexically_normal().lexically_relative(bdir);
		const fs::path d = (root / rel).lexically_normal(); fs::create_directories(d.parent_path(), ec);
		if (fs::exists(s, ec)) fs::copy_file(s, d, fs::copy_options::overwrite_existing, ec);
		map[cgm::NormPath(WideToUtf8(d.wstring()))] = src;
		return WideToUtf8(d.wstring());
	};
	newBank = cp(bank);
	for (auto &p : u.projects) cp(p.path);
	for (auto &f : u.files) cp(f.path);
	return true;
}

static std::vector<std::vector<uint8_t>> ReadAll(const cgm::BankUsers &u) { std::vector<std::vector<uint8_t>> v; for (auto &f : u.files) { std::vector<uint8_t> b; ReadFile(f.path.c_str(), b); v.push_back(std::move(b)); } return v; }

// project-refs-check <bank.cg> [--tmp dir]: on a copy of every project that draws the bank: insert at 0, move, delete one used and one
// unused image with the project-wide remap; afterwards every layer of every rewritable file shows the same picture (or none, for the
// deleted one), shared files are untouched, and undoing the remap gives every file back byte-identical.
static int CmdProjectRefsCheck(int argc, char **argv) {
	if (argc < 1) return 2;
	std::string tmp = "cgm_proj_tmp"; ArgVal(argc, argv, "--tmp", tmp);
	cgm::Bank bk; if (!LoadBank(argv[0], bk)) return 1;
	const cgm::BankUsers u0 = cgm::FindBankUsers(U(argv[0]));
	if (u0.files.empty()) { printf("FAIL %s: no project draws the bank\n", argv[0]); return 1; }
	std::map<std::string, std::string> map; std::string nb; CopyTree(u0, U(argv[0]), tmp, map, nb);
	const cgm::BankUsers u = cgm::FindBankUsers(nb);
	bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[0], m.c_str()); bad = true; };
	if (u.files.size() != u0.files.size() || u.projects.size() != u0.projects.size()) F("the copy does not see the same projects");
	const auto orig = ReadAll(u);
	const cgm::ProjectUsage pu = cgm::CountProjectUsage(u, (int)bk.images.size());
	struct Ref { int file, p, f, l, id; };
	auto refsOf = [&](std::vector<Ref> &out) {
		out.clear();
		for (size_t fi = 0; fi < u.files.size(); fi++) {
			FrameData fd; if (!fd.load(u.files[fi].path.c_str())) continue;
			for (int p = 0; p < fd.get_sequence_count(); p++) { Sequence *s = fd.get_sequence(p); if (!s) continue;
				for (int f = 0; f < (int)s->frames.size(); f++) for (int l = 0; l < (int)s->frames[f].AF.layers.size(); l++) { auto &L = s->frames[f].AF.layers[l]; if (!L.usePat && L.spriteId >= 0) out.push_back({(int)fi, p, f, l, L.spriteId}); } }
		}
	};
	std::vector<Ref> before; refsOf(before);
	// the edit: insert a sprite at 0, move image 5 to N/2, delete one used image (the least used) and one unused
	cgm::Bank w = bk; std::vector<int> total = cgm::IdentityRemap(bk.images.size()); std::string e;
	auto compose = [&](const std::vector<int> &r) { for (int &x : total) if (x >= 0) x = r[x]; };
	{ std::vector<int> r; int id; auto spr = Sprite(40, 40, 0); cgm::NewImageSpec sp; if (!cgm::AddImage(w, 0, sp, spr.data(), 40, 40, r, &id, &e)) F("AddImage: " + e); else compose(r); }
	{ std::vector<int> r; const int from = std::min(6, (int)w.images.size() - 1), to = (int)w.images.size() / 2;   // tiny banks (csel portraits): move what exists
		if (from != to && !cgm::MoveImage(w, from, to, r, &e)) F("MoveImage: " + e); else if (from != to) compose(r); }
	int usedVictim = -1, unusedVictim = -1, best = 1 << 30;
	auto at = bk.buildAtlas();
	for (int i = 0; i < (int)bk.images.size(); i++) if (bk.images[i].drawable() && bk.dependants(i, at).empty()) { const int c = pu.count(i); if (c && c < best) { best = c; usedVictim = i; } if (!c && unusedVictim < 0) unusedVictim = i; }
	for (int v : {usedVictim, unusedVictim}) if (v >= 0) { std::vector<int> r; if (!cgm::DeleteImage(w, total[v], true, r, &e)) F("DeleteImage: " + e); else compose(r); }
	cgm::DiskRemapResult rr; if (!cgm::RemapUserFiles(u, total, false, rr)) F("RemapUserFiles: " + (rr.errors.empty() ? std::string() : rr.errors[0]));
	std::vector<Ref> after; refsOf(after);
	// every reference: same file / pattern / frame / layer; picture identical, or cleared for the deleted used image
	size_t checked = 0, cleared = 0;
	std::map<std::tuple<int, int, int, int>, int> now; for (auto &r : after) now[{r.file, r.p, r.f, r.l}] = r.id;
	for (const Ref &r : before) {
		const bool shared = u.files[r.file].shared;
		auto it = now.find({r.file, r.p, r.f, r.l});
		const bool dangling = r.id >= (int)total.size();   // shipped layers may name an id past the bank (draws nothing); RemapHa6File leaves those as they are
		const int want = (shared || dangling) ? r.id : total[r.id];
		if (want < 0) { if (it != now.end()) F("a reference to the deleted image survived"); cleared++; continue; }
		if (it == now.end() || it->second != want) { F("layer not remapped in " + u.files[r.file].path + " (pattern " + std::to_string(r.p) + " frame " + std::to_string(r.f) + " layer " + std::to_string(r.l) + ": id " + std::to_string(r.id) + " -> want " + std::to_string(want) + ", got " + (it == now.end() ? std::string("none") : std::to_string(it->second)) + ", bank has " + std::to_string(bk.images.size()) + ")"); break; }
		if (!shared && !dangling && !SameDecoded(bk, r.id, w, want)) { F("a layer now shows a different picture"); break; }
		checked++;
	}
	for (size_t fi = 0; fi < u.files.size(); fi++) if (u.files[fi].shared) { std::vector<uint8_t> b; ReadFile(u.files[fi].path.c_str(), b); if (b != orig[fi]) F("a shared file was rewritten: " + u.files[fi].path); }
	// undo: the inverse remap, then the deleted image's references come back by re-reading the originals (cleared refs are not invertible;
	// the window's undo keeps them, here the check compares against the original bytes for files without the deleted image)
	std::vector<int> inv = cgm::InvertRemap(total, w.images.size());
	cgm::DiskRemapResult rr2; cgm::RemapUserFiles(u, inv, false, rr2);
	int identical = 0, differ = 0;
	std::set<int> filesWithDeleted; for (const Ref &r : before) if (r.id < (int)total.size() && total[r.id] < 0) filesWithDeleted.insert(r.file);
	const auto back = ReadAll(u);
	for (size_t fi = 0; fi < u.files.size(); fi++) { if (filesWithDeleted.count((int)fi)) continue; if (back[fi] == orig[fi]) identical++; else { differ++; F("remap -> inverse is not byte-identical: " + u.files[fi].path); } }
	std::error_code ec; fs::remove_all(fs::path(Utf8ToWide(tmp)), ec);
	printf("SECTION cgm-project-refs: %s | %s: %zu projects, %zu files (%d rewritable), %zu layer refs: %zu same picture, %zu cleared (deleted image %d); %zu files rewritten; inverse byte-identical %d/%d\n",
	       bad ? "FAIL" : "pass", fs::path(argv[0]).filename().string().c_str(), u.projects.size(), u.files.size(), u.ownFileCount(), before.size(), checked, cleared, usedVictim, rr.written.size(), identical, identical + differ);
	return bad ? 1 : 0;
}

// folder-check <bank.cg> [--tmp dir]: cgtool workflows on real data. (1) export --cgtool -> import-plain unchanged is byte-identical;
// (2) export -> sync unchanged is byte-identical; (3) delete a file, add a file at 2.5, rename one to move it -> the remap and the pixels agree;
// (4) build a new bank from the export -> every image renders as in the source bank; (5) a bigger replacement keeps the frame anchor.
static int CmdFolderCheck(int argc, char **argv) {
	if (argc < 1) return 2;
	std::string tmp = "cgm_folder_tmp"; ArgVal(argc, argv, "--tmp", tmp);
	std::vector<uint8_t> b; cgm::Bank bk; std::string e; if (!ParseFile(argv[0], b, bk, e)) { printf("FAIL %s: %s\n", argv[0], e.c_str()); return 1; }
	bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[0], m.c_str()); bad = true; };
	std::error_code ec; const std::string dir = tmp + "/cg"; fs::remove_all(fs::path(Utf8ToWide(tmp)), ec);
	cgm::ExportOptions eo; eo.cgtoolLayout = true; cgm::ExportResult er;
	if (!cgm::ExportBank(bk, dir, "bank", eo, er)) { F("export: " + er.error); return 1; }
	int drawable = 0; for (auto &im : bk.images) drawable += im.drawable();
	{ cgm::Bank w = bk; cgm::FolderImportReport r; if (!cgm::ImportPlainFolder(w, dir, r)) F("import-plain: " + r.error); std::vector<uint8_t> o; w.serialize(o); if (o != b) F("cgtool export -> plain import changed the bank (" + std::to_string(r.replaced.size()) + " replaced, " + std::to_string(r.resized.size()) + " re-cut)"); }
	{ cgm::Bank w = bk; std::vector<int> rm; cgm::FolderSyncReport r; if (!cgm::SyncBankToFolder(w, dir, rm, r)) F("sync: " + r.error); else { std::vector<uint8_t> o; w.serialize(o); if (o != b || !cgm::IsIdentityRemap(rm)) F("cgtool export -> sync unchanged is not identical (added " + std::to_string(r.added) + ", removed " + std::to_string(r.removed) + ", moved " + std::to_string(r.moved) + ")"); } }
	// (3) structure from the folder
	std::vector<cgm::FolderEntry> en; cgm::ScanImageFolder(dir, en, &e);
	int delId = -1, movId = -1; std::string movFile;
	auto at = bk.buildAtlas();
	for (auto &f : en) { if (f.id < 3) continue; if (delId < 0 && bk.dependants(f.id, at).empty() && bk.owners(f.id, at).empty()) { delId = f.id; fs::remove(fs::path(Utf8ToWide(f.path)), ec); continue; } if (movId < 0 && f.id > delId + 3 && bk.dependants(f.id, at).empty()) { movId = f.id; movFile = f.path; } }
	// a tiny bank (character-select portraits hold 1-2 images) has no deletable/movable image past id 3: the structure step needs both
	const bool structStep = delId >= 0 && movId >= 0;
	if (structStep) fs::rename(fs::path(Utf8ToWide(movFile)), fs::path(Utf8ToWide(dir + "/0.5#" + fs::path(Utf8ToWide(movFile)).filename().string())), ec);
	if (structStep) { auto spr = Sprite(48, 64, 0); std::vector<uint8_t> canvas((size_t)128 * 128 * 4, 0); for (int y = 0; y < 64; y++) memcpy(&canvas[((size_t)(y + 50) * 128 + 30) * 4], &spr[(size_t)y * 48 * 4], 48 * 4); WritePngRgba(dir + "/2.5#new_sprite.png", canvas.data(), 128, 128, e); }
	if (structStep) {
	{
		cgm::Bank w = bk; std::vector<int> rm; cgm::FolderSyncReport r;
		if (!cgm::SyncBankToFolder(w, dir, rm, r)) F("sync (structure): " + r.error);
		else {
			if (r.removed != 1 || r.added != 1) F("sync: expected 1 removed and 1 added, got " + std::to_string(r.removed) + " / " + std::to_string(r.added));
			if (delId >= 0 && rm[delId] != -1) F("the deleted file's image survived");
			if (movId >= 0 && rm[movId] != 1) F("the image renamed to 0.5# did not land at id 1 (it is at " + std::to_string(rm[movId]) + ")");
			int wrong = 0; for (int k = 0; k < (int)bk.images.size(); k++) if (rm[k] >= 0 && !SameDecoded(bk, k, w, rm[k])) wrong++;
			if (wrong) F(std::to_string(wrong) + " kept image(s) render differently after the sync");
			int nid = -1; for (int k = 0; k < (int)w.images.size(); k++) if (!strncmp(w.images[k].name, "new_sprite", 10)) nid = k;
			if (nid != 4) F("the new file 2.5#new_sprite.png is image " + std::to_string(nid) + ", expected 4 (after 0, 1 (moved in), 2 and 3)");
			else if (w.images[nid].x1 != 16 * (30 / 16) || w.images[nid].y1 != 16 * (50 / 16)) F("the new sprite's position on its canvas is wrong");
			std::vector<uint8_t> o; w.serialize(o); cgm::Bank again; if (!cgm::Bank::parse(o.data(), o.size(), again, &e)) F("the synced bank does not re-parse");
			std::string why; if (!EngineAgrees(w, why)) F("engine disagrees after the sync: " + why);
		}
	}
	}
	// (4) build from the export (fresh folder)
	fs::remove_all(fs::path(Utf8ToWide(dir)), ec); er = cgm::ExportResult(); cgm::ExportBank(bk, dir, "bank", eo, er);
	int buildOk = 0;
	{
		cgm::BuildOptions bo; cgm::Bank nb; cgm::BuildReport r;
		std::vector<uint8_t> slot0(1024); memcpy(slot0.data(), bk.palettes.data(), 1024);
		const std::string pf = tmp + "/slot0.pal"; { uint32_t p[256]; memcpy(p, slot0.data(), 1024); cgm::WritePalFileColors(pf, p, &e); }
		bo.palettePath = pf;
		if (!cgm::BuildBankFromFolder(dir, bo, nb, r)) F("build: " + r.error);
		else {
			for (int k = 0; k < (int)bk.images.size(); k++) {
				if (!bk.images[k].drawable()) continue;
				if (k >= (int)nb.images.size()) { F("build lost image " + std::to_string(k)); break; }
				std::vector<uint8_t> a, c; int aw, ah, cw, ch; cgm::CanvasPicture(bk, k, a, aw, ah); cgm::CanvasPicture(nb, k, c, cw, ch);
				bool same = aw <= cw && ah <= ch;
				for (int y = 0; same && y < ah; y++) for (int x = 0; x < aw; x++) { const uint8_t *p = &a[((size_t)y * aw + x) * 4], *q = &c[((size_t)y * cw + x) * 4]; const bool bin = bk.images[k].type == 0 || bk.images[k].type == 2; uint8_t pa = bin ? (p[3] >= 128 ? 255 : 0) : p[3], qa = bin ? (q[3] >= 128 ? 255 : 0) : q[3]; if (pa != qa || (pa && memcmp(p, q, 3))) { same = false; break; } }
				if (same) buildOk++; else if (buildOk + 1 >= 0) { F("built image " + std::to_string(k) + " renders differently"); break; }
			}
			std::string why; if (!bad && !EngineAgrees(nb, why)) F("engine disagrees on the built bank: " + why);
		}
	}
	// (5) bigger replacement with the bottom-centre anchor
	{
		int id = -1; for (int k = 0; k < (int)bk.images.size() && id < 0; k++) if (bk.images[k].drawable() && bk.images[k].boundsW() < 200 && bk.images[k].y2 + 1 < bk.images[k].h + 1) id = k;
		if (id >= 0) {
			cgm::Bank w = bk; const cgm::Image old = bk.images[id]; const int nw = old.boundsW() + 37, nh = old.boundsH() + 21;
			// colours straight from the bank palette and binary alpha, so every storage type (8-bit included) stores them exactly
			uint32_t bp[256]; w.palette(0, bp); std::vector<uint8_t> spr((size_t)nw * nh * 4, 0);
			for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) { const int dx = x - nw / 2, dy = y - nh / 2; if (dx * dx * 4 / (nw * nw) + dy * dy * 4 / (nh * nh) > 1) continue; int k = 1 + ((x / 7 + y / 5) % 30); while (k < 255 && !(bp[k] >> 24)) k++; memcpy(&spr[((size_t)y * nw + x) * 4], &bp[k], 3); spr[((size_t)y * nw + x) * 4 + 3] = 255; }
			const int x1 = std::max(0, old.x1 + old.boundsW() / 2 - nw / 2), y1 = std::max(0, old.y2 + 1 - nh);
			std::vector<int> un;
			if (!cgm::ReplaceImageResized(w, id, spr.data(), nw, nh, x1, y1, &un, &e)) F("resized replace: " + e);
			else {
				cgm::Rgba r; w.decode(id, r); const cgm::Image &im = w.images[id];
				bool pix = im.x1 <= x1 && im.y1 <= y1;
				for (int y = 0; pix && y < nh; y++) for (int x = 0; x < nw; x++) { const uint8_t *p = &r.px[((size_t)(y + y1 - im.y1) * r.w + x + x1 - im.x1) * 4], *q = &spr[((size_t)y * nw + x) * 4]; if (p[3] != q[3] || (q[3] && memcmp(p, q, 3))) { pix = false; break; } }
				if (!pix) F("resized replacement does not render the new sprite at its anchor");
				int wrong = 0; for (int k = 0; k < (int)bk.images.size(); k += std::max(1, (int)bk.images.size() / 80)) if (k != id && !SameDecoded(bk, k, w, k)) wrong++;
				if (wrong) F("resized replacement changed other images");
				if (!AtlasUnique(w)) F("resized replacement overlaps atlas cells");
				std::string why; if (!EngineAgrees(w, why)) F("engine disagrees after the resized replace: " + why);
			}
		}
	}
	fs::remove_all(fs::path(Utf8ToWide(tmp)), ec);
	printf("SECTION cgm-folder: %s | %s: %d drawable; cgtool export -> plain import / sync identical; %s; build from folder %d/%d images identical; resized replace anchored\n", bad ? "FAIL" : "pass", fs::path(argv[0]).filename().string().c_str(), drawable, structStep ? "sync delete+add+rename" : "sync delete+add+rename skipped (too few images)", buildOk, drawable);
	return bad ? 1 : 0;
}

// ---- palettes ----
static bool LoadSet(const char *p, cgm::PalSet &ps) { std::vector<uint8_t> raw; std::string e; if (!ReadFile(p, raw) || !cgm::PalSet::parse(raw.data(), raw.size(), ps, &e)) { printf("error: cannot read palette %s %s\n", p, e.c_str()); return false; } return true; }

static int CmdPalOps(const std::string &c, int argc, char **argv) {
	std::string out, v, e;
	if (c == "pal-convert") {   // pal-convert <x.pal> --uni|--mbaacc -o out.pal
		cgm::PalSet ps; if (argc < 1 || !LoadSet(argv[0], ps) || !ArgVal(argc, argv, "-o", out)) return 2;
		const cgm::PalSet o = ps.converted(Flag(argc, argv, "--uni"));
		if (!cgm::WritePalSet(out, o, false, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		printf("wrote %s: %d palettes, %s layout%s\n", out.c_str(), o.count, o.offsetDw == 4 ? "UNI2/MBTL" : "MBAACC", o.paired() ? " (65 colours x 2 sets)" : ""); return 0;
	}
	if (c == "pal-move") {   // pal-move <x.pal> <from> <to> -o out.pal  (UNI split files move the c / c+65 pair together)
		cgm::PalSet ps; if (argc < 3 || !LoadSet(argv[0], ps) || !ArgVal(argc, argv, "-o", out)) return 2;
		ps.movePair(atoi(argv[1]), atoi(argv[2]));
		if (!cgm::WritePalSet(out, ps, false, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		printf("moved %s to %s%s, wrote %s\n", argv[1], argv[2], ps.paired() ? " (with its alternate set)" : "", out.c_str()); return 0;
	}
	if (c == "pal-new-pups") {   // pal-new-pups <bank.cg | x.pal> <n> [--from y.pal]: new <stem>_p<n>.pal from the base palette file
		if (argc < 2) return 2;
		std::string from = cgm::PupsPath(argv[0], 0); ArgVal(argc, argv, "--from", from);
		cgm::PalSet base; if (!LoadSet(from.c_str(), base)) return 1;
		std::string p; if (!cgm::CreatePupsBank(argv[0], atoi(argv[1]), base, &p, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		printf("wrote %s (%d palettes, a copy of %s)\n", p.c_str(), base.count, from.c_str()); return 0;
	}
	if (c == "pal-check-uni") {   // pal-check-uni <x.pal>...: pair moves keep pairs; UNI <-> MBAACC conversion keeps colours; UNI single export re-reads
		int pass = 0, fail = 0;
		for (int i = 0; i < argc; i++) {
			cgm::PalSet ps; if (!LoadSet(argv[i], ps)) { fail++; continue; }
			bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[i], m.c_str()); bad = true; };
			if (ps.paired()) {
				cgm::PalSet m = ps; m.movePair(3, 10);
				for (int k = 0; k < 256; k++) if (m.pal(10)[k] != ps.pal(3)[k] || m.pal(10 + ps.half())[k] != ps.pal(3 + ps.half())[k]) { F("movePair did not move the pair"); break; }
				m.movePair(10, 3); if (m.raw != ps.raw) F("movePair and back is not byte-identical");
				cgm::PalSet s2 = ps; s2.swapPair(0, 64); s2.swapPair(0, 64); if (s2.raw != ps.raw) F("swapPair twice is not identical");
				cgm::PalSet mb = ps.converted(false), un = mb.converted(true);
				if (mb.count != ps.count) F("UNI -> MBAACC lost palettes");
				if (un.count == ps.count && memcmp(un.pal(0), ps.pal(0), (size_t)ps.count * 1024)) F("UNI -> MBAACC -> UNI changed colours");
			} else {
				cgm::PalSet un = ps.converted(true);
				for (int cidx = 0; cidx < 65 && cidx < ps.count; cidx++) if (memcmp(un.pal(cidx), ps.pal(cidx), 1024) || memcmp(un.pal(cidx + 65), ps.pal(cidx), 1024)) { F("MBAACC -> UNI changed colour " + std::to_string(cidx)); break; }
			}
			uint32_t back[256]; const std::string f = "cgm_uni_tmp.pal";
			if (!cgm::WritePalFileColors(f, ps.pal(ps.count - 1), &e, cgm::PalLayout::Uni) || !cgm::ReadPalFileColors(f, back, &e, 0) || memcmp(back, ps.pal(ps.count - 1), 1024)) F("UNI-layout single palette export does not read back");
			cgm::PalSet one; std::vector<uint8_t> raw; ReadFile(f.c_str(), raw); if (!cgm::PalSet::parse(raw.data(), raw.size(), one, &e) || one.offsetDw != 4) F("UNI-layout export does not parse as UNI");
			std::filesystem::remove(f);
			if (ps.count > 37 && (!cgm::ReadPalFileColors(argv[i], back, &e, 37) || memcmp(back, ps.pal(37), 1024))) F("import of palette 37 read the wrong palette");
			bad ? fail++ : pass++;
		}
		printf("SECTION cgm-palette-uni: pass %d fail %d | pair move/swap round trip, layout conversion, UNI single export, import of any index\n", pass, fail);
		return fail ? 1 : 0;
	}
	return -1;
}

// cc-export <x.pal> --items src:slot[,src:slot...] --dir <game>\palettes [--chara N] [--full-pal out.pal [--count N]]
static int CmdCc(const std::string &c, int argc, char **argv) {
	std::string v, e;
	if (c == "cc-export") {
		cgm::PalSet ps; if (argc < 1 || !LoadSet(argv[0], ps)) return 2;
		int chara = cgm::CcCharaFromStem(fs::path(argv[0]).stem().string()); if (ArgVal(argc, argv, "--chara", v)) chara = atoi(v.c_str());
		std::vector<cgm::CcExportItem> items;
		if (ArgVal(argc, argv, "--items", v)) { size_t i = 0; while (i < v.size()) { size_t j = v.find(',', i); if (j == std::string::npos) j = v.size(); const std::string t = v.substr(i, j - i); const size_t k = t.find(':'); if (k != std::string::npos) items.push_back({atoi(t.c_str()), atoi(t.c_str() + k + 1)}); i = j + 1; } }
		if (items.empty()) { puts("--items src:slot,... is required (slot is 1-based, 1..252)"); return 2; }
		std::string dir; if (ArgVal(argc, argv, "--dir", dir)) {
			cgm::CcExportResult r; if (!cgm::ExportCcPalettePngs(ps, chara, items, dir, r)) { printf("error: %s\n", r.error.c_str()); return 1; }
			for (auto &w : r.written) { uint32_t back[256]; cgm::ReadCcPalettePng(w, back, &e); const cgm::CcSlotPlace pl = cgm::CcPlaceOf(atoi(fs::path(w).stem().string().substr(fs::path(w).stem().string().find('_') + 1).c_str())); printf("wrote %s (selector bank %d, entry %d)\n", w.c_str(), pl.bank, pl.local + 1); }
		}
		std::string full; if (ArgVal(argc, argv, "--full-pal", full)) {
			int count = 0; if (ArgVal(argc, argv, "--count", v)) count = atoi(v.c_str());
			cgm::PalSet o; if (!cgm::BuildCcPalSet(ps, items, count, o, &e) || !cgm::WritePalSet(full, o, true, &e)) { printf("error: %s\n", e.c_str()); return 1; }
			printf("wrote %s: %d palettes (MBAACC layout, count past 36)\n", full.c_str(), o.count);
		}
		return 0;
	}
	if (c == "cc-swatch") {   // cc-swatch <x.pal> <color_cNN.png>
		cgm::PalSet ps; if (argc < 2 || !LoadSet(argv[0], ps)) return 2;
		std::vector<uint8_t> px; int w, h; if (!ReadImageRgba(argv[1], px, w, h, e)) { printf("error: %s\n", e.c_str()); return 1; }
		auto idx = cgm::CcSwatchIndices(ps, px.data(), w, h);
		printf("swatch entries:"); for (size_t i = 0; i < idx.size() && i < 8; i++) printf(" %d", idx[i]); printf("\n"); return 0;
	}
	if (c == "cc-check") {   // cc-check <x.pal>...: PNG export reads back through PovertyCaster's PLTE rule; grown .pal keeps 0..count-1 and fills with palette 0
		int pass = 0, fail = 0; std::error_code ec;
		for (int i = 0; i < argc; i++) {
			cgm::PalSet ps; if (!LoadSet(argv[i], ps)) { fail++; continue; }
			bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[i], m.c_str()); bad = true; };
			const std::string dir = "cgm_cc_tmp"; fs::remove_all(dir, ec);
			std::vector<cgm::CcExportItem> items = {{1, 37}, {std::min(5, ps.count - 1), 72}, {0, 252}};
			cgm::CcExportResult r; if (!cgm::ExportCcPalettePngs(ps, 1, items, dir, r)) F(r.error);
			for (size_t k = 0; k < r.written.size(); k++) { uint32_t back[256]; if (!cgm::ReadCcPalettePng(r.written[k], back, &e)) { F(e); continue; } for (int x = 1; x < 256; x++) if ((back[x] & 0xFFFFFF) != (ps.pal(items[k].sourcePalette)[x] & 0xFFFFFF)) { F("PNG palette differs at entry " + std::to_string(x)); break; } }
			if (fs::path(r.written.empty() ? "" : r.written[1]).filename().string() != "1_72.png") F("file name is not <charID>_<palNum>.png");
			cgm::PalSet o; if (!cgm::BuildCcPalSet(ps, items, 0, o, &e)) F(e);
			else {
				if (o.count != std::max(252, ps.count) || o.offsetDw != 1) F("grown .pal count is " + std::to_string(o.count));
				if (memcmp(o.pal(36), ps.pal(1), 1024) || memcmp(o.pal(251), ps.pal(0), 1024)) F("custom palettes are not at their slots");
				const int lim = ps.offsetDw == 1 ? std::min(ps.count, 36) : 0;
				for (int k = 0; k < lim; k++) if (memcmp(o.pal(k), ps.pal(k), 1024)) { F("stock palette " + std::to_string(k) + " changed"); break; }
				if (ps.count < 100 && (o.pal(100)[0] != 0 || memcmp(o.pal(100) + 1, ps.pal(0) + 1, 1020))) F("filler is not palette 0 with entry 0 cleared");
			}
			const cgm::CcSlotPlace p = cgm::CcPlaceOf(40); if (p.bank != 1 || p.local != 3 || cgm::CcPalNum(1, 3) != 40) F("bank layout");
			fs::remove_all(dir, ec);
			bad ? fail++ : pass++;
		}
		printf("SECTION cgm-cc-palettes: pass %d fail %d | <charID>_<palNum>.png PLTE read back as PovertyCaster does, .pal grown past 36, bank layout\n", pass, fail);
		return fail ? 1 : 0;
	}
	return -1;
}

// refs-compare <old.cg> <new.cg> <old.HA6> <new.HA6> [<old.HA6> <new.HA6>...]: after an edit + save (e.g. the GUI's one save), every
// CG layer of every file pair still draws the same picture through the new ids; a layer may only go to "no sprite" (a deleted image)
static int CmdRefsCompare(int argc, char **argv) {
	if (argc < 4 || (argc - 2) % 2) return 2;
	cgm::Bank a, b; if (!LoadBank(argv[0], a) || !LoadBank(argv[1], b)) return 1;
	int same = 0, cleared = 0, differ = 0, dangling = 0, files = 0; bool bad = false;
	for (int i = 2; i + 1 < argc; i += 2) {
		FrameData fo, fn; if (!fo.load(argv[i]) || !fn.load(argv[i + 1])) { printf("FAIL cannot load %s / %s\n", argv[i], argv[i + 1]); bad = true; continue; }
		files++;
		if (fo.get_sequence_count() != fn.get_sequence_count()) { printf("FAIL %s: pattern count changed\n", argv[i + 1]); bad = true; continue; }
		for (int p = 0; p < fo.get_sequence_count(); p++) {
			Sequence *so = fo.get_sequence(p), *sn = fn.get_sequence(p); if (!so || !sn) continue;
			for (int f = 0; f < (int)so->frames.size() && f < (int)sn->frames.size(); f++) {
				auto &lo = so->frames[f].AF.layers, &ln = sn->frames[f].AF.layers;
				for (int l = 0; l < (int)lo.size() && l < (int)ln.size(); l++) {
					if (lo[l].usePat || lo[l].spriteId < 0) continue;
					const int io = lo[l].spriteId, in = ln[l].spriteId;
					if (io >= (int)a.images.size()) { dangling += in == io; differ += in != io; continue; }
					if (in < 0) { cleared++; continue; }
					if (in < (int)b.images.size() && SameDecoded(a, io, b, in)) same++;
					else { if (differ < 5) printf("FAIL %s pattern %d frame %d layer %d: image %d -> %d draws another picture\n", argv[i + 1], p, f, l, io, in); differ++; }
				}
			}
		}
	}
	printf("SECTION cgm-refs-compare: %s | %d file pair(s): %d layers same picture, %d cleared, %d past the bank kept, %d differ\n", (bad || differ) ? "FAIL" : "pass", files, same, cleared, dangling, differ);
	return (bad || differ) ? 1 : 0;
}

// sprites <file.HA6> <pattern>: the CG ids each frame's layers draw
static int CmdSprites(int argc, char **argv) {
	if (argc < 2) return 2;
	FrameData fd; if (!fd.load(argv[0])) { printf("error: cannot load %s\n", argv[0]); return 1; }
	Sequence *s = fd.get_sequence(atoi(argv[1])); if (!s) { puts("no such pattern"); return 1; }
	for (size_t f = 0; f < s->frames.size(); f++) { printf("frame %zu:", f); for (auto &L : s->frames[f].AF.layers) printf(" %s%d", L.usePat ? "pat:" : "", L.spriteId); printf("\n"); }
	return 0;
}

int OpsMain(const std::string &c, int argc, char **argv) {
	if (c == "refs-compare") return CmdRefsCompare(argc, argv);
	if (c == "sprites") return CmdSprites(argc, argv);
	if (c == "users") return CmdUsers(argc, argv);
	if (c == "usage") return CmdUsage(argc, argv);
	if (c == "build") return CmdBuild(argc, argv);
	if (c == "project-refs-check") return CmdProjectRefsCheck(argc, argv);
	if (c == "folder-check") return CmdFolderCheck(argc, argv);
	if (c == "insert" || c == "delete" || c == "move" || c == "delete-unused" || c == "replace" || c == "sync" || c == "import-plain") return CmdStruct(c, argc, argv);
	if (c.compare(0, 4, "pal-") == 0) return CmdPalOps(c, argc, argv);
	if (c.compare(0, 3, "cc-") == 0) return CmdCc(c, argc, argv);
	return -1;
}
