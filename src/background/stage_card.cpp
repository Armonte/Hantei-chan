#include "stage_card.h"

#include "../parts/pat_texconv.h" // pattex::EncodeDxt

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace stagecard {

namespace {

const SlotInfo kSlots[] = {
	{"Name (English)", "GRP\\BgSelect\\stsel_en", "chr_stsel_en", "dds", 256, 32, 3},
	{"Name (Japanese)", "GRP\\BgSelect\\stsel_jp", "chr_stsel_jp", "dds", 256, 32, 3},
	{"Preview", "GRP\\BgSelect\\stsel_view", "chr_stsel_view", "dds", 256, 96, 5},
	{"Round call", "grp\\RoundCall_AA\\round\\rd_stage", "rd_st", "bmp", 256, 16, 0},
};

void Put32(std::vector<uint8_t>& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b[at + i] = (uint8_t)(v >> (8 * i)); }
void Put16(std::vector<uint8_t>& b, size_t at, uint16_t v) { b[at] = (uint8_t)v; b[at + 1] = (uint8_t)(v >> 8); }

} // namespace

const SlotInfo& Info(Slot s) { return kSlots[(int)s]; }

std::string GamePath(Slot s, int stageId)
{
	const SlotInfo& i = Info(s);
	char n[96];
	snprintf(n, sizeof n, "%s\\%s%02d.%s", i.folder, i.prefix, stageId, i.ext);
	return n;
}

void Resize(const uint8_t* src, int sw, int sh, std::vector<uint8_t>& dst, int dw, int dh)
{
	dst.assign((size_t)dw * dh * 4, 0);
	if (sw <= 0 || sh <= 0) return;
	// Premultiplied copy, then bilinear (downscale by more than 2x averages the covered source box instead).
	std::vector<float> pm((size_t)sw * sh * 4);
	for (size_t i = 0; i < (size_t)sw * sh; i++) {
		const float a = src[i * 4 + 3] / 255.f;
		for (int c = 0; c < 3; c++) pm[i * 4 + c] = src[i * 4 + c] / 255.f * a;
		pm[i * 4 + 3] = a;
	}
	const float fx = (float)sw / dw, fy = (float)sh / dh;
	for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
		float acc[4] = {0, 0, 0, 0};
		if (fx > 2.f || fy > 2.f) {
			const int x0 = (int)(x * fx), x1 = std::max(x0 + 1, (int)((x + 1) * fx)), y0 = (int)(y * fy), y1 = std::max(y0 + 1, (int)((y + 1) * fy));
			int n = 0;
			for (int yy = y0; yy < std::min(y1, sh); yy++) for (int xx = x0; xx < std::min(x1, sw); xx++, n++)
				for (int c = 0; c < 4; c++) acc[c] += pm[((size_t)yy * sw + xx) * 4 + c];
			for (float& v : acc) v /= std::max(1, n);
		} else {
			const float sx = std::clamp((x + 0.5f) * fx - 0.5f, 0.f, (float)sw - 1), sy = std::clamp((y + 0.5f) * fy - 0.5f, 0.f, (float)sh - 1);
			const int ix = (int)sx, iy = (int)sy, ix1 = std::min(ix + 1, sw - 1), iy1 = std::min(iy + 1, sh - 1);
			const float tx = sx - ix, ty = sy - iy;
			for (int c = 0; c < 4; c++) {
				auto at = [&](int xx, int yy) { return pm[((size_t)yy * sw + xx) * 4 + c]; };
				acc[c] = (at(ix, iy) * (1 - tx) + at(ix1, iy) * tx) * (1 - ty) + (at(ix, iy1) * (1 - tx) + at(ix1, iy1) * tx) * ty;
			}
		}
		uint8_t* o = &dst[((size_t)y * dw + x) * 4];
		const float a = acc[3];
		for (int c = 0; c < 3; c++) o[c] = (uint8_t)std::lround(std::clamp(a > 0 ? acc[c] / a : 0.f, 0.f, 1.f) * 255.f);
		o[3] = (uint8_t)std::lround(std::clamp(a, 0.f, 1.f) * 255.f);
	}
}

std::vector<uint8_t> MakeDds(const uint8_t* rgba, int w, int h, int dxt)
{
	std::vector<uint8_t> blocks;
	pattex::EncodeDxt(rgba, w, h, true, blocks); // DXT5: 8 alpha bytes + 8 colour bytes per block
	if (dxt == 3) {
		// Same colour half; explicit 4-bit alpha (row-major, low nibble first) instead of DXT5's interpolated alpha.
		size_t b = 0;
		for (int by = 0; by < h; by += 4) for (int bx = 0; bx < w; bx += 4, b++) {
			uint8_t* a = &blocks[b * 16];
			for (int py = 0; py < 4; py++) for (int px = 0; px < 4; px += 2) {
				auto al = [&](int xx) { return (int)std::lround(rgba[((size_t)(by + py) * w + bx + xx) * 4 + 3] * 15 / 255.0); };
				a[py * 2 + px / 2] = (uint8_t)(al(px) | (al(px + 1) << 4));
			}
		}
	}
	std::vector<uint8_t> f(128, 0);
	memcpy(f.data(), "DDS ", 4);
	Put32(f, 4, 124);
	Put32(f, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000);   // CAPS HEIGHT WIDTH PIXELFORMAT LINEARSIZE
	Put32(f, 12, (uint32_t)h);
	Put32(f, 16, (uint32_t)w);
	Put32(f, 20, (uint32_t)blocks.size());
	Put32(f, 76, 32);
	Put32(f, 80, 0x4);                                    // FOURCC
	memcpy(&f[84], dxt == 3 ? "DXT3" : "DXT5", 4);
	Put32(f, 108, 0x1000);                                // TEXTURE
	f.insert(f.end(), blocks.begin(), blocks.end());
	return f;
}

std::vector<uint8_t> MakeRoundCallBmp(const uint8_t* rgba, int w, int h)
{
	const uint32_t img = (uint32_t)w * h * 4 + 2;         // vanilla: biSizeImage counts 2 trailing pad bytes
	std::vector<uint8_t> b(54 + img, 0);
	b[0] = 'B'; b[1] = 'M';
	Put32(b, 2, (uint32_t)b.size());
	Put32(b, 10, 54);
	Put32(b, 14, 40);
	Put32(b, 18, (uint32_t)w);
	Put32(b, 22, (uint32_t)h);
	Put16(b, 26, 1);
	Put16(b, 28, 32);
	Put32(b, 34, img);
	Put32(b, 38, 2834);
	Put32(b, 42, 2834);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		const uint8_t* s = &rgba[((size_t)(h - 1 - y) * w + x) * 4];
		uint8_t* d = &b[54 + ((size_t)y * w + x) * 4];
		d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
	}
	return b;
}

std::vector<uint8_t> BuildSlot(Slot s, const uint8_t* rgba, int w, int h)
{
	const SlotInfo& i = Info(s);
	std::vector<uint8_t> px;
	if (w == i.w && h == i.h) px.assign(rgba, rgba + (size_t)w * h * 4);
	else Resize(rgba, w, h, px, i.w, i.h);
	return i.dxt ? MakeDds(px.data(), i.w, i.h, i.dxt) : MakeRoundCallBmp(px.data(), i.w, i.h);
}

} // namespace stagecard
