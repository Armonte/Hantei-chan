#ifndef SQUIRREL_SCRIPT_PANEL_H_GUARD
#define SQUIRREL_SCRIPT_PANEL_H_GUARD

// Dockable "Script" panel: browse a French-Bread game's Squirrel scripts (script/ and the per-character
// _mv/_sc/_cmd/_com/_se_list files under data/), edit them with ImGuiColorTextEdit, save byte-exact
// CP932 (ScriptDocument), and compile on save with the game's VM + API profile (ScriptVM, sandboxed).
// Phase P3 of docs/squirrel/SCOPE.md.

#include <functional>
#include <memory>
#include <string>

namespace sqedit {

class ScriptPanel {
public:
	ScriptPanel();
	~ScriptPanel();

	bool visible() const { return m_visible; }
	void setVisible(bool v) { m_visible = v; }

	// Opens a game root (the folder holding script/ and data/). Profile is "uni2", "mbtl" or "" (auto).
	bool openGameRoot(const std::string& rootUtf8, const std::string& profile, std::string* error);
	bool openFile(const std::string& pathUtf8, std::string* error);

	void draw();

	// Shortcut routing: true while the panel (or its editor) had focus in the last draw.
	bool focused() const { return m_focused; }
	void requestSave() { m_saveRequested = true; }
	bool hasUnsavedChanges() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
	bool m_visible = false;
	bool m_focused = false;
	bool m_saveRequested = false;
};

// Finds docs/squirrel/api/<game>_api.json (or res/squirrel/<game>_api.json) near the working
// directory or the executable. Empty when not found.
std::string FindApiProfile(const std::string& game);
// "uni2" / "mbtl" from the folder name or exe present in the root; "" when unknown.
std::string GuessGame(const std::string& rootUtf8);

} // namespace sqedit

#endif
