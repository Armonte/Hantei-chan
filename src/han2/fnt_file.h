#ifndef HAN2_FNT_FILE_H_GUARD
#define HAN2_FNT_FILE_H_GUARD

// RBO bitmap font (.FNT, Data\Font\*.fnt). Loader sub_43A500 in rbo_ex3.exe (FontBank_LoadFile: first u16 != 0 -> the file is the body; first u16 == 0 ->
// u16 0 + u32 halfOffset prefix, then the body), glyph lookup sub_410D80 / sub_410E30, header parse sub_410CF0 (all renamed in rbo_ex3.exe.i64):
//   [prefix: u16 0, u32 halfOffset]   body-relative offset of the half-width section (== 65032 + glyph bytes)
//   body: u16 nGlyphs | u16 width | u16 height | u8 pixelsPerByte | u8 bitsPerPixel | u16 index[32512] | glyph[nGlyphs][height][rowBytes]
//         | (half section) u16 halfWidth | u16 halfHeight | halfGlyph[256][halfHeight][halfRowBytes]
//   rowBytes = ceil(width / pixelsPerByte). index[] is addressed by the Shift-JIS lead/trail word minus 0x8100 (0xFFFF = no glyph).
#include <cstdint>
#include <string>
#include <vector>

namespace han2 { namespace fnt {

constexpr size_t kIndexCount = 32512;      // RBO: (65032 - 8) / 2, glyph data at body +65032
constexpr size_t kIndexCountGof1 = 32511;  // GOF1 font.fnt (gof.exe sub_42C5E0 uses body +65030)

struct Font {
	bool hasPrefix = false; uint32_t halfOffset = 0;
	uint16_t nGlyphs = 0, width = 0, height = 0; uint8_t pixelsPerByte = 0, bitsPerPixel = 0;
	std::vector<uint16_t> index;                  // kIndexCount (RBO) or kIndexCountGof1
	std::vector<uint8_t> glyphs;                  // nGlyphs * height * rowBytes
	bool hasHalf = false; uint16_t halfWidth = 0, halfHeight = 0;
	std::vector<uint8_t> half;                    // 256 * halfHeight * halfRowBytes
	std::vector<uint8_t> tail;                    // anything after the sections (none expected)
	size_t RowBytes() const { return pixelsPerByte ? (width + pixelsPerByte - 1) / pixelsPerByte : 0; }
};

bool Parse(const uint8_t *p, size_t n, Font &out, std::string *err);
bool Serialize(const Font &f, std::vector<uint8_t> &out, std::string *err);

}} // namespace
#endif
