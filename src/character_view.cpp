#include "character_view.h"
#include "render.h"

CharacterView::CharacterView(CharacterInstance* character, Render* render)
	: m_character(character), m_viewNumber(0)
{
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

	return name;
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
	auto modifyCallback = [this]() {
		m_character->markModified();
		m_character->undoManager.markModified();
	};

	if (m_mainPane) {
		m_mainPane->onModified = modifyCallback;
		m_mainPane->RegenerateNames();
	}
	if (m_rightPane) {
		m_rightPane->onModified = modifyCallback;
	}
	if (m_boxPane) {
		m_boxPane->onModified = modifyCallback;
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
		
		// Update Parts references for PatEditor support
		m_character->parts.updatePatEditorReferences(&m_state, &m_state.renderMode);

		// Create PatEditor panes
		m_partsetPane = std::make_unique<PatPartSetPane>(render, &m_stateRef);
		m_partPane = std::make_unique<PatPartPane>(render, &m_stateRef, m_partsetPane.get());
		m_shapePane = std::make_unique<PatShapePane>(render, &m_stateRef);
		m_texturePane = std::make_unique<PatTexturePane>(render, &m_stateRef);
		m_toolPane = std::make_unique<PatToolPane>(render, &m_stateRef);

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
