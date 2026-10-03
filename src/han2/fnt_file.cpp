#include "fnt_file.h"
#include <cstring>

namespace han2 { namespace fnt {

static uint16_t R16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void W16(std::vector<uint8_t> &o, uint16_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 2); }
static void W32(std::vector<uint8_t> &o, uint32_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 4); }

bool Parse(const uint8_t *b, size_t n, Font &f, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	f = Font();
	if (n < 8) return fail("short file");
	size_t p = 0;
	if (R16(b) == 0) { if (n < 6 + 8) return fail("short file"); f.hasPrefix = true; f.halfOffset = R32(b + 2); p = 6; }
	const uint8_t *body = b + p; const size_t bn = n - p;
	if (bn < 65030) return fail("body shorter than the index table");
	f.nGlyphs = R16(body); f.width = R16(body + 2); f.height = R16(body + 4); f.pixelsPerByte = body[6]; f.bitsPerPixel = body[7];
	if (!f.pixelsPerByte) return fail("pixelsPerByte is 0");
	const size_t gl = (size_t)f.nGlyphs * f.height * f.RowBytes();
	size_t cnt = kIndexCount;
	if (8 + cnt * 2 + gl > bn) cnt = kIndexCountGof1;      // GOF1 layout: glyph data one table entry earlier
	const size_t base = 8 + cnt * 2;
	if (base + gl > bn) return fail("glyph data runs past the file");
	f.index.resize(cnt); memcpy(f.index.data(), body + 8, cnt * 2);
	f.glyphs.assign(body + base, body + base + gl);
	size_t q = base + gl;
	if (f.hasPrefix) {
		if (f.halfOffset != q) return fail("half-section offset does not follow the glyph data");
		if (q + 4 > bn) return fail("no half-width header");
		f.hasHalf = true; f.halfWidth = R16(body + q); f.halfHeight = R16(body + q + 2); q += 4;
		const size_t hb = (size_t)256 * f.halfHeight * ((f.halfWidth + f.pixelsPerByte - 1) / f.pixelsPerByte);
		if (q + hb > bn) return fail("half-width glyphs run past the file");
		f.half.assign(body + q, body + q + hb); q += hb;
	}
	f.tail.assign(body + q, body + bn);
	return true;
}

bool Serialize(const Font &f, std::vector<uint8_t> &o, std::string *err)
{
	(void)err; o.clear();
	if (f.hasPrefix) { W16(o, 0); W32(o, f.halfOffset); }
	W16(o, f.nGlyphs); W16(o, f.width); W16(o, f.height); o.push_back(f.pixelsPerByte); o.push_back(f.bitsPerPixel);
	for (uint16_t v : f.index) W16(o, v);
	o.insert(o.end(), f.glyphs.begin(), f.glyphs.end());
	if (f.hasHalf) { W16(o, f.halfWidth); W16(o, f.halfHeight); o.insert(o.end(), f.half.begin(), f.half.end()); }
	o.insert(o.end(), f.tail.begin(), f.tail.end());
	return true;
}

}} // namespace
