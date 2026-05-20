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

namespace bg {

class Renderer {
public:
	Renderer();
	~Renderer();

	// Current file being rendered (not owned).
	void   SetFile(File* f);
	File*  GetFile() const { return file; }

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
	File* file              = nullptr;
	bool  enabled           = false;
	bool  paused            = false;
	bool  parallaxEnabled   = true;
	bool  showDebugOverlay  = false;
	int   selectedObjIndex  = -1;

	// GL objects we own.
	GLuint program          = 0;
	GLuint vbo              = 0;
	GLint  uProjView        = -1;
	GLint  uTexture         = -1;
	bool   glInit           = false;

	// Texture cache: spriteId -> {texture, w, h}.
	struct Tex { GLuint id; int w; int h; };
	std::unordered_map<int, Tex> textureCache;

	void   InitGL();
	GLuint GetOrCreateTexture(int spriteId, int& outW, int& outH);

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
