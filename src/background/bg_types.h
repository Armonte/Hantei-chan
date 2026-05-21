#ifndef BG_TYPES_H_GUARD
#define BG_TYPES_H_GUARD

#include <cstdint>
#include <vector>
#include <string>

namespace bg {

// Background frame - similar to Frame_AF but simpler (no hitboxes, AS, AT, etc.)
struct Frame {
	// Core rendering data (same as Frame_AF)
	int16_t spriteId = -1;
	int16_t offsetX = 0;
	int16_t offsetY = 0;
	int16_t duration = 1;
	
	// Draw properties
	uint8_t blendMode = 0;     // 0=normal, 2=additive
	uint8_t opacity = 255;     // 0-255
	
	// Animation control. Per u4ick's stepping (see Object::Update):
	//   0, 1 = normal frame (advance, stop at last frame)
	//   2    = jump to jumpFrame (this is how animations loop)
	uint8_t aniType = 1;
	uint8_t jumpFrame = 0;     // Target frame index when aniType==2
	
	// Movement vectors (bgmake-specific)
	uint8_t enableXVec = 0;
	uint8_t enableYVec = 0;
	int16_t xVec = 0;
	int16_t yVec = 0;
	
	// Runtime computed position (for vector movement)
	float runtimeX = 0.0f;
	float runtimeY = 0.0f;
};

// Background object - collection of frames with parallax/layer
struct Object {
	std::string name;
	int32_t parallax = 256;    // 256 = 1.0x camera speed
	int32_t layer = 128;       // Higher = render in front

	std::vector<Frame> frames;

	// Original slot in the file's 256-entry offset table. Preserved so that
	// sparse files (e.g. bg01 has objects at indices 0..11, 13, 14, 19, 21,
	// 23, 25, 26) round-trip without collapsing into dense 0..N indices.
	// -1 means editor-created (placed at the next free slot on save).
	int32_t originalIndex = -1;

	// Original file byte offset this object lived at on load. bg01 has
	// 52-byte gaps between some objects that we can't predict from frame
	// counts alone — keeping the original offset lets Save reproduce them.
	// -1 means "no preference, pack tightly after the previous object."
	int32_t originalOffset = -1;

	// Animation state
	int32_t currentFrame = 0;
	int32_t frameDuration = 0;

	void Update();
	void Reset();
};

// Camera for parallax calculations. Mirrors u4ick's bgmaketool model where
// `panX/Y` is the live drag position and `panLastX/Y` is the previous stable
// position; parallax shows as the difference between them scaled by per-object
// parallax. After a drag ends both pairs are equal again and the rendering
// reduces to `screen = panLast + offset`.
struct Camera {
	float panX = 0.0f;       // movingPoint.X    (live during drag)
	float panY = 0.0f;       // movingPoint.Y
	float panLastX = 0.0f;   // movingPoint_last.X (stable, between drags)
	float panLastY = 0.0f;   // movingPoint_last.Y
	float zoom = 1.0f;
	bool dragging = false;

	// Sprite *world* position for a given object/frame offset. u4ick's
	// MonoForm.cs:253-254 formula is `panLast + (pan - panLast) * f + off`
	// in screen space; the `panLast +` part is the screen anchor (where
	// world (0, 0) sits on screen). We let mainRender's transform handle
	// that anchor (via render.x/y in the host editor), so this returns just
	// the offset plus the live parallax delta. Result == `xOff` when not
	// mid-drag.
	inline float ScreenX(float xOff, int parallax) const {
		float f = parallax / 256.0f;
		return xOff + (panX - panLastX) * f;
	}
	inline float ScreenY(float yOff, int parallax) const {
		float f = parallax / 256.0f;
		return yOff + (panY - panLastY) * f;
	}

	// Drag lifecycle: hands tracking to call BeginDrag on mouse-down,
	// UpdateDrag on move, EndDrag on mouse-up — mirrors Form1.cs:153-191.
	void BeginDrag() { dragging = true; }
	void UpdateDrag(float dx, float dy) { if (dragging) { panX = panLastX + dx; panY = panLastY + dy; } }
	void EndDrag() { panLastX = panX; panLastY = panY; dragging = false; }

	// Set the stable pan position directly (used for initial centering and
	// for instantaneous pan from non-drag controls).
	void SetPan(float x, float y) { panX = panLastX = x; panY = panLastY = y; }
};

} // namespace bg

#endif /* BG_TYPES_H_GUARD */


