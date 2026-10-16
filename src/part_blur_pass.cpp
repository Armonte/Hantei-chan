#include "part_blur_pass.h"
#include "render_target.h"
#include "shader.h"
#include "app_log.h"
#include <glad/glad.h>

namespace PartBlurPass
{
const float kWeights[5] = { 0.1489f, 0.1416f, 0.1219f, 0.0949f, 0.0669f };

namespace {
const char* kVert = R"(
#version 330 core
void main()
{
    // One full-screen triangle from gl_VertexID (no vertex buffer).
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

// PS_CharaPtBlurFuncH / _FuncW of sh_paniblur.txt: 9 taps, one game pixel apart (fBlurTexel), point-sampled at texel
// centres. uMask = 1 is PS_CharaPtBlurDrawW: 0 where tex_Mask.r < 0.001 (discarded here: the game's 0 output has
// alpha 0 under normal blending, so the scene stays).
const char* kFrag = R"(
#version 330 core
uniform sampler2D uBase;
uniform sampler2D uMaskTex;
uniform vec2 uOrigin;   // viewport origin of the pass
uniform vec2 uSize;     // target size in px
uniform vec2 uStep;     // one tap in uv (direction included)
uniform int uMask;
uniform float uW[5];
out vec4 FragColor;
void main()
{
    vec2 uv = (gl_FragCoord.xy - uOrigin) / uSize;
    if (uMask != 0 && texture(uMaskTex, uv).r < 0.001)
        discard;
    vec4 c = texture(uBase, uv) * uW[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uBase, uv + uStep * float(i)) * uW[i];
        c += texture(uBase, uv - uStep * float(i)) * uW[i];
    }
    FragColor = c;
}
)";

struct State
{
	bool ready = false;
	bool failed = false;
	Shader shader;
	unsigned vao = 0;
	RenderTarget scene, mask, blurV;
	int lBase = -1, lMask = -1, lOrigin = -1, lSize = -1, lStep = -1, lUseMask = -1, lW = -1;
};

State& St()
{
	static State s;
	return s;
}

bool Init(State& s)
{
	if (s.ready) return true;
	if (s.failed) return false;
	s.shader.LoadShader(kVert, kFrag, true);
	if (!s.shader.program) {
		s.failed = true;
		HC_LOG_ERROR("[PartBlurPass] blur shader failed to build; PRAL 3 parts draw as a plain mask-less blend");
		return false;
	}
	s.lBase = s.shader.GetLoc("uBase");
	s.lMask = s.shader.GetLoc("uMaskTex");
	s.lOrigin = s.shader.GetLoc("uOrigin");
	s.lSize = s.shader.GetLoc("uSize");
	s.lStep = s.shader.GetLoc("uStep");
	s.lUseMask = s.shader.GetLoc("uMask");
	s.lW = s.shader.GetLoc("uW");
	glGenVertexArrays(1, &s.vao);
	s.ready = true;
	return true;
}
}

void Run(const std::function<void()>& drawMask, float pxPerGamePx)
{
	State& s = St();
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	const int w = vp[2], h = vp[3];
	if (w <= 0 || h <= 0 || !Init(s) ||
		!s.scene.ensure(w, h, true) || !s.mask.ensure(w, h, false) || !s.blurV.ensure(w, h, true)) {
		drawMask();
		return;
	}

	GLint prevDraw = 0, prevRead = 0, prevProg = 0, prevTex = 0, prevVao = 0, prevActive = 0;
	GLint prevSrcRgb, prevDstRgb, prevSrcA, prevDstA, prevEqRgb, prevEqA;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActive);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
	glGetIntegerv(GL_BLEND_SRC_RGB, &prevSrcRgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &prevDstRgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevSrcA);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &prevDstA);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &prevEqRgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prevEqA);
	const GLboolean prevBlend = glIsEnabled(GL_BLEND);
	const GLboolean prevScissor = glIsEnabled(GL_SCISSOR_TEST);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
	glDisable(GL_SCISSOR_TEST);

	// 1. Scene copy (RT B) and the mask RT cleared to opaque black (CharaPtBlur_BeginMaskTarget).
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevDraw);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.scene.framebuffer());
	glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, s.mask.framebuffer());
	glViewport(0, 0, w, h);
	GLboolean cm[4];
	GLfloat prevClear[4];
	glGetBooleanv(GL_COLOR_WRITEMASK, cm);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClear);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 2. The part itself, normal blend, into the mask only.
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawMask();

	// 3. Vertical taps of B into C, then horizontal taps of C over the scene where mask.r >= 0.001.
	glDisable(GL_BLEND);
	glUseProgram(s.shader.program);
	glBindVertexArray(s.vao);
	glUniform1i(s.lBase, 0);
	glUniform1i(s.lMask, 1);
	glUniform1fv(s.lW, 5, kWeights);
	glUniform2f(s.lSize, (float)w, (float)h);
	const float px = pxPerGamePx > 0.f ? pxPerGamePx : 1.f;

	glBindFramebuffer(GL_FRAMEBUFFER, s.blurV.framebuffer());
	glUniform2f(s.lOrigin, 0.f, 0.f);
	glUniform2f(s.lStep, 0.f, px / (float)h);
	glUniform1i(s.lUseMask, 0);
	glBindTexture(GL_TEXTURE_2D, s.scene.texture());
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
	glViewport(vp[0], vp[1], w, h);
	glColorMask(cm[0], cm[1], cm[2], cm[3]);
	glUniform2f(s.lOrigin, (float)vp[0], (float)vp[1]);
	glUniform2f(s.lStep, px / (float)w, 0.f);
	glUniform1i(s.lUseMask, 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, s.mask.texture());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s.blurV.texture());
	// The game blends the blurred RT copy (alpha 1 over an opaque scene) normally, i.e. replaces the scene.
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Restore.
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, prevTex);
	glActiveTexture(prevActive);
	glBindVertexArray(prevVao);
	glUseProgram(prevProg);
	if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (prevScissor) glEnable(GL_SCISSOR_TEST);
	glBlendEquationSeparate(prevEqRgb, prevEqA);
	glBlendFuncSeparate(prevSrcRgb, prevDstRgb, prevSrcA, prevDstA);
	glClearColor(prevClear[0], prevClear[1], prevClear[2], prevClear[3]);
}
}
