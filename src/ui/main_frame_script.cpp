// MainFrame glue for the Squirrel Script panel (squirrel/script_panel.h).

#include "../main_frame.h"

void MainFrame::drawScriptPanel()
{
	if (!scriptPanel) return;
	if (!m_scriptShortcutsRegistered) {
		m_scriptShortcutsRegistered = true;
		// While the Script panel has focus, Ctrl+S saves the script; Ctrl+Z / Ctrl+Y belong to the text
		// widget's own history (it reads them from ImGui), so they must not reach the HA6 undo stack.
		shortcuts.setContextHandler(ShortcutContext::script, [this](ShortcutAction action) {
			switch (action) {
			case ShortcutAction::save: if (scriptPanel) scriptPanel->requestSave(); return true;
			case ShortcutAction::undo:
			case ShortcutAction::redo: return true;
			default: return false;
			}
		});
	}
	scriptPanel->draw();
	if (scriptPanel->focused()) shortcuts.claimFocus(ShortcutContext::script, 1);
}
