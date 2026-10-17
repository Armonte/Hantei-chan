#include "qoh_cg.h"
#include "../misc.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace han2 {

static int32_t S32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t S16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }

std::shared_ptr<QohCgBank> QohCgBank::Parse(const uint8_t *p, size_t n, int version, std::string *err)
{
	auto b = std::make_shared<QohCgBank>();
	if (!(version == qoh::V98 ? qoh::ComputeLayout98(p, n, b->m_layout, err) : qoh::ComputeLayout99(p, n, b->m_layout, err))) return nullptr;
	b->m_file.assign(p, p + n);
	const qoh::Layout &L = b->m_layout; int64_t next = 0;
	for (uint32_t i = 0; i < L.images; i++) {
		const uint8_t *r = p + L.imageRecs + L.imageRecSize * i; const int32_t first = S32(r), cnt = S32(r + 4);
		if (first != next || cnt < 0 || (uint64_t)first + cnt > L.tiles) { if (err) *err = "image tile ranges are not contiguous"; return nullptr; }
		next += cnt;
	}
	if (next != (int64_t)L.tiles) { if (err) *err = "image tile ranges do not cover every tile"; return nullptr; }
	for (int k = 0; k < 16; k++) for (int i = 0; i < 256; i++) {
		if (i >= 16) { b->m_pal[k][i] = 0; continue; }
		const uint8_t *e = p + L.paletteOff + 4 * (16 * k + i);
		b->m_pal[k][i] = i == 0 ? 0u : (0xFF000000u | e[2] | (e[1] << 8) | (e[0] << 16));
	}
	return b;
}

const char *QohCgBank::imageName(unsigned n) const
{
	if (n >= m_layout.images) return nullptr;
	if (m_layout.version == qoh::V98) { memcpy(m_name, m_file.data() + m_layout.timPaths + 260 * n, 8); m_name[8] = 0; }
	else snprintf(m_name, sizeof m_name, "img%u", n);
	return m_name;
}

bool QohCgBank::bounds(unsigned n, int &x1, int &y1, int &x2, int &y2) const
{
	if (n >= m_layout.images) return false;
	const uint8_t *r = m_file.data() + m_layout.imageRecs + m_layout.imageRecSize * n; const int32_t first = S32(r), cnt = S32(r + 4);
	if (cnt <= 0) return false;
	x1 = y1 = 1 << 20; x2 = y2 = -(1 << 20);
	for (int32_t t = 0; t < cnt; t++) { const uint8_t *tl = m_file.data() + m_layout.tileOff + 260ull * (first + t); const int x = S16(tl), y = S16(tl + 2); x1 = std::min(x1, x); y1 = std::min(y1, y); x2 = std::max(x2, x + 15); y2 = std::max(y2, y + 15); }
	return true;
}

bool QohCgBank::imageInfo(unsigned n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) const { bpp = 8; typeId = 0; return bounds(n, x1, y1, x2, y2); }

bool QohCgBank::imageCells(unsigned n, std::vector<CgCellRect> &out) const
{
	if (n >= m_layout.images) return false;
	const uint8_t *r = m_file.data() + m_layout.imageRecs + m_layout.imageRecSize * n; const int32_t first = S32(r), cnt = S32(r + 4);
	for (int32_t t = 0; t < cnt; t++) { const uint8_t *tl = m_file.data() + m_layout.tileOff + 260ull * (first + t); out.push_back({ S16(tl), S16(tl + 2), 16, 16 }); }
	return true;
}

ImageData *QohCgBank::draw(unsigned n, bool toPow2, bool indexed, const unsigned *palette) const
{
	int x1, y1, x2, y2; if (!bounds(n, x1, y1, x2, y2)) return nullptr;
	int w = x2 - x1 + 1, h = y2 - y1 + 1; if (toPow2) { w = to_pow2(w); h = to_pow2(h); }
	unsigned char *px = new unsigned char[(size_t)w * h * 4]; memset(px, 0, (size_t)w * h * 4);
	const uint8_t *r = m_file.data() + m_layout.imageRecs + m_layout.imageRecSize * n; const int32_t first = S32(r), cnt = S32(r + 4);
	for (int32_t t = 0; t < cnt; t++) {
		const uint8_t *tl = m_file.data() + m_layout.tileOff + 260ull * (first + t); const int tx = S16(tl) - x1, ty = S16(tl + 2) - y1;
		for (int yy = 0; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) {
			const uint8_t v = tl[4 + yy * 16 + xx] & 15; const int dx = tx + xx, dy = ty + yy;
			if (dx < 0 || dy < 0 || dx >= w || dy >= h) continue;
			if (indexed) px[(size_t)dy * w + dx] = v;
			else if (v) { unsigned c = palette ? palette[v] : 0xFFFFFFFFu; memcpy(px + 4 * ((size_t)dy * w + dx), &c, 4); }
		}
	}
	return new ImageData{ px, w, h, indexed, false, x1, y1 };
}

bool QohCgBank::replaceImage(unsigned n, const unsigned char *rgba, int w, int h, int paletteIndex, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	int x1, y1, x2, y2; if (!bounds(n, x1, y1, x2, y2)) return fail("no such image");
	if (w != x2 - x1 + 1 || h != y2 - y1 + 1) return fail("size mismatch: image " + std::to_string(n) + " is " + std::to_string(x2 - x1 + 1) + " x " + std::to_string(y2 - y1 + 1) + ", the PNG is " + std::to_string(w) + " x " + std::to_string(h));
	const unsigned *pal = m_pal[paletteIndex & 15];
	{
		std::unique_ptr<ImageData> cur(draw(n, false, false, pal));
		if (cur && cur->width == w && cur->height == h && memcmp(cur->pixels, rgba, (size_t)w * h * 4) == 0) return true;
	}
	auto nearest = [&](const uint8_t *c) { int best = 1, bd = 1 << 30; for (int i = 1; i < 16; i++) { int dr = (int)(pal[i] & 255) - c[0], dg = (int)((pal[i] >> 8) & 255) - c[1], db = (int)((pal[i] >> 16) & 255) - c[2]; int d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = i; } } return (uint8_t)best; };
	const uint8_t *r = m_file.data() + m_layout.imageRecs + m_layout.imageRecSize * n; const int32_t first = S32(r), cnt = S32(r + 4);
	for (int32_t t = 0; t < cnt; t++) {
		uint8_t *tl = m_file.data() + m_layout.tileOff + 260ull * (first + t); const int tx = S16(tl) - x1, ty = S16(tl + 2) - y1;
		for (int yy = 0; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) {
			const int sx = tx + xx, sy = ty + yy; const uint8_t *q = (sx >= 0 && sy >= 0 && sx < w && sy < h) ? rgba + 4 * ((size_t)sy * w + sx) : nullptr;
			tl[4 + yy * 16 + xx] = (!q || q[3] < 128) ? 0 : nearest(q);
		}
	}
	m_dirty = true;
	return true;
}

} // namespace han2
