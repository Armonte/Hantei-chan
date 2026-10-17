// Stage browser: the game's own stage list (BgList.ini / the MBAC table) with
// names, previews and BGM, one-click / hotkey switching between stages, and
// the metadata editor (BgList.ini entries, bgm.txt). See bg_project.h.
#ifndef BG_BROWSER_H_GUARD
#define BG_BROWSER_H_GUARD

#include "bg_project.h"
#include <functional>
#include <string>

namespace bg {

struct BrowserHooks {
	// Open the stage (the host replaces the active stage tab or opens one).
	std::function<void(const std::string& datPath)> open;
	// "Show in game" (live Game Link stage switch). Null until the link
	// supports it; the button is disabled then.
	std::function<bool(int stageId, const std::string& datPath)> showInGame;
	// [authoring] Set only while the Authoring workspace's Setup asked for a stage ("Browse..."): the button
	// "Use for the Authoring setup" hands the selected id back (docs/HANTEI_AUTHORING_MODE.md §5.3).
	std::function<void(int stageId)> pickForSetup;
};

// Draws the "Stage Browser" window contents. `currentDat` = the active stage
// tab's file ("" if none).
void DrawStageBrowser(StageProject& project, const std::string& currentDat, const BrowserHooks& hooks);

// Release GL preview textures (call before the GL context goes away / on reopen).
void ClearStageBrowserCache();

} // namespace bg

#endif
