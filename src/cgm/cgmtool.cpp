// cgmtool: command line front end of the CG manager core (docs/cg/cg_manager_design.md).
//   cgmtool check <bank.cg>...        parse -> serialize must be byte identical; decode must equal CG::draw_texture for every image
//   cgmtool info <bank.cg>            image table summary
#include "cgm_bank.h"
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

int main(int argc, char **argv) {
	if (argc < 2) { puts("usage: cgmtool check|info <bank>..."); return 2; }
	std::string c = argv[1];
	if (c == "check") return CmdCheck(argc - 2, argv + 2);
	if (c == "info") return CmdInfo(argc - 2, argv + 2);
	puts("unknown command"); return 2;
}
