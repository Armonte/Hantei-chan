#include "img_file.h"
#include <cstring>

namespace han2 {

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

bool IsImg(const uint8_t *p, size_t n)
{
	if (n < 20 || rd32(p) != 0) return false;
	uint32_t ver = rd32(p + 4);
	if (ver != 6 && ver != 7) return false;
	uint64_t w = rd32(p + 12), h = rd32(p + 16), bpp = (uint64_t)rd32(p + 8) * 2;
	return bpp == 4 && w && h && 20 + w * h * bpp == n;
}

bool ParseImg(const uint8_t *p, size_t n, ImgFile &out, std::string *err)
{
	if (!IsImg(p, n)) { if (err) *err = "not an IMG file (signature 0, version 6/7, 32 bits per pixel, exact size)"; return false; }
	out.version = rd32(p + 4);
	out.width = (int)rd32(p + 12); out.height = (int)rd32(p + 16);
	out.rgba.assign(p + 20, p + n);
	return true;
}

void SerializeImg(const ImgFile &img, std::vector<uint8_t> &out)
{
	out.assign(20, 0);
	uint32_t v = img.version, bpp = 2, w = (uint32_t)img.width, h = (uint32_t)img.height;
	memcpy(out.data() + 4, &v, 4); memcpy(out.data() + 8, &bpp, 4); memcpy(out.data() + 12, &w, 4); memcpy(out.data() + 16, &h, 4);
	out.insert(out.end(), img.rgba.begin(), img.rgba.end());
}

} // namespace han2
