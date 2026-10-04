#ifndef RENDER_TARGET_H_GUARD
#define RENDER_TARGET_H_GUARD

// Off-screen render targets for character views, detached windows and PNG
// export (docs/HANTEI_WAVE2.md §2).
//
// RenderTarget owns one framebuffer object: an RGBA8 colour texture (which
// ImGui can display directly) plus a depth/stencil renderbuffer. It is sized
// lazily with ensure(); resizing keeps the same texture name, so an ImGui
// image recorded earlier in the frame still points at the right object.
//
// ScopedTargetBinding binds a target for one render pass and restores every
// piece of GL state it touched (framebuffer bindings, viewport, scissor,
// colour mask, clear colour) when it goes out of scope, including on an early
// return. Nothing else about a pass is global: the camera and pass size are
// passed to Render::BeginPass explicitly by the caller.

#include <cstdint>
#include <vector>

class RenderTarget
{
public:
	RenderTarget() = default;
	~RenderTarget();
	RenderTarget(const RenderTarget&) = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;

	// (Re)allocate for width x height (clamped to [1, 8192]). Returns false
	// if the framebuffer is incomplete. `linear` selects the texture filter
	// used when ImGui scales the image.
	bool ensure(int width, int height, bool linear = false);
	void release();

	bool valid() const { return m_fbo != 0; }
	unsigned framebuffer() const { return m_fbo; }
	unsigned texture() const { return m_color; }
	int width() const { return m_width; }
	int height() const { return m_height; }

	// Read the colour attachment as top-down RGBA8 rows.
	bool readRgba(std::vector<uint8_t>& out) const;

	// Copy the whole target to the default framebuffer at (0,0).
	void blitToDefault(int dstWidth, int dstHeight) const;

	// Number of live targets (diagnostics).
	static int liveCount();

private:
	unsigned m_fbo = 0;
	unsigned m_color = 0;
	unsigned m_depth = 0;
	int m_width = 0;
	int m_height = 0;
	bool m_linear = false;
};

class ScopedTargetBinding
{
public:
	// Binds `target` as the draw and read framebuffer, sets the viewport to
	// its size and disables the scissor test (ImGui leaves one enabled).
	explicit ScopedTargetBinding(const RenderTarget& target);
	~ScopedTargetBinding();
	ScopedTargetBinding(const ScopedTargetBinding&) = delete;
	ScopedTargetBinding& operator=(const ScopedTargetBinding&) = delete;

	// Clear colour + depth. With opaque == true the alpha channel is then
	// write-protected for the rest of the pass, so translucent sprites cannot
	// punch holes into an image ImGui later draws with alpha blending.
	void clear(float r, float g, float b, float a, bool opaque);

private:
	int m_prevDraw = 0, m_prevRead = 0;
	int m_prevViewport[4] = {0, 0, 0, 0};
	bool m_prevScissor = false;
	unsigned char m_prevMask[4] = {1, 1, 1, 1};
	float m_prevClear[4] = {0, 0, 0, 0};
};

#endif /* RENDER_TARGET_H_GUARD */
