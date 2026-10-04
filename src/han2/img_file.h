#ifndef HAN2_IMG_FILE_H_GUARD
#define HAN2_IMG_FILE_H_GUARD

// French-Bread .IMG: u32 0 | u32 version (7 in all 88 RBO files, 6..8 accepted by the engine) | u32 format | u32 width | u32 height | pixels.
// format (GOF2.exe lib::CCGImageRead::ReadImgStream_fmt0to3 0x4024A0 / GetPixelRGBA 0x402580): 0 = ARGB1555 (2 B), 1 = ARGB4444 (2 B),
// 2 = R,G,B,A straight alpha (4 B; PACNyx swaps bytes 0 and 2 because .NET Format32bppArgb is B,G,R,A), 3 = R,G,B (3 B, alpha 255).
// Shipped: format 2 in every RBO file and most GOF2 files; format 3 in version 8 files (GOF2 data05: ACED_11, ACOP_11, PRE_BG00, PRE_BG13).
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

struct ImgFile {
	uint32_t version = 7;
	uint32_t format = 2;         // see above
	int width = 0, height = 0;
	std::vector<uint8_t> rgba;   // width*height*4, decoded for display / export (the pixels themselves when format == 2)
	std::vector<uint8_t> native; // the file's own pixel bytes when format != 2 (written back verbatim: formats 0/1 are lossy to RGBA)
};

// 16-bit pixel words as the game's callers interpret them (D3DFMT_A1R5G5B5 = 0, D3DFMT_A4R4G4B4 = 1). dMp IMG v6 files do not name the format (the caller decides),
// RBO/GOF2 IMG name it in the header. Encode is the exact inverse of Decode for every 16-bit word (so an untouched sheet re-encodes bit for bit).
void DecodePixels16(const uint8_t *words, size_t pixels, int fmt16, std::vector<uint8_t> &rgba);
void EncodePixels16(const uint8_t *rgba, size_t pixels, int fmt16, std::vector<uint8_t> &words);

bool IsImg(const uint8_t *p, size_t n);
bool ParseImg(const uint8_t *p, size_t n, ImgFile &out, std::string *err);
void SerializeImg(const ImgFile &img, std::vector<uint8_t> &out);

} // namespace han2

#endif
