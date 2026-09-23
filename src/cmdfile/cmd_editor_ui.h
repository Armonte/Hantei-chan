#ifndef CMDFILE_CMD_EDITOR_UI_H_GUARD
#define CMDFILE_CMD_EDITOR_UI_H_GUARD

// ImGui front end for the command-file editor (ported from Gonptechan EX's
// mbaacc_command_browser, restructured): one window per open file, each with its own
// Workspace (document, notes, undo history, staged save).

#include "cmd_document.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cmdfile {

// Called when a file's staged document changes (edit, undo, revert, save), so viewers
// that show command names (IF 11/35 panels) can refresh.
using DocumentChanged = std::function<void(const std::string& path, const Document& document)>;

class CommandFileEditor {
public:
	CommandFileEditor();
	~CommandFileEditor();

	// Opens (or focuses) a file. Returns false and fills *error when it cannot be read.
	bool open(const std::string& path, std::string* error);
	void draw(const DocumentChanged& onChanged);

	bool hasUnsavedChanges() const;
	bool empty() const;

private:
	struct View;
	std::vector<std::unique_ptr<View>> m_views;
	int m_nextId = 1;
};

} // namespace cmdfile

#endif
