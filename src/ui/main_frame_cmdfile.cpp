// MainFrame glue for the _c.txt command-file editor (cmdfile/).

#include "../main_frame.h"
#include "../cmdfile/cmd_framedata.h"
#include "../cmdfile/cmd_io.h"
#include "../misc.h"

#include <algorithm>
#include <cctype>

namespace {
std::string LowerPath(std::string s)
{
	s = normalizePath(s);
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}
} // namespace

void MainFrame::openCommandEditor(const std::string& path)
{
	std::string error;
	if (!commandEditor.open(path, &error))
		requestErrorPopup("Loading Error", error.empty() ? path : error);
}

void MainFrame::openCommandEditorForActive()
{
	CharacterInstance* active = getActiveCharacter();
	if (!active) return;
	// Prefer the file the character's command table already came from.
	if (!active->frameData.m_commandsPath.empty()) { openCommandEditor(active->frameData.m_commandsPath); return; }
	const auto candidates = cmdfile::CommandFileCandidates(active->getTxtPath());
	if (candidates.empty()) {
		requestErrorPopup("Loading Error", "No _c.txt found next to " + active->getTxtPath());
		return;
	}
	loadCommandsForActive(candidates.front());
	openCommandEditor(candidates.front());
}

void MainFrame::loadCommandsForActive(const std::string& path)
{
	CharacterInstance* active = getActiveCharacter();
	if (!active) return;
	if (!active->frameData.load_commands(path.c_str()))
		requestErrorPopup("Loading Error", path);
}

void MainFrame::drawCommandEditor()
{
	commandEditor.draw([this](const std::string& path, const cmdfile::Document& doc) {
		// Keep command names shown in IF panels in step with the staged edits.
		const std::string key = LowerPath(path);
		for (auto& character : characters) {
			if (character && !character->frameData.m_commandsPath.empty() &&
				LowerPath(character->frameData.m_commandsPath) == key)
				cmdfile::ApplyCommandTable(character->frameData, doc, character->frameData.m_commandsPath);
		}
	});
}
