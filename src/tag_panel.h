#ifndef TAG_PANEL_H_GUARD
#define TAG_PANEL_H_GUARD
// EXPERIMENTAL "Tag / Team" window (Windows > Tag / Team): edit the running MBAACC's tag_tuning.ini (PovertyCaster
// native TAG levers, styles, per-character entries and the directional assist slots) while you play, and watch the
// live tag state over the Game Link. Docs: docs/HANTEI_TAG_PANEL.md. Model: src/tag_tuning/.
//
// The game re-reads tag_tuning.ini within a second of its mtime changing (offline only), so "Apply to game" is an
// atomic save next to MBA.exe. The panel never writes while the link reports a session (netplay / rollback /
// replay / synctest / recording), and refuses a save that would add an ini warning.
#include <functional>
#include <string>
#include <vector>

struct FrameState;

namespace tagpanel {

struct EditorContext {
	FrameState* activeState = nullptr;            // the active view (jump / follow write pattern + frame)
	std::string activeTxtPath;                    // the active character's .txt ("" = none / not from a .txt)
	std::string activeKey;                        // its stem, e.g. "sion_0"
	std::string activeCommandsPath;               // the _c.txt it loaded ("" = none)
	std::function<int()> patternCount;
	std::function<std::string(int)> patternName; // UTF-8
	std::function<int(int)> frameCount;
};

extern bool showPanel;
void DrawPanel(EditorContext& ctx);

// Startup (--tool tag, --tag-ini, --tag-char, --tag-tab): open the window on this ini / character / tab.
void OpenStartup(const std::string& iniPath, const std::string& charFile, const std::string& tab);
// --tag-apply: press "Apply to game" once (scripted check of the session gate / validation).
void StartupApply();

} // namespace tagpanel

#endif
