#include "parts_texture.h"
#include "../texture.h"
#include <glad/glad.h>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <iostream>

template<>
bool PartGfx<>::Decrappress(unsigned char* cdata, unsigned char* outData, size_t csize, size_t outSize)
{
    // Custom RLE-like decompression
    // Format: If byte is 0, next two bytes are: value, count
    // Otherwise: byte is literal value
    size_t wi = 0;
    for (size_t i = 0; i < csize; ++i)
    {
        if (wi > outSize)
            return false;

        if (cdata[i] == 0)
        {
            ++i;
            // cdata[i] = value, cdata[i+1] = count
            for (size_t j = 0; j < cdata[i + 1]; ++j)
                outData[wi++] = cdata[i];
            i += 1;
        }
        else
        {
            outData[wi++] = cdata[i];
        }
    }

    return true;
}

template<>
unsigned int* PartGfx<>::PgLoad(unsigned int *data, const unsigned int *data_end, int id, std::vector<PartGfx<>>* gfxMeta)
{
    PartGfx tex{};
    tex.id = id;

    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "PGNM", 4)) {
            // Melty name (null-terminated, 32 bytes)
            tex.name = (char*)data;
            data += 0x20 / 4;
        }
        else if (!memcmp(buf, "PGTP", 4)) {
            // Texture type hint (MBAACC, single uint32)
            tex.pgtp = (int)data[0];
            ++data;
        }
        else if (!memcmp(buf, "PGTE", 4)) {
            // Two shorts (unknown)
            short values[2];
            memcpy(values, data, sizeof(short) * 2);
            tex.pgte[0] = (int)values[0];
            tex.pgte[1] = (int)values[1];
            ++data;
        }
        else if (!memcmp(buf, "PGT2", 4)) {
            // UNI texture data (with compression)
            unsigned int someSize = data[0]; // Total size including header
            tex.w = data[1];
            tex.h = data[2];
            tex.uvBpp[0] = tex.w / 256;
            tex.uvBpp[1] = tex.h / 256;
            data += 3;

            // Detect texture type
            if (*data == 21 || *data == 22) {
                // D3DFORMAT: 21 A8R8G8B8, 22 X8R8G8B8 (MBTL talk/st_pat/chr012/chr012_ed00.pat; the 4th byte is not alpha)
                tex.type = (int)*data;
            }
            else if (!memcmp(data, "DXT1", 4)) {
                tex.type = 1; // DXT1 (BC1)
            }
            else if (!memcmp(data, "DXT5", 4)) {
                tex.type = 5; // DXT5 (BC3)
            }
            else {
                std::cout << "\tUnknown texture type in PGT2\n";
                assert(0 && "Unknown texture type");
            }

            data += 1;

            tex.noCompress = true;

            // Handle uncompressed DXT5
            if (tex.type == 5 && someSize == tex.w * tex.h + 128)
            {
                data += 2; // Skip unknown fields
                auto* oData = (unsigned char*)data;
                std::memcpy(tex.ddsHeader, oData + 4, 124);
                tex.s3tc = oData + 128;
                tex.dontDelete = true;
                tex.imageSize = someSize - 128;
                tex.imageData = new char[tex.imageSize];
                std::memcpy(tex.imageData, oData + 128, tex.imageSize);
                data += tex.w * tex.h / 4 + 0x20;
            }
            // Handle uncompressed DXT1
            else if (tex.type == 1 && someSize == tex.w * tex.h / 2 + 128)
            {
                data += 2; // Skip unknown fields
                auto* oData = (unsigned char*)data;
                std::memcpy(tex.ddsHeader, oData + 4, 124);
                tex.s3tc = oData + 128;
                tex.dontDelete = true;
                tex.imageSize = someSize - 128;
                tex.imageData = new char[tex.imageSize];
                std::memcpy(tex.imageData, oData + 128, tex.imageSize);
                data += (tex.w * tex.h / 2 + 128) / 4;
            }
            // Handle uncompressed RGB (type 21)
            else if (someSize == tex.w * tex.h * 4 + 128)
            {
                data += 2; // Skip unknown fields
                auto* oData = (unsigned char*)data;
                std::memcpy(tex.ddsHeader, oData + 4, 124);
                tex.s3tc = oData + 128;
                tex.dontDelete = true;
                tex.imageSize = someSize - 128;
                tex.imageData = new char[tex.imageSize];
                std::memcpy(tex.imageData, oData + 128, tex.imageSize);
                auto movepos = tex.w * tex.h + 0x20;
                data += movepos;
            }
            // Handle compressed data
            else
            {
                tex.noCompress = false;
                int cSize = data[4]; // Compressed size (from 'DDS ' to PGED)
                int oSize = data[5]; // Uncompressed size
                data += 6;

                unsigned char* cData = (unsigned char*)data;
                auto oData = new unsigned char[oSize];
                bool result = Decrappress(cData, oData, cSize, oSize);
                assert(result && "Decompression failed");

                char* header = new char[124];
                tex.imageSize = oSize - 128;
                tex.imageData = new char[tex.imageSize];
                std::copy(oData + 4, oData + 128, header);
                std::memcpy(tex.ddsHeader, header, 124);
                tex.s3tc = (unsigned char*)(oData + 128);
                std::copy(oData + 128, oData + tex.imageSize + 128, tex.imageData);

                delete[] header;

                cData += cSize;
                data = (unsigned int*)cData;
            }
        }
        else if (!memcmp(buf, "PGTX", 4)) {
            // Legacy texture data (MBAA)
            tex.w = data[0];
            tex.h = data[1];
            tex.bpp = data[2];
            tex.data = (char*)(data + 3);

            assert(tex.bpp == 32 && "Expected 32bpp texture");
            data += (tex.w * tex.h) + 3;
        }
        else if (!memcmp(buf, "PGED", 4)) {
            // Texture end
            (*gfxMeta)[id] = tex;
            break;
        }
        else {
            char tag[5]{};
            memcmp(tag, buf, 4);
            // Unknown tags are common, don't spam console
            // std::cout << "\tUnknown PG level tag: " << tag << "\n";
        }
    }

    return data;
}

PartGfx<>* GetTexture(unsigned int n, std::vector<PartGfx<>>* gfxMeta)
{
    if (gfxMeta->size() > n)
    {
        return &gfxMeta->at(n);
    }
    return nullptr;
}

template<>
Texture* PartGfx<>::GetTextureFromId(std::vector<Texture*> textures)
{
    for (auto tex : textures) {
        if (textureIndex == tex->id)
            return tex;
    }
    return nullptr;
}

namespace {

// One BC1 colour block (DXT1 / the colour half of DXT5) -> 16 BGRA pixels.
void DecodeBc1Colors(const uint8_t* blk, uint8_t out[16][4], bool dxt1Alpha)
{
    const uint16_t c0 = blk[0] | (blk[1] << 8), c1 = blk[2] | (blk[3] << 8);
    uint8_t pal[4][4];   // b, g, r, a
    auto expand = [](uint16_t c, uint8_t* o) {
        o[2] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        o[0] = (uint8_t)((c & 31) * 255 / 31);
        o[3] = 255;
    };
    expand(c0, pal[0]);
    expand(c1, pal[1]);
    for (int k = 0; k < 3; k++) {
        if (c0 > c1 || !dxt1Alpha) {
            pal[2][k] = (uint8_t)((2 * pal[0][k] + pal[1][k]) / 3);
            pal[3][k] = (uint8_t)((pal[0][k] + 2 * pal[1][k]) / 3);
        } else {
            pal[2][k] = (uint8_t)((pal[0][k] + pal[1][k]) / 2);
            pal[3][k] = 0;
        }
    }
    pal[2][3] = 255;
    pal[3][3] = (c0 > c1 || !dxt1Alpha) ? 255 : 0;
    const uint32_t idx = blk[4] | (blk[5] << 8) | (blk[6] << 16) | ((uint32_t)blk[7] << 24);
    for (int i = 0; i < 16; i++) memcpy(out[i], pal[(idx >> (2 * i)) & 3], 4);
}

// The alpha half of a DXT5 block.
void DecodeBc3Alpha(const uint8_t* blk, uint8_t out[16][4])
{
    uint8_t a[8];
    a[0] = blk[0];
    a[1] = blk[1];
    if (a[0] > a[1]) for (int i = 1; i < 7; i++) a[i + 1] = (uint8_t)(((7 - i) * a[0] + i * a[1]) / 7);
    else {
        for (int i = 1; i < 5; i++) a[i + 1] = (uint8_t)(((5 - i) * a[0] + i * a[1]) / 5);
        a[6] = 0;
        a[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; i++) bits |= (uint64_t)blk[2 + i] << (8 * i);
    for (int i = 0; i < 16; i++) out[i][3] = a[(bits >> (3 * i)) & 7];
}

int MaskShift(uint32_t m) { int s = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; s++; } return s; }

} // namespace

template<>
std::string PartGfx<>::DecodeDds(const char* file, size_t size, std::vector<uint8_t>& bgra, int& w, int& h)
{
    if (size < 128 || memcmp(file, "DDS ", 4) != 0) return "Not a DDS file";
    const uint32_t* d = (const uint32_t*)file;
    // DDS_HEADER: size, flags, HEIGHT, WIDTH, ... pixel format at +76 (d[19]): size, flags, fourCC, bit count, masks
    h = (int)d[3];
    w = (int)d[4];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return "The DDS size is not supported";
    const uint32_t pfFlags = d[20];
    const uint8_t* px = (const uint8_t*)file + 128;
    const size_t avail = size - 128;
    bgra.assign((size_t)w * h * 4, 0);
    if (pfFlags & 4) {   // DDPF_FOURCC
        const bool dxt1 = !memcmp(&d[21], "DXT1", 4), dxt5 = !memcmp(&d[21], "DXT5", 4);
        if (!dxt1 && !dxt5) return "DDS compression must be DXT1 or DXT5 (or uncompressed 32-bit)";
        const int bw = (w + 3) / 4, bh = (h + 3) / 4, bsz = dxt1 ? 8 : 16;
        if (avail < (size_t)bw * bh * bsz) return "The DDS file is truncated";
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                const uint8_t* blk = px + ((size_t)by * bw + bx) * bsz;
                uint8_t out[16][4];
                DecodeBc1Colors(dxt5 ? blk + 8 : blk, out, dxt1);
                if (dxt5) DecodeBc3Alpha(blk, out);
                for (int i = 0; i < 16; i++) {
                    const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if (x < w && y < h) memcpy(&bgra[((size_t)y * w + x) * 4], out[i], 4);
                }
            }
        return "";
    }
    if (d[22] != 32) return "Uncompressed DDS must be 32-bit";
    if (avail < (size_t)w * h * 4) return "The DDS file is truncated";
    const uint32_t rm = d[23], gm = d[24], bm = d[25], am = (pfFlags & 1) ? d[26] : 0;
    const int rs = MaskShift(rm), gs = MaskShift(gm), bs = MaskShift(bm), as = MaskShift(am);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        uint32_t v;
        memcpy(&v, px + i * 4, 4);
        bgra[i * 4 + 0] = (uint8_t)((v & bm) >> bs);
        bgra[i * 4 + 1] = (uint8_t)((v & gm) >> gs);
        bgra[i * 4 + 2] = (uint8_t)((v & rm) >> rs);
        bgra[i * 4 + 3] = am ? (uint8_t)((v & am) >> as) : 255;
    }
    return "";
}

template<>
std::string PartGfx<>::ImportTexture(const char *filename, std::vector<Texture*>& textures, bool mbaaccFormat)
{
    std::string path = filename;
    char* data;
    unsigned int size;

    if (!ReadInMem(filename, data, size))
        return "Unable to read data in memory";
    if (size < 128 || memcmp(data, "DDS ", 4) != 0)
    {
        delete[] data;
        return "Not a DDS file";
    }

    unsigned int* d = (unsigned int*)(data);
    // DDS_HEADER order is height, then width (d[3], d[4]); the old code read them the other way round, which only
    // worked for square textures.
    const int ddsW = (int)d[4], ddsH = (int)d[3];

    if (mbaaccFormat)
    {
        // MBAA.exe reads PGTX only (32-bit BGRA rows). Decode whatever the DDS holds into that.
        std::vector<uint8_t> px;
        int dw = 0, dh = 0;
        std::string err = DecodeDds(data, size, px, dw, dh);
        delete[] data;
        if (!err.empty()) return err;
        if (name.empty())
            name = path.substr(path.find_last_of("/\\") + 1);
        w = dw;
        h = dh;
        bpp = 32;
        uvBpp[0] = w / 256;
        uvBpp[1] = h / 256;
        // Never freed: an undo snapshot may still point at the previous pixels, and this buffer may be the next one.
        char* pixels = new char[px.size()];
        memcpy(pixels, px.data(), px.size());
        this->data = pixels;
        type = 0;
        s3tc = nullptr;
        imageData = nullptr;
        imageSize = 0;
        memset(ddsHeader, 0, sizeof ddsHeader);
#ifndef PAT_HEADLESS
        Texture* texture = GetTextureFromId(textures);
        if (!texture) {
            texture = new Texture;
            textures.push_back(texture);
        }
        texture->LoadDirect(this->data, w, h, true);
        texture->Apply(false, false);
        textureIndex = texture->id;
#endif
        return "";
    }

    // UNI / MBTL (PGT2): sizes must be a multiple of 256 (UV is in 1/256 of the texture)
    if (ddsW % 256 != 0 || ddsW <= 0)
    {
        delete[] data;
        return "Width of the texture to import is not divisible by 256";
    }
    if (ddsH % 256 != 0 || ddsH <= 0)
    {
        delete[] data;
        return "Height of the texture to import is not divisible by 256";
    }

    // Set name from filename if empty
    if (name.empty())
        name = path.substr(path.find_last_of("/\\") + 1);

    // Set dimensions
    w = ddsW;
    h = ddsH;
    uvBpp[0] = w / 256;
    uvBpp[1] = h / 256;
    pgte[0] = w;
    pgte[1] = h;

    // Detect texture type from DDS header
    auto typeBuf = &d[21];
    if (d[21] == 0) {
        type = (d[20] & 0x40) && !(d[20] & 0x1) && d[26] == 0 ? 22 : 21; // RGB: no alpha bit and no alpha mask = X8R8G8B8
    }
    else if (!memcmp(typeBuf, "DXT1", 4)) {
        type = 1; // DXT1
    }
    else if (!memcmp(typeBuf, "DXT5", 4)) {
        type = 5; // DXT5
    }
    else {
        delete[] data;
        return "DDS compression must be DXT1 or DXT5 (or uncompressed)";
    }

    // Copy DDS header
    std::memcpy(ddsHeader, data + 4, 124);

    // Copy image data
    s3tc = (unsigned char*)(data + 128);
    imageSize = size - 128;
    imageData = new char[imageSize];
    std::copy(data + 128, data + 128 + imageSize, imageData);
    this->data = nullptr;   // a PGT2 texture (a stale PGTX pointer would make Save write PGTX)

    // Load into OpenGL texture (skipped in headless / no-GL builds).
#ifndef PAT_HEADLESS
    Texture* texture = GetTextureFromId(textures);
    if (!texture) {
        texture = new Texture;
        textures.push_back(texture);
    }

    if (type == 21 || type == 22)   // 22 = X8R8G8B8: uploaded like 21; the alpha byte is ignored when drawn
    {
        // Uncompressed RGB/BGRA
        texture->LoadDirect((char*)s3tc, w, h, true);  // BGRA format
        // Default to GL_NEAREST; filtering will be applied per-part based on PRFL flag
        texture->Apply(false, false);  // repeat=false, linearFilter=false
    }
    else
    {
        // Compressed (DXT1 or DXT5)
        size_t compressedSize = type == 5 ? (size_t)w * h : (size_t)w * h / 2;
        texture->LoadCompressed((char*)s3tc, w, h, compressedSize, type);
        // LoadCompressed creates texture internally, set default filtering
        // Actual filtering will be applied per-part based on PRFL flag in DrawPart()
        glBindTexture(GL_TEXTURE_2D, texture->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    textureIndex = texture->id;
#endif

    return ""; // Success
}

template<>
std::string PartGfx<>::ExportTexture(const char *filename) const
{
    std::string out;
    if (data != nullptr && imageSize == 0 && w > 0 && h > 0)
    {
        // MBAACC PGTX: 32-bit BGRA rows -> an uncompressed A8R8G8B8 DDS (sosfiro wrote an empty header and no pixels).
        DDS_HEADER hd{};
        hd.size = 124;
        hd.flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;   // CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
        hd.height = (uint32_t)h;
        hd.width = (uint32_t)w;
        hd.pitchOrLinearSize = (uint32_t)w * 4;
        hd.ddspf.size = 32;
        hd.ddspf.flags = 0x40 | 0x1;                  // RGB | ALPHAPIXELS
        hd.ddspf.RGBBitCount = 32;
        hd.ddspf.RBitMask = 0x00FF0000;
        hd.ddspf.GBitMask = 0x0000FF00;
        hd.ddspf.BBitMask = 0x000000FF;
        hd.ddspf.ABitMask = 0xFF000000;
        hd.caps = 0x1000;                             // TEXTURE
        out.append("DDS ", 4);
        out.append((const char*)&hd, sizeof hd);
        out.append(data, (size_t)w * h * 4);
    }
    else if (imageData != nullptr && imageSize > 0)
    {
        out.append("DDS ", 4);
        out.append(ddsHeader, 124);
        out.append(imageData, imageSize);
    }
    else
        return "This texture has no image data";
    if (!WriteFileAtomic(filename, out.data(), out.size()))
        return std::string("Could not write ") + filename;
    return "";
}

template<>
void PartGfx<>::CompressDDS(std::ostream &file, const PartGfx *gfx, std::streampos pgt2A)
{
    // Custom RLE-like compression
    // Format: 0x00 <value> <count> for runs, or literal bytes
    bool compressionActive = false;
    char fixedVals[2] = {static_cast<char>(0), static_cast<char>(255)};
    int count = 1;
    int compressSize = 4;
    char currentVal;
    char nextVal;

    file.write("DDS\x20", 4);

    for (int i = 0; i < gfx->imageSize + 124; ++i)
    {
        if (!compressionActive) {
            currentVal = i < 124 ? gfx->ddsHeader[i] : gfx->imageData[i - 124];
            compressionActive = true;
        }
        else {
            nextVal = i < 124 ? gfx->ddsHeader[i] : gfx->imageData[i - 124];

            // Increase count if same value
            if (currentVal == nextVal)
            {
                count += 1;
                if (count > 255)
                {
                    // Max run length reached, write it
                    file.write(VAL(fixedVals[0]), 1);
                    file.write(VAL(currentVal), 1);
                    file.write(VAL(fixedVals[1]), 1);
                    --i;
                    compressionActive = false;
                    compressSize += 3;
                    count = 1;
                }
            }
            // Write repetition
            else if (count >= 4 || currentVal == 0)
            {
                file.write(VAL(fixedVals[0]), 1);
                file.write(VAL(currentVal), 1);
                file.write(VAL(count), 1);
                --i;
                compressionActive = false;
                compressSize += 3;
                count = 1;
            }
            // Write values as-is (not worth compressing)
            else
            {
                for (int c = 0; c < count; ++c)
                {
                    file.write(VAL(currentVal), 1);
                }
                --i;
                compressionActive = false;
                compressSize += count;
                count = 1;
            }
        }
    }

    // Write remaining data
    if (currentVal == 0 || count >= 4)
    {
        file.write(VAL(fixedVals[0]), 1);
        file.write(VAL(currentVal), 1);
        file.write(VAL(count), 1);
        compressSize += 3;
    }
    else
    {
        for (int c = 0; c < count; ++c)
        {
            file.write(VAL(currentVal), 1);
        }
        compressSize += count;
    }

    // Go back and write the sizes in PGT2 header
    auto currentPos = file.tellp();
    file.seekp(pgt2A);
    int otherCompress = compressSize + 16;
    file.write(VAL(otherCompress), 4);
    file.seekp(7 * 4, std::ios_base::cur);
    file.write(VAL(compressSize), 4);
    file.seekp(currentPos);
}

template<>
void PartGfx<>::Save(std::ostream &file, const PartGfx *gfx)
{
    std::streampos pointer;

    // Save name if present (PGNM is shared by MBAACC and UNI formats)
    if (!gfx->name.empty()) {
        file.write("PGNM", 4);
        char buf[32]{};
        std::string name = utf82sj(gfx->name);
        strncpy(buf, name.c_str(), 32);
        buf[31] = 0;
        file.write(PTR(buf), 32);
    }

    // MBAACC PGTX path: legacy uncompressed BGRA texture. Detected by data
    // pointer being set (UNI PGT2 path sets imageSize/imageData instead).
    if (gfx->data != nullptr && gfx->imageSize == 0)
    {
        if (gfx->pgtp != 0) {
            file.write("PGTP", 4);
            file.write(VAL(gfx->pgtp), 4);
        }
        file.write("PGTX", 4);
        file.write(VAL(gfx->w), 4);
        file.write(VAL(gfx->h), 4);
        file.write(VAL(gfx->bpp), 4);
        file.write(gfx->data, gfx->w * gfx->h * (gfx->bpp / 8));
        return;
    }

    // UNI PGT2 path.
    file.write("PGTE", 4);
    file.write(VAL(gfx->pgte[0]), 2);
    file.write(VAL(gfx->pgte[1]), 2);

    int typeName = gfx->type == 22 ? 22 : 21;
    if (gfx->type == 1)
        typeName = 827611204; // "DXT1" as int32
    else if (gfx->type == 5)
        typeName = 894720068; // "DXT5" as int32

    file.write("PGT2", 4);
    int totalSize = gfx->imageSize;

    if (!gfx->noCompress)
    {
        totalSize += 128;
        pointer = file.tellp();
        int pgt2[9]{0,
                    gfx->w,
                    gfx->h,
                    typeName,
                    4, 3, 17828863,
                    0, 0};
        file.write(PTR(&pgt2), 9 * 4);
        file.write(VAL(totalSize), 4);
        PartGfx::CompressDDS(file, gfx, pointer);
    }
    else
    {
        totalSize += 128;
        file.write(VAL(totalSize), 4);
        int pgt2[5]{gfx->w,
                    gfx->h,
                    typeName,
                    4, 3};
        file.write(PTR(&pgt2), 5 * 4);
        file.write("DDS\x20", 4);
        file.write(PTR(gfx->ddsHeader), 124);
        file.write(PTR(gfx->imageData), gfx->imageSize);
    }
}

template<>
bool PartGfx<>::IsModifiedData(const PartGfx *gfx)
{
    if (gfx->name.empty())
        return false;
    // UNI PGT2 data (compressed/DDS) — has imageSize set
    if (gfx->imageSize > 0)
        return true;
    // MBAACC PGTX data — has data pointer + dimensions
    if (gfx->data != nullptr && gfx->w > 0 && gfx->h > 0)
        return true;
    return false;
}

template<>
void PartGfx<>::CopyTo(PartGfx *gfx)
{
    gfx->name = name;
    gfx->w = w;
    gfx->h = h;
    gfx->type = type;
    gfx->bpp = bpp;
    gfx->pgte[0] = pgte[0];
    gfx->pgte[1] = pgte[1];
    gfx->s3tc = s3tc;
    gfx->noCompress = noCompress;
    gfx->textureIndex = textureIndex;
    memcpy(gfx->ddsHeader, ddsHeader, 124);
    gfx->imageSize = imageSize;
    gfx->imageData = imageData;
    gfx->data = data;
    gfx->dontDelete = dontDelete;
}
