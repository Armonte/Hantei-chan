#ifndef PAT_TEXCONV_H_GUARD
#define PAT_TEXCONV_H_GUARD
// .pat part textures <-> RGBA / PNG, without GL (the PAT editor UI and the CLI both call it).
// Texture kinds (PartGfx::type):
//   1  DXT1 (BC1)               5  DXT5 (BC3)
//   21 D3DFMT_A8R8G8B8 (BGRA)   22 D3DFMT_X8R8G8B8 (BGRX: the 4th byte is not alpha, drawn opaque; MBTL talk st_pat chr012_ed00)
//   MBAACC PGTX: 32-bit BGRA in PartGfx::data (imageSize 0).
// UNI2 / MBTL (PGT2) texture sides must be multiples of 128 (shipped: 128 .. 2048; the cut-out UVs are in 1/256 units per side).
#include "parts_texture.h"
#include <cstdint>
#include <string>
#include <vector>

namespace pattex {

bool IsRawBgra(int type);   // 21 or 22
// The texture as straight RGBA (memory R,G,B,A).
bool Decode(const PartGfx<> &g, std::vector<uint8_t> &rgba, int &w, int &h, std::string *err);
// Replaces the texture's pixels. type: -1 = keep the texture's own kind (an MBAACC PGTX stays PGTX), or 1 / 5 / 21 / 22.
// DXT kinds are encoded here (box-fit endpoints per 4x4 block); 21 / 22 are lossless. The texture becomes self-owned
// (imageData), is saved RLE-packed like the shipped files, and its DDS header is rebuilt when the kind or size changes.
bool Encode(PartGfx<> &g, const uint8_t *rgba, int w, int h, int type, std::string *err);
bool CheckSize(const PartGfx<> &g, int w, int h, std::string *err);

bool ExportPng(const PartGfx<> &g, const std::string &utf8Path, std::string *err);
bool ImportPng(PartGfx<> &g, const std::string &utf8Path, int type, std::string *err);

// BC1 / BC3 codecs (4x4 blocks, w and h multiples of 4)
void DecodeDxt(const uint8_t *src, int w, int h, bool dxt5, uint8_t *rgba);
void EncodeDxt(const uint8_t *rgba, int w, int h, bool dxt5, std::vector<uint8_t> &out);

} // namespace pattex
#endif
