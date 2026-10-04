#include "cgm_ops.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace cgm {

namespace {
// median cut (same algorithm as CG::replace_image_rgba) on packed 0xRRGGBB
struct QBox { std::vector<unsigned> px; };
void MedianCut(std::vector<unsigned> colors, int maxColors, std::vector<unsigned> &outPal) {
	std::vector<QBox> boxes(1); boxes[0].px = std::move(colors);
	while ((int)boxes.size() < maxColors) {
		int pick = -1; size_t bestN = 1;
		for (size_t i = 0; i < boxes.size(); i++) if (boxes[i].px.size() > bestN) { bestN = boxes[i].px.size(); pick = (int)i; }
		if (pick < 0) break;
		QBox &b = boxes[pick];
		int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
		for (unsigned c : b.px) for (int k = 0; k < 3; k++) { int v = (c >> (16 - 8 * k)) & 255; lo[k] = std::min(lo[k], v); hi[k] = std::max(hi[k], v); }
		int axis = 0; for (int k = 1; k < 3; k++) if (hi[k] - lo[k] > hi[axis] - lo[axis]) axis = k;
		if (hi[axis] == lo[axis]) { b.px.resize(1); continue; }
		const int sh = 16 - 8 * axis;
		std::sort(b.px.begin(), b.px.end(), [&](unsigned x, unsigned y) { return ((x >> sh) & 255) < ((y >> sh) & 255); });
		QBox nb; size_t half = b.px.size() / 2;
		nb.px.assign(b.px.begin() + half, b.px.end()); b.px.resize(half);
		boxes.push_back(std::move(nb));
	}
	for (auto &b : boxes) {
		unsigned long long r = 0, g = 0, bl = 0; for (unsigned c : b.px) { r += (c >> 16) & 255; g += (c >> 8) & 255; bl += c & 255; }
		size_t n = std::max<size_t>(1, b.px.size());
		outPal.push_back(((unsigned)(r / n) << 16) | ((unsigned)(g / n) << 8) | (unsigned)(bl / n));
	}
}
inline unsigned Pack(const uint8_t *p) { return ((unsigned)p[0] << 16) | ((unsigned)p[1] << 8) | p[2]; }
inline unsigned Pack(uint32_t rgbaMem) { return ((rgbaMem & 0xFF) << 16) | (rgbaMem & 0xFF00) | ((rgbaMem >> 16) & 0xFF); }   // memory RGBA dword -> 0xRRGGBB
inline uint32_t Unpack(unsigned c) { return ((c >> 16) & 255) | (c & 0xFF00) | ((c & 255) << 16); }                              // 0xRRGGBB -> memory RGBA rgb part
}

bool SameRendering(const Bank &bank, int n, const uint8_t *rgba, int w, int h) {
	Rgba cur;
	if (!bank.decode(n, cur) || cur.w != w || cur.h != h) return false;
	const bool bin = bank.images[n].type == 2 || bank.images[n].type == 0;
	for (int i = 0; i < w * h; i++) {
		const uint8_t *a = &cur.px[i * 4], *b = &rgba[i * 4];
		uint8_t ba = b[3]; if (bin) ba = ba >= 128 ? 255 : 0;
		if (a[3] != ba || (a[3] && memcmp(a, b, 3) != 0)) return false;
	}
	return true;
}

bool ReplaceImage(Bank &bank, int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep, const std::vector<uint8_t> *indices) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (n < 0 || n >= (int)bank.images.size() || !bank.images[n].present) return fail("no such image");
	Image &im = bank.images[n];
	if (!im.drawable()) return fail("image " + std::to_string(n) + " has no pixels (type " + std::to_string(im.type) + ")");
	if (w != im.boundsW() || h != im.boundsH()) return fail("size mismatch: image " + std::to_string(n) + " is " + std::to_string(im.boundsW()) + " x " + std::to_string(im.boundsH()) + ", the new image is " + std::to_string(w) + " x " + std::to_string(h));
	const int ty = im.type;
	if (ty < 0 || ty > 4) return fail("storage type " + std::to_string(ty) + " cannot be imported");
	if ((ty == 0 && im.bpp != 8 && im.bpp != 32) || (ty != 0 && im.bpp != 32)) return fail("unsupported bit depth");
	if (ty == 0 && indices && (int)indices->size() != w * h) return fail("index plane has the wrong size");
	ReplaceReport local; ReplaceReport &R = rep ? *rep : local; R = ReplaceReport();
	auto atlas = bank.buildAtlas();
	R.alsoChanges = bank.dependants(n, atlas);

	// current state (indices, palette) of this image
	std::vector<uint8_t> curIdx, curAlpha; uint32_t curPal[256] = {};
	Rgba curRgba; bank.decode(n, curRgba);
	if (ty == 0 || ty == 2 || ty == 4) bank.decodeIndexed(n, curIdx, curPal, &curAlpha);

	std::vector<uint8_t> newBlob(*im.blob);
	std::vector<uint8_t> idx((size_t)w * h, 0);
	uint32_t newPal[256]; memcpy(newPal, curPal, sizeof(newPal));
	if (ty == 0) {
		bank.palette(0, newPal);
		for (int i = 0; i < w * h; i++) {
			const uint8_t *s = &rgba[i * 4];
			if (indices) { idx[i] = (*indices)[i]; continue; }
			if (s[3] < 128) { idx[i] = 0; continue; }
			if (!curIdx.empty() && curIdx[i] && curRgba.px[i * 4 + 3] && memcmp(&curRgba.px[i * 4], s, 3) == 0) { idx[i] = curIdx[i]; continue; }   // unchanged: keep its stored index
			int best = 1, bd = 1 << 30;
			for (int k = 1; k < 256; k++) {
				if (!(newPal[k] >> 24)) continue;
				const int dr = (int)(newPal[k] & 255) - s[0], dg = (int)((newPal[k] >> 8) & 255) - s[1], db = (int)((newPal[k] >> 16) & 255) - s[2], d = dr * dr + dg * dg + db * db;
				if (d < bd) { bd = d; best = k; if (!d) break; }
			}
			idx[i] = (uint8_t)best;
		}
	} else if (ty == 2 || ty == 4) {
		std::vector<unsigned> opaque;
		for (int i = 0; i < w * h; i++) if (rgba[i * 4 + 3] != 0 && (ty == 4 || rgba[i * 4 + 3] >= 128)) opaque.push_back(Pack(&rgba[i * 4]));
		std::vector<unsigned> uniq = opaque; std::sort(uniq.begin(), uniq.end()); uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
		uint32_t pal[256] = {}; bool used[256] = {}; std::map<unsigned, int> where;
		const uint32_t *stored = (const uint32_t *)im.blob->data();
		if ((int)uniq.size() <= 255) {
			// keep the stored position of every colour that survives, give new colours the free slots
			std::map<unsigned, int> old;
			for (int k = 1; k < 256; k++) old.emplace(Pack(stored[k]), k);
			std::vector<unsigned> fresh;
			for (unsigned c : uniq) { auto it = old.find(c); if (it != old.end()) { where[c] = it->second; used[it->second] = true; pal[it->second] = stored[it->second]; } else fresh.push_back(c); }
			int slot = 1;
			for (unsigned c : fresh) { while (slot < 256 && used[slot]) slot++; where[c] = slot; used[slot] = true; pal[slot] = Unpack(c); }
			// unused slots keep their old contents (harmless, and fewer changed bytes)
			for (int k = 1; k < 256; k++) if (!used[k]) pal[k] = stored[k];
		} else {
			std::vector<unsigned> q; MedianCut(opaque, 255, q);
			for (size_t k = 0; k < q.size(); k++) { pal[k + 1] = Unpack(q[k]); used[k + 1] = true; }
		}
		pal[0] = stored[0];
		for (int k = 0; k < 256; k++) newPal[k] = pal[k];
		R.paletteChanged = memcmp(pal, stored, 1024) != 0;
		for (int i = 0; i < w * h; i++) {
			const uint8_t *s = &rgba[i * 4];
			const bool transparent = s[3] == 0 || (ty == 2 && s[3] < 128);
			if (transparent) { idx[i] = (ty == 4 && !curIdx.empty()) ? curIdx[i] : 0; continue; }
			const unsigned c = Pack(s);
			if (!curIdx.empty() && curIdx[i] && !R.paletteChanged && Pack(stored[curIdx[i]]) == c) { idx[i] = curIdx[i]; continue; }
			auto it = where.find(c);
			if (it != where.end()) { idx[i] = (uint8_t)it->second; continue; }
			int best = 1, bd = 1 << 30;   // quantised: nearest palette entry
			for (int k = 1; k < 256; k++) if (used[k]) {
				const int dr = (int)(pal[k] & 255) - s[0], dg = (int)((pal[k] >> 8) & 255) - s[1], db = (int)((pal[k] >> 16) & 255) - s[2], d = dr * dr + dg * dg + db * db;
				if (d < bd) { bd = d; best = k; }
			}
			idx[i] = (uint8_t)best;
		}
		memcpy(newBlob.data(), pal, 1024);
	}
	// write the owned blocks
	std::map<int, bool> lostFrom;
	for (const Block &b : im.blocks) {
		for (int ly = 0; ly < b.h; ly++) for (int lx = 0; lx < b.w; lx++) {
			const int cx = b.x + lx - im.x1, cy = b.y + ly - im.y1;
			const bool inside = cx >= 0 && cy >= 0 && cx < w && cy < h;
			if (b.copy) {
				if (inside && curRgba.px.size()) {
					const size_t pix = (size_t)cy * w + cx; const uint8_t *s = &rgba[pix * 4], *c = &curRgba.px[pix * 4];
					uint8_t sa = s[3]; if (ty == 0 || ty == 2) sa = sa >= 128 ? 255 : 0;
					if (sa != c[3] || (sa && memcmp(s, c, 3))) R.lostPixels++;
				}
				continue;
			}
			const size_t o = b.dataOff + ((size_t)ly * b.w + lx) * (ty == 1 ? 4 : 1);
			if (o >= newBlob.size()) continue;
			const size_t pix = (size_t)cy * w + cx;
			if (ty == 1) {
				if (o + 4 > newBlob.size()) continue;
				uint8_t v[4] = {0, 0, 0, 0};
				if (inside) { const uint8_t *s = &rgba[pix * 4]; v[0] = s[2]; v[1] = s[1]; v[2] = s[0]; v[3] = s[3]; }
				else memcpy(v, &(*im.blob)[o], 4);   // outside the bounds: keep
				if (memcmp(&newBlob[o], v, 4)) { memcpy(&newBlob[o], v, 4); R.changedPixels++; }
			} else if (ty == 3) {
				const uint8_t a = inside ? rgba[pix * 4 + 3] : (*im.blob)[o];
				if (newBlob[o] != a) { newBlob[o] = a; R.changedPixels++; }
			} else {
				const uint8_t v = inside ? idx[pix] : (*im.blob)[o];
				if (newBlob[o] != v) { newBlob[o] = v; R.changedPixels++; }
				if (ty == 4) {
					const size_t ao = o + (size_t)b.w * b.h; if (ao < newBlob.size()) {
						const uint8_t a = inside ? rgba[pix * 4 + 3] : (*im.blob)[ao];
						if (newBlob[ao] != a) { newBlob[ao] = a; R.changedPixels++; }
					}
				}
			}
		}
	}
	if (R.lostPixels) { auto o = bank.owners(n, atlas); R.borrowedFrom = o; }
	if (newBlob != *im.blob) im.blob = std::make_shared<const std::vector<uint8_t>>(std::move(newBlob));
	else R.alsoChanges.clear();
	return true;
}

} // namespace cgm
