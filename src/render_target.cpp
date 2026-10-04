#include "render_target.h"

#include <glad/glad.h>
#include <algorithm>

namespace {
int g_liveTargets = 0;
}

RenderTarget::~RenderTarget()
{
	release();
}

int RenderTarget::liveCount()
{
	return g_liveTargets;
}

void RenderTarget::release()
{
	if (m_depth) glDeleteRenderbuffers(1, &m_depth);
	if (m_color) glDeleteTextures(1, &m_color);
	if (m_fbo) {
		glDeleteFramebuffers(1, &m_fbo);
		--g_liveTargets;
	}
	m_fbo = m_color = m_depth = 0;
	m_width = m_height = 0;
}

bool RenderTarget::ensure(int width, int height, bool linear)
{
	width = std::clamp(width, 1, 8192);
	height = std::clamp(height, 1, 8192);
	if (m_fbo && width == m_width && height == m_height && linear == m_linear)
		return true;

	GLint prevDraw = 0, prevRead = 0, prevTex = 0, prevRb = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRb);

	if (!m_fbo) {
		glGenFramebuffers(1, &m_fbo);
		glGenTextures(1, &m_color);
		glGenRenderbuffers(1, &m_depth);
		++g_liveTargets;
	}

	glBindTexture(GL_TEXTURE_2D, m_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depth);
	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
	glBindTexture(GL_TEXTURE_2D, prevTex);
	glBindRenderbuffer(GL_RENDERBUFFER, prevRb);

	if (!complete) {
		release();
		return false;
	}
	m_width = width;
	m_height = height;
	m_linear = linear;
	return true;
}

bool RenderTarget::readRgba(std::vector<uint8_t>& out) const
{
	if (!m_fbo) return false;
	GLint prevRead = 0, prevPack = 4;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
	glGetIntegerv(GL_PACK_ALIGNMENT, &prevPack);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	std::vector<uint8_t> bottomUp((size_t)m_width * m_height * 4);
	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
	const bool ok = glGetError() == GL_NO_ERROR;
	glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
	if (!ok) return false;
	out.resize(bottomUp.size());
	const size_t row = (size_t)m_width * 4;
	for (int y = 0; y < m_height; ++y)
		std::copy_n(bottomUp.data() + (size_t)(m_height - 1 - y) * row, row, out.data() + (size_t)y * row);
	return true;
}

void RenderTarget::blitToDefault(int dstWidth, int dstHeight) const
{
	if (!m_fbo) return;
	GLint prevDraw = 0, prevRead = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
	const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	glDisable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, dstWidth, dstHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
	if (scissor) glEnable(GL_SCISSOR_TEST);
}

ScopedTargetBinding::ScopedTargetBinding(const RenderTarget& target)
{
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_prevDraw);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_prevRead);
	glGetIntegerv(GL_VIEWPORT, m_prevViewport);
	m_prevScissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
	GLboolean mask[4];
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	for (int i = 0; i < 4; ++i) m_prevMask[i] = mask[i];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, m_prevClear);

	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
	glViewport(0, 0, target.width(), target.height());
	glDisable(GL_SCISSOR_TEST);
}

ScopedTargetBinding::~ScopedTargetBinding()
{
	glColorMask(m_prevMask[0], m_prevMask[1], m_prevMask[2], m_prevMask[3]);
	glClearColor(m_prevClear[0], m_prevClear[1], m_prevClear[2], m_prevClear[3]);
	if (m_prevScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_prevDraw);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_prevRead);
}

void ScopedTargetBinding::clear(float r, float g, float b, float a, bool opaque)
{
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (opaque) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
}
