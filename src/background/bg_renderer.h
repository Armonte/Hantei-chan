// Faithful port of u4ick's bgmaketool MonoForm.Draw + supporting state.
// Renders MBAACC bgmake .dat backgrounds with parallax, per-sprite blend
// modes, and layer-based depth ordering — owns its own shader, vertex
// buffer, and depth state instead of borrowing the host editor's
// SpriteTransform helpers (which couldn't carry per-vertex z for the
// depth test that drives u4ick's layer ordering).
#ifndef BG_RENDERER_H_GUARD
#define BG_RENDERER_H_GUARD

#include "bg_file.h"
#include <glad/glad.h>
#include <unordered_map>
#include <memory>

class Render;
class Parts;

namespace bg {

// Which part of the stage to draw. The game draws band 0 (objhdr+21 == 0)
// at render priority 10 (behind the fighters), then the DropObj weather at
// priority 522, then band 1 at 600 (in front of the fighters) —
// Background_DrawAllInstances 0x4b8f80. The host calls Back before and Front
// after its character layers; All draws both (stage-only previews).
enum class Pass { All, Back, Front };

class Renderer {
public:
	Renderer();
	~Renderer();

	// Current file being rendered (not owned).
	void   SetFile(File* f);
	File*  GetFile() const { return file; }

	// Host editor renderer — used to draw PAT-pattern stage objects
	// (sprite-id < 10000) through the editor's Parts pipeline. Not owned.
	void   SetHostRender(::Render* r) { hostRender = r; }

	// Per-frame animation tick. Skipped when paused.
	void   Update();

	// Draw all objects in `file` into the current GL framebuffer using the
	// viewport (clientW, clientH). Camera state is read live every call.
	void   Render(const Camera& camera, int clientW, int clientH, Pass pass = Pass::All);

	// Toggles & state.
	void   SetEnabled(bool v)            { enabled = v; }
	bool   IsEnabled() const             { return enabled; }
	void   SetPaused(bool v)             { paused = v; }
	bool   IsPaused() const              { return paused; }
	void   SetParallaxEnabled(bool v)    { parallaxEnabled = v; }
	bool   IsParallaxEnabled() const     { return parallaxEnabled; }
	void   SetShowDebugOverlay(bool v)   { showDebugOverlay = v; }
	bool   IsShowingDebugOverlay() const { return showDebugOverlay; }
	void   SetSelectedObject(int i)      { selectedObjIndex = i; }
	int    GetSelectedObject() const     { return selectedObjIndex; }
	void   SetShowWeather(bool v)        { showWeather = v; }
	bool   IsShowingWeather() const      { return showWeather; }
	void   SetShowLights(bool v)         { showLights = v; }
	bool   IsShowingLights() const       { return showLights; }
	// Debug self-capture of the stage viewport to C:/dev/bg_dump.png
	// (off by default; armed on demand from the inspector).
	void   RequestDebugDump(int frames = 2) { dumpCountdown = frames; }
	// Game-accurate textures (bg_gametex.h): pow2 textures composed like
	// MBAA, DXT5 on over-budget stages, the game's UV insets. On by default;
	// off shows the clean CG (for editing).
	void   SetGameTextures(bool v)       { if (v != gameTextures) { gameTextures = v; ClearTextureCache(); } }
	bool   IsGameTextures() const        { return gameTextures; }
	// Budget of the loaded stage and whether the game would use DXT5.
	long   GetTextureBudgetKB();
	bool   IsDxtStage()                  { return GetTextureBudgetKB() > 30000; }
	void   ClearTextureCache();

private:
	// State.
	File*     file          = nullptr;
	::Render* hostRender    = nullptr;
	bool  enabled           = false;
	bool  paused            = false;
	bool  parallaxEnabled   = true;
	bool  showDebugOverlay  = true;  // u4ick draws them unconditionally.
	bool  showWeather       = true;
	bool  showLights        = true;
	int   selectedObjIndex  = -1;
	GLuint whiteTex         = 0;     // 1x1 white, for untextured lines
	GLuint dropTex          = 0;     // DropObj bitmap (sakura00.bmp)
	int    dropTexW = 0, dropTexH = 0;
	bool   dropTexTried     = false;
	GLint  uTint = -1, uAdd = -1, uAlphaLoc = -1;

	// GL objects we own.
	GLuint program          = 0;
	GLuint vbo              = 0;
	GLint  uProjView        = -1;
	GLint  uTexture         = -1;
	bool   glInit           = false;

	// Texture cache: spriteId -> {texture, w, h, contentOriginX, contentOriginY}.
	// originX/Y come from ImageData::offsetX/Y (= the sprite's bounds_x1/y1
	// in the CG canvas). u4ick draws the full sprite canvas at the bg
	// position; our cg lib returns just the tight bounded region, so we add
	// (origin_x, origin_y) to the draw position to compensate. Without this
	// step, sprites whose content origin shifts frame-to-frame (a fire
	// animation does this even though w/h are fixed) appeared to jitter.
	struct Tex { GLuint id; int w; int h; int originX; int originY; int texW; int texH; };
	bool   gameTextures = true;
	long   budgetKB = -1;
	std::unordered_map<int, Tex> textureCache;

	// The embedded older-PAT, converted into the editor's Parts model so it
	// can be drawn by the proven orthographic part renderer
	// (Render::DrawBgPattern). Built lazily on first render after SetFile.
	std::unique_ptr<Parts> patParts;
	bool                   patPartsBuilt = false;

	// Debug: when > 0, counts down each Render(); on reaching 0 the bg
	// viewport is read back and written to C:/dev/bg_dump.png so the actual
	// rendered output can be inspected. Re-armed by SetFile.
	int                    dumpCountdown = 0;

	void   InitGL();
	GLuint GetOrCreateTexture(int spriteId, int& outW, int& outH,
	                          int& outOriginX, int& outOriginY);

	// Convert file->GetOldPat() into `patParts` (uploads PAT textures to GL).
	void   BuildPatParts();

	// Draw one older-PAT pattern as flat quads in this renderer's own ortho
	// (the bg has NO perspective — g_D3DMatrix_Projection in MBAA.exe is a
	// pure translate — so PAT objects render in the same flat space as the
	// CG objects). Reads cutout/part/texture data from `patParts`.
	// `next`/`t`: frame +20 scale (and MBAC rotation) interpolation toward
	// the next frame's pattern (null = none). objLinear = objhdr+22.
	void   DrawPatPatternFlat(const PatPattern& pat, const PatPattern* next, float t,
	                          float worldX, float worldY, bool objLinear, bool mbac);
	// Weather particles (DropObject_RenderWithBloom) and light markers.
	void   DrawWeather(const Camera& camera);
	void   DrawLights(const Camera& camera);
	void   LoadDropTexture();
	// Low-level quad/line emitters in stage-screen space.
	void   EmitQuad(GLuint tex, const float xy[8], const float uv[8],
	                const float rgba[4], bool linear);
	void   EmitLine(float x0, float y0, float x1, float y1, float width,
	                const float rgba0[4], const float rgba1[4]);
	void   DrawPass(const Camera& camera, int clientW, int clientH, int band);

	// Build the orthographic projection that u4ick uses:
	// Matrix.CreateOrthographicOffCenter(0, W, H, 0, 0, 1).
	void   BuildProjView(int clientW, int clientH, float zoom, float pmat[16]);

	// Submit one sprite quad with all positions / blend / depth set up.
	void   DrawSprite(int spriteId,
	                  float x, float y,
	                  float w, float h,
	                  float alpha, int blendMode,
	                  float tintRGB, bool linear);
};

} // namespace bg

#endif // BG_RENDERER_H_GUARD
