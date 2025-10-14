#ifndef CHARACTER_VIEW_H_GUARD
#define CHARACTER_VIEW_H_GUARD

#include "character_instance.h"
#include "main_pane.h"
#include "right_pane.h"
#include "box_pane.h"
#include "state_reference.h"
#include "PatEditor/pat_partset_pane.h"
#include "PatEditor/pat_part_pane.h"
#include "PatEditor/pat_shape_pane.h"
#include "PatEditor/pat_texture_pane.h"
#include "PatEditor/pat_tool_pane.h"
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

	// Get PatEditor panes (only valid when isPatEditor() == true)
	PatPartSetPane* getPartSetPane() const { return m_partsetPane.get(); }
	PatPartPane* getPartPane() const { return m_partPane.get(); }
	PatShapePane* getShapePane() const { return m_shapePane.get(); }
	PatTexturePane* getTexturePane() const { return m_texturePane.get(); }
	PatToolPane* getToolPane() const { return m_toolPane.get(); }

	// Display name (e.g., "Ciel - 400" or "Ciel #2 - 100")
	std::string getDisplayName() const;
	void setViewNumber(int num) { m_viewNumber = num; }
	int getViewNumber() const { return m_viewNumber; }

	// PAT Editor mode
	bool isPatEditor() const { return m_isPatEditor; }
	void setPatEditor(bool enabled) { m_isPatEditor = enabled; }

	// Recreate panes (called when switching to this view)
	void refreshPanes(class Render* render);

	// View-specific rendering settings
	float getZoom() const { return m_zoom; }
	void setZoom(float zoom) { m_zoom = zoom; }

private:
	CharacterInstance* m_character;  // Pointer to shared character data (not owned)
	FrameState m_state;              // Independent view state
	int m_viewNumber = 0;            // 0 = primary, 1+ = additional views
	bool m_isPatEditor = false;      // True if this view is for PAT editing
	float m_zoom = 3.0f;             // Per-view zoom level

	// UI panes for this view
	std::unique_ptr<MainPane> m_mainPane;
	std::unique_ptr<RightPane> m_rightPane;
	std::unique_ptr<BoxPane> m_boxPane;

	// PatEditor panes (only created when isPatEditor == true)
	StateReference m_stateRef;
	std::unique_ptr<PatPartSetPane> m_partsetPane;
	std::unique_ptr<PatPartPane> m_partPane;
	std::unique_ptr<PatShapePane> m_shapePane;
	std::unique_ptr<PatTexturePane> m_texturePane;
	std::unique_ptr<PatToolPane> m_toolPane;
};

#endif /* CHARACTER_VIEW_H_GUARD */
