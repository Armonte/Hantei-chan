#include "qoh_img.h"
#include <cstring>

namespace han2 {

static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

bool ParseQohImg(const uint8_t *p, size_t n, QohImg &o, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	o = QohImg();
	if (n < 20) return fail("too short");
	o.reserved = R32(p); o.paletteEntries = R32(p + 4); o.bpp = R32(p + 8); o.width = R32(p + 12); o.height = R32(p + 16);
	if (o.bpp != 4 && o.bpp != 8 && o.bpp != 24) return fail("bits per pixel must be 4, 8 or 24");
	if ((o.bpp == 24) != (o.paletteEntries == 0)) return fail("palette entries and bit depth disagree");
	if (o.paletteEntries > 256 || !o.width || !o.height || o.width > 4096 || o.height > 4096) return fail("implausible size");
	const size_t pal = 4ull * o.paletteEntries, px = o.rowBytes() * o.height;
	if (20 + pal + px != n) return fail("size does not match the header");
	o.palette.assign(p + 20, p + 20 + pal); o.pixels.assign(p + 20 + pal, p + n);
	o.rgba.assign((size_t)o.width * o.height * 4, 0);
	for (uint32_t y = 0; y < o.height; y++) {
		const uint8_t *row = o.pixels.data() + o.rowBytes() * y; uint8_t *dst = &o.rgba[(size_t)(o.height - 1 - y) * o.width * 4];   // DIB convention: the first stored row is the bottom row
		for (uint32_t x = 0; x < o.width; x++) {
			uint8_t *d = dst + 4 * x;
			if (o.bpp == 24) { d[0] = row[3 * x + 2]; d[1] = row[3 * x + 1]; d[2] = row[3 * x]; d[3] = 255; continue; }
			const unsigned idx = o.bpp == 8 ? row[x] : ((x & 1) ? row[x / 2] >> 4 : row[x / 2] & 15);
			if (idx >= o.paletteEntries) { d[3] = 0; continue; }
			const uint8_t *e = &o.palette[4 * idx]; d[0] = e[2]; d[1] = e[1]; d[2] = e[0]; d[3] = 255;
		}
	}
	return true;
}

bool QohImgSetRgba(QohImg &o, const uint8_t *rgba, int w, int h, std::string *err)
{
	if ((uint32_t)w != o.width || (uint32_t)h != o.height) { if (err) *err = "size mismatch: the image keeps its dimensions"; return false; }
	std::vector<uint8_t> px(o.pixels.size(), 0);
	for (uint32_t y = 0; y < o.height; y++) {
		uint8_t *row = px.data() + o.rowBytes() * y; const uint8_t *src = rgba + (size_t)y * o.width * 4;
		for (uint32_t x = 0; x < o.width; x++) {
			const uint8_t *c = src + 4 * x;
			if (o.bpp == 24) { row[3 * x] = c[2]; row[3 * x + 1] = c[1]; row[3 * x + 2] = c[0]; continue; }
			int best = 0, bd = 1 << 30;
			for (uint32_t i = 0; i < o.paletteEntries; i++) { int dr = (int)o.palette[4 * i + 2] - c[0], dg = (int)o.palette[4 * i + 1] - c[1], db = (int)o.palette[4 * i] - c[2]; int d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = (int)i; } }
			if (o.bpp == 8) row[x] = (uint8_t)best; else row[x / 2] |= (uint8_t)((x & 1) ? best << 4 : best);
		}
	}
	// unchanged pixels keep the stored bytes (the nearest-colour search may pick another index of the same colour)
	QohImg cur; { std::vector<uint8_t> tmp; SerializeQohImg(o, tmp); std::string e; if (ParseQohImg(tmp.data(), tmp.size(), cur, &e) && cur.rgba.size() == (size_t)w * h * 4 && memcmp(cur.rgba.data(), rgba, cur.rgba.size()) == 0) return true; }
	o.pixels = std::move(px);
	std::vector<uint8_t> tmp; SerializeQohImg(o, tmp); std::string e; QohImg re; if (ParseQohImg(tmp.data(), tmp.size(), re, &e)) o.rgba = re.rgba;
	return true;
}

void SerializeQohImg(const QohImg &i, std::vector<uint8_t> &o)
{
	o.resize(20); memcpy(o.data(), &i.reserved, 4); memcpy(o.data() + 4, &i.paletteEntries, 4); memcpy(o.data() + 8, &i.bpp, 4); memcpy(o.data() + 12, &i.width, 4); memcpy(o.data() + 16, &i.height, 4);
	o.insert(o.end(), i.palette.begin(), i.palette.end()); o.insert(o.end(), i.pixels.begin(), i.pixels.end());
}

} // namespace han2
