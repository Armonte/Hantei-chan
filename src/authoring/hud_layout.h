#ifndef AUTHORING_HUD_LAYOUT_H_GUARD
#define AUTHORING_HUD_LAYOUT_H_GUARD
// [authoring] The TAG HUD layout sidecar (povertycaster\tag\hud.ini, then local\hud.ini on top, key by key;
// docs/HANTEI_AUTHORING_MODE.md §8.6 / §9.4) and its editor: drag the elements in a 640x480 preview drawn with the
// game's own HUD art (<gamedir>\GRP\Gauge_AA\...), the edit goes to local\hud.ini, and the game follows.
//
// MIRRORS PovertyCaster pc-adapters/mbaacc/include/mbaacc/TagHud.hpp TagHudSettings / TagHudLayout (defaults and
// field names). Side-0 coordinates; side 1 is the mirror image (x' = 640 - x - w).
#include <string>
#include <vector>

namespace tagtune { class TagIni; }

namespace authoring {

class EditSink;

struct HudRect { float x = 0, y = 0, w = 0, h = 0; };

struct TagHudValues {
	bool enabled = true, nativeRedirect = true, reserveFace = true, partnerBar = true, teamName = true, banners = true,
	     assist = true, swapCooldown = true;
	float tweenMs = 260.0f;
	HudRect mainPortrait{ 0, 0, 256, 96 };
	HudRect mainSrc{ 0, 0, 256, 96 };
	HudRect reservePortrait{ 72, 0, 66, 37 };
	HudRect reserveSrc{ 0, 0, 96, 54 };
	HudRect partnerThumb{ 112, 119, 21, 12 };
	HudRect partnerBarRect{ 134, 122, 139, 8 };
	HudRect swapSliver{ 134, 131, 139, 2 };
	float assistLabelX = 134, assistLabelY = 136;
	HudRect assistPip{ 158, 137, 48, 6 };
	float nameX = 134, nameY = 148, namePx = 1;
	HudRect banner{ 134, 147, 176, 11 };
};

// Every key of §9.4, in file order: key, kind ("bool", "float", "rect", "xy", "xyp").
struct HudKey { const char* key; const char* kind; };
const std::vector<HudKey>& HudKeys();

// shipped then local ([taghud] in each). Bad values are warnings and keep the default.
TagHudValues ResolveHud(const tagtune::TagIni* shipped, const tagtune::TagIni* local, std::vector<std::string>* warnings = nullptr);
// The text of one key in the values ("134,122,139,8", "1", "260").
std::string HudValueText(const TagHudValues& v, const std::string& key);
// Set a key in a hud document ([taghud] section created when missing); byte-preserving like every sidecar edit.
void SetHudKey(tagtune::TagIni& doc, const std::string& key, const std::string& value);
bool ClearHudKey(tagtune::TagIni& doc, const std::string& key);

// The editor. `gameDir` supplies the art; the sink supplies the workspace (hud docs) and the undo / save / apply.
struct HudEditorState;
void DrawHudLayoutEditor(EditSink& sink, const std::string& gameDir, const int previewChara[4]);

} // namespace authoring

#endif
