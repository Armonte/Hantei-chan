#include "cgm_bank.h"
#include <algorithm>
#include <cstring>

namespace cgm {

static uint32_t Rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int32_t Rd32s(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t Rd16s(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static void Wr32(std::vector<uint8_t> &o, uint32_t v) { const uint8_t *p = (const uint8_t *)&v; o.insert(o.end(), p, p + 4); }
static void Wr16(std::vector<uint8_t> &o, int16_t v) { const uint8_t *p = (const uint8_t *)&v; o.insert(o.end(), p, p + 2); }

static const size_t kHdrDw = 0x2014, kIdxOff = 0x2044, kTail = kIdxOff + 4 * kMaxImages, kFirstImage = 0x4f30;

static size_t BytesPerPixel(int type) { return type == 1 ? 4 : type == 4 ? 2 : 1; }   // per owned block pixel (type 4: index + alpha plane)
static size_t PrefixBytes(int type, int bpp) { return bpp == 32 ? (type == 2 || type == 4 ? 1024 : type == 3 ? 4 : 0) : 0; }

void Bank::recomputeLayout() {
	for (Image &im : images) {
		uint32_t off = (uint32_t)PrefixBytes(im.type, im.bpp);
		for (Block &b : im.blocks) {
			b.dataOff = off;
			if (!b.copy) off += (uint32_t)((size_t)b.w * b.h * BytesPerPixel(im.type));
		}
	}
	uint32_t na = 0;
	for (const Image &im : images) na += (uint32_t)im.blocks.size();
	H[2] = na; H[3] = (uint32_t)images.size();
}

bool Bank::parse(const uint8_t *d, size_t size, Bank &out, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	out = Bank();
	if (size < kFirstImage || (memcmp(d, "BMP Cutter3", 11) && memcmp(d, "BMP Cutter2", 11))) return fail("not a BMP Cutter bank");
	memcpy(out.head.data(), d, 0x14);
	memcpy(out.palettes.data(), d + 0x14, 0x2000);
	for (int i = 0; i < 12; i++) out.H[i] = Rd32(d + kHdrDw + 4 * i);
	const uint32_t nimg = out.H[3], nalign = out.H[2];
	if (nimg >= (uint32_t)kMaxImages) return fail("too many images");
	const uint32_t ao = Rd32(d + kTail);
	out.tailMid = Rd32(d + kTail + 4);
	if (Rd32(d + kTail + 8) != size) return fail("size field in the tail does not match the file size");
	if (ao > size || (size - ao) != (size_t)nalign * 24) return fail("alignment table does not end the file");
	for (uint32_t i = nimg; i < (uint32_t)kMaxImages; i++) if (Rd32(d + kIdxOff + 4 * i) != kAbsent) return fail("index entry beyond the image count");
	out.images.resize(nimg);
	std::vector<std::pair<uint32_t, uint32_t>> order;   // (offset, index)
	for (uint32_t i = 0; i < nimg; i++) { uint32_t o = Rd32(d + kIdxOff + 4 * i); if (o != kAbsent) order.push_back({o, i}); }
	for (size_t k = 0; k < order.size(); k++) {
		if (k && order[k].first <= order[k - 1].first) return fail("image offsets are not strictly increasing");
	}
	uint32_t pos = (uint32_t)kFirstImage, runAlign = 0;
	for (size_t k = 0; k < order.size(); k++) {
		const uint32_t o = order[k].first, idx = order[k].second;
		const uint32_t end = k + 1 < order.size() ? order[k + 1].first : ao;
		if (o != pos && k == 0) return fail("first image is not at 0x4f30");
		if (o + 72 > end || end > ao) return fail("image " + std::to_string(idx) + " has no room for its header");
		Image &im = out.images[idx];
		im.present = true;
		memcpy(im.name, d + o, 32);
		im.type = Rd32s(d + o + 32); im.w = Rd32s(d + o + 36); im.h = Rd32s(d + o + 40); im.bpp = Rd32s(d + o + 44);
		im.x1 = Rd32s(d + o + 48); im.y1 = Rd32s(d + o + 52); im.x2 = Rd32s(d + o + 56); im.y2 = Rd32s(d + o + 60);
		const uint32_t as = Rd32(d + o + 64), al = Rd32(d + o + 68);
		if ((uint64_t)as + al > nalign) return fail("image " + std::to_string(idx) + ": alignment range outside the table");
		if (as != runAlign) out.alignSequential = false;
		runAlign = as + al;
		im.blocks.resize(al);
		for (uint32_t j = 0; j < al; j++) {
			const uint8_t *a = d + ao + 24 * (size_t)(as + j);
			Block &b = im.blocks[j];
			b.x = Rd32s(a); b.y = Rd32s(a + 4); b.w = Rd32s(a + 8); b.h = Rd32s(a + 12);
			b.sx = Rd16s(a + 16); b.sy = Rd16s(a + 18); b.page = Rd16s(a + 20); b.copy = Rd16s(a + 22);
		}
		im.blob = std::make_shared<const std::vector<uint8_t>>(d + o + 72, d + end);
		pos = end;
	}
	if (!out.alignSequential) return fail("alignment ranges are not sequential in image order");
	if (runAlign != nalign) return fail("alignment ranges do not cover the table");
	out.recomputeLayout();
	return true;
}

void Bank::serialize(std::vector<uint8_t> &o) const {
	o.clear();
	o.insert(o.end(), head.begin(), head.end());
	o.insert(o.end(), palettes.begin(), palettes.end());
	uint32_t na = 0; for (const Image &im : images) na += (uint32_t)im.blocks.size();
	for (int i = 0; i < 12; i++) Wr32(o, i == 2 ? na : i == 3 ? (uint32_t)images.size() : H[i]);
	const size_t idxPos = o.size();
	for (int i = 0; i < kMaxImages; i++) Wr32(o, kAbsent);
	const size_t tailPos = o.size();
	Wr32(o, 0); Wr32(o, tailMid); Wr32(o, 0);
	for (size_t i = 0; i < images.size(); i++) {
		const Image &im = images[i];
		if (!im.present) continue;
		uint32_t off = (uint32_t)o.size(); memcpy(&o[idxPos + 4 * i], &off, 4);
		o.insert(o.end(), (const uint8_t *)im.name, (const uint8_t *)im.name + 32);
		Wr32(o, (uint32_t)im.type); Wr32(o, (uint32_t)im.w); Wr32(o, (uint32_t)im.h); Wr32(o, (uint32_t)im.bpp);
		Wr32(o, (uint32_t)im.x1); Wr32(o, (uint32_t)im.y1); Wr32(o, (uint32_t)im.x2); Wr32(o, (uint32_t)im.y2);
		uint32_t as = 0; for (size_t k = 0; k < i; k++) as += (uint32_t)images[k].blocks.size();
		Wr32(o, as); Wr32(o, (uint32_t)im.blocks.size());
		if (im.blob) o.insert(o.end(), im.blob->begin(), im.blob->end());
	}
	const uint32_t ao = (uint32_t)o.size();
	memcpy(&o[idxPos + 4 * kMaxImages], &ao, 4);
	for (const Image &im : images) for (const Block &b : im.blocks) {
		Wr32(o, (uint32_t)b.x); Wr32(o, (uint32_t)b.y); Wr32(o, (uint32_t)b.w); Wr32(o, (uint32_t)b.h);
		Wr16(o, b.sx); Wr16(o, b.sy); Wr16(o, b.page); Wr16(o, b.copy);
	}
	const uint32_t total = (uint32_t)o.size();
	memcpy(&o[tailPos + 8], &total, 4);
}

void Bank::palette(int slot, uint32_t out[256]) const {
	memcpy(out, palettes.data() + 0x400 * (slot & 7), 1024);
	for (int i = 0; i < 256; i++) out[i] = (out[i] & 0xFFFFFF) | ((out[i] >> 24) ? 0xFF000000u : 0);
	out[0] = 0;
}
void Bank::setPaletteSlot(int slot, const uint32_t rgba[256]) { memcpy(palettes.data() + 0x400 * (slot & 7), rgba, 1024); }

std::unordered_map<uint32_t, int> Bank::buildAtlas() const {
	std::unordered_map<uint32_t, int> m;
	const int cu = cellUnit();
	for (size_t n = 0; n < images.size(); n++) for (const Block &b : images[n].blocks) {
		if (b.copy) continue;
		for (int cy = b.sy / cu; cy < (b.sy + b.h) / cu; cy++) for (int cx = b.sx / cu; cx < (b.sx + b.w) / cu; cx++) m[cellKey(b.page, cx, cy)] = (int)n;
	}
	return m;
}

std::vector<int> Bank::dependants(int n, const std::unordered_map<uint32_t, int> &atlas) const {
	std::vector<int> r; const int cu = cellUnit();
	for (size_t i = 0; i < images.size(); i++) {
		if ((int)i == n) continue;
		bool uses = false;
		for (const Block &b : images[i].blocks) if (b.copy) {
			for (int cy = b.sy / cu; !uses && cy < (b.sy + b.h) / cu; cy++) for (int cx = b.sx / cu; !uses && cx < (b.sx + b.w) / cu; cx++) {
				auto it = atlas.find(cellKey(b.page, cx, cy)); if (it != atlas.end() && it->second == n) uses = true;
			}
		}
		if (uses) r.push_back((int)i);
	}
	return r;
}
std::vector<int> Bank::owners(int n, const std::unordered_map<uint32_t, int> &atlas) const {
	std::vector<int> r; const int cu = cellUnit();
	for (const Block &b : images[n].blocks) if (b.copy)
		for (int cy = b.sy / cu; cy < (b.sy + b.h) / cu; cy++) for (int cx = b.sx / cu; cx < (b.sx + b.w) / cu; cx++) {
			auto it = atlas.find(cellKey(b.page, cx, cy)); if (it != atlas.end() && std::find(r.begin(), r.end(), it->second) == r.end()) r.push_back(it->second);
		}
	return r;
}

namespace {
// Locates the owned block that holds atlas cell (page, cx, cy) and the byte offset of its first pixel in that block.
struct CellRef { const Image *im = nullptr; const Block *blk = nullptr; };
}

// Core decoder: fills for every pixel of the bounds rect either an RGBA value (rgba != null) or an index (+alpha).
static bool DecodeImpl(const Bank &bk, int n, std::vector<uint32_t> *rgbaOut, std::vector<uint8_t> *idxOut, std::vector<uint8_t> *alphaOut, const uint32_t *pal0, uint32_t *palOut) {
	if (n < 0 || n >= (int)bk.images.size()) return false;
	const Image &im = bk.images[n];
	if (!im.drawable() || !im.blob) return false;
	const int W = im.boundsW(), H = im.boundsH(), cu = bk.cellUnit();
	if ((int64_t)W * H > (1 << 24)) return false;
	uint32_t pal[256];
	const uint8_t *blob = im.blob->data(); const size_t bsz = im.blob->size();
	if (im.bpp == 32 && im.type == 3) {
		if (bsz < 4) return false;
		uint32_t col; memcpy(&col, blob, 4); col &= 0xFFFFFF;
		pal[0] = 0; for (int i = 1; i < 256; i++) pal[i] = ((uint32_t)i << 24) | col;
	} else if (im.bpp == 32 && (im.type == 2 || im.type == 4)) {
		if (bsz < 1024) return false;
		memcpy(pal, blob, 1024);
		for (int i = 0; i < 256; i++) pal[i] |= 0xFF000000u;
		if (im.type == 2) pal[0] = 0;
	} else if (pal0) memcpy(pal, pal0, 1024);
	else bk.palette(0, pal);
	if (palOut) memcpy(palOut, pal, 1024);
	if (rgbaOut) rgbaOut->assign((size_t)W * H, 0);
	if (idxOut) idxOut->assign((size_t)W * H, 0);
	if (alphaOut) alphaOut->assign((size_t)W * H, 0);
	std::unordered_map<uint32_t, int> atlas;
	bool haveAtlas = false;
	const bool alphaPlane = im.type == 4, bgra = im.type == 1;
	for (const Block &b : im.blocks) {
		for (int ly = 0; ly < b.h; ly++) for (int lx = 0; lx < b.w; lx++) {
			const int cx_ = b.x + lx - im.x1, cy_ = b.y + ly - im.y1;
			if (cx_ < 0 || cy_ < 0 || cx_ >= W || cy_ >= H) continue;
			// find the data of this pixel
			const Image *src = &im; const Block *sb = &b; int sx = lx, sy = ly;
			if (b.copy) {
				const int px = b.sx + lx, py = b.sy + ly;
				if (!haveAtlas) { atlas = bk.buildAtlas(); haveAtlas = true; }
				auto it = atlas.find(Bank::cellKey(b.page, px / cu, py / cu));
				if (it == atlas.end()) continue;
				src = &bk.images[it->second]; sb = nullptr;
				for (const Block &c : src->blocks) if (!c.copy && c.page == b.page && px >= c.sx && px < c.sx + c.w && py >= c.sy && py < c.sy + c.h) { sb = &c; break; }
				if (!sb || !src->blob) continue;
				sx = px - sb->sx; sy = py - sb->sy;
			}
			const std::vector<uint8_t> &sbl = *src->blob;
			const size_t o = sb->dataOff + ((size_t)sy * sb->w + sx) * (src->type == 1 ? 4 : 1);
			const size_t pix = (size_t)cy_ * W + cx_;
			if (src->type == 1) {
				if (o + 4 > sbl.size()) continue;
				uint32_t v; memcpy(&v, &sbl[o], 4);
				v = (v & 0xff00ff00) | ((v & 0xff) << 16) | ((v & 0xff0000) >> 16);
				if (rgbaOut) (*rgbaOut)[pix] = v;
			} else {
				if (o >= sbl.size()) continue;
				const uint8_t i0 = sbl[o];
				uint32_t v;
				if (src->type == 3) { v = pal[i0]; }   // plane value is alpha: handled below
				else v = pal[i0];
				if (src->type == 4) {
					const size_t ao = o + (size_t)sb->w * sb->h;
					const uint8_t a = ao < sbl.size() ? sbl[ao] : 0;
					v = (pal[i0] & 0xFFFFFF) | ((uint32_t)a << 24);
					if (alphaOut) (*alphaOut)[pix] = a;
				}
				if (rgbaOut) (*rgbaOut)[pix] = v;
				if (idxOut) (*idxOut)[pix] = i0;
			}
		}
	}
	(void)bgra; (void)alphaPlane;
	return true;
}

bool Bank::decode(int n, Rgba &out, const uint32_t *pal) const {
	std::vector<uint32_t> v;
	if (!DecodeImpl(*this, n, &v, nullptr, nullptr, pal, nullptr)) return false;
	const Image &im = images[n];
	out.w = im.boundsW(); out.h = im.boundsH(); out.px.resize(v.size() * 4);
	memcpy(out.px.data(), v.data(), out.px.size());
	return true;
}

bool Bank::decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], std::vector<uint8_t> *alpha8, const uint32_t *bankPal) const {
	if (n < 0 || n >= (int)images.size()) return false;
	const Image &im = images[n];
	if (!im.drawable() || im.type == 1 || im.type == 3) return false;
	std::vector<uint8_t> a;
	if (!DecodeImpl(*this, n, nullptr, &idx, &a, bankPal, pal)) return false;
	if (alpha8) *alpha8 = std::move(a);
	return true;
}

uint64_t HashRgba(const uint8_t *px, size_t n) {
	uint64_t h = 1469598103934665603ull;
	for (size_t i = 0; i < n; i += 4) {
		const uint8_t a = px[i + 3];
		const uint8_t v[4] = {a ? px[i] : (uint8_t)0, a ? px[i + 1] : (uint8_t)0, a ? px[i + 2] : (uint8_t)0, a};
		for (int k = 0; k < 4; k++) { h ^= v[k]; h *= 1099511628211ull; }
	}
	return h;
}

} // namespace cgm
