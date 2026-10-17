#include "cgm_ccpal.h"
#include "../png_writer.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace cgm {

CcSlotPlace CcPlaceOf(int palNum) {
	CcSlotPlace p; const int id = std::max(0, palNum - 1);
	p.bank = id / kCcPalettesPerBank; p.local = id % kCcPalettesPerBank; p.page = p.local / 6; p.row = p.local % 6;
	return p;
}
int CcPalNum(int bank, int local) { return bank * kCcPalettesPerBank + local + 1; }

int CcCharaFromStem(const std::string &stem) {
	static const std::map<std::string, int> k = {
		{"SION", 0}, {"ARC", 1}, {"CIEL", 2}, {"AKIHA", 3}, {"HISUI", 5}, {"KOHAKU", 6}, {"KOHAKU_M", 6}, {"SHIKI", 7},
		{"MIYAKO", 8}, {"WARAKIA", 9}, {"NERO", 10}, {"V_SION", 11}, {"WARC", 12}, {"AKAAKIHA", 13}, {"M_HISUI", 14},
		{"M_HISUI_P", 14}, {"M_HISUI_M", 14}, {"NANAYA", 15}, {"SATSUKI", 17}, {"LEN", 18}, {"P_CIEL", 19}, {"NECO", 20},
		{"NECO_P", 20}, {"AOKO", 22}, {"WLEN", 23}, {"NECHAOS", 25}, {"KISHIMA", 28}, {"S_AKIHA", 29}, {"RIES", 30},
		{"ROA", 31}, {"RYOUGI", 33}, {"P_ARC", 51},
		// bosses (docs/cc_select_patch/DESIGN.md section 4): PovertyCaster's table must learn these too
		{"G_AKIHA", 16}, {"G_CHAOS", 27}, {"HERMES", 32}, {"G_AKIHA_MBAC", 36},
	};
	std::string up = stem; for (char &c : up) c = (char)toupper((unsigned char)c);
	const auto it = k.find(up);
	return it == k.end() ? -1 : it->second;
}

std::string CcPaletteFileName(int chara, int palNum) { return std::to_string(chara) + "_" + std::to_string(palNum) + ".png"; }

bool WriteCcPalettePng(const std::string &path, const uint32_t pal[256], std::string *err) {
	uint8_t idx[256]; for (int i = 0; i < 256; i++) idx[i] = (uint8_t)i;
	uint32_t p[256]; for (int i = 0; i < 256; i++) p[i] = pal[i] | 0xFF000000u;   // the PLTE carries RGB; PovertyCaster sets the alpha flag itself
	std::string e; if (!WritePngIndexed(path, idx, 16, 16, p, e)) { if (err) *err = e; return false; }
	return true;
}

// The PLTE chunk, read the way PovertyCaster's parsePngPalette does (colour type 3 only).
bool ReadCcPalettePng(const std::string &path, uint32_t pal[256], std::string *err) {
	std::ifstream f(std::filesystem::path(Utf8ToWide(path)), std::ios::binary);
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	if (b.size() < 8 || memcmp(b.data(), sig, 8)) { if (err) *err = "not a PNG"; return false; }
	bool indexed = false; size_t off = 8;
	while (off + 12 <= b.size()) {
		const uint32_t len = ((uint32_t)b[off] << 24) | ((uint32_t)b[off + 1] << 16) | ((uint32_t)b[off + 2] << 8) | b[off + 3];
		const uint8_t *type = &b[off + 4], *data = &b[off + 8];
		if (len > b.size() - off - 12) break;
		if (!memcmp(type, "IHDR", 4) && len >= 10) indexed = data[9] == 3;
		if (!memcmp(type, "PLTE", 4)) {
			if (!indexed || len % 3 || !len) break;
			for (int i = 0; i < 256; i++) pal[i] = 0;
			for (uint32_t i = 0; i < len / 3 && i < 256; i++) pal[i] = Rgb(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
			return true;
		}
		if (!memcmp(type, "IEND", 4)) break;
		off += 12 + len;
	}
	if (err) *err = "not an indexed PNG with a palette (PovertyCaster skips it)";
	return false;
}

bool ExportCcPalettePngs(const PalSet &set, int chara, const std::vector<CcExportItem> &items, const std::string &dir, CcExportResult &res) {
	std::string e;
	if (chara < 0) { res.error = "unknown character id (the .pal name is not in PovertyCaster's table)"; return false; }
	if (!CreateDirectoriesUtf8(dir, e)) { res.error = e; return false; }
	for (const CcExportItem &it : items) {
		if (it.sourcePalette < 0 || it.sourcePalette >= set.count) { res.error = "palette " + std::to_string(it.sourcePalette) + " is not in the file"; return false; }
		if (it.palNum < 1 || it.palNum > kCcMaxPalettes) { res.error = "slot " + std::to_string(it.palNum) + " is outside 1.." + std::to_string(kCcMaxPalettes); return false; }
		const std::string p = dir + "/" + CcPaletteFileName(chara, it.palNum);
		if (!WriteCcPalettePng(p, set.pal(it.sourcePalette), &e)) { res.error = e; return false; }
		res.written.push_back(p);
	}
	return true;
}

bool BuildCcPalSet(const PalSet &set, const std::vector<CcExportItem> &items, int count, PalSet &out, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (set.count < 1) return fail("empty palette file");
	int need = count; for (const CcExportItem &it : items) need = std::max(need, it.palNum);
	if (need > kCcMaxPalettes) return fail("more than " + std::to_string(kCcMaxPalettes) + " palettes");
	const PalSet src = set.offsetDw == 1 ? set : set.converted(false);
	uint32_t filler[256]; memcpy(filler, src.pal(0), 1024); filler[0] = 0;
	out = PalSet::makeMbaacc(std::max(need, src.count), filler);
	for (int i = 0; i < src.count; i++) memcpy(out.pal(i), src.pal(i), 1024);
	for (const CcExportItem &it : items) {
		if (it.sourcePalette < 0 || it.sourcePalette >= set.count || it.palNum < 1) return fail("bad palette item");
		memcpy(out.pal(it.palNum - 1), set.pal(it.sourcePalette), 1024);
	}
	return true;
}

std::vector<int> CcSwatchIndices(const PalSet &stock, const uint8_t *px, int w, int h) {
	std::vector<std::pair<int, int>> score;   // (pixels matched, entry)
	if (stock.count < kCcPalettesPerBank || w < 256 || h < 96) return {};
	for (int e = 1; e < 256; e++) {
		int total = 0; bool all = true;
		for (int i = 0; i < kCcPalettesPerBank && all; i++) {
			const uint32_t c = stock.pal(i)[e] & 0xFFFFFFu; int hits = 0;
			const int x0 = 32 * (i & 7), y0 = 16 * (i >> 3);
			for (int y = y0; y < y0 + 16; y++) for (int x = x0; x < x0 + 32; x++) {
				const uint8_t *p = &px[((size_t)y * w + x) * 4];
				if (p[3] && (Rgb(p[0], p[1], p[2]) & 0xFFFFFFu) == c) hits++;
			}
			if (!hits) all = false; total += hits;
		}
		if (all) score.push_back({total, e});
	}
	std::sort(score.begin(), score.end(), [](auto &a, auto &b) { return a.first > b.first; });
	std::vector<int> r; for (auto &s : score) r.push_back(s.second);
	return r;
}

std::string CcSwatchJson(const std::map<int, std::vector<int>> &per) {
	std::string s = "{";
	bool first = true;
	for (auto &kv : per) {
		if (kv.second.empty()) continue;
		s += (first ? "" : ",") + std::string("\"") + std::to_string(kv.first) + "\":[" + std::to_string(kv.second[0]) + "," + std::to_string(kv.second.size() > 1 ? kv.second[1] : kv.second[0]) + "]";
		first = false;
	}
	return s + "}\n";
}

} // namespace cgm
