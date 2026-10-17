#include "pat_texconv.h"
#include "../png_writer.h"
#include <algorithm>
#include <cstring>

namespace pattex {

bool IsRawBgra(int t) { return t == 21 || t == 22; }
bool IsDxt(int t) { return t == 1 || t == 3 || t == 5; }

static void Rgb565(uint16_t c, int &r, int &g, int &b) { r = ((c >> 11) & 31) * 255 / 31; g = ((c >> 5) & 63) * 255 / 63; b = (c & 31) * 255 / 31; }

void DecodeDxt(const uint8_t *src, int w, int h, bool dxt5, uint8_t *out) { DecodeDxtKind(src, w, h, dxt5 ? 5 : 1, out); }

void DecodeDxtKind(const uint8_t *src, int w, int h, int kind, uint8_t *out) {
	const bool dxt3 = kind == 3, dxt5 = kind == 5 || dxt3;   // dxt5: a 16-byte block with an alpha half (BC3 interpolated / BC2 explicit)
	const int bw = (w + 3) / 4, bh = (h + 3) / 4;
	for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
		const uint8_t *blk = src + (size_t)(by * bw + bx) * (dxt5 ? 16 : 8);
		uint8_t alpha[16]; for (int i = 0; i < 16; i++) alpha[i] = 255;
		if (dxt3) {   // BC2: 16 explicit 4-bit alphas, low nibble first
			for (int i = 0; i < 16; i++) { const int a = (blk[i >> 1] >> ((i & 1) * 4)) & 15; alpha[i] = (uint8_t)(a * 17); }
			blk += 8;
		} else if (dxt5) {
			const int a0 = blk[0], a1 = blk[1]; int pal[8] = {a0, a1};
			if (a0 > a1) for (int k = 1; k < 7; k++) pal[k + 1] = ((7 - k) * a0 + k * a1) / 7;
			else { for (int k = 1; k < 5; k++) pal[k + 1] = ((5 - k) * a0 + k * a1) / 5; pal[6] = 0; pal[7] = 255; }
			uint64_t bits = 0; for (int k = 0; k < 6; k++) bits |= (uint64_t)blk[2 + k] << (8 * k);
			for (int i = 0; i < 16; i++) alpha[i] = (uint8_t)pal[(bits >> (3 * i)) & 7];
			blk += 8;
		}
		const uint16_t c0 = blk[0] | (blk[1] << 8), c1 = blk[2] | (blk[3] << 8);
		int col[4][4]; Rgb565(c0, col[0][0], col[0][1], col[0][2]); Rgb565(c1, col[1][0], col[1][1], col[1][2]);
		col[0][3] = col[1][3] = col[2][3] = col[3][3] = 255;
		if (dxt5 || c0 > c1) for (int k = 0; k < 3; k++) { col[2][k] = (2 * col[0][k] + col[1][k]) / 3; col[3][k] = (col[0][k] + 2 * col[1][k]) / 3; }
		else { for (int k = 0; k < 3; k++) { col[2][k] = (col[0][k] + col[1][k]) / 2; col[3][k] = 0; } col[3][3] = 0; }
		const uint32_t idx = blk[4] | (blk[5] << 8) | (blk[6] << 16) | ((uint32_t)blk[7] << 24);
		for (int i = 0; i < 16; i++) {
			const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2); if (x >= w || y >= h) continue;
			const int *c = col[(idx >> (2 * i)) & 3]; uint8_t *o = &out[((size_t)y * w + x) * 4];
			o[0] = (uint8_t)c[0]; o[1] = (uint8_t)c[1]; o[2] = (uint8_t)c[2]; o[3] = dxt5 ? alpha[i] : (uint8_t)c[3];
		}
	}
}

static uint16_t To565(int r, int g, int b) { return (uint16_t)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255)); }

void EncodeDxt(const uint8_t *rgba, int w, int h, bool dxt5, std::vector<uint8_t> &out) { EncodeDxtKind(rgba, w, h, dxt5 ? 5 : 1, out); }

void EncodeDxtKind(const uint8_t *rgba, int w, int h, int kind, std::vector<uint8_t> &out) {
	const bool dxt3 = kind == 3, dxt5 = kind == 5 || dxt3;
	const int bw = (w + 3) / 4, bh = (h + 3) / 4;
	out.assign((size_t)bw * bh * (dxt5 ? 16 : 8), 0);
	for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
		uint8_t px[16][4]; bool transparent = false;
		for (int i = 0; i < 16; i++) { const int x = std::min(bx * 4 + (i & 3), w - 1), y = std::min(by * 4 + (i >> 2), h - 1); memcpy(px[i], &rgba[((size_t)y * w + x) * 4], 4); if (px[i][3] < 128) transparent = true; }
		uint8_t *blk = &out[(size_t)(by * bw + bx) * (dxt5 ? 16 : 8)];
		if (dxt3) {   // BC2 explicit alpha: nibble = round(a * 15 / 255)
			for (int i = 0; i < 8; i++) blk[i] = 0;
			for (int i = 0; i < 16; i++) blk[i >> 1] |= (uint8_t)(((px[i][3] * 15 + 127) / 255) << ((i & 1) * 4));
			blk += 8; transparent = false;   // the BC2 colour block is always the 4-colour mode
		} else if (dxt5) {
			int lo = 255, hi = 0; for (auto &p : px) { lo = std::min<int>(lo, p[3]); hi = std::max<int>(hi, p[3]); }
			blk[0] = (uint8_t)hi; blk[1] = (uint8_t)lo; uint64_t bits = 0;
			if (hi > lo) {
				int pal[8] = {hi, lo}; for (int k = 1; k < 7; k++) pal[k + 1] = ((7 - k) * hi + k * lo) / 7;
				for (int i = 0; i < 16; i++) { int best = 0, bd = 1 << 30; for (int k = 0; k < 8; k++) { const int d = std::abs(pal[k] - px[i][3]); if (d < bd) { bd = d; best = k; } } bits |= (uint64_t)best << (3 * i); }
			}
			for (int k = 0; k < 6; k++) blk[2 + k] = (uint8_t)(bits >> (8 * k));
			blk += 8; transparent = false;   // BC3 colour block is always the 4-colour mode
		}
		int mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0}; bool any = false;
		for (auto &p : px) { if (!dxt5 && p[3] < 128) continue; any = true; for (int k = 0; k < 3; k++) { mn[k] = std::min<int>(mn[k], p[k]); mx[k] = std::max<int>(mx[k], p[k]); } }
		if (!any) { mn[0] = mn[1] = mn[2] = mx[0] = mx[1] = mx[2] = 0; }
		uint16_t c0 = To565(mx[0], mx[1], mx[2]), c1 = To565(mn[0], mn[1], mn[2]);
		if (transparent) { if (c0 > c1) std::swap(c0, c1); }        // 3-colour mode: index 3 = transparent
		else if (c0 < c1) std::swap(c0, c1);
		else if (c0 == c1 && c0 > 0) c1 = c0 - 1;                     // keep the 4-colour mode
		int col[4][3]; Rgb565(c0, col[0][0], col[0][1], col[0][2]); Rgb565(c1, col[1][0], col[1][1], col[1][2]);
		const int ncol = transparent ? 3 : 4;
		if (ncol == 4) for (int k = 0; k < 3; k++) { col[2][k] = (2 * col[0][k] + col[1][k]) / 3; col[3][k] = (col[0][k] + 2 * col[1][k]) / 3; }
		else for (int k = 0; k < 3; k++) col[2][k] = (col[0][k] + col[1][k]) / 2;
		uint32_t idx = 0;
		for (int i = 0; i < 16; i++) {
			int best = 0;
			if (transparent && px[i][3] < 128) best = 3;
			else { int bd = 1 << 30; for (int k = 0; k < ncol; k++) { const int d = (col[k][0] - px[i][0]) * (col[k][0] - px[i][0]) + (col[k][1] - px[i][1]) * (col[k][1] - px[i][1]) + (col[k][2] - px[i][2]) * (col[k][2] - px[i][2]); if (d < bd) { bd = d; best = k; } } }
			idx |= (uint32_t)best << (2 * i);
		}
		blk[0] = c0 & 255; blk[1] = c0 >> 8; blk[2] = c1 & 255; blk[3] = c1 >> 8;
		for (int k = 0; k < 4; k++) blk[4 + k] = (uint8_t)(idx >> (8 * k));
	}
}

bool Decode(const PartGfx<> &g, std::vector<uint8_t> &rgba, int &w, int &h, std::string *err) {
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	w = g.w; h = g.h;
	if (w <= 0 || h <= 0) return fail("the texture has no size");
	rgba.assign((size_t)w * h * 4, 0);
	if (g.imageSize == 0 && g.data) {   // MBAACC PGTX: BGRA
		if (g.bpp != 32) return fail("only 32-bit PGTX textures are supported");
		const uint8_t *s = (const uint8_t *)g.data;
		for (size_t i = 0; i < (size_t)w * h; i++) { rgba[i * 4] = s[i * 4 + 2]; rgba[i * 4 + 1] = s[i * 4 + 1]; rgba[i * 4 + 2] = s[i * 4]; rgba[i * 4 + 3] = s[i * 4 + 3]; }
		return true;
	}
	const uint8_t *s = (const uint8_t *)g.imageData;
	if (!s || g.imageSize <= 0) return fail("the texture has no pixel data");
	if (IsRawBgra(g.type)) {
		if ((size_t)g.imageSize < (size_t)w * h * 4) return fail("the texture data is shorter than its size");
		for (size_t i = 0; i < (size_t)w * h; i++) { rgba[i * 4] = s[i * 4 + 2]; rgba[i * 4 + 1] = s[i * 4 + 1]; rgba[i * 4 + 2] = s[i * 4]; rgba[i * 4 + 3] = g.type == 22 ? 255 : s[i * 4 + 3]; }
		return true;
	}
	if (IsDxt(g.type)) {
		const size_t need = (size_t)((w + 3) / 4) * ((h + 3) / 4) * (g.type == 1 ? 8 : 16);
		if ((size_t)g.imageSize < need) return fail("the texture data is shorter than its size");
		DecodeDxtKind(s, w, h, g.type, rgba.data());
		return true;
	}
	return fail("unknown texture kind");
}

bool CheckSize(const PartGfx<> &g, int w, int h, std::string *err) {
	const bool pgtx = g.imageSize == 0 && g.data;
	if (w <= 0 || h <= 0) { if (err) *err = "empty image"; return false; }
	if (!pgtx && (w % 128 || h % 128)) { if (err) *err = "UNI2 / MBTL part textures need sides that are multiples of 128 (this one is " + std::to_string(w) + " x " + std::to_string(h) + ")"; return false; }
	return true;
}

static void Header(char hdr[124], int type, int w, int h, size_t dataSize) {
	uint32_t d[31] = {};
	d[0] = 124; d[2] = (uint32_t)h; d[3] = (uint32_t)w;
	d[26] = 0x1000;   // caps: texture
	uint32_t *pf = &d[18]; pf[0] = 32;
	if (IsDxt(type)) { d[1] = 0x81007; d[4] = (uint32_t)dataSize; pf[1] = 4; memcpy(&pf[2], type == 1 ? "DXT1" : type == 3 ? "DXT3" : "DXT5", 4); }
	else { d[1] = 0x1007; pf[1] = type == 22 ? 0x40 : 0x41; pf[3] = 32; pf[4] = 0xFF0000; pf[5] = 0xFF00; pf[6] = 0xFF; pf[7] = type == 22 ? 0 : 0xFF000000u; }
	memcpy(hdr, d, 124);
}

bool Encode(PartGfx<> &g, const uint8_t *rgba, int w, int h, int type, std::string *err) {
	if (!CheckSize(g, w, h, err)) return false;
	const bool pgtx = g.imageSize == 0 && g.data;
	if (pgtx && (type == -1 || IsRawBgra(type))) {   // MBAACC: stays PGTX (MBAACC has no PGT2)
		char *d = new char[(size_t)w * h * 4];
		for (size_t i = 0; i < (size_t)w * h; i++) { d[i * 4] = (char)rgba[i * 4 + 2]; d[i * 4 + 1] = (char)rgba[i * 4 + 1]; d[i * 4 + 2] = (char)rgba[i * 4]; d[i * 4 + 3] = (char)rgba[i * 4 + 3]; }
		g.data = d; g.w = w; g.h = h; g.bpp = 32;   // the previous data points into the loaded file buffer: not freed here
		return true;
	}
	if (pgtx) { if (err) *err = "MBAACC part textures are 32-bit (PGTX): DXT is UNI2 / MBTL only"; return false; }
	if (type == -1) type = g.type;
	if (!IsDxt(type) && !IsRawBgra(type)) type = 21;
	std::vector<uint8_t> data;
	if (IsDxt(type)) EncodeDxtKind(rgba, w, h, type, data);
	else { data.resize((size_t)w * h * 4); for (size_t i = 0; i < (size_t)w * h; i++) { data[i * 4] = rgba[i * 4 + 2]; data[i * 4 + 1] = rgba[i * 4 + 1]; data[i * 4 + 2] = rgba[i * 4]; data[i * 4 + 3] = type == 22 ? 0 : rgba[i * 4 + 3]; } }
	const bool keepHeader = g.type == type && g.w == w && g.h == h && g.imageSize == (int)data.size();
	if (!keepHeader) Header(g.ddsHeader, type, w, h, data.size());
	char *img = new char[data.size()]; memcpy(img, data.data(), data.size());
	g.imageData = img; g.imageSize = (int)data.size(); g.s3tc = (unsigned char *)img; g.dontDelete = true;
	g.type = type; g.w = w; g.h = h; g.uvBpp[0] = std::max(1, w / 256); g.uvBpp[1] = std::max(1, h / 256); g.pgte[0] = w; g.pgte[1] = h;
	g.noCompress = false;   // saved RLE-packed, like the shipped textures
	return true;
}

bool ExportPng(const PartGfx<> &g, const std::string &path, std::string *err) {
	std::vector<uint8_t> px; int w, h;
	if (!Decode(g, px, w, h, err)) return false;
	std::string e; if (!WritePngRgba(path, px.data(), w, h, e)) { if (err) *err = e; return false; }
	return true;
}

bool ImportPng(PartGfx<> &g, const std::string &path, int type, std::string *err) {
	std::vector<uint8_t> px; int w, h; std::string e;
	if (!ReadImageRgba(path, px, w, h, e)) { if (err) *err = e; return false; }
	return Encode(g, px.data(), w, h, type, err);
}

} // namespace pattex
