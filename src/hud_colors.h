#ifndef HUD_COLORS_H_GUARD
#define HUD_COLORS_H_GUARD

// HUD meter / guard colours of MBAA.exe (issue #56). Each colour is a 32-bit
// ARGB immediate in the gauge drawing code (mov [esp+14h], imm32 for the
// meter, mov reg, imm32 for the guard bar); the table's RVAs come from the
// issue report and are re-located on the exe by matching the instruction, so
// an off-by-a-few address still lands on the immediate.
#include <cstdint>
#include <string>
#include <vector>

namespace hud {

struct ColorSlot {
	const char* key;        // hud_theme.json key ("meter.lower", "guard.quality_high", ...)
	const char* label;
	uint32_t defaultArgb;
	int overlaySpeed;       // meter shine direction/speed (0 for guard colours)
	uint32_t rvaHint;       // MBAA.exe RVA reported for the immediate
};

const std::vector<ColorSlot>& Slots();

struct ExeColor {
	bool found = false;
	uint32_t fileOffset = 0;
	uint32_t argb = 0;
};

// Locate every slot's immediate in an MBAA.exe image. Returns false if the
// file cannot be read or is not a PE image.
bool ReadExeColors(const std::string& exePath, std::vector<ExeColor>& out, std::string* error);

// Copy `srcExe` to `dstExe` with the given colours written into the located
// immediates (slots not found are left alone). dst must differ from src.
bool WritePatchedExe(const std::string& srcExe, const std::string& dstExe,
                     const std::vector<uint32_t>& argb, std::string* error);

// hud_theme.json (the CCCaster HUD plugin profile, see hud_theme_exporter).
bool LoadThemeColors(const std::string& jsonPath, std::vector<uint32_t>& argb, std::string* error);
bool SaveThemeColors(const std::string& jsonPath, const std::vector<uint32_t>& argb, std::string* error);

std::string ToHex(uint32_t argb);        // "#aarrggbb"
bool FromHex(const std::string& s, uint32_t* argb);

} // namespace hud

#endif /* HUD_COLORS_H_GUARD */
