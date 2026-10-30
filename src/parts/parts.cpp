#include "parts.h"
#include "part_transform.h"
#include "part_render_math.h"
#include "../misc.h"
#include "../cg.h"
#include "../app_log.h"

#include <algorithm>
#include <iostream>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <filesystem>

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
        struct RecordEnd {
            Parts* self; const unsigned int* start; unsigned int*& cur; char kind; uint32_t id;
            ~RecordEnd() {
                if (kind) self->rawRecords.push_back({kind, id,
                    std::string((const char*)start, (const char*)cur - (const char*)start), 0});
            }
        } rec{this, buf, data, 0, 0};
        if (!memcmp(buf, "P_ST", 4)) { rec.kind = 'P'; rec.id = data[0]; }
        else if (!memcmp(buf, "PPST", 4)) { rec.kind = 'C'; rec.id = data[0]; }
        else if (!memcmp(buf, "PGST", 4)) { rec.kind = 'G'; rec.id = data[0]; }
        else if (!memcmp(buf, "VEST", 4)) { rec.kind = 'V'; }

        if (!memcmp(buf, "P_ST", 4)) {
            // PartSet
            unsigned int p_id = *data;
            ++data;
            while(partSets.size() <= p_id) {
                auto p = &partSets.emplace_back();
                p->partId = partSets.size() - 1;
                p->origId = p->partId;   // holes too: renumbering moves them like any slot
            }
            PartSet<>* partSet = &partSets[p_id];
            partSet->wasLoaded = true;
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
            HC_LOG_WARN("[Parts] unknown top level tag {}", tag);
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

    // The standalone-open dispatcher can receive truncated/untrusted files.
    // Validate the complete fixed header before either memcmp or +0x20 access.
    if (size < 0x24 || memcmp(loadData, "PAniDataFile", 12))
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
    rawRecords.clear();
    rawHeader.assign(loadData, 0x24);
    MainLoad(d + 1, d_end);
    for (auto& r : rawRecords)
        r.fp = Fingerprint(r.kind, r.id);
    diskIdOfOrig.resize(partSets.size());
    for (size_t i = 0; i < partSets.size(); i++) diskIdOfOrig[i] = (int)i;
    
    // Count non-empty part sets
    int nonEmptyPartSets = 0;
    for (auto& ps : partSets) {
        if (!ps.groups.empty()) nonEmptyPartSets++;
    }

    // Load textures into OpenGL (skipped in headless / no-GL builds).
#ifndef PAT_HEADLESS
    for (size_t idx = 0; idx < gfxMeta.size(); idx++)
    {
        auto& gfx = gfxMeta[idx];

        textures.push_back(new Texture);
        if (gfx.s3tc)
        {
            if (gfx.type == 21 || gfx.type == 22)
            {
                // Uncompressed BGRA (21 = A8R8G8B8; 22 = X8R8G8B8, whose 4th byte is not alpha: drawn opaque)
                std::vector<char> opaque;
                char *src = (char*)gfx.s3tc;
                if (gfx.type == 22) { opaque.assign(src, src + (size_t)gfx.w * gfx.h * 4); for (size_t k = 3; k < opaque.size(); k += 4) opaque[k] = (char)255; src = opaque.data(); }
                textures.back()->LoadDirect(src, gfx.w, gfx.h, true);
                // Default to GL_NEAREST; filtering will be applied per-part based on PRFL flag
                textures.back()->Apply(false, false);  // repeat=false, linearFilter=false
            }
            else
            {
                // Compressed (DXT1 or DXT5)
                size_t compressedSize;
                if (gfx.type == 5 || gfx.type == 3)
                    compressedSize = gfx.w * gfx.h;
                else if (gfx.type == 1)
                    compressedSize = (gfx.w * gfx.h) * 3 / 6;
                else
                    assert(0 && "Unknown compression type");

                textures.back()->LoadCompressed((char*)gfx.s3tc, gfx.w, gfx.h, compressedSize, gfx.type);
                // LoadCompressed creates texture internally, set default filtering
                // Actual filtering will be applied per-part based on PRFL flag in DrawPart()
                glBindTexture(GL_TEXTURE_2D, textures.back()->id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            }
            if (!gfx.dontDelete)
                delete[](gfx.s3tc - 128);
        }
        else if (gfx.data)
        {
            // PGTX data is also in BGRA format
            textures.back()->LoadDirect(gfx.data, gfx.w, gfx.h, true);
            // Default to GL_NEAREST; filtering will be applied per-part based on PRFL flag
            textures.back()->Apply(false, false);  // repeat=false, linearFilter=false
        }
        else
        {
            // No texture data - empty slot
        }

        // Validate texture was created before storing ID
        if (textures.back() && textures.back()->id != 0) {
            gfx.textureIndex = textures.back()->id;
        } else {
            gfx.textureIndex = 0;
        }
    }
#endif

    filePath = name;

    // Detect MBAACC vs UNI format by inspecting the loaded texture data.
    // MBAACC textures load via PGTX -> sets `data`, leaves `imageSize` at 0.
    // UNI textures load via PGT2 -> sets `s3tc` / `imageSize`.
    // For texture-less files we fall back to scanning the raw file for the
    // signature legacy tag PANM, which doesn't appear in UNI/MBTL pats.
    useMBAACCFormat = false;
    for (const auto& gfx : gfxMeta) {
        if (gfx.data != nullptr && gfx.imageSize == 0) {
            useMBAACCFormat = true;
            break;
        }
    }
    if (!useMBAACCFormat && this->data != nullptr) {
        unsigned int sz = size;
        const char* raw = this->data;
        for (unsigned int k = 0x20; k + 4 <= sz; ++k) {
            if (raw[k] == 'P' && raw[k+1] == 'A' && raw[k+2] == 'N' && raw[k+3] == 'M') {
                useMBAACCFormat = true;
                break;
            }
        }
    }

    // Auto-load .pal file if it exists (similar to how .txt loading auto-loads .pal for .cg)
    // This makes it easier to work with .pat files without manually loading palettes
    if (cg && !filePath.empty()) {
        auto extensionPos = filePath.find_last_of(".");
        if (extensionPos != std::string::npos) {
            auto palPath = filePath.substr(0, extensionPos) + ".pal";
            // Native file paths follow ReadInMem's ACP contract; filesystem's
            // narrow-path UTF-8 conversion may throw for an accented filename.
            std::ifstream paletteFile(palPath, std::ios::binary);
            if (paletteFile) {
                paletteFile.close();
                if (!cg->loadPalette(palPath.c_str()))
                    HC_LOG_WARN("[Parts] failed to auto-load palette {}", palPath);
            }
        }
    }

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
    fbPartModel = false;
    fbMaxPoses = 0;
    ownedPixels.clear();
    diskIdOfOrig.clear();
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

std::vector<int> Parts::CurrentIndexOfOrig() const
{
    std::vector<int> cur(diskIdOfOrig.size(), -1);
    for (size_t j = 0; j < partSets.size(); j++) {
        const int o = partSets[j].origId;
        if (o < 0) continue;
        if (o >= (int)cur.size()) cur.resize(o + 1, -1);
        cur[o] = (int)j;
    }
    return cur;
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

// ---------------------------------------------------------------------------------------------------------------
// Drawing. Follows uni2.exe's part path (PatPartSet_Draw_PrimQueue 0x4BFFB0 -> PatPart_SetupDraw_PrimQueue
// 0x415020 -> PatPart_DrawPrimitive_DispatchByShapeType 0x4C06D0); the per-rule references are in
// part_render_math.h.

void Parts::DrawPart(int i, bool useLinearFilter, int textureSlot)
{
    // Safety: Don't render if Parts is being freed or unloaded
    if (!loaded || gfxMeta.empty()) {
        return;
    }

    // textureSlot >= 0: the texture / UV views draw a whole sheet, whatever cut-out i holds (#111: UNI2/MBTL cut-out 0
    // is often the empty "〇" entry with texture -1, and the sheet stayed blank).
    int slot = textureSlot;
    if (slot < 0) {
        if (i < 0 || i >= (int)cutOuts.size()) {
            HC_LOG_WARN_ONCE("[DrawPart] invalid cut-out index {} (size {})", i, cutOuts.size());
            return;
        }
        slot = cutOuts[i].texture;
    }

    if (slot < 0 || slot >= (int)gfxMeta.size()) {
        HC_LOG_WARN_ONCE("[DrawPart] invalid texture index {} (textures {})", slot, gfxMeta.size());
        return;
    }

    // Texture not created (empty slot or failed upload): skip.
    if (gfxMeta[slot].textureIndex == 0) {
        HC_LOG_WARN_ONCE("[DrawPart] texture {} has no GL id", slot);
        return;
    }

    // Determine which texture to use
    curTexId = gfxMeta[slot].textureIndex;

    // Override texture in TEXTURE_VIEW mode (show currently selected texture)
    if (renderMode && currState && *renderMode == RenderMode::TEXTURE_VIEW && !currState->animating) {
        auto gfx = GetPartGfx(currState->partGraph);
        if (gfx != nullptr && gfx->textureIndex != 0) {
            curTexId = gfx->textureIndex;
        }
    }

    // Override texture in UV_SETTING_VIEW mode (show texture from current cutout)
    if (renderMode && currState && *renderMode == RenderMode::UV_SETTING_VIEW && !currState->animating) {
        auto cutoutSelected = GetCutOut(currState->partCutOut);
        if (cutoutSelected != nullptr) {
            auto gfx = GetPartGfx(cutoutSelected->texture);
            if (gfx != nullptr && gfx->textureIndex != 0) {
                curTexId = gfx->textureIndex;
            }
        }
    }

    // (No glIsTexture validation here: it forces a GPU sync per part per frame,
    // and the textureIndex != 0 guard above already covers missing textures.)
    while (glGetError() != GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_2D, curTexId);

    // PRFL: exactly 1 = linear, anything else = point (D3D_SetTexFilterFromTable via PRAL_PRFL globals).
    const GLint filter = useLinearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        HC_LOG_WARN_ONCE("[DrawPart] GL error 0x{:X} after binding texture {}", (unsigned)err, curTexId);
    }

    partVertices.Draw(0);
}

namespace {
// Blend state for one part (PRAL). Leaves the equation at ADD for every mode except subtract.
void ApplyPartBlend(partrender::PartBlend mode)
{
    switch (mode) {
    case partrender::kBlendAdd:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case partrender::kBlendSub:
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    default:
        // Normal, and PRAL 3. uni2 1.42 draws character / effect .pat layers through the shader path
        // (Han6Draw_DrawPatLayerParts_3D 0x4BF9A0 -> PatPart_SetBlendState_PRAL: 3 -> normal), which has no blur.
        // The CharaPtBlur mask passes (sh_paniblur.txt, CharaPtBlur_BeginMaskTarget 0x87FBA0 /
        // CharaPtBlur_DrawBlurWH_Passes 0x87F9A0) are queued only by the non-shader 2D path
        // (Han6Draw_DrawPatLayerParts_2D 0x4C01D0). Measured in game (render-match17, chr020 set 991 slots forced
        // to PRAL 3 vs PRAL 0): the same image.
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

int ToByte01(float v) { return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f); }
}

partrender::PartBlend Parts::PartBlendOf(const PartProperty& part)
{
    // The editor's on/off view of PRAL plus the raw byte (2 and 3 are distinct modes, not "on").
    if (!part.additive) return partrender::kBlendNormal;
    return partrender::BlendFromPral(part.pral ? part.pral : 1);
}

void Parts::Draw(const DrawParams& params,
    std::function<void(glm::mat4)> setMatrix,
    std::function<void(const float*)> setAddColor,
    float color[4])
{
    using namespace partrender;
    curTexId = -1;
    const int pattern = params.pattern;

    // Safety: Don't render if Parts is being freed or unloaded
    // The texture / UV views draw one full-texture part (below) whatever the part set holds: the PAT tab shows part
    // set 0 there, which is often empty (UNI2 chr000), and a file may have textures but no part sets or cut-outs yet.
    const bool sheetView = renderMode && currState && *renderMode != DEFAULT && !currState->animating;
    if (!loaded || ((partSets.empty() || cutOuts.empty()) && !sheetView)) {
        HC_LOG_WARN_ONCE("[Parts::Draw] nothing to draw: loaded={} partSets={} cutOuts={}", loaded, partSets.size(), cutOuts.size());
        return;
    }

    if (!sheetView && (pattern < 0 || pattern >= (int)partSets.size())) {
        HC_LOG_WARN_ONCE("[Parts::Draw] invalid part set {} (count {})", pattern, partSets.size());
        return;
    }

    if (!sheetView && partSets[pattern].groups.empty()) {
        return;
    }

    // Draw order: highest priority first (see part_transform.h).
    auto copyGroups = sheetView ? decltype(partSets[0].groups){} : partSets[pattern].groups;
    partxf::SortForDraw(fbPartModel, copyGroups);

    constexpr int tX[] = { 0,1,1, 1,0,0 };
    constexpr int tY[] = { 0,0,1, 1,1,0 };
    const int *tXd = tX, *tYd = tY;
    // The games queue every ring / arc / sphere / cone segment as one 4-vertex prim with UVs (u0,v0) (u1,v0) (u0,v1)
    // (u1,v1) (uni2 1.42 CPAniPrimRender_DrawQuads 0x4C0F10 and the shape classes' corner getters) and draw it split
    // along (u1,v0)-(u0,v1). Their segments are not parallelograms, so the split shows in the texture (render-match8:
    // the 3-segment arc of chr019 p153, 29.45 -> 0.28). kSplit* is that split for shapes mapping u to tX and v to tY
    // (arc). Both games' arcs use it: UNI2 measured in render-match8; MBTL's arc code is the same (MBTL 25646393
    // CPAniPrimRender_DrawQuads 0x4A3480 + RenderBatch_AppendQuad_Idx012_321 0x40DBA0) and, measured with the
    // footprint-independent registration (v2_model --exact-reg), flipV p157 goes 5.11 -> 4.62 (render-match10).
    // MBAACC and the ring / sphere stay on the default split (no measured case isolates them).
    constexpr int kSplitX[] = { 0,1,0, 1,1,0 };
    constexpr int kSplitY[] = { 0,0,1, 0,1,1 };

    struct {
        float x, y, z, s, t, p, q;
    } point[6];

    // UVs are in 1/256 of the texture (the tag-format UV unit).
    constexpr float uvUnit = 256.f;

    // TEXTURE_VIEW/UV_SETTING_VIEW: one dummy part showing the whole texture.
    const bool rawTextureView = renderMode && currState && *renderMode != DEFAULT && !currState->animating;
    if (rawTextureView) {
        copyGroups.clear();
        auto prop = &copyGroups.emplace_back();
        prop->ppId = 0;
    }

    // AFHK: the next frame's part set. Parts pair up by slot (PRST id), like the game's
    // `next->slots[slot]` lookup in PatPartSet_Draw_PrimQueue.
    const float t = params.curWeight;
    const PartSet<>* nextSet = nullptr;
    if (!rawTextureView && t < 1.f && params.nextPattern >= 0 && params.nextPattern < (int)partSets.size() &&
        params.nextPattern != pattern)
        nextSet = &partSets[params.nextPattern];

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    for (auto& part : copyGroups)
    {
        // Skip unused or invalid parts silently (the sheet view's dummy part needs no cut-out)
        const bool validCut = part.ppId >= 0 && part.ppId < (int)cutOuts.size();
        if (!validCut && !rawTextureView) {
            continue;
        }

        auto cutout = validCut ? cutOuts[part.ppId] : CutOut<>{};

        // PatEditor: Override cutout for texture/UV viewing modes
        auto partGfx = GetPartGfx(currState ? currState->partGraph : 0);
        auto cutoutSelected = GetCutOut(currState ? currState->partCutOut : 0);
        if (renderMode && currState && *renderMode == UV_SETTING_VIEW && cutoutSelected != nullptr) {
            partGfx = GetPartGfx(cutoutSelected->texture);
        }
        if (rawTextureView && partGfx != nullptr) {
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

        if (cutout.texture < 0 || cutout.texture >= (int)gfxMeta.size()) {
            continue;
        }

        // MBAACC .pat files have no shapes: every cut-out is an implicit plane.
        Shape currentShape;
        if (rawTextureView || shapes.empty() || cutout.shapeIndex < 0 || cutout.shapeIndex >= (int)shapes.size()) {
            currentShape.type = ShapeType::PLANE;
        } else {
            currentShape = shapes[cutout.shapeIndex];
        }

        // FB parts store the quad's top-left; the engine pivots scale and rotation about it + the cut-out
        // origin (see part_transform.h). The quad below is drawn at -xy, so the matrix origin is the pivot.
        glm::vec2 offset = partxf::Pivot(fbPartModel, (float)part.x, (float)part.y, cutout);
        glm::vec3 rotation = glm::vec3(part.rotation[1], part.rotation[2], part.rotation[3]);
        glm::vec2 scale = glm::vec2(part.scaleX, part.scaleY);
        glm::vec2 pras = params.applyPras ? glm::vec2((float)part.pras[0], (float)part.pras[1]) : glm::vec2(0.f);

        // Opacity handling: use full opacity in special render modes or when part is highlighted
        const bool fullOpacity = rawTextureView || partHighlight == -1 || partHighlight == part.propId;
        int bgra[4] = { part.bgra[0], part.bgra[1], part.bgra[2], part.bgra[3] };
        int spec[4] = { ToByte01(part.addColor[0]), ToByte01(part.addColor[1]), ToByte01(part.addColor[2]),
                        ToByte01(part.addColor[3]) };

        Shape outShape = currentShape;

        // Prevent crash on invalid shapes. Must be continue, not return: one degenerate part must not drop
        // every remaining part.
        if(outShape.type != ShapeType::PLANE && outShape.type != ShapeType::RECT &&
            outShape.vertexCount == 0)
            continue;
        if((outShape.type == ShapeType::SPHERE || outShape.type == ShapeType::CONE) &&
           outShape.vertexCount2 == 0)
            continue;

        // AFHK: lerp toward the same slot of the next frame's part set (PatPart_BuildTransformMatrix 0x4C0EC0,
        // PatPart_LerpDiffuseColor_PRCL 0x413880, PatPart_ComputeSpecularAddColor_PRSP 0x413AE0,
        // PrimRect_Setup 0x413E80). The cut-out, texture and blend stay the current frame's.
        if (nextSet && part.propId >= 0 && part.propId < (int)nextSet->groups.size()) {
            const PartProperty& np = nextSet->groups[part.propId];
            if (np.ppId >= 0 && np.ppId < (int)cutOuts.size()) {
                const auto& nextCutout = cutOuts[np.ppId];
                const glm::vec2 nextOffset = partxf::Pivot(fbPartModel, (float)np.x, (float)np.y, nextCutout);
                // MBAA.exe skips the transform lerp at weight 0 and draws the current part (PartLerpsTransform).
                if (partrender::PartLerpsTransform(t, params.uniDialect)) {
                    offset.x = Lerp(offset.x, nextOffset.x, t);
                    offset.y = Lerp(offset.y, nextOffset.y, t);
                    rotation[0] = LerpTurns(rotation[0], np.rotation[1], t);
                    rotation[1] = LerpTurns(rotation[1], np.rotation[2], t);
                    rotation[2] = LerpTurns(rotation[2], np.rotation[3], t);
                    scale.x = Lerp(scale.x, np.scaleX, t);
                    scale.y = Lerp(scale.y, np.scaleY, t);
                    if (params.applyPras) {
                        pras.x = Lerp(pras.x, (float)np.pras[0], t);
                        pras.y = Lerp(pras.y, (float)np.pras[1], t);
                    }
                }
                for (int c = 0; c < 4; c++)
                    bgra[c] = LerpByte(bgra[c], np.bgra[c], t);
                if (t == 0.f) {
                    for (int c = 0; c < 4; c++) spec[c] = ToByte01(np.addColor[c]);
                } else {
                    // In between, the game rebuilds the specular colour from RGB only (alpha 0).
                    for (int c = 0; c < 3; c++) spec[c] = LerpByte(spec[c], ToByte01(np.addColor[c]), t);
                    spec[3] = 0;
                }

                if (nextCutout.shapeIndex >= 0 && nextCutout.shapeIndex < (int)shapes.size()) {
                    const Shape<>& nextShape = shapes[nextCutout.shapeIndex];
                    if (nextShape.type == currentShape.type) {
                        auto mixi = [t](int a, int b) { return a + (int)((float)(b - a) * (1.f - t)); };
                        if (currentShape.type == ShapeType::RECT) {
                            outShape.rectW = mixi(outShape.rectW, nextShape.rectW);
                            outShape.rectH = mixi(outShape.rectH, nextShape.rectH);
                        } else if ((int)currentShape.type > 2) {
                            outShape.dRadius = mixi(outShape.dRadius, nextShape.dRadius);
                            outShape.dz = mixi(outShape.dz, nextShape.dz);
                            outShape.length = mixi(outShape.length, nextShape.length);
                            outShape.length2 = mixi(outShape.length2, nextShape.length2);
                            outShape.radius = mixi(outShape.radius, nextShape.radius);
                            outShape.width = mixi(outShape.width, nextShape.width);
                        }
                    }
                }
            }
        }

        // Part matrix (uni2.exe PatPart_BuildTransformMatrix): scale -> PRAS -> rotZ -> rotX -> rotY ->
        // translate, all angles positive turns. MBTL does not read PRAS.
        glm::mat4 view = partxf::PartMatrix(offset, rotation, pras, scale);
        setMatrix(view);

        ApplyPartBlend(rawTextureView ? kBlendNormal : PartBlendOf(part));

        // Diffuse = layer colour x PRCL; the palette colour slot (PPPA) tints RGB in the character shader
        // (PS_CharaPtDrawV: out.rgb *= palette[PPPA].rgb). Slot 0 = no tint: MBAA's PAT loader
        // (File_ParsePPTPChunks 0x404220) has no colour-slot tag, and palette entry 0 is the transparent
        // key colour (tinting by it turned white material blue, EX report: Linne pattern 135).
        // The tint reads the base <cg>.pal of the selected colour even while the pattern's PUPS bank is in
        // effect (partrender::PartTintPalette; MBTL chr011 pattern 309 halo, slot 248).
        float newColor[4];
        memcpy(newColor, color, sizeof(float) * 4);
        const CG* tintCg = partrender::PartTintOwner(params.tintCg, (const CG*)cg);
        if (params.paletteTint && cutout.colorSlot > 0 && cutout.colorSlot < 256 && tintCg) {
            const unsigned *tintPal = partrender::PartTintPalette(tintCg->paletteAt(tintCg->paletteNumber(), 0), tintCg->getPalettePtr());
            unsigned int palColor = tintPal ? tintPal[cutout.colorSlot] : 0;
            if (palColor) {
                float rgb[3];
                partrender::PaletteEntryRgb(palColor, rgb);
                newColor[0] *= rgb[0];
                newColor[1] *= rgb[1];
                newColor[2] *= rgb[2];
            }
        }
        newColor[0] *= bgra[2] / 255.f; // R
        newColor[1] *= bgra[1] / 255.f; // G
        newColor[2] *= bgra[0] / 255.f; // B
        newColor[3] *= (fullOpacity ? (float)bgra[3] : highlightOpacity * 255.f) / 255.f;

        glVertexAttrib4fv(2, newColor);
        // PRSP specular: stored R,G,B,A in addColor[0..3]; the shader adds it to the output, alpha included.
        const float addRgba[4] = { spec[0] / 255.f, spec[1] / 255.f, spec[2] / 255.f, spec[3] / 255.f };
        setAddColor(addRgba);

        // PRRV mirrors the UV rectangle only (PatPart_GetCutoutUVWithFlip 0x413C50); geometry is unchanged.
        const UvRect uvr = FlipUv(cutout.uv[0], cutout.uv[1], cutout.uv[2], cutout.uv[3], rawTextureView ? 0 : part.flip);
        auto uvS = [&](float f) { return (uvr.u + uvr.w * f) / uvUnit; };
        auto uvT = [&](float f) { return (uvr.v + uvr.h * f) / uvUnit; };
        // Plane / rect quads carry the games' per-corner UV bias (partrender::PartUvBiasU/V); not in the raw texture view.
        auto uvSq = [&](float f) { return uvS(f) + (rawTextureView ? 0.f : partrender::PartUvBiasU(f)); };
        auto uvTq = [&](float f) { return uvT(f) + (rawTextureView ? 0.f : partrender::PartUvBiasV(f)); };

        std::vector<float> vertexData;

        switch (outShape.type)
        {
        case ShapeType::PLANE:
        {
            // Quad hanging at -PPCC, PPSS in size.
            for (int i = 0; i < 6; i++)
            {
                point[i].x = -cutout.xy[0] + cutout.wh[0] * tX[i];
                point[i].y = -cutout.xy[1] + cutout.wh[1] * tY[i];
                point[i].z = 0;
                point[i].s = point[i].p = uvSq((float)tX[i]);
                point[i].t = point[i].q = uvTq((float)tY[i]);
            }
            partVertices.Prepare(6 * 7 * sizeof(float), &point[0]);
            break;
        }
        case ShapeType::RECT:
        {
            // Shape type 2 (CPAniPrimRender_Rect): a quad at (x, y) of w x h from the shape itself, not the
            // cut-out's PPCC/PPSS.
            for (int i = 0; i < 6; i++)
            {
                point[i].x = (float)(outShape.rectX + outShape.rectW * tX[i]);
                point[i].y = (float)(outShape.rectY + outShape.rectH * tY[i]);
                point[i].z = 0;
                point[i].s = point[i].p = uvSq((float)tX[i]);
                point[i].t = point[i].q = uvTq((float)tY[i]);
            }
            partVertices.Prepare(6 * 7 * sizeof(float), &point[0]);
            break;
        }
        // Shape z is the game's own sign (uni2 1.42 CPAniPrimRender_Ring 0x414120 / _Donuts 0x414590 / _Sphere 0x414A60 /
        // _Corn 0x414DF0: dz steps and the sphere's start vertex (0, 0, -r) are in +z = away from the eye, the
        // left-handed part space of partrender::PartPerspective). Before the perspective was made left-handed these
        // were stored negated to make up for it.
        case ShapeType::RING:
        {
            // Cylindrical ring
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            vertexData.resize(6 * outShape.vertexCount * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < 6; j++)
                {
                    point[j].x = float(outShape.radius - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::sin(angle + delta * tY[j]);
                    point[j].y = -float(outShape.radius - ((i + tY[j] == outShape.vertexCount && outShape.length == 10000) ? 0 : float(outShape.dRadius * (i + tY[j])) / outShape.vertexCount)) * glm::cos(angle + delta * tY[j]);
                    point[j].z = float(outShape.width * (tX[j] * 2 - 1) + float(outShape.dz * (i + tY[j])) / outShape.vertexCount);
                    point[j].s = point[j].p = uvS((float)tX[j]);
                    point[j].t = point[j].q = uvT(1.0f * (i + tY[j]) / outShape.vertexCount);
                }
                angle += delta;
                memcpy(&vertexData[6 * 7 * i], point, sizeof(point));
            }
            partVertices.Prepare(6 * 7 * outShape.vertexCount * sizeof(float), &vertexData[0]);
            break;
        }
        case ShapeType::ARC:
        {
            // Curved arc surface (game corners: inner-cur, outer-cur, inner-next, outer-next; split as kSplit* in
            // UNI2 and MBTL, the uniDialect games). MBAACC keeps the default split (not traced or measured).
            const int *tX = params.uniDialect ? kSplitX : tXd, *tY = params.uniDialect ? kSplitY : tYd;
            float angle = 0;
            float delta = glm::pi<float>() * outShape.length / 5000 / outShape.vertexCount;
            vertexData.resize(6 * outShape.vertexCount * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < 6; j++)
                {
                    // Geometric shrink of both edges (partrender::ArcEdgeScale, uni2 0x414590 / 0x4148D0). A closed
                    // ring's last edge reuses the unshrunk start points, keeping the previous edge's z.
                    const int e = i + tY[j];
                    const bool closing = e == outShape.vertexCount && outShape.length == 10000;
                    const float k = closing ? 1.f : partrender::ArcEdgeScale(outShape.radius, outShape.dRadius, outShape.vertexCount, e);
                    const float r = (outShape.radius - (1 - tX[j]) * outShape.width) * k;
                    point[j].x = r * glm::sin(angle + delta * tY[j]);
                    point[j].y = -r * glm::cos(angle + delta * tY[j]);
                    point[j].z = float(outShape.dz * (closing ? e - 1 : e)) / outShape.vertexCount;
                    point[j].s = point[j].p = uvS((float)tX[j]);
                    point[j].t = point[j].q = uvT(1.0f * (i + tY[j]) / outShape.vertexCount);
                }
                angle += delta;
                memcpy(&vertexData[6 * 7 * i], point, sizeof(point));
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
            vertexData.resize(6 * outShape.vertexCount * outShape.vertexCount2 * 7);
            for (int i = 0; i < outShape.vertexCount; i++) {
                angle2 = 0;
                for (int j = 0; j < outShape.vertexCount2; j++) {
                    for (int k = 0; k < 6; k++)
                    {
                        point[k].x = float((outShape.radius) * glm::sin(angle2 + delta2 * tX[k]) * glm::sin(angle + delta * tY[k]));
                        point[k].y = float((outShape.radius) * glm::sin(angle2 + delta2 * tX[k]) * glm::cos(angle + delta * tY[k]));
                        point[k].z = -outShape.radius * glm::cos(angle2 + delta2 * tX[k]);
                        point[k].s = point[k].p = uvS(float(i + tY[k]) / outShape.vertexCount);
                        point[k].t = point[k].q = uvT(float(j + tX[k]) / outShape.vertexCount2);
                    }
                    memcpy(&vertexData[6 * 7 * (outShape.vertexCount2 * i + j)], point, sizeof(point));
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
            vertexData.resize(6 * outShape.vertexCount * outShape.vertexCount2 * 7);
            float w = float(outShape.radius) / outShape.vertexCount2;
            for (int i = 0; i < outShape.vertexCount; i++) {
                for (int j = 0; j < outShape.vertexCount2; j++) {
                    for (int k = 0; k < 6; k++) {
                        point[k].x = -w * (outShape.vertexCount2 - 1 - j + tX[k]) * glm::sin(angle + delta * tY[k]);
                        point[k].y = -w * (outShape.vertexCount2 - 1 - j + tX[k]) * glm::cos(angle + delta * tY[k]);
                        point[k].z = outShape.dz * float(j + (1 - tX[k])) / outShape.vertexCount2;
                        point[k].s = point[k].p = uvS(1.0f * (i + tY[k]) / outShape.vertexCount);
                        point[k].t = point[k].q = uvT(1.0f * (1 + j - tX[k]) / outShape.vertexCount2);
                    }
                    memcpy(&vertexData[6 * 7 * (outShape.vertexCount2 * i + j)], point, sizeof(point));
                }
                angle += delta;
            }
            partVertices.Prepare(6 * 7 * outShape.vertexCount * outShape.vertexCount2 * sizeof(float), &vertexData[0]);
            break;
        }
        default:
            continue;
        }

        if(params.observeGeometry && !rawTextureView && cutout.texture>=0 &&
           cutout.texture<int(textures.size()) && textures[cutout.texture] &&
           gfxMeta[cutout.texture].textureIndex!=0 && (newColor[3]>0 || spec[3]>0)) {
            const float* submitted=vertexData.empty()?reinterpret_cast<const float*>(point):vertexData.data();
            const size_t count=vertexData.empty()?6:vertexData.size()/7;
            params.observeGeometry(submitted,count,view,*textures[cutout.texture],spec[3]>0);
        }
        partVertices.Load();
        partVertices.Bind();
        // PRFL: exactly 1 draws linear; other non-zero bytes draw point like the game.
        DrawPart(part.ppId, part.filter && (part.prfl == 0 || part.prfl == 1), rawTextureView ? cutout.texture : -1);
        partVertices.Clear();
    }

    // Restore depth test and the default blend
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    // Built in memory and written with WriteFileAtomic (temp file + rename, as the HA6 save): a failed or interrupted
    // save leaves the previous .pat untouched.
    std::ostringstream file(std::ios_base::out | std::ios_base::binary);

    // Write header (as loaded when possible)
    if (rawHeader.size() == 0x24) {
        file.write(rawHeader.data(), rawHeader.size());
    } else {
        char header[32] = "PAniDataFile";
        file.write(header, sizeof(header));
        file.write("_STR", 4);
    }

    auto encode = [&](char kind, uint32_t i) {
        std::ostringstream os(std::ios_base::out | std::ios_base::binary);
        switch (kind) {
        case 'P':
            os.write("P_ST", 4); os.write(VAL(i), 4);
            PartSet<>::Save(os, &partSets[i], useMBAACCFormat);
            os.write("P_ED", 4);
            break;
        case 'C':
            os.write("PPST", 4); os.write(VAL(i), 4);
            CutOut<>::Save(os, &cutOuts[i], useMBAACCFormat);
            os.write("PPED", 4);
            break;
        case 'G':
            os.write("PGST", 4); os.write(VAL(i), 4);
            PartGfx<>::Save(os, &gfxMeta[i]);
            os.write("PGED", 4);
            break;
        case 'V': {
            int paramSize = 16;
            uint32_t shapeCount = (uint32_t)shapes.size();
            os.write("VEST", 4);
            os.write(VAL(shapeCount), 4);
            os.write(VAL(paramSize), 4);
            for (uint32_t k = 0; k < shapes.size(); k++)
                Shape<>::Save(os, &shapes[k], false);
            os.write("VNST", 4);
            for (uint32_t k = 0; k < shapes.size(); k++)
                Shape<>::Save(os, &shapes[k], true);
            os.write("VEED", 4);
            break;
        }
        }
        return os.str();
    };
    auto exists = [&](char kind, uint32_t i) {
        switch (kind) {
        case 'P': return i < partSets.size();
        case 'C': return i < cutOuts.size();
        case 'G': return i < gfxMeta.size();
        default: return true;
        }
    };

    // Part sets: a loaded slot finds its record through origId. When renumbering moved slots (pat_renumber.h), every
    // part set record is written as one ascending block where the first one was, a moved record with its original
    // bytes and only the P_ST id changed.
    const std::vector<int> curOfOrig = CurrentIndexOfOrig();
    bool renumbered = false;
    for (size_t o = 0; o < curOfOrig.size(); o++) if (curOfOrig[o] != (int)o) renumbered = true;
    for (size_t j = 0; j < partSets.size(); j++) if (partSets[j].origId < 0 && PartSet<>::IsModifiedData(&partSets[j])) renumbered = true;
    std::vector<int> rawOfOrig;
    for (size_t k = 0; k < rawRecords.size(); k++)
        if (rawRecords[k].kind == 'P') {
            if (rawOfOrig.size() <= rawRecords[k].id) rawOfOrig.resize(rawRecords[k].id + 1, -1);
            if (rawOfOrig[rawRecords[k].id] < 0) rawOfOrig[rawRecords[k].id] = (int)k;
        }
    auto rawOf = [&](size_t j) { const int o = partSets[j].origId; return o >= 0 && o < (int)rawOfOrig.size() ? rawOfOrig[o] : -1; };

    // Loaded records in their original order: original bytes while the
    // model is unchanged, re-encoded otherwise.
    std::vector<uint8_t> doneP(partSets.size()), doneC(cutOuts.size()), doneG(gfxMeta.size());
    bool doneV = false, blockP = false;
    for (const auto& r : rawRecords) {
        uint32_t rid = r.id;
        if (r.kind == 'P') {
            const int j = r.id < curOfOrig.size() ? curOfOrig[r.id] : -1;
            if (j < 0) continue;   // deleted by renumbering
            if (renumbered) {
                if (blockP) continue;
                blockP = true;
                for (size_t k = 0; k < partSets.size(); k++) {
                    const int ri = rawOf(k);
                    if (ri < 0 && !PartSet<>::IsModifiedData(&partSets[k])) continue;
                    doneP[k] = 1;
                    if (ri >= 0 && Fingerprint('P', (uint32_t)k) == rawRecords[ri].fp) {
                        std::string raw = rawRecords[ri].raw; const uint32_t id = (uint32_t)k;
                        memcpy(&raw[4], &id, 4);
                        file.write(raw.data(), raw.size());
                    } else {
                        const std::string e = encode('P', (uint32_t)k); file.write(e.data(), e.size());
                    }
                }
                continue;
            }
            rid = (uint32_t)j;   // == r.id: not renumbered
        }
        if (!exists(r.kind, rid)) continue;
        switch (r.kind) {
        case 'P': if (doneP[rid]) continue; doneP[rid] = 1; break;
        case 'C': if (doneC[rid]) continue; doneC[rid] = 1; break;
        case 'G': if (doneG[rid]) continue; doneG[rid] = 1; break;
        case 'V': if (doneV) continue; doneV = true; break;
        }
        if (Fingerprint(r.kind, rid) == r.fp)
            file.write(r.raw.data(), r.raw.size());
        else {
            const std::string e = encode(r.kind, rid);
            file.write(e.data(), e.size());
        }
    }

    // Records created in the editor.
    for (uint32_t i = 0; i < partSets.size(); i++)
        if (!doneP[i] && (partSets[i].wasLoaded || PartSet<>::IsModifiedData(&partSets[i]))) {
            const std::string e = encode('P', i); file.write(e.data(), e.size());
        }
    for (uint32_t i = 0; i < cutOuts.size(); i++)
        if (!doneC[i] && CutOut<>::IsModifiedData(&cutOuts[i])) {
            const std::string e = encode('C', i); file.write(e.data(), e.size());
        }
    // MBAACC files have no VEST block (MBAA.exe has no shapes); don't add one, also not to a New PAT in that layout.
    if (!doneV && !useMBAACCFormat && (rawRecords.empty() || !shapes.empty())) {
        const std::string e = encode('V', 0); file.write(e.data(), e.size());
    }
    for (uint32_t i = 0; i < gfxMeta.size(); i++)
        if (!doneG[i] && PartGfx<>::IsModifiedData(&gfxMeta[i])) {
            const std::string e = encode('G', i); file.write(e.data(), e.size());
        }

    file.write("_END", 4);
    const std::string bytes = file.str();
    if (!WriteFileAtomic(filename, bytes.data(), bytes.size())) return false;
    for (size_t o = 0; o < diskIdOfOrig.size(); o++) diskIdOfOrig[o] = o < curOfOrig.size() ? curOfOrig[o] : -1;
    return true;
}

void Parts::SetHighlightOpacity(float opacity)
{
    highlightOpacity = opacity;
}

// FNV-1a over the model of one record (see RawRecord in parts.h).
namespace {
struct Fnv {
    uint64_t h = 1469598103934665603ull;
    void add(const void* p, size_t n) {
        const unsigned char* c = (const unsigned char*)p;
        for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; }
    }
    template<class T> void v(const T& x) { add(&x, sizeof(x)); }
    void str(const std::string& s) { size_t n = s.size(); v(n); add(s.data(), n); }
};
}

uint64_t Parts::Fingerprint(char kind, uint32_t id) const
{
    Fnv f;
    f.v(kind);
    switch (kind) {
    case 'P': {
        if (id >= partSets.size()) return 0;
        const auto& ps = partSets[id];
        f.str(std::string(ps.name.data(), ps.name.size()));
        size_t n = ps.groups.size(); f.v(n);
        for (const auto& pr : ps.groups) {
            f.v(pr.propId); f.v(pr.priority); f.add(pr.rotation, sizeof(pr.rotation)); f.add(pr.pras, sizeof(pr.pras));
            f.v(pr.x); f.v(pr.y); f.v(pr.scaleX); f.v(pr.scaleY); f.v(pr.ppId); f.v(pr.additive); f.v(pr.filter);
            f.v(pr.pral); f.v(pr.prfl); f.v(pr.prpa); f.v(pr.hasPrpa); f.v(pr.flip);
            f.add(pr.bgra, sizeof(pr.bgra)); f.add(pr.addColor, sizeof(pr.addColor));
        }
        break;
    }
    case 'C': {
        if (id >= cutOuts.size()) return 0;
        const auto& c = cutOuts[id];
        f.str(std::string(c.name.data(), c.name.size()));
        f.add(c.uv, sizeof(c.uv)); f.add(c.xy, sizeof(c.xy)); f.add(c.wh, sizeof(c.wh)); f.v(c.texture);
        f.add(c.ppjp, sizeof(c.ppjp)); f.v(c.colorSlot); f.v(c.shapeIndex); f.add(c.ppte, sizeof(c.ppte));
        f.v(c.pptx); f.v(c.altTags); f.v(c.nameFixed);
        break;
    }
    case 'G': {
        if (id >= gfxMeta.size()) return 0;
        const auto& g = gfxMeta[id];
        f.str(std::string(g.name.data(), g.name.size()));
        f.v(g.w); f.v(g.h); f.v(g.bpp); f.v(g.type); f.add(g.pgte, sizeof(g.pgte)); f.v(g.pgtp);
        f.v(g.noCompress); f.add(g.ddsHeader, sizeof(g.ddsHeader)); f.v(g.imageSize);
        if (g.imageData && g.imageSize > 0) f.add(g.imageData, (size_t)g.imageSize);
        if (g.data && g.imageSize == 0 && g.w > 0 && g.h > 0) f.add(g.data, (size_t)g.w * g.h * (g.bpp / 8));
        break;
    }
    case 'V': {
        size_t n = shapes.size(); f.v(n);
        for (const auto& sh : shapes) {
            f.str(std::string(sh.name.data(), sh.name.size()));
            f.v(sh.type); f.v(sh.radius); f.v(sh.width); f.v(sh.vertexCount); f.v(sh.vertexCount2);
            f.v(sh.length); f.v(sh.length2); f.v(sh.dz); f.v(sh.dRadius);
            f.v(sh.rectW); f.v(sh.rectH); f.v(sh.rectX); f.v(sh.rectY);
        }
        break;
    }
    }
    return f.h;
}
