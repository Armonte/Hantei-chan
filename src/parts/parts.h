#ifndef PARTS_H_GUARD
#define PARTS_H_GUARD

#include "../cg.h"
#include "../texture.h"
#include "../vao.h"
#include "parts_texture.h"
#include "parts_shape.h"
#include "parts_part.h"
#include "parts_partset.h"
#include <vector>
#include <functional>
#include <glm/mat4x4.hpp>

class Parts {
public:
    CG* cg = nullptr;
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

    // Rendering
    void Draw(int pattern, int nextPattern, float interpolationFactor,
        std::function<void(glm::mat4)> setMatrix,
        std::function<void(float, float, float)> setAddColor,
        std::function<void(char)> setFlip,
        float color[4]);
    void DrawPart(int id);

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

    // PatEditor
    void SetHighlightOpacity(float opacity);
};

#endif /* PARTS_H_GUARD */
