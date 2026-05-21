// Port of u4ick's bgmaketool MonoForm.Draw rendering pipeline.
//
// References to the original (paths relative to repo root):
//   src/dpu4/stagesrc/bgmaketool/MonoForm.cs        (Draw method)
//   src/dpu4/stagesrc/bgmaketool/bgmake_app.cs      (camera/state globals)
//
// What we replicate:
//   - Orthographic projection: CreateOrthographicOffCenter(0, W, H, 0, 0, 1).
//     Same as XNA's screen-space ortho with Y growing downward.
//   - View matrix = Scale(zoom). No translation (camera/pan is in sprite pos).
//   - Sprite screen position = panLast + (pan - panLast) * (parallax/256) + offset.
//     The (pan - panLast) delta is only non-zero during a live drag, so this
//     produces u4ick's transient parallax-during-drag effect.
//   - Per-sprite blend modes: blendMode == 2 -> additive (GL_SRC_ALPHA, GL_ONE);
//     anything else -> custom alpha (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
//   - Depth test enabled with GL_LEQUAL and depth writes on. Per-vertex z is
//     layerDepth = layer/1024 - 1. Lower layer = smaller depth = wins the
//     test = ends up on top. This matches u4ick's SpriteSortMode.FrontToBack
//     ordering through the depth buffer rather than a CPU sort.
//   - The dur=0 / aniType=1 frame skip from MonoForm.cs:211-214 (transient
//     loop-boundary placeholder frames should not be rendered).
//
// What we DON'T do (out of scope here; matches u4ick's interactive view):
//   - SaveRender's 1057x818 fixed render target.
//   - AlphaTestEffect (Greater, ReferenceAlpha=0). With our blend funcs alpha=0
//     fragments produce no visible output anyway, and depth writes for fully
//     transparent pixels don't cause visible artifacts at our render scale.

#include "bg_renderer.h"
#include "../cg.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace bg {

// ---- shaders ---------------------------------------------------------------

static const char* kVS = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
uniform mat4 uProjView;
out vec2 vUV;
void main() {
    gl_Position = uProjView * vec4(aPos, 1.0);
    vUV = aUV;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec4 c = texture(uTexture, vUV);
    FragColor = vec4(c.rgb, c.a * uAlpha);
}
)GLSL";

static GLuint CompileShader(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		std::cerr << "[bg] shader compile failed: " << log << std::endl;
	}
	return s;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
	GLuint p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		std::cerr << "[bg] program link failed: " << log << std::endl;
	}
	return p;
}

// ---- lifecycle -------------------------------------------------------------

Renderer::Renderer() = default;

Renderer::~Renderer() {
	ClearTextureCache();
	if (vbo) glDeleteBuffers(1, &vbo);
	if (program) glDeleteProgram(program);
}

void Renderer::InitGL() {
	if (glInit) return;
	GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
	program = LinkProgram(vs, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	uProjView = glGetUniformLocation(program, "uProjView");
	uTexture  = glGetUniformLocation(program, "uTexture");

	// No VAO — glad here doesn't expose glGenVertexArrays. Bind VBO and
	// re-set attribute pointers on every DrawSprite call instead. Cheap.
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 5 * 6, nullptr, GL_DYNAMIC_DRAW);

	glInit = true;
}

void Renderer::ClearTextureCache() {
	for (auto& kv : textureCache) glDeleteTextures(1, &kv.second.id);
	textureCache.clear();
}

void Renderer::SetFile(File* f) {
	if (file != f) ClearTextureCache();
	file = f;
}

void Renderer::Update() {
	if (!enabled || !file || paused) return;
	file->UpdateAnimations();
}

// ---- texture cache ---------------------------------------------------------

GLuint Renderer::GetOrCreateTexture(int spriteId, int& outW, int& outH,
                                    int& outOriginX, int& outOriginY) {
	auto it = textureCache.find(spriteId);
	if (it != textureCache.end()) {
		outW = it->second.w;
		outH = it->second.h;
		outOriginX = it->second.originX;
		outOriginY = it->second.originY;
		return it->second.id;
	}
	if (!file) return 0;
	CG* cg = file->GetCG();
	if (!cg) return 0;
	ImageData* img = cg->draw_texture(spriteId, false, false);
	if (!img) return 0;

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	textureCache[spriteId] = { id, img->width, img->height, img->offsetX, img->offsetY };
	outW = img->width;
	outH = img->height;
	outOriginX = img->offsetX;
	outOriginY = img->offsetY;
	delete img;
	return id;
}

// ---- projection ------------------------------------------------------------

// XNA's CreateOrthographicOffCenter(left, right, bottom, top, near, far)
// in row-major form. Composed with Scale(zoom) on the view side; we fold
// the scale directly into the same matrix so there's only one uniform.
void Renderer::BuildProjView(int W, int H, float zoom, float m[16]) {
	const float l = 0.0f, r = (float)W, b = (float)H, t = 0.0f, n = 0.0f, f = 1.0f;
	// glm::ortho-style result (column-major in GLSL but we send transposed below).
	// proj * scale(zoom):
	//   X: 2/(r-l) * zoom, Y: 2/(t-b) * zoom (negative because b>t), Z: -2/(f-n)
	//   Translation column accounts for -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n)
	float sx = 2.0f / (r - l) * zoom;
	float sy = 2.0f / (t - b) * zoom;
	float sz = -2.0f / (f - n);
	float tx = -(r + l) / (r - l);
	float ty = -(t + b) / (t - b);
	float tz = -(f + n) / (f - n);
	// Column-major (OpenGL).
	m[0]=sx; m[1]=0;  m[2]=0;  m[3]=0;
	m[4]=0;  m[5]=sy; m[6]=0;  m[7]=0;
	m[8]=0;  m[9]=0;  m[10]=sz;m[11]=0;
	m[12]=tx;m[13]=ty;m[14]=tz;m[15]=1;
}

// ---- sprite quad submission -----------------------------------------------

void Renderer::DrawSprite(int spriteId,
                          float x, float y, float w, float h,
                          float alpha, int blendMode,
                          float layerDepth)
{
	int sw, sh, ox, oy;
	GLuint tex = GetOrCreateTexture(spriteId, sw, sh, ox, oy);
	if (tex == 0) return;
	(void)sw; (void)sh; // sprite is drawn at the size requested by caller.

	// Blend mode — set right before the draw, per-sprite (matches u4ick's
	// per-batch BlendState swap in the FullBlend path).
	if (blendMode == 2) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	} else {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	const float z = layerDepth;
	float verts[5 * 6] = {
		x,       y,       z,  0.0f, 0.0f,
		x + w,   y,       z,  1.0f, 0.0f,
		x + w,   y + h,   z,  1.0f, 1.0f,

		x + w,   y + h,   z,  1.0f, 1.0f,
		x,       y + h,   z,  0.0f, 1.0f,
		x,       y,       z,  0.0f, 0.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

	// Re-bind attribs per draw (no VAO support in this glad).
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
	                      (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(uTexture, 0);
	GLint uAlpha = glGetUniformLocation(program, "uAlpha");
	glUniform1f(uAlpha, alpha);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ---- main entry ------------------------------------------------------------

void Renderer::Render(const Camera& camera, int clientW, int clientH) {
	if (!enabled || !file) return;
	CG* cg = file->GetCG();
	if (!cg || !cg->m_loaded) return;
	if (clientW <= 0 || clientH <= 0) return;

	InitGL();

	auto& objects = file->GetObjects();
	if (objects.empty()) return;

	// --- GL state ---
	glUseProgram(program);

	// Save host depth/blend so we don't leak state to character/grid.
	GLboolean prevDepthTest, prevDepthMask, prevBlend;
	glGetBooleanv(GL_DEPTH_TEST, &prevDepthTest);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	glGetBooleanv(GL_BLEND, &prevBlend);

	// Disable depth test — we sort on the CPU instead so we don't have to
	// worry about how our layerDepth range interacts with the host editor's
	// depth buffer, projection range, or character render afterwards. (My
	// earlier attempt set GL_DEPTH_TEST + layerDepth z = -1..0, which fell
	// outside our ortho's [0, 1] z range and clipped most sprites — "everything
	// looks worse." Pure paint sidesteps the whole problem.)
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	// Projection (Scale(zoom) folded in).
	float pview[16];
	BuildProjView(clientW, clientH, camera.zoom, pview);
	glUniformMatrix4fv(uProjView, 1, GL_FALSE, pview);

	// --- sort by layer ASCENDING (higher layer drawn LAST = on top) ---
	// User confirmed "higher on top" matches u4ick's visual ordering in bg51.
	std::vector<size_t> order(objects.size());
	for (size_t i = 0; i < objects.size(); ++i) order[i] = i;
	std::stable_sort(order.begin(), order.end(),
	                 [&](size_t a, size_t b) { return objects[a].layer < objects[b].layer; });

	// --- emit sprites ---
	for (size_t i : order) {
		const auto& obj = objects[i];
		if (obj.frames.empty()) continue;
		// currentFrame can briefly land out of range after an odd jump
		// chain — u4ick's draw is wrapped in try/catch; we just skip.
		if (obj.currentFrame < 0 || obj.currentFrame >= (int)obj.frames.size())
			continue;
		const Frame& fr = obj.frames[obj.currentFrame];

		// Skip transient duration=0 loop-boundary placeholder frames
		// (MonoForm.cs:211-214).
		if (fr.duration == 0 && fr.aniType == 1 && obj.frames.size() > 1)
			continue;
		if (fr.spriteId < 0) continue;

		int para = parallaxEnabled ? obj.parallax : 256;
		// -STAGE_CENTER_X / -STAGE_FLOOR_Y shift the whole stage so u4ick's
		// playfield centre (bg-x +127.5) and floor (bg-y +224) coincide
		// with the editor's world origin / grid lines. obj.vecX/vecY is the
		// accumulated movement-vector drift (bobbing orbs etc.).
		float screenX = camera.ScreenX((float)fr.offsetX, para) + camera.panLastX
		                - STAGE_CENTER_X + obj.vecX * STAGE_VEC_SCALE;
		float screenY = camera.ScreenY((float)fr.offsetY, para) + camera.panLastY
		                - STAGE_FLOOR_Y + obj.vecY * STAGE_VEC_SCALE;

		int sw, sh, ox, oy;
		GLuint tex = GetOrCreateTexture(fr.spriteId, sw, sh, ox, oy);
		if (tex == 0) continue;

		// Compensate for the cg lib returning a TIGHT bounded region: u4ick
		// draws the full sprite canvas at the bg offset, so the content
		// ends up at (offset + bounds_x1, offset + bounds_y1). Our tight
		// texture starts at the content's top-left, so we add (ox, oy)
		// to the draw position to land in the same place.
		screenX += (float)ox;
		screenY += (float)oy;

		float alpha = (fr.blendMode > 0) ? (fr.opacity / 255.0f) : 1.0f;

		DrawSprite(fr.spriteId, screenX, screenY,
		           (float)sw, (float)sh,
		           alpha, fr.blendMode, 0.0f);
	}

	// --- restore state ---
	if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	glDepthMask(prevDepthMask);
	if (!prevBlend) glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace bg
