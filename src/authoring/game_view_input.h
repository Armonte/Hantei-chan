#ifndef AUTHORING_GAME_VIEW_INPUT_H_GUARD
#define AUTHORING_GAME_VIEW_INPUT_H_GUARD
// [game-view] The Game panel's input -> LinkInputInject (docs/HANTEI_AUTHORING_MODE.md §12.2). Pure: the panel fills
// GameViewKeys from ImGui's input events (keyboard + gamepad, never a key poll of the OS) while it is focused; this turns
// them into the wire struct. SOCD: left + right = neutral horizontally, up + down = neutral vertically.
#include "../game_link_proto.h"

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

} // namespace authoring

#endif
