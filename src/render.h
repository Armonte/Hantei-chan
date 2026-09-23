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

namespace bg {
	class Renderer;
	struct Camera;
}

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
	bool usePat;           // True if layer uses PAT rendering
	Parts* sourceParts;    // Parts to use (for PAT rendering, like sourceCG)

	// Spawn flags for positioning behavior
	int spawnFlagset1;     // From effect parameters[2]
	int spawnFlagset2;     // From effect parameters[3]

	// Pattern comparison overlay (issue #63): draw this layer's boxes as
	// outlines only, so they read apart from the main pattern's filled boxes.
	bool boxesOutlineOnly = false;

	RenderLayer() :
		spriteId(-1), spawnOffsetX(0), spawnOffsetY(0),
		frameOffsetX(0), frameOffsetY(0),
		scaleX(1.0f), scaleY(1.0f),
		rotX(0.0f), rotY(0.0f), rotZ(0.0f),
		AFRT(false),
		blendMode(0), zPriority(0),
		alpha(1.0f), tintColor(1.0f, 1.0f, 1.0f, 1.0f),
		isSpawned(false), sourceCG(nullptr),
		usePat(false), sourceParts(nullptr),
		spawnFlagset1(0), spawnFlagset2(0) {}
};

class Render
{
private:
	glm::mat4 projection, view;
	glm::mat4 perspective;  // Perspective projection for PAT rendering
	glm::mat4 invOrtho;     // Inverse orthographic for coordinate transforms

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
	bool gridLinesHaveOverlay = false;

	int lProjectionS, lProjectionT, lProjectionParts;
	int lAlphaS;
	int lFlipParts, lAddColorParts;
	int lIndexedT = -1;              //sTextured 'indexed' mode uniform
	unsigned int paletteTexId = 0;   //256x1 palette texture on unit 1
	Shader sSimple;
	Shader sTextured;

	// Sprite textures, cached per (CG, CG generation, image id). Decoding and
	// uploading a sprite on every switch made multi-actor scenes (spawns,
	// onion-skin samples, several views) re-upload the same images many
	// times per frame. LRU by byte budget; a CG load or palette change
	// renews its generation, so stale entries are never hit (only evicted).
	struct SpriteKey {
		const CG* cg; unsigned long long generation; int id;
		bool operator==(const SpriteKey& o) const { return cg == o.cg && generation == o.generation && id == o.id; }
	};
	struct SpriteKeyHash {
		size_t operator()(const SpriteKey& k) const {
			return std::hash<const void*>()(k.cg) ^ (std::hash<unsigned long long>()(k.generation) * 31u) ^ ((size_t)k.id * 0x9E3779B97F4A7C15ull);
		}
	};
	struct CachedSprite {
		unsigned int tex = 0;
		int w = 0, h = 0, ox = 0, oy = 0;
		bool indexed = false;
		bool linear = false;   // GL filter currently set on tex
		size_t bytes = 0;
		unsigned long long lastUse = 0;
	};
	std::unordered_map<SpriteKey, CachedSprite, SpriteKeyHash> spriteCache;
	size_t spriteCacheBytes = 0;
	unsigned long long spriteUseClock = 0;
	unsigned int spriteTex = 0;      // texture of the current sprite (0 = none)
	bool spriteIndexed = false;
	CachedSprite* curSprite = nullptr; // entry of spriteTex (map node, stable)
	const CG* curImageCg = nullptr;
	unsigned long long curImageGen = 0;
	void EvictSprites(size_t budget);
	void BindSpriteTexture();
	float colorRgba[4];

	//Set sTextured's indexed mode for the current sprite texture and, when
	//indexed, upload the CG's current palette. Call with sTextured active.
	void ApplySpriteTextureMode();

	//Per-item draws for DrawLayers. Caller sets x/y/offsetX/offsetY and the
	//GL baseline state; these must not rely on anything else being set.
	void DrawPatLayerItem(const RenderLayer& layer, Parts* origParts);
	void DrawCgLayerItem(const RenderLayer& layer, const float* baseColorRgba);

	int curImageId;

	// Multi-layer rendering support
	std::vector<RenderLayer> renderLayers;
	int currentLayerIndex;

	// Background (stage) rendering — borrowed from MainFrame, not owned here.
	bg::Renderer* bgRenderer = nullptr;
	bg::Camera*   bgCamera   = nullptr;

	void AdjustImageQuad(int x, int y, int w, int h);
	void SetModelView(glm::mat4&& view);
	void SetMatrix(int location);
	void SetMatrixPersp(int location, glm::mat4 view, glm::mat4 pre);  // For PAT perspective rendering
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

	// Parts rendering state (public for PatEditor)
	bool usePat = false;
	int curPattern = 0;
	int curNextPattern = 0;
	float curInterp = 0.0f;
	
	Render();
	~Render();
	Render(const Render&) = delete;
	Render& operator=(const Render&) = delete;
	size_t SpriteCacheEntries() const { return spriteCache.size(); }

	// Per-pass camera (docs/HANTEI_WAVE2.md §2). Every render pass (main
	// view, detached view, onion sample, PNG export) calls BeginPass with its
	// own target size and camera before drawing; x/y/scale/projection below
	// are derived from it and nothing carries over from a previous pass.
	struct PassParams {
		int width = 1, height = 1;         // target size in pixels
		float originX = 0.f, originY = 0.f; // target pixel of world (0,0)
		float zoom = 1.f;                  // target pixels per world unit
	};
	void BeginPass(const PassParams& params);
	const PassParams& CurrentPass() const { return pass; }
private:
	PassParams pass;
public:

	void Draw();
	void DrawGridLines();   // Draw only grid lines
	void ResetGridLines();  // Plain grid again (drops a PAT-editor overlay)
	void DrawSpriteOnly(bool drawHitboxes = true);  // Draw sprite and optionally hitboxes
	void UpdateProj(float w, float h);

	void GenerateHitboxVertices(const BoxList &hitboxes);
	bool GeneratePartCenterVertices();  // Draw origin cross for selected part in PatEditor
	bool GenerateUVRectangleVertices();  // Draw UV bounds rectangle for selected cutout in TEXTURE_VIEW
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

	// Background (stage) rendering.
	void SetBackgroundRenderer(bg::Renderer* renderer, bg::Camera* camera);
	void DrawBackground();

	// Draw one PAT pattern for a stage object whose sprite-id is < 10000.
	// (worldX, worldY) is the object's position in editor-world space
	// WITHOUT the camera pan (render.x/y supplies that). Used by the bg
	// Renderer for PAT-based stage objects.
	void DrawBgPattern(Parts* parts, int pattern,
	                   float worldX, float worldY, float alpha, int blendMode);
	bg::Renderer* GetBackgroundRenderer() { return bgRenderer; }
	bg::Camera*   GetBackgroundCamera()   { return bgCamera;   }

	// Hooks the background renderer uses to draw quads through our shader/state.
	void SetupSpriteShader();
	void SetSpriteTransform(float x, float y, float scaleX, float scaleY);

	enum blendType{
		normal,
		additive,
		subtractive
	};

	blendType blendingMode;
};

#endif /* RENDER_H_GUARD */
