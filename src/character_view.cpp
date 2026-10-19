#include "character_view.h"
#include "ini.h"
#include "render.h"

namespace {
uint64_t g_nextViewId = 1;
}

void CharacterView::setId(uint64_t id)
{
	if (id == 0) return;
	m_id = id;
	if (id >= g_nextViewId) g_nextViewId = id + 1;
}

CharacterView::CharacterView(CharacterInstance* character, Render* render)
	: m_character(character), m_viewNumber(0)
{
	m_id = g_nextViewId++;
	// A new view starts from the character's last camera (projects store it
	// per character for older files).
	if (character) {
		m_camera.panX = (float)character->renderX;
		m_camera.panY = (float)character->renderY;
	}

	// Initialize independent state for this view
	m_state.pattern = 0;
	m_state.frame = 0;
	m_state.spriteId = -1;

	// Create panes for this view
	refreshPanes(render);
}

CharacterView::~CharacterView()
{
}

std::string CharacterView::getDisplayName() const
{
	if (isStageView()) {
		return std::string("Stage: ") + m_stageDisplayName;
	}
	if (!m_character) {
		return "Unknown";
	}

	std::string name = m_character->getDisplayName();

	// Add view number suffix for multi-views (e.g., "Ciel #2")
	if (m_viewNumber > 0) {
		name += " #" + std::to_string(m_viewNumber + 1);
	}

	// Always add pattern number (e.g., "Ciel #2 - 400")
	name += " - " + std::to_string(m_state.pattern);

	// ...and the start of the pattern's name, so tabs on the same character
	// can be told apart (issue #4): at most 12 characters, cut on a UTF-8
	// boundary.
	if (const Sequence* seq = m_character->frameData.get_sequence(m_state.pattern)) {
		const std::string& pat = seq->name.empty() ? seq->codeName : seq->name;
		constexpr int maxChars = 12;
		size_t end = 0;
		int chars = 0;
		while (end < pat.size() && chars < maxChars) {
			const unsigned char c = (unsigned char)pat[end];
			const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
			if (end + len > pat.size()) break;
			end += len;
			++chars;
		}
		std::string shortName = pat.substr(0, end);
		while (!shortName.empty() && shortName.back() == ' ') shortName.pop_back();
		// "##" in a label would hide the rest of it (ImGui ID syntax).
		for (auto& ch : shortName) if (ch == '#') ch = '-';
		if (!shortName.empty()) {
			name += " " + shortName;
			if (end < pat.size()) name += "...";
		}
	}

	return name;
}

void CharacterView::setStageFile(std::unique_ptr<bg::File> file, const std::string& displayName)
{
	m_stageFile = std::move(file);
	m_stageDisplayName = displayName;
	// Stage tabs default to scale 1.0 — u4ick's tool also renders 1:1, so
	// the bg sprites end up at the same pixel size you'd see in his editor.
	// The pan anchor (m_stageRenderX/Y) is *not* set here; it gets
	// initialized by MainFrame::loadStageFile once clientRect is known so
	// world (0, 0) lands at the proportional position 401/1280, 538/720
	// (the character-feet anchor in u4ick's render window) regardless of the
	// user's window dimensions. Setting it here would jitter on load.
	m_camera.zoom = 1.0f;
	m_stageRenderInit = false;
}

void CharacterView::refreshPanes(Render* render)
{
	if (!m_character) {
		return;
	}

	// Recreate panes with this view's state
	m_mainPane = std::make_unique<MainPane>(render, &m_character->frameData, m_state);
	m_rightPane = std::make_unique<RightPane>(render, &m_character->frameData, m_state);
	m_boxPane = std::make_unique<BoxPane>(render, &m_character->frameData, m_state);

	// Set modification callbacks to mark the character (not the view) as modified
	// Pane widgets edit the view's pattern: an indexed mark keeps the commit to that pattern (cross-pattern pane
	// operations also call FrameData::mark_modified(), which feeds the undo manager).
	auto modifyCallback = [this]() {
		m_character->markModified();
		m_character->undoManager.markModified(m_state.pattern);
	};

	// Panel visibility of new tabs follows the Window menu (saved in the ini).
	if (m_mainPane) m_mainPane->isVisible = gSettings.showAnimPane;
	if (m_rightPane) m_rightPane->isVisible = gSettings.showAttackPane;
	if (m_boxPane) m_boxPane->isVisible = gSettings.showBoxPane;

	if (m_mainPane) {
		m_mainPane->onModified = modifyCallback;
		m_mainPane->RegenerateNames();
		// Hide HA6 editor panes by default in PAT editor mode
		if (m_isPatEditor) {
			m_mainPane->isVisible = false;
		}
	}
	if (m_rightPane) {
		m_rightPane->onModified = modifyCallback;
		// Hide Right Pane (attack params) by default in PAT editor mode
		if (m_isPatEditor) {
			m_rightPane->isVisible = false;
		}
	}
	if (m_boxPane) {
		m_boxPane->onModified = modifyCallback;
		m_boxPane->onResizeDuration=[this](int pattern,int state,int duration) {
			auto* seq=m_character->frameData.get_sequence(pattern);
			if(!seq || state<0 || state>=int(seq->frames.size()) || seq->frames[state].AF.duration==duration) return false;
			m_character->undoManager.beginTransaction("Resize state duration");
			seq->frames[state].AF.duration=duration;
			m_character->frameData.mark_modified(pattern); m_character->markModified(); m_character->undoManager.markModified(pattern);
			m_character->undoManager.commitTransaction();
			m_state.forceSpawnTreeRebuild=true;
			// Keep the preferred local offset: display clamps it to the new duration,
			// and undo can recover the previous location without a navigation history.
			m_state.authoredInspection=true; m_state.animating=false;
			return true;
		};
		// Hide Box Pane by default in PAT editor mode
		if (m_isPatEditor) {
			m_boxPane->isVisible = false;
		}
	}

	// Create PatEditor panes if this is a PAT editor view
	if (m_isPatEditor) {
		// Set up StateReference to point to this view's data
		m_stateRef.framedata = &m_character->frameData;
		m_stateRef.currState = &m_state;
		m_stateRef.cg = &m_character->cg;
		m_stateRef.curPalette = nullptr;  // PAT editor doesn't use palettes
		m_stateRef.currentFilePath = nullptr;
		m_stateRef.currentPatFilePath = nullptr;
		m_stateRef.parts = &m_character->parts;
		m_stateRef.renderMode = &m_state.renderMode;
		m_stateRef.onEdited = [this] { m_character->undoManager.markModified(m_state.pattern); };
		m_stateRef.isPatDocument = [this] { return m_character->isPatDocument(); };
		
		// Update Parts references for PatEditor support
		m_character->parts.updatePatEditorReferences(&m_state, &m_state.renderMode);

		// Create PatEditor panes
		m_partsetPane = std::make_unique<PatPartSetPane>(render, &m_stateRef);
		m_partPane = std::make_unique<PatPartPane>(render, &m_stateRef, m_partsetPane.get());
		m_shapePane = std::make_unique<PatShapePane>(render, &m_stateRef);
		m_texturePane = std::make_unique<PatTexturePane>(render, &m_stateRef);
		m_toolPane = std::make_unique<PatToolPane>(render, &m_stateRef);

		m_partsetPane->isVisible = gSettings.showPatPartSetPane;
		m_partPane->isVisible = gSettings.showPatPartPane;
		m_shapePane->isVisible = gSettings.showPatShapePane;
		m_texturePane->isVisible = gSettings.showPatTexturePane;
		m_toolPane->isVisible = gSettings.showPatToolPane;

		// Regenerate names for all PatEditor panes
		if (m_partsetPane) {
			m_partsetPane->RegeneratePartSetNames();
			m_partsetPane->RegeneratePartPropNames();
		}
		if (m_partPane) {
			m_partPane->RegeneratePartCutOutsNames();
		}
		if (m_shapePane) {
			m_shapePane->RegenerateShapesNames();
		}
		if (m_texturePane) {
			m_texturePane->RegenerateTexturesNames();
		}
		if (m_toolPane) {
			m_toolPane->RegeneratePartsetNames();
		}
	}
}
