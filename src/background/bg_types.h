#ifndef BG_TYPES_H_GUARD
#define BG_TYPES_H_GUARD

#include <cstdint>
#include <cmath>
#include <vector>
#include <string>

namespace bg {

// u4ick's stage floor sits at bg-y = +224 relative to the bgmake camera
// origin (the yellow ground line in MonoForm.cs:433 is drawn at 224 + y1).
// We subtract this from every bg y-coordinate so the floor lands on the
// editor's world y=0 — i.e. on the grid's horizontal line and, eventually,
// at a character's feet when a stage is shown behind a character.
constexpr float STAGE_FLOOR_Y = 224.0f;

// u4ick's playfield rect spans bg-x -401..+656 (width 1057, MonoForm.cs:
// 433-435), so its horizontal centre is at bg-x (-401 + 1057/2) = +127.5,
// NOT at the camera origin. We subtract this from every bg x-coordinate
// so the stage's centre lands on the editor's world x=0 / grid vertical
// line.
constexpr float STAGE_CENTER_X = 127.5f;

// Object runtime positions accumulate velocity in 1/128-px units. MBAACC's
// Background_RenderLayerWithPalette draws each object at (pos >> 7) plus the
// frame offset — i.e. pos divided by 128.
constexpr float STAGE_POS_SCALE = 1.0f / 128.0f;

// Background frame - similar to Frame_AF but simpler (no hitboxes, AS, AT, etc.)
struct Frame {
	// Raw sprite-id straight from the file. >= 10000 -> CG sprite (CG index
	// is spriteId - 10000). < 10000 -> PAT pattern index. -1 -> no sprite.
	int16_t spriteId = -1;
	int16_t offsetX = 0;
	int16_t offsetY = 0;
	int16_t duration = 1;
	
	// Draw properties
	uint8_t blendMode = 0;     // 0=normal, 2=additive
	uint8_t opacity = 255;     // 0-255
	
	// Hantei4 animation_flow (see Object::Update / han4docs):
	//   0, 1, 3 = advance to next frame (stop at last frame)
	//   2, 4, 5 = jump to jumpFrame (2=Jump, 4=Jump+landing,
	//             5=Loop check / Loop ED — this is how animations loop)
	uint8_t aniType = 1;
	uint8_t jumpFrame = 0;     // Target frame index for jump types (2/4/5)
	
	// Movement block. Verified against MBAACC Background_UpdateLayerPositions
	// (frame-relative offsets — u4ick's bgmaketool mislabeled these, it read
	// +51/+53 one byte short of the real +52/+54). On entering a frame the
	// game reloads the object's velocity/acceleration per these flags:
	//   flagClearX (+44): velX = accX = 0
	//   flagClearY (+45): velY = accY = 0
	//   flagSetX   (+46): velX = velX field, accX = accX field
	//   flagSetY   (+47): velY = velY field, accY = accY field
	uint8_t flagClearX = 0;    // +44
	uint8_t flagClearY = 0;    // +45
	uint8_t flagSetX   = 0;    // +46
	uint8_t flagSetY   = 0;    // +47
	int16_t velX = 0;          // +52
	int16_t velY = 0;          // +54
	int16_t accX = 0;          // +60
	int16_t accY = 0;          // +62
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

	// Editor-only: cleared to hide this object (layer-debugging / solo).
	// Not part of the file format.
	bool visible = true;

	// Animation state
	int32_t currentFrame = 0;
	int32_t frameDuration = 0;

	// Kinematic integrator state — MBAACC Background_UpdateLayerPositions.
	// Each tick: posX += velX; posY += velY; velX += accX; velY += accY.
	// posX/posY are 1/128-px (draw uses pos >> 7). curVel/curAcc carry over
	// between frames; a frame only resets them via its flag bytes. velLoaded
	// is cleared on every frame change so the new frame's flags get applied
	// before integration resumes. This is how bg34's orbs bob: their two
	// frames share one sprite+offset but carry +velY / -velY.
	int32_t posX = 0, posY = 0;
	int32_t curVelX = 0, curVelY = 0;
	int32_t curAccX = 0, curAccY = 0;
	bool    velLoaded = false;

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

	// u4ick snaps movingPoint_last = movingPoint instantly on mouse-up, so
	// the parallax delta jumps to zero in one frame. We instead leave
	// panLast where it is and let Settle() ease it toward panX over the
	// next handful of frames — the parallax layers slide into the
	// common-frame view instead of popping.
	void EndDrag() { dragging = false; }

	// Called once per frame. Eases panLast toward panX so the parallax
	// delta (panX - panLast) decays smoothly to zero after a drag. No-op
	// while dragging (delta must stay live) or once already settled.
	void Settle() {
		if (dragging) return;
		const float k = 0.22f;  // per-frame ease factor
		panLastX += (panX - panLastX) * k;
		panLastY += (panY - panLastY) * k;
		if (std::fabs(panX - panLastX) < 0.5f) panLastX = panX;
		if (std::fabs(panY - panLastY) < 0.5f) panLastY = panY;
	}

	// Set the stable pan position directly (used for initial centering and
	// for instantaneous pan from non-drag controls).
	void SetPan(float x, float y) { panX = panLastX = x; panY = panLastY = y; }
};

} // namespace bg

#endif /* BG_TYPES_H_GUARD */


