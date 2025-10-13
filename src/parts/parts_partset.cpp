#include "parts_partset.h"
#include <fstream>
#include <cstring>

template<>
unsigned int* PartSet<>::P_Load(unsigned int* data, const unsigned int* data_end, int id, PartSet<>* partSet)
{
    std::string name;
    partSet->partId = id;
    bool hasData = false;

    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "PANM", 4)) {
            // Melty name (null-terminated, 32 bytes)
            name = (char*)data;
            data += 0x20 / 4;
        }
        else if (!memcmp(buf, "PANA", 4)) {
            // UNI name (length-prefixed Shift-JIS)
            unsigned char* cdata = (unsigned char*)data;
            name.resize(cdata[0]); // Length at byte 0
            memcpy(name.data(), cdata + 1, cdata[0]);
            cdata += cdata[0] + 1;
            data = (unsigned int*)cdata;
            name = sj2utf8(name);
            partSet->name = name;
        }
        else if (!memcmp(buf, "PRST", 4)) {
            // Part property start
            int propId = data[0];
            hasData = true;
            ++data;

            // Ensure groups vector has enough space
            while(partSet->groups.size() <= propId) {
                auto p = &partSet->groups.emplace_back();
                p->propId = partSet->groups.size() - 1;
            }

            data = PartSet<>::PrLoad(data, data_end, id, propId, partSet);
        }
        else if (!memcmp(buf, "P_ED", 4)) {
            // PartSet end
            break;
        }
        else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cout << "\tUnknown P_ level tag: " << tag << "\n";
        }
    }

    return data;
}

template <template <typename> class Allocator>
unsigned int* PartSet<Allocator>::PrLoad(unsigned int* data, const unsigned int* data_end, int groupId, int propId,
    PartSet<>* partSet)
{
    PartProperty pr{};
    pr.propId = propId;

    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "PRXY", 4)) {
            // Position X, Y
            pr.x = ((int*)data)[0];
            pr.y = ((int*)data)[1];
            data += 2;
        }
        else if (!memcmp(buf, "PRAL", 4)) {
            // Additive blend mode
            unsigned char* cdata = (unsigned char*)data;
            pr.additive = *cdata;
            if (pr.additive == 2) {
                pr.additive = 1;
            }
            ++cdata;
            data = (unsigned int*)cdata;
        }
        else if (!memcmp(buf, "PRRV", 4)) {
            // Flip flags (flip & 1 = flip horizontally; flip & 2 = flip vertically)
            auto* cdata = (unsigned char*)data;
            pr.flip = *cdata;
            ++cdata;
            data = (unsigned int*)cdata;
        }
        else if (!memcmp(buf, "PRFL", 4)) {
            // Filter flag
            unsigned char* cdata = (unsigned char*)data;
            pr.filter = *cdata;
            ++cdata;
            data = (unsigned int*)cdata;
        }
        else if (!memcmp(buf, "PRZM", 4)) {
            // Scale X, Y
            pr.scaleX = *((float*)data);
            pr.scaleY = *((float*)data + 1);
            data += 2;
        }
        else if (!memcmp(buf, "PRSP", 4)) {
            // Additive color (RGBA stored as bytes)
            int byte1 = (data[0] >> 24) & 0xFF;
            int byte2 = (data[0] >> 16) & 0xFF;
            int byte3 = (data[0] >> 8) & 0xFF;
            int byte4 = data[0] & 0xFF;
            pr.addColor[0] = byte4 / 255.f;
            pr.addColor[1] = byte3 / 255.f;
            pr.addColor[2] = byte2 / 255.f;
            pr.addColor[3] = byte1 / 255.f;
            ++data;
        }
        else if (!memcmp(buf, "PRAN", 4)) {
            // Rotation (legacy, single float). Clockwise. 1.f = 360 degrees
            pr.rotation[3] = *((float*)data);
            ++data;
        }
        else if (!memcmp(buf, "PRA3", 4)) {
            // 3D rotation (4 floats)
            memcpy(pr.rotation, data, sizeof(float) * 4);
            data += 4;
        }
        else if (!memcmp(buf, "PRPR", 4)) {
            // Priority. Higher value means draw first / lower on the stack
            pr.priority = *data;
            pr.priority += propId / 1000.f; // Preserve sub-priority
            ++data;
        }
        else if (!memcmp(buf, "PRID", 4)) {
            // Part/CutOut ID. Which thing to render
            pr.ppId = *data;
            ++data;
        }
        else if (!memcmp(buf, "PRCL", 4)) {
            // Color (BGRA stored as bytes)
            int byte1 = (data[0] >> 24) & 0xFF;
            int byte2 = (data[0] >> 16) & 0xFF;
            int byte3 = (data[0] >> 8) & 0xFF;
            int byte4 = data[0] & 0xFF;
            pr.rgba[0] = byte2 / 255.f; // R from B position
            pr.rgba[1] = byte3 / 255.f; // G from G position
            pr.rgba[2] = byte4 / 255.f; // B from R position
            pr.rgba[3] = byte1 / 255.f; // A from A position
            ++data;
        }
        else if (!memcmp(buf, "PRED", 4)) {
            // Part property end
            partSet->groups[propId] = pr;
            break;
        }
        else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cout << "\tUnknown PR level tag: " << tag << "\n";
        }
    }

    return data;
}

template<>
bool PartSet<std::allocator>::IsModifiedPropData(const PartProperty *prop)
{
    if(prop->x != 0 || prop->y != 0)
        return true;
    if(prop->ppId > 0)
        return true;
    if(prop->priority != 0)
        return true;
    if(prop->rotation[0] != 0 ||
        prop->rotation[1] != 0 ||
        prop->rotation[2] != 0 ||
        prop->rotation[3] != 0)
            return true;
    if(prop->scaleX != 1.f || prop->scaleY != 1.f)
        return true;
    if(prop->additive || prop->filter)
        return true;
    if(prop->flip != 0)
        return true;
    if(prop->rgba[0] != 1 ||
        prop->rgba[1] != 1 ||
        prop->rgba[2] != 1 ||
        prop->rgba[3] != 1)
            return true;
    if(prop->addColor[0] != 0 ||
       prop->addColor[1] != 0 ||
       prop->addColor[2] != 0 ||
       prop->addColor[3] != 0)
        return true;

    return false;
}

template<>
void PartSet<std::allocator>::CopyPropertyTo(PartProperty *propDst, PartProperty *propSrc)
{
    for(int i = 0; i < 4; i++) {
        propDst->addColor[i] = propSrc->addColor[i];
        propDst->rgba[i] = propSrc->rgba[i];
        propDst->rotation[i] = propSrc->rotation[i];
    }

    propDst->x = propSrc->x;
    propDst->y = propSrc->y;
    propDst->scaleX = propSrc->scaleX;
    propDst->scaleY = propSrc->scaleY;
    propDst->additive = propSrc->additive;
    propDst->priority = propSrc->priority;
    propDst->ppId = propSrc->ppId;
    propDst->flip = propSrc->flip;
    propDst->filter = propSrc->filter;
}

template<>
void PartSet<>::Save(std::ofstream &file, const PartSet *partSet)
{
    if(!partSet->name.empty()) {
        file.write("PANA", 4);
        std::string name = utf82sj(partSet->name);
        uint32_t size = name.size();
        file.write(VAL(size), 1);
        file.write(PTR(name.data()), size);
    }

    auto props = &partSet->groups;
    for(uint32_t i = 0; i < props->size(); i++)
    {
        auto prop = &props->at(i);
        int tempValue = 1;

        if(!IsModifiedPropData(prop))
            continue;

        file.write("PRST", 4);
        file.write(VAL(i), 4);

        if(prop->x != 0 || prop->y != 0)
        {
            file.write("PRXY", 4);
            file.write(VAL(prop->x), 4);
            file.write(VAL(prop->y), 4);
        }

        if(prop->additive)
        {
            file.write("PRAL", 4);
            file.write(VAL(tempValue), 1);
        }

        if(prop->flip != 0)
        {
            file.write("PRRV", 4);
            file.write(VAL(prop->flip), 1);
        }

        if(prop->filter)
        {
            file.write("PRFL", 4);
            file.write(VAL(tempValue), 1);
        }

        if(prop->scaleX != 1.f || prop->scaleY != 1.f)
        {
            file.write("PRZM", 4);
            file.write(VAL(prop->scaleX), 4);
            file.write(VAL(prop->scaleY), 4);
        }

        if(prop->rgba[0] != 1 ||
           prop->rgba[1] != 1 ||
           prop->rgba[2] != 1 ||
           prop->rgba[3] != 1)
        {
            file.write("PRCL", 4);
            // Write as BGRA
            int values[4]{};
            values[0] = prop->rgba[0] * 255.f;
            values[1] = prop->rgba[1] * 255.f;
            values[2] = prop->rgba[2] * 255.f;
            values[3] = prop->rgba[3] * 255.f;
            file.write(VAL(values[2]), 1); // B
            file.write(VAL(values[1]), 1); // G
            file.write(VAL(values[0]), 1); // R
            file.write(VAL(values[3]), 1); // A
        }

        if(prop->addColor[0] != 0 ||
           prop->addColor[1] != 0 ||
           prop->addColor[2] != 0 ||
           prop->addColor[3] != 0)
        {
            file.write("PRSP", 4);
            int values[4]{};
            values[0] = prop->addColor[0] * 255.f;
            values[1] = prop->addColor[1] * 255.f;
            values[2] = prop->addColor[2] * 255.f;
            values[3] = prop->addColor[3] * 255.f;
            file.write(VAL(values[0]), 1);
            file.write(VAL(values[1]), 1);
            file.write(VAL(values[2]), 1);
            file.write(VAL(values[3]), 1);
        }

        if(prop->rotation[0] != 0 ||
           prop->rotation[1] != 0 ||
           prop->rotation[2] != 0 ||
           prop->rotation[3] != 0)
        {
            file.write("PRA3", 4);
            file.write(VAL(prop->rotation[0]), 4);
            file.write(VAL(prop->rotation[1]), 4);
            file.write(VAL(prop->rotation[2]), 4);
            file.write(VAL(prop->rotation[3]), 4);
        }

        if(prop->priority != 0)
        {
            file.write("PRPR", 4);
            file.write(VAL(prop->priority), 4);
        }

        if(prop->ppId >= 0)
        {
            file.write("PRID", 4);
            file.write(VAL(prop->ppId), 4);
        }

        file.write("PRED", 4);
    }
}

template<>
bool PartSet<>::IsModifiedData(const PartSet *partSet)
{
    if(!partSet->name.empty() || !partSet->groups.empty())
        return true;
    return false;
}

template<>
void PartSet<>::CopyPartSetTo(PartSet *partSet)
{
    partSet->name = name;
    partSet->groups.clear();
    for(size_t i = 0; i < groups.size(); ++i){
        auto prop = &partSet->groups.emplace_back();
        auto srcProp = &groups[i];
        CopyPropertyTo(prop, srcProp);
    }
}

