#ifndef RENDER_H_GUARD
#define RENDER_H_GUARD

#include "cg.h"
#include "texture.h"
#include "shader.h"
#include "vao.h"
#include "hitbox.h"
#include <vector>
#include <unordered_map>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

// Forward declarations
class Parts;

// Layer information for multi-layer rendering
struct RenderLayer {
	int spriteId;
	int spawnOffsetX, spawnOffsetY;  // Offset from spawn parameters
	int frameOffsetX, frameOffsetY;  // Offset from frame AF data
	float scaleX, scaleY;
	float rotX, rotY, rotZ;
	bool AFRT;             // Rotation order flag: false=X→Y→Z, true=Z→X→Y
	int blendMode;
	int zPriority;         // Z-priority for sorting (from AF.priority)
	float alpha;
	glm::vec4 tintColor;
	bool isSpawned;
	BoxList hitboxes;  // Hitboxes for this layer
	CG* sourceCG;          // CG to pull sprite from (for effect.ha6 support)

	// Spawn flags for positioning behavior
	int spawnFlagset1;     // From effect parameters[2]
	int spawnFlagset2;     // From effect parameters[3]

	RenderLayer() :
		spriteId(-1), spawnOffsetX(0), spawnOffsetY(0),
		frameOffsetX(0), frameOffsetY(0),
		scaleX(1.0f), scaleY(1.0f),
		rotX(0.0f), rotY(0.0f), rotZ(0.0f),
		AFRT(false),
		blendMode(0), zPriority(0),
		alpha(1.0f), tintColor(1.0f, 1.0f, 1.0f, 1.0f),
		isSpawned(false), sourceCG(nullptr),
		spawnFlagset1(0), spawnFlagset2(0) {}
};

class Render
{
private:
	glm::mat4 projection, view;

	CG *cg;
	Parts *m_parts;
	Vao vSprite;
	Vao vGeometry;
	Shader sPartShader;
	enum{
		LINES = 0,
		BOXES,
		GEO_SIZE
	};
	int geoParts[GEO_SIZE];
	float imageVertex[6*4];
	std::vector<float> clientQuads;
	int quadsToDraw;

	int lProjectionS, lProjectionT, lProjectionParts;
	int lAlphaS;
	int lFlipParts, lAddColorParts;
	Shader sSimple;
	Shader sTextured;
	Texture texture;
	float colorRgba[4];

	int curImageId;
	bool usePat = false;
	int curPattern = 0;
	int curNextPattern = 0;
	float curInterp = 0.0f;

	// Multi-layer rendering support
	std::vector<RenderLayer> renderLayers;
	int currentLayerIndex;

	void AdjustImageQuad(int x, int y, int w, int h);
	void SetModelView(glm::mat4&& view);
	void SetMatrix(int location);
	void SetBlendingMode();

public:
	bool filter;
	int x, offsetX;
	int y, offsetY;
	float scale;
	float scaleX, scaleY;
	float rotX, rotY, rotZ;
	bool AFRT = false;  // Rotation order flag: false=X→Y→Z, true=Z→X→Y
	int highLightN = -1;
	
	Render();
	void Draw();
	void DrawGridLines();   // Draw only grid lines
	void DrawSpriteOnly();  // Draw sprite without lines/boxes
	void UpdateProj(float w, float h);

	void GenerateHitboxVertices(const BoxList &hitboxes);
	void SetCg(CG *cg);
	void SetParts(Parts *parts);
	void SwitchImage(int id);
	void DontDraw();
	void ClearTexture();
	void SetImageColor(float *rgbaArr);

	// Multi-layer rendering
	void ClearLayers();
	void AddLayer(const RenderLayer& layer);
	void SortLayersByZPriority(int mainPatternPriority);
	void DrawLayers();
	bool HasLayers() const { return !renderLayers.empty(); }

	enum blendType{
		normal,
		additive,
		subtractive
	};

	blendType blendingMode;
};

#endif /* RENDER_H_GUARD */
