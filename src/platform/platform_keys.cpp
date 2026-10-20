// SDL key event <-> Win32 virtual-key code (PLAN.md 6 P0 step 3).
//
// Every binding in src/shortcut_router.*, the ini key overrides and
// --post-key <vk> are virtual-key codes, so the SDL loop translates each key
// event into the code the old WndProc got in WM_KEYDOWN's wParam instead of
// changing the binding model.
//
// On Windows the code comes from the event's raw scan code through the active
// keyboard LAYOUT (MapVirtualKey(raw, MAPVK_VSC_TO_VK_EX)), which is what the
// keyboard driver did for WM_KEYDOWN: on AZERTY the key left of Z is VK_A, the
// OEM keys follow the layout, and keys the fixed table below does not list
// (VK_OEM_102, VK_APPS, F13-F24, the JIS keys) still arrive. Three things the
// scan-code mapping cannot know are fixed up here the way the driver does:
// the numeric keypad (NumLock), and Pause / NumLock, whose scan codes SDL folds.
// The table is the fallback for an event without a raw code, and the whole
// mapping off Windows (P5); letters there come from the layout-aware keycode.
//
// Letters and digits use the VK codes Win32 defines for them (VK_A = 0x41 is
// also ASCII 'A', VK_0 = 0x30 is ASCII '0'), which is what the registry's
// comments assume.

#include "platform.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
	{ SDL_SCANCODE_F13, 0x7C }, { SDL_SCANCODE_F14, 0x7D }, { SDL_SCANCODE_F15, 0x7E }, { SDL_SCANCODE_F16, 0x7F },
	{ SDL_SCANCODE_F17, 0x80 }, { SDL_SCANCODE_F18, 0x81 }, { SDL_SCANCODE_F19, 0x82 }, { SDL_SCANCODE_F20, 0x83 },
	{ SDL_SCANCODE_F21, 0x84 }, { SDL_SCANCODE_F22, 0x85 }, { SDL_SCANCODE_F23, 0x86 }, { SDL_SCANCODE_F24, 0x87 },
	// editing + navigation
	{ SDL_SCANCODE_ESCAPE, 0x1B }, { SDL_SCANCODE_TAB, 0x09 }, { SDL_SCANCODE_BACKSPACE, 0x08 },
	{ SDL_SCANCODE_RETURN, 0x0D }, { SDL_SCANCODE_SPACE, 0x20 },
	{ SDL_SCANCODE_INSERT, 0x2D }, { SDL_SCANCODE_DELETE, 0x2E }, { SDL_SCANCODE_HOME, 0x24 },
	{ SDL_SCANCODE_END, 0x23 }, { SDL_SCANCODE_PAGEUP, 0x21 }, { SDL_SCANCODE_PAGEDOWN, 0x22 },
	{ SDL_SCANCODE_LEFT, 0x25 }, { SDL_SCANCODE_UP, 0x26 }, { SDL_SCANCODE_RIGHT, 0x27 }, { SDL_SCANCODE_DOWN, 0x28 },
	{ SDL_SCANCODE_PRINTSCREEN, 0x2C }, { SDL_SCANCODE_SCROLLLOCK, 0x91 }, { SDL_SCANCODE_PAUSE, 0x13 },
	{ SDL_SCANCODE_APPLICATION, 0x5D },   // VK_APPS (the context-menu key)
	{ SDL_SCANCODE_CLEAR, 0x0C },
	// modifiers and locks. WM_KEYDOWN reported the generic VK_SHIFT/VK_CONTROL/
	// VK_MENU for either side (only the Windows keys are sided), so these are
	// the generic codes too.
	{ SDL_SCANCODE_LSHIFT, 0x10 }, { SDL_SCANCODE_RSHIFT, 0x10 },
	{ SDL_SCANCODE_LCTRL, 0x11 }, { SDL_SCANCODE_RCTRL, 0x11 },
	{ SDL_SCANCODE_LALT, 0x12 }, { SDL_SCANCODE_RALT, 0x12 },
	{ SDL_SCANCODE_LGUI, 0x5B }, { SDL_SCANCODE_RGUI, 0x5C },
	{ SDL_SCANCODE_CAPSLOCK, 0x14 }, { SDL_SCANCODE_NUMLOCKCLEAR, 0x90 },
	// numpad with NumLock on (VK_NUMPAD0..9 and the operators; VK_MULTIPLY/ADD/SUBTRACT/DECIMAL/DIVIDE).
	// NumLock off is NumpadVk below.
	{ SDL_SCANCODE_KP_0, 0x60 }, { SDL_SCANCODE_KP_1, 0x61 }, { SDL_SCANCODE_KP_2, 0x62 }, { SDL_SCANCODE_KP_3, 0x63 },
	{ SDL_SCANCODE_KP_4, 0x64 }, { SDL_SCANCODE_KP_5, 0x65 }, { SDL_SCANCODE_KP_6, 0x66 }, { SDL_SCANCODE_KP_7, 0x67 },
	{ SDL_SCANCODE_KP_8, 0x68 }, { SDL_SCANCODE_KP_9, 0x69 },
	{ SDL_SCANCODE_KP_MULTIPLY, 0x6A }, { SDL_SCANCODE_KP_PLUS, 0x6B }, { SDL_SCANCODE_KP_MINUS, 0x6D },
	{ SDL_SCANCODE_KP_PERIOD, 0x6E }, { SDL_SCANCODE_KP_DIVIDE, 0x6F }, { SDL_SCANCODE_KP_ENTER, 0x0D },
	{ SDL_SCANCODE_KP_COMMA, 0x6C },      // VK_SEPARATOR (ABNT keypad)
	// punctuation (OEM keys: the codes Windows reports for a US layout)
	{ SDL_SCANCODE_MINUS, 0xBD }, { SDL_SCANCODE_EQUALS, 0xBB },
	{ SDL_SCANCODE_LEFTBRACKET, 0xDB }, { SDL_SCANCODE_RIGHTBRACKET, 0xDD },
	{ SDL_SCANCODE_BACKSLASH, 0xDC }, { SDL_SCANCODE_SEMICOLON, 0xBA },
	{ SDL_SCANCODE_APOSTROPHE, 0xDE }, { SDL_SCANCODE_GRAVE, 0xC0 },
	{ SDL_SCANCODE_COMMA, 0xBC }, { SDL_SCANCODE_PERIOD, 0xBE }, { SDL_SCANCODE_SLASH, 0xBF },
	{ SDL_SCANCODE_NONUSBACKSLASH, 0xE2 },   // VK_OEM_102 (ISO <> key)
	// JIS / Korean keys (the codes the Japanese and Korean layouts report)
	{ SDL_SCANCODE_INTERNATIONAL1, 0xE2 },   // Ro: VK_OEM_102
	{ SDL_SCANCODE_INTERNATIONAL2, 0x15 },   // Katakana/Hiragana: VK_KANA
	{ SDL_SCANCODE_INTERNATIONAL3, 0xDC },   // Yen: VK_OEM_5
	{ SDL_SCANCODE_INTERNATIONAL4, 0x1C },   // Henkan: VK_CONVERT
	{ SDL_SCANCODE_INTERNATIONAL5, 0x1D },   // Muhenkan: VK_NONCONVERT
	{ SDL_SCANCODE_LANG1, 0x15 },            // Hangul: VK_HANGUL
	{ SDL_SCANCODE_LANG2, 0x19 },            // Hanja: VK_HANJA
	// media keys
	{ SDL_SCANCODE_MUTE, 0xAD }, { SDL_SCANCODE_VOLUMEDOWN, 0xAE }, { SDL_SCANCODE_VOLUMEUP, 0xAF },
	{ SDL_SCANCODE_MEDIA_NEXT_TRACK, 0xB0 }, { SDL_SCANCODE_MEDIA_PREVIOUS_TRACK, 0xB1 },
	{ SDL_SCANCODE_MEDIA_STOP, 0xB2 }, { SDL_SCANCODE_MEDIA_PLAY_PAUSE, 0xB3 },
};

uint32_t TableVk(int sc)
{
	for (const KeyPair& k : kKeys)
		if ((int)k.sc == sc) return k.vk;
	return 0;
}

// The sided modifier codes MapVirtualKey returns for a scan code, as the
// generic codes WM_KEYDOWN carried.
uint32_t GenericModifierVk(uint32_t vk)
{
	switch (vk) {
	case 0xA0: case 0xA1: return 0x10;   // VK_LSHIFT / VK_RSHIFT -> VK_SHIFT
	case 0xA2: case 0xA3: return 0x11;   // VK_LCONTROL / VK_RCONTROL -> VK_CONTROL
	case 0xA4: case 0xA5: return 0x12;   // VK_LMENU / VK_RMENU -> VK_MENU
	}
	return vk;
}

// The numeric keypad's digit keys and its decimal key. The keyboard driver
// turns them into VK_NUMPADn / VK_DECIMAL only with NumLock on and Shift up;
// otherwise into the navigation key printed under the digit (Shift with NumLock
// on also releases Shift for that key). Returns 0 for any other key.
bool IsNumpadDigit(int sc)
{
	return (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_0) || sc == SDL_SCANCODE_KP_PERIOD;
}

uint32_t NumpadVk(int sc, uint16_t mod)
{
	if (!IsNumpadDigit(sc)) return 0;
	if (NumpadIsDigitMode(mod)) return TableVk(sc);
	switch (sc) {
	case SDL_SCANCODE_KP_7: return 0x24;       // VK_HOME
	case SDL_SCANCODE_KP_8: return 0x26;       // VK_UP
	case SDL_SCANCODE_KP_9: return 0x21;       // VK_PRIOR
	case SDL_SCANCODE_KP_4: return 0x25;       // VK_LEFT
	case SDL_SCANCODE_KP_5: return 0x0C;       // VK_CLEAR
	case SDL_SCANCODE_KP_6: return 0x27;       // VK_RIGHT
	case SDL_SCANCODE_KP_1: return 0x23;       // VK_END
	case SDL_SCANCODE_KP_2: return 0x28;       // VK_DOWN
	case SDL_SCANCODE_KP_3: return 0x22;       // VK_NEXT
	case SDL_SCANCODE_KP_0: return 0x2D;       // VK_INSERT
	case SDL_SCANCODE_KP_PERIOD: return 0x2E;  // VK_DELETE
	}
	return 0;
}

#ifdef _WIN32
uint32_t WinVscToVk(uint32_t raw)
{
	return (uint32_t)MapVirtualKeyW(raw, MAPVK_VSC_TO_VK_EX);
}

// True when the active keyboard layout has AltGr: some character on it needs
// Ctrl+Alt (VkKeyScanEx shift state 6). Cached per layout handle.
bool LayoutHasAltGr()
{
	static HKL cached = nullptr;
	static bool has = false;
	const HKL hkl = GetKeyboardLayout(0);
	if (hkl != cached) {
		cached = hkl;
		has = false;
		for (WCHAR ch = 0x20; ch < 0x250 && !has; ch++) {
			const SHORT r = VkKeyScanExW(ch, hkl);
			if (r != -1 && (HIBYTE(r) & 6) == 6) has = true;
		}
	}
	return has;
}
#endif

}  // namespace

bool NumpadIsDigitMode(uint16_t mod)
{
	return (mod & SDL_KMOD_NUM) && !(mod & SDL_KMOD_SHIFT);
}

uint32_t VkFromKeyParts(const KeyParts& k, uint32_t (*vscToVk)(uint32_t raw))
{
	// Scan codes SDL folds: it reports Pause as E0 46 (Ctrl+Break's code, which
	// the layout maps to VK_CANCEL), and NumLock's E0 45 is Pause's code too.
	if (k.scancode == SDL_SCANCODE_PAUSE) return 0x13;          // VK_PAUSE
	if (k.scancode == SDL_SCANCODE_NUMLOCKCLEAR) return 0x90;   // VK_NUMLOCK
	// The keypad depends on NumLock, which the scan-code mapping ignores.
	if (const uint32_t vk = NumpadVk(k.scancode, k.mod)) return vk;
	// Everything else goes through the keyboard layout, as WM_KEYDOWN did.
	if (k.raw && vscToVk) {
		if (const uint32_t vk = vscToVk(k.raw)) return GenericModifierVk(vk);
	}
	// No raw code (a pushed event, or a platform without one): letters follow
	// the layout through the keycode, the rest by position.
	if (k.keycode >= 'a' && k.keycode <= 'z') return k.keycode - 'a' + 'A';
	return TableVk(k.scancode);
}

uint32_t VkFromKeyEvent(int sdlScancode, uint32_t keycode, uint16_t mod, uint16_t raw)
{
	KeyParts k;
	k.scancode = sdlScancode;
	k.keycode = keycode;
	k.mod = mod;
	k.raw = raw;
#ifdef _WIN32
	return VkFromKeyParts(k, WinVscToVk);
#else
	return VkFromKeyParts(k, nullptr);
#endif
}

uint32_t ChordModsFromKeyLayout(int sdlScancode, uint16_t mod, bool altGrLayout)
{
	uint32_t m = 0;
	if (mod & SDL_KMOD_CTRL) m |= kModCtrl;
	if (mod & SDL_KMOD_SHIFT) m |= kModShift;
	if (mod & SDL_KMOD_ALT) m |= kModAlt;
	// AltGr on a layout that has it: Windows sends a fake left Ctrl with the
	// right Alt (SDL drops that key, so the event's mod has RALT only), and
	// GetKeyState(VK_CONTROL) saw it, so the chord was Ctrl+Alt.
	if (altGrLayout && (mod & SDL_KMOD_RALT)) m |= kModCtrl;
	// Shift + a keypad digit with NumLock on: the driver lifts Shift around the
	// key (a fake Shift up / down), so WM_KEYDOWN saw VK_UP etc. with Shift up.
	if (IsNumpadDigit(sdlScancode) && (mod & SDL_KMOD_NUM) && (mod & SDL_KMOD_SHIFT)) m &= ~kModShift;
	return m;
}

uint32_t ChordModsFromKey(int sdlScancode, uint16_t mod)
{
#ifdef _WIN32
	return ChordModsFromKeyLayout(sdlScancode, mod, LayoutHasAltGr());
#else
	return ChordModsFromKeyLayout(sdlScancode, mod, false);
#endif
}

bool IsModifierOnlyVk(uint32_t vk)
{
	switch (vk) {
	case 0x10: case 0x11: case 0x12:   // VK_SHIFT / VK_CONTROL / VK_MENU
	case 0xA0: case 0xA1:              // VK_LSHIFT / VK_RSHIFT
	case 0xA2: case 0xA3:              // VK_LCONTROL / VK_RCONTROL
	case 0xA4: case 0xA5:              // VK_LMENU / VK_RMENU (Alt, AltGr)
	case 0x5B: case 0x5C:              // VK_LWIN / VK_RWIN
		return true;
	}
	return false;
}

int WheelZoomStep(float wheelY, bool invert)
{
	// WM_MOUSEWHEEL was vertical only (a tilt wheel / touchpad sideways scroll
	// is WM_MOUSEHWHEEL, which the app never handled): an event without a
	// vertical part does not zoom.
	if (wheelY == 0.0f) return 0;
	return ((wheelY > 0.0f) != invert) ? 1 : -1;
}

int ScancodeFromVk(uint32_t vk)
{
	// The generic modifier codes (VK_SHIFT/VK_CONTROL/VK_MENU) come first in
	// the table as the left keys; the sided codes WM_KEYDOWN never carried are
	// accepted too, for --post-key.
	switch (vk) {
	case 0xA1: return (int)SDL_SCANCODE_RSHIFT;
	case 0xA3: return (int)SDL_SCANCODE_RCTRL;
	case 0xA5: return (int)SDL_SCANCODE_RALT;
	}
	vk = GenericModifierVk(vk);
	// The keypad: VK_NUMPADn / VK_DECIMAL are its keys with NumLock on.
	for (const KeyPair& k : kKeys)
		if (k.vk == vk) return (int)k.sc;
	return 0;
}

bool PostKeyParts(uint32_t vk, KeyParts& out)
{
	out = KeyParts{};
	out.scancode = ScancodeFromVk(vk);
	// NumLock as the key needs it: on for VK_NUMPADn / VK_DECIMAL, which only
	// exist in digit mode; the navigation keys use their own (extended) keys.
	if (IsNumpadDigit(out.scancode)) out.mod = SDL_KMOD_NUM;
#ifdef _WIN32
	// The raw code the keyboard would send for this key under the active
	// layout: the event loop maps it back through the same layout.
	if (IsNumpadDigit(out.scancode)) {
		// MapVirtualKey has no VK_NUMPADn -> scan code entry on every layout:
		// take the navigation key's non-extended code, which is the keypad's.
		static const uint16_t kPadRaw[] = { 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x53 };
		out.raw = kPadRaw[out.scancode - SDL_SCANCODE_KP_1];
	} else {
		out.raw = (uint16_t)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
	}
#endif
	if (!out.scancode && !out.raw) return false;
	return true;
}

}  // namespace platform
