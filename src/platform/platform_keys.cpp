// SDL scancode <-> Win32 virtual-key code (PLAN.md 6 P0 step 3).
//
// Every binding in src/shortcut_router.*, the ini key overrides and
// --post-key <vk> are virtual-key codes, so the SDL loop translates scancodes
// instead of changing the binding model. The table covers the keys the editor
// binds plus the usual editing set; 0 means "no Win32 equivalent" and the event
// is dropped (the old WndProc never saw those either).
//
// Letters and digits use the VK codes Win32 defines for them (VK_A = 0x41 is
// also ASCII 'A', VK_0 = 0x30 is ASCII '0'), which is what the registry's
// comments assume.

#include "platform.h"

#include <SDL3/SDL.h>

namespace platform {
namespace {

struct KeyPair { SDL_Scancode sc; uint32_t vk; };

const KeyPair kKeys[] = {
	// letters
	{ SDL_SCANCODE_A, 0x41 }, { SDL_SCANCODE_B, 0x42 }, { SDL_SCANCODE_C, 0x43 }, { SDL_SCANCODE_D, 0x44 },
	{ SDL_SCANCODE_E, 0x45 }, { SDL_SCANCODE_F, 0x46 }, { SDL_SCANCODE_G, 0x47 }, { SDL_SCANCODE_H, 0x48 },
	{ SDL_SCANCODE_I, 0x49 }, { SDL_SCANCODE_J, 0x4A }, { SDL_SCANCODE_K, 0x4B }, { SDL_SCANCODE_L, 0x4C },
	{ SDL_SCANCODE_M, 0x4D }, { SDL_SCANCODE_N, 0x4E }, { SDL_SCANCODE_O, 0x4F }, { SDL_SCANCODE_P, 0x50 },
	{ SDL_SCANCODE_Q, 0x51 }, { SDL_SCANCODE_R, 0x52 }, { SDL_SCANCODE_S, 0x53 }, { SDL_SCANCODE_T, 0x54 },
	{ SDL_SCANCODE_U, 0x55 }, { SDL_SCANCODE_V, 0x56 }, { SDL_SCANCODE_W, 0x57 }, { SDL_SCANCODE_X, 0x58 },
	{ SDL_SCANCODE_Y, 0x59 }, { SDL_SCANCODE_Z, 0x5A },
	// top-row digits
	{ SDL_SCANCODE_1, 0x31 }, { SDL_SCANCODE_2, 0x32 }, { SDL_SCANCODE_3, 0x33 }, { SDL_SCANCODE_4, 0x34 },
	{ SDL_SCANCODE_5, 0x35 }, { SDL_SCANCODE_6, 0x36 }, { SDL_SCANCODE_7, 0x37 }, { SDL_SCANCODE_8, 0x38 },
	{ SDL_SCANCODE_9, 0x39 }, { SDL_SCANCODE_0, 0x30 },
	// function keys
	{ SDL_SCANCODE_F1, 0x70 }, { SDL_SCANCODE_F2, 0x71 }, { SDL_SCANCODE_F3, 0x72 }, { SDL_SCANCODE_F4, 0x73 },
	{ SDL_SCANCODE_F5, 0x74 }, { SDL_SCANCODE_F6, 0x75 }, { SDL_SCANCODE_F7, 0x76 }, { SDL_SCANCODE_F8, 0x77 },
	{ SDL_SCANCODE_F9, 0x78 }, { SDL_SCANCODE_F10, 0x79 }, { SDL_SCANCODE_F11, 0x7A }, { SDL_SCANCODE_F12, 0x7B },
	// editing + navigation
	{ SDL_SCANCODE_ESCAPE, 0x1B }, { SDL_SCANCODE_TAB, 0x09 }, { SDL_SCANCODE_BACKSPACE, 0x08 },
	{ SDL_SCANCODE_RETURN, 0x0D }, { SDL_SCANCODE_SPACE, 0x20 },
	{ SDL_SCANCODE_INSERT, 0x2D }, { SDL_SCANCODE_DELETE, 0x2E }, { SDL_SCANCODE_HOME, 0x24 },
	{ SDL_SCANCODE_END, 0x23 }, { SDL_SCANCODE_PAGEUP, 0x21 }, { SDL_SCANCODE_PAGEDOWN, 0x22 },
	{ SDL_SCANCODE_LEFT, 0x25 }, { SDL_SCANCODE_UP, 0x26 }, { SDL_SCANCODE_RIGHT, 0x27 }, { SDL_SCANCODE_DOWN, 0x28 },
	{ SDL_SCANCODE_PRINTSCREEN, 0x2C }, { SDL_SCANCODE_SCROLLLOCK, 0x91 }, { SDL_SCANCODE_PAUSE, 0x13 },
	// modifiers and locks
	{ SDL_SCANCODE_LSHIFT, 0xA0 }, { SDL_SCANCODE_RSHIFT, 0xA1 },
	{ SDL_SCANCODE_LCTRL, 0xA2 }, { SDL_SCANCODE_RCTRL, 0xA3 },
	{ SDL_SCANCODE_LALT, 0xA4 }, { SDL_SCANCODE_RALT, 0xA5 },
	{ SDL_SCANCODE_LGUI, 0x5B }, { SDL_SCANCODE_RGUI, 0x5C },
	{ SDL_SCANCODE_CAPSLOCK, 0x14 }, { SDL_SCANCODE_NUMLOCKCLEAR, 0x90 },
	// numpad (VK_NUMPAD0..9 and the operators; VK_MULTIPLY/ADD/SUBTRACT/DECIMAL/DIVIDE)
	{ SDL_SCANCODE_KP_0, 0x60 }, { SDL_SCANCODE_KP_1, 0x61 }, { SDL_SCANCODE_KP_2, 0x62 }, { SDL_SCANCODE_KP_3, 0x63 },
	{ SDL_SCANCODE_KP_4, 0x64 }, { SDL_SCANCODE_KP_5, 0x65 }, { SDL_SCANCODE_KP_6, 0x66 }, { SDL_SCANCODE_KP_7, 0x67 },
	{ SDL_SCANCODE_KP_8, 0x68 }, { SDL_SCANCODE_KP_9, 0x69 },
	{ SDL_SCANCODE_KP_MULTIPLY, 0x6A }, { SDL_SCANCODE_KP_PLUS, 0x6B }, { SDL_SCANCODE_KP_MINUS, 0x6D },
	{ SDL_SCANCODE_KP_PERIOD, 0x6E }, { SDL_SCANCODE_KP_DIVIDE, 0x6F }, { SDL_SCANCODE_KP_ENTER, 0x0D },
	// punctuation (OEM keys: the codes Windows reports for a US layout)
	{ SDL_SCANCODE_MINUS, 0xBD }, { SDL_SCANCODE_EQUALS, 0xBB },
	{ SDL_SCANCODE_LEFTBRACKET, 0xDB }, { SDL_SCANCODE_RIGHTBRACKET, 0xDD },
	{ SDL_SCANCODE_BACKSLASH, 0xDC }, { SDL_SCANCODE_SEMICOLON, 0xBA },
	{ SDL_SCANCODE_APOSTROPHE, 0xDE }, { SDL_SCANCODE_GRAVE, 0xC0 },
	{ SDL_SCANCODE_COMMA, 0xBC }, { SDL_SCANCODE_PERIOD, 0xBE }, { SDL_SCANCODE_SLASH, 0xBF },
};

}  // namespace

uint32_t VkFromScancode(int sdlScancode)
{
	for (const KeyPair& k : kKeys)
		if ((int)k.sc == sdlScancode) return k.vk;
	return 0;
}

int ScancodeFromVk(uint32_t vk)
{
	for (const KeyPair& k : kKeys)
		if (k.vk == vk) return (int)k.sc;
	return 0;
}

}  // namespace platform
