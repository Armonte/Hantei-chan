// MainFrame glue for the Game Link window (game_link_panel.h): hand it the files the open characters came from
// (for auto-reload on save), the active view's pattern/frame (for follow mode) and a save callback (for
// "Push to game"). Everything else lives in the panel and its worker thread.
#include "../main_frame.h"
#include "../game_link_panel.h"

#include <filesystem>

namespace {
std::string TxtStem(const std::string& path)
{
	if (path.empty()) return {};
	return std::filesystem::path(path).stem().string();
}
} // namespace

void MainFrame::drawGameLink()
{
	if (!gamelink::showPanel) return;
	gamelink::EditorContext ctx;
	for (const auto& ch : characters) {
		if (!ch) continue;
		const std::string key = !ch->getTxtPath().empty() ? TxtStem(ch->getTxtPath()) : TxtStem(ch->getTopHA6Path());
		for (const auto& p : ch->getHA6Paths()) if (!p.empty()) ctx.files.push_back({ p, key });
		if (!ch->getPATPath().empty()) ctx.files.push_back({ ch->getPATPath(), key });
		if (!ch->getTxtPath().empty()) ctx.files.push_back({ ch->getTxtPath(), key });
		if (!ch->frameData.m_commandsPath.empty()) ctx.files.push_back({ ch->frameData.m_commandsPath, key });
	}
	CharacterView* view = getActiveView();
	CharacterInstance* active = getActiveCharacter();
	if (view && active) {
		ctx.activeState = &view->getState();
		ctx.activeKey = !active->getTxtPath().empty() ? TxtStem(active->getTxtPath()) : TxtStem(active->getTopHA6Path());
		ctx.patternCount = [active] { return (int)active->frameData.get_sequence_count(); };
		ctx.frameCount = [active](int p) {
			auto* seq = active->frameData.get_sequence(p);
			return seq ? (int)seq->frames.size() : 0;
		};
	}
	ctx.saveAll = [this] { return saveAllModifiedCharacters(); };
	gamelink::DrawPanel(ctx);
}
