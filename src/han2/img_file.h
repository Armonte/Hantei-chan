#ifndef HAN2_IMG_FILE_H_GUARD
#define HAN2_IMG_FILE_H_GUARD

// French-Bread .IMG: u32 0 | u32 version (7 in all 88 RBO files; PACNyx also accepts 6) | u32 bytesPerPixel/2 (2) | u32 width |
// u32 height | width*height*4 bytes, R,G,B,A straight alpha (PACNyx swaps bytes 0 and 2 because .NET Format32bppArgb is B,G,R,A).
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

struct ImgFile {
	uint32_t version = 7;
	int width = 0, height = 0;
	std::vector<uint8_t> rgba;   // width*height*4
};

bool IsImg(const uint8_t *p, size_t n);
bool ParseImg(const uint8_t *p, size_t n, ImgFile &out, std::string *err);
void SerializeImg(const ImgFile &img, std::vector<uint8_t> &out);

} // namespace han2

#endif
