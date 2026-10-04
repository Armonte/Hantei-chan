// cgmtool: command line front end of the CG manager core (docs/cg/cg_manager_design.md).
//   cgmtool check <bank.cg>...        parse -> serialize must be byte identical; decode must equal CG::draw_texture for every image
//   cgmtool info <bank.cg>            image table summary
#include "cgm_bank.h"
#include "cgm_export.h"
#include "cgm_ops.h"
#include "../png_writer.h"
#include <filesystem>
#include "../cg.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

static bool ReadFile(const char *p, std::vector<uint8_t> &b) {
	std::ifstream f(p, std::ios::binary); if (!f) return false;
	b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}

static int CmdCheck(int argc, char **argv) {
	int pass = 0, fail = 0; size_t imgs = 0, undrawn = 0, cmpOk = 0, cmpBad = 0;
	std::map<int, int> types;
	for (int i = 0; i < argc; i++) {
		std::vector<uint8_t> b; std::string err;
		if (!ReadFile(argv[i], b)) { printf("FAIL %s: cannot read\n", argv[i]); fail++; continue; }
		cgm::Bank bk;
		if (!cgm::Bank::parse(b.data(), b.size(), bk, &err)) { printf("FAIL %s: parse: %s\n", argv[i], err.c_str()); fail++; continue; }
		std::vector<uint8_t> o; bk.serialize(o);
		if (o != b) { size_t d = 0; while (d < o.size() && d < b.size() && o[d] == b[d]) d++; printf("FAIL %s: serialize differs (first diff 0x%zx, sizes %zu vs %zu)\n", argv[i], d, o.size(), b.size()); fail++; continue; }
		CG cg; cg.loadFromMemory(b.data(), (unsigned)b.size());
		int bad = 0;
		for (size_t n = 0; n < bk.images.size(); n++) {
			const cgm::Image &im = bk.images[n];
			if (!im.present) continue;
			imgs++; types[im.type]++;
			cgm::Rgba r; bool mine = bk.decode((int)n, r);
			ImageData *ref = cg.draw_texture((unsigned)n, false, false);
			if (!ref) { if (mine) { printf("FAIL %s: image %zu decodes here but not in CG\n", argv[i], n); bad++; } else undrawn++; continue; }
			if (!mine) { printf("FAIL %s: image %zu: CG draws it, the model does not\n", argv[i], n); bad++; delete ref; continue; }
			bool same = ref->width == r.w && ref->height == r.h;
			for (int k = 0; same && k < r.w * r.h; k++) {
				const uint8_t *a = &ref->pixels[k * 4], *c = &r.px[k * 4];
				if (a[3] != c[3] || (a[3] && memcmp(a, c, 3))) same = false;
			}
			if (same) cmpOk++; else { cmpBad++; if (bad < 3) printf("FAIL %s: image %zu (type %d) pixels differ from CG::draw_texture\n", argv[i], n, im.type); bad++; }
			delete ref;
		}
		if (bad) fail++; else pass++;
	}
	printf("SECTION cgm-bank: pass %d fail %d | images %zu (undrawable %zu), decode==CG %zu, differ %zu |", pass, fail, imgs, undrawn, cmpOk, cmpBad);
	for (auto &t : types) printf(" type%d:%d", t.first, t.second);
	printf("\n");
	return fail ? 1 : 0;
}

static int CmdInfo(int argc, char **argv) {
	if (argc < 1) return 2;
	std::vector<uint8_t> b; std::string err; cgm::Bank bk;
	if (!ReadFile(argv[0], b) || !cgm::Bank::parse(b.data(), b.size(), bk, &err)) { printf("error: %s\n", err.c_str()); return 1; }
	auto atlas = bk.buildAtlas();
	printf("images %zu, pages %d, cell %d, blocks %u\n", bk.images.size(), bk.pages(), bk.cellUnit(), bk.H[2]);
	for (size_t n = 0; n < bk.images.size(); n++) {
		const cgm::Image &im = bk.images[n]; if (!im.present) { printf("%4zu absent\n", n); continue; }
		printf("%4zu %-24.32s t%d %dx%d bpp%d blocks %zu owners %zu dependants %zu\n", n, im.name, im.type, im.boundsW(), im.boundsH(), im.bpp, im.blocks.size(), bk.owners((int)n, atlas).size(), bk.dependants((int)n, atlas).size());
	}
	return 0;
}

static bool WriteFile(const std::string &p, const std::vector<uint8_t> &b) {
	std::ofstream f(std::filesystem::path(Utf8ToWide(p)), std::ios::binary); if (!f) return false;
	f.write((const char *)b.data(), (std::streamsize)b.size()); return (bool)f;
}
static bool ParseFile(const std::string &p, std::vector<uint8_t> &b, cgm::Bank &bk, std::string &err) {
	if (!ReadFile(p.c_str(), b)) { err = "cannot read " + p; return false; }
	return cgm::Bank::parse(b.data(), b.size(), bk, &err);
}

// cgmtool export <bank> <dir> [--no-rgba] [--no-indexed] [--cgtool]
static int CmdExport(int argc, char **argv) {
	if (argc < 2) { puts("usage: cgmtool export <bank.cg> <outdir> [--no-rgba] [--no-indexed] [--cgtool]"); return 2; }
	cgm::ExportOptions o; for (int i = 2; i < argc; i++) { std::string a = argv[i]; if (a == "--no-rgba") o.rgba = false; else if (a == "--no-indexed") o.indexed = false; else if (a == "--cgtool") o.cgtoolLayout = true; }
	std::vector<uint8_t> b; cgm::Bank bk; std::string err;
	if (!ParseFile(argv[0], b, bk, err)) { printf("error: %s\n", err.c_str()); return 1; }
	cgm::ExportResult r; std::string nm = std::filesystem::path(argv[0]).filename().string();
	if (!cgm::ExportBank(bk, argv[1], nm, o, r)) { printf("error: %s\n", r.error.c_str()); return 1; }
	printf("exported %d images, %d files to %s\n", r.images, r.files, argv[1]); return 0;
}

// cgmtool import <bank> <dir> -o <out.cg> : writes a NEW file, only re-encoding images whose pixels changed
static int CmdImport(int argc, char **argv) {
	std::string out; if (argc < 2) { puts("usage: cgmtool import <bank.cg> <dir> -o <out.cg>"); return 2; }
	for (int i = 2; i + 1 < argc; i++) if (!strcmp(argv[i], "-o")) out = argv[i + 1];
	if (out.empty()) { puts("-o <out.cg> is required (the input bank is never overwritten)"); return 2; }
	std::vector<uint8_t> b; cgm::Bank bk; std::string err;
	if (!ParseFile(argv[0], b, bk, err)) { printf("error: %s\n", err.c_str()); return 1; }
	cgm::ImportResult r;
	if (!cgm::ImportBank(bk, argv[1], r)) { printf("error: %s\n", r.error.c_str()); return 1; }
	for (auto &w : r.warnings) printf("warning: %s\n", w.c_str());
	std::vector<uint8_t> o; bk.serialize(o);
	if (!WriteFile(out, o)) { puts("could not write the output"); return 1; }
	printf("%zu image(s) re-encoded (", r.changed.size()); for (int c : r.changed) printf(" %d", c); printf(" ), %d unchanged, wrote %s (%zu bytes)\n", r.skipped, out.c_str(), o.size());
	return 0;
}

// Per bank: (1) export all -> import unchanged -> identical bytes; (2) edit locality: change a few pixels of up to 3 images (one per storage type)
// in their exported RGBA PNG, import, and only that image's blob may differ (the stored size and the index must not move).
static int CmdRoundtrip(int argc, char **argv) {
	std::string tmp = "cgm_rt_tmp"; std::vector<std::string> files;
	for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--tmp") && i + 1 < argc) tmp = argv[++i]; else files.push_back(argv[i]); }
	int pass = 0, fail = 0; size_t edits = 0, editsOk = 0, imagesChecked = 0;
	for (const std::string &f : files) {
		std::vector<uint8_t> b; cgm::Bank bk; std::string err;
		if (!ParseFile(f, b, bk, err)) { printf("FAIL %s: %s\n", f.c_str(), err.c_str()); fail++; continue; }
		const std::string dir = (std::filesystem::path(tmp) / std::filesystem::path(f).filename()).string();
		std::filesystem::remove_all(dir);
		cgm::ExportResult er; cgm::ExportOptions eo;
		if (!cgm::ExportBank(bk, dir, std::filesystem::path(f).filename().string(), eo, er)) { printf("FAIL %s: export: %s\n", f.c_str(), er.error.c_str()); fail++; continue; }
		bool bad = false;
		{   // (1) unchanged
			cgm::Bank b2; cgm::Bank::parse(b.data(), b.size(), b2, nullptr); cgm::ImportResult ir;
			if (!cgm::ImportBank(b2, dir, ir)) { printf("FAIL %s: import: %s\n", f.c_str(), ir.error.c_str()); bad = true; }
			else {
				std::vector<uint8_t> o; b2.serialize(o);
				if (!ir.changed.empty() || o != b) { printf("FAIL %s: unchanged export -> import altered the bank (%zu images re-encoded, bytes %s)\n", f.c_str(), ir.changed.size(), o == b ? "identical" : "differ"); bad = true; }
			}
		}
		// (2) edit locality
		std::vector<int> picks; bool seenType[5] = {};
		auto atlas = bk.buildAtlas();
		for (int n = 0; n < (int)bk.images.size() && picks.size() < 3; n++) {
			const cgm::Image &im = bk.images[n]; if (!im.drawable() || im.type < 0 || im.type > 4 || seenType[im.type]) continue;
			if (!bk.dependants(n, atlas).empty() || !bk.owners(n, atlas).empty()) continue;   // shared images change together by design
			cgm::Rgba r; if (!bk.decode(n, r)) continue;
			int p1 = -1, p2 = -1;   // two pixels with different values
			for (int i = 0; i < r.w * r.h && p1 < 0; i++) if (r.px[i * 4 + 3] > 0) { for (int k = i + 1; k < r.w * r.h; k++) { const uint8_t *a = &r.px[i * 4], *c = &r.px[k * 4]; if (c[3] > 0 && (im.type == 3 ? a[3] != c[3] : memcmp(a, c, 3) != 0)) { p1 = i; p2 = k; break; } } }
			if (p1 < 0) continue;
			seenType[im.type] = true; picks.push_back(n);
			if (im.type == 3) r.px[p1 * 4 + 3] = r.px[p2 * 4 + 3]; else memcpy(&r.px[p1 * 4], &r.px[p2 * 4], 3);
			cgm::Bank b3; cgm::Bank::parse(b.data(), b.size(), b3, nullptr);
			{ std::filesystem::remove_all(dir); cgm::ExportResult e2; cgm::ExportOptions o1 = eo; o1.onlyIds = {n}; if (!cgm::ExportBank(bk, dir, std::filesystem::path(f).filename().string(), o1, e2)) { printf("FAIL %s: re-export\n", f.c_str()); bad = true; continue; } }
			// write the edited PNG over the exported one
			char nb[16]; snprintf(nb, sizeof(nb), "%04d", n);
			std::string png;
			for (auto &e : std::filesystem::directory_iterator(std::filesystem::path(dir) / "rgba")) { std::string nm = e.path().filename().string(); if (nm.compare(0, 4, nb) == 0 && nm[4] == '_') png = e.path().string(); }
			std::string we;
			if (png.empty() || !WritePngRgba(png, r.px.data(), r.w, r.h, we)) { printf("FAIL %s: image %d: cannot write the edited PNG\n", f.c_str(), n); bad = true; continue; }
			// also remove the indexed twin so the RGBA edit is the one that counts (an unchanged indexed PNG would not override it anyway)
			cgm::ImportResult ir; if (!cgm::ImportBank(b3, dir, ir)) { printf("FAIL %s: image %d: import: %s\n", f.c_str(), n, ir.error.c_str()); bad = true; continue; }
			edits++;
			std::vector<uint8_t> o; b3.serialize(o);
			size_t s0 = 0, e0 = 0; bk.blobSpan(n, s0, e0);
			bool ok = o.size() == b.size() && ir.changed.size() == 1 && ir.changed[0] == n;
			size_t firstOut = 0, nDiff = 0;
			for (size_t k = 0; ok && k < b.size(); k++) if (o[k] != b[k]) { nDiff++; if (k < s0 || k >= e0) { ok = false; firstOut = k; } }
			if (ok && nDiff == 0) { printf("FAIL %s: image %d (type %d): the edit changed nothing\n", f.c_str(), n, im.type); ok = false; }
			cgm::Rgba after; if (ok && !(b3.decode(n, after) && after.px == r.px)) { printf("FAIL %s: image %d (type %d): re-import does not render the edited pixels\n", f.c_str(), n, im.type); ok = false; }
			else if (!ok) printf("FAIL %s: image %d (type %d): bytes changed outside its blob (0x%zx) or size/index moved (changed=%zu)\n", f.c_str(), n, im.type, firstOut, ir.changed.size());
			if (ok) { editsOk++; imagesChecked++; } else bad = true;
		}
		std::filesystem::remove_all(dir);
		if (bad) fail++; else pass++;
	}
	printf("SECTION cgm-roundtrip: pass %d fail %d | export->import unchanged identical; locality edits %zu ok %zu\n", pass, fail, edits, editsOk);
	return fail ? 1 : 0;
}

int main(int argc, char **argv) {
	if (argc < 2) { puts("usage: cgmtool check|info|export|import|roundtrip ..."); return 2; }
	std::string c = argv[1];
	if (c == "check") return CmdCheck(argc - 2, argv + 2);
	if (c == "info") return CmdInfo(argc - 2, argv + 2);
	if (c == "export") return CmdExport(argc - 2, argv + 2);
	if (c == "import") return CmdImport(argc - 2, argv + 2);
	if (c == "roundtrip") return CmdRoundtrip(argc - 2, argv + 2);
	puts("unknown command"); return 2;
}
