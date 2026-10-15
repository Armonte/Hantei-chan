#ifndef SQUIRREL_SCRIPT_PANEL_H_GUARD
#define SQUIRREL_SCRIPT_PANEL_H_GUARD

// Dockable "Script" panel: browse a French-Bread game's Squirrel scripts (script/ and the per-character
// _mv/_sc/_cmd/_com/_se_list files under data/), edit them with ImGuiColorTextEdit, save byte-exact
// CP932 (ScriptDocument), and compile on save with the game's VM + API profile (ScriptVM, sandboxed).
// Phase P3 of docs/squirrel/SCOPE.md.

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

	// A pattern reference found under the mouse in a script (P4): a pattern number (pat=N, start_pat=N,
	// SetPattern(N), def_PAT_* constants), a PTCN code name (pat="StdC_End", SetPattern("...")), or a frame
	// id (JumpFrameID(N), case N under GetUpdateFrameID) that matches AFID.
	struct PatRef {
		int pattern = -1;
		std::string code;
		int afid = -1;
	};
	// Set by the main frame. scriptDir is the folder of the open script (data/chrNNN for character files).
	// describe returns the hover text ("" = unknown); open jumps to it in the HA6 editor (Ctrl+click).
	std::function<std::string(const std::string& scriptDirUtf8, const PatRef&)> describeRef;
	std::function<void(const std::string& scriptDirUtf8, const PatRef&)> openRef;
	// Problem text for a reference ("" = fine or cannot tell): missing pattern / code name / frame id, a pattern with
	// no frames, one whose last frame is "Next" with nothing after it, or a frame jumping past the pattern's end.
	std::function<std::string(const std::string& scriptDirUtf8, const PatRef&)> checkRef;
	// Pattern names / code names of the character a script folder belongs to that contain `query`: (label, ref).
	std::function<std::vector<std::pair<std::string, PatRef>>(const std::string& scriptDirUtf8, const std::string& query)> searchHa6;

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
// "uni2" / "mbtl" / "dfci" / "unib" from the folder name or exe present in the root; "" when unknown.
std::string GuessGame(const std::string& rootUtf8);

} // namespace sqedit

#endif
