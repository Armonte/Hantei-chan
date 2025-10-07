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
	if (!enabled || !file) return;
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

	// Get all objects and sort by layer (lower = back)
	auto& objects = file->GetObjects();
	//printf("[BG Render] Rendering %zu objects, camera=(%.0f,%.0f)\n",
	//       objects.size(), camera.x, camera.y);
	
	std::vector<const Object*> sortedObjs;
	for (const auto& obj : objects) {
		sortedObjs.push_back(&obj);
	}
	
	std::sort(sortedObjs.begin(), sortedObjs.end(),
		[](const Object* a, const Object* b) {
			return a->layer < b->layer;
		});
	
	// Render each object in layer order
	for (const Object* obj : sortedObjs) {
		RenderObject(*obj, camera, mainRender);
	}

	// Draw debug overlay if enabled
	if (showDebugOverlay) {
		DrawDebugOverlay(camera, mainRender);
	}
}

void Renderer::RenderObject(const Object& obj, const Camera& camera, ::Render* mainRender) {
	if (obj.frames.empty()) {
		return;
	}

	const Frame& frame = obj.frames[obj.currentFrame];

	// Debug: Print first few objects to check for offset variation
	static int debugFrameCount = 0;
	if (debugFrameCount < 20) {
		printf("[BG RenderObj] Obj frame=%d/%zu spriteId=%d offset=(%d,%d) aniType=%d\n",
		       obj.currentFrame, obj.frames.size()-1, frame.spriteId,
		       frame.offsetX, frame.offsetY, frame.aniType);
		debugFrameCount++;
	}

	if (frame.spriteId < 0) {
		//printf("  -> Skipped: invalid spriteId\n");
		return;  // No sprite
	}

	// Get sprite texture
	int spriteW, spriteH;
	GLuint texId = GetOrCreateTexture(frame.spriteId, spriteW, spriteH);
	if (texId == 0) {
		return;  // Failed to load texture
	}

	// Transform from bgmake coordinates to Hantei world space:
	// - bgmake: character reference at screen (401, 538), draws at (401 + x_off, 538 + y_off)
	// - bgmake: floor at Y=224, so character is 314px below floor (538 - 224 = 314)
	// - Hantei: floor at Y=0, so character reference at Y=314
	// - bgmake uses top-left anchor (XNA default), so we do the same
	float worldX = (float)frame.offsetX;
	float worldY = 314.0f + (float)frame.offsetY;

	//printf("  -> frameOff=(%d,%d) worldPos=(%.0f,%.0f) parallax=%d (ignored for now)\n",
	//       frame.offsetX, frame.offsetY, worldX, worldY, obj.parallax);

	// bgmake doesn't scale sprites - uses native size
	float scaledW = spriteW;
	float scaledH = spriteH;
	
	// Calculate alpha
	float alpha = frame.opacity / 255.0f;

	// CRITICAL: Treat opacity=0 as fully opaque (bgmake convention)
	// In MBAA .dat files, 0 means "no transparency effect applied" = fully visible
	if (frame.opacity == 0) {
		alpha = 1.0f;
	}

	// Skip drawing if alpha is very low (performance optimization)
	if (alpha < 0.01f && frame.opacity != 0) {
		return;
	}

	// Draw the sprite in world coordinates (shader handles viewport transform)
	DrawTexturedQuad(texId, worldX, worldY, (int)scaledW, (int)scaledH,
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

	// Simple debug overlay - just show key reference points

	// Floor line (yellow) at character's feet (Y=0 in editor)
	float floorY = 0.0f;
	DrawLine(-500, floorY, 500, floorY, 1.0f, 1.0f, 0.0f, 1.0f, mainRender);

	// Character position crosshair (red) at origin (0,0)
	DrawLine(-20, 0, 20, 0, 1.0f, 0.0f, 0.0f, 1.0f, mainRender);
	DrawLine(0, -20, 0, 20, 1.0f, 0.0f, 0.0f, 1.0f, mainRender);

	// Draw object bounding boxes
	auto& objects = file->GetObjects();
	for (size_t objIdx = 0; objIdx < objects.size(); objIdx++) {
		const auto& obj = objects[objIdx];
		if (obj.frames.empty()) continue;

		const Frame& frame = obj.frames[obj.currentFrame];
		if (frame.spriteId < 0) continue;

		// Get sprite dimensions
		int spriteW, spriteH;
		GLuint texId = GetOrCreateTexture(frame.spriteId, spriteW, spriteH);
		if (texId == 0) continue;

		// World position (matching RenderObject transformation)
		float worldX = (float)frame.offsetX;
		float worldY = 314.0f + (float)frame.offsetY;

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

