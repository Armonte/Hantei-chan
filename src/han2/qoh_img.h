#ifndef HAN2_QOH_IMG_H_GUARD
#define HAN2_QOH_IMG_H_GUARD

// Queen of Heart '99 .Img (docs/formats/qoh99.md section 7): u32 reserved | u32 paletteEntries | u32 bpp (4, 8, 24) | u32 width | u32 height | palette[4 * entries] (B, G, R, 0) |
// pixel rows padded to 4 bytes (8 bpp: width, 4 bpp: width / 2 two pixels per byte, 24 bpp: 3 * width, B G R). No compression: the stored pixel bytes are kept verbatim.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

struct QohImg {
	uint32_t reserved = 0, paletteEntries = 0, bpp = 0, width = 0, height = 0;
	std::vector<uint8_t> palette;   // 4 * paletteEntries
	std::vector<uint8_t> pixels;    // stored rows
	std::vector<uint8_t> rgba;      // decoded for display (width * height * 4), bottom-up rows flipped to top-down
	size_t rowBytes() const { return bpp == 24 ? (3ull * width + 3) & ~3ull : bpp == 8 ? ((uint64_t)width + 3) & ~3ull : (((uint64_t)width + 1) / 2 + 3) & ~3ull; }
};
// Replaces the pixels with `rgba` (top-down, exactly width x height): 24 bpp stores B G R, palettised images take the nearest palette entry (transparent pixels = entry 0 when the image has a black/colour-key entry, else the nearest). The decoded `rgba` is refreshed.
bool QohImgSetRgba(QohImg &img, const uint8_t *rgba, int w, int h, std::string *err);
bool ParseQohImg(const uint8_t *p, size_t n, QohImg &out, std::string *err);
void SerializeQohImg(const QohImg &img, std::vector<uint8_t> &out);   // from the stored fields (pixels), never from rgba

} // namespace han2
#endif
