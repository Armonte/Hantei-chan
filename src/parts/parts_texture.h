#ifndef PARTS_TEXTURE_H_GUARD
#define PARTS_TEXTURE_H_GUARD

#include <vector>
#include <functional>
#include <glm/mat4x4.hpp>
#include <string>
#include <fstream>
#include <cstdint>
#include "../misc.h"
#include "../texture.h"

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

template<template<typename> class Allocator = std::allocator>
class PartGfx {
public:
    // DDS (DirectDraw Surface) header structures
    struct DDS_PIXELFORMAT
    {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t RGBBitCount;
        uint32_t RBitMask;
        uint32_t GBitMask;
        uint32_t BBitMask;
        uint32_t ABitMask;
    };

    struct DDS_HEADER
    {
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };

    int id = 0;
    std::basic_string<char, std::char_traits<char>, Allocator<char>> name;

    // Texture data
    char* data = nullptr;           // Raw texture data (legacy)
    int w = 0, h = 0;               // Width, height
    int uvBpp[2]{};                 // UV coordinates (in multiples of 256)
    int bpp = 0;                    // Bits per pixel
    int type = 0;                   // Texture type (1=DXT1, 5=DXT5, 21=RGB)
    int textureIndex = 0;           // GL texture ID
    int pgte[2]{};                  // Unknown (stored as short[2])

    // DDS compression data
    char ddsHeader[124]{};          // DDS header (without "DDS " magic)
    unsigned char* s3tc = nullptr;  // Compressed S3TC data pointer
    char* imageData = nullptr;      // Decompressed/raw image data
    int imageSize = 0;              // Size of image data
    bool dontDelete = false;        // If true, don't delete s3tc (points to original file data)
    bool noCompress = false;        // If true, don't compress on save

    // Loading
    static unsigned int* PgLoad(unsigned int *data, const unsigned int *data_end, int id, std::vector<PartGfx<>>* gfxMeta);
    static PartGfx<>* GetTexture(unsigned int n, std::vector<PartGfx<>>* gfxMeta);

    // Texture management
    Texture* GetTextureFromId(std::vector<Texture*> textures);
    std::string ImportTexture(const char *filename, std::vector<Texture*> textures);
    void ExportTexture(const char *filename);

    // Compression/decompression
    static bool Decrappress(unsigned char* cdata, unsigned char* outData, size_t csize, size_t outSize);
    static void CompressDDS(std::ofstream &file, const PartGfx *gfx, std::streampos pgt2A);

    // Saving
    static void Save(std::ofstream &file, const PartGfx *gfx);
    static bool IsModifiedData(const PartGfx *gfx);

    // Utilities
    void CopyTo(PartGfx *gfx);
};

#endif // PARTS_TEXTURE_H_GUARD
