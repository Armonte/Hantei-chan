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
#include "background/bg_file.h"
#include "render_target.h"
#include <glm/vec3.hpp>
#include <cstdint>
#include <memory>
#include <string>

// Per-view camera. pan is the screen-pixel offset of world (0,0) from the
// centre of the view's render target (the old CharacterInstance::renderX/Y,
// which every view of a character used to share).
struct ViewCamera {
	float panX = 0.f;
	float panY = 150.f;
	float zoom = 3.f;
};

// Onion skin: ghost samples N ticks before/after the current tick, read from
// the view's cached preview simulator (docs/HANTEI_WAVE2.md §4).
struct OnionSkinSettings {
	bool enabled = false;
	int before = 2;            // samples before the current tick
	int after = 2;             // samples after the current tick
	int spacing = 3;           // ticks between samples
	bool keyframesOnly = false;// step by root keyframe entries instead of ticks
	bool includeSpawns = true; // ghost spawned actors too
	float alpha = 0.45f;       // opacity of the nearest sample
	float falloff = 0.65f;     // opacity multiplier per further sample
	glm::vec3 pastTint{1.0f, 0.45f, 0.45f};
	glm::vec3 futureTint{0.45f, 0.75f, 1.0f};
};

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

	// Stage mode — view shows a bgmake .dat file instead of a character. The
	// view owns the bg::File*. character may be null for stage-only views.
	bool isStageView() const { return m_stageFile != nullptr; }
	bg::File* getStageFile() const { return m_stageFile.get(); }
	void setStageFile(std::unique_ptr<bg::File> file, const std::string& displayName);
	const std::string& getStageDisplayName() const { return m_stageDisplayName; }

	// Recreate panes (called when switching to this view)
	void refreshPanes(class Render* render);

	// View-specific rendering settings
	float getZoom() const { return m_camera.zoom; }
	void setZoom(float zoom) { m_camera.zoom = zoom; }
	ViewCamera& camera() { return m_camera; }
	const ViewCamera& camera() const { return m_camera; }
	OnionSkinSettings& onion() { return m_onion; }
	const OnionSkinSettings& onion() const { return m_onion; }

	// Stable identity for the workspace session (tab ownership, persistence).
	uint64_t getId() const { return m_id; }
	void setId(uint64_t id);

	// The view's own off-screen target (docs/HANTEI_WAVE2.md §2).
	RenderTarget& renderTarget() { return m_target; }
	// Surface size requested by whatever displays this view this frame
	// (main window client rect or a detached window's image area).
	int surfaceWidth = 0, surfaceHeight = 0;
	bool surfaceVisible = false;

private:
	CharacterInstance* m_character;  // Pointer to shared character data (not owned)
	FrameState m_state;              // Independent view state
	int m_viewNumber = 0;            // 0 = primary, 1+ = additional views
	bool m_isPatEditor = false;      // True if this view is for PAT editing
	ViewCamera m_camera;             // Per-view pan + zoom
	OnionSkinSettings m_onion;
	uint64_t m_id = 0;
	RenderTarget m_target;

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

	// Stage view state (owned by the view, freed when view is destroyed).
	std::unique_ptr<bg::File> m_stageFile;
	std::string m_stageDisplayName;
	// Per-stage-tab pan state (Hantei world-space units). Lets the user pan
	// each stage tab independently; restored by setActiveView.
	float m_stageRenderX = 0.0f;
	float m_stageRenderY = 0.0f;
	bool  m_stageRenderInit = false;
public:
	float getStageRenderX() const { return m_stageRenderX; }
	float getStageRenderY() const { return m_stageRenderY; }
	void  setStageRenderXY(float rx, float ry) { m_stageRenderX = rx; m_stageRenderY = ry; }
	bool  isStageRenderInit() const { return m_stageRenderInit; }
	void  setStageRenderInit(bool v) { m_stageRenderInit = v; }
};

#endif /* CHARACTER_VIEW_H_GUARD */
