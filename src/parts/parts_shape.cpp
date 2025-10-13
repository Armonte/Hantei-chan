#include "parts_shape.h"
#include <fstream>
#include <cstring>

template<>
unsigned int* Shape<>::VeLoad(unsigned int* data, const unsigned int* data_end, int amount, int len, std::vector<Shape<>>* shapes)
{
    for (int id = 0; id < amount && data < data_end; id++) {
        Shape sh{};
        sh.id = id;
        sh.type = (ShapeType)data[0];

        switch (sh.type)
        {
            // Plane
            case ShapeType::PLANE:
            // Type 2 - unknown, treated like plane
            case ShapeType::UNK2:
                // No additional parameters
                break;

            // Ring, Arc
            case ShapeType::RING:
            case ShapeType::ARC:
                sh.radius = data[3];
                sh.width = data[4];
                sh.vertexCount = data[5];
                sh.length = data[6];
                sh.dz = data[7];
                sh.dRadius = data[8];
                break;

            // Sphere
            case ShapeType::SPHERE:
                sh.radius = data[3];
                sh.vertexCount = data[4];
                sh.vertexCount2 = data[5];
                sh.length = data[6];
                sh.length2 = data[7];
                break;

            // Cone
            case ShapeType::CONE:
                sh.radius = data[3];
                sh.dz = data[4];
                sh.vertexCount = data[5];
                sh.vertexCount2 = data[6];
                sh.length = data[7];
                break;

            default:
                std::cout << "\tUnknown Shape type: " << (int)sh.type << "\n";
                break;
        }

        shapes->push_back(sh);
        data += len;
    }

    return data;
}

// UNI only - load shape names
template<>
unsigned int* Shape<>::VnLoad(unsigned int* data, const unsigned int* data_end, int amount, std::vector<Shape<>>* shapes)
{
    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "VNST", 4)) {
            // Names section
            for (int id = 0; id < amount && data < data_end; id++) {
                auto sh = &(*shapes)[id];
                sh->name = (char*)data;
                sh->name = sj2utf8(sh->name);
                data += 0x20 / 4; // 32-byte name field
            }
        }
        else if (!memcmp(buf, "VEED", 4)) {
            // Shape section end
            break;
        }
        else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cout << "\tUnknown VE level tag: " << tag << "\n";
        }
    }

    return data;
}

template<>
void Shape<>::Save(std::ofstream &file, const Shape<> *shape, bool saveName)
{
    if(saveName) {
        // Save name (32-byte field)
        char buf[32]{};
        std::string name = utf82sj(shape->name);
        strncpy(buf, name.c_str(), 32);
        buf[31] = 0;
        file.write(PTR(buf), 32);
    }
    else {
        // Save shape data (16 int32 parameters)
        int params[16];
        for(int i = 0; i < 16; ++i)
            params[i] = 0;

        params[0] = (int)shape->type;

        switch(shape->type)
        {
            case ShapeType::RING:
            case ShapeType::ARC:
                params[3] = shape->radius;
                params[4] = shape->width;
                params[5] = shape->vertexCount;
                params[6] = shape->length;
                params[7] = shape->dz;
                params[8] = shape->dRadius;
                break;

            case ShapeType::SPHERE:
                params[3] = shape->radius;
                params[4] = shape->vertexCount;
                params[5] = shape->vertexCount2;
                params[6] = shape->length;
                params[7] = shape->length2;
                break;

            case ShapeType::CONE:
                params[3] = shape->radius;
                params[4] = shape->dz;
                params[5] = shape->vertexCount;
                params[6] = shape->vertexCount2;
                params[7] = shape->length;
                break;

            default:
                break;
        }

        for(int i = 0; i < 16; ++i)
            file.write(VAL(params[i]), 4);
    }
}

template<>
bool Shape<>::IsModifiedData(const Shape<> *shape)
{
    if(!shape->name.empty())
        return true;

    switch(shape->type)
    {
        case ShapeType::RING:
        case ShapeType::ARC:
            if(shape->radius != 0 ||
                shape->width != 0 ||
                shape->vertexCount != 0 ||
                shape->length != 0 ||
                shape->dz != 0 ||
                shape->dRadius != 0)
                return true;
            break;

        case ShapeType::SPHERE:
            if(shape->radius != 0 ||
               shape->vertexCount != 0 ||
               shape->vertexCount2 != 0 ||
               shape->length != 0 ||
               shape->length2 != 0)
                return true;
            break;

        case ShapeType::CONE:
            if(shape->radius != 0 ||
               shape->dz != 0 ||
               shape->vertexCount != 0 ||
               shape->vertexCount2 != 0 ||
               shape->length != 0)
                return true;
            break;

        default:
            break;
    }

    return false;
}

template<>
void Shape<>::CopyTo(Shape<>* shape)
{
    shape->name = name;
    shape->type = type;
    shape->vertexCount = vertexCount;
    shape->vertexCount2 = vertexCount2;
    shape->length = length;
    shape->length2 = length2;
    shape->radius = radius;
    shape->dRadius = dRadius;
    shape->width = width;
    shape->dz = dz;
}
