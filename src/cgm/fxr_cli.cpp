// cgmtool fxr ...: command line front end of the effect recolour authoring core (docs/cg/fxrecolor_config_spec.md).
//   fxr classify <char.HA6> <bank.cg> [--csv]       effect classification summary (+ one CSV row per effect pattern)
//   fxr check <rules.ini>                           parse -> write -> parse is lossless; prints the parser messages
//   fxr shade <rules.ini> <ruleId> [slot [pal]]     stdin: W H then W*H*4 RGBA bytes; stdout: the same recoloured by the rule (the runtime's shader maths)
//   fxr apply <bank.cg> <rules.ini> <slot> <pal> <outdir>   PNGs of every recoloured fixed sprite (pal = a .pal file, MBAACC layout)
//   fxr dgv                                          writes DGV's Akiha starter ruleset to stdout
#include "fxr_core.h"
#include "cgm_palette.h"
#include "../framedata.h"
#include "../png_writer.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static bool Slurp(const std::string &p, std::vector<uint8_t> &b) {
	std::ifstream f(p, std::ios::binary); if (!f) return false;
	b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}
static std::string SlurpText(const std::string &p, bool *ok = nullptr) {
	std::vector<uint8_t> b; const bool r = Slurp(p, b); if (ok) *ok = r; return std::string(b.begin(), b.end());
}

static int Classify(int argc, char **argv) {
	if (argc < 2) return 2;
	FrameData fd; if (!fd.load(argv[0])) { printf("FAIL cannot load %s\n", argv[0]); return 1; }
	std::vector<uint8_t> b; cgm::Bank bk; std::string err;
	if (!Slurp(argv[1], b) || !cgm::Bank::parse(b.data(), b.size(), bk, &err)) { printf("FAIL bank: %s\n", err.c_str()); return 1; }
	cgm::UsageIndex u; u.build(fd, (int)bk.images.size());
	cgm::FxClassification c; cgm::ClassifyEffects(bk, u, fd, c);
	printf("fixed-colour images %d (drawn by a pattern %d, by 2+ patterns %d = %d%%), effect patterns %zu\n", c.fixedImages, c.usedFixedImages, c.sharedFixedImages,
	       c.usedFixedImages ? 100 * c.sharedFixedImages / c.usedFixedImages : 0, c.patterns.size());
	if (argc > 2 && std::string(argv[2]) == "--csv") {
		puts("pattern,name,layers,fixed_layers,score,fixed_sprites,follow_sprites,shared_sprites,blend_mask");
		for (const auto &r : c.patterns) printf("%d,\"%s\",%d,%d,%d,%zu,%d,%d,%u\n", r.pattern, r.name.c_str(), r.layers, r.fixedLayers, r.effectScore, r.fixedSprites.size(), r.followSprites, r.sharedSprites, r.blendMask);
	}
	return 0;
}

static int Check(int argc, char **argv) {
	if (argc < 1) return 2;
	bool ok; const std::string t = SlurpText(argv[0], &ok); if (!ok) { printf("FAIL cannot read %s\n", argv[0]); return 1; }
	cgm::fx::CharRules a, b; std::string e1, e2;
	const int n = cgm::LoadRules(t, a, e1);
	const std::string w = cgm::SaveRules(a);
	const int m = cgm::LoadRules(w, b, e2);
	const std::string w2 = cgm::SaveRules(b);
	if (!e1.empty()) printf("parser: %s", e1.c_str());
	const bool same = n == m && w == w2 && e2.empty();
	printf("SECTION fxr-ini: %s | %d rules, write->parse->write %s\n", same ? "pass" : "FAIL", n, same ? "identical" : "DIFFERS");
	return same ? 0 : 1;
}

static bool LoadPal(const char *path, int slot, uint32_t out[256]) {
	cgm::PalSet s; std::vector<uint8_t> b; std::string err;
	if (!Slurp(path, b) || !cgm::PalSet::parse(b.data(), b.size(), s, &err) || slot >= s.count) return false;
	memcpy(out, s.pal(slot), 1024); return true;
}

static int Shade(int argc, char **argv) {
	if (argc < 2) return 2;
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
#endif
	cgm::fx::CharRules c; std::string err; cgm::LoadRules(SlurpText(argv[0]), c, err);
	const int ri = cgm::FindRuleIndex(c, argv[1]); if (ri < 0) { fprintf(stderr, "no rule %s\n", argv[1]); return 1; }
	const int slot = argc > 2 ? atoi(argv[2]) : 0;
	uint32_t pal[256] = {}; const bool havePal = argc > 3 && LoadPal(argv[3], slot, pal);
	int w = 0, h = 0; if (scanf("%d %d%*c", &w, &h) != 2) return 1;
	std::vector<uint8_t> px((size_t)w * h * 4);
	if (fread(px.data(), 1, px.size(), stdin) != px.size()) return 1;
	uint32_t rp[256]; cgm::ToRuntimePalette(havePal ? pal : nullptr, rp);
	cgm::fx::Packed p; cgm::fx::pack(c.rules[ri], slot, havePal ? rp : nullptr, p);
	for (size_t i = 0; i + 3 < px.size(); i += 4) {
		if (!px[i + 3]) continue;
		const cgm::fx::Rgb o = cgm::fx::applyCpu(p, { px[i] / 255.0f, px[i + 1] / 255.0f, px[i + 2] / 255.0f });
		px[i] = (uint8_t)(cgm::fx::clamp01(o.r) * 255 + 0.5f); px[i + 1] = (uint8_t)(cgm::fx::clamp01(o.g) * 255 + 0.5f); px[i + 2] = (uint8_t)(cgm::fx::clamp01(o.b) * 255 + 0.5f);
	}
	fwrite(px.data(), 1, px.size(), stdout); return 0;
}

static int Apply(int argc, char **argv) {
	if (argc < 5) return 2;
	std::vector<uint8_t> b; cgm::Bank bk; std::string err;
	if (!Slurp(argv[0], b) || !cgm::Bank::parse(b.data(), b.size(), bk, &err)) { printf("FAIL bank: %s\n", err.c_str()); return 1; }
	cgm::fx::CharRules c; cgm::LoadRules(SlurpText(argv[1]), c, err);
	const int slot = atoi(argv[2]); uint32_t pal[256] = {};
	if (!LoadPal(argv[3], slot, pal)) { printf("FAIL cannot read palette %d of %s\n", slot, argv[3]); return 1; }
	std::filesystem::create_directories(argv[4]);
	int written = 0;
	for (size_t i = 0; i < bk.images.size(); i++) {
		if (!bk.images[i].present || bk.images[i].type <= 0) continue;
		const cgm::fx::Rule *r = nullptr;
		for (const auto &x : c.rules) if (x.enabled && (x.sprites.empty() || std::find(x.sprites.begin(), x.sprites.end(), (int)i) != x.sprites.end()) && x.patterns.empty()) { r = &x; break; }
		if (!r) continue;
		cgm::Rgba out; if (!cgm::RecolorImage(bk, (int)i, r, slot, pal, out)) continue;
		char nm[32]; snprintf(nm, sizeof nm, "%04zu.png", i); std::string we;
		if (WritePngRgba((std::filesystem::path(argv[4]) / nm).string(), out.px.data(), out.w, out.h, we)) written++;
	}
	printf("wrote %d recoloured sprites\n", written); return 0;
}

int FxrMain(int argc, char **argv) {
	if (argc < 1) { puts("usage: cgmtool fxr classify|check|shade|apply|dgv ..."); return 2; }
	const std::string c = argv[0];
	if (c == "classify") return Classify(argc - 1, argv + 1);
	if (c == "check") return Check(argc - 1, argv + 1);
	if (c == "shade") return Shade(argc - 1, argv + 1);
	if (c == "apply") return Apply(argc - 1, argv + 1);
	if (c == "dgv") { fputs(cgm::DgvAkihaIni(), stdout); return 0; }
	puts("unknown fxr command"); return 2;
}
