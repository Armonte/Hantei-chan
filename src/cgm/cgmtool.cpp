// cgmtool: command line front end of the CG manager core (docs/cg/cg_manager_design.md).
//   cgmtool check <bank.cg>...        parse -> serialize must be byte identical; decode must equal CG::draw_texture for every image
//   cgmtool info <bank.cg>            image table summary
#include "cgm_bank.h"
#include "cgm_export.h"
#include "cgm_ops.h"
#include "cgm_palette.h"
#include "cgm_struct.h"
#include "cgm_usage.h"
#include "../framedata.h"
#include <random>
#include "../png_writer.h"
#include <filesystem>
#include "../cg.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <algorithm>
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
			if (!im.present || (int)n >= (int)bk.images.size() - bk.hidden) continue;
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

static bool LoadPal(const char *p, cgm::PalSet &ps, std::vector<uint8_t> &raw) {
	std::string e; if (!ReadFile(p, raw) || !cgm::PalSet::parse(raw.data(), raw.size(), ps, &e)) { printf("error: cannot read palette %s %s\n", p, e.c_str()); return false; }
	return true;
}

// pal-check <x.pal>...: parse -> raw bytes identical; every palette exports to .act/.gpl/.png and re-imports with the same colours; neutral adjust is the identity
static int CmdPalCheck(int argc, char **argv) {
	int pass = 0, fail = 0, pals = 0;
	for (int i = 0; i < argc; i++) {
		cgm::PalSet ps; std::vector<uint8_t> raw;
		if (!LoadPal(argv[i], ps, raw)) { fail++; continue; }
		bool bad = false;
		if (ps.raw != raw) { printf("FAIL %s: raw bytes differ\n", argv[i]); bad = true; }
		const std::string tmp = (std::filesystem::path(argv[i]).filename().string());
		for (int n = 0; n < ps.count && !bad; n += std::max(1, ps.count / 3)) {   // first, middle, last-ish palettes
			uint32_t back[256]; std::string e; pals++;
			for (const char *ext : {".act", ".gpl", ".png"}) {
				const std::string f = std::string("cgm_pal_tmp") + ext;
				if (!cgm::WritePalFileColors(f, ps.pal(n), &e) || !cgm::ReadPalFileColors(f, back, &e)) { printf("FAIL %s palette %d: %s file: %s\n", argv[i], n, ext, e.c_str()); bad = true; break; }
				for (int k = 0; k < 256; k++) if ((back[k] & 0xFFFFFF) != (ps.pal(n)[k] & 0xFFFFFF)) { printf("FAIL %s palette %d: %s round trip differs at %d\n", argv[i], n, ext, k); bad = true; break; }
				std::filesystem::remove(f);
			}
			cgm::PalSet c2 = ps; cgm::ColorAdjust none; cgm::RecolorScope sc; cgm::RecolorSet(c2, none, sc);
			if (c2.raw != ps.raw) { printf("FAIL %s: neutral recolour changed bytes\n", argv[i]); bad = true; }
			cgm::ColorAdjust h; h.hueDeg = 40; cgm::RecolorSet(c2, h, sc); h.hueDeg = -40; cgm::RecolorSet(c2, h, sc);   // there and back: within rounding
			int worst = 0; for (int k = 1; k < 256; k++) { const uint32_t a = c2.pal(n)[k], b2 = ps.pal(n)[k]; worst = std::max({worst, std::abs((int)cgm::R(a) - cgm::R(b2)), std::abs((int)cgm::G(a) - cgm::G(b2)), std::abs((int)cgm::B(a) - cgm::B(b2))}); }
			if (worst > 12 && n == 0) { printf("FAIL %s: hue +40/-40 drifted by %d\n", argv[i], worst); bad = true; }
		}
		if (bad) fail++; else pass++;
	}
	printf("SECTION cgm-palette: pass %d fail %d | palettes checked %d (raw exact, .act/.gpl/.png round trip, neutral recolour identity)\n", pass, fail, pals);
	return fail ? 1 : 0;
}

static bool ArgVal(int argc, char **argv, const char *k, std::string &v) { for (int i = 0; i + 1 < argc; i++) if (!strcmp(argv[i], k)) { v = argv[i + 1]; return true; } return false; }

// pal-export <x.pal> <n> <out.act|.gpl|.png|.pal>        pal-import <x.pal> <n> <in> -o <out.pal>
// pal-recolor <x.pal> [--hue D] [--sat M] [--val M] [--palette N] [--range a-b] -o <out.pal>
static int CmdPal(const std::string &c, int argc, char **argv) {
	if (c == "pal-check") return CmdPalCheck(argc, argv);
	if (argc < 1) { puts("usage: cgmtool pal-info|pal-export|pal-import|pal-recolor|pal-check ..."); return 2; }
	cgm::PalSet ps; std::vector<uint8_t> raw; if (!LoadPal(argv[0], ps, raw)) return 1;
	std::string e, v;
	if (c == "pal-info") { printf("%d palettes, layout %s, %zu bytes\n", ps.count, ps.offsetDw == 1 ? "MBAACC" : "UNI/MBTL", raw.size()); return 0; }
	if (c == "pal-export" && argc >= 3) { uint32_t p[256]; const int n = atoi(argv[1]); if (n < 0 || n >= ps.count) { puts("no such palette"); return 1; } memcpy(p, ps.pal(n), 1024); if (!cgm::WritePalFileColors(argv[2], p, &e)) { printf("error: %s\n", e.c_str()); return 1; } printf("wrote %s\n", argv[2]); return 0; }
	std::string out; if (!ArgVal(argc, argv, "-o", out)) { puts("-o <out.pal> is required (the input is never overwritten)"); return 2; }
	if (c == "pal-import" && argc >= 3) {
		const int n = atoi(argv[1]); uint32_t p[256]; if (n < 0 || n >= ps.count + 1 || !cgm::ReadPalFileColors(argv[2], p, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		if (n == ps.count && !ps.addCopy(ps.count - 1, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		for (int k = 0; k < 256; k++) ps.pal(n)[k] = (ps.pal(n)[k] & 0xFF000000u) | (p[k] & 0xFFFFFFu);
	} else if (c == "pal-recolor") {
		cgm::ColorAdjust a; cgm::RecolorScope sc;
		if (ArgVal(argc, argv, "--hue", v)) a.hueDeg = (float)atof(v.c_str());
		if (ArgVal(argc, argv, "--sat", v)) a.satMul = (float)atof(v.c_str());
		if (ArgVal(argc, argv, "--val", v)) a.valMul = (float)atof(v.c_str());
		if (ArgVal(argc, argv, "--palette", v)) { sc.allPalettes = false; sc.onlyPalette = atoi(v.c_str()); }
		if (ArgVal(argc, argv, "--range", v)) sscanf(v.c_str(), "%d-%d", &sc.fromIndex, &sc.toIndex);
		printf("%d palette(s) recoloured\n", cgm::RecolorSet(ps, a, sc));
	} else { puts("bad arguments"); return 2; }
	if (!WriteFile(out, ps.raw)) { puts("could not write the output"); return 1; }
	printf("wrote %s\n", out.c_str()); return 0;
}

// recolor-bank <bank.cg> [--hue D] [--sat M] [--val M] [--no-slots] [--no-images] -o <out.cg>
static int CmdRecolorBank(int argc, char **argv) {
	if (argc < 1) return 2;
	std::string out, v; if (!ArgVal(argc, argv, "-o", out)) { puts("-o <out.cg> is required"); return 2; }
	std::vector<uint8_t> b; cgm::Bank bk; std::string err; if (!ParseFile(argv[0], b, bk, err)) { printf("error: %s\n", err.c_str()); return 1; }
	cgm::ColorAdjust a; if (ArgVal(argc, argv, "--hue", v)) a.hueDeg = (float)atof(v.c_str()); if (ArgVal(argc, argv, "--sat", v)) a.satMul = (float)atof(v.c_str()); if (ArgVal(argc, argv, "--val", v)) a.valMul = (float)atof(v.c_str());
	bool slots = true, imgs = true; for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--no-slots")) slots = false; if (!strcmp(argv[i], "--no-images")) imgs = false; }
	std::vector<int> ch; const int n = cgm::RecolorBank(bk, a, slots, imgs, &ch);
	std::vector<uint8_t> o; bk.serialize(o); if (!WriteFile(out, o)) { puts("could not write the output"); return 1; }
	printf("%d palette(s) changed (%zu image palettes), wrote %s\n", n, ch.size(), out.c_str()); return 0;
}

// ---- structural edits ----
static std::vector<uint8_t> Sprite(int w, int h, int mode) {   // mode 0: few colours, binary alpha; 1: few colours + soft alpha; 2: many colours
	std::vector<uint8_t> px((size_t)w * h * 4, 0);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		const int dx = x - w / 2, dy = y - h / 2; if (dx * dx * 4 / (w * w) + dy * dy * 4 / (h * h) > 1) continue;
		uint8_t *p = &px[((size_t)y * w + x) * 4];
		if (mode == 2) { p[0] = (uint8_t)(x * 255 / w); p[1] = (uint8_t)(y * 255 / h); p[2] = (uint8_t)((x * y) & 255); p[3] = 255; }
		else { const int k = ((x / 7) + (y / 5)) % 6; p[0] = (uint8_t)(40 + 35 * k); p[1] = (uint8_t)(220 - 30 * k); p[2] = (uint8_t)(90 + 20 * k); p[3] = mode == 1 ? (uint8_t)(64 + 32 * (k % 4)) : 255; }
	}
	return px;
}
static bool SameDecoded(const cgm::Bank &a, int ia, const cgm::Bank &b, int ib) {
	cgm::Rgba x, y; const bool dx = a.decode(ia, x), dy = b.decode(ib, y);
	if (dx != dy) return false; if (!dx) return true;
	if (x.w != y.w || x.h != y.h) return false;
	for (int i = 0; i < x.w * x.h; i++) { const uint8_t *p = &x.px[i * 4], *q = &y.px[i * 4]; if (p[3] != q[3] || (p[3] && memcmp(p, q, 3))) return false; }
	return true;
}
static bool AtlasUnique(const cgm::Bank &b) {
	std::map<uint32_t, int> own; const int cu = b.cellUnit();
	for (size_t n = 0; n < b.images.size(); n++) for (const cgm::Block &bl : b.images[n].blocks) if (!bl.copy)
		for (int cy = bl.sy / cu; cy < (bl.sy + bl.h) / cu; cy++) for (int cx = bl.sx / cu; cx < (bl.sx + bl.w) / cu; cx++) if (!own.emplace(cgm::Bank::cellKey(bl.page, cx, cy), (int)n).second) return false;
	return true;
}
static bool EngineAgrees(const cgm::Bank &b, std::string &why) {   // the original engine loader (CG) must draw every image like the model
	std::vector<uint8_t> o; b.serialize(o); CG cg; if (!cg.loadFromMemory(o.data(), (unsigned)o.size())) { why = "CG refuses the bank"; return false; }
	for (size_t n = 0; n < b.images.size(); n++) {
		cgm::Rgba r; const bool mine = b.decode((int)n, r); ImageData *ref = cg.draw_texture((unsigned)n, false, false);
		if (!!ref != mine) { why = "image " + std::to_string(n) + ": CG and the model disagree about drawability"; delete ref; return false; }
		if (!ref) continue; bool same = ref->width == r.w && ref->height == r.h;
		for (int k = 0; same && k < r.w * r.h; k++) { const uint8_t *a = &ref->pixels[k * 4], *c = &r.px[k * 4]; if (a[3] != c[3] || (a[3] && memcmp(a, c, 3))) same = false; }
		delete ref; if (!same) { why = "image " + std::to_string(n) + " renders differently in CG"; return false; }
	}
	return true;
}

// struct-check <bank>...: add / insert / delete / move / permute / clear / unshare / duplicate, each proven against the original bytes or pixels
static int CmdStructCheck(int argc, char **argv) {
	int pass = 0, fail = 0, ops = 0;
	for (int i = 0; i < argc; i++) {
		std::vector<uint8_t> b; cgm::Bank bk; std::string err, why;
		if (!ParseFile(argv[i], b, bk, err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fail++; continue; }
		bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[i], m.c_str()); bad = true; };
		const int N = (int)bk.images.size();
		std::vector<uint8_t> out;
		auto sameBytes = [&](const cgm::Bank &x) { x.serialize(out); return out == b; };
		auto rt = [&](const cgm::Bank &x) { x.serialize(out); cgm::Bank y; std::string e; return cgm::Bank::parse(out.data(), out.size(), y, &e) && (y.serialize(out), true); };
		// 1. add (three storage kinds) + delete == original; insert in the middle + delete == original
		for (int mode = 0; mode < 3 && !bad; mode++) for (int at : {-1, N / 3}) {
			cgm::Bank w = bk; std::vector<int> remap; int id = -1; cgm::NewImageSpec sp; sp.name = "test_new.bmp";
			auto spr = Sprite(45 + mode * 9, 70 - mode * 4, mode); ops++;
			if (!cgm::AddImage(w, at, sp, spr.data(), 45 + mode * 9, 70 - mode * 4, remap, &id, &err)) { F("AddImage: " + err); break; }
			if (!rt(w)) { F("added bank does not re-parse"); break; }
			if (!AtlasUnique(w)) { F("added image overlaps atlas cells"); break; }
			const int wantType = mode == 0 ? 2 : mode == 1 ? 4 : 1; if (w.images[id].type != wantType) { F("AddImage chose type " + std::to_string(w.images[id].type) + ", expected " + std::to_string(wantType)); break; }
			cgm::Rgba got; w.decode(id, got); bool pix = got.w >= 45; // the sprite is padded to whole cells; compare its own area
			for (int y = 0; pix && y < 70 - mode * 4; y++) for (int x = 0; x < 45 + mode * 9; x++) { const uint8_t *a = &got.px[((size_t)y * got.w + x) * 4], *c = &spr[((size_t)y * (45 + mode * 9) + x) * 4]; uint8_t ca = c[3]; if (mode == 0) ca = ca >= 128 ? 255 : 0; if (a[3] != ca || (ca && memcmp(a, c, 3))) { pix = false; break; } }
			if (!pix) { F("added image does not render as the PNG (mode " + std::to_string(mode) + ")"); break; }
			if (!EngineAgrees(w, why) && mode == 0 && at < 0) { F("engine disagrees after add: " + why); break; }
			for (int k = 0; k < N && !bad; k += std::max(1, N / 40)) if (!SameDecoded(bk, k, w, remap[k])) { F("image " + std::to_string(k) + " changed by AddImage"); }
			std::vector<int> r2; if (!cgm::DeleteImage(w, id, false, r2, &err)) { F("DeleteImage: " + err); break; }
			{ int mp = -1; for (const cgm::Image &im : w.images) for (const cgm::Block &bl : im.blocks) mp = std::max<int>(mp, bl.page); if ((uint32_t)(mp + 1) <= bk.H[0]) w.H[0] = bk.H[0]; }   // pages added for the image are given back
			if (!sameBytes(w)) { F("add -> delete is not byte-identical"); break; }
		}
		// 2. move / permute and back
		if (!bad && N > 4) {
			std::mt19937 rng(12345); std::vector<int> order = cgm::IdentityRemap((size_t)N); std::shuffle(order.begin(), order.end(), rng);
			cgm::Bank w = bk; std::vector<int> remap, inv; ops++;
			if (!cgm::Permute(w, order, remap, &err)) F("Permute: " + err);
			else {
				for (int k = 0; k < N && !bad; k += std::max(1, N / 60)) if (!SameDecoded(bk, k, w, remap[k])) F("image " + std::to_string(k) + " differs after Permute");
				if (!bad && !rt(w)) F("permuted bank does not re-parse");
				std::vector<int> undo(N); for (int k = 0; k < N; k++) undo[k] = remap[k];   // order that puts the image now at remap[k] back at k
				if (!cgm::Permute(w, undo, inv, &err)) F("inverse Permute: " + err); else if (!sameBytes(w)) F("permute -> inverse is not byte-identical");
			}
			cgm::Bank m2 = bk; std::vector<int> mr; ops++;
			if (!cgm::MoveImage(m2, 1, N - 2, mr, &err)) F("MoveImage: " + err); else { if (!SameDecoded(bk, 1, m2, mr[1])) F("moved image differs"); std::vector<int> mr2; if (!cgm::MoveImage(m2, N - 2, 1, mr2, &err) || !sameBytes(m2)) F("move -> move back is not byte-identical"); }
		}
		// 3. shared cells: unshare, delete owner, duplicate, clear
		if (!bad) {
			auto atlas = bk.buildAtlas(); int dep = -1, own = -1;
			for (int n = 0; n < N && dep < 0; n++) { auto o = bk.owners(n, atlas); if (!o.empty() && bk.images[n].drawable()) { dep = n; own = o[0]; } }
			if (dep >= 0) {
				cgm::Bank w = bk; ops++;
				if (!cgm::UnshareImage(w, dep, &err)) F("UnshareImage: " + err);
				else {
					auto a2 = w.buildAtlas(); if (!w.owners(dep, a2).empty()) F("image still borrows cells after UnshareImage");
					if (!SameDecoded(bk, dep, w, dep)) F("UnshareImage changed the pixels"); if (!AtlasUnique(w)) F("UnshareImage: atlas cells overlap"); if (!rt(w)) F("unshared bank does not re-parse");
					std::string why2; if (!EngineAgrees(w, why2)) F("engine disagrees after unshare: " + why2);
				}
				cgm::Bank d = bk; std::vector<int> remap; ops++;
				std::string e1; if (cgm::DeleteImage(d, own, false, remap, &e1)) F("DeleteImage deleted an image whose cells are borrowed without being asked to unshare");
				if (!cgm::DeleteImage(d, own, true, remap, &err)) F("DeleteImage(unshare): " + err);
				else { for (int k = 0; k < N && !bad; k += std::max(1, N / 50)) if (k != own && !SameDecoded(bk, k, d, remap[k])) F("image " + std::to_string(k) + " changed by deleting the owner of shared cells"); if (!bad && !AtlasUnique(d)) F("delete+unshare: atlas cells overlap"); }
				cgm::Bank c = bk; if (cgm::ClearImage(c, own, &e1)) F("ClearImage cleared an image whose cells are borrowed");
			}
			int plain = -1; for (int n = 0; n < N && plain < 0; n++) if (bk.images[n].drawable() && bk.owners(n, atlas).empty() && bk.dependants(n, atlas).empty()) plain = n;
			if (plain >= 0) {
				cgm::Bank c = bk; ops++;
				if (!cgm::ClearImage(c, plain, &err)) F("ClearImage: " + err); else { if (c.images[plain].drawable()) F("cleared image still drawable"); if (!rt(c)) F("cleared bank does not re-parse"); }
				cgm::Bank dup = bk; std::vector<int> remap; int nid = -1; ops++;
				if (!cgm::DuplicateImage(dup, plain, plain + 1, remap, &nid, &err)) F("DuplicateImage: " + err); else { if (!SameDecoded(dup, nid, bk, plain)) F("duplicate renders differently"); if (!AtlasUnique(dup)) F("duplicate shares atlas cells"); std::string w2; if (!EngineAgrees(dup, w2)) F("engine disagrees after duplicate: " + w2); }
			}
		}
		if (bad) fail++; else pass++;
	}
	printf("SECTION cgm-struct: pass %d fail %d | structural operations proven %d (add/insert/delete round trip byte-identical, permute + inverse, move, unshare, delete owner, clear, duplicate; engine loader agrees)\n", pass, fail, ops);
	return fail ? 1 : 0;
}

// refs-check <char.HA6> <bank.cg>: the frame references still show the same pictures after permute / insert / delete (the fix-up end to end)
static int CmdRefsCheck(int argc, char **argv) {
	if (argc < 2) return 2;
	FrameData fd; if (!fd.load(argv[0])) { printf("FAIL cannot load %s\n", argv[0]); return 1; }
	std::vector<uint8_t> b; cgm::Bank bk; std::string err; if (!ParseFile(argv[1], b, bk, err)) { printf("FAIL %s\n", err.c_str()); return 1; }
	cgm::UsageIndex u0; u0.build(fd, (int)bk.images.size());
	int used = 0; for (size_t n = 0; n < u0.byImage.size(); n++) used += !u0.byImage[n].empty();
	bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s\n", m.c_str()); bad = true; };
	struct Ref { int p, f, l, id; }; std::vector<Ref> before;
	for (size_t n = 0; n < u0.byImage.size(); n++) for (auto &r : u0.byImage[n]) before.push_back({r.pattern, r.frame, r.layer, (int)n});
	auto check = [&](const cgm::Bank &nb, const char *what, const std::vector<int> &remap) {
		cgm::UsageIndex u1; u1.build(fd, (int)nb.images.size()); int shown = 0;
		for (const Ref &r : before) {
			const int want = remap[r.id]; const auto &layer = fd.get_sequence(r.p)->frames[r.f].AF.layers[r.l];
			if (layer.spriteId != want) { F(std::string(what) + ": layer reference not rewritten"); return; }
			if (want >= 0 && !SameDecoded(bk, r.id, nb, want)) { F(std::string(what) + ": a layer now shows a different picture"); return; }
			shown++;
		}
		printf("OK  %s: %d layer references still show the same pictures\n", what, shown);
	};
	{   // permute
		std::mt19937 rng(7); std::vector<int> order = cgm::IdentityRemap(bk.images.size()); std::shuffle(order.begin(), order.end(), rng);
		cgm::Bank w = bk; std::vector<int> remap; if (!cgm::Permute(w, order, remap, &err)) F(err);
		else { cgm::RemapSprites(fd, remap); check(w, "random permutation", remap); cgm::RemapSprites(fd, cgm::InvertRemap(remap, bk.images.size())); }
	}
	{   // insert at the front
		cgm::Bank w = bk; std::vector<int> remap; int id; auto spr = Sprite(32, 32, 0); cgm::NewImageSpec sp;
		if (!cgm::AddImage(w, 0, sp, spr.data(), 32, 32, remap, &id, &err)) F(err); else { cgm::RemapSprites(fd, remap); check(w, "insert at 0", remap); cgm::RemapSprites(fd, cgm::InvertRemap(remap, bk.images.size())); }
	}
	{   // delete an unused image in the middle
		int victim = -1; for (size_t n = bk.images.size() / 2; n < bk.images.size() && victim < 0; n++) if (u0.byImage[n].empty() && bk.images[n].drawable()) victim = (int)n;
		if (victim >= 0) { cgm::Bank w = bk; std::vector<int> remap; if (!cgm::DeleteImage(w, victim, true, remap, &err)) F(err); else { cgm::RemapSprites(fd, remap); check(w, "delete unused image", remap); cgm::RemapSprites(fd, cgm::InvertRemap(remap, bk.images.size())); } }
	}
	{   // delete a used image: references are cleared and recorded, restoring puts them back
		int victim = -1; for (size_t n = 0; n < u0.byImage.size() && victim < 0; n++) if (!u0.byImage[n].empty()) victim = (int)n;
		cgm::Bank w = bk; std::vector<int> remap; std::vector<cgm::ClearedRef> cl;
		if (victim >= 0 && cgm::DeleteImage(w, victim, true, remap, &err)) {
			const int n1 = cgm::RemapSprites(fd, remap, &cl); if (cl.size() != u0.byImage[victim].size()) F("cleared reference count differs from the usage count");
			cgm::RemapSprites(fd, cgm::InvertRemap(remap, bk.images.size())); cgm::RestoreCleared(fd, cl); (void)n1;
		}
	}
	cgm::UsageIndex u2; u2.build(fd, (int)bk.images.size()); int diff = 0; for (size_t n = 0; n < u2.byImage.size() && n < u0.byImage.size(); n++) diff += u2.byImage[n].size() != u0.byImage[n].size();
	if (diff) F("after undoing every step the references are not back where they were");
	printf("SECTION cgm-refs: %s | %d images used by %zu layers, undo of every remap restores the references\n", bad ? "FAIL" : "pass", used, before.size());
	return bad ? 1 : 0;
}

int main(int argc, char **argv) {
	if (argc < 2) { puts("usage: cgmtool check|info|export|import|roundtrip ..."); return 2; }
	std::string c = argv[1];
	if (c == "check") return CmdCheck(argc - 2, argv + 2);
	if (c == "info") return CmdInfo(argc - 2, argv + 2);
	if (c == "export") return CmdExport(argc - 2, argv + 2);
	if (c == "import") return CmdImport(argc - 2, argv + 2);
	if (c == "roundtrip") return CmdRoundtrip(argc - 2, argv + 2);
	if (c.compare(0, 4, "pal-") == 0) return CmdPal(c, argc - 2, argv + 2);
	if (c == "struct-check") return CmdStructCheck(argc - 2, argv + 2);
	if (c == "refs-check") return CmdRefsCheck(argc - 2, argv + 2);
	if (c == "recolor-bank") return CmdRecolorBank(argc - 2, argv + 2);
	puts("unknown command"); return 2;
}
