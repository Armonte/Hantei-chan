#include "bg_renderer.h"
#include "../render.h"
#include "../texture.h"
#include "../vao.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace bg {

Renderer::Renderer() {
	// Create persistent VBO for quad rendering
	glGenBuffers(1, &quadVBO);

	// Create 1x1 white texture for debug lines
	glGenTextures(1, &whiteTexture);
	glBindTexture(GL_TEXTURE_2D, whiteTexture);
	unsigned char white[4] = {255, 255, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

Renderer::~Renderer() {
	ClearTextureCache();
	if (quadVBO) {
		glDeleteBuffers(1, &quadVBO);
	}
	if (whiteTexture) {
		glDeleteTextures(1, &whiteTexture);
	}
}

void Renderer::SetFile(File* file) {
	if (this->file != file) {
		// New file - clear texture cache
		ClearTextureCache();
	}
	this->file = file;
}

void Renderer::Update() {
	if (!enabled || !file || paused) return;
	file->UpdateAnimations();
}

void Renderer::Render(const Camera& camera, ::Render* mainRender) {
	if (!enabled || !file || !mainRender) {
		return;
	}

	CG* cg = file->GetCG();
	if (!cg || !cg->m_loaded) {
		return;
	}

	// Two-pass blending (mirrors u4ick's MonoForm.Draw's non-FullBlend path
	// at MonoForm.cs:280-419): first pass draws blendMode != 2 with custom
	// blend func, then a second pass draws additive (blendMode == 2) on top.
	// Within each pass we keep file order; FullBlend mode (single sorted
	// pass) is intentionally not implemented yet since u4ick exposes it
	// only as an opt-in "full blend" checkbox.
	auto& objects = file->GetObjects();

	for (const auto& obj : objects) {
		if (obj.frames.empty()) continue;
		const Frame& fr = obj.frames[obj.currentFrame];
		if (fr.blendMode == 2) continue;
		RenderObject(obj, camera, mainRender);
	}
	for (const auto& obj : objects) {
		if (obj.frames.empty()) continue;
		const Frame& fr = obj.frames[obj.currentFrame];
		if (fr.blendMode != 2) continue;
		RenderObject(obj, camera, mainRender);
	}

	if (showDebugOverlay) {
		DrawDebugOverlay(camera, mainRender);
	}
}

void Renderer::RenderObject(const Object& obj, const Camera& camera, ::Render* mainRender) {
	if (obj.frames.empty()) {
		return;
	}

	const Frame& frame = obj.frames[obj.currentFrame];

	// Match u4ick's render-time skip: frames with duration=0 and aniType=1
	// (loop) in a multi-frame object are transient placeholders that the
	// loop logic advances past. Rendering them for the one tick they're
	// 'current' produces a visible flicker between loop iterations.
	// See bgmaketool/MonoForm.cs:211-214.
	if (frame.duration == 0 && frame.aniType == 1 && obj.frames.size() > 1)
		return;

	if (frame.spriteId < 0) {
		return;
	}

	int spriteW, spriteH;
	GLuint texId = GetOrCreateTexture(frame.spriteId, spriteW, spriteH);
	if (texId == 0) {
		return;
	}

	// Position math ported from MonoForm.cs lines 253-254 / 334-335 / 400-401:
	//   x = panLast.X + (pan.X - panLast.X) * parallaxFactor + offsetX
	//   y = panLast.Y + (pan.Y - panLast.Y) * parallaxFactor + offsetY
	// Parallax only contributes during a live drag (the pan/panLast delta);
	// at rest the formula collapses to `panLast + offset`.
	int para = parallaxEnabled ? obj.parallax : 256;
	float screenX = camera.ScreenX((float)frame.offsetX, para);
	float screenY = camera.ScreenY((float)frame.offsetY, para);

	// Alpha: bgmake convention (MonoForm.cs:262) — when draw_type > 0 use
	// opacity directly; otherwise fully opaque. The previous code used a
	// special "opacity == 0 means fully opaque" rule which only matched part
	// of the original behavior.
	float alpha = (frame.blendMode > 0) ? (frame.opacity / 255.0f) : 1.0f;

	DrawTexturedQuad(texId, screenX, screenY, spriteW, spriteH,
	                 alpha, frame.blendMode, mainRender);
}

GLuint Renderer::GetOrCreateTexture(int spriteId, int& outWidth, int& outHeight) {
	// Check cache
	auto it = textureCache.find(spriteId);
	if (it != textureCache.end()) {
		outWidth = it->second.width;
		outHeight = it->second.height;
		return it->second.textureId;
	}
	
	// Get sprite from CG
	CG* cg = file->GetCG();
	if (!cg) return 0;
	
	ImageData* sprite = cg->draw_texture(spriteId, false, false);
	if (!sprite) {
		return 0;
	}
	
	// Create OpenGL texture
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
	             sprite->width, sprite->height,
	             0, GL_RGBA, GL_UNSIGNED_BYTE,
	             sprite->pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	// Cache it
	CachedTexture cached;
	cached.textureId = texId;
	cached.width = sprite->width;
	cached.height = sprite->height;
	textureCache[spriteId] = cached;
	
	outWidth = sprite->width;
	outHeight = sprite->height;
	
	delete sprite;  // ImageData destructor cleans up pixels
	
	return texId;
}

void Renderer::DrawTexturedQuad(GLuint texture, float x, float y, int w, int h,
                                 float alpha, int blendMode, ::Render* mainRender) {
	// CRITICAL: Disable depth writes so background doesn't occlude character
	// Background should be purely cosmetic and always behind gameplay elements
	glDepthMask(GL_FALSE);

	// Set up shader and projection
	mainRender->SetupSpriteShader();
	// EXACT bgmake: native size, no scaling
	mainRender->SetSpriteTransform(x, y, 1.0f, 1.0f);
	
	// Set blend mode
	if (blendMode == 2) {  // Additive
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	} else {  // Normal
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	// Build vertex data: position (x,y), UV (u,v) - 4 floats per vertex, 6 vertices
	// Positions relative to 0,0 since transform is in SetSpriteTransform
	float fw = (float)w;
	float fh = (float)h;
	float vertices[24] = {
		// Triangle 1
		0.0f, 0.0f, 0.0f, 0.0f,  // Top-left
		fw,   0.0f, 1.0f, 0.0f,  // Top-right
		fw,   fh,   1.0f, 1.0f,  // Bottom-right
		// Triangle 2
		fw,   fh,   1.0f, 1.0f,  // Bottom-right
		0.0f, fh,   0.0f, 1.0f,  // Bottom-left
		0.0f, 0.0f, 0.0f, 0.0f,  // Top-left
	};
	
	// Ensure we're using texture unit 0
	glActiveTexture(GL_TEXTURE0);

	// Bind texture
	glBindTexture(GL_TEXTURE_2D, texture);

	// Use persistent VBO for quad rendering (avoids creating/deleting VBOs every frame)
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

	// Set up vertex attributes
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Set color with alpha
	float colorRgba[4] = {1.0f, 1.0f, 1.0f, alpha};
	glDisableVertexAttribArray(2);
	glVertexAttrib4fv(2, colorRgba);
	
	// Draw
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// DON'T unbind VBO here - it causes GL_INVALID_OPERATION in OpenGL 2.1
	// The next renderer (Vao::Bind) will bind its own VBO anyway

	// CRITICAL: Unbind texture so character renderer uses its own texture
	glBindTexture(GL_TEXTURE_2D, 0);

	// Re-enable depth writes for subsequent rendering
	glDepthMask(GL_TRUE);

	// Reset blend mode
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::ClearTextureCache() {
	for (auto& pair : textureCache) {
		glDeleteTextures(1, &pair.second.textureId);
	}
	textureCache.clear();
}

void Renderer::DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, ::Render* mainRender) {
	// Draw a colored quad line (1px thickness)
	float dx = x2 - x1;
	float dy = y2 - y1;
	float len = std::sqrt(dx*dx + dy*dy);
	if (len < 0.1f) return;

	// Perpendicular vector for line thickness (1px)
	float thickness = 1.0f;
	float px = -dy / len * thickness;
	float py = dx / len * thickness;

	// Four corners of the quad
	float vertices[24] = {
		// Triangle 1
		x1 - px, y1 - py, 0.0f, 0.0f,
		x2 - px, y2 - py, 1.0f, 0.0f,
		x2 + px, y2 + py, 1.0f, 1.0f,
		// Triangle 2
		x2 + px, y2 + py, 1.0f, 1.0f,
		x1 + px, y1 + py, 0.0f, 1.0f,
		x1 - px, y1 - py, 0.0f, 0.0f
	};

	// Disable depth test so lines always draw on top
	glDisable(GL_DEPTH_TEST);

	mainRender->SetupSpriteShader();
	// Debug overlay at 1:1 scale (coordinates are in world space, not sprite space)
	mainRender->SetSpriteTransform(0, 0, 1.0f, 1.0f);

	// Use 1x1 white texture for solid color lines (like bgmake)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, whiteTexture);

	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Set color with vertex attribute (same method as sprite rendering)
	float colorRgba[4] = {r, g, b, a};
	glDisableVertexAttribArray(2);
	glVertexAttrib4fv(2, colorRgba);

	// Debug: print first line color
	static int debugCount = 0;
	if (debugCount < 1) {
		//printf("[Debug Line] Color RGBA: (%.2f, %.2f, %.2f, %.2f) WhiteTex=%u\n", r, g, b, a, whiteTexture);
		debugCount++;
	}

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Re-enable depth test
	glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawDebugOverlay(const Camera& camera, ::Render* mainRender) {
	if (!file) return;

	// Reference markers from u4ick's MonoForm.cs:433-435 — relative to the
	// pan anchor (panLastX/Y). The 1057x810 play area, the y=224 floor line,
	// and the y=272 ground-thickness line, all measured from x=-401 / y=0.
	float ax = camera.panLastX;
	float ay = camera.panLastY;

	// Floor line (yellow) at character feet level (y = ay).
	DrawLine(ax - 401, ay, ax - 401 + 1057, ay,
	         1.0f, 1.0f, 0.0f, 1.0f, mainRender);
	// Play area rectangle (purple).
	DrawLine(ax - 401, ay - 538, ax - 401 + 1057, ay - 538,
	         0.6f, 0.0f, 0.8f, 0.8f, mainRender);
	DrawLine(ax - 401, ay + 272, ax - 401 + 1057, ay + 272,
	         0.6f, 0.0f, 0.8f, 0.8f, mainRender);

	auto& objects = file->GetObjects();
	for (size_t objIdx = 0; objIdx < objects.size(); objIdx++) {
		const auto& obj = objects[objIdx];
		if (obj.frames.empty()) continue;

		const Frame& frame = obj.frames[obj.currentFrame];
		if (frame.spriteId < 0) continue;

		int spriteW, spriteH;
		GLuint texId = GetOrCreateTexture(frame.spriteId, spriteW, spriteH);
		if (texId == 0) continue;

		int para = parallaxEnabled ? obj.parallax : 256;
		float worldX = camera.ScreenX((float)frame.offsetX, para);
		float worldY = camera.ScreenY((float)frame.offsetY, para);

		// Native size (no scaling)
		float scaledW = (float)spriteW;
		float scaledH = (float)spriteH;

		// Check if this is the selected object
		bool isSelected = (selectedObjIndex >= 0 && (int)objIdx == selectedObjIndex);

		// Color based on selection status
		float r, g, b, a;
		if (isSelected) {
			// Selected object: bright yellow, fully opaque
			r = 1.0f; g = 1.0f; b = 0.0f; a = 1.0f;
		} else if (obj.layer > 128) {
			// Foreground: blue
			r = 0.5f; g = 0.5f; b = 1.0f; a = 0.5f;
		} else {
			// Background: green
			r = 0.0f; g = 1.0f; b = 0.5f; a = 0.5f;
		}

		// Draw bounding box at same scale as rendered sprite
		DrawLine(worldX, worldY, worldX + scaledW, worldY, r, g, b, a, mainRender);
		DrawLine(worldX + scaledW, worldY, worldX + scaledW, worldY + scaledH, r, g, b, a, mainRender);
		DrawLine(worldX + scaledW, worldY + scaledH, worldX, worldY + scaledH, r, g, b, a, mainRender);
		DrawLine(worldX, worldY + scaledH, worldX, worldY, r, g, b, a, mainRender);

		// Draw origin point for selected object only (magenta crosshair)
		if (isSelected) {
			DrawLine(worldX - 10, worldY, worldX + 10, worldY, 1.0f, 0.0f, 1.0f, 1.0f, mainRender);
			DrawLine(worldX, worldY - 10, worldX, worldY + 10, 1.0f, 0.0f, 1.0f, 1.0f, mainRender);
		}
	}
}

} // namespace bg

