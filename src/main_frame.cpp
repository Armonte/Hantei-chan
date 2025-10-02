#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"
#include "imgui_utils.h"
#include "project_manager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

MainFrame::MainFrame(ContextGl *context_):
context(context_)
{
	LoadSettings();
}

MainFrame::~MainFrame()
{
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

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);

	auto* active = getActiveCharacter();
	if (active) {
		render.x = (active->renderX + clientRect.x/2) / render.scale;
		render.y = (active->renderY + clientRect.y/2) / render.scale;
	}
	render.Draw();
}

void MainFrame::DrawUi()
{
	ImGuiID errorPopupId = ImGui::GetID("Loading Error");
	

	//Fullscreen docker to provide the layout for the panes
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Dock Window", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBackground 
	);
		ImGui::PopStyleVar(3);
		Menu(errorPopupId);

		// View tabs
		if (ImGui::BeginTabBar("##character_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
			for (size_t i = 0; i < views.size(); i++) {
				bool open = true;
				auto* view = views[i].get();
				auto* character = view->getCharacter();
				ImGuiTabItemFlags flags = (character && character->isModified()) ? ImGuiTabItemFlags_UnsavedDocument : 0;

				// Skip rendering if we're waiting for user to confirm closing this view
				if (pendingCloseViewIndex == (int)i) {
					// Keep the tab visible while waiting for dialog response
					if (ImGui::BeginTabItem(view->getDisplayName().c_str(), nullptr, flags)) {
						if (activeViewIndex != (int)i) {
							setActiveView(i);
						}
						ImGui::EndTabItem();
					}
					continue;
				}

				if (ImGui::BeginTabItem(view->getDisplayName().c_str(), &open, flags)) {
					if (activeViewIndex != (int)i) {
						setActiveView(i);
					}
					ImGui::EndTabItem();
				}

				// Right-click context menu - check AFTER EndTabItem
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
					contextMenuViewIndex = i;
					ImGui::OpenPopup("ViewContextMenu");
				}

				// If user clicked X button, use tryCloseView to handle unsaved changes
				if (!open) {
					tryCloseView(i);
				}
			}

			// Right-click context menu popup
			if (ImGui::BeginPopup("ViewContextMenu")) {
				if (contextMenuViewIndex >= 0 && contextMenuViewIndex < views.size()) {
					if (ImGui::MenuItem("New View of Character")) {
						auto* character = views[contextMenuViewIndex]->getCharacter();
						createViewForCharacter(character);
					}
				}
				ImGui::EndPopup();
			}

			// "+" button to add new character with dropdown menu
			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
				ImGui::OpenPopup("AddCharacterPopup");
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Add Character");
			}

			if (ImGui::BeginPopup("AddCharacterPopup")) {
				if (ImGui::MenuItem("Load from .txt...")) {
					std::string path = FileDialog(fileType::TXT, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup("DuplicateFileError");
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadFromTxt(path)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							}
						}
					}
				}
				if (ImGui::MenuItem("Load HA6...")) {
					std::string path = FileDialog(fileType::HA6, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup("DuplicateFileError");
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadHA6(path, false)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							}
						}
					}
				}
				if (ImGui::MenuItem("Load HA6 and Patch...")) {
					std::string path = FileDialog(fileType::HA6, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup("DuplicateFileError");
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadHA6(path, true)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							}
						}
					}
				}
				if (ImGui::MenuItem("New Character")) {
					auto character = std::make_unique<CharacterInstance>();
					character->frameData.initEmpty();
					character->setName("Untitled");
					characters.push_back(std::move(character));
					createViewForCharacter(characters.back().get());
					markProjectModified();
				}
				ImGui::EndPopup();
			}

			// Duplicate file error popup
			if (ImGui::BeginPopupModal("DuplicateFileError", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Character already loaded!");
				ImGui::Text("Use right-click on the tab and select 'New View of Character'");
				ImGui::Text("to open multiple views of the same character.");
				ImGui::Separator();
				if (ImGui::Button("OK", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			ImGui::EndTabBar();
		}

		ImGuiID dockspaceID = ImGui::GetID("Dock Space");
		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, clientRect); 

			ImGuiID toSplit = dockspaceID;
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Left, 0.30f, nullptr, &toSplit);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Right, 0.45f, nullptr, &toSplit);
			ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Down, 0.20f, nullptr, &toSplit);

			ImGui::DockBuilderDockWindow("Left Pane", dock_left_id);
			ImGui::DockBuilderDockWindow("Right Pane", dock_right_id);
 			ImGui::DockBuilderDockWindow("Box Pane", dock_down_id);

			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_NoDockingInCentralNode |
			ImGuiDockNodeFlags_AutoHideTabBar 
			//ImGuiDockNodeFlags_NoSplit
		); 
	ImGui::End();

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Loading Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("There was a problem loading the file.\n"
			"The file couldn't be accessed or it's not a valid file.\n\n");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	// Unsaved changes dialog - open it if requested
	if (shouldOpenUnsavedDialog) {
		ImGui::OpenPopup("Unsaved Changes");
		shouldOpenUnsavedDialog = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Unsaved Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (pendingCloseViewIndex >= 0 && pendingCloseViewIndex < views.size()) {
			auto* view = views[pendingCloseViewIndex].get();
			auto* character = view->getCharacter();
			if (character) {
				ImGui::Text("Character '%s' has unsaved changes.", character->getName().c_str());
				ImGui::Text("Do you want to save before closing?\n\n");
				ImGui::Separator();

				if (ImGui::Button("Save", ImVec2(120, 0))) {
					if (character->save()) {
						closeView(pendingCloseViewIndex);
						pendingCloseViewIndex = -1;
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
					closeView(pendingCloseViewIndex);
					pendingCloseViewIndex = -1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					pendingCloseViewIndex = -1;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndPopup();
	}

	// Project load error dialog
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Project Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Failed to load project file.\n"
			"The file may be corrupted or some character files may be missing.\n\n");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	// Project save error dialog
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Project Save Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Failed to save project file.\n"
			"Check that you have write permissions for the selected location.\n\n");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	// Unsaved project dialog - open it if requested
	if (shouldOpenUnsavedProjectDialog) {
		ImGui::OpenPopup("Unsaved Project");
		shouldOpenUnsavedProjectDialog = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Unsaved Project", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The current project has unsaved changes.\n");
		ImGui::Text("Do you want to save before closing?\n\n");
		ImGui::Separator();

		if (ImGui::Button("Save", ImVec2(120, 0))) {
			saveProject();
			if (!m_projectModified) { // Only proceed if save succeeded
				m_pendingProjectClose = false;
				ImGui::CloseCurrentPopup();

				// Execute the pending action
				switch (m_projectCloseAction) {
					case ProjectCloseAction::New:
						// Clear and prepare for new project
						views.clear();
						characters.clear();
						activeViewIndex = -1;
						render.DontDraw();
						render.ClearTexture();
						render.SetCg(nullptr);
						ProjectManager::ClearCurrentProjectPath();
						m_projectModified = false;
						updateWindowTitle();
						break;
					case ProjectCloseAction::Open:
						// Will trigger file dialog in next frame
						openProject();
						break;
					case ProjectCloseAction::Close:
						closeProject();
						break;
					default:
						break;
				}
				m_projectCloseAction = ProjectCloseAction::None;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
			m_pendingProjectClose = false;
			ImGui::CloseCurrentPopup();

			// Execute the pending action without saving
			switch (m_projectCloseAction) {
				case ProjectCloseAction::New:
					views.clear();
					characters.clear();
					activeViewIndex = -1;
					render.DontDraw();
					render.ClearTexture();
					render.SetCg(nullptr);
					ProjectManager::ClearCurrentProjectPath();
					m_projectModified = false;
					updateWindowTitle();
					break;
				case ProjectCloseAction::Open:
					openProject();
					break;
				case ProjectCloseAction::Close:
					closeProject();
					break;
				default:
					break;
			}
			m_projectCloseAction = ProjectCloseAction::None;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			m_pendingProjectClose = false;
			m_projectCloseAction = ProjectCloseAction::None;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Only draw panes if we have an active view
	auto* view = getActiveView();
	if (view) {
		if (view->getMainPane()) view->getMainPane()->Draw();
		if (view->getRightPane()) view->getRightPane()->Draw();
		if (view->getBoxPane()) view->getBoxPane()->Draw();
	}
	aboutWindow.Draw();
	vectors.Draw();

	RenderUpdate();
}

void MainFrame::RenderUpdate()
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) {
		render.DontDraw();
		return;
	}

	auto& state = view->getState();
	Sequence *seq;
	if((seq = active->frameData.get_sequence(state.pattern)) &&
		seq->frames.size() > 0)
	{
		auto &frame =  seq->frames[state.frame];

		// Render currently selected layer
		if(!frame.AF.layers.empty()) {
			// Ensure selectedLayer is valid
			if(state.selectedLayer >= frame.AF.layers.size()) {
				state.selectedLayer = frame.AF.layers.size() - 1;
			}
			if(state.selectedLayer < 0) {
				state.selectedLayer = 0;
			}

			auto &layer = frame.AF.layers[state.selectedLayer];
			state.spriteId = layer.spriteId;
			render.GenerateHitboxVertices(frame.hitboxes);
			render.offsetX = (layer.offset_x)*1;
			render.offsetY = (layer.offset_y)*1;
			render.SetImageColor(layer.rgba);
			render.rotX = layer.rotation[0];
			render.rotY = layer.rotation[1];
			render.rotZ = layer.rotation[2];
			render.scaleX = layer.scale[0];
			render.scaleY = layer.scale[1];

			switch (layer.blend_mode)
			{
			case 2:
				render.blendingMode = Render::additive;
				break;
			case 3:
				render.blendingMode = Render::subtractive;
				break;
			default:
				render.blendingMode = Render::normal;
				break;
			}
		}
		else {
			state.spriteId = -1;
			render.DontDraw();
		}
	}
	else
	{
		state.spriteId = -1;
		render.DontDraw();
	}
	render.SwitchImage(state.spriteId);
}

void MainFrame::AdvancePattern(int dir)
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) return;

	auto& state = view->getState();
	state.pattern += dir;
	if(state.pattern < 0)
		state.pattern = 0;
	else if(state.pattern >= active->frameData.get_sequence_count())
		state.pattern = active->frameData.get_sequence_count()-1;
	state.frame = 0;
}

void MainFrame::AdvanceFrame(int dir)
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) return;

	auto& state = view->getState();
	auto seq = active->frameData.get_sequence(state.pattern);
	state.frame += dir;
	if(state.frame < 0)
		state.frame = 0;
	else if(seq && state.frame >= seq->frames.size())
		state.frame = seq->frames.size()-1;
}

void MainFrame::UpdateBackProj(float x, float y)
{
	render.UpdateProj(x, y);
	glViewport(0, 0, x, y);
}

void MainFrame::HandleMouseDrag(int x_, int y_, bool dragRight, bool dragLeft)
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) return;

	if(dragRight)
	{
		if (view->getBoxPane()) view->getBoxPane()->BoxDrag(x_, y_);
	}
	else if(dragLeft)
	{
		active->renderX += x_;
		active->renderY += y_;
	}
}

void MainFrame::RightClick(int x_, int y_)
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) return;

	auto* boxPane = view->getBoxPane();
	if (!boxPane) return;

	boxPane->BoxStart((x_ - active->renderX - clientRect.x/2)/render.scale,
	                  (y_ - active->renderY - clientRect.y/2)/render.scale);
}

bool MainFrame::HandleKeys(uint64_t vkey)
{
	bool ctrlPressed = GetKeyState(VK_CONTROL) & 0x8000;

	// Multi-view shortcuts (work even without active view)
	if (ctrlPressed) {
		switch (vkey) {
		case VK_TAB:
			// Ctrl+Tab: switch to next view
			if (!views.empty()) {
				int nextIndex = (activeViewIndex + 1) % views.size();
				setActiveView(nextIndex);
				return true;
			}
			break;
		case 'W':
			// Ctrl+W: close active view
			if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
				tryCloseView(activeViewIndex);
				return true;
			}
			break;
		case 'S':
			// Ctrl+Shift+S: save project as
			if (GetKeyState(VK_SHIFT) & 0x8000) {
				saveProjectAs();
				return true;
			}
			// Ctrl+S: save project if we have one, otherwise save active character
			else if (ProjectManager::HasCurrentProject()) {
				saveProject();
				return true;
			}
			else if (auto* active = getActiveCharacter()) {
				active->save();
				return true;
			}
			break;
		case 'O':
			// Ctrl+O: open project
			openProject();
			return true;
		case 'N':
			// Ctrl+N: new project
			newProject();
			return true;
		}
	}

	// View-specific shortcuts (require active view)
	auto* view = getActiveView();
	if (!view) return false;

	switch (vkey)
	{
	case VK_UP:
		AdvancePattern(-1);
		return true;
	case VK_DOWN:
		AdvancePattern(1);
		return true;
	case VK_LEFT:
		AdvanceFrame(-1);
		return true;
	case VK_RIGHT:
		AdvanceFrame(+1);
		return true;
	case 'Z':
		if (view->getBoxPane()) view->getBoxPane()->AdvanceBox(-1);
		return true;
	case 'X':
		if (view->getBoxPane()) view->getBoxPane()->AdvanceBox(+1);
		return true;
	}
	return false;
}

void MainFrame::ChangeClearColor(float r, float g, float b)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
}

void MainFrame::SetZoom(float level)
{
	render.scale = level;
	zoom_idx = level;
}

void MainFrame::LoadTheme(int i )
{
	style_idx = i;
	switch (i)
	{
		case 0: WarmStyle(); break;
		case 1: ImGui::StyleColorsDark(); ChangeClearColor(0.202f, 0.243f, 0.293f); break;
		case 2: ImGui::StyleColorsLight(); ChangeClearColor(0.534f, 0.568f, 0.587f); break;
		case 3: ImGui::StyleColorsClassic(); ChangeClearColor(0.142f, 0.075f, 0.147f); break;
	}
}

void MainFrame::Menu(unsigned int errorPopupId)
{
	if (ImGui::BeginMenuBar())
	{
		//ImGui::Separator();
		if (ImGui::BeginMenu("File"))
		{
			auto* active = getActiveCharacter();
			bool hasActive = (active != nullptr);

			// Project menu items
			if (ImGui::MenuItem("New Project"))
			{
				newProject();
			}

			if (ImGui::MenuItem("Open Project..."))
			{
				openProject();
			}

			// Recent projects submenu
			if (ImGui::BeginMenu("Recent Projects", !gSettings.recentProjects.empty()))
			{
				for (const auto& recentPath : gSettings.recentProjects) {
					// Extract filename for display
					size_t lastSlash = recentPath.find_last_of("/\\");
					std::string filename = (lastSlash != std::string::npos)
						? recentPath.substr(lastSlash + 1)
						: recentPath;

					if (ImGui::MenuItem(filename.c_str())) {
						openRecentProject(recentPath);
					}

					// Show full path as tooltip (convert backslashes to forward slashes for display)
					if (ImGui::IsItemHovered()) {
						std::string displayPath = recentPath;
						std::replace(displayPath.begin(), displayPath.end(), '\\', '/');
						ImGui::SetTooltip("%s", displayPath.c_str());
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Clear Recent Projects")) {
					gSettings.recentProjects.clear();
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			bool hasProject = ProjectManager::HasCurrentProject();
			if (ImGui::MenuItem("Save Project", nullptr, false, hasProject))
			{
				saveProject();
			}

			if (ImGui::MenuItem("Save Project As..."))
			{
				saveProjectAs();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Close Project", nullptr, false, hasProject))
			{
				closeProject();
			}

			ImGui::Separator();

			// Character menu items
			if (ImGui::MenuItem("New Character"))
			{
				auto character = std::make_unique<CharacterInstance>();
				character->frameData.initEmpty();
				character->setName("Untitled");
				characters.push_back(std::move(character));
				createViewForCharacter(characters.back().get());
				markProjectModified();
			}

			if (ImGui::MenuItem("Close Character", nullptr, false, hasActive))
			{
				if (hasActive) {
					tryCloseView(activeViewIndex);
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Load from .txt..."))
			{
				std::string path = FileDialog(fileType::TXT, false);
				if (!path.empty()) {
					// Check for duplicate
					if (findCharacterByPath(path)) {
						ImGui::OpenPopup(errorPopupId);
					} else {
						auto character = std::make_unique<CharacterInstance>();
						if (character->loadFromTxt(path)) {
							characters.push_back(std::move(character));
							createViewForCharacter(characters.back().get());
							markProjectModified();
						} else {
							ImGui::OpenPopup(errorPopupId);
						}
					}
				}
			}

			if (ImGui::MenuItem("Load HA6..."))
			{
				std::string path = FileDialog(fileType::HA6, false);
				if (!path.empty()) {
					// Check for duplicate
					if (findCharacterByPath(path)) {
						ImGui::OpenPopup(errorPopupId);
					} else {
						auto character = std::make_unique<CharacterInstance>();
						if (character->loadHA6(path, false)) {
							characters.push_back(std::move(character));
							createViewForCharacter(characters.back().get());
							markProjectModified();
						} else {
							ImGui::OpenPopup(errorPopupId);
						}
					}
				}
			}

			if (ImGui::MenuItem("Load HA6 and Patch..."))
			{
				std::string path = FileDialog(fileType::HA6, false);
				if (!path.empty()) {
					// Check for duplicate
					if (findCharacterByPath(path)) {
						ImGui::OpenPopup(errorPopupId);
					} else {
						auto character = std::make_unique<CharacterInstance>();
						if (character->loadHA6(path, true)) {
							characters.push_back(std::move(character));
							createViewForCharacter(characters.back().get());
							markProjectModified();
						} else {
							ImGui::OpenPopup(errorPopupId);
						}
					}
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save Character", nullptr, false, hasActive))
			{
				if (hasActive) {
					active->save();
				}
			}

			if (ImGui::MenuItem("Save Character As...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::HA6, true);
					if(!file.empty())
					{
						active->saveAs(file);
					}
				}
			}

			if (ImGui::MenuItem("Save as MOD...", nullptr, false, hasActive && !active->getTxtPath().empty()))
			{
				if (hasActive) {
					// Generate MOD filename from top HA6 path
					const auto& topHA6 = active->getTopHA6Path();
					if(!topHA6.empty())
					{
						size_t dotPos = topHA6.find_last_of(".");
						std::string basePath = (dotPos != std::string::npos) ? topHA6.substr(0, dotPos) : topHA6;
						std::string modPath = basePath + "_MOD.HA6";

						active->saveModifiedOnly(modPath);
					}
				}
			}

			if (ImGui::MenuItem("Load Commands (_c.txt)...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::TXT);
					if(!file.empty())
					{
						active->frameData.load_commands(file.c_str());
					}
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Load CG...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::CG);
					if(!file.empty())
					{
						if(!active->loadCG(file))
						{
							ImGui::OpenPopup(errorPopupId);
						}
						render.SwitchImage(-1);
					}
				}
			}

			if (ImGui::MenuItem("Load palette...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::PAL);
					if(!file.empty())
					{
						if(!active->cg.loadPalette(file.c_str()))
						{
							ImGui::OpenPopup(errorPopupId);
						}
						render.SwitchImage(-1);
					}
				}
			}

			if (ImGui::MenuItem("Load vector.txt..."))
			{
				std::string&& file = FileDialog(fileType::VECTOR);
				if (!file.empty())
				{
					if (!vectors.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					render.SwitchImage(-1);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Preferences"))
		{
			if (ImGui::BeginMenu("Switch preset style"))
			{		
				if (ImGui::Combo("Style", &style_idx, "Warm\0Dark\0Light\0ImGui\0"))
				{
					LoadTheme(style_idx);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Background color"))
			{
				ImGui::ColorEdit3("##clearColor", (float*)&clearColor, ImGuiColorEditFlags_NoInputs);
				ImGui::EndMenu();
			}
			auto* active = getActiveCharacter();
			if (active && active->cg.getPalNumber() > 0 && ImGui::BeginMenu("Palette number"))
			{
				ImGui::SetNextItemWidth(80);
				ImGui::InputInt("Palette", &active->palette);
				if(active->palette >= active->cg.getPalNumber())
					active->palette = active->cg.getPalNumber()-1;
				else if(active->palette < 0)
					active->palette = 0;
				if(active->cg.changePaletteNumber(active->palette))
					render.SwitchImage(-1);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Zoom level"))
			{
				ImGui::SetNextItemWidth(80);
				if (ImGui::SliderFloat("Zoom", &zoom_idx, 0.25f, 20.0f, "%.2f"))
				{
					SetZoom(zoom_idx);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					Tooltip("Ctrl + Click to set exact value");
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Filter"))
			{
				if (ImGui::Checkbox("Bilinear", &smoothRender))
				{
					render.filter = smoothRender;
					render.SwitchImage(-1);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows"))
		{
			if (ImGui::MenuItem("Vectors Guide")) vectors.drawWindow = !vectors.drawWindow;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About")) aboutWindow.isVisible = !aboutWindow.isVisible;
			ImGui::EndMenu();
		}

		// Status display - show active character info
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto* active = getActiveCharacter();
		if (active) {
			const auto& txtPath = active->getTxtPath();
			const auto& topHA6 = active->getTopHA6Path();

			if (!txtPath.empty() && !topHA6.empty())
			{
				// Extract just the filename from the paths for cleaner display
				size_t txtSlash = txtPath.find_last_of("/\\");
				size_t ha6Slash = topHA6.find_last_of("/\\");
				std::string txtName = (txtSlash != std::string::npos) ? txtPath.substr(txtSlash + 1) : txtPath;
				std::string ha6Name = (ha6Slash != std::string::npos) ? topHA6.substr(ha6Slash + 1) : topHA6;

				ImGui::TextDisabled("Loaded: %s", txtName.c_str());
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "-> Saves to: %s", ha6Name.c_str());
			}
			else if (!topHA6.empty())
			{
				size_t slash = topHA6.find_last_of("/\\");
				std::string filename = (slash != std::string::npos) ? topHA6.substr(slash + 1) : topHA6;
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Save to: %s", filename.c_str());
			}
			else
			{
				ImGui::TextDisabled("No save target set for %s", active->getName().c_str());
			}
		}
		else
		{
			ImGui::TextDisabled("No character loaded");
		}

		ImGui::EndMenuBar();
	}
}

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
		activeViewIndex = index;

		// Refresh panes for this view
		auto* view = getActiveView();
		if (view) {
			view->refreshPanes(&render);

			// Update render state to use this view's character's CG
			auto* character = view->getCharacter();
			if (character) {
				render.SetCg(&character->cg);
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

	// Count existing views for this character to determine view number
	int viewNumber = countViewsForCharacter(character);

	auto view = std::make_unique<CharacterView>(character, &render);
	view->setViewNumber(viewNumber);
	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
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

		// Update active index
		if (views.empty()) {
			activeViewIndex = -1;

			// Clear the render system
			render.DontDraw();
			render.ClearTexture();
			render.SetCg(nullptr);
		} else {
			// Select previous view, or first if we closed the first one
			if (activeViewIndex >= views.size()) {
				activeViewIndex = views.size() - 1;
			}
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

void MainFrame::newProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::New) {
		m_projectCloseAction = ProjectCloseAction::New;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	// Clear all views and characters
	views.clear();
	characters.clear();
	activeViewIndex = -1;
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
}

void MainFrame::openProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::Open) {
		m_projectCloseAction = ProjectCloseAction::Open;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	std::string path = FileDialog(fileType::HPROJ, false);
	if (path.empty()) {
		return;
	}

	int loadedTheme;
	float loadedZoom;
	bool loadedSmooth;
	float loadedColor[3];

	if (ProjectManager::LoadProject(path, characters, views, activeViewIndex, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor))
	{
		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view
		if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
			setActiveView(activeViewIndex);
		}

		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		// Show error popup
		ImGui::OpenPopup("Project Load Error");
	}
}

void MainFrame::saveProject()
{
	if (!ProjectManager::HasCurrentProject()) {
		saveProjectAs();
		return;
	}

	if (ProjectManager::SaveProject(ProjectManager::GetCurrentProjectPath(),
	                                characters, views, activeViewIndex,
	                                style_idx, zoom_idx, smoothRender, clearColor))
	{
		m_projectModified = false;
		updateWindowTitle();
	} else {
		ImGui::OpenPopup("Project Save Error");
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
	                                style_idx, zoom_idx, smoothRender, clearColor))
	{
		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		ImGui::OpenPopup("Project Save Error");
	}
}

void MainFrame::closeProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::Close) {
		m_projectCloseAction = ProjectCloseAction::Close;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	// Clear all views and characters
	views.clear();
	characters.clear();
	activeViewIndex = -1;

	// Clear the render system
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
}

void MainFrame::updateWindowTitle()
{
	std::wstring title = L"gonptéchan v" HA6GUIVERSION;

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
	// Remove if already exists
	auto it = std::find(gSettings.recentProjects.begin(), gSettings.recentProjects.end(), path);
	if (it != gSettings.recentProjects.end()) {
		gSettings.recentProjects.erase(it);
	}

	// Add to front
	gSettings.recentProjects.insert(gSettings.recentProjects.begin(), path);

	// Keep only last 10
	if (gSettings.recentProjects.size() > 10) {
		gSettings.recentProjects.resize(10);
	}
}

void MainFrame::openRecentProject(const std::string& path)
{
	// Check for unsaved changes
	m_projectCloseAction = ProjectCloseAction::Open;
	if (!tryCloseProject()) {
		return; // Dialog will handle the action
	}
	m_projectCloseAction = ProjectCloseAction::None;

	int loadedTheme;
	float loadedZoom;
	bool loadedSmooth;
	float loadedColor[3];

	if (ProjectManager::LoadProject(path, characters, views, activeViewIndex, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor))
	{
		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view
		if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
			setActiveView(activeViewIndex);
		}

		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		// Show error popup and remove from recent
		ImGui::OpenPopup("Project Load Error");
		auto it = std::find(gSettings.recentProjects.begin(), gSettings.recentProjects.end(), path);
		if (it != gSettings.recentProjects.end()) {
			gSettings.recentProjects.erase(it);
		}
	}
}

