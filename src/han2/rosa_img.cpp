#include "rosa_img.h"
#include <cstring>

namespace han2 {

static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void W32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static const uint8_t kKey[9] = { 0x8C, 0x4B, 0x92, 0x4A, 0x20, 0x89, 0xC2, 0x97, 0xF7 };

std::string RosaStemOfName(const std::string &fileName)
{
	size_t s = fileName.find_last_of("/\\"); std::string b = s == std::string::npos ? fileName : fileName.substr(s + 1);
	size_t d = b.find_last_of('.'); if (d != std::string::npos) b = b.substr(0, d);
	for (auto &c : b) if (c >= 'a' && c <= 'z') c -= 32;
	if (b.size() > 16) b.resize(16);
	return b;
}

static uint8_t Mask(size_t i) { return (uint8_t)(((i >> 3) & 0xFF) + (~(8 * i) & 0xFF)); }
static void Dec(uint8_t *b, size_t n, const std::string &stem) { for (size_t i = 0; i < n; i++) { uint8_t t = (uint8_t)stem[i % stem.size()] ^ Mask(i) ^ b[i]; b[i] = (uint8_t)((t - kKey[i % 9] - 109) & 0xFF); } }
static void Enc(uint8_t *b, size_t n, const std::string &stem) { for (size_t i = 0; i < n; i++) { uint8_t t = (uint8_t)((b[i] + kKey[i % 9] + 109) & 0xFF); b[i] = t ^ (uint8_t)stem[i % stem.size()] ^ Mask(i); } }

size_t RosaImg::rowBytes() const
{
	switch (bpp) { case 4: return (((size_t)width + 1) / 2 + 3) & ~3ull; case 8: return ((size_t)width + 3) & ~3ull; case 16: return (2ull * width + 3) & ~3ull; default: return (3ull * width + 3) & ~3ull; }
}

bool ParseRosaImg(const uint8_t *p, size_t n, const std::string &stem, RosaImg &o, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	o = RosaImg();
	if (n < 28 || R32(p) != 0) return fail("not a Rosa IMG");
	const uint32_t ver = R32(p + 4);
	size_t hdr;
	if (ver == 3 || ver == 4) {
		if (n < 44 || stem.empty()) return fail("too short");
		o.version = ver; memcpy(o.nameSeed, p + 8, 16);
		uint8_t dec[16]; memcpy(dec, p + 8, 16);
		for (int i = 0; i < 16; i++) dec[i] ^= kKey[i % 9];
		for (size_t i = 0; i < stem.size(); i++) if (dec[i] != (uint8_t)stem[i]) return fail("the name block does not start with the file stem");
		uint32_t f[5]; for (int k = 0; k < 5; k++) { uint8_t t[4]; memcpy(t, p + 0x18 + 4 * k, 4); Dec(t, 4, stem); f[k] = R32(t); }
		o.flag = f[0]; o.paletteCount = f[1]; o.bpp = f[2]; o.width = f[3]; o.height = f[4]; hdr = 44;
	} else if (ver <= 2) {
		o.version = ver; o.flag = R32(p + 8); o.paletteCount = R32(p + 12); o.bpp = R32(p + 16); o.width = R32(p + 20); o.height = R32(p + 24); hdr = 28;
	} else return fail("unknown version");
	if (o.paletteCount > 256 || !o.width || !o.height || o.width > 8192 || o.height > 8192 || (o.bpp != 4 && o.bpp != 8 && o.bpp != 16 && o.bpp != 24)) return fail("implausible header");
	const size_t pal = 4ull * o.paletteCount, px = o.rowBytes() * o.height;
	if (hdr + pal + px != n) return fail("size does not match the header");
	o.palette.assign(p + hdr, p + hdr + pal); o.pixels.assign(p + hdr + pal, p + n);
	if (ver == 3 || ver == 4) { Dec(o.palette.data(), o.palette.size(), stem); Dec(o.pixels.data(), o.pixels.size(), stem); }
	o.rgba.assign((size_t)o.width * o.height * 4, 0);
	for (uint32_t y = 0; y < o.height; y++) {
		const uint8_t *row = o.pixels.data() + o.rowBytes() * y;
		for (uint32_t x = 0; x < o.width; x++) {
			uint8_t *d = &o.rgba[((size_t)y * o.width + x) * 4]; uint8_t b = 0, g = 0, r = 0;
			if (o.bpp == 24) { b = row[3 * x]; g = row[3 * x + 1]; r = row[3 * x + 2]; }
			else if (o.bpp == 8 || o.bpp == 4) { const unsigned idx = o.bpp == 8 ? row[x] : ((x & 1) ? row[x / 2] >> 4 : row[x / 2] & 15); if (idx < o.paletteCount) { b = o.palette[4 * idx]; g = o.palette[4 * idx + 1]; r = o.palette[4 * idx + 2]; } }
			else { const unsigned v = row[2 * x] | (row[2 * x + 1] << 8); r = (uint8_t)(((v >> 10) & 31) * 255 / 31); g = (uint8_t)(((v >> 5) & 31) * 255 / 31); b = (uint8_t)((v & 31) * 255 / 31); }
			d[0] = r; d[1] = g; d[2] = b; d[3] = (r | g | b) ? 255 : 0;
		}
	}
	return true;
}

void SerializeRosaImg(const RosaImg &i, const std::string &stem, std::vector<uint8_t> &o)
{
	const bool v4 = i.version == 3 || i.version == 4;
	o.assign(v4 ? 44 : 28, 0);
	W32(o.data() + 4, i.version);
	std::vector<uint8_t> pal = i.palette, px = i.pixels;
	if (v4) {
		memcpy(o.data() + 8, i.nameSeed, 16);
		const uint32_t f[5] = { i.flag, i.paletteCount, i.bpp, i.width, i.height };
		for (int k = 0; k < 5; k++) { uint8_t t[4]; W32(t, f[k]); Enc(t, 4, stem); memcpy(o.data() + 0x18 + 4 * k, t, 4); }
		Enc(pal.data(), pal.size(), stem); Enc(px.data(), px.size(), stem);
	} else { W32(o.data() + 8, i.flag); W32(o.data() + 12, i.paletteCount); W32(o.data() + 16, i.bpp); W32(o.data() + 20, i.width); W32(o.data() + 24, i.height); }
	o.insert(o.end(), pal.begin(), pal.end()); o.insert(o.end(), px.begin(), px.end());
}

} // namespace han2
