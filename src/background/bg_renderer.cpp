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

GLuint Renderer::GetOrCreateTexture(int spriteId, int& outW, int& outH) {
	auto it = textureCache.find(spriteId);
	if (it != textureCache.end()) {
		outW = it->second.w;
		outH = it->second.h;
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

	textureCache[spriteId] = { id, img->width, img->height };
	outW = img->width;
	outH = img->height;
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
	int sw, sh;
	GLuint tex = GetOrCreateTexture(spriteId, sw, sh);
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

	// --- GL state setup mirroring MonoForm.Draw ---
	// Use our own program/VAO so we don't fight whatever shader/VAO the host
	// editor had bound (the host edits character/grid state aggressively).
	glUseProgram(program);

	// Save host's depth-test state so character rendering doesn't see our
	// changes; mainRender's next draw will rebind whatever it needs anyway.
	GLboolean prevDepthTest, prevDepthMask, prevBlend;
	glGetBooleanv(GL_DEPTH_TEST, &prevDepthTest);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	glGetBooleanv(GL_BLEND, &prevBlend);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	// Clear ONLY the depth buffer for our slice — leaves the color buffer
	// untouched so we can composite over whatever the host already drew.
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_BLEND);

	// Projection + view (Scale(zoom) folded in).
	float pview[16];
	BuildProjView(clientW, clientH, camera.zoom, pview);
	glUniformMatrix4fv(uProjView, 1, GL_FALSE, pview);

	// --- emit sprites ---
	// Mirrors MonoForm.cs FullBlend path: iterate all objects in their file
	// order, set blend mode per sprite, let the depth test sort visibility
	// via per-vertex layerDepth = layer/1024 - 1.
	for (const auto& obj : objects) {
		if (obj.frames.empty()) continue;
		const Frame& fr = obj.frames[obj.currentFrame];

		// Skip transient duration=0 loop-boundary placeholder frames.
		if (fr.duration == 0 && fr.aniType == 1 && obj.frames.size() > 1)
			continue;
		if (fr.spriteId < 0) continue;

		int para = parallaxEnabled ? obj.parallax : 256;
		float screenX = camera.ScreenX((float)fr.offsetX, para);
		float screenY = camera.ScreenY((float)fr.offsetY, para);

		// Apply pan anchor here (camera.ScreenX returned just offset + delta).
		screenX += camera.panLastX;
		screenY += camera.panLastY;

		int sw, sh;
		GLuint tex = GetOrCreateTexture(fr.spriteId, sw, sh);
		if (tex == 0) continue;

		float alpha = (fr.blendMode > 0) ? (fr.opacity / 255.0f) : 1.0f;
		float layerDepth = (float)obj.layer / 1024.0f - 1.0f;

		DrawSprite(fr.spriteId, screenX, screenY,
		           (float)sw, (float)sh,
		           alpha, fr.blendMode, layerDepth);
	}

	// --- restore state ---
	if (!prevDepthTest) glDisable(GL_DEPTH_TEST);
	glDepthMask(prevDepthMask);
	if (!prevBlend) glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace bg
