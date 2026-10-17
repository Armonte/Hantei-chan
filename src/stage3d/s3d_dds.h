#ifndef S3D_DDS_H_GUARD
#define S3D_DDS_H_GUARD
// Minimal DDS reader for French-Bread 3D stage textures: DXT1 / DXT3 / DXT5 and uncompressed 32/24-bit RGB(A), top mip
// only (the stages ship without mips). Decodes to straight-alpha RGBA8, top-down rows (D3D UV origin = first row).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

struct Image {
	int width = 0, height = 0;
	std::vector<uint8_t> rgba;
	std::string format;   // "DXT1", "DXT5", "A8R8G8B8", ...
};

bool DecodeDds(const uint8_t* data, size_t size, Image& out, std::string& err);
bool LoadDdsFile(const std::string& utf8Path, Image& out, std::string& err);

}  // namespace s3d
#endif
