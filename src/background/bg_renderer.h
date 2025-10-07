#ifndef BG_RENDERER_H_GUARD
#define BG_RENDERER_H_GUARD

#include "bg_file.h"
#include <unordered_map>
#include <glad/glad.h>

// Forward declare Render class from global namespace
class Render;

namespace bg {

// Handles rendering of background files
class Renderer {
public:
	Renderer();
	~Renderer();
	
	// Set current background file to render
	void SetFile(File* file);
	
	// Update animations (call once per frame at 60fps)
	void Update();
	
	// Render the background with camera parallax
	// Call this BEFORE drawing character sprites
	void Render(const Camera& camera, ::Render* mainRender);
	
	// Enable/disable rendering
	void SetEnabled(bool enabled) { this->enabled = enabled; }
	bool IsEnabled() const { return enabled; }

	// Debug visualization
	void SetShowDebugOverlay(bool show) { showDebugOverlay = show; }
	bool IsShowingDebugOverlay() const { return showDebugOverlay; }

	// Debug: disable parallax to see raw positions
	void SetParallaxEnabled(bool enable) { parallaxEnabled = enable; }
	bool IsParallaxEnabled() const { return parallaxEnabled; }
	
	// Clear all cached textures
	void ClearTextureCache();
	
private:
	File* file = nullptr;
	bool enabled = false;
	bool showDebugOverlay = false;
	bool parallaxEnabled = true;
	
	// Texture caching
	struct CachedTexture {
		GLuint textureId;
		int width;
		int height;
	};
	std::unordered_map<int, CachedTexture> textureCache;
	
	// Persistent VBO for quad rendering
	GLuint quadVBO = 0;

	// 1x1 white texture for debug lines
	GLuint whiteTexture = 0;
	
	// Get or create OpenGL texture for sprite
	GLuint GetOrCreateTexture(int spriteId, int& outWidth, int& outHeight);
	
	// Render a single object
	void RenderObject(const Object& obj, const Camera& camera, ::Render* mainRender);
	
	// Helper to draw a textured quad
	void DrawTexturedQuad(GLuint texture, float x, float y, int w, int h,
	                      float alpha, int blendMode, ::Render* mainRender);

	// Debug rendering helpers
	void DrawDebugOverlay(const Camera& camera, ::Render* mainRender);
	void DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, ::Render* mainRender);
};

} // namespace bg

#endif /* BG_RENDERER_H_GUARD */

