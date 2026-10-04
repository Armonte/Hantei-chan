#include "img_file.h"
#include <cstring>

namespace han2 {

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static int BytesPerPixel(uint32_t format) { return format == 0 || format == 1 ? 2 : format == 2 ? 4 : format == 3 ? 3 : 0; }

void DecodePixels16(const uint8_t *s, size_t px, int fmt16, std::vector<uint8_t> &rgba)
{
	rgba.resize(px * 4);
	for (size_t i = 0; i < px; i++) {
		uint8_t *d = &rgba[i * 4]; unsigned v = s[i * 2] | (s[i * 2 + 1] << 8);
		if (fmt16 == 0) { d[0] = (uint8_t)(255 * ((v >> 10) & 31) / 31); d[1] = (uint8_t)(255 * ((v >> 5) & 31) / 31); d[2] = (uint8_t)(255 * (v & 31) / 31); d[3] = (v >> 15) ? 255 : 0; }
		else { d[0] = (uint8_t)(255 * ((v >> 8) & 15) / 15); d[1] = (uint8_t)(255 * ((v >> 4) & 15) / 15); d[2] = (uint8_t)(255 * (v & 15) / 15); d[3] = (uint8_t)(255 * (v >> 12) / 15); }
	}
}

void EncodePixels16(const uint8_t *rgba, size_t px, int fmt16, std::vector<uint8_t> &words)
{
	words.resize(px * 2);
	for (size_t i = 0; i < px; i++) {
		const uint8_t *c = rgba + i * 4; unsigned v;
		if (fmt16 == 0) v = ((c[3] >= 128 ? 1u : 0u) << 15) | (((c[0] * 31 + 127) / 255) << 10) | (((c[1] * 31 + 127) / 255) << 5) | ((c[2] * 31 + 127) / 255);
		else v = (((c[3] * 15 + 127) / 255) << 12) | (((c[0] * 15 + 127) / 255) << 8) | (((c[1] * 15 + 127) / 255) << 4) | ((c[2] * 15 + 127) / 255);
		words[i * 2] = (uint8_t)v; words[i * 2 + 1] = (uint8_t)(v >> 8);
	}
}

bool IsImg(const uint8_t *p, size_t n)
{
	if (n < 20 || rd32(p) != 0) return false;
	uint32_t ver = rd32(p + 4);
	if (ver < 6 || ver > 8) return false;
	int bpp = BytesPerPixel(rd32(p + 8));
	uint64_t w = rd32(p + 12), h = rd32(p + 16);
	return bpp && w && h && 20 + w * h * (uint64_t)bpp == n;
}

bool ParseImg(const uint8_t *p, size_t n, ImgFile &out, std::string *err)
{
	if (!IsImg(p, n)) { if (err) *err = "not an IMG file (signature 0, version 6..8, format 0..3, exact size)"; return false; }
	out.version = rd32(p + 4); out.format = rd32(p + 8);
	out.width = (int)rd32(p + 12); out.height = (int)rd32(p + 16);
	const size_t px = (size_t)out.width * out.height;
	const uint8_t *s = p + 20;
	out.native.clear();
	if (out.format == 2) { out.rgba.assign(s, s + px * 4); return true; }
	out.native.assign(s, p + n);
	out.rgba.resize(px * 4);
	for (size_t i = 0; i < px; i++) {
		uint8_t *d = &out.rgba[i * 4];
		if (out.format == 3) { d[0] = s[i * 3]; d[1] = s[i * 3 + 1]; d[2] = s[i * 3 + 2]; d[3] = 255; continue; }
		unsigned v = s[i * 2] | (s[i * 2 + 1] << 8);
		if (out.format == 0) { d[0] = (uint8_t)(255 * ((v >> 10) & 31) / 31); d[1] = (uint8_t)(255 * ((v >> 5) & 31) / 31); d[2] = (uint8_t)(255 * (v & 31) / 31); d[3] = (v >> 15) ? 255 : 0; }
		else { d[0] = (uint8_t)(255 * ((v >> 8) & 15) / 15); d[1] = (uint8_t)(255 * ((v >> 4) & 15) / 15); d[2] = (uint8_t)(255 * (v & 15) / 15); d[3] = (uint8_t)(255 * (v >> 12) / 15); }
	}
	return true;
}

void SerializeImg(const ImgFile &img, std::vector<uint8_t> &out)
{
	// format 2 (and any image whose pixels were replaced: the viewer clears `native`) is written as RGBA8888; other formats go back verbatim
	const bool nat = img.format != 2 && !img.native.empty();
	out.assign(20, 0);
	uint32_t v = img.version, fmt = nat ? img.format : 2, w = (uint32_t)img.width, h = (uint32_t)img.height;
	memcpy(out.data() + 4, &v, 4); memcpy(out.data() + 8, &fmt, 4); memcpy(out.data() + 12, &w, 4); memcpy(out.data() + 16, &h, 4);
	const auto &src = nat ? img.native : img.rgba;
	out.insert(out.end(), src.begin(), src.end());
}

} // namespace han2
