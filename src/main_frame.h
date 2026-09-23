#ifndef MAINFRAME_H_GUARD
#define MAINFRAME_H_GUARD
#include "var_refs.h"
#include "pattern_refs.h"
#include "context_gl.h"
#include "render.h"
#include "main_pane.h"
#include "right_pane.h"
#include "box_pane.h"
#include "about.h"
#include "vectors.h"
#include "character_instance.h"
#include "character_view.h"
#include "state_reference.h"
#include "PatEditor/pat_partset_pane.h"
#include "PatEditor/pat_part_pane.h"
#include "PatEditor/pat_shape_pane.h"
#include "PatEditor/pat_texture_pane.h"
#include "PatEditor/pat_tool_pane.h"
#include "background/bg_file.h"
#include "background/bg_renderer.h"
#include "background/bg_types.h"
#include "cmdfile/cmd_editor_ui.h"
#include "shortcut_router.h"
#include <imgui.h>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>
#include <memory>

class MainFrame
{
public:
	MainFrame(ContextGl *context);
	~MainFrame();
	
	void Draw();
	void UpdateBackProj(float x, float y);
	void HandleMouseDrag(int x, int y, bool dragRight, bool dragLeft);
	// Drag start/end edges — currently used to begin/end the bg camera
	// parallax-preview drag (u4ick's movingPoint/movingPoint_last dance).
	void HandleMouseDown(bool dragRight, bool dragLeft);
	void HandleMouseUp(bool dragRight, bool dragLeft);
	// Key press routed through the central shortcut table. `imguiWantsKeyboard`
	// / `imguiTextInput` are ImGui's io.WantCaptureKeyboard / io.WantTextInput:
	// a focused text field keeps its keys (incl. its own Ctrl+Z).
	bool HandleKeys(uint64_t vkey, bool isRepeat = false,
	                bool imguiWantsKeyboard = false, bool imguiTextInput = false);
	// Left press in the viewport (after HandleMouseDown); starts a position
	// tool drag when it lands on a handle.
	void LeftClick(int x, int y);
	// Window lost capture/activation mid-gesture: close box drags, cancel
	// position drags.
	void CancelViewportGestures();
	void HandleMouseWheel(bool isIncrease, int mouseX, int mouseY);

	void RightClick(int x, int y);
	void LoadSettings();
	void ProcessStartupArgs();   // --open / --capture (startup_args.cpp)

private:
	ContextGl *context;
	float clearColor[3];
	int style_idx = 0;
	float zoom_idx = 3.0f;
	bool smoothRender = false;


	Render render;
	VectorTXT vectors;

	// Multi-character/view architecture
	std::vector<std::unique_ptr<CharacterInstance>> characters;  // Shared character data
	std::vector<std::unique_ptr<CharacterView>> views;           // Independent views
	int activeViewIndex = -1;

	std::string currentFilePath;

	// Helper methods
	CharacterView* getActiveView();
	const CharacterView* getActiveView() const;
	CharacterInstance* getActiveCharacter();
	const CharacterInstance* getActiveCharacter() const;

	void setActiveView(int index);
	void closeView(int index);
	bool tryCloseView(int index); // Returns true if closed, false if cancelled

	// Character/view management
	CharacterInstance* findCharacterByPath(const std::string& path);
	void createViewForCharacter(CharacterInstance* character);
	void createPatEditorView(const std::string& patPath);
	int countViewsForCharacter(CharacterInstance* character);

	void DrawBack();
	void DrawUi();
	void DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character);
	void Menu(unsigned int errorId);

	void RenderUpdate();
	void AdvancePattern(int dir);
	void AdvanceFrame(int dir);

	void SetZoom(float level);
	void LoadTheme(int i );
	void WarmStyle();
	void ChangeClearColor(float r, float g, float b);

	int mDeltaX = 0, mDeltaY = 0;

	// View close confirmation
	int pendingCloseViewIndex = -1;
	bool shouldOpenUnsavedDialog = false;

	// Right-click context menu
	int contextMenuViewIndex = -1;

	// Dock layout rebuild flag (set when switching to PatEditor mode)
	bool needsDockRebuild = false;

	// Project management
	bool m_projectModified = false;
	bool m_pendingProjectClose = false;
	bool shouldOpenUnsavedProjectDialog = false;
	enum class ProjectCloseAction { None, New, Open, Close } m_projectCloseAction = ProjectCloseAction::None;
	std::string m_pendingProjectPath; // Open target awaiting the unsaved-changes prompt (empty = ask with file dialog)

	// Project actions are never run from inside a menu/popup: the public entry
	// points below only queue a request, and processDeferredProjectAction()
	// runs it at the top level of DrawUi.
	ProjectCloseAction m_deferredProjectAction = ProjectCloseAction::None;
	std::string m_deferredProjectPath;
	bool m_deferredProjectConfirmed = false; // true = unsaved-changes prompt already answered
	void requestProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed);
	void processDeferredProjectAction();
	void runProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed);
	void clearProjectState();
	void loadProjectFromPath(const std::string& path, bool isRecent);
	bool saveAllModifiedCharacters();

	// Error popups requested from menus/popups/shortcuts are opened at the top
	// level of DrawUi so their ID matches the BeginPopupModal that draws them.
	const char* m_pendingErrorPopup = nullptr;
	std::string m_errorDetail;
	void requestErrorPopup(const char* popupName, const std::string& detail = std::string());
	bool saveCharacter(CharacterInstance* character);  // Reports failure to the user
	bool saveCharacterAs(CharacterInstance* character, const std::string& path);

	void newProject();
	void openProject();
	void saveProject();
	void saveProjectAs();
	void closeProject();
	void updateWindowTitle();
	bool tryCloseProject(); // Returns true if closed, false if cancelled
	void markProjectModified();
	void addRecentProject(const std::string& path);
	void openRecentProject(const std::string& path);

	// PatEditor panes
	StateReference stateRef;
	std::unique_ptr<PatPartSetPane> partsetPane;
	std::unique_ptr<PatPartPane> partPane;
	std::unique_ptr<PatShapePane> shapePane;
	std::unique_ptr<PatTexturePane> texturePane;
	std::unique_ptr<PatToolPane> toolPane;
	void updateStateReference();

	// Background (stage) system — ported from bgmk branch. The renderer and
	// camera are owned here and handed to Render via SetBackgroundRenderer.
	bg::File*    currentBgFile = nullptr;
	bg::Renderer bgRenderer;
	bg::Camera   bgCamera;
	void loadStageFile(const std::string& path);
	void clearStage();

	// Stage-view smooth zoom-to-cursor. A wheel tick bumps bgZoomTarget;
	// DrawBack eases render.scale toward it and re-pins the world point
	// that was under the cursor (bgZoomAnchorWorld) at the cursor's
	// screen position (bgZoomAnchorScrn) every frame of the animation.
	float bgZoomTarget       = 1.0f;
	bool  bgZoomAnimating    = false;
	float bgZoomAnchorWorldX = 0.0f, bgZoomAnchorWorldY = 0.0f;
	float bgZoomAnchorScrnX  = 0.0f, bgZoomAnchorScrnY  = 0.0f;

	AboutWindow aboutWindow;

	// _c.txt command-file editor (ui/main_frame_cmdfile.cpp).
	cmdfile::CommandFileEditor commandEditor;
	void openCommandEditor(const std::string& path);
	void openCommandEditorForActive();
	void loadCommandsForActive(const std::string& path);
	void drawCommandEditor();
	bool m_commandShortcutsRegistered = false;
	// ---- Editing tools (ui/editor_tools_impl.h) ----------------------------
	ShortcutRouter shortcuts;
	bool RunShortcut(ShortcutAction action);
	bool NudgeLayer(CharacterView* view, int dx, int dy);
	// ---- Tool windows (ui/tool_windows_impl.h) ------------------------------
	struct VarRefsWindow {
		bool open = false;
		unsigned categories = varrefs::catAll;
		int from = 0, to = 0;
		bool searched = false;
		std::vector<varrefs::Ref> results;
		std::string status;
	} m_varRefs;
	void drawVarRefsWindow();
	struct PatternManagerWindow {
		bool open = false;
		CharacterInstance* character = nullptr;
		std::vector<int> selection;   // click order
		int lastClicked = -1;
		char filter[64] = {};
		bool hideEmpty = false;
		int placement = 0;
		int target = 0;
		bool remapPaste = true;
		bool remapMove = true;
		std::string status;
		std::vector<patrefs::Ref> refs;
		int refsFor = -1;
		uint64_t refsVersion = 0;
	} m_patMgr;
	void drawPatternManagerWindow();
	bool m_showNotes = false;
	void drawNotesWindow();
	void markToolEdit(CharacterInstance* character);
	void navigateActiveView(int pattern, int frame);
	bool PerformUndoRedo(bool redo);
	void RefreshViewsAfterHistory(CharacterInstance* character, const UndoManager::Entry* entry);
	bool isLiveCharacter(const CharacterInstance* character) const;

	// Right-button box drawing is one undo transaction.
	CharacterInstance* m_boxDragCharacter = nullptr;
	void EndBoxDrag();

	// J/K/L transport. Forward playback is FrameState::animating; reverse
	// playback is driven here, one tick per UI frame, for one view.
	CharacterView* m_reverseView = nullptr;
	void UpdateTransport();
	void StepTick(CharacterView* view, int dir);
	void StopTransport(CharacterView* view);

	// Viewport position tool.
	struct PositionTarget {
		enum Kind { Layer, Effect } kind = Layer;
		int index = -1;           // layer index or EF index
		int xParam = 0, yParam = 1; // EF parameter slots (Effect only)
		ImVec2 screen{};          // handle position in client pixels
		float m[4] = {1, 0, 0, 1}; // screen delta = M * authored delta (column-major 2x2)
		bool invertible = true;
		std::string label;
		ImU32 color = 0;
	};
	struct PositionDrag {
		bool active = false;
		CharacterInstance* character = nullptr;
		int pattern = -1, frame = -1;
		PositionTarget target;
		int startX = 0, startY = 0;
		float totalDX = 0, totalDY = 0;
	} m_posDrag;
	int m_posHoverKind = -1, m_posHoverIndex = -1;
	std::vector<PositionTarget> CollectPositionTargets();
	void DrawPositionTool();
	void PositionDragBy(int dx, int dy);
	void EndPositionDrag(bool cancel);
};


#endif /* MAINFRAME_H_GUARD */
