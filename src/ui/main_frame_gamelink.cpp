// MainFrame glue for the Game Link window (game_link_panel.h): hand it the files the open characters came from
// (for auto-reload on save), the active view's pattern/frame (for follow mode) and a save callback (for
// "Push to game"), and the open stage's files (for the stage auto-reload, docs/HANTEI_STAGE_LINK.md). Everything else
// lives in the panel and its worker thread.
#include "../main_frame.h"
#include "../game_link_panel.h"
#include "../tag_panel.h"
#include "../authoring/authoring_window.h"
#include "../authoring/game_view.h"
#include "../render_target.h"
#include <glad/glad.h>
#include "../box_pane.h"
#include "../game_link.h"
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

// The open characters' files (auto-reload on save).
static std::vector<gamelink::WatchedFile> CollectLinkFiles(const std::vector<std::unique_ptr<CharacterInstance>>& characters)
{
	std::vector<gamelink::WatchedFile> files;
	for (const auto& ch : characters) {
		if (!ch) continue;
		const std::string key = !ch->getTxtPath().empty() ? TxtStem(ch->getTxtPath()) : TxtStem(ch->getTopHA6Path());
		for (const auto& p : ch->getHA6Paths()) if (!p.empty()) files.push_back({ p, key });
		if (!ch->getPATPath().empty()) files.push_back({ ch->getPATPath(), key });
		if (!ch->getTxtPath().empty()) files.push_back({ ch->getTxtPath(), key });
		if (!ch->frameData.m_commandsPath.empty()) files.push_back({ ch->frameData.m_commandsPath, key });
	}
	return files;
}

void MainFrame::drawGameLink()
{
	if (!gamelink::showPanel) return;
	gamelink::EditorContext ctx;
	ctx.files = CollectLinkFiles(characters);
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

// [tag-panel] The Tag / Team window: the active view (jump / follow), the active character's .txt, _c.txt and pattern
// names (pickers). docs/HANTEI_TAG_PANEL.md.
void MainFrame::drawTagPanel()
{
	if (!tagpanel::showPanel) { tagpanel::EditorContext none; tagpanel::DrawPanel(none); return; }
	tagpanel::EditorContext ctx;
	CharacterView* view = getActiveView();
	CharacterInstance* active = getActiveCharacter();
	if (view && active) {
		ctx.activeState = &view->getState();
		ctx.activeTxtPath = active->getTxtPath();
		ctx.activeKey = !active->getTxtPath().empty() ? TxtStem(active->getTxtPath()) : TxtStem(active->getTopHA6Path());
		ctx.activeCommandsPath = active->frameData.m_commandsPath;
		ctx.patternCount = [active] { return (int)active->frameData.get_sequence_count(); };
		ctx.patternName = [active](int p) {
			auto* seq = active->frameData.get_sequence(p);
			return seq ? std::string(seq->name.c_str()) : std::string();
		};
		ctx.frameCount = [active](int p) {
			auto* seq = active->frameData.get_sequence(p);
			return seq ? (int)seq->frames.size() : 0;
		};
	}
	tagpanel::DrawPanel(ctx);
}

// [authoring] The Authoring workspace (docs/HANTEI_AUTHORING_MODE.md §5 / §8.9): it opens the picked characters as tabs,
// reads their loaded HA6 (read-only) for the pattern pickers and the inline frame data, and drives the tabs for
// Follow point. While it is open and the Game Link window is not, the open characters' files are watched here so a save
// still hot-reloads them in the game.
namespace {
std::string LowerPathKey(std::string s)
{
	for (char& c : s) { if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if (c == '/') c = '\\'; }
	return s;
}
} // namespace

// [game-view] Hantei-chan's open stage at a game camera (the game-exact mapping bg_render uses: world -> screen =
// (world - cam) * zoom + (320, 432) at 640x480), into the Game panel's back / front target. A dedicated renderer, so the
// stage tab's own renderer state is untouched. Returns 0 when no stage tab is open.
unsigned MainFrame::renderGameViewStage(float camX, float camY, float zoom, int w, int h, int pass, float heat)
{
	bg::File* file = nullptr;
	for (const auto& v : views)
		if (v && v->isStageView() && v->getStageFile() && v->getStageFile()->IsLoaded()) { file = v->getStageFile(); break; }
	if (currentBgFile && currentBgFile->IsLoaded()) file = currentBgFile;   // the active stage tab wins
	if (!file || w <= 0 || h <= 0) return 0;
	if (!m_gvStage) {
		m_gvStage = std::make_unique<bg::Renderer>();
		m_gvStage->SetHostRender(&render);
		m_gvBack = std::make_unique<RenderTarget>();
		m_gvFront = std::make_unique<RenderTarget>();
	}
	if (m_gvStage->GetFile() != file) m_gvStage->SetFile(file);
	m_gvStage->SetEnabled(true);
	m_gvStage->SetShowDebugOverlay(false);
	const uint64_t frame = (uint64_t)ImGui::GetFrameCount();
	if (frame != m_gvStageFrame) { m_gvStageFrame = frame; m_gvStage->Update(); }   // one animation tick per UI frame
	m_gvStage->SetHeatPreview(pass == 2 ? 0.0f : heat, (float)ImGui::GetTime());
	RenderTarget& rt = pass == 2 ? *m_gvFront : *m_gvBack;
	if (!rt.ensure(w, h, true)) return 0;
	{
		ScopedTargetBinding bind(rt);
		glClearColor(0, 0, 0, pass == 2 ? 0.0f : 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		bg::Camera cam;
		cam.zoom = zoom > 0 ? zoom : 1.0f;
		cam.SetPan(w * 0.5f / cam.zoom - camX, h * 0.9f / cam.zoom - camY);
		cam.SetGameCam(camX, camY);
		m_gvStage->Render(cam, w, h, pass == 2 ? bg::Pass::Front : pass == 1 ? bg::Pass::Back : bg::Pass::All);
	}
	return rt.texture();
}

void MainFrame::drawAuthoring()
{
	if (!authoring::showWindow && !authoring::showGameView) {
		authoring::HostContext none;
		authoring::Draw(none);
		authoring::DrawGameView(none);
		return;
	}
	static bool registered = false;
	if (!registered) {
		registered = true;
		shortcuts.setContextHandler(ShortcutContext::authoring, [](ShortcutAction a) {
			if (a == ShortcutAction::undo) return authoring::UndoRedo(false);
			if (a == ShortcutAction::redo) return authoring::UndoRedo(true);
			return false;
		});
	}
	auto findChar = [this](const std::string& txt) -> CharacterInstance* {
		const std::string k = LowerPathKey(txt);
		for (const auto& ch : characters)
			if (ch && !ch->getTxtPath().empty() && LowerPathKey(ch->getTxtPath()) == k) return ch.get();
		return nullptr;
	};
	auto viewIndexOf = [this](CharacterInstance* ch) {
		for (size_t i = 0; i < views.size(); ++i) if (views[i] && views[i]->getCharacter() == ch) return (int)i;
		return -1;
	};
	authoring::HostContext host;
	host.openCharacter = [this, findChar, viewIndexOf](const std::string& txt, bool focus) {
		if (CharacterInstance* ch = findChar(txt)) {
			if (focus) { const int vi = viewIndexOf(ch); if (vi >= 0) setActiveView(vi); }
			return true;
		}
		auto character = std::make_unique<CharacterInstance>();
		if (!character->loadFromTxt(txt)) return false;
		characters.push_back(std::move(character));
		const int before = activeViewIndex;
		createViewForCharacter(characters.back().get());
		if (!focus && before >= 0) setActiveView(before);
		return true;
	};
	host.frameDataFor = [findChar](const std::string& txt) -> const FrameData* {
		CharacterInstance* ch = findChar(txt);
		return ch ? &ch->frameData : nullptr;
	};
	host.showPattern = [this, findChar, viewIndexOf](const std::string& txt, int pattern, int frame, bool focusTab) {
		CharacterInstance* ch = findChar(txt);
		if (!ch) return;
		const int vi = viewIndexOf(ch);
		if (vi < 0) return;
		if (focusTab && vi != activeViewIndex) setActiveView(vi);
		if (pattern < 0 || pattern >= (int)ch->frameData.get_sequence_count()) return;
		FrameState& st = views[vi]->getState();
		st.animating = false;
		st.pattern = pattern;
		auto* seq = ch->frameData.get_sequence(pattern);
		const int frames = seq ? (int)seq->frames.size() : 0;
		st.frame = frames > 0 ? (frame >= 0 && frame < frames ? frame : 0) : 0;
		st.currentTick = 0;
	};
	host.browseStage = [this] { m_showStageBrowser = true; };
	if (CharacterInstance* active = getActiveCharacter()) host.activeTxt = active->getTxtPath();
	if (!gamelink::showPanel) {
		gamelink::Client& c = gamelink::SharedClient();
		c.SetAutoReload(true);
		c.SetWatchedFiles(CollectLinkFiles(characters));
	}
	host.renderStage = [this](float cx, float cy, float z, int w, int h, int pass, float heat) {
		return renderGameViewStage(cx, cy, z, w, h, pass, heat);
	};
	host.selectedBoxFor = [this, findChar](const std::string& txt) -> int {
		CharacterInstance* ch = findChar(txt);
		CharacterView* v = getActiveView();
		if (!ch || !v || v->getCharacter() != ch || !v->getBoxPane()) return -1;
		return v->getBoxPane()->SelectedBoxId();
	};
	authoring::Draw(host);
	authoring::DrawGameView(host);
	// the Authoring window (tuning undo) or the Game panel (input forwarding) keeps the editor's shortcuts away
	if (authoring::HasFocus() || authoring::GameViewCapturesInput()) shortcuts.claimFocus(ShortcutContext::authoring, 0);
}
