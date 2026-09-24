#include "render.h"
#include "main.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

//Error
#include <windows.h>
#include <glad/glad.h>
#include <sstream>

#include "hitbox.h"
#include "parts/parts.h"
#include "enums.h"  // For RenderMode enum
#include "background/bg_renderer.h"
#include "background/bg_types.h"

constexpr int maxBoxes = 33;

const char* simpleSrcVert = R"(
#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Color;

out vec4 Frag_Color;

uniform mat4 ProjMtx;
uniform float Alpha;

void main()
{
    
    Frag_Color = vec4(Color, Alpha);
    gl_Position = ProjMtx * vec4(Position, 1);
};
)";

const char* simpleSrcFrag = R"(
#version 330 core

in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    FragColor = Frag_Color;
};
)";

const char* texturedSrcVert = R"(
#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;

out vec2 Frag_UV;
out vec4 Frag_Color;

uniform mat4 ProjMtx;

void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
};
)";

const char* texturedSrcFrag = R"(
#version 330 core
uniform sampler2D Texture;
uniform sampler2D Palette;   // 256x1 RGBA, current CG palette
uniform int indexed;         // 0 = direct RGBA, 1 = indexed nearest, 2 = indexed + bilinear

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

// Palette lookup for an 8bpp index texture (index stored as R/255).
vec4 palLookup(vec2 uv)
{
    float idx = texture(Texture, uv).r;
    return texture(Palette, vec2(idx * (255.0/256.0) + (0.5/256.0), 0.5));
}

void main()
{
    vec4 col;
    if (indexed == 0) {
        col = texture(Texture, Frag_UV.st);
    } else if (indexed == 1) {
        col = palLookup(Frag_UV.st);
    } else {
        // Manual bilinear AFTER palette lookup (filtering raw indices is garbage).
        vec2 ts = vec2(textureSize(Texture, 0));
        vec2 p = Frag_UV.st * ts - 0.5;
        vec2 f = fract(p);
        vec2 base = (floor(p) + 0.5) / ts;
        vec4 c00 = palLookup(base);
        vec4 c10 = palLookup(base + vec2(1.0, 0.0) / ts);
        vec4 c01 = palLookup(base + vec2(0.0, 1.0) / ts);
        vec4 c11 = palLookup(base + vec2(1.0, 1.0) / ts);
        col = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }

    FragColor = col * Frag_Color;
};
)";

// Parts shader with flip support, additive color, and vertex color tinting
const char* partsSrcVert = R"(
#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 UV;
layout (location = 2) in vec4 Color;

out vec2 Frag_UV;
out vec4 Frag_Color;

uniform mat4 ProjMtx;
uniform int flip;

void main()
{
    // Choose between s,t or p,q based on flip
    if (flip != 0) {
        Frag_UV = UV.pq;
    } else {
        Frag_UV = UV.st;
    }
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position, 1);
}
)";

const char* partsSrcFrag = R"(
#version 330 core
uniform sampler2D Texture;
uniform vec3 addColor;

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    vec4 col = texture(Texture, Frag_UV);
    col.rgba *= Frag_Color;  // Apply vertex color tint (BGRA color from part)
    col.rgb += addColor;     // Apply additive color
    FragColor = col;
}
)";


Render::Render():
cg(nullptr),
m_parts(nullptr),
filter(false),
vSprite(Vao::F2F2, GL_DYNAMIC_DRAW),
vGeometry(Vao::F3F3, GL_STREAM_DRAW),
imageVertex{
	256, 256, 	0, 0,
	512, 256,  	1, 0, 
	512, 512,  	1, 1, 

	512, 512, 	1, 1,
	256, 512,  	0, 1,
	256, 256,  	0, 0,
},
colorRgba{1,1,1,1},
curImageId(-1),
quadsToDraw(0),
x(0), offsetX(0),
y(0), offsetY(0),
rotX(0), rotY(0), rotZ(0),
blendingMode(normal)
{
	sSimple.BindAttrib("Position", 0);
	sSimple.BindAttrib("Color", 1);
	//sSimple.LoadShader("src/simple.vert", "src/simple.frag");
	sSimple.LoadShader(simpleSrcVert, simpleSrcFrag, true);

	sSimple.Use();

	sTextured.BindAttrib("Position", 0);
	sTextured.BindAttrib("UV", 1);
	sTextured.BindAttrib("Color", 2);
	//sTextured.LoadShader("src/textured.vert", "src/textured.frag");
	sTextured.LoadShader(texturedSrcVert, texturedSrcFrag, true);

	sPartShader.BindAttrib("Position", 0);
	sPartShader.BindAttrib("UV", 1);
	sPartShader.BindAttrib("Color", 2);
	sPartShader.LoadShader(partsSrcVert, partsSrcFrag, true);

	lAlphaS = sSimple.GetLoc("Alpha");
	lProjectionS = sSimple.GetLoc("ProjMtx");
	lProjectionT = sTextured.GetLoc("ProjMtx");
	lIndexedT = sTextured.GetLoc("indexed");
	lProjectionParts = sPartShader.GetLoc("ProjMtx");
	lFlipParts = sPartShader.GetLoc("flip");
	lAddColorParts = sPartShader.GetLoc("addColor");

	//Palette texture lives on unit 1; the sampler binding never changes.
	sTextured.Use();
	glUniform1i(sTextured.GetLoc("Palette"), 1);
	glGenTextures(1, &paletteTexId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, paletteTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glActiveTexture(GL_TEXTURE0);

	vSprite.Prepare(sizeof(imageVertex), imageVertex);
	vSprite.Load();

	// Initialize LINES buffer with 8 vertices (4 lines):
	// - 2 grid lines (4 vertices)
	// - 2 origin marker lines (4 vertices, initially hidden at 0,0)
	float lines[]
	{
		-10000, 0, -1,	1,1,1,
		10000, 0, -1,	1,1,1,
		0, 10000, -1,	1,1,1,
		0, -10000, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
	};

	geoParts[LINES] = vGeometry.Prepare(sizeof(lines), lines);
	geoParts[BOXES] = vGeometry.Prepare(sizeof(float)*6*4*maxBoxes, nullptr);
	vGeometry.Load();
	vGeometry.InitQuads(geoParts[BOXES]);

	UpdateProj(clientRect.x, clientRect.y);

	glViewport(0, 0, clientRect.x, clientRect.y);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
}

void Render::BeginPass(const PassParams& params)
{
	pass = params;
	if (pass.width < 1) pass.width = 1;
	if (pass.height < 1) pass.height = 1;
	if (!(pass.zoom > 0.f)) pass.zoom = 1.f;
	UpdateProj((float)pass.width, (float)pass.height);
	scale = pass.zoom;
	// Same integer truncation the single-surface renderer used, so a pass
	// with the old camera reproduces the old image pixel for pixel.
	x = (int)(pass.originX / pass.zoom);
	y = (int)(pass.originY / pass.zoom);
}

void Render::ApplySpriteTextureMode()
{
	if (spriteIndexed) {
		//1 = nearest, 2 = shader-side bilinear (indices can't be filtered raw)
		glUniform1i(lIndexedT, filter ? 2 : 1);
		//Refresh the palette texture from the CG's current palette. 1KB —
		//negligible, and it makes palette/PUPS switches free (no re-bake).
		if (cg && cg->getPalettePtr()) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, paletteTexId);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, cg->getPalettePtr());
			glActiveTexture(GL_TEXTURE0);
		}
	} else {
		glUniform1i(lIndexedT, 0);
	}
}

void Render::DrawGridLines()
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
	}

	//Lines only
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	SetModelView(std::move(view));

	// Ensure sSimple shader is active and vertex attributes are enabled
	sSimple.Use();
	glUniform1f(lAlphaS, 0.25f);
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	vGeometry.Draw(geoParts[LINES], 0, GL_LINES);
}

void Render::Draw()
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
		//PostQuitMessage(1);
	}

	// Note: DrawBackground is invoked from MainFrame::DrawBack so it runs
	// even when there is no active character (and master's code paths use
	// DrawGridLines/DrawLayers directly rather than this Render::Draw).

	//Lines
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	SetModelView(std::move(view));
	glUniform1f(lAlphaS, 0.25f);
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	vGeometry.Draw(geoParts[LINES], 0, GL_LINES);

	// Check if we should use Parts rendering
	// Debug: Only print when renderMode changes
	static int lastRenderMode = -1;
	static int debugDrawCount = 0;
	if (debugDrawCount < 5 && m_parts && m_parts->renderMode && *m_parts->renderMode != lastRenderMode) {
		printf("[Render::Draw] usePat=%d, m_parts=%p, loaded=%d\n",
			usePat, (void*)m_parts, m_parts ? m_parts->loaded : -1);
		printf("  renderMode=%d (0=DEFAULT, 1=TEXTURE_VIEW, 2=UV_SETTING)\n",
			(int)*m_parts->renderMode);
		lastRenderMode = *m_parts->renderMode;
		debugDrawCount++;
	}

	if (usePat && m_parts && m_parts->loaded)
	{

		// Use Parts rendering with callbacks
		constexpr float tau = glm::pi<float>()*2.f;

		sPartShader.Use();

		// Disable vertex attribute array for color so glVertexAttrib4fv sets a constant value
		glDisableVertexAttribArray(2);

		// Callback to set matrix transform
		// Use perspective projection for PAT rendering (like sosfiro)
		auto setMatrix = [this](glm::mat4 partMatrix) {
			// Build the pre-transform matrix with screen-space transforms only
			glm::mat4 rview = projection;
			rview = glm::scale(rview, glm::vec3(scale, scale, 1.f));
			rview = glm::translate(rview, glm::vec3(x, y, 0));
			// Don't multiply by partMatrix here - pass it separately to SetMatrixPersp
			rview = glm::translate(rview, glm::vec3(0, 0, 1024.f));
			rview *= invOrtho;

			// Apply perspective projection - pass partMatrix separately (like sosfiro)
			SetMatrixPersp(lProjectionParts, partMatrix, rview);
		};

		// Callback to set additive color
		auto setAddColor = [this](float r, float g, float b) {
			glUniform3f(lAddColorParts, r, g, b);
		};

		// Callback to set flip mode
		auto setFlip = [this](char flip) {
			glUniform1i(lFlipParts, (int)flip);
		};

		// Draw Parts with interpolation (wrapped in try-catch for crash diagnosis)
		try {
			m_parts->Draw(curPattern, curNextPattern, curInterp, setMatrix, setAddColor, setFlip, colorRgba);
		} catch (const std::exception& e) {
			printf("[Render] EXCEPTION in Parts::Draw: %s\n", e.what());
			return;
		} catch (...) {
			printf("[Render] UNKNOWN EXCEPTION in Parts::Draw\n");
			return;
		}

		// Draw hitboxes after Parts
		sSimple.Use();
		vGeometry.Bind();
		glUniform1f(lAlphaS, 0.6f);
		vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
		glUniform1f(lAlphaS, 0.3f);
		vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);

		return;  // Skip sprite rendering
	}

	//Sprite
	constexpr float tau = glm::pi<float>()*2.f;
	view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	// Apply scale and rotations based on AFRT flag
	if (AFRT) {
		// AFRT=true: Scale → X → Y → Z (PR #39 order - fixes pattern 15)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
	} else {
		// AFRT=false: Scale → Z → Y → X (EXACT original pre-PR#39 order)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
	}
	view = glm::translate(view, glm::vec3(-128+offsetX,-224+offsetY,0.f));
	SetModelView(std::move(view));
	sTextured.Use();
	ApplySpriteTextureMode();
	SetMatrix(lProjectionT);
	if(spriteTex)
	{
		BindSpriteTexture();
		SetBlendingMode();
		glDisableVertexAttribArray(2);
		glVertexAttrib4fv(2, colorRgba);
		vSprite.Bind();
		vSprite.Draw(0);
	}
	//Reset state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	//Boxes
	sSimple.Use();
	vGeometry.Bind();
	glUniform1f(lAlphaS, 0.6f);
	vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
	glUniform1f(lAlphaS, 0.3f);
	vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);
}

void Render::DrawSpriteOnly(bool drawHitboxes)
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
	}

	// Disable depth write so layers don't occlude each other
	// We still depth TEST against the background, but don't write to depth buffer
	glDepthMask(GL_FALSE);

	//Sprite (with full transform including offset)
	constexpr float tau = glm::pi<float>()*2.f;
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	// Apply scale and rotations based on AFRT flag
	if (AFRT) {
		// AFRT=true: Scale → X → Y → Z (PR #39 order - fixes pattern 15)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
	} else {
		// AFRT=false: Scale → Z → Y → X (EXACT original pre-PR#39 order)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
	}
	view = glm::translate(view, glm::vec3(-128+offsetX,-224+offsetY,0.f));
	SetModelView(std::move(view));
	sTextured.Use();
	ApplySpriteTextureMode();
	SetMatrix(lProjectionT);
	if(spriteTex)
	{
		BindSpriteTexture();
		SetBlendingMode();
		glDisableVertexAttribArray(2);
		glVertexAttrib4fv(2, colorRgba);
		vSprite.Bind();
		vSprite.Draw(0);
	}
	//Reset state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	// Draw hitboxes if requested
	if (drawHitboxes) {
		//Boxes (without offsetX/offsetY - boxes should only be positioned by x,y)
		// Match the behavior of Draw() function which doesn't apply offset to boxes
		view = glm::mat4(1.f);
		view = glm::scale(view, glm::vec3(scale, scale, 1.f));
		view = glm::translate(view, glm::vec3(x,y,0.f));
		SetModelView(std::move(view));
		sSimple.Use();
		SetMatrix(lProjectionS);
		vGeometry.Bind();
		glUniform1f(lAlphaS, 0.6f);
		vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
		glUniform1f(lAlphaS, 0.3f);
		vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);
	}

	// Re-enable depth write
	glDepthMask(GL_TRUE);
}

void Render::SetModelView(glm::mat4&& view_)
{
	view = std::move(view_);
}

void Render::SetMatrix(int lProjection)
{
	glUniformMatrix4fv(lProjection, 1, GL_FALSE, glm::value_ptr(projection*view));
}

void Render::SetMatrixPersp(int lProjection, glm::mat4 partView, glm::mat4 pre)
{
	// Apply perspective projection for PAT rendering (like sosfiro)
	glUniformMatrix4fv(lProjection, 1, GL_FALSE, glm::value_ptr(pre * perspective * partView));
}

void Render::UpdateProj(float w, float h)
{
	if(w == 0 || h == 0)
		return;

	projection = glm::ortho<float>(0, w, h, 0, -1024.f, 1024.f);
	invOrtho = glm::inverse(projection);

	// Perspective for PAT rendering, plane-fit at z=0 (parts at z=0 map 1:1 to
	// editor pixels; z!=0 foreshortens). The eye distance comes from uni2.exe's
	// Pat_GetGlobalViewProjMatrix_640x480 (RE'd via IDA):
	//   PerspectiveFovLH(1.3045008 rad, aspect 1, zn 0.01, zf 1000), eye at
	//   z = -cot(fov/2) in unit space, part z pre-scaled by 1/1024
	// => effective eye distance = 1024 * cot(1.3045008/2) pixels (~1338.9).
	// (sosfiro's f=1.3 was an empirical approximation of the same value.)
	constexpr float dist = 1024;
	constexpr float dist2 = dist * 2;
	const float gameFovY = 1.3045008f;                 // radians, from the binary
	const float f = 1.f / glm::tan(gameFovY * 0.5f);   // = cot(fov/2) ~= 1.30737
	perspective = glm::translate(glm::mat4(1.f), glm::vec3(-1, 1, 0)) *
		glm::frustum<float>(-w/dist2, w/dist2, h/dist2, -h/dist2, 1*f, dist*2*f);
	perspective = glm::translate(perspective, glm::vec3(0.f, 0.f, -dist*f));
}

void Render::SetCg(CG *cg_)
{
	cg = cg_;
	// Reset current image ID to force texture reload on next SwitchImage call
	curImageId = -1;
}

void Render::SetParts(Parts *parts)
{
	// When switching between different Parts instances, clear CG sprite texture
	// This prevents CG sprite textures from overlaying PAT textures
	if (m_parts != parts) {
		// Only clear if switching to/from Parts rendering, or between different Parts
		if ((m_parts == nullptr) != (parts == nullptr)) {
			// Switching between PAT and non-PAT mode
			ClearTexture();
		} else if (m_parts != nullptr && parts != nullptr && m_parts != parts) {
			// Switching between different PAT files
			ClearTexture();
		}
	}
	m_parts = parts;
}

Render::~Render()
{
	for (auto& entry : spriteCache)
		if (entry.second.tex) glDeleteTextures(1, &entry.second.tex);
	spriteCache.clear();
	if (paletteTexId) glDeleteTextures(1, &paletteTexId);
}

void Render::EvictSprites(size_t budget)
{
	while (spriteCacheBytes > budget && !spriteCache.empty()) {
		auto oldest = spriteCache.begin();
		for (auto it = spriteCache.begin(); it != spriteCache.end(); ++it)
			if (it->second.lastUse < oldest->second.lastUse) oldest = it;
		if (&oldest->second == curSprite) { curSprite = nullptr; spriteTex = 0; curImageId = -1; }
		glDeleteTextures(1, &oldest->second.tex);
		spriteCacheBytes -= oldest->second.bytes;
		spriteCache.erase(oldest);
	}
}

void Render::BindSpriteTexture()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, spriteTex);
	// Indexed sprites filter in the shader (always NEAREST here); direct RGBA
	// sprites follow the current filter setting.
	if (curSprite && !curSprite->indexed && curSprite->linear != filter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
		curSprite->linear = filter;
	}
}

void Render::SwitchImage(int id)
{
	if (!cg || !cg->m_loaded)
		return;
	const unsigned long long gen = cg->generation();
	// id == -1 always clears (palette/CG change callers rely on it).
	if (id == curImageId && id != -1 && cg == curImageCg && gen == curImageGen && spriteTex)
		return;
	curImageId = id;
	curImageCg = cg;
	curImageGen = gen;
	spriteTex = 0;
	curSprite = nullptr;
	if (id < 0)
		return;

	const SpriteKey key{cg, gen, id};
	auto it = spriteCache.find(key);
	if (it == spriteCache.end()) {
		//8bpp images stay indexed on the GPU; the shader resolves the
		//palette (see ApplySpriteTextureMode). Other formats bake as before.
		std::unique_ptr<ImageData> image(cg->draw_texture(id, false, cg->image_is_8bpp(id)));
		if (!image || image->width <= 0 || image->height <= 0)
			return;   // (also avoids GL_INVALID_VALUE on empty images)
		CachedSprite entry;
		entry.w = image->width; entry.h = image->height;
		entry.ox = image->offsetX; entry.oy = image->offsetY;
		entry.indexed = image->is8bpp;
		glGenTextures(1, &entry.tex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, entry.tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		const bool linear = !entry.indexed && filter;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
		if (entry.indexed) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, entry.w, entry.h, 0, GL_RED, GL_UNSIGNED_BYTE, image->pixels);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			entry.bytes = (size_t)entry.w * entry.h;
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, entry.w, entry.h, 0,
				image->bgr ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
			entry.bytes = (size_t)entry.w * entry.h * 4;
		}
		entry.linear = linear;
		constexpr size_t kBudget = 256u << 20;   // 256 MiB of sprite textures
		EvictSprites(kBudget > entry.bytes ? kBudget - entry.bytes : 0);
		spriteCacheBytes += entry.bytes;
		it = spriteCache.emplace(key, entry).first;
	}
	it->second.lastUse = ++spriteUseClock;
	curSprite = &it->second;
	spriteTex = it->second.tex;
	spriteIndexed = it->second.indexed;
	AdjustImageQuad(it->second.ox, it->second.oy, it->second.w, it->second.h);
	vSprite.UpdateBuffer(0, imageVertex);
}

void Render::AdjustImageQuad(int x, int y, int w, int h)
{
	w+=x;
	h+=y;

	imageVertex[0] = imageVertex[16] = imageVertex[20] = x;
	imageVertex[4] = imageVertex[8] = imageVertex[12] = w;

	imageVertex[1] = imageVertex[5] = imageVertex[21] = y;
	imageVertex[9] = imageVertex[13] = imageVertex[17] = h;
}

void Render::GenerateHitboxVertices(const BoxList &hitboxes)
{
	int size = hitboxes.size();
	if(size <= 0)
	{
		quadsToDraw = 0;
		return;
	}
	static Hitbox **lastHitbox = 0;
	static int lastSize = 0;

	const float *color;
	//red, green, blue, z order
	constexpr float collisionColor[] 	{1, 1, 1, 1};
	constexpr float greenColor[] 		{0.2, 1, 0.2, 2};
	constexpr float shieldColor[] 		{0, 0, 1, 3}; //Not only for shield
	constexpr float clashColor[]		{1, 1, 0, 4};
	constexpr float projectileColor[] 	{0, 1, 1, 5}; //飛び道具
	constexpr float purple[] 			{0.5, 0, 1, 6}; //特別
	constexpr float redColor[] 			{1, 0.2, 0.2, 7};
	constexpr float hiLightColor[]		{1, 0.5, 1, 10};

	constexpr int tX[] = {0,1,1,0};
	constexpr int tY[] = {0,0,1,1};

	int floats = size*4*6; //4 Vertices with 6 attributes.
	if(clientQuads.size() < floats)
		clientQuads.resize(floats);
	
	int dataI = 0;
	for(const auto &boxPair : hitboxes)
	{
		int i = boxPair.first;
		const Hitbox& hitbox = boxPair.second;

		if (highLightN == i)
			color = hiLightColor;
		else if(i==0)
			color = collisionColor;
		else if (i >= 1 && i <= 8)
			color = greenColor;
		else if(i >=9 && i <= 10)
			color = shieldColor;
		else if(i == 11)
			color = clashColor;
		else if(i == 12)
			color = projectileColor;
		else if(i>12 && i<=24)
			color = purple;
		else
			color = redColor;

		
		for(int j = 0; j < 4*6; j+=6)
		{
			//X, Y, Z, R, G, B
			clientQuads[dataI+j+0] = hitbox.xy[0] + (hitbox.xy[2]-hitbox.xy[0])*tX[j/6];
			clientQuads[dataI+j+1] = hitbox.xy[1] + (hitbox.xy[3]-hitbox.xy[1])*tY[j/6];
			clientQuads[dataI+j+2] = color[3]+1000.f;
			clientQuads[dataI+j+3] = color[0];
			clientQuads[dataI+j+4] = color[1];
			clientQuads[dataI+j+5] = color[2];
		}
		dataI += 4*6;
	}
	quadsToDraw = size;
	vGeometry.UpdateBuffer(geoParts[BOXES], clientQuads.data(), dataI*sizeof(float));
}

bool Render::GeneratePartCenterVertices()
{
	// This function draws a colored plus sign at the origin (x, y) of the selected part
	// This helps visualize the transform origin when editing parts in the PatEditor

	// Safety: m_parts must be loaded to access part properties
	if (!m_parts || !m_parts->loaded || !m_parts->currState) {
		return false;
	}

	// Only draw in DEFAULT mode when not animating
	if (m_parts->currState->animating || m_parts->currState->renderMode != RenderMode::DEFAULT) {
		return false;
	}

	// Get the selected part property to draw its origin
	int partSetIndex = m_parts->currState->partSet;
	int partPropIndex = m_parts->currState->partProp;

	auto* partProp = m_parts->GetPartProp(partSetIndex, partPropIndex);
	if (!partProp) {
		return false;
	}

	// Draw a colored plus sign at the part's origin (x, y)
	// This helps visualize where transforms are applied from
	float propX = partProp->x;
	float propY = partProp->y;
	float lineSize = 50.0f;  // Length of the cross arms in pixels

	// Color for the origin marker (pink/magenta by default)
	float color[3] = {1.0f, 0.5f, 1.0f};

	// Lines buffer format: 4 grid lines + 4 origin marker lines
	// Each line: x1, y1, z, r, g, b, x2, y2, z, r, g, b (but stored as individual vertices)
	float lines[] = {
		// Grid lines (horizontal and vertical through 0,0)
		-10000, 0, -1,   1,1,1,
		10000, 0, -1,    1,1,1,
		0, 10000, -1,    1,1,1,
		0, -10000, -1,   1,1,1,

		// Origin marker (plus sign at part origin)
		propX - lineSize, propY, 1,    color[0], color[1], color[2],  // Left arm
		propX + lineSize, propY, 1,    color[0], color[1], color[2],  // Right arm
		propX, propY + lineSize, 1,    color[0], color[1], color[2],  // Top arm
		propX, propY - lineSize, 1,    color[0], color[1], color[2],  // Bottom arm
	};

	vGeometry.UpdateBuffer(geoParts[LINES], lines, sizeof(lines));
	gridLinesHaveOverlay = true;
	return true;
}

bool Render::GenerateUVRectangleVertices()
{
	// This function draws a colored rectangle showing the UV bounds of the selected cutout
	// when in TEXTURE_VIEW or UV_SETTING_VIEW mode. This helps visualize which portion
	// of the texture the cutout uses.

	// Safety: m_parts must be loaded to access cutout properties
	if (!m_parts || !m_parts->loaded || !m_parts->currState) {
		return false;
	}

	// Only draw in TEXTURE_VIEW or UV_SETTING_VIEW mode when not animating
	if (m_parts->currState->animating || m_parts->currState->renderMode == RenderMode::DEFAULT) {
		return false;
	}

	// Get the selected cutout to draw its UV bounds
	int cutOutIndex = m_parts->currState->partCutOut;
	auto* cutOut = m_parts->GetCutOut(cutOutIndex);
	if (!cutOut) {
		return false;
	}

	// Get the texture to calculate pixel coordinates from UV coordinates
	auto* gfx = m_parts->GetPartGfx(cutOut->texture);
	if (!gfx) {
		return false;
	}

	// Calculate pixel coordinates from UV coordinates
	// UV coordinates are in texture space (0-255), we need to convert to pixel space
	float uvBppX = gfx->uvBpp[0];  // UV units per pixel X
	float uvBppY = gfx->uvBpp[1];  // UV units per pixel Y

	float x1 = cutOut->uv[0] / uvBppX;  // Left edge
	float y1 = cutOut->uv[1] / uvBppY;  // Top edge
	float x2 = (cutOut->uv[0] + cutOut->uv[2]) / uvBppX;  // Right edge
	float y2 = (cutOut->uv[1] + cutOut->uv[3]) / uvBppY;  // Bottom edge

	// Offset to center the rectangle on the texture
	float offsetX = -gfx->w / 2.0f;
	float offsetY = -gfx->h / 2.0f;

	x1 += offsetX;
	x2 += offsetX;
	y1 += offsetY;
	y2 += offsetY;

	// Color for the UV rectangle outline (cyan/blue)
	constexpr float uvRectColor[] = {0.0f, 1.0f, 1.0f};  // Cyan
	constexpr float zOrder = 100.0f;  // Draw on top

	// Triangle vertex indices for a quad
	constexpr int tX[] = {0,1,1,0};
	constexpr int tY[] = {0,0,1,1};

	// Generate quad vertices (4 vertices with 6 attributes each: X, Y, Z, R, G, B)
	float rectQuad[4*6];
	for(int j = 0; j < 4*6; j+=6)
	{
		rectQuad[j+0] = x1 + (x2-x1)*tX[j/6];  // X
		rectQuad[j+1] = y1 + (y2-y1)*tY[j/6];  // Y
		rectQuad[j+2] = zOrder;                 // Z (draw on top)
		rectQuad[j+3] = uvRectColor[0];        // R
		rectQuad[j+4] = uvRectColor[1];        // G
		rectQuad[j+5] = uvRectColor[2];        // B
	}

	quadsToDraw = 1;
	vGeometry.UpdateBuffer(geoParts[BOXES], rectQuad, sizeof(rectQuad));
	return true;
}

void Render::DontDraw()
{
	quadsToDraw = 0;
	gridLinesHaveOverlay = true;   // force the reset below
	ResetGridLines();
}

void Render::ResetGridLines()
{
	if (!gridLinesHaveOverlay) return;
	gridLinesHaveOverlay = false;
	// Reset lines buffer to show only grid (no part origin markers)
	float lines[]
	{
		-10000, 0, -1,	1,1,1,
		10000, 0, -1,	1,1,1,
		0, 10000, -1,	1,1,1,
		0, -10000, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
	};
	vGeometry.UpdateBuffer(geoParts[LINES], lines, sizeof(lines));
}

void Render::ClearTexture()
{
	// Forget the current sprite; cached textures stay (keyed by CG generation).
	spriteTex = 0;
	curSprite = nullptr;
	curImageId = -1;
	curImageCg = nullptr;
}

void Render::SetImageColor(float *rgba)
{
	if(rgba)
	{
		for(int i = 0; i < 4; i++)
			colorRgba[i] = rgba[i];
	}
	else
	{
		for(int i = 0; i < 4; i++)
			colorRgba[i] = 1.f;
	}
}

void Render::SetBlendingMode()
{
	switch (blendingMode)
	{
	default:
	case normal:
		//glBlendEquation(GL_FUNC_ADD);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case additive:
		//glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case subtractive:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		break;
	}
}

// Multi-layer rendering support
void Render::ClearLayers()
{
	renderLayers.clear();
	currentLayerIndex = 0;
}

void Render::AddLayer(const RenderLayer& layer)
{
	renderLayers.push_back(layer);
}

void Render::SortLayersByZPriority(int mainPatternPriority)
{
	// Sort layers by Z-priority
	// Lower priority = draw first (behind)
	// Higher priority = draw last (in front)
	// stable_sort: equal priorities keep their insertion order, so ties
	// don't flicker between frames.
	std::stable_sort(renderLayers.begin(), renderLayers.end(),
		[mainPatternPriority](const RenderLayer& a, const RenderLayer& b) {
			// Calculate effective priorities based on game logic
			int priorityA = a.zPriority;
			int priorityB = b.zPriority;

			// Handle special projectile priority logic if needed
			// (For now using raw priority values)

			return priorityA < priorityB;  // Lower draws first
		});
}

void Render::DrawPatLayerItem(const RenderLayer& layer, Parts* origParts)
{
	Parts* layerParts = layer.sourceParts ? layer.sourceParts : m_parts;
	if (!layerParts || !layerParts->loaded)
		return;

	// Clean shader slate before switching Parts (prevents GL_INVALID_OPERATION)
	glUseProgram(0);
	bool switchedParts = false;
	if (layerParts != m_parts) {
		SetParts(layerParts);
		switchedParts = true;

		// Effect Parts don't have their own views, so they inherit state from
		// the main character for TEXTURE_VIEW support.
		if (origParts && origParts->renderMode && origParts->currState) {
			m_parts->updatePatEditorReferences(origParts->currState, origParts->renderMode);
		}
	}

	sPartShader.Use();
	glDisableVertexAttribArray(2);

	// Matrix callback with the layer transform. Order matches the game's
	// PatPart_BuildTransformMatrix and our CG path: pivot at entity+spawn
	// position, scale, Z/Y/X rotation, then the AFOF offset in rotated space.
	auto setMatrix = [this, &layer](glm::mat4 partMatrix) {
		constexpr float tau = glm::pi<float>()*2.f;
		glm::mat4 rview = projection;
		rview = glm::scale(rview, glm::vec3(scale, scale, 1.f));
		rview = glm::translate(rview, glm::vec3(x, y, 0));
		rview = glm::scale(rview, glm::vec3(layer.scaleX, layer.scaleY, 1.f));
		rview = glm::rotate(rview, layer.rotZ*tau, glm::vec3(0.f, 0.f, 1.f));
		rview = glm::rotate(rview, layer.rotY*tau, glm::vec3(0.f, 1.f, 0.f));
		rview = glm::rotate(rview, layer.rotX*tau, glm::vec3(1.f, 0.f, 0.f));
		rview = glm::translate(rview, glm::vec3(offsetX, offsetY, 0));
		rview = glm::translate(rview, glm::vec3(0, 0, 1024.f));
		rview *= invOrtho;
		SetMatrixPersp(lProjectionParts, partMatrix, rview);
	};

	auto setAddColor = [this](float r, float g, float b) {
		glUniform3f(lAddColorParts, r, g, b);
	};

	auto setFlip = [this](char flip) {
		glUniform1i(lFlipParts, (int)flip);
	};

	float layerColor[4] = {
		layer.tintColor.r,
		layer.tintColor.g,
		layer.tintColor.b,
		layer.alpha
	};

	m_parts->Draw(layer.spriteId, layer.spriteId, 0.0f, setMatrix, setAddColor, setFlip, layerColor);

	// Leave a clean slate: unbind buffers/textures/shader, swallow PAT GL errors.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	while (glGetError() != GL_NO_ERROR);

	if (switchedParts) {
		SetParts(origParts);
	}

	// CG textures must rebind after the unbind above.
	curImageId = -1;
}

void Render::DrawCgLayerItem(const RenderLayer& layer, const float* baseColorRgba)
{
	// Switch CG if this layer uses a different one (e.g., effect.ha6)
	if (layer.sourceCG && layer.sourceCG != cg) {
		SetCg(layer.sourceCG);
	}
	// Switch Parts if this layer uses different Parts (e.g., effect.pat)
	if (layer.sourceParts && layer.sourceParts != m_parts) {
		SetParts(layer.sourceParts);
	}

	// Layer scale and rotation (position members are set by the caller)
	scaleX = layer.scaleX;
	scaleY = layer.scaleY;
	rotX = layer.rotX;
	rotY = layer.rotY;
	rotZ = layer.rotZ;
	AFRT = layer.AFRT;

	switch (layer.blendMode)
	{
	case 2:
		blendingMode = additive;
		break;
	case 3:
		blendingMode = subtractive;
		break;
	default:
		blendingMode = normal;
		break;
	}

	colorRgba[0] = layer.tintColor.r * baseColorRgba[0];
	colorRgba[1] = layer.tintColor.g * baseColorRgba[1];
	colorRgba[2] = layer.tintColor.b * baseColorRgba[2];
	colorRgba[3] = layer.alpha * baseColorRgba[3];

	// PUPS palette file of this layer's pattern (issue #76). Indexed sprites
	// pick the palette up in the shader; baked ones need a re-bake.
	if (cg && followPups && cg->setPupsBank(layer.pups))
		curImageId = -1;

	SwitchImage(layer.spriteId);
	DrawSpriteOnly(false);
}

void Render::DrawLayers()
{
	if (renderLayers.empty()) {
		return;
	}

	// Save every piece of shared state the per-item draws mutate.
	Parts* origParts = m_parts;
	CG* origCG = cg;
	int origX = x;
	int origY = y;
	int origOffsetX = offsetX;
	int origOffsetY = offsetY;
	float origScaleX = scaleX;
	float origScaleY = scaleY;
	float origRotX = rotX;
	float origRotY = rotY;
	float origRotZ = rotZ;
	bool origAFRT = AFRT;
	blendType origBlendMode = blendingMode;
	float origColorRgba[4] = {colorRgba[0], colorRgba[1], colorRgba[2], colorRgba[3]};

	// Single pass in sorted order: PAT and CG layers interleave by z-priority.
	// (The old PAT-pass-then-CG-pass split forced every PAT layer underneath
	// every CG layer regardless of priority.) Every item starts from an
	// explicit GL baseline — nothing inherits blend/depth/texture state from
	// the previous item.
	// TODO: positioning flags (flagset1 & 0x10 camera-relative,
	// flagset2 & 0x100 opponent-relative, flagset2 & 0x200 fixed-origin).
	for (const auto& layer : renderLayers)
	{
		// Fully transparent layers draw no sprite (their hitboxes still draw
		// in the box pass below — see issue #78).
		if (layer.alpha == 0.0f) continue;

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);
		glDepthMask(GL_FALSE);

		x = origX + layer.spawnOffsetX;
		y = origY + layer.spawnOffsetY;
		offsetX = layer.frameOffsetX;
		offsetY = layer.frameOffsetY;

		if (layer.usePat) {
			// PAT needs a valid part-set id (unlike CG, where -1 still has boxes)
			if (layer.spriteId < 0) continue;
			DrawPatLayerItem(layer, origParts);
		} else {
			// The layer's tint/alpha already carry the frame RGBA; the base
			// colour is white (it used to be the root frame's RGBA again,
			// squaring the colour/alpha of CG layer 0 and tinting spawns).
			static const float kWhite[4] = {1.f, 1.f, 1.f, 1.f};
			DrawCgLayerItem(layer, kWhite);
		}
	}
	glDepthMask(GL_TRUE);

	// Explicit baseline for the box pass too.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	// Second pass: Draw all hitboxes on top of all sprites.
	// PAT layers are NOT skipped here: the frame's hitboxes ride on layer 0, and
	// MBTL/UNI characters whose layer 0 is a PAT part would otherwise lose their
	// boxes entirely (issue #68). The box draw below only needs x/y + the flat
	// projection, so it is safe for PAT layers.
	for (const auto& layer : renderLayers)
	{
		// Only draw hitboxes if this layer has any
		if (layer.hitboxes.empty()) continue;

		// Apply layer-specific position for hitboxes (no offsetX/offsetY for boxes)
		x = origX + layer.spawnOffsetX;
		y = origY + layer.spawnOffsetY;

		// Generate hitbox vertices for this layer
		GenerateHitboxVertices(layer.hitboxes);

		// Draw hitboxes
		glDepthMask(GL_FALSE);
		glm::mat4 view = glm::mat4(1.f);
		view = glm::scale(view, glm::vec3(scale, scale, 1.f));
		view = glm::translate(view, glm::vec3(x, y, 0.f));
		SetModelView(std::move(view));
		sSimple.Use();
		SetMatrix(lProjectionS);
		vGeometry.Bind();
		glUniform1f(lAlphaS, 0.6f);
		vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
		if (!layer.boxesOutlineOnly) {
			glUniform1f(lAlphaS, 0.3f);
			vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);
		}
		glDepthMask(GL_TRUE);
	}

	// Back to the base palette file for anything drawn outside the layers.
	for (const auto& layer : renderLayers) {
		if (layer.sourceCG && layer.sourceCG->pupsBank() != 0) {
			layer.sourceCG->setPupsBank(0);
			if (layer.sourceCG == cg) curImageId = -1;
		}
	}

	// Restore original CG and Parts
	if (cg != origCG) {
		SetCg(origCG);
	}
	if (m_parts != origParts) {
		SetParts(origParts);
	}

	// Restore original state
	x = origX;
	y = origY;
	offsetX = origOffsetX;
	offsetY = origOffsetY;
	scaleX = origScaleX;
	scaleY = origScaleY;
	rotX = origRotX;
	rotY = origRotY;
	rotZ = origRotZ;
	AFRT = origAFRT;
	blendingMode = origBlendMode;
	colorRgba[0] = origColorRgba[0];
	colorRgba[1] = origColorRgba[1];
	colorRgba[2] = origColorRgba[2];
	colorRgba[3] = origColorRgba[3];
}
// ---------------------------------------------------------------------------
// Background (stage) rendering — ported from the bgmk branch.

void Render::SetBackgroundRenderer(bg::Renderer* renderer, bg::Camera* camera)
{
	bgRenderer = renderer;
	bgCamera   = camera;
}

void Render::DrawBackground()
{
	// Bg renderer is now invoked directly from MainFrame::DrawBack with
	// real clientRect dimensions — it owns its own GL program and doesn't
	// need to thread through our shader helpers anymore. Kept here as a
	// no-op so existing call sites compile; will be removed once the
	// remaining stale plumbing is cleaned up.
}

// Draw one PAT pattern as a stage object. The bg Renderer calls this for
// objects whose sprite-id is < 10000 (PAT patterns rather than CG sprites).
// Mirrors the per-layer PAT draw in DrawLayers.
void Render::DrawBgPattern(Parts* parts, int pattern,
                           float worldX, float worldY, float alpha, int blendMode)
{
	if (!parts || !parts->loaded)
		return;

	sPartShader.Use();
	glDisableVertexAttribArray(2);

	// Blend mode for the whole pattern (frame draw_type: 2 = additive).
	if (blendMode == 2)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Position the pattern at editor-world (worldX, worldY); x/y supply the
	// camera pan and `scale` the zoom — same convention as DrawLayers.
	float wx = worldX, wy = worldY;
	auto setMatrix = [this, wx, wy](glm::mat4 partMatrix) {
		glm::mat4 rview = projection;
		rview = glm::scale(rview, glm::vec3(scale, scale, 1.f));
		rview = glm::translate(rview, glm::vec3(x + wx, y + wy, 0.f));
		rview = glm::translate(rview, glm::vec3(0.f, 0.f, 1024.f));
		rview *= invOrtho;
		SetMatrixPersp(lProjectionParts, partMatrix, rview);
	};
	auto setAddColor = [this](float r, float g, float b) {
		glUniform3f(lAddColorParts, r, g, b);
	};
	auto setFlip = [this](char flip) {
		glUniform1i(lFlipParts, (int)flip);
	};

	float color[4] = { 1.f, 1.f, 1.f, alpha };
	try {
		parts->Draw(pattern, pattern, 0.0f, setMatrix, setAddColor, setFlip, color);
	} catch (...) {
		// Parts::Draw already guards bad indices; swallow anything else so
		// one bad pattern can't kill the whole stage draw.
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	while (glGetError() != GL_NO_ERROR) {}
}

// The bg renderer drives quads through these two helpers so it doesn't have
// to know about our shader/projection setup.
void Render::SetupSpriteShader()
{
	sTextured.Use();
	//The bg renderer uploads direct RGBA textures — never indexed.
	glUniform1i(lIndexedT, 0);
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x, y, 0.f));
	SetModelView(std::move(view));
	SetMatrix(lProjectionT);
}

void Render::SetSpriteTransform(float spriteX, float spriteY, float spriteScaleX, float spriteScaleY)
{
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x, y, 0.f));
	view = glm::translate(view, glm::vec3(spriteX, spriteY, 0.f));
	view = glm::scale(view, glm::vec3(spriteScaleX, spriteScaleY, 1.f));
	SetModelView(std::move(view));
	SetMatrix(lProjectionT);
}
