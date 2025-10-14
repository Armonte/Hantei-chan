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
    
    // Count non-empty part sets
    int nonEmptyPartSets = 0;
    for (auto& ps : partSets) {
        if (!ps.groups.empty()) nonEmptyPartSets++;
    }
    
    std::cout << "Loaded " << partSets.size() << " part sets (" << nonEmptyPartSets 
              << " non-empty), " << cutOuts.size() << " cutouts, "
              << shapes.size() << " shapes, "
              << gfxMeta.size() << " textures" << std::endl;
    
    // DEBUG: Check gfxMeta immediately
    printf("[DEBUG] gfxMeta.size() right after MainLoad: %zu\n", gfxMeta.size());
    for (size_t i = 0; i < gfxMeta.size() && i < 5; i++) {
        printf("  gfxMeta[%zu]: w=%d, h=%d, type=%d, data=%p, s3tc=%p\n",
            i, gfxMeta[i].w, gfxMeta[i].h, gfxMeta[i].type, 
            gfxMeta[i].data, gfxMeta[i].s3tc);
    }
    
    // Show ALL part sets with data (not just first 20)
    std::cout << "=== All Part Sets with Data ===" << std::endl;
    for (size_t i = 0; i < partSets.size(); i++) {
        if (!partSets[i].groups.empty()) {
            int usedCount = 0;
            std::vector<int> usedPpIds;
            for (auto& prop : partSets[i].groups) {
                if (prop.ppId >= 0) {
                    usedCount++;
                    usedPpIds.push_back(prop.ppId);
                }
            }
            std::cout << "  PartSet " << i << ": " << partSets[i].groups.size() 
                      << " props (" << usedCount << " used) - ppIds: [";
            for (size_t j = 0; j < usedPpIds.size() && j < 10; j++) {
                std::cout << usedPpIds[j];
                if (j < usedPpIds.size()-1 && j < 9) std::cout << ",";
            }
            if (usedPpIds.size() > 10) std::cout << "...";
            std::cout << "]" << std::endl;
        }
    }
    std::cout << "================================" << std::endl;
    
    // List all cutOuts to see what's available
    std::cout << "=== CutOut List ===" << std::endl;
    for (size_t i = 0; i < cutOuts.size() && i < 30; i++) {
        auto& co = cutOuts[i];
        std::cout << "  CutOut " << i << ": texture=" << co.texture 
                  << ", shape=" << co.shapeIndex 
                  << ", uv=(" << co.uv[0] << "," << co.uv[1] << "," << co.uv[2] << "," << co.uv[3] << ")"
                  << ", name=\"" << co.name << "\"" << std::endl;
    }
    if (cutOuts.size() > 30) {
        std::cout << "  ... and " << (cutOuts.size() - 30) << " more cutOuts" << std::endl;
    }
    std::cout << "===================" << std::endl;

    // Load textures into OpenGL
    printf("[Texture Loading] Processing %zu gfxMeta entries...\n", gfxMeta.size());
    for (size_t idx = 0; idx < gfxMeta.size(); idx++)
    {
        auto& gfx = gfxMeta[idx];
        printf("  Tex %zu: type=%d, %dx%d, s3tc=%p, data=%p\n",
            idx, gfx.type, gfx.w, gfx.h, gfx.s3tc, gfx.data);
        
        textures.push_back(new Texture);
        if (gfx.s3tc)
        {
            if (gfx.type == 21)
            {
                // Uncompressed BGRA (type 21 uses BGRA format)
                textures.back()->LoadDirect((char*)gfx.s3tc, gfx.w, gfx.h, true);
                textures.back()->Apply();  // Create OpenGL texture
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
            // PGTX data is also in BGRA format
            textures.back()->LoadDirect(gfx.data, gfx.w, gfx.h, true);
            textures.back()->Apply();  // Create OpenGL texture
            printf("    → Loaded PGTX data\n");
        }
        else
        {
            printf("    → ERROR: No texture data!\n");
        }
        gfx.textureIndex = textures.back()->id;
    }
    printf("[Texture Loading] Created %zu OpenGL textures\n", textures.size());

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

void Parts::updatePatEditorReferences(FrameState* state, RenderMode* mode)
{
    currState = state;
    renderMode = mode;
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
    // Safety: Don't render if Parts is being freed or unloaded
    if (!loaded || cutOuts.empty() || gfxMeta.empty()) {
        return;
    }

    // Validate cutOut index
    if (i < 0 || i >= cutOuts.size()) {
        printf("[DrawPart] Invalid cutOut index: %d (size=%zu)\n", i, cutOuts.size());
        return;
    }

    auto& cutout = cutOuts[i];

    // Validate texture index (must be >= 0 and < gfxMeta.size())
    if (cutout.texture < 0 || cutout.texture >= gfxMeta.size()) {
        printf("[DrawPart] Invalid texture index: %d (gfxMeta.size=%zu)\n", cutout.texture, gfxMeta.size());
        return;
    }

    // Validate that the texture was actually created
    if (gfxMeta[cutout.texture].textureIndex == 0) {
        return;  // Texture not loaded, skip silently
    }

    // Bind texture and draw
    curTexId = gfxMeta[cutout.texture].textureIndex;
    glBindTexture(GL_TEXTURE_2D, curTexId);
    partVertices.Draw(0);
}

void Parts::Draw(int pattern, int nextPattern, float interpolationFactor,
    std::function<void(glm::mat4)> setMatrix,
    std::function<void(float, float, float)> setAddColor,
    std::function<void(char)> setFlip,
    float color[4])
{
    curTexId = -1;

    // Safety: Don't render if Parts is being freed or unloaded
    if (!loaded || partSets.empty() || cutOuts.empty()) {
        return;
    }

    // Validate pattern indices
    if(pattern < 0 || pattern >= partSets.size() ||
       nextPattern < 0 || nextPattern >= partSets.size())
        return;

    if (partSets[pattern].groups.empty())
        return;

    // Sort groups by priority (higher priority = draw first)
    auto copyGroups = partSets[pattern].groups;
    // Note: No reverse() needed - stable_sort with rbegin/rend handles ordering
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

    // Debug pattern changes only
    static int lastPattern = -1;
    if (pattern != lastPattern) {
        printf("\n========== Pattern %d ==========\n", pattern);
        printf("Total parts in pattern: %zu\n", copyGroups.size());
        
        // Show what each part references
        int validParts = 0;
        for (size_t i = 0; i < copyGroups.size(); i++) {
            auto& p = copyGroups[i];
            if (p.ppId >= 0 && p.ppId < cutOuts.size()) {
                auto& co = cutOuts[p.ppId];
                bool validTex = (co.texture >= 0 && co.texture < textures.size() && textures[co.texture] != nullptr);
                bool validUV = (co.uv[2] > 0 || co.uv[3] > 0);
                
                printf("  Part %d: ppId=%d, tex=%d %s, UV=(%d,%d,%dx%d) %s, BGRA=(%d,%d,%d,%d)\n",
                    p.propId, p.ppId, co.texture,
                    validTex ? "✓" : "✗MISSING",
                    co.uv[0], co.uv[1], co.uv[2], co.uv[3],
                    validUV ? "✓" : "✗EMPTY",
                    p.bgra[0], p.bgra[1], p.bgra[2], p.bgra[3]);
                
                if (validTex && validUV) validParts++;
            } else if (p.ppId >= 0) {
                printf("  Part %d: ppId=%d ✗OUT_OF_RANGE (cutOuts.size=%zu)\n", 
                    p.propId, p.ppId, cutOuts.size());
            }
        }
        printf("Valid renderable parts: %d/%zu\n", validParts, copyGroups.size());
        printf("Loaded textures: %zu\n", textures.size());
        printf("===============================\n\n");
        lastPattern = pattern;
    }
    
    for (auto& part : copyGroups)
    {
        // Skip unused or invalid parts silently
        if (part.ppId < 0 || part.ppId >= cutOuts.size()) {
            continue;
        }
        
        auto cutout = cutOuts[part.ppId];
        
        // PatEditor: Override cutout for texture/UV viewing modes
        auto partGfx = GetPartGfx(currState ? currState->partGraph : 0);
        auto cutoutSelected = GetCutOut(currState ? currState->partCutOut : 0);
        if (renderMode && currState && *renderMode == UV_SETTING_VIEW && cutoutSelected != nullptr) {
            partGfx = GetPartGfx(cutoutSelected->texture);
        }
        if (renderMode && currState && *renderMode != DEFAULT && !currState->animating && partGfx != nullptr) {
            // Override cutout to show full texture
            cutout.texture = partGfx->id;
            cutout.shapeIndex = 0;
            cutout.uv[0] = 0;
            cutout.uv[1] = 0;
            cutout.uv[2] = 255;
            cutout.uv[3] = 255;
            cutout.xy[0] = 0;
            cutout.xy[1] = 0;
            cutout.wh[0] = partGfx->w;
            cutout.wh[1] = partGfx->h;
        }
        
        // Skip zero-size cutouts (like PACNyx Rectangle.Empty check)
        if (cutout.uv[2] == 0 && cutout.uv[3] == 0) {
            continue;
        }
        
        // Validate texture reference
        if (cutout.texture < 0 || cutout.texture >= gfxMeta.size()) {
            continue;
        }

        // Handle MBAACC format: create default plane shape if shapes array is empty
        // MBAACC .pat files have 0 shapes and use implicit flat planes for all cutouts
        Shape currentShape;
        if (shapes.empty() || cutout.shapeIndex < 0 || cutout.shapeIndex >= shapes.size()) {
            // Default to flat plane for MBAACC compatibility
            currentShape.type = ShapeType::PLANE;
            currentShape.vertexCount = 0;
            currentShape.vertexCount2 = 0;
            currentShape.length = 0;
            currentShape.length2 = 0;
            currentShape.radius = 0;
            currentShape.dRadius = 0;
            currentShape.width = 0;
            currentShape.dz = 0;
        } else {
            currentShape = shapes[cutout.shapeIndex];
        }

        glm::vec2 offset = glm::vec2(part.x, part.y);
        glm::vec3 rotation = glm::vec3(part.rotation[1], part.rotation[2], part.rotation[3]);
        glm::vec2 scale = glm::vec2(part.scaleX, part.scaleY);
        
        // Opacity handling: use full opacity in special render modes or when part is highlighted
        float opacity = (renderMode && currState && *renderMode != DEFAULT && !currState->animating) ||
                        partHighlight == -1 || partHighlight == part.propId ?
                        part.bgra[3] : highlightOpacity * 255.f;
        
        glm::vec4 bgra = glm::vec4(part.bgra[0], part.bgra[1], part.bgra[2], opacity);

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
                    bgra[0] = mix(bgra[0], (*nextPart).bgra[0]);
                    bgra[1] = mix(bgra[1], (*nextPart).bgra[1]);
                    bgra[2] = mix(bgra[2], (*nextPart).bgra[2]);
                    
                    // Interpolate opacity with same highlighting logic
                    float opacityNext = partHighlight == -1 || partHighlight == (*nextPart).propId ?
                        (*nextPart).bgra[3] : highlightOpacity * 255.f;
                    bgra[3] = mix(bgra[3], opacityNext);

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

        // Disable depth test for proper alpha blending of 2D sprites
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

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
        // Apply part color (BGRA order like Eiton - swap B and R)
        newColor[0] *= bgra[2] / 255.f; // RED = bgra[2]
        newColor[1] *= bgra[1] / 255.f; // GREEN = bgra[1]
        newColor[2] *= bgra[0] / 255.f; // BLUE = bgra[0]
        newColor[3] *= bgra[3] / 255.f; // ALPHA = bgra[3]

        // Debug first part render only
        static bool debugOnce = true;
        if (debugOnce) {
            printf("[RENDER DEBUG] Part propId=%d, ppId=%d\n", part.propId, part.ppId);
            printf("  bgra input: (%d, %d, %d, %d)\n", part.bgra[0], part.bgra[1], part.bgra[2], part.bgra[3]);
            printf("  Final color to GL: (%.3f, %.3f, %.3f, %.3f)\n", 
                newColor[0], newColor[1], newColor[2], newColor[3]);
            debugOnce = false;
        }

        glVertexAttrib4fv(2, newColor);
        // Note: addColor BGR->RGB swap when passing to shader
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
        case ShapeType::ARC:
        {
            // Curved arc surface
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            vertexData.resize(size + 6 * outShape.vertexCount * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < 6; j++)
                {
                    point[j].x = float((outShape.radius - (1 - tX[j]) * outShape.width - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::sin(angle + delta * tY[j]));
                    point[j].y = -float((outShape.radius - (1 - tX[j]) * outShape.width - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::cos(angle + delta * tY[j]));
                    point[j].z = -float(outShape.dz * (i + tY[j])) / outShape.vertexCount;
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
        case ShapeType::SPHERE:
        {
            // Spherical surface
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            float angle2 = 0;
            float delta2 = glm::pi<float>() * outShape.length2 / 10000 / outShape.vertexCount2;
            vertexData.resize(size + 6 * outShape.vertexCount * outShape.vertexCount2 * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                angle2 = 0;
                for (int j = 0; j < outShape.vertexCount2; j++) {
                    for (int k = 0; k < 6; k++)
                    {
                        point[k].x = float((outShape.radius) * glm::sin(angle2 + delta2 * tX[k]) * glm::sin(angle + delta * tY[k]));
                        point[k].y = float((outShape.radius) * glm::sin(angle2 + delta2 * tX[k]) * glm::cos(angle + delta * tY[k]));
                        point[k].z = outShape.radius * glm::cos(angle2 + delta2 * tX[k]);
                        point[k].s = float(cutout.uv[0] + float(cutout.uv[2] * (i + tY[k])) / outShape.vertexCount) / width;
                        point[k].t = float(cutout.uv[1] + float(cutout.uv[3] * (j + tX[k]) / outShape.vertexCount2)) / height;
                        point[k].p = float(cutout.uv[0] + float(cutout.uv[2] * (outShape.vertexCount - i - tY[k])) / outShape.vertexCount) / width;
                        point[k].q = float(cutout.uv[1] + float(cutout.uv[3] * (outShape.vertexCount2 - j - tX[k]) / outShape.vertexCount2)) / height;
                    }
                    memcpy(&vertexData[size + 6 * 7 * (outShape.vertexCount2 * i + j)], point, sizeof(point));
                    angle2 += delta2;
                }
                angle += delta;
            }
            partVertices.Prepare(6 * 7 * outShape.vertexCount * outShape.vertexCount2 * sizeof(float), &vertexData[0]);
            break;
        }
        case ShapeType::CONE:
        {
            // Conical surface
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            vertexData.resize(size + 6 * outShape.vertexCount * outShape.vertexCount2 * 7);
            float w = float(outShape.radius) / outShape.vertexCount2;
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < outShape.vertexCount2; j++) {
                    for (int k = 0; k < 6; k++) {
                        point[k].x = -w * (outShape.vertexCount2 - 1 - j + tX[k]) * glm::sin(angle + delta * tY[k]);
                        point[k].y = -w * (outShape.vertexCount2 - 1 - j + tX[k]) * glm::cos(angle + delta * tY[k]);
                        point[k].z = -outShape.dz * float(j + (1 - tX[k])) / outShape.vertexCount2;
                        point[k].s = float(cutout.uv[0] + 1.0f * cutout.uv[2] * (i + tY[k]) / outShape.vertexCount) / width;
                        point[k].t = float(cutout.uv[1] + 1.0f * cutout.uv[3] * (1 + j - tX[k]) / outShape.vertexCount2) / height;
                        point[k].p = float(cutout.uv[0] + 1.0f * cutout.uv[2] * (outShape.vertexCount - i - tY[k]) / outShape.vertexCount) / width;
                        point[k].q = float(cutout.uv[1] + 1.0f * cutout.uv[3] * (outShape.vertexCount2 - 1 - j + tX[k]) / outShape.vertexCount2) / height;
                    }
                    memcpy(&vertexData[size + 6 * 7 * (outShape.vertexCount2 * i + j)], point, sizeof(point));
                }
                angle += delta;
            }
            partVertices.Prepare(6 * 7 * outShape.vertexCount * outShape.vertexCount2 * sizeof(float), &vertexData[0]);
            break;
        }
        default:
            continue;
        }

        // Render the part
        partVertices.Load();
        partVertices.Bind();
        DrawPart(part.ppId);
        partVertices.Clear();
    }
    
    // Restore depth test state
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
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

void Parts::SetHighlightOpacity(float opacity)
{
    highlightOpacity = opacity;
}
