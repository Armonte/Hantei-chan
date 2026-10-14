// pattex: .pat part textures <-> PNG (src/parts/pat_texconv.h). Headless (no GL).
//   pattex list <x.pat>
//   pattex export <x.pat> <outdir>                      every texture as NNN_<name>.png (straight RGBA)
//   pattex import <x.pat> <index> <in.png> -o <out.pat> [--type 1|5|21|22]   (default: the texture's own kind)
//   pattex check <x.pat>...                             decode every texture; raw kinds survive PNG -> import byte-exact; DXT encode -> decode
//                                                       stays close; an imported texture is saved and reloaded unchanged
#include "parts/parts.h"
#include "parts/pat_texconv.h"
#include "png_writer.h"
#include <cmath>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static bool ArgVal(int argc, char **argv, const char *k, std::string &v) { for (int i = 0; i + 1 < argc; i++) if (!strcmp(argv[i], k)) { v = argv[i + 1]; return true; } return false; }
static const char *Kind(const PartGfx<> &g) {
	if (g.imageSize == 0 && g.data) return "PGTX 32-bit (MBAACC)";
	switch (g.type) { case 1: return "DXT1"; case 5: return "DXT5"; case 21: return "A8R8G8B8"; case 22: return "X8R8G8B8"; }
	return "?";
}

static int Check(int argc, char **argv) {
	int pass = 0, fail = 0, tex = 0, raw = 0, dxt = 0; double worst = 1e9;
	const std::string tmp = "pattex_tmp"; std::error_code ec; std::filesystem::create_directories(tmp, ec);
	for (int i = 0; i < argc; i++) {
		Parts &p = *new Parts(nullptr);   // never destroyed: Parts::Free needs a GL context
		if (!p.Load(argv[i])) { printf("FAIL %s: load\n", argv[i]); fail++; continue; }
		bool bad = false; auto F = [&](const std::string &m) { printf("FAIL %s: %s\n", argv[i], m.c_str()); bad = true; };
		for (size_t t = 0; t < p.gfxMeta.size(); t++) {
			PartGfx<> &g = p.gfxMeta[t];
			if (g.w <= 0 || (!g.imageData && !g.data)) continue;
			std::vector<uint8_t> px; int w, h; std::string e;
			if (!pattex::Decode(g, px, w, h, &e)) { F("texture " + std::to_string(t) + ": " + e); continue; }
			tex++;
			const std::string png = tmp + "/t.png";
			if (!pattex::ExportPng(g, png, &e)) { F("export: " + e); continue; }
			PartGfx<> c = g;   // shallow: the import below gives it its own buffers
			if (pattex::IsRawBgra(g.type) || (g.imageSize == 0 && g.data)) {
				if (g.type == 22) continue;   // X8: the 4th byte is not stored in the PNG (drawn opaque); the decode check above covers it
				if (!pattex::ImportPng(c, png, -1, &e)) { F("import: " + e); continue; }
				const char *a = g.imageSize ? g.imageData : g.data, *b = c.imageSize ? c.imageData : c.data;
				const size_t n = (size_t)w * h * 4;
				if (memcmp(a, b, n)) { // straight alpha 0 pixels may carry colour in the source: compare only visible ones
					size_t diff = 0; for (size_t k = 0; k < n; k += 4) if ((uint8_t)a[k + 3] && memcmp(a + k, b + k, 4)) diff++;
					if (diff) F("texture " + std::to_string(t) + ": PNG -> import changed " + std::to_string(diff) + " visible pixel(s)");
				}
				raw++;
			} else {
				if (!pattex::ImportPng(c, png, -1, &e)) { F("import: " + e); continue; }
				std::vector<uint8_t> back; int w2, h2; pattex::Decode(c, back, w2, h2, &e);
				double se = 0; size_t n = 0;
				for (size_t k = 0; k < px.size(); k += 4) { if (!px[k + 3] && !back[k + 3]) continue; for (int q = 0; q < 3; q++) { const double d = (double)px[k + q] - back[k + q]; se += d * d; } n += 3; }
				const double psnr = n && se ? 10.0 * std::log10(255.0 * 255.0 * n / se) : 99.0;
				worst = std::min(worst, psnr); dxt++;
				if (psnr < 28.0) F("texture " + std::to_string(t) + ": DXT re-encode PSNR " + std::to_string(psnr));
			}
		}
		// an imported texture goes through Save -> Load
		for (size_t t = 0; t < p.gfxMeta.size() && !bad; t++) {
			PartGfx<> &g = p.gfxMeta[t]; std::vector<uint8_t> px; int w, h; std::string e;
			if (g.w <= 0 || !pattex::Decode(g, px, w, h, &e)) continue;
			for (size_t k = 0; k < px.size(); k += 4 * 97) { px[k] ^= 0x40; px[k + 3] = 255; }
			if (!pattex::Encode(g, px.data(), w, h, g.imageSize ? 21 : -1, &e)) { F("encode: " + e); break; }
			const std::string out = tmp + "/x.pat";
			if (!p.Save(out.c_str())) { F("save after import"); break; }
			Parts &q = *new Parts(nullptr); if (!q.Load(out.c_str()) || q.gfxMeta.size() <= t) { F("reload after import"); break; }
			std::vector<uint8_t> back; int w2, h2; if (!pattex::Decode(q.gfxMeta[t], back, w2, h2, &e) || back != px) F("an imported texture does not survive save -> load");
			break;
		}
		bad ? fail++ : pass++;
	}
	std::filesystem::remove_all(tmp, ec);
	printf("SECTION pat-textures: pass %d fail %d | %d textures decoded; %d raw PNG round trips exact; %d DXT re-encodes (worst PSNR %.1f dB); import -> save -> load exact\n", pass, fail, tex, raw, dxt, dxt ? worst : 0.0);
	return fail ? 1 : 0;
}

static int RealMain(int argc, char **argv);
// Parts' destructors expect a GL context: leave like pat_roundtrip does, after flushing
int main(int argc, char **argv) { const int r = RealMain(argc, argv); fflush(stdout); _exit(r); }

static int RealMain(int argc, char **argv) {
	if (argc < 3) { puts("usage: pattex list|export|import|check ..."); return 2; }
	const std::string c = argv[1];
	if (c == "check") return Check(argc - 2, argv + 2);
	Parts &p = *new Parts(nullptr);
	if (!p.Load(argv[2])) { printf("error: cannot load %s\n", argv[2]); return 1; }
	if (c == "list") {
		for (size_t t = 0; t < p.gfxMeta.size(); t++) { const auto &g = p.gfxMeta[t]; printf("%3zu %-32s %4d x %-4d %s\n", t, g.name.c_str(), g.w, g.h, Kind(g)); }
		return 0;
	}
	if (c == "export" && argc >= 4) {
		std::string e; CreateDirectoriesUtf8(argv[3], e); int n = 0;
		for (size_t t = 0; t < p.gfxMeta.size(); t++) {
			const auto &g = p.gfxMeta[t]; if (g.w <= 0) continue;
			char nm[256]; snprintf(nm, sizeof(nm), "%s/%03zu_%s.png", argv[3], t, std::filesystem::path(g.name.c_str()).stem().string().c_str());
			if (!pattex::ExportPng(g, nm, &e)) { printf("texture %zu: %s\n", t, e.c_str()); continue; }
			n++;
		}
		printf("exported %d texture(s) to %s\n", n, argv[3]); return 0;
	}
	if (c == "import" && argc >= 5) {
		std::string out, v, e; if (!ArgVal(argc, argv, "-o", out)) { puts("-o <out.pat> is required"); return 2; }
		int type = -1; if (ArgVal(argc, argv, "--type", v)) type = atoi(v.c_str());
		const size_t t = (size_t)atoi(argv[3]); if (t >= p.gfxMeta.size()) { puts("no such texture"); return 1; }
		if (!pattex::ImportPng(p.gfxMeta[t], argv[4], type, &e)) { printf("error: %s\n", e.c_str()); return 1; }
		if (!p.Save(out.c_str())) { puts("could not save"); return 1; }
		printf("texture %zu <- %s (%s %dx%d), wrote %s\n", t, argv[4], Kind(p.gfxMeta[t]), p.gfxMeta[t].w, p.gfxMeta[t].h, out.c_str()); return 0;
	}
	puts("bad arguments"); return 2;
}
