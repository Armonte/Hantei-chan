#ifndef BG_TYPES_H_GUARD
#define BG_TYPES_H_GUARD

#include <cstdint>
#include <cstring>
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
constexpr float STAGE_CENTER_X = 128.0f;

// VERIFIED (MBAA.exe Sprite_EmitTransformedQuad): a CG sprite's quad spans
// (bounds - (128, 224)), i.e. the CG canvas point (128, 224) is the object's
// origin — which is exactly what STAGE_CENTER_X / STAGE_FLOOR_Y undo (u4ick
// draws the canvas top-left at the object origin). PAT patterns get NO such
// pivot in the game (Background_DrawInstance PAT branch), so the PAT path
// must add these back. Stage world (0,0) == character world origin: the
// camera matrix is T(-cam/128) * zoom * T(320, 432), same as fighters.
constexpr float CG_PIVOT_X = 128.0f;
constexpr float CG_PIVOT_Y = 224.0f;

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
	
	// Hantei4 animation_flow. Decoded from MBAA.exe
	// Background_UpdateLayerAnimations @0x4b88b0 (renamed
	// Background_StepInstanceAnimations):
	//   0       = DESPAWN the instance when the frame expires (state=0; it is
	//             no longer drawn — objects come back only if a spawner
	//             command re-creates them, see bg::Runtime)
	//   1, 3    = advance to currentFrame + 1
	//   2, 4    = unconditional jump to jumpFrame
	//   5       = Loop ED — jump to jumpFrame while loopCounter > 0, else
	//             fall through to loopEnd. THIS is how looped animations end.
	uint8_t aniType = 1;
	uint8_t jumpFrame = 0;     // +12 — target frame for jump types (2/4/5)

	// Loop control, frame bytes +21/+22. Decoded from MBAA.exe
	// BackgroundLayer_UpdateLayerState @0x4b6d10. On entering a frame, if
	// loopCount != 0 the object's runtime loopCounter is (re)loaded from it.
	// A type-5 frame decrements loopCounter on expiry and jumps to jumpFrame
	// while it stays > 0; once it hits 0 it goes to loopEnd instead. Types
	// 2/4 also decrement the counter but always jump to jumpFrame.
	uint8_t loopEnd   = 0;     // +21 — fall-through target when a loop ends
	uint8_t loopCount = 0;     // +22 — loop iteration count (0 = don't reload)
	
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

	// --- fields added from the MBAA/MBAC runtime RE (docs/bg_research) ---
	// +16/+18 int16 scale X/Y (256 = 1.0, 0 = 1.0). Read ONLY by MBAC's
	// Background_DrawInstance (mbacPC 0x401ea0); MBAACC ignores them. All
	// shipped stages leave them 0.
	int16_t scaleX = 0;        // +16
	int16_t scaleY = 0;        // +18
	// +20: interpolate toward the next frame over this frame's duration —
	// CG: alpha (+10) lerp; PAT: part scale lerp (MBAC also scale/rotation).
	uint8_t interpolate = 0;   // +20
	// +100: 8 int16 indices into the object's position-TRIGGER table,
	// +116: 8 int16 indices into the object's COMMAND table (-1 = none).
	// u4ick's tool wrote 0xFF over both (= "no events").
	int16_t triggerRef[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
	int16_t commandRef[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
	// Verbatim 132-byte record as loaded; Save starts from it so bytes the
	// editor does not model (+8, +13..15, +23..43, +48..51, +56..59,
	// +64..99) survive a round trip. Frames created in the editor get the
	// u4ick default (zeros, 0xFF in +100..131).
	uint8_t raw[132] = {0};
	bool    hasRaw = false;
};

// 52-byte event record (MBAA.exe Background_RunFrameCommands @0x4b8cd0 /
// Background_RunPositionTriggers @0x4b8df0). Layout: int16 type @+0,
// int16 w2 @+2, int32 d[k] @+4*k (k = 1..12). See BG_HA4_RE.md.
//   trigger type 1: d[1]=target frame (-1 despawn) d[2]=threshold (1/128 px)
//                   d[3]=axis (0 X,1 Y) d[4]=cmp (0 pos>thr, 1 pos<thr)
//   command 1: spawn object w2 at (d[1], d[2]) relative to the spawner
//   command 2: spawn object w2 + rand % max(1, int16@+20) at random
//              x in [d[1], d[3]], y in [d[2], d[4]]
//   command 100 (w2 must be 0): axis int32@+20 (0 X, 1 Y):
//              vel = rand in [d[1], d[2]), acc = rand in [d[3], d[4])
struct EventRecord {
	int16_t type = 0;
	int16_t w2 = 0;
	int32_t d[13] = {0};        // d[0] aliases type/w2; d[1..12] = +4..+48
	uint8_t raw[52] = {0};

	// Re-derive raw[] from type/w2/d[1..12] (the only modelled fields).
	void SyncRaw() {
		std::memcpy(raw + 0, &type, 2);
		std::memcpy(raw + 2, &w2, 2);
		for (int k = 1; k < 13; ++k) std::memcpy(raw + 4 * k, &d[k], 4);
		std::memcpy(&d[0], raw, 4);
	}
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

	// Object header bytes beyond parallax/layer (MBAA.exe
	// Background_SpawnInitialInstances @0x4b6e00, BgInstance_Init @0x4b6db0):
	//   +12 int32 trigger-table offset (relative to the object, -1 = none)
	//   +16 int32 command-table offset (relative to the object, -1 = none)
	//   +20 u8    1 = NOT spawned at load (only by spawn commands)
	//   +21 u8    1 = foreground band: drawn after ALL band-0 objects, at
	//             render priority 600 (in front of the characters)
	//   +22 u8    1 = bilinear texture filtering for the whole object
	int32_t triggerTableOff = -1;
	int32_t commandTableOff = -1;
	uint8_t noAutoSpawn  = 0;
	uint8_t foreground   = 0;
	uint8_t linearFilter = 0;
	uint8_t rawHeader[60] = {0};
	bool    hasRawHeader = false;
	// The 52-byte trigger/command records live right after the frames
	// (the "unpredictable gaps" older code zero-filled). Kept verbatim and
	// written back after the frames; table offsets are shifted by the
	// frame-count delta on save.
	std::vector<uint8_t> recordBytes;
	std::vector<EventRecord> triggers;   // parsed views of recordBytes
	std::vector<EventRecord> commands;
	// Editor bookkeeping for the record tables. recordsEdited: some record's
	// fields changed (written back in place on save). recordsRelayout: a
	// record was added/removed, so Save rebuilds recordBytes as
	// [triggers][commands] and recomputes both table offsets.
	bool recordsEdited   = false;
	bool recordsRelayout = false;

	// Editor-only: cleared to hide this object (layer-debugging / solo).
	// Not part of the file format.
	bool visible = true;

	// Animation state
	int32_t currentFrame = 0;
	int32_t frameDuration = 0;

	// Loop counter — MBAA.exe RuntimeBGObject.frame_timer. (Re)loaded from a
	// frame's loopCount field on entry; type-2/4/5 frames decrement it; a
	// type-5 frame jumps while it is > 0 and exits to loopEnd once it reaches
	// 0. Without this, looped animations (aniType 5) never terminate / branch.
	int32_t loopCounter = 0;

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

// One live runtime instance — MBAA.exe's 44-byte slot (pool of 2000 at
// 0x750840). Several instances of the same object can exist at once
// (spawners re-create scrolling layers while the old copy still runs).
struct Instance {
	uint8_t state = 0;        // +0  0 free, 1 new, 2 active
	int     objIndex = -1;    // +1  index into File::GetObjects()
	int     curFrame = 0;     // +2
	int     nextFrame = 0;    // +3  precomputed by EnterFrame (interp target)
	int     loopCounter = 0;  // +4
	bool    cmdDone = false;  // +5  frame commands already run
	bool    motionLoaded = false; // +6
	int32_t posX = 0, posY = 0;   // +16/+20, 1/128 px
	int32_t velX = 0, velY = 0;   // +24/+28
	int32_t accX = 0, accY = 0;   // +32/+36
	int32_t timer = 0;        // +40
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

	// Game camera in world px (MBAA keeps it x128 at 0x55DEC4/0x55DEC8): the
	// world point drawn at the screen's horizontal centre, with world y = camY
	// on screen row 432 (Camera_UpdateMatrices: T(-cam) * S(zoom) * T(320, 432)).
	// An object with parallax p is translated by (p/256 - 1) * (-cam) before
	// that matrix (Background_DrawInstance), i.e. by (1 - p/256) * cam in
	// world space. The host derives cam from the view each frame
	// (SetGameCamFromView) so panning the stage view shows the game's parallax.
	float camX = 0.0f;
	float camY = 0.0f;
	// Parallax shift of a layer, in world px. The game builds it as
	// (p/256 - 1) * ((1, 1) * cameraMatrix) (MBAA 0x4b70cd), i.e. the camera
	// transform of the point (1, 1) rather than of the origin, which leaves a
	// sub-pixel (p/256 - 1) term: shift = (1 - p/256) * (cam - 1).
	inline float ParallaxX(int parallax) const { return (1.0f - parallax / 256.0f) * (camX - 1.0f); }
	inline float ParallaxY(int parallax) const { return (1.0f - parallax / 256.0f) * (camY - 1.0f); }
	// cam from a view of W x H pixels whose world origin sits at panLast
	// (world units) under `zoom`: the view centre is world x camX, and screen
	// row 432/480 of a 640x480 game frame is world y camY.
	// Follow the view: panning at a fixed zoom moves the game camera with it,
	// zooming never does (zoom is a pure scale of the composed stage; the game
	// camera, and so every layer's parallax shift, stays put).
	bool  camInit = false;
	float camLastPanX = 0.0f, camLastPanY = 0.0f, camLastZoom = 0.0f;
	void SetGameCamFromView(float W, float H) {
		(void)W; (void)H;
		const float z = zoom > 0.0f ? zoom : 1.0f;
		if (!camInit) { camInit = true; }
		else if (z == camLastZoom) { camX -= panLastX - camLastPanX; camY -= panLastY - camLastPanY; }
		camLastPanX = panLastX; camLastPanY = panLastY; camLastZoom = z;
	}
	// Set the camera explicitly (bg_render, inspector); the next view update
	// continues from here.
	void SetGameCam(float x, float y) { camX = x; camY = y; }

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


