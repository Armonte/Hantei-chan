#ifndef CHARACTER_VIEW_H_GUARD
#define CHARACTER_VIEW_H_GUARD

#include "character_instance.h"
#include "main_pane.h"
#include "right_pane.h"
#include "box_pane.h"
#include <memory>
#include <string>

// A view represents an independent viewport into a CharacterInstance
// Multiple views can share the same CharacterInstance (synchronized editing)
// but each has its own FrameState (independent navigation)
class CharacterView
{
public:
	CharacterView(CharacterInstance* character, class Render* render);
	~CharacterView();

	// Get the underlying character data
	CharacterInstance* getCharacter() const { return m_character; }

	// Get independent view state
	FrameState& getState() { return m_state; }
	const FrameState& getState() const { return m_state; }

	// Get panes for this view
	MainPane* getMainPane() const { return m_mainPane.get(); }
	RightPane* getRightPane() const { return m_rightPane.get(); }
	BoxPane* getBoxPane() const { return m_boxPane.get(); }

	// Display name (e.g., "Angel" or "Angel (2)")
	std::string getDisplayName() const;
	void setViewNumber(int num) { m_viewNumber = num; }

	// Recreate panes (called when switching to this view)
	void refreshPanes(class Render* render);

private:
	CharacterInstance* m_character;  // Pointer to shared character data (not owned)
	FrameState m_state;              // Independent view state
	int m_viewNumber = 0;            // 0 = primary, 1+ = additional views

	// UI panes for this view
	std::unique_ptr<MainPane> m_mainPane;
	std::unique_ptr<RightPane> m_rightPane;
	std::unique_ptr<BoxPane> m_boxPane;
};

#endif /* CHARACTER_VIEW_H_GUARD */
