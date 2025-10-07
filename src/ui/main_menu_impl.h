#ifndef UI_MAIN_MENU_IMPL_H_GUARD
#define UI_MAIN_MENU_IMPL_H_GUARD

// ============================================================================
// Main Menu Implementation
// ============================================================================
// Contains the MainFrame::Menu() member function implementation
//
// This file is included at the end of main_frame.cpp to keep the menu system
// separate from the main file for better organization.
//
// Menu Contents:
// - File menu (New, Open, Recent, Save, Close project)
// - Edit menu (Undo, Redo, Cut, Copy, Paste)
// - View menu (Zoom, Theme, Grid, Clear color)
// - Tools menu (Rebuild palettes, Settings)
// - Help menu (About, Documentation)
// ============================================================================

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
							tryLoadEffectCharacter(characters.back().get());
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
							tryLoadEffectCharacter(characters.back().get());
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
							tryLoadEffectCharacter(characters.back().get());
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

			// Effect.ha6 status and management
			if (effectCharacter) {
				// Show status when loaded
				ImGui::TextDisabled("Effect.ha6: Loaded");
				if (ImGui::IsItemHovered()) {
					std::string folder = effectCharacter->getBaseFolder();
					int patternCount = effectCharacter->frameData.get_sequence_count();
					int imageCount = effectCharacter->cg.get_image_count();

					// Get first image filename if available
					const char* cgFileName = (imageCount > 0) ? effectCharacter->cg.get_filename(0) : nullptr;
					std::string cgFile = cgFileName ? cgFileName : "None";

					ImGui::BeginTooltip();
					ImGui::Text("Effect.ha6 Details:");
					ImGui::Separator();
					ImGui::Text("Folder: %s", folder.c_str());
					ImGui::Text("CG File: %s", cgFile.c_str());
					ImGui::Text("Images: %d", imageCount);
					ImGui::Text("Pattern Count: %d", patternCount);
					ImGui::EndTooltip();
				}

				if (ImGui::MenuItem("Unload Effect.ha6")) {
					printf("[Effect] Manually unloading effect.ha6\n");
					effectCharacter.reset();
				}
			} else {
				// Show status when not loaded
				ImGui::TextDisabled("Effect.ha6: Not loaded");

				if (ImGui::MenuItem("Load Effect.ha6... (select effect.txt)")) {
					std::string effectTxtPath = FileDialog(fileType::TXT, false);
					if (!effectTxtPath.empty()) {
						// Extract folder from effect.txt path
						size_t lastSlash = effectTxtPath.find_last_of("/\\");
						std::string folder = (lastSlash != std::string::npos)
							? effectTxtPath.substr(0, lastSlash)
							: effectTxtPath;

						printf("[Effect] Manually loading effect.ha6 from: %s\n", folder.c_str());
						bool loaded = loadEffectCharacter(folder);
						if (loaded) {
							printf("[Effect] ✓ Successfully loaded\n");
						} else {
							printf("[Effect] ✗ Failed to load from: %s\n", folder.c_str());
							ImGui::OpenPopup(errorPopupId);
						}
					}
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
		if (ImGui::BeginMenu("Stage"))
		{
			if (ImGui::MenuItem("Load Stage File..."))
			{
				std::string path = FileDialog(fileType::DAT, false);
				if (!path.empty()) {
					loadStageFile(path);
				}
			}

			bool bgEnabled = bgRenderer.IsEnabled();
			if (ImGui::MenuItem("Clear Stage", nullptr, false, currentBgFile != nullptr))
			{
				clearStage();
			}

			ImGui::Separator();

			if (ImGui::Checkbox("Enable Stage Rendering", &bgEnabled))
			{
				bgRenderer.SetEnabled(bgEnabled);
			}

			bool showDebug = bgRenderer.IsShowingDebugOverlay();
			if (ImGui::Checkbox("Show Debug Overlay", &showDebug))
			{
				bgRenderer.SetShowDebugOverlay(showDebug);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Shows bounding boxes, camera position, and screen center");
			}

			bool parallaxEnabled = bgRenderer.IsParallaxEnabled();
			if (ImGui::Checkbox("Enable Parallax", &parallaxEnabled))
			{
				bgRenderer.SetParallaxEnabled(parallaxEnabled);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Disable to see objects at world positions without parallax effect");
			}

			if (currentBgFile && currentBgFile->IsLoaded())
			{
				ImGui::Separator();
				ImGui::TextDisabled("Loaded: %zu objects", currentBgFile->GetObjects().size());
				ImGui::TextDisabled("Camera: (%.0f, %.0f)", bgCamera.x, bgCamera.y);
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

#endif /* UI_MAIN_MENU_IMPL_H_GUARD */
