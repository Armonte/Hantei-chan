// cgmtex: the CG manager's unified CG + .pat texture layer (cgm_texsrc.h) from the command line, and its tests.
//   cgmtex selftest                                   unit tests of the shared layer (synthetic data, no game files)
//   cgmtex list <x.cg|x.pat>                          the browser's list: id, size, format, name
//   cgmtex check [--max N] <x.cg|x.pat>...            per file: container byte-exact (parse -> serialize); every item decoded; PNG export ->
//                                                     import of the first N items (default 64, 0 = all) changes no stored byte; one item
//                                                     replaced -> saved -> reloaded decodes as edited; .pat: the parts sampling the
//                                                     replaced texture follow it. Prints "SECTION texsrc-<kind>: pass P fail F | ..."
//   cgmtex refs <x.ha6> <x.cg|x.pat>                  users of every image / texture (pattern layers; .pat: parts and part sets too)
//   cgmtex stripe <x.cg> <x.ha6> <x.pal> <out.cg> [patterns]   recolour in stripes every CG image the patterns draw (default 0-40)
//   cgmtex modsave <game> <gameDir> <key> <file>      write a file into the UNI2-IM / MBTL-IM mod folder (original kept as .hcbak)
//   cgmtex modrestore <game> <gameDir> <key>          undo modsave
#include <windows.h>
#include "cgm_texsrc.h"
#include "cgm_bank.h"
#include "cgm_io.h"
#include "cgm_palette.h"
#include "../parts/parts.h"
#include "../parts/pat_texconv.h"
#include "../framedata.h"
#include "../framedata_ha4.h"
#include "../png_writer.h"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool Slurp(const std::string &p, std::vector<uint8_t> &b) {
	std::ifstream f(fs::u8path(p), std::ios::binary); if (!f) return false;
	b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}
static bool EndsWith(const std::string &s, const char *e) { const size_t n = strlen(e); if (s.size() < n) return false; return !_stricmp(s.c_str() + s.size() - n, e); }
static std::string TmpDir(const char *tag) {
	char base[MAX_PATH]; GetTempPathA(MAX_PATH, base);
	std::string d = std::string(base) + "hc_cgmtex_" + tag + "_" + std::to_string(GetCurrentProcessId());
	std::error_code ec; fs::create_directories(d, ec); return d;
}

// A file opened as a TexSource plus how to serialize its container.
struct Opened {
	std::unique_ptr<cgm::Bank> bank; std::unique_ptr<cgm::BmpCutterIO> io;
	Parts *parts = nullptr;   // never destroyed (Parts::Free needs GL)
	std::unique_ptr<cgm::TexSource> src;
	std::string path;
	std::vector<uint8_t> embedded;   // DAT: the embedded CG as loaded (the byte-exact reference)
	bool serialize(std::vector<uint8_t> &out, const std::string &tmp) {
		if (bank) { bank->serialize(out); return true; }
		const std::string p = tmp + "/s.pat";
		if (!parts->Save(p.c_str())) return false;
		return Slurp(p, out);
	}
};

static bool Open(const std::string &path, Opened &o, std::string &err) {
	o.path = path;
	if (EndsWith(path, ".pat")) {
		o.parts = new Parts(nullptr);
		if (!o.parts->Load(path.c_str())) { err = "cannot load the .pat"; return false; }
		o.src.reset(new cgm::PatTexSource(*o.parts));
		return true;
	}
	std::vector<uint8_t> b; if (!Slurp(path, b)) { err = "cannot read"; return false; }
	if (b.size() > 11 && memcmp(b.data(), "BMP Cutter", 10)) {   // ActCadenza / ReAct .DAT: its embedded CG
		FrameData fd;
		if (!fd.load(path.c_str()) || !fd.isHA4() || fd.m_ha4->cg.empty()) { err = "not a CG bank (and no DAT with an embedded CG)"; return false; }
		b.assign(fd.m_ha4->cg.begin(), fd.m_ha4->cg.end());
		o.embedded = b;
	}
	o.bank.reset(new cgm::Bank);
	if (!cgm::Bank::parse(b.data(), b.size(), *o.bank, &err)) return false;
	o.io.reset(new cgm::BmpCutterIO(*o.bank));
	o.src.reset(new cgm::CgTexSource(*o.io));
	return true;
}

// ---------------------------------------------------------------- selftest
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// Minimal BankIO: one 4x4 32-bit image, replace stores what it gets.
class FakeIO : public cgm::BankIO {
public:
	cgm::Rgba img; int replaces = 0;
	FakeIO() { img.w = img.h = 4; img.px.assign(64, 0); for (int i = 0; i < 16; i++) { img.px[i * 4] = (uint8_t)(i * 10); img.px[i * 4 + 3] = i < 8 ? 255 : 0; } }
	int slots() const override { return 2; }
	bool info(int n, cgm::ImgInfo &o) const override { o = cgm::ImgInfo(); if (n == 1) return true; o.present = o.drawable = true; o.type = 1; o.w = o.h = 4; o.x2 = o.y2 = 3; strcpy(o.name, "fake"); return n == 0; }
	bool decode(int n, cgm::Rgba &o, const uint32_t *) const override { if (n) return false; o = img; return true; }
	bool indexable(int) const override { return false; }
	bool decodeIndexed(int, std::vector<uint8_t> &, uint32_t *, const uint32_t *) const override { return false; }
	void defaultPalette(uint32_t pal[256]) const override { memset(pal, 0, 1024); }
	int paletteSlots() const override { return 1; }
	void slotPalette(int, uint32_t pal[256]) const override { memset(pal, 0, 1024); }
	bool replace(int, const uint8_t *rgba, int w, int h, std::string *, cgm::ReplaceReport *, const std::vector<uint8_t> *) override { img.w = w; img.h = h; img.px.assign(rgba, rgba + w * h * 4); replaces++; return true; }
};

static int SelfTest() {
	const std::string tmp = TmpDir("self");
	// CG side
	{
		FakeIO io; cgm::CgTexSource s(io);
		cgm::TexEntry e; CHECK(s.entry(0, e) && e.present && e.w == 4 && e.name == "fake");
		CHECK(s.entry(1, e) && !e.present);
		cgm::Rgba px; CHECK(s.decode(0, px));
		cgm::Rgba same = px; for (int i = 8; i < 16; i++) same.px[i * 4] = 77;   // only transparent pixels differ
		bool ch = true; std::string err; CHECK(s.replace(0, same, &err, &ch) && !ch && io.replaces == 0);
		const std::string png = tmp + "/cg.png";
		CHECK(cgm::ExportPng(s, 0, png, &err));
		CHECK(cgm::ImportPng(s, 0, png, &err, &ch) && !ch && io.replaces == 0);   // PNG round trip: nothing re-encoded
		cgm::Rgba edit = px; edit.px[0] ^= 0xFF; CHECK(s.replace(0, edit, &err, &ch) && ch && io.replaces == 1);
		CHECK(!cgm::ExportPng(s, 1, png, &err));   // empty slot
	}
	// .pat side: a 128 x 128 A8R8G8B8 texture, two parts sampling it, one part set placing one of them
	{
		Parts &p = *new Parts(nullptr);
		p.gfxMeta.resize(2);
		PartGfx<> &g = p.gfxMeta[0];
		std::vector<uint8_t> rgba(128 * 128 * 4); for (size_t k = 0; k < rgba.size(); k++) rgba[k] = (uint8_t)(k * 7 + 3); for (size_t k = 3; k < rgba.size(); k += 4) rgba[k] = 255;
		std::string err; g.name = "t.dds"; g.type = 21; g.w = g.h = 1;
		g.imageData = new char[4](); g.imageSize = 4;   // makes it PGT2, so Encode writes 21 (not MBAACC PGTX)
		CHECK(pattex::Encode(g, rgba.data(), 128, 128, 21, &err));
		p.cutOuts.resize(3); p.cutOuts[0].id = 0; p.cutOuts[0].texture = 0; p.cutOuts[1].id = 1; p.cutOuts[1].texture = 1; p.cutOuts[2].id = 2; p.cutOuts[2].texture = 0;
		p.partSets.resize(2); p.partSets[1].groups.resize(1); p.partSets[1].groups[0].ppId = 2;
		cgm::PatTexSource s(p);
		CHECK(s.count() == 2);
		cgm::TexEntry e; CHECK(s.entry(0, e) && e.present && e.w == 128 && e.format == "A8R8G8B8");
		CHECK(s.entry(1, e) && !e.present);
		cgm::Rgba px; CHECK(s.decode(0, px) && px.px == rgba);
		const char *before = g.imageData;
		const std::string png = tmp + "/pat.png"; bool ch = true;
		CHECK(cgm::ExportPng(s, 0, png, &err));
		CHECK(cgm::ImportPng(s, 0, png, &err, &ch) && !ch && g.imageData == before);   // nothing re-encoded
		cgm::Rgba edit = px; for (size_t k = 0; k < edit.px.size(); k += 4) edit.px[k] = 255 - edit.px[k];
		CHECK(s.replace(0, edit, &err, &ch) && ch);
		cgm::Rgba back; CHECK(s.decode(0, back) && back.px == edit.px);
		CHECK(s.touchedParts.size() == 2 && p.cutOuts[0].ppte[0] == 128 && p.cutOuts[2].ppte[1] == 128 && p.cutOuts[1].ppte[0] == 0);
		cgm::Rgba bad; bad.w = 100; bad.h = 128; bad.px.assign(100 * 128 * 4, 0);
		CHECK(!s.replace(0, bad, &err, &ch) && !err.empty());   // PGT2 sides must be multiples of 128
		auto u = cgm::PatTextureUsers(p, 0, nullptr);
		int parts = 0, sets = 0; for (auto &x : u) { if (x.partSet < 0) parts++; else sets++; }
		CHECK(parts == 2 && sets == 1);
		CHECK(cgm::PatTextureUsers(p, 1, nullptr).size() == 1);
		// MBAACC PGTX stays PGTX
		PartGfx<> &m = p.gfxMeta[1]; m.w = m.h = 4; m.data = new char[64](); m.imageSize = 0; m.imageData = nullptr;
		CHECK(std::string(cgm::PatTextureFormat(p, 1)) == "PGTX 32-bit");
		cgm::Rgba small; small.w = small.h = 4; small.px.assign(64, 200);
		CHECK(s.replace(1, small, &err, &ch) && ch && m.imageSize == 0 && m.data);
		CHECK(std::string(cgm::PatTextureFormat(p, 1)) == "PGTX 32-bit");
	}
	// mod folder
	{
		CHECK(cgm::UsesModFolder("uni2") && cgm::UsesModFolder("mbtl") && !cgm::UsesModFolder("mbaacc"));
		CHECK(cgm::ModFolderPath("uni2", "C:\\g", "data/chr003/chr003.cg") == "C:\\g\\UNI2-IM\\Mods\\data\\chr003\\chr003.cg");
		CHECK(cgm::ModFolderPath("mbtl", "C:\\g\\", "./data/chr000/chr000.pat") == "C:\\g\\MBTL-IM\\Mods\\data\\chr000\\chr000.pat");
		CHECK(cgm::ModFolderPath("mbaacc", "C:\\g", "data/x.cg").empty());
		CHECK(cgm::GameKeyOf("C:\\dev\\hantei-chan\\gamedata\\uni2\\data\\chr003\\chr003.cg") == "data/chr003/chr003.cg");
		CHECK(cgm::GameKeyOf("C:/x/y.cg").empty());
		const std::string d = cgm::ModFolderPath("uni2", tmp, "data/chr003/a.cg");
		std::string err; std::vector<uint8_t> b1{1, 2, 3}, b2{4, 5}, r;
		CHECK(cgm::WriteModFile(d, b1, &err) && Slurp(d, r) && r == b1);
		CHECK(cgm::RestoreModFile(d, &err) && !fs::exists(fs::u8path(d)));   // there was nothing: removed
		CHECK(cgm::WriteModFile(d, b1, &err));                                // an existing file (someone else's) ...
		fs::remove(fs::u8path(d + ".hcbak"));
		CHECK(cgm::WriteModFile(d, b2, &err) && Slurp(d, r) && r == b2);      // ... is kept as .hcbak ...
		CHECK(cgm::WriteModFile(d, b1, &err) && Slurp(d + ".hcbak", r) && r == b1);
		CHECK(cgm::RestoreModFile(d, &err) && Slurp(d, r) && r == b1 && !fs::exists(fs::u8path(d + ".hcbak")));   // ... and put back
	}
	std::error_code ec; fs::remove_all(tmp, ec);
	printf("SECTION texsrc-unit: %s (%d failure(s))\n", g_fail ? "FAIL" : "pass", g_fail);
	return g_fail ? 1 : 0;
}

// ---------------------------------------------------------------- check
static int Check(int argc, char **argv) {
	int maxItems = 64, pass = 0, fail = 0; long items = 0, pngRt = 0, exact = 0; std::string kindName;
	std::vector<std::string> files;
	for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--max") && i + 1 < argc) maxItems = atoi(argv[++i]); else files.push_back(argv[i]); }
	const std::string tmp = TmpDir("check");
	for (const auto &f : files) {
		Opened o; std::string err; bool bad = false;
		auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", f.c_str(), m.c_str()); bad = true; };
		if (!Open(f, o, err)) { F("open: " + err); fail++; continue; }
		kindName = o.bank ? "cg" : "pat";
		std::vector<uint8_t> orig, base, after;
		Slurp(f, orig); if (!o.embedded.empty()) orig = o.embedded;
		if (!o.serialize(base, tmp)) { F("serialize"); fail++; continue; }
		if (base == orig) exact++;
		else if (o.bank) F("parse -> serialize is not byte-exact");   // the CG model guarantees it; Parts::Save re-packs (reported only)
		cgm::TexSource &s = *o.src; int done = 0, first = -1;
		for (int i = 0; i < s.count(); i++) {
			cgm::TexEntry e; if (!s.entry(i, e) || !e.present) continue;
			cgm::Rgba px; if (!s.decode(i, px)) { F("decode " + std::to_string(i)); continue; }
			items++;
			if (first < 0 && px.w > 0) first = i;
			if (maxItems && done >= maxItems) continue;
			const std::string png = tmp + "/i.png"; bool ch = true;
			if (!cgm::ExportPng(s, i, png, &err)) { F("export " + std::to_string(i) + ": " + err); continue; }
			if (!cgm::ImportPng(s, i, png, &err, &ch)) { F("import " + std::to_string(i) + ": " + err); continue; }
			if (ch) F("PNG round trip re-encoded item " + std::to_string(i));
			done++; pngRt++;
		}
		if (!o.serialize(after, tmp) || after != base) F("container bytes changed after the PNG round trips");
		// replace one item -> save -> reload -> decodes as edited
		if (first >= 0 && !bad) {
			cgm::Rgba px; s.decode(first, px);
			cgm::Rgba edit = px;
			if (o.bank) { for (int y = 0; y < px.h; y++) for (int x = 0; x < px.w; x++) memcpy(&edit.px[((size_t)y * px.w + x) * 4], &px.px[((size_t)y * px.w + (px.w - 1 - x)) * 4], 4); }   // mirror: palette-exact for every type
			else for (size_t k = 0; k < edit.px.size(); k += 4) { edit.px[k] = 255 - edit.px[k]; }
			bool ch = false;
			if (!s.replace(first, edit, &err, &ch)) F("replace: " + err);
			else {
				cgm::Rgba mem; s.decode(first, mem);
				std::vector<uint8_t> saved; o.serialize(saved, tmp);
				const std::string p2 = tmp + (o.bank ? "/r.cg" : "/r.pat");
				{ std::ofstream w(fs::u8path(p2), std::ios::binary); w.write((const char *)saved.data(), (std::streamsize)saved.size()); }
				Opened r; cgm::Rgba back;
				if (!Open(p2, r, err) || !r.src->decode(first, back) || back.px != mem.px) F("replaced item does not survive save -> reload");
				if (!o.bank) {
					auto *ps = static_cast<cgm::PatTexSource *>(o.src.get());
					for (int c : ps->touchedParts) if (c < (int)o.parts->cutOuts.size() && o.parts->cutOuts[c].ppte[0] != o.parts->gfxMeta[first].pgte[0]) F("part " + std::to_string(c) + " did not follow the texture");
				}
			}
		}
		bad ? fail++ : pass++;
	}
	std::error_code ec; fs::remove_all(tmp, ec);
	printf("SECTION texsrc-%s: pass %d fail %d | %ld items decoded, %ld PNG export -> import round trips with no byte changed, %d/%zu containers byte-exact on parse -> serialize\n",
	       kindName.c_str(), pass, fail, items, pngRt, (int)exact, files.size());
	return fail ? 1 : 0;
}

// ---------------------------------------------------------------- refs
static int Refs(const char *ha6, const char *file) {
	FrameData fd; if (!fd.load(ha6)) { printf("cannot load %s\n", ha6); return 1; }
	Opened o; std::string err; if (!Open(file, o, err)) { printf("cannot open %s: %s\n", file, err.c_str()); return 1; }
	int used = 0, unused = 0; long layers = 0;
	for (int i = 0; i < o.src->count(); i++) {
		cgm::TexEntry e; if (!o.src->entry(i, e) || !e.present) continue;
		auto u = o.bank ? cgm::CgImageUsers(fd, i) : cgm::PatTextureUsers(*o.parts, i, &fd);
		std::set<int> pats; int parts = 0, sets = 0; long l = 0;
		for (auto &x : u) { if (x.pattern >= 0) { pats.insert(x.pattern); l++; } else if (x.partSet >= 0) sets++; else parts++; }
		layers += l; (l || parts) ? used++ : unused++;
		if (!o.bank || l) printf("%4d %-28s %ld layer(s) in %zu pattern(s)%s\n", i, e.name.c_str(), l, pats.size(),
		                         o.bank ? "" : (", " + std::to_string(parts) + " part(s), " + std::to_string(sets) + " part-set slot(s)").c_str());
	}
	printf("refs: %d used, %d unused, %ld layer references\n", used, unused, layers);
	return 0;
}

// ---------------------------------------------------------------- stripe (in-game proof edit)
static int Stripe(const char *cg, const char *ha6, const char *pal, const char *out, const char *range) {
	int a = 0, b = 40; if (range) sscanf(range, "%d-%d", &a, &b);
	FrameData fd; if (!fd.load(ha6)) { printf("cannot load %s\n", ha6); return 1; }
	Opened o; std::string err; if (!Open(cg, o, err) || !o.bank) { printf("cannot open %s: %s\n", cg, err.c_str()); return 1; }
	uint32_t game[256] = {}, bankPal[256]; cgm::ReadPalFileColors(pal, game, &err, 0); o.io->defaultPalette(bankPal);
	// the stripe colour: an index whose bank-palette entry is unique (replace snaps colours to the bank palette) and whose game colour is vivid
	int best = -1; float score = -1;
	for (int k = 1; k < 256; k++) {
		bool uniq = true; for (int j = 0; j < 256 && uniq; j++) if (j != k && bankPal[j] == bankPal[k]) uniq = false;
		if (!uniq || !(bankPal[k] >> 24)) continue;
		const int r = game[k] & 255, g = (game[k] >> 8) & 255, bl = (game[k] >> 16) & 255, mx = std::max(r, std::max(g, bl)), mn = std::min(r, std::min(g, bl));
		const float sc = (float)(mx - mn) + mx * 0.25f;
		if (sc > score) { score = sc; best = k; }
	}
	if (best < 0) { puts("no usable stripe index"); return 1; }
	std::set<int> ids;
	for (int p = a; p <= b && p < fd.get_sequence_count(); p++) { Sequence *s = fd.get_sequence(p); if (!s) continue; for (auto &fr : s->frames) for (auto &L : fr.AF.layers) if (!L.usePat && L.spriteId >= 0) ids.insert(L.spriteId); }
	int changed = 0;
	for (int id : ids) {
		cgm::Rgba px; if (!o.src->decode(id, px)) continue;
		for (int y = 0; y < px.h; y++) { if ((y / 6) % 2) continue; for (int x = 0; x < px.w; x++) { uint8_t *q = &px.px[((size_t)y * px.w + x) * 4]; if (q[3] < 128) continue; memcpy(q, &bankPal[best], 4); q[3] = 255; } }
		bool ch = false; if (o.src->replace(id, px, &err, &ch) && ch) changed++;
	}
	std::vector<uint8_t> bytes; o.bank->serialize(bytes);
	std::ofstream w(fs::u8path(out), std::ios::binary); w.write((const char *)bytes.data(), (std::streamsize)bytes.size());
	printf("striped %d of %zu image(s) drawn by patterns %d-%d with index %d (game colour %08X) -> %s\n", changed, ids.size(), a, b, best, game[best], out);
	return w ? 0 : 1;
}

static int RealMain(int argc, char **argv) {
	if (argc < 2) { puts("usage: cgmtex selftest|list|check|refs|stripe|modsave|modrestore ..."); return 2; }
	const std::string c = argv[1];
	if (c == "selftest") return SelfTest();
	if (c == "check") return Check(argc - 2, argv + 2);
	if (c == "refs" && argc >= 4) return Refs(argv[2], argv[3]);
	if (c == "stripe" && argc >= 6) return Stripe(argv[2], argv[3], argv[4], argv[5], argc > 6 ? argv[6] : nullptr);
	if (c == "list" && argc >= 3) {
		Opened o; std::string err; if (!Open(argv[2], o, err)) { printf("error: %s\n", err.c_str()); return 1; }
		for (int i = 0; i < o.src->count(); i++) { cgm::TexEntry e; if (o.src->entry(i, e) && e.present) printf("%4d %5d x %-5d %-22s %s\n", i, e.w, e.h, e.format.c_str(), e.name.c_str()); }
		return 0;
	}
	if (c == "modsave" && argc >= 6) {
		const std::string dst = cgm::ModFolderPath(argv[2], argv[3], argv[4]); std::vector<uint8_t> b; std::string err;
		if (dst.empty() || !Slurp(argv[5], b)) { puts("bad game / key / file"); return 1; }
		if (!cgm::WriteModFile(dst, b, &err)) { printf("error: %s\n", err.c_str()); return 1; }
		printf("wrote %s (%zu bytes)\n", dst.c_str(), b.size()); return 0;
	}
	if (c == "modrestore" && argc >= 5) {
		const std::string dst = cgm::ModFolderPath(argv[2], argv[3], argv[4]); std::string err;
		if (dst.empty() || !cgm::RestoreModFile(dst, &err)) { printf("error: %s\n", err.c_str()); return 1; }
		printf("restored %s\n", dst.c_str()); return 0;
	}
	puts("bad arguments"); return 2;
}
// Parts' destructors expect a GL context: leave like pattex does, after flushing
int main(int argc, char **argv) { const int r = RealMain(argc, argv); fflush(stdout); _exit(r); }
