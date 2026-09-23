#ifndef GAME_LINK_PANEL_H_GUARD
#define GAME_LINK_PANEL_H_GUARD
// The "Game Link" window: connect Hantei-chan to a running MBAA.exe (PovertyCaster pchost.dll with
// PCHOST_MBAACC_LINK=1), reload characters on save, follow what a game slot is playing, pick characters.
// Client: game_link.h. Docs: docs/HANTEI_GAME_LINK.md.
#include "game_link.h"

#include <functional>
#include <string>
#include <vector>

struct FrameState;

namespace gamelink {

// What the editor tells the panel each frame. Filled by MainFrame::drawGameLink (ui/main_frame_gamelink.cpp).
struct EditorContext {
	std::vector<WatchedFile> files;          // every file the open characters were loaded from
	FrameState* activeState = nullptr;       // the active view's pattern/frame (follow mode writes it)
	std::string activeKey;                   // the active character's .txt stem, e.g. "akiha_0"
	std::function<int()> patternCount;       // of the active character
	std::function<int(int)> frameCount;      // frames in a pattern of the active character
	std::function<bool()> saveAll;           // "Push to game" saves modified characters first
};

extern bool showPanel;
Client& SharedClient();
void DrawPanel(EditorContext& ctx);
// Open the window, connect, and follow + jump to game slot `slot` (0-3). Used by --game-link.
void StartFollowing(int slot);

} // namespace gamelink

#endif
