#ifndef AUTHORING_GAME_VIEW_INPUT_H_GUARD
#define AUTHORING_GAME_VIEW_INPUT_H_GUARD
// [game-view] The Game panel's input -> LinkInputInject (docs/HANTEI_AUTHORING_MODE.md §12.2). Pure: the panel fills
// GameViewKeys from ImGui's input events (keyboard + gamepad, never a key poll of the OS) while it is focused; this turns
// them into the wire struct. SOCD: left + right = neutral horizontally, up + down = neutral vertically.
#include "../game_frame_share.h"
#include "../game_link_proto.h"

#include <string>

namespace authoring {

struct GameViewKeys {
	bool up = false, down = false, left = false, right = false;
	bool a = false, b = false, c = false, d = false, e = false, fn2 = false, start = false;
};

inline uint8_t NumpadDirection(const GameViewKeys& k)
{
	const int h = (k.right ? 1 : 0) - (k.left ? 1 : 0);
	const int v = (k.up ? 1 : 0) - (k.down ? 1 : 0);
	return (uint8_t)(5 + h + 3 * v);   // 1 2 3 / 4 5 6 / 7 8 9
}

inline uint8_t InjectButtons(const GameViewKeys& k)
{
	namespace w = gamelink::wire;
	return (uint8_t)((k.a ? w::kInjBtnA : 0) | (k.b ? w::kInjBtnB : 0) | (k.c ? w::kInjBtnC : 0) | (k.d ? w::kInjBtnD : 0) |
	                 (k.e ? w::kInjBtnE : 0) | (k.fn2 ? w::kInjBtnFN2 : 0) | (k.start ? w::kInjBtnStart : 0));
}

inline gamelink::wire::InputInject MakeInject(const GameViewKeys& k, uint8_t player, uint32_t serial, uint16_t holdFrames)
{
	gamelink::wire::InputInject in{};
	in.version = gamelink::wire::kInjectVersion;
	in.player = player;
	in.direction = NumpadDirection(k);
	in.buttons = InjectButtons(k);
	in.holdFrames = holdFrames;
	in.serial = serial;
	return in;
}

inline gamelink::wire::InputInject MakeRelease(uint8_t player, uint32_t serial)
{
	gamelink::wire::InputInject in{};
	in.version = gamelink::wire::kInjectVersion;
	in.player = player;
	in.flags = gamelink::wire::kInjFlagRelease;
	in.direction = 5;
	in.holdFrames = 1;
	in.serial = serial;
	return in;
}

// A world point (1/128 px) -> frame pixels, for the overlay (§12.3 CHANGED by PC agent: cameraX/Y is the camera AS
// DRAWN, shakeX/Y are 0 and are NOT added; the game's 640x480 picture sits in FrameCamera.view (78,0 468x351 between
// sidebars in a 624x351 frame), 0 = it fills the frame).
struct FramePoint { float x, y, scale; };   // scale = frame px per game px (box sizes)
inline FramePoint WorldToFrame(const framering::FrameSlotHeader& f, int32_t wx, int32_t wy)
{
	const framering::FrameCamera& c = f.camera;
	const float zoom = c.zoomX1000 ? c.zoomX1000 / 1000.0f : 1.0f;
	const float vw = c.viewW ? (float)c.viewW : (float)f.width, vh = c.viewH ? (float)c.viewH : (float)f.height;
	FramePoint p;
	p.x = (float)c.viewX + (((wx - c.cameraX) / 128.0f) * zoom + 320.0f) * (vw / 640.0f);
	p.y = (float)c.viewY + (((wy - c.cameraY) / 128.0f) * zoom + 432.0f) * (vh / 480.0f);
	p.scale = zoom * (vw / 640.0f);
	return p;
}

// Can the panel inject as this player? "" = yes. §12.3: pchost owns only the P1 / P2 pad words (P3 / P4 answer
// Unsupported), and P2 answers Unsupported while the Training dummy drives it (g_GameModeKind 0x1010).
inline std::string PlayerBlockReason(int player, uint32_t gameModeKind)
{
	if (player < 0 || player > 3) return "no such player";
	if (player >= 2) return "P3 / P4 cannot be injected (pchost owns only the P1 / P2 pad words)";
	if (player == 1 && gameModeKind == 0x1010) return "P2 is the Training dummy's in Training: inject as P1";
	return {};
}

} // namespace authoring

#endif
