#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"
#include "imgui_utils.h"
#include "project_manager.h"
#include "preset_effects.h"
#include "version.h"
#include "framestate.h"
#include "misc.h"
#include "background/bg_inspector.h"
#include "extension_profile.h"
#include "../third_party/json/json.hpp"
#include "framedata_ha4.h"
#include "ha4_character.h"
#include "game_link_panel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

wchar_t MainFrame::s_swallowChar = 0;

MainFrame::MainFrame(ContextGl *context_):
context(context_)
{
	LoadSettings();
	// User key bindings (issue #9); unknown or stale entries are ignored.
	for (const auto& line : gSettings.keyBindings)
		shortcuts.registry().applyOverride(line);
	// Hand the background renderer/camera to the GL Render so its Draw()
	// loop calls into bgRenderer at the right point (behind the character).
	render.SetBackgroundRenderer(&bgRenderer, &bgCamera);
	// The bg renderer draws PAT-pattern stage objects through Render's
	// Parts pipeline (sprite-id < 10000) — give it the back-reference.
	bgRenderer.SetHostRender(&render);
	// Stage edits keep their own history (bg::File), separate from the
	// character undo stack: while a stage view owns focus, Ctrl+Z / Ctrl+Y
	// go to the stage and Ctrl+S saves the stage file.
	shortcuts.setContextHandler(ShortcutContext::stageView, [this](ShortcutAction a) {
		switch (a) {
		case ShortcutAction::undo: return stageUndoRedo(false, true);
		case ShortcutAction::redo: return stageUndoRedo(true, true);
		case ShortcutAction::save: if (!currentBgFile) return false; saveStageAll(); return true;
		case ShortcutAction::nextStage: stepStage(1); return true;
		case ShortcutAction::previousStage: stepStage(-1); return true;
		default: return false;
		}
	});
}

MainFrame::~MainFrame()
{
	clearStage();
	ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
}

void MainFrame::LoadSettings()
{
	LoadTheme(gSettings.theme);
	SetZoom(gSettings.zoomLevel);
	smoothRender = gSettings.bilinear;
	memcpy(clearColor, gSettings.color, sizeof(float)*3);
}

void MainFrame::Draw()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	DrawUi();
	DrawBack();
	ImGui::Render();

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	ProcessStartupArgs();

	// Detached windows (ImGui platform windows) render through the same GL
	// context, each with its own DC; the main window is current again after.
	WorkspaceViewports::RenderSecondaryWindows();

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

// ============================================================================
// Per-view scene rendering (render targets, onion skin, detached views, PNG
// export) - see ui/view_render_impl.h and docs/HANTEI_WAVE2.md
// ============================================================================
#include "ui/view_render_impl.h"
#include "ui/workspace_hosts_impl.h"
#include "ui/png_export_impl.h"
#include "ui/package_tools_impl.h"


// ============================================================================
// UI Implementation - extracted to ui/main_ui_impl.h for better organization
// ============================================================================
#include "ui/main_ui_impl.h"

// ============================================================================
// Menu Implementation - extracted to ui/main_menu_impl.h
// ============================================================================
#include "ui/main_menu_impl.h"

// ============================================================================
// Editing tools (shortcuts, undo/redo, J/K/L, box-drag undo, position tool)
// ============================================================================
#include "ui/editor_tools_impl.h"
#include "ui/tool_windows_impl.h"


void MainFrame::WarmStyle()
{
	ImVec4* colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.95f, 0.91f, 0.85f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.98f, 0.96f, 0.93f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.95f, 1.00f, 0.62f, 1.00f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 1.00f, 0.81f, 1.00f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.82f, 0.73f, 0.64f, 0.81f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.97f, 0.65f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(1.00f, 0.96f, 0.87f, 0.99f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.98f, 0.94f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.72f, 0.66f, 0.48f, 0.99f);
	colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.55f, 0.53f, 0.32f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.74f, 1.00f, 0.53f, 0.25f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.77f, 0.41f, 0.96f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.47f, 0.00f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.74f, 0.57f, 0.33f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.94f, 0.75f, 0.36f, 0.42f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.75f, 0.01f, 0.61f);
	colors[ImGuiCol_Separator]              = ImVec4(0.38f, 0.34f, 0.25f, 0.66f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.76f, 0.70f, 0.59f, 0.98f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.32f, 0.32f, 0.32f, 0.45f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.41f, 0.80f, 1.00f, 0.84f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.61f, 0.23f, 1.00f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.79f, 0.74f, 0.64f, 0.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.64f, 0.06f, 0.85f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.69f, 0.40f, 0.12f, 0.31f);
	colors[ImGuiCol_TabUnfocused]           = ImVec4(0.93f, 0.92f, 0.92f, 0.98f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.91f, 0.87f, 0.74f, 1.00f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.98f, 0.35f, 0.22f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.58f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.40f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.69f, 0.53f, 0.32f, 0.30f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.69f, 0.58f, 0.44f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.70f, 0.62f, 0.42f, 0.40f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.98f, 0.89f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.98f, 0.94f, 0.95f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.97f, 0.98f, 0.80f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	ChangeClearColor(0.324f, 0.409f, 0.185f);
}

// Multi-character/view support helper methods

CharacterView* MainFrame::getActiveView()
{
	if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
		return views[activeViewIndex].get();
	}
	return nullptr;
}

const CharacterView* MainFrame::getActiveView() const
{
	if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
		return views[activeViewIndex].get();
	}
	return nullptr;
}

CharacterInstance* MainFrame::getActiveCharacter()
{
	auto* view = getActiveView();
	return view ? view->getCharacter() : nullptr;
}

const CharacterInstance* MainFrame::getActiveCharacter() const
{
	auto* view = getActiveView();
	return view ? view->getCharacter() : nullptr;
}

void MainFrame::setActiveView(int index)
{
	if (index >= 0 && index < views.size()) {
		// A view in a detached window is shown there: select its tab and
		// raise that window instead of taking over the main window.
		const uint64_t viewId = views[index]->getId();
		const auto owner = m_session.owner(viewId);
		if (owner && *owner != WorkspaceSession::MainHost) {
			m_session.select(*owner, viewId);
			m_focusHostRequest = *owner;
			return;
		}
		if (!owner) m_session.add(viewId, WorkspaceSession::MainHost, true);
		else m_session.select(WorkspaceSession::MainHost, viewId);
		activeViewIndex = index;

		// Update render state to use this view's character and settings
		auto* view = getActiveView();
		if (view) {
			auto* character = view->getCharacter();
			if (character) {
				// Update CG reference (this resets curImageId)
				render.SetCg(&character->cg);

				// Set Parts if this view has them loaded (PAT editor or character with PAT)
				// SetParts internally handles texture clearing when switching between Parts/CG
				if (character->parts.loaded) {
					render.SetParts(&character->parts);
				} else {
					render.SetParts(nullptr);  // Clear Parts for non-PAT views
				}
			}

			// Restore this view's zoom level
			float viewZoom = view->getZoom();
			render.scale = viewZoom;
			zoom_idx = viewZoom;  // Update UI slider

			// Point the background renderer at this view's stage (if any).
			// currentBgFile mirrors the active tab's stage so the Stage menu
			// and Background Inspector keep working transparently.
			if (view->isStageView()) {
				currentBgFile = view->getStageFile();
				bgRenderer.SetFile(currentBgFile);
				bgRenderer.SetEnabled(true);
				// Apply stored pan from the view into bgCamera (the source
				// of truth) and mirror into render.x/y so the very first
				// DrawBackground call this frame uses correct coords.
				bgCamera.SetPan(view->getStageRenderX(),
				                view->getStageRenderY());
				render.x = bgCamera.panLastX;
				render.y = bgCamera.panLastY;
			} else {
				currentBgFile = nullptr;
				bgRenderer.SetFile(nullptr);
				bgRenderer.SetEnabled(false);
			}
		}
	}
}

CharacterInstance* MainFrame::findCharacterByPath(const std::string& path)
{
	for (auto& character : characters) {
		if (character->getTxtPath() == path || character->getTopHA6Path() == path) {
			return character.get();
		}
	}
	return nullptr;
}

void MainFrame::createViewForCharacter(CharacterInstance* character)
{
	if (!character) return;

	// Find the lowest available view number for this character
	int viewNumber = 0;
	bool foundNumber = false;
	while (!foundNumber) {
		foundNumber = true;
		for (auto& view : views) {
			if (view->getCharacter() == character && view->getViewNumber() == viewNumber) {
				viewNumber++;
				foundNumber = false;
				break;
			}
		}
	}

	auto view = std::make_unique<CharacterView>(character, &render);
	view->setViewNumber(viewNumber);
	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
}

bool MainFrame::reopenClosedTab()
{
	while (!m_closedTabs.empty()) {
		ClosedTab t = m_closedTabs.back();
		m_closedTabs.pop_back();
		CharacterInstance* character = findCharacterByPath(t.path);
		if (!character) {
			auto loaded = std::make_unique<CharacterInstance>();
			const bool ok = t.isTxt ? loaded->loadFromTxt(t.path) : loaded->loadHA6(t.path, false);
			if (!ok) continue;
			character = loaded.get();
			characters.push_back(std::move(loaded));
		}
		createViewForCharacter(character);
		markProjectModified();
		if (auto* view = getActiveView()) {
			auto& st = view->getState();
			st.pattern = t.pattern;
			st.frame = t.frame;
			Sequence* seq = character->frameData.get_sequence(st.pattern);
			if (!seq) st.pattern = 0;
			seq = character->frameData.get_sequence(st.pattern);
			const int frames = seq ? (int)seq->frames.size() : 0;
			if (st.frame >= frames) st.frame = frames > 0 ? frames - 1 : 0;
			st.currentTick = frames > 0 ? CalculateTickFromFrame(&character->frameData, st.pattern, st.frame) : 0;
			if (view->getMainPane()) view->getMainPane()->RegenerateNames();
		}
		return true;
	}
	return false;
}

void MainFrame::createPatEditorView(const std::string& patPath)
{
	// Create a dummy character for PAT editing
	auto character = std::make_unique<CharacterInstance>();
	character->frameData.initEmpty();

	// Extract filename from path for display name
	size_t lastSlash = patPath.find_last_of("/\\");
	std::string filename = (lastSlash != std::string::npos) ? patPath.substr(lastSlash + 1) : patPath;
	character->setName(filename);

	// Create dummy frame with usePat layer
	auto seq = character->frameData.get_sequence(0);
	if (seq) {
		auto frame = &seq->frames.emplace_back();
		// Ensure frame has at least one layer
		if (frame->AF.layers.empty()) {
			frame->AF.layers.push_back({});
		}
		frame->AF.layers[0].usePat = true;
		frame->AF.layers[0].spriteId = 0;  // Initialize to first partSet for .pat rendering
	}

	// Load the PAT file
	if (!character->loadPAT(patPath)) {
		// Failed to load - don't create the view
		return;
	}

	// Set up Parts rendering
	render.SetParts(&character->parts);

	// Add character to list and create PatEditor view
	characters.push_back(std::move(character));
	auto view = std::make_unique<CharacterView>(characters.back().get(), &render);
	view->setPatEditor(true);
	view->setViewNumber(0);

	// Refresh panes AFTER setting isPatEditor flag to create PatEditor panes
	// This will automatically hide HA6 editor panes and show PAT editor panes
	view->refreshPanes(&render);

	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
	markProjectModified();

	// Force dock layout rebuild to show PatEditor panes
	needsDockRebuild = true;
}

int MainFrame::countViewsForCharacter(CharacterInstance* character)
{
	int count = 0;
	for (auto& view : views) {
		if (view->getCharacter() == character) {
			count++;
		}
	}
	return count;
}

void MainFrame::closeView(int index)
{
	if (index >= 0 && index < views.size()) {
		auto* view = views[index].get();
		auto* character = view->getCharacter();
		const uint64_t view_id = view->getId();
		if (m_reverseView == view) m_reverseView = nullptr;

		// Remember it for Reopen closed tab (issue #61). Only views whose
		// character can be loaded again from a file.
		if (character && !view->isStageView() && !view->isPatEditor()) {
			ClosedTab t;
			t.path = !character->getTxtPath().empty() ? character->getTxtPath() : character->getTopHA6Path();
			t.isTxt = !character->getTxtPath().empty();
			t.pattern = view->getState().pattern;
			t.frame = view->getState().frame;
			if (!t.path.empty()) {
				m_closedTabs.push_back(t);
				if (m_closedTabs.size() > 10) m_closedTabs.erase(m_closedTabs.begin());
			}
		}

		// Remove the view
		views.erase(views.begin() + index);
		markProjectModified();

		// If this was the last view for this character, remove the character too
		if (character && countViewsForCharacter(character) == 0) {
			for (size_t i = 0; i < characters.size(); i++) {
				if (characters[i].get() == character) {
					characters.erase(characters.begin() + i);
					break;
				}
			}
		}

		// Update active index: the main window shows its own next tab.
		m_session.close(view_id);
		if (const auto* mainHost = m_session.host(WorkspaceSession::MainHost))
			activeViewIndex = findViewIndexById(mainHost->active);
		if (views.empty() || activeViewIndex < 0) {
			activeViewIndex = -1;

			// Clear the render system
			render.DontDraw();
			render.ClearTexture();
			render.SetCg(nullptr);
			render.SetParts(nullptr);
		} else {
			setActiveView(activeViewIndex);
		}
	}
}

bool MainFrame::tryCloseView(int index)
{
	if (index < 0 || index >= views.size()) {
		return false;
	}

	auto& view = views[index];
	auto* character = view->getCharacter();

	// Only prompt if this is the last view and character is modified
	if (character && character->isModified() && countViewsForCharacter(character) == 1) {
		// Show unsaved changes dialog
		pendingCloseViewIndex = index;
		shouldOpenUnsavedDialog = true;
		return false; // Not closed yet, user needs to confirm
	}

	closeView(index);
	return true;
}

// ============================================================================
// Project Management
// ============================================================================

void MainFrame::markProjectModified()
{
	m_projectModified = true;
	updateWindowTitle();
}

void MainFrame::requestErrorPopup(const char* popupName, const std::string& detail)
{
	m_pendingErrorPopup = popupName;
	m_errorDetail = detail;
}

bool MainFrame::saveCharacter(CharacterInstance* character)
{
	if (!character) return false;
	if (character->save()) return true;
	const std::string& path = character->getTopHA6Path();
	if (character->frameData.isHA4() && !ha4::LastSaveError().empty()) {
		requestErrorPopup("Save Error", "MBAC .DAT not saved: " + ha4::LastSaveError());
		return false;
	}
	requestErrorPopup("Save Error", path.empty()
		? "Character '" + character->getName() + "' has no HA6 file to save to. Use Save Character As."
		: "Could not write " + path);
	return false;
}

bool MainFrame::saveCharacterAs(CharacterInstance* character, const std::string& path)
{
	if (!character) return false;
	if (character->saveAs(path)) return true;
	requestErrorPopup("Save Error", "Could not write " + path);
	return false;
}

bool MainFrame::saveAllModifiedCharacters()
{
	for (auto& character : characters) {
		if (character->isModified() && !saveCharacter(character.get())) {
			return false;
		}
	}
	return true;
}

void MainFrame::clearProjectState()
{
	m_session.clear();
	m_hosts.clear();
	m_nextHostId = 1;
	m_reverseView = nullptr;
	views.clear();
	characters.clear();
	activeViewIndex = -1;
	pendingCloseViewIndex = -1;

	// Clear the render system
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);
	render.SetParts(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
}

void MainFrame::requestProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed)
{
	m_deferredProjectAction = action;
	m_deferredProjectPath = path;
	m_deferredProjectConfirmed = confirmed;
}

void MainFrame::processDeferredProjectAction()
{
	if (m_deferredProjectAction == ProjectCloseAction::None) {
		return;
	}
	ProjectCloseAction action = m_deferredProjectAction;
	std::string path = std::move(m_deferredProjectPath);
	bool confirmed = m_deferredProjectConfirmed;
	m_deferredProjectAction = ProjectCloseAction::None;
	m_deferredProjectPath.clear();
	m_deferredProjectConfirmed = false;
	runProjectAction(action, path, confirmed);
}

void MainFrame::runProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed)
{
	// Ask about unsaved changes first; the dialog re-queues this action
	// (with the same path) as confirmed.
	if (!confirmed) {
		m_projectCloseAction = action;
		m_pendingProjectPath = path;
		if (!tryCloseProject()) {
			return;
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;
	m_pendingProjectPath.clear();

	switch (action) {
		case ProjectCloseAction::New:
		case ProjectCloseAction::Close:
			clearProjectState();
			break;
		case ProjectCloseAction::Open:
			if (path.empty()) {
				std::string chosen = FileDialog(fileType::HPROJ, false);
				if (!chosen.empty()) {
					loadProjectFromPath(chosen, false);
				}
			} else {
				loadProjectFromPath(path, true);
			}
			break;
		default:
			break;
	}
}

void MainFrame::loadProjectFromPath(const std::string& path, bool isRecent)
{
	int loadedTheme = style_idx;
	float loadedZoom = zoom_idx;
	bool loadedSmooth = smoothRender;
	float loadedColor[3] = { clearColor[0], clearColor[1], clearColor[2] };
	std::vector<std::string> failedCharacters;

	// LoadProject builds the new project off to the side and only replaces
	// characters/views when the file parsed; on failure the current project
	// is left untouched.
	std::vector<std::unique_ptr<CharacterInstance>> newCharacters;
	std::vector<std::unique_ptr<CharacterView>> newViews;
	int newActiveView = -1;
	std::string workspaceJson;
	if (ProjectManager::LoadProject(path, newCharacters, newViews, newActiveView, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor,
	                                &failedCharacters, &workspaceJson))
	{
		// Drop render references into the old project before it is destroyed.
		render.DontDraw();
		render.ClearTexture();
		render.SetCg(nullptr);
		render.SetParts(nullptr);
		pendingCloseViewIndex = -1;

		m_session.clear();
		m_hosts.clear();
		m_reverseView = nullptr;
		views = std::move(newViews);
		characters = std::move(newCharacters);
		activeViewIndex = newActiveView;

		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view, then put tabs back into their windows.
		if (activeViewIndex >= 0 && activeViewIndex < (int)views.size()) {
			setActiveView(activeViewIndex);
		}
		RestoreWorkspace(workspaceJson);

		// Effect loading is now automatic per-character in CharacterInstance::loadFromTxt()

		ProjectManager::SetCurrentProjectPath(path);
		// A partially loaded project must not look clean: saving it would drop
		// the missing characters from the .hproj.
		m_projectModified = !failedCharacters.empty();
		addRecentProject(path);
		updateWindowTitle();

		if (!failedCharacters.empty()) {
			std::string detail = "These characters could not be loaded and were skipped:\n";
			for (const auto& f : failedCharacters) {
				detail += "  " + f + "\n";
			}
			detail += "Saving the project now would remove them from it.";
			requestErrorPopup("Project Load Error", detail);
		}
	} else {
		requestErrorPopup("Project Load Error", path);
		if (isRecent) {
			// Use normalized path comparison to find and remove the entry
			std::string normalizedPath = normalizePath(path);
			auto it = std::find_if(gSettings.recentProjects.begin(), gSettings.recentProjects.end(),
				[&normalizedPath](const std::string& existing) {
					return normalizePath(existing) == normalizedPath;
				});
			if (it != gSettings.recentProjects.end()) {
				gSettings.recentProjects.erase(it);
			}
		}
	}
}

void MainFrame::newProject()
{
	requestProjectAction(ProjectCloseAction::New, std::string(), false);
}

void MainFrame::openProject()
{
	requestProjectAction(ProjectCloseAction::Open, std::string(), false);
}

void MainFrame::saveProject()
{
	if (!ProjectManager::HasCurrentProject()) {
		saveProjectAs();
		return;
	}

	if (ProjectManager::SaveProject(ProjectManager::GetCurrentProjectPath(),
	                                characters, views, activeViewIndex,
	                                style_idx, zoom_idx, smoothRender, clearColor,
	                                SerializeWorkspace()))
	{
		m_projectModified = false;
		updateWindowTitle();
	} else {
		requestErrorPopup("Project Save Error", ProjectManager::GetCurrentProjectPath());
	}
}

void MainFrame::saveProjectAs()
{
	std::string path = FileDialog(fileType::HPROJ, true);
	if (path.empty()) {
		return;
	}

	// Ensure .hproj extension
	if (path.find(".hproj") == std::string::npos) {
		path += ".hproj";
	}

	if (ProjectManager::SaveProject(path, characters, views, activeViewIndex,
	                                style_idx, zoom_idx, smoothRender, clearColor,
	                                SerializeWorkspace()))
	{
		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		requestErrorPopup("Project Save Error", path);
	}
}

void MainFrame::closeProject()
{
	requestProjectAction(ProjectCloseAction::Close, std::string(), false);
}

void MainFrame::updateWindowTitle()
{
	// Build version string with build number and git hash
	std::wstring title = L"gonptéchan v" VERSION_WITH_COMMIT_W;

	if (ProjectManager::HasCurrentProject()) {
		const std::string& projectPath = ProjectManager::GetCurrentProjectPath();

		// Extract filename from path
		size_t lastSlash = projectPath.find_last_of("/\\");
		std::string filename = (lastSlash != std::string::npos)
			? projectPath.substr(lastSlash + 1)
			: projectPath;

		// Convert to wide string
		std::wstring wFilename(filename.begin(), filename.end());

		title = wFilename + L" - " + title;

		if (m_projectModified) {
			title = L"*" + title;
		}
	}

#ifdef _WIN32
	SetWindowTextW(mainWindowHandle, title.c_str());
#else
	// For GLFW, convert to UTF-8
	std::string utf8Title;
	for (wchar_t wc : title) {
		if (wc < 0x80) {
			utf8Title += static_cast<char>(wc);
		} else if (wc < 0x800) {
			utf8Title += static_cast<char>(0xC0 | (wc >> 6));
			utf8Title += static_cast<char>(0x80 | (wc & 0x3F));
		} else {
			utf8Title += static_cast<char>(0xE0 | (wc >> 12));
			utf8Title += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
			utf8Title += static_cast<char>(0x80 | (wc & 0x3F));
		}
	}
	glfwSetWindowTitle(mainWindowHandle, utf8Title.c_str());
#endif
}

bool MainFrame::tryCloseProject()
{
	if (m_projectModified || std::any_of(characters.begin(), characters.end(),
	                                     [](const auto& c) { return c->isModified(); }))
	{
		m_pendingProjectClose = true;
		shouldOpenUnsavedProjectDialog = true;
		return false; // Don't close yet, wait for user response
	}

	return true; // No unsaved changes, ok to close
}

void MainFrame::addRecentProject(const std::string& path)
{
	if (path.empty()) {
		return;
	}

	std::string normalizedPath = normalizePath(path);

	// Remove if already exists (compare normalized paths)
	auto it = std::find_if(gSettings.recentProjects.begin(), gSettings.recentProjects.end(),
		[&normalizedPath](const std::string& existing) {
			return normalizePath(existing) == normalizedPath;
		});
	
	if (it != gSettings.recentProjects.end()) {
		gSettings.recentProjects.erase(it);
	}

	// Add to front (store normalized path for consistency)
	// Use forward slashes as the canonical format for storage
	gSettings.recentProjects.insert(gSettings.recentProjects.begin(), normalizedPath);

	// Keep only last 10
	if (gSettings.recentProjects.size() > 10) {
		gSettings.recentProjects.resize(10);
	}
}

void MainFrame::openRecentProject(const std::string& path)
{
	// Deferred: the caller iterates gSettings.recentProjects, which a failed
	// load modifies. The path is carried through the unsaved-changes prompt.
	requestProjectAction(ProjectCloseAction::Open, path, false);
}


// ---------------------------------------------------------------------------
// Background (stage) load / clear. Each loaded stage becomes its own view
// (tab) so it can be docked / undocked alongside character tabs and live in
// its own viewport. currentBgFile mirrors the active tab's stage file so
// existing menu items and the inspector keep working without knowing about
// the view abstraction.

void MainFrame::loadStageFile(const std::string& path)
{
	auto file = std::make_unique<bg::File>();
	if (!file->Load(path.c_str()))
		return;

	std::string displayName = path;
	auto slash = displayName.find_last_of("/\\");
	if (slash != std::string::npos) displayName = displayName.substr(slash + 1);

	auto view = std::make_unique<CharacterView>(nullptr, &render);
	view->setStageFile(std::move(file), displayName);

	// Default the camera so world (0, 0) lands at u4ick's proportional
	// character-feet anchor: 401/1280 across and 538/720 down of the
	// viewport. That's where his SaveRender output puts character feet,
	// and matches the 'fire at bottom near feet' layout the user sees in
	// his tool. Diagnostics (Screenshot 2026-05) confirmed the underlying
	// math is correct end-to-end — the only choice was where to put the
	// initial anchor, and (0, 0) only matches u4ick's *uninteracted*
	// default (everything off-screen until you drag).
	float clientW = clientRect.x > 0 ? clientRect.x : 1280.0f;
	float clientH = clientRect.y > 0 ? clientRect.y : 720.0f;
	float defaultPanX = clientW * (401.0f / 1280.0f);
	float defaultPanY = clientH * (538.0f / 720.0f);
	view->setStageRenderXY(defaultPanX, defaultPanY);
	view->setStageRenderInit(true);
	bgCamera.SetPan(defaultPanX, defaultPanY);

	views.push_back(std::move(view));
	setActiveView((int)views.size() - 1);
	// The stage list of the game this stage came from (browser, PageUp/Down).
	if (!stageProject.IsOpen() || !stageProject.FindByDat(path))
		stageProject.Open(path, currentBgFile ? currentBgFile->GetGame() : bg::Game::MBAACC);
}

void MainFrame::openStageInActiveTab(const std::string& path)
{
	CharacterView* view = getActiveView();
	if (!view || !view->isStageView()) { loadStageFile(path); return; }
	auto file = std::make_unique<bg::File>();
	if (!file->Load(path.c_str())) return;
	std::string displayName = path;
	auto slash = displayName.find_last_of("/\\");
	if (slash != std::string::npos) displayName = displayName.substr(slash + 1);
	// Same tab, same camera: the view keeps its pan/zoom.
	view->setStageFile(std::move(file), displayName);
	setActiveView(activeViewIndex);
	if (!stageProject.IsOpen() || !stageProject.FindByDat(path))
		stageProject.Open(path, currentBgFile ? currentBgFile->GetGame() : bg::Game::MBAACC);
}

void MainFrame::stepStage(int dir)
{
	if (!currentBgFile) return;
	if (!stageProject.IsOpen()) stageProject.Open(currentBgFile->GetFilename(), currentBgFile->GetGame());
	const bg::StageEntry* cur = stageProject.FindByDat(currentBgFile->GetFilename());
	const bg::StageEntry* next = stageProject.Step(cur ? cur->id : -1, dir);
	if (next && !next->datPath.empty()) openStageInActiveTab(next->datPath);
}

// One timeline for the two stage histories: undo takes the step with the
// newest edit sequence; redo takes the most recently undone one, which is
// the redo top with the lowest sequence.
static int PickStageHistory(bool redo, bool fileCan, uint64_t fileSeq, bool projCan, uint64_t projSeq)
{
	if (!fileCan && !projCan) return 0;
	if (!projCan) return 1;
	if (!fileCan) return 2;
	if (redo) return fileSeq <= projSeq ? 1 : 2;
	return fileSeq >= projSeq ? 1 : 2;
}

bool MainFrame::stageUndoRedo(bool redo, bool apply)
{
	const bool fileCan = currentBgFile && (redo ? currentBgFile->CanRedo() : currentBgFile->CanUndo());
	const bool projCan = stageProject.IsOpen() && (redo ? stageProject.CanRedo() : stageProject.CanUndo());
	const uint64_t fileSeq = currentBgFile ? (redo ? currentBgFile->RedoSeq() : currentBgFile->UndoSeq()) : 0;
	const uint64_t projSeq = redo ? stageProject.RedoSeq() : stageProject.UndoSeq();
	const int pick = PickStageHistory(redo, fileCan, fileSeq, projCan, projSeq);
	if (!pick) return false;
	if (!apply) return true;
	if (pick == 1) return redo ? currentBgFile->Redo() : currentBgFile->Undo();
	return redo ? stageProject.Redo() : stageProject.Undo();
}

std::string MainFrame::stageUndoLabel(bool redo)
{
	const bool fileCan = currentBgFile && (redo ? currentBgFile->CanRedo() : currentBgFile->CanUndo());
	const bool projCan = stageProject.IsOpen() && (redo ? stageProject.CanRedo() : stageProject.CanUndo());
	const uint64_t fileSeq = currentBgFile ? (redo ? currentBgFile->RedoSeq() : currentBgFile->UndoSeq()) : 0;
	const uint64_t projSeq = redo ? stageProject.RedoSeq() : stageProject.UndoSeq();
	const int pick = PickStageHistory(redo, fileCan, fileSeq, projCan, projSeq);
	if (pick == 1) return "stage edit";
	if (pick == 2) return redo ? stageProject.RedoLabel() : stageProject.UndoLabel();
	return std::string();
}

void MainFrame::saveStageAll()
{
	if (currentBgFile) {
		if (currentBgFile->IsDirty() && currentBgFile->Save(currentBgFile->GetFilename().c_str())) currentBgFile->ClearDirty();
		if (currentBgFile->IsInfoDirty()) currentBgFile->SaveInfo();
	}
	if (stageProject.IsOpen() && stageProject.IsDirty()) stageProject.SaveAll();
}

void MainFrame::drawStageBrowser()
{
	if (!m_showStageBrowser) return;
	const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mainPos.x + 520.0f, mainPos.y + 80.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(620.0f, 720.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Stage Browser", &m_showStageBrowser)) { ImGui::End(); return; }
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		shortcuts.claimFocus(ShortcutContext::stageView, getActiveView() ? getActiveView()->getId() : 0);
	bg::BrowserHooks hooks;
	hooks.open = [this](const std::string& p) { openStageInActiveTab(p); };
	hooks.showInGame = stageShowInGame;
	bg::DrawStageBrowser(stageProject, currentBgFile ? currentBgFile->GetFilename() : std::string(), hooks);
	ImGui::End();
}

void MainFrame::clearStage()
{
	// Drop the active stage view (if any) and detach the renderer.
	bgRenderer.SetFile(nullptr);
	bgRenderer.SetEnabled(false);
	currentBgFile = nullptr;

	if (activeViewIndex >= 0 && activeViewIndex < (int)views.size()) {
		auto* view = views[activeViewIndex].get();
		if (view && view->isStageView()) {
			views.erase(views.begin() + activeViewIndex);
			if (views.empty())
				activeViewIndex = -1;
			else if (activeViewIndex >= (int)views.size())
				setActiveView((int)views.size() - 1);
			else
				setActiveView(activeViewIndex);
		}
	}
}
