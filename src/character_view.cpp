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
	auto modifyCallback = [this]() { m_character->markModified(); };
	if (m_mainPane) {
		m_mainPane->onModified = modifyCallback;
		m_mainPane->RegenerateNames();
	}
	if (m_rightPane) m_rightPane->onModified = modifyCallback;
	if (m_boxPane) m_boxPane->onModified = modifyCallback;
}
