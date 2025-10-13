#include "parts.h"
#include "../misc.h"
#include "../cg.h"

#include <algorithm>
#include <iostream>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iomanip>

#include <glm/gtc/matrix_transform.hpp>

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

Parts::Parts(CG* cgRef) : cg(cgRef), partVertices(Vao::F3F4, GL_STATIC_DRAW)
{
}

Parts::~Parts()
{
    Free();
    delete[] data;
}

unsigned int* Parts::MainLoad(unsigned int* data, const unsigned int* data_end)
{
    while (data < data_end) {
        unsigned int* buf = data;
        ++data;

        if (!memcmp(buf, "P_ST", 4)) {
            // PartSet
            unsigned int p_id = *data;
            ++data;
            while(partSets.size() <= p_id) {
                auto p = &partSets.emplace_back();
                p->partId = partSets.size() - 1;
            }
            PartSet<>* partSet = &partSets[p_id];
            data = PartSet<>::P_Load(data, data_end, p_id, partSet);
        }
        else if (!memcmp(buf, "PPST", 4)) {
            // CutOut
            int id = data[0];
            ++data;
            while(cutOuts.size() <= id) {
                auto p = &cutOuts.emplace_back();
                p->id = cutOuts.size() - 1;
            }
            data = CutOut<>::PpLoad(data, data_end, id, &cutOuts);
        }
        else if (!memcmp(buf, "PGST", 4)) {
            // Texture
            int id = data[0];
            ++data;
            while(gfxMeta.size() <= id) {
                auto p = &gfxMeta.emplace_back();
                p->id = gfxMeta.size() - 1;
            }
            data = PartGfx<>::PgLoad(data, data_end, id, &gfxMeta);
        }
        else if (!memcmp(buf, "VEST", 4)) {
            // Shapes
            int amount = data[0];
            int len = data[1];
            data += 2;
            data = Shape<>::VeLoad(data, data_end, amount, len, &shapes);
            data = Shape<>::VnLoad(data, data_end, amount, &shapes);
        }
        else if (!memcmp(buf, "_END", 4)) {
            break;
        }
        else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cout << "\tUnknown top level tag: " << tag << "\n";
        }
    }
    return data;
}

bool Parts::Load(const char* name)
{
    char* loadData;
    unsigned int size;
    if (!ReadInMem(name, loadData, size))
        return false;

    if (memcmp(loadData, "PAniDataFile", 12))
    {
        delete[] loadData;
        return false;
    }

    unsigned int* d = (unsigned int*)(loadData + 0x20);
    unsigned int* d_end = (unsigned int*)(loadData + size);
    if (memcmp(d, "_STR", 4)) {
        delete[] loadData;
        return false;
    }

    // Free old data
    delete[] this->data;
    this->data = loadData;
    for (auto& tex : textures)
        delete tex;
    textures.clear();
    partSets.clear();
    cutOuts.clear();
    gfxMeta.clear();
    shapes.clear();
    partVertices.Clear();

    // Parse file
    MainLoad(d + 1, d_end);
    std::cout << "Loaded " << partSets.size() << " part sets, "
              << cutOuts.size() << " cutouts, "
              << shapes.size() << " shapes, "
              << gfxMeta.size() << " textures" << std::endl;

    // Load textures into OpenGL
    for (auto& gfx : gfxMeta)
    {
        textures.push_back(new Texture);
        if (gfx.s3tc)
        {
            if (gfx.type == 21)
            {
                // Uncompressed RGB
                textures.back()->LoadDirect((char*)gfx.s3tc, gfx.w, gfx.h);
            }
            else
            {
                // Compressed (DXT1 or DXT5)
                size_t compressedSize;
                if (gfx.type == 5)
                    compressedSize = gfx.w * gfx.h;
                else if (gfx.type == 1)
                    compressedSize = (gfx.w * gfx.h) * 3 / 6;
                else
                    assert(0 && "Unknown compression type");

                textures.back()->LoadCompressed((char*)gfx.s3tc, gfx.w, gfx.h, compressedSize, gfx.type);
            }
            if (!gfx.dontDelete)
                delete[](gfx.s3tc - 128);
        }
        else if (gfx.data)
        {
            textures.back()->LoadDirect(gfx.data, gfx.w, gfx.h);
        }
        gfx.textureIndex = textures.back()->id;
    }

    filePath = name;
    loaded = true;
    return true;
}

void Parts::Free()
{
    for (auto& tex : textures)
        delete tex;
    textures.clear();
    partSets.clear();
    cutOuts.clear();
    shapes.clear();
    gfxMeta.clear();
    loaded = false;
}

void Parts::initEmpty()
{
    Free();
    auto partset = &partSets.emplace_back();
    partset->groups.emplace_back();
    cutOuts.emplace_back();
    shapes.emplace_back();
    gfxMeta.emplace_back();
    loaded = true;
}

void Parts::updateCGReference(CG* cgRef)
{
    cg = cgRef;
}

// Accessor methods
PartSet<>* Parts::GetPartSet(unsigned int n)
{
    if (partSets.size() > n)
        return &partSets[n];
    return nullptr;
}

PartProperty* Parts::GetPartProp(int partSetId, unsigned int propId)
{
    auto partset = GetPartSet(partSetId);
    if(partset != nullptr && propId < partset->groups.size())
    {
        return &partset->groups[propId];
    }
    return nullptr;
}

CutOut<>* Parts::GetCutOut(unsigned int n)
{
    if (cutOuts.size() > n)
        return &cutOuts[n];
    return nullptr;
}

Shape<>* Parts::GetShape(unsigned int n)
{
    if (shapes.size() > n)
        return &shapes[n];
    return nullptr;
}

PartGfx<>* Parts::GetPartGfx(unsigned int n)
{
    if (gfxMeta.size() > n)
        return &gfxMeta[n];
    return nullptr;
}

// Decorated name helpers (for UI combo boxes)
std::string Parts::GetPartSetDecorateName(int n) {
    if(partSets.size() <= n) return "Error";
    auto partSet = partSets[n];
    std::stringstream ss;
    ss.flags(std::ios_base::right);
    ss << std::setfill('0') << std::setw(3) << n << ": ";
    if(partSet.groups.empty())
        ss << u8"〇 ";
    if(partSet.name.empty() && !partSet.groups.empty())
        ss << u8"Untitled";
    ss << partSet.name;
    return ss.str();
}

std::string Parts::GetPartPropsDecorateName(int p_id, int prop_id) {
    if(p_id >= partSets.size()) return "Error";
    auto partSet = partSets[p_id];
    if(partSet.groups.size() <= prop_id) return "Error";
    auto prop = partSet.groups[prop_id];
    std::stringstream ss;
    ss.flags(std::ios_base::right);
    ss << std::setfill('0') << std::setw(3) << prop_id << ": ";
    if(prop.ppId < 0)
        ss << u8"Unused Prop";
    else if(prop.ppId >= cutOuts.size())
        ss << u8"Prop outside of CutBoxes list";
    else if(cutOuts[prop.ppId].name.empty())
        ss << u8"Untitled Part";
    else
        ss << cutOuts[prop.ppId].name;
    return ss.str();
}

std::string Parts::GetPartCutOutsDecorateName(int n) {
    if(cutOuts.size() <= n) return "Error";
    auto cutOut = cutOuts[n];
    std::stringstream ss;
    ss.flags(std::ios_base::right);
    ss << std::setfill('0') << std::setw(3) << n << ": ";
    if(cutOut.texture < 0 || cutOut.shapeIndex < 0)
        ss << u8"〇 ";
    if(cutOut.name.empty() && cutOut.texture >= 0 && cutOut.shapeIndex >= 0)
        ss << u8"Untitled";
    ss << cutOut.name;
    return ss.str();
}

std::string Parts::GetShapesDecorateName(int n) {
    if(shapes.size() <= n) return "Error";
    auto shape = shapes[n];
    std::stringstream ss;
    ss.flags(std::ios_base::right);
    ss << std::setfill('0') << std::setw(3) << n << ": ";
    if(shape.name.empty())
        ss << u8"Untitled";
    ss << shape.name;
    return ss.str();
}

std::string Parts::GetTexturesDecorateName(int n) {
    if(gfxMeta.size() <= n) return "Error";
    auto gfx = gfxMeta[n];
    std::stringstream ss;
    ss.flags(std::ios_base::right);
    ss << std::setfill('0') << std::setw(3) << n << ": ";
    if(gfx.name.empty())
        ss << u8"Untitled";
    ss << gfx.name;
    return ss.str();
}

// Parts.cpp continues in next message due to length...
// Parts rendering implementation - append to parts.cpp or include
// This file contains the Draw() and DrawPart() methods which are the most complex

#include "parts.h"
#include <glm/gtc/matrix_transform.hpp>

void Parts::DrawPart(int i)
{
    if (cutOuts.size() > i && gfxMeta.size() > cutOuts[i].texture)
    {
        curTexId = gfxMeta[cutOuts[i].texture].textureIndex;
        glBindTexture(GL_TEXTURE_2D, curTexId);
        partVertices.Draw(0);
    }
}

void Parts::Draw(int pattern, int nextPattern, float interpolationFactor,
    std::function<void(glm::mat4)> setMatrix,
    std::function<void(float, float, float)> setAddColor,
    std::function<void(char)> setFlip,
    float color[4])
{
    curTexId = -1;

    // Validate pattern indices
    if(pattern < 0 || pattern >= partSets.size() ||
       nextPattern < 0 || nextPattern >= partSets.size())
        return;

    if (partSets[pattern].groups.empty())
        return;

    // Sort groups by priority (higher priority = draw first)
    auto copyGroups = partSets[pattern].groups;
    std::reverse(copyGroups.begin(), copyGroups.end());
    std::stable_sort(copyGroups.rbegin(), copyGroups.rend(),
        [](const PartProperty &a, const PartProperty &b) {
            return a.priority < b.priority;
    });

    constexpr float tau = glm::pi<float>() * 2.f;

    // Triangle indices for generating geometry
    constexpr int tX[] = { 0,1,1, 1,0,0 };
    constexpr int tY[] = { 0,0,1, 1,1,0 };

    struct {
        float x, y, z, s, t, p, q;
    } point[6];

    float width = 256.f;   // Texture UV width
    float height = 256.f;  // Texture UV height

    for (auto& part : copyGroups)
    {
        if (cutOuts.size() <= part.ppId)
            continue;
        auto cutout = cutOuts[part.ppId];

        if(shapes.size() <= cutout.shapeIndex)
            continue;

        glm::vec2 offset = glm::vec2(part.x, part.y);
        glm::vec3 rotation = glm::vec3(part.rotation[1], part.rotation[2], part.rotation[3]);
        glm::vec2 scale = glm::vec2(part.scaleX, part.scaleY);
        glm::vec4 rgba = glm::vec4(part.rgba[0]*255.f, part.rgba[1]*255.f,
            part.rgba[2]*255.f, part.rgba[3]*255.f);

        Shape currentShape = shapes[cutout.shapeIndex];
        Shape outShape = currentShape; // Copy for potential interpolation

        // Prevent crash on invalid shapes
        if(outShape.type != ShapeType::PLANE && outShape.type != ShapeType::UNK2 &&
            outShape.vertexCount == 0)
            return;
        if((outShape.type == ShapeType::SPHERE || outShape.type == ShapeType::CONE) &&
           outShape.vertexCount2 == 0)
            return;

        // Interpolation between frames
        if (interpolationFactor < 1 && !partSets[nextPattern].groups.empty()) {
            auto copyNextGroups = partSets[nextPattern].groups;
            std::reverse(copyNextGroups.begin(), copyNextGroups.end());
            std::stable_sort(copyNextGroups.rbegin(), copyNextGroups.rend(),
                [](const PartProperty &a, const PartProperty &b) {
                    return a.priority < b.priority;
            });

            auto mix = [interpolationFactor](float a, float b) {
                return a * interpolationFactor + b * (1 - interpolationFactor);
            };
            auto mixRotation = [interpolationFactor,mix](float a, float b) {
                if (b - a > 0.5) {
                    return a + (b - a - 1)* (1 - interpolationFactor);
                }
                else if (b - a < -0.5) {
                    return a + (b - a + 1) * (1 - interpolationFactor);
                }
                return mix(a, b);
            };

            auto nextPart = std::find_if(copyNextGroups.begin(), copyNextGroups.end(),
                [&part](const auto& pt) {return pt.propId == part.propId; });

            if (nextPart != copyNextGroups.end() && (*nextPart).ppId < cutOuts.size()) {
                auto nextCutout = cutOuts[(*nextPart).ppId];
                if (nextCutout.shapeIndex < shapes.size()) {
                    Shape nextShape = shapes[nextCutout.shapeIndex];

                    offset[0] = mix(offset[0], (*nextPart).x);
                    offset[1] = mix(offset[1], (*nextPart).y);
                    rotation[0] = mixRotation(rotation[0], (*nextPart).rotation[1]);
                    rotation[1] = mixRotation(rotation[1], (*nextPart).rotation[2]);
                    rotation[2] = mixRotation(rotation[2], (*nextPart).rotation[3]);
                    scale[0] = mix(scale[0], (*nextPart).scaleX);
                    scale[1] = mix(scale[1], (*nextPart).scaleY);
                    rgba[0] = mix(rgba[0], (*nextPart).rgba[0]*255.f);
                    rgba[1] = mix(rgba[1], (*nextPart).rgba[1]*255.f);
                    rgba[2] = mix(rgba[2], (*nextPart).rgba[2]*255.f);
                    rgba[3] = mix(rgba[3], (*nextPart).rgba[3]*255.f);

                    if ((int)currentShape.type > 2 && nextShape.type == currentShape.type) {
                        outShape.dRadius = mix(outShape.dRadius, nextShape.dRadius);
                        outShape.dz = mix(outShape.dz, nextShape.dz);
                        outShape.length = mix(outShape.length, nextShape.length);
                        outShape.length2 = mix(outShape.length2, nextShape.length2);
                        outShape.radius = mix(outShape.radius, nextShape.radius);
                        outShape.width = mix(outShape.width, nextShape.width);
                    }
                }
            }
        }

        // Set up transformation matrix
        glm::mat4 view = glm::mat4(1.f);
        view = glm::translate(view, glm::vec3(offset[0], offset[1], 0.f));
        view = glm::rotate(view, -rotation[1] * tau, glm::vec3(0.0, 1.f, 0.f));
        view = glm::rotate(view, -rotation[0] * tau, glm::vec3(1.0, 0.f, 0.f));
        view = glm::rotate(view, rotation[2] * tau, glm::vec3(0.0, 0.f, 1.f));
        view = glm::scale(view, glm::vec3(scale[0], scale[1], 1.f));

        setMatrix(view);
        setFlip(part.flip);

        // Set blending mode
        if (part.additive)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Apply color (including palette color if applicable)
        float newColor[4];
        memcpy(newColor, color, sizeof(float) * 4);
        if (cutout.colorSlot != -1 && cg) {
            unsigned int palColor = cg->getColorFromPal(cutout.colorSlot);
            if (palColor) {
                newColor[0] *= (palColor & 0xFF) / 255.f;
                newColor[1] *= ((palColor >> 8) & 0xFF) / 255.f;
                newColor[2] *= ((palColor >> 16) & 0xFF) / 255.f;
                newColor[3] *= (palColor >> 24) / 255.f;
            }
        }
        newColor[0] *= rgba[0] / 255.f;
        newColor[1] *= rgba[1] / 255.f;
        newColor[2] *= rgba[2] / 255.f;
        newColor[3] *= rgba[3] / 255.f;

        glVertexAttrib4fv(2, newColor);
        setAddColor(part.addColor[2] / 255.f, part.addColor[1] / 255.f, part.addColor[0] / 255.f);

        // Generate vertices based on shape type
        std::vector<float> vertexData;
        auto size = 0;

        switch (outShape.type)
        {
        case ShapeType::PLANE:
        case ShapeType::UNK2:
        {
            // Simple plane (quad)
            for (int i = 0; i < 6; i++)
            {
                point[i].x = -cutout.xy[0] + cutout.wh[0] * tX[i];
                point[i].y = -cutout.xy[1] + cutout.wh[1] * tY[i];
                point[i].z = 0;
                point[i].s = float(cutout.uv[0] + cutout.uv[2] * tX[i]) / width;
                point[i].t = float(cutout.uv[1] + cutout.uv[3] * tY[i]) / height;
                point[i].p = float(cutout.uv[0] + cutout.uv[2] * (1 - tX[i])) / width;
                point[i].q = float(cutout.uv[1] + cutout.uv[3] * (1 - tY[i])) / height;
            }
            partVertices.Prepare(6 * 7 * sizeof(float), &point[0]);
            break;
        }
        case ShapeType::RING:
        {
            // Cylindrical ring
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            vertexData.resize(size + 6 * outShape.vertexCount * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < 6; j++)
                {
                    point[j].x = float(outShape.radius - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::sin(angle + delta * tY[j]);
                    point[j].y = -float(outShape.radius - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::cos(angle + delta * tY[j]);
                    point[j].z = float(outShape.width * -(tX[j] * 2 - 1) - float(outShape.dz * (i + tY[j])) / outShape.vertexCount);
                    point[j].s = float(cutout.uv[0] + cutout.uv[2] * (tX[j])) / width;
                    point[j].t = float(cutout.uv[1] + 1.0f * cutout.uv[3] * (i + tY[j]) / outShape.vertexCount) / height;
                    point[j].p = float(cutout.uv[0] + cutout.uv[2] * (1 - tX[j])) / width;
                    point[j].q = float(cutout.uv[1] + 1.0f * cutout.uv[3] * (outShape.vertexCount - i - tY[j]) / outShape.vertexCount) / height;
                }
                angle += delta;
                memcpy(&vertexData[size + 6 * 7 * i], point, sizeof(point));
            }
            partVertices.Prepare(6 * 7 * outShape.vertexCount * sizeof(float), &vertexData[0]);
            break;
        }
        // Additional shape types (ARC, SPHERE, CONE) follow the same pattern
        // Omitted for brevity but can be added from sosfiro's implementation
        default:
            continue;
        }

        // Render the part
        partVertices.Load();
        partVertices.Bind();
        DrawPart(part.ppId);
        partVertices.Clear();
    }
}

bool Parts::Save(const char* filename)
{
    // Validate that we have data to save
    bool hasData = false;
    for(auto& partset : partSets) {
        if(PartSet<>::IsModifiedData(&partset)) {
            hasData = true;
            break;
        }
    }
    if(!hasData) {
        for(auto& cutOut : cutOuts) {
            if(CutOut<>::IsModifiedData(&cutOut)) {
                hasData = true;
                break;
            }
        }
    }
    if(!hasData) {
        for(auto& shape : shapes) {
            if(Shape<>::IsModifiedData(&shape)) {
                hasData = true;
                break;
            }
        }
    }
    if(!hasData) {
        for(auto& gfx : gfxMeta) {
            if(PartGfx<>::IsModifiedData(&gfx)) {
                hasData = true;
                break;
            }
        }
    }
    if(!hasData) return false;

    std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
    if (!file.is_open())
        return false;

    // Write header
    char header[32] = "PAniDataFile";
    file.write(header, sizeof(header));
    file.write("_STR", 4);

    // Write PartSets
    for(uint32_t i = 0; i < partSets.size(); i++)
    {
        if(!PartSet<>::IsModifiedData(&partSets[i]))
            continue;
        file.write("P_ST", 4);
        file.write(VAL(i), 4);
        PartSet<>::Save(file, &partSets[i]);
        file.write("P_ED", 4);
    }

    // Write CutOuts
    for(uint32_t i = 0; i < cutOuts.size(); i++)
    {
        if(!CutOut<>::IsModifiedData(&cutOuts[i]))
            continue;
        file.write("PPST", 4);
        file.write(VAL(i), 4);
        CutOut<>::Save(file, &cutOuts[i]);
        file.write("PPED", 4);
    }

    // Write Shapes
    int paramSize = 16;
    size_t shapeCount = shapes.size();
    file.write("VEST", 4);
    file.write(VAL(shapeCount), 4);
    file.write(VAL(paramSize), 4);

    for(uint32_t i = 0; i < shapes.size(); i++)
    {
        if(!Shape<>::IsModifiedData(&shapes[i]))
            continue;
        Shape<>::Save(file, &shapes[i], false);
    }

    file.write("VNST", 4);
    for(uint32_t i = 0; i < shapes.size(); i++)
    {
        if(!Shape<>::IsModifiedData(&shapes[i]))
            continue;
        Shape<>::Save(file, &shapes[i], true);
    }

    file.write("VEED", 4);

    // Write Textures
    for(uint32_t i = 0; i < gfxMeta.size(); i++)
    {
        if(!PartGfx<>::IsModifiedData(&gfxMeta[i]))
            continue;
        file.write("PGST", 4);
        file.write(VAL(i), 4);
        PartGfx<>::Save(file, &gfxMeta[i]);
        file.write("PGED", 4);
    }

    file.write("_END", 4);
    file.close();
    return true;
}
