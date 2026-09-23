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
	void   Render(const Camera& camera, int clientW, int clientH);

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
	void   ClearTextureCache();

private:
	// State.
	File*     file          = nullptr;
	::Render* hostRender    = nullptr;
	bool  enabled           = false;
	bool  paused            = false;
	bool  parallaxEnabled   = true;
	bool  showDebugOverlay  = true;  // u4ick draws them unconditionally.
	int   selectedObjIndex  = -1;

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
	struct Tex { GLuint id; int w; int h; int originX; int originY; };
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
	void   DrawPatPatternFlat(int pattern, float worldX, float worldY,
	                          float alpha, int frameBlend);

	// Build the orthographic projection that u4ick uses:
	// Matrix.CreateOrthographicOffCenter(0, W, H, 0, 0, 1).
	void   BuildProjView(int clientW, int clientH, float zoom, float pmat[16]);

	// Submit one sprite quad with all positions / blend / depth set up.
	void   DrawSprite(int spriteId,
	                  float x, float y,
	                  float w, float h,
	                  float alpha, int blendMode,
	                  float layerDepth);
};

} // namespace bg

#endif // BG_RENDERER_H_GUARD
