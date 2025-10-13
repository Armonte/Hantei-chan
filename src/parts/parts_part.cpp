#include "parts_part.h"
#include <cstring>
#include <fstream>

template<>
unsigned int* CutOut<>::PpLoad(unsigned int* data, const unsigned int* data_end, int id, std::vector<CutOut<>>* cutOuts)
{
    CutOut pp{};
    pp.id = id;
    std::string name;

    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "PPNM", 4)) {
            // Melty name (null-terminated, 32 bytes)
            pp.name = (char*)data;
            data += 0x20 / 4;
        }
        else if (!memcmp(buf, "PPNA", 4)) {
            // UNI name (length-prefixed Shift-JIS)
            unsigned char* cdata = (unsigned char*)data;
            name.resize(*cdata);
            memcpy(name.data(), (cdata + 1), *cdata);
            cdata += *cdata + 1;
            data = (unsigned int*)cdata;
            name = sj2utf8(name);
            pp.name = name;
        }
        else if (!memcmp(buf, "PPXY", 4)) {
            // Origin X, Y
            memcpy(pp.xy, data, sizeof(int) * 2);
            data += 2;
        }
        else if (!memcmp(buf, "PPCC", 4)) {
            // Alternative XY coords (Transform coordinates, inverted for vertex coords)
            memcpy(pp.xy, data, sizeof(int) * 2);
            data += 2;
        }
        else if (!memcmp(buf, "PPUV", 4)) {
            // Texture coords (LEFT, TOP, W, H)
            memcpy(pp.uv, data, sizeof(int) * 4);
            data += 4;
        }
        else if (!memcmp(buf, "PPWH", 4)) {
            // Width and Height
            memcpy(pp.wh, data, sizeof(int) * 2);
            data += 2;
        }
        else if (!memcmp(buf, "PPSS", 4)) {
            // Alternative W and H
            memcpy(pp.wh, data, sizeof(int) * 2);
            data += 2;
        }
        else if (!memcmp(buf, "PPTE", 4)) {
            // Two shorts (unknown)
            short values[2];
            memcpy(values, data, sizeof(short) * 2);
            pp.ppte[0] = (int)values[0];
            pp.ppte[1] = (int)values[1];
            ++data;
        }
        else if (!memcmp(buf, "PPCL", 4)) {
            // Color slot (palette index)
            pp.colorSlot = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPPA", 4)) {
            // Alternative color slot tag
            pp.colorSlot = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPGR", 4)) {
            // Texture reference ID (graphic)
            pp.texture = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPTP", 4)) {
            // Alternative texture reference tag
            pp.texture = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPVT", 4)) {
            // Shape index (vertex type)
            pp.shapeIndex = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPPP", 4)) {
            // Alternative shape index tag
            pp.shapeIndex = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPTX", 4)) {
            // Unknown (doesn't seem to affect rendering)
            pp.pptx = *data;
            ++data;
        }
        else if (!memcmp(buf, "PPJP", 4)) {
            // Jump offset (doesn't affect rendering)
            memcpy(pp.ppjp, data, sizeof(int) * 2);
            data += 2;
        }
        else if (!memcmp(buf, "PPED", 4)) {
            // CutOut end
            (*cutOuts)[id] = pp;
            break;
        }
        else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cout << "\tUnknown PP level tag: " << tag << "\n";
        }
    }

    return data;
}

template<>
void CutOut<>::Save(std::ofstream &file, const CutOut *cutOut)
{
    if(!cutOut->name.empty()) {
        file.write("PPNA", 4);
        std::string name = utf82sj(cutOut->name);
        uint32_t size = name.size();
        file.write(VAL(size), 1);
        file.write(PTR(name.data()), size);
    }

    if(cutOut->xy[0] != 0 || cutOut->xy[1] != 0)
    {
        file.write("PPXY", 4);
        file.write(VAL(cutOut->xy[0]), 4);
        file.write(VAL(cutOut->xy[1]), 4);
    }

    if(cutOut->uv[0] != 0 ||
            cutOut->uv[1] != 0 ||
            cutOut->uv[2] != 0 ||
            cutOut->uv[3] != 0)
    {
        file.write("PPUV", 4);
        file.write(VAL(cutOut->uv[0]), 4);
        file.write(VAL(cutOut->uv[1]), 4);
        file.write(VAL(cutOut->uv[2]), 4);
        file.write(VAL(cutOut->uv[3]), 4);
    }

    if(cutOut->wh[0] != 0 || cutOut->wh[1] != 0)
    {
        file.write("PPWH", 4);
        file.write(VAL(cutOut->wh[0]), 4);
        file.write(VAL(cutOut->wh[1]), 4);
    }

    if(cutOut->texture > 0)
    {
        file.write("PPGR", 4);
        file.write(VAL(cutOut->texture), 4);
    }

    if(cutOut->pptx != 0)
    {
        file.write("PPTX", 4);
        file.write(VAL(cutOut->pptx), 4);
    }

    if(cutOut->ppte[0] != 0 || cutOut->ppte[1] != 0)
    {
        file.write("PPTE", 4);
        file.write(VAL(cutOut->ppte[0]), 2);
        file.write(VAL(cutOut->ppte[1]), 2);
    }

    if(cutOut->colorSlot != -1)
    {
        file.write("PPCL", 4);
        file.write(VAL(cutOut->colorSlot), 4);
    }

    if(cutOut->shapeIndex > 0)
    {
        file.write("PPVT", 4);
        file.write(VAL(cutOut->shapeIndex), 4);
    }

    if(cutOut->ppjp[0] != 0 || cutOut->ppjp[1] != 0)
    {
        file.write("PPJP", 4);
        file.write(VAL(cutOut->ppjp[0]), 4);
        file.write(VAL(cutOut->ppjp[1]), 4);
    }
}

template<>
bool CutOut<>::IsModifiedData(const CutOut *cutOut)
{
    if(!cutOut->name.empty())
        return true;
    if(cutOut->xy[0] != 0 || cutOut->xy[1] != 0)
        return true;
    if(cutOut->uv[0] != 0 || cutOut->uv[1] != 0 || cutOut->uv[2] != 0 || cutOut->uv[3] != 0)
        return true;
    if(cutOut->wh[0] != 0 || cutOut->wh[1] != 0)
        return true;
    if(cutOut->texture > 0)
        return true;
    if(cutOut->pptx != 0)
        return true;
    if(cutOut->colorSlot != -1)
        return true;
    if(cutOut->shapeIndex > 0)
        return true;
    if(cutOut->ppjp[0] != 0 || cutOut->ppjp[1] != 0)
        return true;

    return false;
}

template<>
void CutOut<>::CopyTo(CutOut *cutOut)
{
    cutOut->name = name;
    for(int i = 0; i < 4; ++i)
        cutOut->uv[i] = uv[i];
    cutOut->wh[0] = wh[0];
    cutOut->wh[1] = wh[1];
    cutOut->xy[0] = xy[0];
    cutOut->xy[1] = xy[1];
    cutOut->ppjp[0] = ppjp[0];
    cutOut->ppjp[1] = ppjp[1];
    cutOut->ppte[0] = ppte[0];
    cutOut->ppte[1] = ppte[1];
    cutOut->texture = texture;
    cutOut->colorSlot = colorSlot;
    cutOut->shapeIndex = shapeIndex;
    cutOut->pptx = pptx;
}
