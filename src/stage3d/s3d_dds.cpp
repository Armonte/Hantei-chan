#include "s3d_dds.h"
#include "s3d_model.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace s3d {
namespace {

uint32_t Rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
uint16_t Rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

void Rgb565(uint16_t c, uint8_t* o)
{
	const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
	o[0] = uint8_t((r << 3) | (r >> 2));
	o[1] = uint8_t((g << 2) | (g >> 4));
	o[2] = uint8_t((b << 3) | (b >> 2));
}

// Colour part of a BC1/BC2/BC3 block. `bc1` enables the 3-colour + transparent mode when c0 <= c1.
void ColorBlock(const uint8_t* b, bool bc1, uint8_t out[16][4])
{
	const uint16_t c0 = Rd16(b), c1 = Rd16(b + 2);
	uint8_t pal[4][4] = {};
	Rgb565(c0, pal[0]);
	Rgb565(c1, pal[1]);
	pal[0][3] = pal[1][3] = pal[2][3] = pal[3][3] = 255;
	if (!bc1 || c0 > c1) {
		for (int k = 0; k < 3; ++k) {
			pal[2][k] = uint8_t((2 * pal[0][k] + pal[1][k] + 1) / 3);
			pal[3][k] = uint8_t((pal[0][k] + 2 * pal[1][k] + 1) / 3);
		}
	} else {
		for (int k = 0; k < 3; ++k) pal[2][k] = uint8_t((pal[0][k] + pal[1][k]) / 2);
		pal[3][0] = pal[3][1] = pal[3][2] = 0;
		pal[3][3] = 0;
	}
	const uint32_t bits = Rd32(b + 4);
	for (int i = 0; i < 16; ++i) memcpy(out[i], pal[(bits >> (2 * i)) & 3], 4);
}

void AlphaBc3(const uint8_t* b, uint8_t out[16][4])
{
	uint8_t a[8];
	a[0] = b[0];
	a[1] = b[1];
	if (a[0] > a[1]) for (int i = 1; i < 7; ++i) a[i + 1] = uint8_t(((7 - i) * a[0] + i * a[1] + 3) / 7);
	else { for (int i = 1; i < 5; ++i) a[i + 1] = uint8_t(((5 - i) * a[0] + i * a[1] + 2) / 5); a[6] = 0; a[7] = 255; }
	uint64_t bits = 0;
	for (int i = 0; i < 6; ++i) bits |= uint64_t(b[2 + i]) << (8 * i);
	for (int i = 0; i < 16; ++i) out[i][3] = a[(bits >> (3 * i)) & 7];
}

void AlphaBc2(const uint8_t* b, uint8_t out[16][4])
{
	for (int i = 0; i < 16; ++i) { const int v = (b[i / 2] >> ((i & 1) * 4)) & 15; out[i][3] = uint8_t(v * 17); }
}

int Shift(uint32_t mask) { int s = 0; if (!mask) return 0; while (!(mask & 1)) { mask >>= 1; ++s; } return s; }
int Bits(uint32_t mask) { int n = 0; while (mask) { n += mask & 1; mask >>= 1; } return n; }
uint8_t Chan(uint32_t px, uint32_t mask)
{
	if (!mask) return 255;
	const uint32_t v = (px & mask) >> Shift(mask);
	const int n = Bits(mask);
	return n >= 8 ? uint8_t(v >> (n - 8)) : uint8_t(v * 255 / ((1u << n) - 1));
}

}  // namespace

bool DecodeDds(const uint8_t* d, size_t size, Image& out, std::string& err)
{
	out = Image{};
	if (!d || size < 128 || memcmp(d, "DDS ", 4) != 0 || Rd32(d + 4) != 124) { err = "not a DDS file"; return false; }
	const int h = int(Rd32(d + 12)), w = int(Rd32(d + 16));
	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { err = "bad DDS size"; return false; }
	const uint32_t pfFlags = Rd32(d + 80), fourcc = Rd32(d + 84), bpp = Rd32(d + 88);
	const uint32_t rm = Rd32(d + 92), gm = Rd32(d + 96), bm = Rd32(d + 100), am = Rd32(d + 104);
	const uint8_t* p = d + 128;
	const size_t avail = size - 128;
	out.width = w;
	out.height = h;
	out.rgba.assign(size_t(w) * h * 4, 255);
	if (pfFlags & 4) {   // DDPF_FOURCC
		int kind = 0;
		if (fourcc == 0x31545844) kind = 1, out.format = "DXT1";
		else if (fourcc == 0x33545844 || fourcc == 0x32545844) kind = 3, out.format = "DXT3";
		else if (fourcc == 0x35545844 || fourcc == 0x34545844) kind = 5, out.format = "DXT5";
		else { err = "unsupported DDS fourcc"; return false; }
		const int bw = (w + 3) / 4, bh = (h + 3) / 4, bsz = kind == 1 ? 8 : 16;
		if (uint64_t(bw) * bh * bsz > avail) { err = "DDS data truncated"; return false; }
		uint8_t px[16][4];
		for (int by = 0; by < bh; ++by)
			for (int bx = 0; bx < bw; ++bx) {
				const uint8_t* b = p + (size_t(by) * bw + bx) * bsz;
				ColorBlock(kind == 1 ? b : b + 8, kind == 1, px);
				if (kind == 3) AlphaBc2(b, px);
				if (kind == 5) AlphaBc3(b, px);
				for (int i = 0; i < 16; ++i) {
					const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
					if (x < w && y < h) memcpy(&out.rgba[(size_t(y) * w + x) * 4], px[i], 4);
				}
			}
		return true;
	}
	if ((pfFlags & 0x40) && (bpp == 32 || bpp == 24)) {   // DDPF_RGB
		const size_t stride = size_t(w) * (bpp / 8);
		if (stride * h > avail) { err = "DDS data truncated"; return false; }
		out.format = bpp == 32 ? (am && (pfFlags & 1) ? "A8R8G8B8" : "X8R8G8B8") : "R8G8B8";
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x) {
				const uint8_t* s = p + size_t(y) * stride + size_t(x) * (bpp / 8);
				const uint32_t v = bpp == 32 ? Rd32(s) : uint32_t(s[0] | (s[1] << 8) | (s[2] << 16));
				uint8_t* o = &out.rgba[(size_t(y) * w + x) * 4];
				o[0] = Chan(v, rm); o[1] = Chan(v, gm); o[2] = Chan(v, bm);
				o[3] = (pfFlags & 1) ? Chan(v, am) : 255;
			}
		return true;
	}
	err = "unsupported DDS pixel format";
	return false;
}

bool LoadDdsFile(const std::string& utf8Path, Image& out, std::string& err)
{
	std::ifstream f(std::filesystem::u8path(utf8Path), std::ios::binary);
	if (!f) { err = "cannot open " + utf8Path; return false; }
	std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (buf.size() >= 4 && memcmp(buf.data(), "DDS ", 4) != 0) {   // UNIEL: RC4-enciphered DDS
		std::vector<uint8_t> plain = buf;
		UnielCrypt(plain.data(), plain.size());
		if (plain.size() >= 4 && memcmp(plain.data(), "DDS ", 4) == 0) return DecodeDds(plain.data(), plain.size(), out, err);
	}
	return DecodeDds(buf.data(), buf.size(), out, err);
}

}  // namespace s3d
