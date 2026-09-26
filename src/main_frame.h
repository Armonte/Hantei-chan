#ifndef MAINFRAME_H_GUARD
#define MAINFRAME_H_GUARD
#include "background/bg_browser.h"
#include <functional>
#include "var_refs.h"
#include "bgm_player.h"
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
#include "workspace_session.h"
#include "workspace_viewports.h"
#include <imgui.h>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <optional>

struct ImRect;

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
	uint64_t m_startupViewId = 0;
	double m_lastSceneMs = 0.0;   // CPU time of the last main-view scene pass

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
	struct ClosedTab { std::string path; bool isTxt = false; int pattern = 0; int frame = 0; };
	std::vector<ClosedTab> m_closedTabs;   // most recent last (max 10)
	bool reopenClosedTab();
	void createPatEditorView(const std::string& patPath);
	int countViewsForCharacter(CharacterInstance* character);

	void DrawBack();
	void DrawUi();

	// ---- Per-view rendering (ui/view_render_impl.h) ------------------------
	struct SceneOptions {
		int tick = -1;       // -1: the view's current tick and selected frame
		bool grid = true;
		bool boxes = true;   // hitbox overlay
		int spawns = -1;     // -1: the view's setting, 0: off, 1: on
		bool onion = true;   // honour the view's onion-skin settings
	};
	void DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character,
		ImDrawList* drawList, ImVec2 origin, float zoom);
	void AddSimulatedActorLayers(CharacterView* view, const preview::TickState& ts,
		int rootFrameIndex, int layer0Sprite, bool includeSpawns, bool boxes,
		float alphaMul, const glm::vec3* sampleTint);
	void DrawOnionSkin(CharacterView* view, int tick);
	// Draw one character/PAT view into the currently bound target.
	void DrawCharacterScene(CharacterView* view, const Render::PassParams& pass, const SceneOptions& opt);
	void DrawMainViewScene(CharacterView* view, int width, int height);
	void RenderDetachedViewTargets();

	// ---- Detached windows (ui/workspace_hosts_impl.h) -----------------------
	struct HostWindow {
		uint64_t id = 0;
		float x = 120.f, y = 120.f, w = 960.f, h = 720.f; // desktop coordinates
		bool applyGeometry = true;  // push x/y/w/h to ImGui on the next frame
		bool showPanes = true;      // dock the active view's panes in this window
	};
	WorkspaceSession m_session;                 // tab ownership, order, active tab
	std::map<uint64_t, HostWindow> m_hosts;     // detached windows by host id
	uint64_t m_nextHostId = 1;
	uint64_t m_focusedHostId = 0;               // window focused last frame (0 = main)
	uint64_t m_focusHostRequest = 0;            // bring this window to the front
	std::map<uint64_t, uint64_t> m_tabShown;    // tab ImGui showed selected, per host
	std::map<uint64_t, bool> m_tabPushPending;
	struct TabDrag { bool active = false; bool dropped = false; uint64_t viewId = 0; ImVec2 start{}; } m_tabDrag;
	struct PendingTabMove { uint64_t viewId = 0; uint64_t hostId = 0; std::optional<size_t> index; bool valid = false; } m_pendingTabMove;
	int m_pendingTabClose = -1;
	uint64_t m_hostBoxDragView = 0;
	std::vector<CharacterInstance*> m_paneCharacters;  // characters whose panes drew this frame

	CharacterView* findViewById(uint64_t id);
	int findViewIndexById(uint64_t id) const;
	CharacterView* getShortcutView();
	// The view the tool windows (pattern manager, notes, comparison, ...) act
	// on: the active tab of the window (main or detached) the user worked in
	// last. Focusing a floating tool window does not change it.
	CharacterView* getToolView();
	CharacterInstance* getToolCharacter() { auto* v = getToolView(); return v ? v->getCharacter() : nullptr; }
	void NoteToolViewFocus();
	uint64_t m_toolViewId = 0;
	void SyncWorkspaceSession();
	uint64_t DetachViewToNewHost(uint64_t viewId, ImVec2 screenPos);
	void MoveViewToHost(uint64_t viewId, uint64_t hostId, std::optional<size_t> index);
	bool DrawViewTabItem(uint64_t hostId, CharacterView* view, size_t position, bool forceSelect);
	void DrawHostTabs(uint64_t hostId);
	void DrawViewTabContextItems();
	void DrawTabBarDropTarget(uint64_t hostId, const ImRect& barRect);
	void ApplyPendingTabActions();
	void FinishTabDrag();
	void DrawViewPanes(CharacterView* view, const std::string& ns);
	void FinishPaneUndoFrame();
	void DrawOnionSkinControls(CharacterView* view);
	void DrawDetachedViewSurface(uint64_t hostId, CharacterView* view, ImVec2 mn, ImVec2 mx);
	void DrawDetachedHostToolbar(HostWindow& hw, CharacterView* view);
	void DrawDetachedHosts();
	std::string SerializeWorkspace() const;
	void RestoreWorkspace(const std::string& json);
	void DrawRenderMenu();

	// ---- PNG export (ui/png_export_impl.h) ----------------------------------
	struct ExportSettings {
		enum { currentFrame = 0, tickRange = 1, wholePattern = 2 };
		enum { transparent = 0, editorColor = 1, customColorBg = 2 };
		enum { fitAll = 0, fitEach = 1, fixedCanvas = 2 };
		int range = wholePattern;
		int fromTick = 0, toTick = 30, step = 1, maxTicks = 600;
		bool keyframesOnly = false;
		bool skipEmpty = true;
		bool spawns = true;
		bool boxes = false;
		bool smooth = false;
		int scale = 1;
		int background = transparent;
		float customColor[3] = {1.f, 0.f, 1.f};
		int crop = fitAll;
		int padding = 4;
		int canvasW = 512, canvasH = 512, canvasOriginX = 256, canvasOriginY = 448;
		std::string folder;      // UTF-8
		std::string baseName;    // UTF-8, sanitised when used
		bool writeManifest = true;
	} m_export;
	struct ExportResult {
		bool ok = false;
		int files = 0, skipped = 0;
		double ms = 0.0;
		std::string folder, firstFile, message;
	} m_lastExport;
	enum class ExportRequest { none, quickFrame, run } m_exportRequest = ExportRequest::none;
	uint64_t m_exportViewId = 0;
	bool m_showExportWindow = false;
	bool m_exportBaseAuto = true;
	double m_lastExportTime = -100.0;
	std::vector<int> ExportTicks(CharacterView* view, const ExportSettings& s, bool& liveFrame);
public:
	bool RunPngExport(CharacterView* view, const ExportSettings& s, ExportResult& result);
	static WorkspaceViewports::KeyHook DetachedKeyHook();
private:
	std::string DefaultExportFolder(CharacterView* view) const;
	void PrepareExportSettings(CharacterView* view);
	void ProcessPendingExport();
	void DrawExportWindow();
	void DrawRenderToolWindows();

	// ---- MBAACC package tools (ui/package_tools_impl.h) ----------------------
	void DrawPackageToolWindows();
	void DrawPackageToolsMenuItems();
	preview::TickState m_sceneState, m_onionState;  // reused buffers
	struct OnionStats { int samples = 0; double simMs = 0.0, totalMs = 0.0; } m_onionStats;
	void Menu(unsigned int errorId);

	void RenderUpdate();
	void AdvanceViewPlayback(CharacterView* view);
	void AdvancePattern(int dir) { AdvancePattern(getActiveView(), dir); }
	void AdvanceFrame(int dir) { AdvanceFrame(getActiveView(), dir); }
	void AdvancePattern(CharacterView* view, int dir);
	void AdvanceFrame(CharacterView* view, int dir);

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

	// Stage browser + metadata (BgList.ini / bgm.txt) of the game the stage
	// belongs to (background/bg_project.h, bg_browser.h).
	bg::StageProject stageProject;
	bool m_showStageBrowser = false;
	bool m_showGameViewRect = true;   // outline of the game's 640x480 view at the game camera
	void drawStageBrowser();
	// Opens the remembered stage list (settings) if none is open yet.
	bool ensureStageProject();
	// Dropdown of the game's stages; picking one loads it in the stage tab.
	void drawStageCombo(float width);
	// Replace the active stage tab's file (keeps the camera), or open a tab.
	void openStageInActiveTab(const std::string& path);
	void stepStage(int dir);          // PageUp / PageDown in the stage tab
	// Edit > Undo / Redo for stage tabs: object + Info.txt edits (bg::File)
	// and metadata edits (StageProject) share one timeline by edit sequence.
	bool stageUndoRedo(bool redo, bool apply);
	std::string stageUndoLabel(bool redo);
	void saveStageAll();
	// "Show in game" for the browser: set by the Game Link stage switch when
	// it is available (PovertyCaster mbaacc/stage-link); null until then.
	std::function<bool(int stageId, const std::string& datPath)> stageShowInGame;

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
	void drawGameLink();   // Game Link window (ui/main_frame_gamelink.cpp)
	void drawTagPanel();   // Tag / Team window, experimental (ui/main_frame_gamelink.cpp, tag_panel.h)
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
	bool m_showKeyBindings = false;
	bool m_showCompare = false;
	bool m_showBgm = false;
	struct BgmWindow {
		std::string folder;
		std::vector<BgmEntry> entries;
		int selected = -1;
		std::string status;
		float leadIn = 5.0f;
		float volume = 0.8f;
		std::unique_ptr<BgmPlayer> player;
	} m_bgm;
	void drawBgmWindow();
	bool m_showHud = false;
	struct HudTexture { std::string name; unsigned id = 0; int w = 0, h = 0; };
	struct HudWindow {
		std::string gameDir;
		std::vector<uint32_t> colors;
		std::string status;
		int meterMode = 0;
		float meterPct = 120.f;
		bool halfMoon = false;
		float guardQuality = 0.8f;
		bool guardBroken = false;
		std::string texturesFor;
		std::vector<HudTexture> textures;
	} m_hud;
	void drawHudWindow();
	int m_keyCapture = -1;   // binding index waiting for a key (Keyboard shortcuts window)
	int m_textNavDir = 0;    // keyframe step requested from a text field
public:
	static wchar_t s_swallowChar; // WM_CHAR to drop (the key already acted as a shortcut)
private:
	void drawKeyBindingsWindow();
public:
	// Pattern comparison overlay (issue #63)
	struct CompareState {
		bool enabled = false;
		CharacterInstance* character = nullptr;  // validated every frame
		int pattern = 0;
		int frame = 0;
		bool followTick = true;     // step with the main view's tick
		bool movement = false;      // offset by the simulated root movement difference
		bool mirror = false;
		int offsetX = 0, offsetY = 0;
		float alpha = 0.55f;
		float tint[3] = {1.0f, 0.55f, 0.55f};
		bool boxes = true;
		std::shared_ptr<preview::PreviewSim> sim;
	};
private:
	CompareState m_compare;
	void drawCompareWindow();
	void AddCompareLayers(CharacterView* view, CharacterInstance* active);
	void drawNotesWindow();
	void markToolEdit(CharacterInstance* character);
	void navigateActiveView(int pattern, int frame);
	bool PerformUndoRedo(CharacterView* view, bool redo);
	void RefreshViewsAfterHistory(CharacterView* activeView, CharacterInstance* character, const UndoManager::Entry* entry);
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
