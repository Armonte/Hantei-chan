#ifndef AUTHORING_WINDOW_H_GUARD
#define AUTHORING_WINDOW_H_GUARD
// [authoring] The Authoring workspace (docs/HANTEI_AUTHORING_MODE.md §5 as changed by §8.9): one window, behind the
// "Experimental: Authoring" menu, that
//   * picks the match (mode, scene, style, KO rule, timer, assists, stage, 2-4 characters with moon and palette),
//     launches MBAA.exe with PovertyCaster or attaches to a running one, and loads the setup in the game;
//   * opens every picked character as an editor tab (point highlighted, Follow point);
//   * edits the TAG tuning sidecars (global + per character + per moon, local overlay over the shipped defaults) with
//     instant apply, one undo history, checkpoints, A/B snapshots and Promote to defaults;
//   * shows the frame data of the moves the tuning points at, and the MBAC reference values;
//   * edits the TAG HUD layout by dragging in a preview;
//   * shows what the game resolved (Live), a filtered game log, and offers a relaunch after a crash.
// The window talks to the game through gamelink::SharedClient() (one pipe per editor).
#include "authoring_model.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class FrameData;
struct FrameState;

namespace authoring {

// What the editor (MainFrame) provides each frame.
struct HostContext {
	// Open (or focus) a character by its .txt. False when it cannot be loaded.
	std::function<bool(const std::string& txtPath, bool focus)> openCharacter;
	// The FrameData of an OPEN tab loaded from this .txt (nullptr = not open). Read-only use.
	std::function<const FrameData*(const std::string& txtPath)> frameDataFor;
	// Focus the tab of this .txt (if open) and show pattern / frame.
	std::function<void(const std::string& txtPath, int pattern, int frame, bool focusTab)> showPattern;
	// Open the Stage Browser in "pick for the Authoring setup" mode.
	std::function<void()> browseStage;
	std::string activeTxt;       // the active tab's .txt ("" = none)
};

extern bool showWindow;
void Draw(HostContext& host);

// Tabs: "Setup", "Setups", "Tuning", "HUD", "Live", "Log".
void Open(const std::string& tab);
// Startup options (--tool authoring ...): gameDir / tagRoot override the settings for this run; setupHex replaces the
// setup being edited; charFile + layer ("all", "c", "f", "h") choose the Tuning character tab; link connects.
struct StartupOptions {
	std::string tab, setupHex, gameDir, tagRoot, charFile, layer, subTab;
	bool link = false;
	bool loadInGame = false;     // press Load in game once the link is up
	bool sessionUnlock = false;
	std::string abDemo;          // "1": take A, change a value, take B (a capture of the A/B strip)
	uint32_t pid = 0;            // --authoring-pid: link ONLY to this pid (a mock / one game among several)
};
void ApplyStartup(const StartupOptions& o);
void StartupLoadInGame();
// The stage browser's pick hook: set while the Setup tab asked for a stage.
bool WantsStagePick();
void StagePicked(int stageId);
// Undo routing (MainFrame): true while the Authoring window (or a child) has focus.
bool HasFocus();
bool UndoRedo(bool redo);
std::string UndoLabel(bool redo);
// The badge a character tab shows ("P1 point", "P2 partner", "") and whether it is a team's point right now.
std::string TabBadge(const std::string& txtPath, bool* isPoint);
// Save the settings + setup library (called on exit too).
void SaveSettings();

} // namespace authoring

#endif
