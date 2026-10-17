// cgmtool ac-moon3: rebase a converted ActCadenza HA6 onto the MBAACC character's .cg for the moon-3 set.
//   ac-moon3 <ac.ha6> <ac.cg> <mbaacc.cg> --ha6-out <c>_3.HA6 --cg-out <c>_3.cg [--pat-map map.tsv] [--report r.txt]
// Per CG sprite reference (usePat == 0), by ActCadenza image:
//   - the MBAACC bank has an image with the same BMP name and the same canvas size -> its MBAACC id (same art, MBAACC cut);
//   - otherwise the ActCadenza image is appended to <c>_3.cg (a copy of the MBAACC bank, every MBAACC id kept) with its own
//     canvas and bounds, so it draws exactly where ActCadenza draws it. This covers the images MBAACC lacks and the ones MBAACC
//     re-canvassed (the "AFOF fix": no offset delta is needed because the ActCadenza canvas is kept);
//   - an id the ActCadenza bank does not have (ActCadenza draws nothing) -> -1.
// 8-bit images keep their exact indices, so they draw on the character's MBAACC .pal like the rest of the bank.
// usePat references are rewritten from --pat-map (ActCadenza part set id -> id in <c>_3.pat) when given.
// Only id bytes of the HA6 change (offsets recorded by the loader); every other byte stays as converted.
#include "cgm_bank.h"
#include "cgm_ops.h"
#include "cgm_struct.h"
#include "../framedata.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

bool ReadFile(const char *p, std::vector<uint8_t> &b);
bool WriteFile(const std::string &p, const std::vector<uint8_t> &b);
bool ParseFile(const std::string &p, std::vector<uint8_t> &b, cgm::Bank &bk, std::string &err);
bool ArgVal(int argc, char **argv, const char *k, std::string &v);

namespace {

std::string Upper(const char *s) { std::string r(s); for (char &c : r) if (c >= 'a' && c <= 'z') c = (char)(c - 32); return r; }

// Same picture at the same place: every pixel in canvas space (indices for 8-bit images, RGBA otherwise). Bounds may
// differ by transparent padding and the canvas by right/bottom growth (AppendImageFrom): neither moves a pixel, the game
// and the editor place an image by its bounds origin.
bool Compose(const cgm::Bank &b, int i, bool indexed, int W, int H, std::vector<uint32_t> &out) {
	const cgm::Image &im = b.images[i];
	out.assign((size_t)W * H, 0);
	const int bw = im.boundsW(), bh = im.boundsH();
	if (indexed) {
		std::vector<uint8_t> p; uint32_t pal[256];
		if (!b.decodeIndexed(i, p, pal) || p.size() < (size_t)bw * bh) return false;
		for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
			const int cx = im.x1 + x, cy = im.y1 + y; const uint8_t v = p[(size_t)y * bw + x];
			if (!v) continue;
			if (cx >= W || cy >= H) return false;
			out[(size_t)cy * W + cx] = v;
		}
		return true;
	}
	cgm::Rgba p;
	if (!b.decode(i, p)) return false;
	for (int y = 0; y < p.h; y++) for (int x = 0; x < p.w; x++) {
		const uint8_t *q = &p.px[((size_t)y * p.w + x) * 4];
		if (!q[3]) continue;
		const int cx = im.x1 + x, cy = im.y1 + y;
		if (cx >= W || cy >= H) return false;
		uint32_t v; memcpy(&v, q, 4); out[(size_t)cy * W + cx] = v;
	}
	return true;
}

bool SamePicture(const cgm::Bank &a, int ia, const cgm::Bank &b, int ib) {
	const cgm::Image &x = a.images[ia], &y = b.images[ib];
	const bool ix = x.type == 0 || a.cutter2, iy = y.type == 0 || b.cutter2;
	if (ix != iy) return false;
	if (y.w < x.w || y.h < x.h) return false;   // the copy's canvas may only grow
	const int W = y.w, H = y.h;
	std::vector<uint32_t> p, q;
	return Compose(a, ia, ix, W, H, p) && Compose(b, ib, iy, W, H, q) && p == q;
}

} // namespace

namespace cgm {

// Append image `sid` of `src` to `dst` with the same canvas, bounds and pixels (8-bit: the same indices). -> new id.
bool AppendImageFrom(Bank &dst, const Bank &src, int sid, int *newId, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (sid < 0 || sid >= (int)src.images.size() || !src.images[sid].drawable()) return fail("source image not drawable");
	const Image &si = src.images[sid];
	const int cu = dst.cellUnit();
	if (si.x1 % cu || si.y1 % cu) return fail("source bounds are not on the destination cell grid");
	const int bw = si.boundsW(), bh = si.boundsH();
	// Pad the bounds to whole cells. When that would run past the canvas, start the padding further left / up (still on
	// the cell grid, the extra cells are transparent); when no cell-aligned start fits, grow the canvas right / down.
	// Pixels keep their canvas position either way.
	auto fit = [&](int b1, int size, int canvas, int &n1, int &padded, int &newCanvas) {
		n1 = b1; padded = (size + cu - 1) / cu * cu; newCanvas = canvas;
		while (n1 + padded > canvas && n1 - cu >= 0) { n1 -= cu; padded = (b1 + size - n1 + cu - 1) / cu * cu; }
		if (n1 + padded > canvas) { n1 = b1; padded = (size + cu - 1) / cu * cu; newCanvas = n1 + padded; }
	};
	int nx1, W, cw, ny1, H, ch;
	fit(si.x1, bw, si.w, nx1, W, cw);
	fit(si.y1, bh, si.h, ny1, H, ch);
	const int ox = si.x1 - nx1, oy = si.y1 - ny1;
	NewImageSpec sp; sp.name = si.name; sp.canvasW = cw; sp.canvasH = ch; sp.x1 = nx1; sp.y1 = ny1;
	std::vector<uint8_t> rgba((size_t)W * H * 4, 0), idx;
	const bool indexed = si.type == 0 || src.cutter2;
	if (indexed) {
		uint32_t pal[256]; std::vector<uint8_t> raw;
		if (!src.decodeIndexed(sid, raw, pal)) return fail("cannot decode source image");
		idx.assign((size_t)W * H, 0);
		for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
			const uint8_t v = raw[(size_t)y * bw + x]; const size_t at = (size_t)(y + oy) * W + x + ox; idx[at] = v;
			if (v) { uint8_t *o = &rgba[at * 4]; o[0] = (uint8_t)pal[v]; o[1] = (uint8_t)(pal[v] >> 8); o[2] = (uint8_t)(pal[v] >> 16); o[3] = 255; }
		}
		sp.type = 0;
	} else {
		Rgba px; if (!src.decode(sid, px)) return fail("cannot decode source image");
		for (int y = 0; y < bh; y++) memcpy(&rgba[((size_t)(y + oy) * W + ox) * 4], &px.px[(size_t)y * bw * 4], (size_t)bw * 4);
		sp.type = si.type == 3 ? -99 : si.type;
	}
	std::vector<int> remap; int id = -1;
	if (!AddImage(dst, -1, sp, rgba.data(), W, H, remap, &id, err)) return false;
	Image &ni = dst.images[id];
	if (ni.boundsW() != W || ni.boundsH() != H) return fail("appended bounds differ");
	if (indexed && !ReplaceImage(dst, id, rgba.data(), W, H, err, nullptr, &idx)) return false;
	// keep the source's exact (unpadded) bounds so the canvas geometry is identical
	ni.x2 = si.x2; ni.y2 = si.y2;
	if (newId) *newId = id;
	return true;
}

} // namespace cgm

int CmdAcMoon3(int argc, char **argv) {
	if (argc < 3) { puts("ac-moon3 <ac.ha6> <ac.cg> <mbaacc.cg> --ha6-out X --cg-out Y [--pat-map map.tsv] [--report r.txt]"); return 2; }
	std::string ha6Out, cgOut, patMapPath, repPath, err;
	ArgVal(argc, argv, "--ha6-out", ha6Out); ArgVal(argc, argv, "--cg-out", cgOut);
	ArgVal(argc, argv, "--pat-map", patMapPath); ArgVal(argc, argv, "--report", repPath);
	if (ha6Out.empty() || cgOut.empty()) { puts("--ha6-out and --cg-out are required"); return 2; }
	std::vector<uint8_t> ab, mb; cgm::Bank ac, mbk;
	if (!ParseFile(argv[1], ab, ac, err) || !ParseFile(argv[2], mb, mbk, err)) { printf("error: %s\n", err.c_str()); return 1; }
	std::vector<uint8_t> bytes; if (!ReadFile(argv[0], bytes)) { printf("error: cannot read %s\n", argv[0]); return 1; }
	Ha6SpriteRefSink sink; FrameData fd;
	g_ha6SpriteRefSink = &sink; const bool ok = fd.load(argv[0]); g_ha6SpriteRefSink = nullptr;
	if (!ok || !sink.base) { printf("error: cannot load %s as HA6\n", argv[0]); return 1; }
	const std::vector<uint8_t> orig = bytes;
	std::map<int, int> patMap;
	if (!patMapPath.empty()) {
		std::ifstream f(patMapPath); int a, b;
		if (!f) { printf("error: cannot read %s\n", patMapPath.c_str()); return 1; }
		std::string line; while (std::getline(f, line)) if (sscanf(line.c_str(), "%d %d", &a, &b) == 2) patMap[a] = b;
	}
	std::map<std::string, int> byName;
	for (int i = 0; i < (int)mbk.images.size(); i++) if (mbk.images[i].drawable()) byName.emplace(Upper(mbk.images[i].name), i);
	const int mbCount = (int)mbk.images.size();
	cgm::Bank out = mbk;
	std::map<int, int> map; std::map<std::string, int> stats; std::vector<std::string> notes;
	auto resolve = [&](int id) -> int {
		auto it = map.find(id); if (it != map.end()) return it->second;
		int r = -1;
		if (id >= (int)ac.images.size() || !ac.images[id].drawable()) { stats["ac_id_absent (draws nothing, -> -1)"]++; }
		else {
			const cgm::Image &ai = ac.images[id];
			auto n = byName.find(Upper(ai.name));
			if (n != byName.end() && mbk.images[n->second].w == ai.w && mbk.images[n->second].h == ai.h) { r = n->second; stats["images_matched_mbaacc"]++; }
			else {
				const bool recanvas = n != byName.end();
				if (!cgm::AppendImageFrom(out, ac, id, &r, &err)) { notes.push_back("append " + std::to_string(id) + " " + ai.name + ": " + err); r = -1; stats["append_failed"]++; }
				else if (!SamePicture(ac, id, out, r)) { notes.push_back("appended " + std::to_string(id) + " differs after append"); stats["append_differs"]++; }
				else stats[recanvas ? "images_appended_recanvassed (AFOF fix)" : "images_appended_missing"]++;
				if (recanvas) notes.push_back("recanvas " + std::string(ai.name) + " ac " + std::to_string(id) + " -> " + std::to_string(r));
				else notes.push_back("missing " + std::string(ai.name) + " ac " + std::to_string(id) + " -> " + std::to_string(r));
			}
		}
		map[id] = r; return r;
	};
	int refsCg = 0, refsPat = 0, patUnmapped = 0;
	for (const auto &rf : sink.refs) {
		if (rf.first + 4 > bytes.size()) continue;
		int32_t id; memcpy(&id, &bytes[rf.first], 4);
		if (id < 0) continue;
		if (rf.second) {
			refsPat++;
			auto it = patMap.find(id);
			if (it == patMap.end()) { patUnmapped++; continue; }
			const int32_t now = it->second; memcpy(&bytes[rf.first], &now, 4);
			continue;
		}
		refsCg++;
		const int32_t now = resolve(id); memcpy(&bytes[rf.first], &now, 4);
	}
	if ((int)out.images.size() >= cgm::kMaxImages) { printf("error: %zu images exceed the bank limit\n", out.images.size()); return 1; }
	{ std::vector<uint8_t> o; out.serialize(o); if (!WriteFile(cgOut, o)) { printf("error: cannot write %s\n", cgOut.c_str()); return 1; } }
	if (!WriteFile(ha6Out, bytes)) { printf("error: cannot write %s\n", ha6Out.c_str()); return 1; }
	// verify: the written files load, every CG reference resolves to a drawable image of the written bank that shows the
	// ActCadenza picture (appended) or the same-named MBAACC image; absent ActCadenza ids are -1.
	int bad = 0, checked = 0;
	{
		std::vector<uint8_t> ob; cgm::Bank ok2; if (!ParseFile(cgOut, ob, ok2, err)) { printf("FAIL re-read %s: %s\n", cgOut.c_str(), err.c_str()); return 1; }
		std::vector<uint8_t> nb; ReadFile(ha6Out.c_str(), nb);
		Ha6SpriteRefSink s2; FrameData f2;
		g_ha6SpriteRefSink = &s2; const bool ok3 = f2.load(ha6Out.c_str()); g_ha6SpriteRefSink = nullptr;
		if (!ok3 || s2.refs.size() != sink.refs.size()) { printf("FAIL re-load %s\n", ha6Out.c_str()); return 1; }
		for (size_t i = 0; i < s2.refs.size(); i++) {
			if (s2.refs[i].second) continue;
			int32_t was, now;
			memcpy(&now, &nb[s2.refs[i].first], 4); memcpy(&was, &orig[sink.refs[i].first], 4);
			if (was < 0) continue;
			checked++;
			if (now < 0) { if (map[was] != -1) bad++; continue; }
			if (now >= (int)ok2.images.size() || !ok2.images[now].drawable()) { bad++; continue; }
			if (now >= mbCount && !SamePicture(ac, was, ok2, now)) bad++;
			if (now < mbCount && Upper(ok2.images[now].name) != Upper(ac.images[was].name)) bad++;
		}
		if ((size_t)ok2.images.size() != out.images.size()) bad++;
	}
	char line[256];
	std::vector<std::string> rep;
	snprintf(line, sizeof line, "cg_refs %d pat_refs %d pat_refs_unmapped %d mbaacc_images %d out_images %zu", refsCg, refsPat, patUnmapped, mbCount, out.images.size()); rep.push_back(line);
	for (auto &kv : stats) { snprintf(line, sizeof line, "%s %d", kv.first.c_str(), kv.second); rep.push_back(line); }
	snprintf(line, sizeof line, "verify: %d CG refs checked, %d bad", checked, bad); rep.push_back(line);
	for (auto &n : notes) rep.push_back(n);
	for (size_t i = 0; i < rep.size() && i < 12; i++) printf("%s\n", rep[i].c_str());
	if (!repPath.empty()) { std::string s; for (auto &l : rep) s += l + "\n"; WriteFile(repPath, std::vector<uint8_t>(s.begin(), s.end())); }
	return bad || stats["append_failed"] || stats["append_differs"] ? 1 : 0;
}
