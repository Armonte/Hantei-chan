// Standalone tests for the SDL -> WndProc input rules in src/platform/platform_keys.cpp
// (P0 fix 2): the key event -> virtual-key mapping (keyboard layout, NumLock keypad,
// generic modifiers, keys outside the fixed table), the chord modifiers taken from the
// event, the shortcut recorder's modifier-only filter, the wheel rule and the
// --post-key round trip. Exit code 0 = pass.
#include "platform/platform.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
#define CHECK_EQ(a, b) do { const unsigned long long va_ = (unsigned long long)(a), vb_ = (unsigned long long)(b); \
	if (va_ != vb_) { std::fprintf(stderr, "FAIL %s:%d: %s = 0x%llX, want 0x%llX\n", __FILE__, __LINE__, #a, va_, vb_); ++g_failures; } } while (0)

using platform::KeyParts;

static KeyParts K(SDL_Scancode sc, uint16_t raw, uint16_t mod = 0, uint32_t keycode = 0)
{
	KeyParts k;
	k.scancode = (int)sc;
	k.raw = raw;
	k.mod = mod;
	k.keycode = keycode;
	return k;
}

// A French AZERTY layout's scan code -> VK table for the keys it moves (what
// MapVirtualKey returns under KBDFR): A/Q, Z/W and M swap places, the OEM keys move.
static uint32_t Azerty(uint32_t raw)
{
	switch (raw) {
	case 0x10: return 'A';
	case 0x11: return 'Z';
	case 0x1E: return 'Q';
	case 0x2C: return 'W';
	case 0x27: return 'M';
	case 0x32: return 0xBC;   // VK_OEM_COMMA where US has M
	case 0x56: return 0xE2;   // VK_OEM_102
	case 0x2A: return 0xA0;   // VK_LSHIFT
	case 0xE038: return 0xA5; // VK_RMENU (AltGr)
	case 0xE046: return 0x03; // VK_CANCEL: what the layout says for SDL's Pause code
	}
	return 0;
}

// A Japanese (JIS) layout's extra keys.
static uint32_t Jis(uint32_t raw)
{
	switch (raw) {
	case 0x73: return 0xE2;   // Ro: VK_OEM_102
	case 0x7D: return 0xDC;   // Yen: VK_OEM_5
	case 0x79: return 0x1C;   // Henkan: VK_CONVERT
	case 0x7B: return 0x1D;   // Muhenkan: VK_NONCONVERT
	case 0x70: return 0xF2;   // Katakana/Hiragana: VK_DBE_HIRAGANA
	case 0x29: return 0xF3;   // Hankaku/Zenkaku: VK_DBE_SBCSCHAR
	}
	return 0;
}

static uint32_t Nothing(uint32_t) { return 0; }

int main()
{
	// --- (1) the VK follows the keyboard layout, through the raw scan code ---
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_Q, 0x10), Azerty), 'A');   // AZERTY: the key left of Z types A
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_A, 0x1E), Azerty), 'Q');
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_W, 0x11), Azerty), 'Z');
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_SEMICOLON, 0x27), Azerty), 'M');
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_M, 0x32), Azerty), 0xBC);
	// without a raw code (a pushed event, P5) the layout-aware keycode decides letters
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_Q, 0, 0, 'a'), Azerty), 'A');
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_Q, 0, 0, 'q'), nullptr), 'Q');
	// a raw code the layout does not map falls back to the position table
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_S, 0x1F), Azerty), 'S');
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_F5, 0x3F), Nothing), 0x74);

	// --- WM_KEYDOWN carried the generic modifier codes ---
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_LSHIFT, 0x2A), Azerty), 0x10);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_RALT, 0xE038), Azerty), 0x12);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_RCTRL, 0), nullptr), 0x11);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_LALT, 0), nullptr), 0x12);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_RGUI, 0), nullptr), 0x5C);   // the Windows keys stay sided

	// --- Pause / NumLock: scan codes SDL folds ---
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_PAUSE, 0xE046), Azerty), 0x13);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_NUMLOCKCLEAR, 0xE045), Azerty), 0x90);

	// --- (6) keys missing from the old table reach shortcuts ---
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL1, 0x73), Jis), 0xE2);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL3, 0x7D), Jis), 0xDC);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL4, 0x79), Jis), 0x1C);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL5, 0x7B), Jis), 0x1D);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL2, 0x70), Jis), 0xF2);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_GRAVE, 0x29), Jis), 0xF3);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_NONUSBACKSLASH, 0x56), Azerty), 0xE2);
	// and through the table when there is no raw code
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_NONUSBACKSLASH, 0), nullptr), 0xE2);   // VK_OEM_102
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_APPLICATION, 0), nullptr), 0x5D);      // VK_APPS
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_F13, 0), nullptr), 0x7C);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_F24, 0), nullptr), 0x87);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL1, 0), nullptr), 0xE2);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL3, 0), nullptr), 0xDC);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL4, 0), nullptr), 0x1C);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_INTERNATIONAL5, 0), nullptr), 0x1D);

	// --- (6) the keypad follows NumLock like the driver ---
	const uint16_t num = SDL_KMOD_NUM, shift = SDL_KMOD_LSHIFT;
	const struct { SDL_Scancode sc; uint16_t raw; uint32_t on, off; } pad[] = {
		{ SDL_SCANCODE_KP_7, 0x47, 0x67, 0x24 }, { SDL_SCANCODE_KP_8, 0x48, 0x68, 0x26 }, { SDL_SCANCODE_KP_9, 0x49, 0x69, 0x21 },
		{ SDL_SCANCODE_KP_4, 0x4B, 0x64, 0x25 }, { SDL_SCANCODE_KP_5, 0x4C, 0x65, 0x0C }, { SDL_SCANCODE_KP_6, 0x4D, 0x66, 0x27 },
		{ SDL_SCANCODE_KP_1, 0x4F, 0x61, 0x23 }, { SDL_SCANCODE_KP_2, 0x50, 0x62, 0x28 }, { SDL_SCANCODE_KP_3, 0x51, 0x63, 0x22 },
		{ SDL_SCANCODE_KP_0, 0x52, 0x60, 0x2D }, { SDL_SCANCODE_KP_PERIOD, 0x53, 0x6E, 0x2E },
	};
	for (const auto& p : pad) {
		CHECK_EQ(platform::VkFromKeyParts(K(p.sc, p.raw, num), Azerty), p.on);
		CHECK_EQ(platform::VkFromKeyParts(K(p.sc, p.raw, 0), Azerty), p.off);
		CHECK_EQ(platform::VkFromKeyParts(K(p.sc, p.raw, num | shift), Azerty), p.off);   // Shift + NumLock: navigation
		CHECK_EQ(platform::VkFromKeyParts(K(p.sc, 0, 0), nullptr), p.off);
	}
	CHECK(platform::NumpadIsDigitMode(num) && !platform::NumpadIsDigitMode(0) && !platform::NumpadIsDigitMode(num | shift));
	// the operators do not depend on NumLock
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_KP_MULTIPLY, 0), nullptr), 0x6A);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_KP_DIVIDE, 0), nullptr), 0x6F);
	CHECK_EQ(platform::VkFromKeyParts(K(SDL_SCANCODE_KP_ENTER, 0), nullptr), 0x0D);

	// --- (5) chord modifiers come from the event's mod state ---
	using platform::kModCtrl; using platform::kModShift; using platform::kModAlt;
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_S, SDL_KMOD_LCTRL), kModCtrl);
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_S, SDL_KMOD_RCTRL | SDL_KMOD_LSHIFT), kModCtrl | kModShift);
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_S, SDL_KMOD_RALT), kModAlt);
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_S, SDL_KMOD_GUI | SDL_KMOD_CAPS | SDL_KMOD_NUM), 0);
	// Shift + keypad digit with NumLock on: the driver released Shift around the key
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_KP_8, SDL_KMOD_NUM | SDL_KMOD_LSHIFT), 0);
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_KP_8, SDL_KMOD_LSHIFT), kModShift);   // NumLock off: Shift stays
	CHECK_EQ(platform::ChordModsFromKey(SDL_SCANCODE_KP_MULTIPLY, SDL_KMOD_NUM | SDL_KMOD_LSHIFT), kModShift);

	// --- (4) the shortcut recorder skips bare modifiers, Alt and AltGr included ---
	for (uint32_t vk : { 0x10u, 0x11u, 0x12u, 0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u, 0x5Bu, 0x5Cu })
		CHECK(platform::IsModifierOnlyVk(vk));
	for (uint32_t vk : { (uint32_t)'A', (uint32_t)'0', 0x1Bu, 0x14u, 0x90u, 0x79u, 0x6Au, 0x5Du, 0x0Du })
		CHECK(!platform::IsModifierOnlyVk(vk));
	// what the event loop hands the recorder for Alt / AltGr is one of them
	CHECK(platform::IsModifierOnlyVk(platform::VkFromKeyParts(K(SDL_SCANCODE_LALT, 0x38), nullptr)));
	CHECK(platform::IsModifierOnlyVk(platform::VkFromKeyParts(K(SDL_SCANCODE_RALT, 0xE038), Azerty)));

	// --- (3) the wheel: only a vertical step zooms ---
	CHECK_EQ(platform::WheelZoomStep(0.0f, false), 0);
	CHECK_EQ(platform::WheelZoomStep(0.0f, true), 0);
	CHECK_EQ(platform::WheelZoomStep(-0.0f, false), 0);
	CHECK_EQ(platform::WheelZoomStep(1.0f, false), 1);
	CHECK_EQ(platform::WheelZoomStep(1.0f, true), -1);
	CHECK_EQ(platform::WheelZoomStep(-1.0f, false), -1);
	CHECK_EQ(platform::WheelZoomStep(-1.0f, true), 1);
	CHECK_EQ(platform::WheelZoomStep(0.25f, false), 1);    // a precision touchpad's partial step
	CHECK_EQ(platform::WheelZoomStep(-0.1f, false), -1);

#ifdef _WIN32
	// --- the real layout (MapVirtualKeyW) on keys every layout maps the same way ---
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_UP, 0, 0, 0xE048), 0x26);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_HOME, 0, num, 0xE047), 0x24);   // the dedicated key ignores NumLock
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_8, 0, num, 0x48), 0x68);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_8, 0, 0, 0x48), 0x26);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_5, 0, 0, 0x4C), 0x0C);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_F1, 0, 0, 0x3B), 0x70);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_F13, 0, 0, 0x64), 0x7C);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_APPLICATION, 0, 0, 0xE05D), 0x5D);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_ESCAPE, 0, 0, 0x01), 0x1B);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_RETURN, 0, 0, 0x1C), 0x0D);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_ENTER, 0, 0, 0xE01C), 0x0D);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_DIVIDE, 0, 0, 0xE035), 0x6F);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_KP_MULTIPLY, 0, 0, 0x37), 0x6A);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_LSHIFT, 0, 0, 0x2A), 0x10);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_RSHIFT, 0, 0, 0x36), 0x10);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_RCTRL, 0, 0, 0xE01D), 0x11);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_RALT, 0, 0, 0xE038), 0x12);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_LGUI, 0, 0, 0xE05B), 0x5B);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_PAUSE, 0, 0, 0xE046), 0x13);
	CHECK_EQ(platform::VkFromKeyEvent(SDL_SCANCODE_NUMLOCKCLEAR, 0, 0, 0xE045), 0x90);
#endif

	// --- --post-key: the pushed event maps back to the same VK ---
	{
		std::vector<uint32_t> vks;
		for (uint32_t vk = 'A'; vk <= 'Z'; vk++) vks.push_back(vk);
		for (uint32_t vk = '0'; vk <= '9'; vk++) vks.push_back(vk);
		for (uint32_t vk = 0x60; vk <= 0x6F; vk++) if (vk != 0x6C) vks.push_back(vk);   // keypad (VK_SEPARATOR: ABNT only)
		for (uint32_t vk = 0x70; vk <= 0x87; vk++) vks.push_back(vk);                   // F1-F24
		for (uint32_t vk : { 0x08u, 0x09u, 0x0Cu, 0x0Du, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x1Bu, 0x20u, 0x21u, 0x22u, 0x23u,
		                     0x24u, 0x25u, 0x26u, 0x27u, 0x28u, 0x2Du, 0x2Eu, 0x5Bu, 0x5Cu, 0x5Du, 0x90u, 0x91u,
		                     0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u, 0xBAu, 0xBBu, 0xBCu, 0xBDu, 0xBEu, 0xBFu, 0xC0u,
		                     0xDBu, 0xDCu, 0xDDu, 0xDEu, 0xE2u })
			vks.push_back(vk);
		for (uint32_t vk : vks) {
			KeyParts kp;
			if (!platform::PostKeyParts(vk, kp)) { std::fprintf(stderr, "FAIL post-key 0x%X: no key\n", vk); ++g_failures; continue; }
			// keycode as startup_args sets it for a letter; the rest go by raw code / scancode
			if (vk >= 'A' && vk <= 'Z') kp.keycode = vk - 'A' + 'a';
			const uint32_t back = platform::VkFromKeyEvent(kp.scancode, kp.keycode, kp.mod, kp.raw);
			uint32_t want = vk;
			if (vk == 0xA0 || vk == 0xA1) want = 0x10;
			if (vk == 0xA2 || vk == 0xA3) want = 0x11;
			if (vk == 0xA4 || vk == 0xA5) want = 0x12;
			if (back != want) {
				std::fprintf(stderr, "FAIL post-key 0x%X: scancode %d raw 0x%X mod 0x%X -> 0x%X\n", vk, kp.scancode, kp.raw, kp.mod, back);
				++g_failures;
			}
		}
		KeyParts none;
		CHECK(!platform::PostKeyParts(0xFF, none));   // no key: --post-key keeps warning
	}

	if (g_failures) { std::fprintf(stderr, "%d failure(s)\n", g_failures); return 1; }
	std::printf("platform_input: all checks passed\n");
	return 0;
}
