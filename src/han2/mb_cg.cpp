#include "mb_cg.h"
#include <algorithm>
#include "../misc.h"
#include <cstring>

namespace han2 {

static int16_t S16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint32_t U32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static const size_t kImgOffTable = 0x14, kPalOff = 0x2EF4, kPieceCountOff = 0x4EF4, kPieceOff = 0x4EFC, kPieceSize = 14, kImgHeader = 52;

std::shared_ptr<MbCgBank> MbCgBank::Parse(const uint8_t *p, size_t n, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return std::shared_ptr<MbCgBank>(); };
	if (n < kPieceOff) return fail("CG blob too short");
	auto b = std::make_shared<MbCgBank>();
	b->m_blob.assign(p, p + n);
	b->m_trueColor = p[0x10] == 0xFF;
	const uint32_t pieces = U32(p + kPieceCountOff);
	if (kPieceOff + (size_t)pieces * kPieceSize > n) return fail("strip table runs past the blob");
	b->m_pieceOff = kPieceOff;
	b->m_img.assign(3000, Img());
	unsigned maxId = 0; size_t expectedNext = kPieceOff + (size_t)pieces * kPieceSize;
	std::vector<std::pair<uint32_t, unsigned>> order;
	for (unsigned i = 0; i < 3000; i++) {
		int32_t off; memcpy(&off, p + kImgOffTable + 4 * i, 4);
		if (off == -1) continue;
		if (off < 0 || (size_t)off + kImgHeader > n) return fail("image offset out of range");
		b->m_img[i].off = (uint32_t)off; b->m_img[i].present = true; maxId = i + 1; order.push_back({ (uint32_t)off, i });
	}
	b->m_count = maxId;
	std::sort(order.begin(), order.end());
	for (size_t k = 0; k < order.size(); k++) {
		Img &im = b->m_img[order[k].second];
		if (im.off != expectedNext) return fail("images are not contiguous");
		im.firstPiece = U32(p + im.off + 0x2C); im.pieceCount = (uint32_t)(p[im.off + 0x30] | (p[im.off + 0x31] << 8));
		if ((uint64_t)im.firstPiece + im.pieceCount > pieces) return fail("image strips outside the strip table");
		im.dataOff = im.off + kImgHeader;
		size_t px = 0;
		for (uint32_t j = 0; j < im.pieceCount; j++) { MbCgStrip s = b->strip(im.firstPiece + j); px += (size_t)s.width * s.height * (b->m_trueColor ? 3 : 1); }
		expectedNext = im.dataOff + px;
		if (expectedNext > n) return fail("image pixels run past the blob");
	}
	if (expectedNext != n) return fail("images do not end exactly at the end of the blob");
	for (int s = 0; s < 8; s++) for (int i = 0; i < 256; i++) {
		const uint8_t *e = p + kPalOff + (size_t)s * 1024 + 4 * i;
		b->m_pal[s][i] = i == 0 ? 0u : (0xFF000000u | e[0] | (e[1] << 8) | (e[2] << 16));
	}
	return b;
}

MbCgStrip MbCgBank::strip(uint32_t i) const
{
	const uint8_t *q = m_blob.data() + m_pieceOff + (size_t)i * kPieceSize;
	return { S16(q), S16(q + 2), S16(q + 4), S16(q + 6), S16(q + 8), S16(q + 10), S16(q + 12) };
}

const char *MbCgBank::imageName(unsigned n) const { return n < m_img.size() && m_img[n].present ? (const char *)m_blob.data() + m_img[n].off : nullptr; }

bool MbCgBank::imageInfo(unsigned n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) const
{
	if (n >= m_img.size() || !m_img[n].present) return false;
	const uint8_t *h = m_blob.data() + m_img[n].off;
	x1 = S16(h + 0x24); y1 = S16(h + 0x26); x2 = S16(h + 0x28); y2 = S16(h + 0x2A);
	bpp = m_trueColor ? 24 : 8; typeId = m_trueColor ? 1 : 0;
	return x2 >= x1 && y2 >= y1;
}

bool MbCgBank::imageCells(unsigned n, std::vector<CgCellRect> &out) const
{
	if (n >= m_img.size() || !m_img[n].present) return false;
	for (uint32_t j = 0; j < m_img[n].pieceCount; j++) { MbCgStrip s = strip(m_img[n].firstPiece + j); out.push_back({ s.x, s.y, s.width, s.height }); }
	return true;
}

ImageData *MbCgBank::draw(unsigned n, bool toPow2, bool indexed, const unsigned *palette) const
{
	int bpp, ty, x1, y1, x2, y2;
	if (!imageInfo(n, bpp, ty, x1, y1, x2, y2)) return nullptr;
	int w = x2 - x1 + 1, h = y2 - y1 + 1;
	if (toPow2) { w = to_pow2(w); h = to_pow2(h); }
	const bool idx = indexed && !m_trueColor;
	unsigned char *px = new unsigned char[(size_t)w * h * 4]; memset(px, 0, (size_t)w * h * 4);
	const Img &im = m_img[n]; size_t at = im.dataOff;
	for (uint32_t j = 0; j < im.pieceCount; j++) {
		const MbCgStrip s = strip(im.firstPiece + j);
		for (int r = 0; r < s.height; r++) for (int c = 0; c < s.width; c++) {
			const int dx = s.x + c - x1, dy = s.y + r - y1;
			const bool inside = dx >= 0 && dy >= 0 && dx < w && dy < h;
			if (m_trueColor) {
				const uint8_t *q = m_blob.data() + at + 3 * ((size_t)r * s.width + c);
				if (inside && (q[0] | q[1] | q[2])) { uint8_t *d = px + 4 * ((size_t)dy * w + dx); d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; d[3] = 255; }
			} else {
				const uint8_t v = m_blob[at + (size_t)r * s.width + c];
				if (!inside) continue;
				if (idx) px[(size_t)dy * w + dx] = v;
				else if (v) { unsigned col = palette ? palette[v] : 0xFFFFFFFFu; memcpy(px + 4 * ((size_t)dy * w + dx), &col, 4); }
			}
		}
		at += (size_t)s.width * s.height * (m_trueColor ? 3 : 1);
	}
	return new ImageData{ px, w, h, idx, false, x1, y1 };
}

bool MbCgBank::replaceImage(unsigned n, const unsigned char *rgba, int w, int h, int paletteIndex, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	int bpp, ty, x1, y1, x2, y2;
	if (!imageInfo(n, bpp, ty, x1, y1, x2, y2)) return fail("no such image");
	if (w != x2 - x1 + 1 || h != y2 - y1 + 1) return fail("size mismatch: image " + std::to_string(n) + " is " + std::to_string(x2 - x1 + 1) + " x " + std::to_string(y2 - y1 + 1) + ", the PNG is " + std::to_string(w) + " x " + std::to_string(h));
	const unsigned *pal = m_pal[paletteIndex & 7];
	{   // unchanged pixels: keep the stored bytes (the nearest-colour search could pick another index of the same colour)
		std::unique_ptr<ImageData> cur(draw(n, false, false, pal));
		if (cur && cur->width == w && cur->height == h && memcmp(cur->pixels, rgba, (size_t)w * h * 4) == 0) return true;
	}
	// nearest palette entry (1..255) by squared RGB distance, cached per colour
	std::vector<std::pair<uint32_t, uint8_t>> cache;
	auto nearest = [&](const uint8_t *c) -> uint8_t {
		const uint32_t key = c[0] | (c[1] << 8) | (c[2] << 16);
		for (auto &kv : cache) if (kv.first == key) return kv.second;
		int best = 1, bd = 1 << 30;
		for (int i = 1; i < 256; i++) { int dr = (int)(pal[i] & 255) - c[0], dg = (int)((pal[i] >> 8) & 255) - c[1], db = (int)((pal[i] >> 16) & 255) - c[2]; int d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = i; } }
		cache.push_back({ key, (uint8_t)best }); return (uint8_t)best;
	};
	const Img &im = m_img[n]; size_t at = im.dataOff;
	for (uint32_t j = 0; j < im.pieceCount; j++) {
		const MbCgStrip s = strip(im.firstPiece + j);
		for (int r = 0; r < s.height; r++) for (int c = 0; c < s.width; c++) {
			const int sx = s.x + c - x1, sy = s.y + r - y1;
			const uint8_t *q = (sx >= 0 && sy >= 0 && sx < w && sy < h) ? rgba + 4 * ((size_t)sy * w + sx) : nullptr;
			const bool clear = !q || q[3] < 128;
			if (m_trueColor) {
				uint8_t *d = m_blob.data() + at + 3 * ((size_t)r * s.width + c);
				if (clear) d[0] = d[1] = d[2] = 0; else { d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; if (!(d[0] | d[1] | d[2])) d[2] = 1; }
			} else {
				m_blob[at + (size_t)r * s.width + c] = clear ? 0 : nearest(q);
			}
		}
		at += (size_t)s.width * s.height * (m_trueColor ? 3 : 1);
	}
	m_dirty = true;
	return true;
}

} // namespace han2
