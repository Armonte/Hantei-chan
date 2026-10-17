#ifndef HAN2_ROSA_IMG_H_GUARD
#define HAN2_ROSA_IMG_H_GUARD

// Rosa Chinensis Four hand .IMG (docs/formats/rosa.md section 2). Version 4 (2002 PAC): 44-byte header, every field / the palette / the pixels enciphered by
// Img_DecryptStream with the key 8C 4B 92 4A 20 89 C2 97 F7 and the upper-cased file stem; version 1 (2005 PAC): 28-byte plain header. No compression: the stored pixel
// bytes are kept verbatim, so an unedited image re-encodes bit for bit; `Encode` can also rebuild the 2002 form from the 2005 one.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

struct RosaImg {
	uint32_t version = 4;                 // 4 (enciphered, 44-byte header) or 1 (plain, 28-byte header)
	uint8_t nameSeed[16]{};               // version 4 only: stored name block (stem repeated to 16 bytes XOR key), kept verbatim
	uint32_t flag = 0, paletteCount = 0, bpp = 0, width = 0, height = 0;
	std::vector<uint8_t> palette;         // 4 * paletteCount, B G R 0
	std::vector<uint8_t> pixels;          // stored rows, top-down
	std::vector<uint8_t> rgba;            // decoded: black (R=G=B=0) is transparent, as the game's colour key
	size_t rowBytes() const;
};

// `stem`: upper-cased file name without extension. Version 4 verifies that the decoded name block starts with the stem.
bool ParseRosaImg(const uint8_t *p, size_t n, const std::string &stem, RosaImg &out, std::string *err);
void SerializeRosaImg(const RosaImg &img, const std::string &stem, std::vector<uint8_t> &out);
std::string RosaStemOfName(const std::string &fileName);

} // namespace han2
#endif
