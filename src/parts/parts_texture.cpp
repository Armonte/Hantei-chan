#include "parts_texture.h"
#include "../texture.h"
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
            // Texture type (optional tag, not always used)
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
            if (*data == 21) {
                tex.type = 21; // Uncompressed RGB
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

template<>
std::string PartGfx<>::ImportTexture(const char *filename, std::vector<Texture*> textures)
{
    std::string path = filename;
    char* data;
    unsigned int size;

    if (!ReadInMem(filename, data, size))
        return "Unable to read data in memory";

    unsigned int* d = (unsigned int*)(data);

    // Validate dimensions (must be divisible by 256)
    if (d[3] % 256 != 0 || d[3] <= 0)
    {
        delete[] data;
        return "Width of the texture to import is not divisible by 256";
    }
    if (d[4] % 256 != 0 || d[4] <= 0)
    {
        delete[] data;
        return "Height of the texture to import is not divisible by 256";
    }

    // Set name from filename if empty
    if (name.empty())
        name = path.substr(path.find_last_of("/\\") + 1);

    // Set dimensions
    w = d[3];
    h = d[4];
    uvBpp[0] = w / 256;
    uvBpp[1] = h / 256;
    pgte[0] = w;
    pgte[1] = h;

    // Detect texture type from DDS header
    auto typeBuf = &d[21];
    if (d[21] == 0) {
        type = 21; // RGB
    }
    else if (!memcmp(typeBuf, "DXT1", 4)) {
        type = 1; // DXT1
    }
    else if (!memcmp(typeBuf, "DXT5", 4)) {
        type = 5; // DXT5
    }

    // Copy DDS header
    char* header = new char[124];
    std::copy(data + 4, data + 128, header);
    std::memcpy(ddsHeader, header, 124);
    delete[] header;

    // Copy image data
    data += 128;
    s3tc = (unsigned char*)data;
    imageSize = size - 128;
    imageData = new char[imageSize];
    std::copy(data, data + imageSize, imageData);

    // Load into OpenGL texture
    auto texture = GetTextureFromId(textures);
    if (!texture) {
        texture = new Texture;
        textures.push_back(texture);
    }

    if (s3tc)
    {
        if (type == 21)
        {
            // Uncompressed RGB
            textures.back()->LoadDirect((char*)s3tc, w, h);
        }
        else
        {
            // Compressed (DXT1 or DXT5)
            size_t compressedSize;
            if (type == 5)
                compressedSize = w * h; // DXT5
            else if (type == 1)
                compressedSize = (w * h) * 3 / 6; // DXT1
            else
                assert(0 && "Unknown compression type");

            textures.back()->LoadCompressed((char*)s3tc, w, h, compressedSize, type);
        }
    }
    else
    {
        textures.back()->LoadDirect(data, w, h);
    }

    textureIndex = textures.back()->id;

    return ""; // Success
}

template<>
void PartGfx<>::ExportTexture(const char *filename)
{
    std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
    if (!file.is_open())
        return;

    // Write DDS file: magic + header + image data
    file.write("DDS\x20", 4);
    file.write(PTR(ddsHeader), 124);
    file.write(imageData, imageSize);

    file.close();
}

template<>
void PartGfx<>::CompressDDS(std::ofstream &file, const PartGfx *gfx, std::streampos pgt2A)
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
void PartGfx<>::Save(std::ofstream &file, const PartGfx *gfx)
{
    std::streampos pointer;

    // Save name if present
    if (!gfx->name.empty()) {
        file.write("PGNM", 4);
        char buf[32]{};
        std::string name = utf82sj(gfx->name);
        strncpy(buf, name.c_str(), 32);
        buf[31] = 0;
        file.write(PTR(buf), 32);
    }

    // Save PGTE (dimensions)
    file.write("PGTE", 4);
    file.write(VAL(gfx->pgte[0]), 2);
    file.write(VAL(gfx->pgte[1]), 2);

    // Determine type name
    int typeName = 21;
    if (gfx->type == 1)
        typeName = 827611204; // "DXT1" as int32
    else if (gfx->type == 5)
        typeName = 894720068; // "DXT5" as int32

    // Save PGT2 (texture data)
    file.write("PGT2", 4);
    int totalSize = gfx->imageSize;

    if (!gfx->noCompress)
    {
        // Save with compression
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
        // CompressDDS writes both necessary sizes in PGT2
        PartGfx::CompressDDS(file, gfx, pointer);
    }
    else
    {
        // Save without compression
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
    if (!gfx->name.empty() && gfx->imageSize > 0)
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
