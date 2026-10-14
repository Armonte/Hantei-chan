#ifndef PARTS_H_GUARD
#define PARTS_H_GUARD

#include "../cg.h"
#include "../texture.h"
#include "../vao.h"
#include "../enums.h"
#include "../framestate.h"
#include "parts_texture.h"
#include "parts_shape.h"
#include "parts_part.h"
#include "parts_partset.h"
#include "part_render_math.h"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <glm/mat4x4.hpp>

class Parts {
public:
    CG* cg = nullptr;
    class FrameState* currState = nullptr;  // For PatEditor state access
    enum RenderMode* renderMode = nullptr;   // For PatEditor render mode
    char* data = nullptr;
    std::string filePath;

    std::vector<PartSet<>> partSets;
    std::vector<CutOut<>> cutOuts;
    std::vector<Shape<>> shapes;
    std::vector<PartGfx<>> gfxMeta;
    std::vector<Texture*> textures;
    Vao partVertices;

    int curTexId = -1;
    bool loaded = false;
    // True if the loaded .pat used MBAACC-era legacy tag names (PANM, PPNM,
    // PPSS, PPCC, PPPA, PPPP, PPTP, PGTX). Save uses this to emit the same
    // tag variants back instead of the canonical UNI ones (PANA, PPNA, PPWH,
    // PPXY, PPCL, PPVT, PPGR, PGT2), preserving byte-1:1 with the original.
    bool useMBAACCFormat = false;
    // French-Bread HAN2RBO / GOF PAT (RBO, GOF2, GOF1): a part's position is its
    // top-left corner, the cut-out's xy is the pivot OFFSET from that corner
    // (scale and rotation about top-left + xy), and parts draw in ascending layer
    // order (last = on top). See docs/formats/fb_part_transform.md. Everything
    // else (MBAA / UNI / MBTL) keeps position = pivot and descending priority.
    bool fbPartModel = false;
    // French-Bread PAT: number of pose slots of the loaded block (1000 for v2/v3, 2000 for GOF2 v4); 0 = not a French-Bread PAT.
    int fbMaxPoses = 0;

    // Pixel buffers owned by this model (sprite sheets imported in the editor). Append-only until Free(), so a
    // PartGfx::data pointer kept by an undo snapshot stays valid for the life of the loaded character.
    std::vector<std::unique_ptr<char[]>> ownedPixels;
    char* AdoptPixels(size_t bytes) { ownedPixels.emplace_back(new char[bytes ? bytes : 1]); return ownedPixels.back().get(); }

    // Records as loaded, in file order, with a fingerprint of the model they
    // decoded to. Save() writes a record's original bytes while its model is
    // unchanged (so unmodified .pat files round-trip byte-identical, including
    // their texture compression) and re-encodes only edited records.
    struct RawRecord {
        char kind;          // 'P' P_ST, 'C' PPST, 'G' PGST, 'V' VEST
        uint32_t id;
        std::string raw;    // tag .. end tag
        uint64_t fp = 0;
    };
    std::vector<RawRecord> rawRecords;
    std::string rawHeader;  // 0x20-byte header + "_STR"
    uint64_t Fingerprint(char kind, uint32_t id) const;

    // PatEditor highlighting
    int partHighlight = -1;        // Index of highlighted part property (-1 = none)
    float highlightOpacity = 0.3f; // Opacity for non-highlighted parts

    // Constructor
    Parts(CG* cgRef);
    ~Parts();

    // Loading/Saving
    bool Load(const char* name);
    bool Save(const char* filename);
    unsigned int* MainLoad(unsigned int* data, const unsigned int* data_end);

    // Rendering. One part set, drawn the way uni2.exe's PatPartSet_Draw_PrimQueue 0x4BFFB0 does it
    // (rules in part_render_math.h).
    struct DrawParams {
        int pattern = 0;          // part set
        int nextPattern = -1;     // AFHK: part set of the next frame's same layer (-1 = none)
        float curWeight = 1.f;    // AFHK: weight of the current frame (1 = no interpolation)
        bool paletteTint = true;  // PPPA tint: the game applies it in the character shader path only
        bool applyPras = true;    // PRAS: UNI2 applies it, MBTL does not read it
    };
    // setAddColor gets the PRSP specular colour as RGBA 0..1 (added to the output, alpha included).
    void Draw(const DrawParams& params,
        std::function<void(glm::mat4)> setMatrix,
        std::function<void(const float*)> setAddColor,
        float color[4]);
    void DrawPart(int id, bool useLinearFilter = false);
    // PRAL blend a part draws with (the editor's on/off "additive" plus the raw byte).
    static partrender::PartBlend PartBlendOf(const PartProperty& part);

    // Accessors
    PartSet<>* GetPartSet(unsigned int n);
    PartProperty* GetPartProp(int partSetId, unsigned int propId);
    CutOut<>* GetCutOut(unsigned int n);
    PartGfx<>* GetPartGfx(unsigned int n);
    Shape<>* GetShape(unsigned int n);

    // UI helpers (for decorated combo box names)
    std::string GetPartSetDecorateName(int n);
    std::string GetPartPropsDecorateName(int p_id, int prop_id);
    std::string GetPartCutOutsDecorateName(int n);
    std::string GetShapesDecorateName(int n);
    std::string GetTexturesDecorateName(int n);

    // Management
    void Free();
    void initEmpty();
    void updateCGReference(CG* cgRef);
    void updatePatEditorReferences(class FrameState* state, enum RenderMode* mode);

    // PatEditor
    void SetHighlightOpacity(float opacity);
};

#endif /* PARTS_H_GUARD */
