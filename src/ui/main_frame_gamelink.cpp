// MainFrame glue for the Game Link window (game_link_panel.h): hand it the files the open characters came from
// (for auto-reload on save), the active view's pattern/frame (for follow mode) and a save callback (for
// "Push to game"), and the open stage's files (for the stage auto-reload, docs/HANTEI_STAGE_LINK.md). Everything else
// lives in the panel and its worker thread.
#include "../main_frame.h"
#include "../game_link_panel.h"
#include "../background/bg_file.h"
#include "../character_view.h"

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
	// [stage-link] the open stage's files, for "auto-reload stage on save" (the game reloads only when it is
	// showing this stage — gamelink::StageFileMatches) and the "Show open stage" button.
	// Every open stage tab is watched (the game reloads only the one it shows); the active tab's stage is the one
	// "Show open stage" switches to.
	using K = gamelink::StageFileKind;
	auto addStage = [&ctx](const bg::File& f) {
		auto add = [&ctx](const std::string& p, K k) {
			if (p.empty()) return;
			for (const auto& w : ctx.stageFiles) if (w.path == p) return;   // BgList.ini is shared by every stage
			ctx.stageFiles.push_back({ p, k });
		};
		add(f.GetFilename(), K::Dat);
		add(f.GetStageInfo().path, K::Info);
		add(f.GetLightFile().path, K::Light);
		add(f.GetStageList().path, K::List);
	};
	for (const auto& v : views)
		if (v && v->isStageView() && v->getStageFile() && v->getStageFile()->IsLoaded()) addStage(*v->getStageFile());
	if (currentBgFile && currentBgFile->IsLoaded())
		if (const bg::StageListEntry* e = currentBgFile->GetStageListEntry()) ctx.openStageIndex = e->index;
	gamelink::DrawPanel(ctx);
}
