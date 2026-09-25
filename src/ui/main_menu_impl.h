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
			if (ImGui::MenuItem("New Project", shortcuts.registry().label(ShortcutAction::newProject).c_str()))
			{
				newProject();
			}

			if (ImGui::MenuItem("Open Project...", shortcuts.registry().label(ShortcutAction::openProject).c_str()))
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
			if (ImGui::MenuItem("Save Project", hasProject ? shortcuts.registry().label(ShortcutAction::save).c_str() : nullptr, false, hasProject))
			{
				saveProject();
			}

			if (ImGui::MenuItem("Save Project As...", shortcuts.registry().label(ShortcutAction::saveProjectAs).c_str()))
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

			if (ImGui::MenuItem("Reopen Closed Tab", shortcuts.registry().label(ShortcutAction::reopenClosedView).c_str(), false, !m_closedTabs.empty()))
				reopenClosedTab();

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

			if (ImGui::MenuItem("Load chr HA6 from .txt..."))
			{
				std::string path = FileDialog(fileType::TXT, false);
				if (!path.empty()) {
					// Check for duplicate
					if (findCharacterByPath(path)) {
						ImGui::OpenPopup(errorPopupId);
					} else {
						auto character = std::make_unique<CharacterInstance>();
						if (character->loadChrHA6FromTxt(path)) {
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

			// MBAC (Act Cadenza) Hantei4 .DAT; detected by content (see framedata_ha4.h)
			if (ImGui::MenuItem("Load MBAC .DAT (Act Cadenza)..."))
			{
				std::string path = FileDialog(fileType::HA4, false);
				if (!path.empty()) {
					if (findCharacterByPath(path)) {
						ImGui::OpenPopup(errorPopupId);
					} else {
						auto character = std::make_unique<CharacterInstance>();
						if (character->loadHA6(path, false)) {
							characters.push_back(std::move(character));
							createViewForCharacter(characters.back().get());
							markProjectModified();
						} else {
							requestErrorPopup("Load Error", "Not a Hantei4 (MBAC) or HA6 file:\n" + path);
						}
					}
				}
			}

			if (ImGui::MenuItem("Export MBAC as HA6...", nullptr, false, hasActive && active->frameData.isHA4()))
			{
				std::string &&file = FileDialog(fileType::HA6, true);
				if (!file.empty()) {
					std::string report;
					bool ok = ha4ui::ExportAsHA6(active, file, report);
					requestErrorPopup(ok ? "MBAC Export" : "Export Error", report);
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

			// Ctrl+S saves the active character (and the .hproj when a project is open).
			if (ImGui::MenuItem("Save Character", shortcuts.registry().label(ShortcutAction::save).c_str(), false, hasActive))
			{
				if (hasActive) {
					saveCharacter(active);
				}
			}

			if (ImGui::MenuItem("Save Character As...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::HA6, true);
					if(!file.empty())
					{
						saveCharacterAs(active, file);
					}
				}
			}

			if (ImGui::MenuItem("Save Merged Stack As...", nullptr, false, hasActive && active->frameData.ownFile() >= 0))
			{
				// Flatten every file of the .txt stack into one HA6 (what Save
				// used to write into the target file before issue #71).
				std::string &&file = FileDialog(fileType::HA6, true);
				if (!file.empty() && !active->frameData.save_merged(file.c_str()))
					requestErrorPopup("Save Error", "Could not write " + file);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Write the merged view of all files in the .txt stack to one HA6.\nSave Character writes only the target file's own patterns plus your edits.");

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

						if (!active->saveModifiedOnly(modPath)) {
							requestErrorPopup("Save Error", "Could not write " + modPath);
						}
					}
				}
			}

			if (ImGui::MenuItem("Load Commands (_c.txt)...", nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::TXT);
					if(!file.empty())
					{
						loadCommandsForActive(file);
					}
				}
			}
			if (ImGui::MenuItem("Command File Editor (active character)", nullptr, false, hasActive))
				openCommandEditorForActive();
			if (ImGui::MenuItem("Open Command File..."))
			{
				std::string &&file = FileDialog(fileType::CMDTXT);
				if (!file.empty()) openCommandEditor(file);
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

			if (ImGui::MenuItem("Load Parts (.pat)..."))
			{
				std::string &&file = FileDialog(fileType::PAT);
				if(!file.empty())
				{
					if (hasActive) {
						// Load PAT into existing character
						if(!active->loadPAT(file))
						{
							ImGui::OpenPopup(errorPopupId);
						}
						else
						{
							render.SetParts(&active->parts);
							// Hide Right Pane (attack params) when loading PAT - it's not relevant for PAT editing
							auto* view = getActiveView();
							if (view && view->getRightPane()) {
								view->getRightPane()->isVisible = false;
							}
						}
					}
					else {
						// No character loaded - create standalone PAT editor
						createPatEditorView(file);
					}
				}
			}

			if (ImGui::MenuItem("Save Parts (.pat)", nullptr, false, hasActive && active->parts.loaded))
			{
				if (hasActive) {
					if(!active->savePAT())
					{
						ImGui::OpenPopup(errorPopupId);
					}
				}
			}

			if (ImGui::MenuItem("Save Parts As...", nullptr, false, hasActive && active->parts.loaded))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::PAT, true);
					if(!file.empty())
					{
						if(!active->savePATAs(file))
						{
							ImGui::OpenPopup(errorPopupId);
						}
					}
				}
			}

			ImGui::Separator();

			// Effect.ha6 status (per-character, auto-loaded from character folder)
			if (active && active->effectCharacter) {
				// Show status when loaded
				ImGui::TextDisabled("Effect.ha6: Loaded for %s", active->getName().c_str());
				if (ImGui::IsItemHovered()) {
					std::string folder = active->effectCharacter->getBaseFolder();
					int patternCount = active->effectCharacter->frameData.get_sequence_count();
					int imageCount = active->effectCharacter->cg.get_image_count();

					// Get first image filename if available
					const char* cgFileName = (imageCount > 0) ? active->effectCharacter->cg.get_filename(0) : nullptr;
					std::string cgFile = cgFileName ? cgFileName : "None";

					ImGui::BeginTooltip();
					ImGui::Text("Effect.ha6 Details for %s:", active->getName().c_str());
					ImGui::Separator();
					ImGui::Text("Folder: %s", folder.c_str());
					ImGui::Text("CG File: %s", cgFile.c_str());
					ImGui::Text("Images: %d", imageCount);
					ImGui::Text("Pattern Count: %d", patternCount);
					ImGui::EndTooltip();
				}

				if (ImGui::MenuItem("Reload Effect.ha6")) {
					printf("[Effect] Reloading effect.ha6 for %s\n", active->getName().c_str());
					active->effectCharacter.reset();
					if (active->loadEffectCharacter()) {
						printf("[Effect] ✓ Successfully reloaded\n");
					} else {
						printf("[Effect] ✗ Failed to reload\n");
					}
				}
			} else if (active) {
				// Show status when not loaded for current character
				ImGui::TextDisabled("Effect.ha6: Not loaded for %s", active->getName().c_str());
			} else {
				// No active character
				ImGui::TextDisabled("Effect.ha6: No character loaded");
			}

			ImGui::Separator();
			DrawPackageToolsMenuItems();
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			auto* active = getActiveCharacter();
			auto* view = getActiveView();
			const bool editable = active && view && !view->isStageView();
			const UndoManager* undo = editable ? &active->undoManager : nullptr;
			auto stepName = [](const UndoManager::Entry* e) {
				if (!e) return std::string();
				std::string name = e->label.empty() ? "Edit" : e->label;
				name += e->changes.size() == 1
					? " (pattern " + std::to_string(e->changes.front().index) + ")"
					: " (" + std::to_string(e->changes.size()) + " patterns)";
				return name;
			};
			const std::string undoLabel = "Undo " + (undo ? stepName(undo->peekUndo()) : std::string());
			const std::string redoLabel = "Redo " + (undo ? stepName(undo->peekRedo()) : std::string());
			if (ImGui::MenuItem(undoLabel.c_str(), shortcuts.registry().label(ShortcutAction::undo).c_str(),
			                    false, undo && undo->canUndo()))
				PerformUndoRedo(getActiveView(), false);
			if (ImGui::MenuItem(redoLabel.c_str(), shortcuts.registry().label(ShortcutAction::redo).c_str(),
			                    false, undo && undo->canRedo()))
				PerformUndoRedo(getActiveView(), true);
			if (undo) {
				ImGui::TextDisabled("History: %zu undo / %zu redo, ~%zu KiB",
					undo->undoCount(), undo->redoCount(), undo->historyBytes() / 1024);
			}
			// Stage tabs: object / frame / event / Info.txt edits (bg::File) and
			// BgList.ini / bgm.txt edits (Stage Browser) share one history.
			if (view && view->isStageView()) {
				const std::string su = stageUndoLabel(false), sr = stageUndoLabel(true);
				if (ImGui::MenuItem(("Undo " + su).c_str(), shortcuts.registry().label(ShortcutAction::undo).c_str(),
				                    false, !su.empty()))
					stageUndoRedo(false, true);
				if (ImGui::MenuItem(("Redo " + sr).c_str(), shortcuts.registry().label(ShortcutAction::redo).c_str(),
				                    false, !sr.empty()))
					stageUndoRedo(true, true);
				if (ImGui::MenuItem("Save stage + metadata", shortcuts.registry().label(ShortcutAction::save).c_str(),
				                    false, currentBgFile != nullptr))
					saveStageAll();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Keyboard shortcuts...", nullptr, m_showKeyBindings))
				m_showKeyBindings = !m_showKeyBindings;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Preferences"))
		{
			DrawExtensionProfileMenu();
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
			if (ImGui::BeginMenu("Game format"))
			{
				// Which game's HA6 dialect the labels and the writer follow (issues #14, #74).
				const Ha6Game detected = active ? active->frameData.detectedGame() : Ha6Game::Auto;
				std::string autoLabel = std::string("Auto (detected: ") + Ha6GameName(detected) + ")";
				if (ImGui::MenuItem(autoLabel.c_str(), nullptr, g_ha6GameOverride == Ha6Game::Auto))
					g_ha6GameOverride = Ha6Game::Auto;
				if (ImGui::MenuItem("MBAACC", nullptr, g_ha6GameOverride == Ha6Game::MBAACC))
					g_ha6GameOverride = Ha6Game::MBAACC;
				if (ImGui::MenuItem("UNI / UNIST / UNI2 (5 layers)", nullptr, g_ha6GameOverride == Ha6Game::UNI))
					g_ha6GameOverride = Ha6Game::UNI;
				if (ImGui::MenuItem("MELTY BLOOD: TYPE LUMINA (3 layers)", nullptr, g_ha6GameOverride == Ha6Game::MBTL))
					g_ha6GameOverride = Ha6Game::MBTL;
				ImGui::EndMenu();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Detected from the data: AFGX layer count (5 = UNI, 3 = MBTL),\n"
					"no ATV2/AFGX = MBAACC. Switches field labels (e.g. attack flag 4, #74)\n"
					"and the format new patterns are saved in.");
			ImGui::MenuItem("PUPS palette files", nullptr, &render.followPups);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Draw patterns with PUPS n using <cg>_pn.pal, as the game does (#76).");
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
				if (active->cg.getPalNumber() == 130)
					ImGui::TextDisabled("130 = 65 colours x 2 sets:\npalette n+65 is colour n's alternate set.");
				if (active->cg.pupsBankCount() > 1) {
					ImGui::TextDisabled("PUPS files loaded:");
					for (int b = 0; b < active->cg.pupsBankCount(); ++b)
						if (active->cg.hasPupsBank(b)) { ImGui::SameLine(); ImGui::TextDisabled(b == 0 ? ".pal" : "_p%d", b); }
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Zoom level"))
			{
				auto* view = getActiveView();
				float currentZoom = view ? view->getZoom() : zoom_idx;
				
				ImGui::SetNextItemWidth(80);
				if (ImGui::SliderFloat("Zoom", &currentZoom, 0.25f, 20.0f, "%.2f"))
				{
					SetZoom(currentZoom);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					Tooltip("Ctrl + Click to set exact value (per-view setting)");
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
				if (!path.empty())
					loadStageFile(path);
			}
			if (ImGui::MenuItem("Save Stage As...", nullptr, false, currentBgFile != nullptr))
			{
				std::string path = FileDialog(fileType::DAT, true);
				if (!path.empty())
					currentBgFile->Save(path.c_str());
			}
			if (ImGui::MenuItem("Clear Stage", nullptr, false, currentBgFile != nullptr))
				clearStage();
			if (ImGui::MenuItem("Stage Browser", nullptr, m_showStageBrowser))
				m_showStageBrowser = !m_showStageBrowser;
			if (ImGui::BeginMenu("Open game stage", ensureStageProject())) {
				const bg::StageEntry* cur = currentBgFile ? stageProject.FindByDat(currentBgFile->GetFilename()) : nullptr;
				for (const auto& e : stageProject.Entries()) {
					if (e.datPath.empty()) continue;
					if (ImGui::MenuItem(e.Label().c_str(), nullptr, cur && cur->id == e.id)) openStageInActiveTab(e.datPath);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Previous stage", shortcuts.registry().label(ShortcutAction::previousStage).c_str(), false, currentBgFile != nullptr))
				stepStage(-1);
			if (ImGui::MenuItem("Next stage", shortcuts.registry().label(ShortcutAction::nextStage).c_str(), false, currentBgFile != nullptr))
				stepStage(1);
			bool gameTex = bgRenderer.IsGameTextures();
			if (ImGui::Checkbox("Game-accurate textures", &gameTex))
				bgRenderer.SetGameTextures(gameTex);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("On: textures as the game builds them (DXT5 on stages over the\n"
				                  "30000 KB budget via bg_dxt32.exe, pow2 textures, the game's UV insets).\n"
				                  "Off: the clean CG, for editing.");

			ImGui::Separator();

			bool bgEnabled = bgRenderer.IsEnabled();
			if (ImGui::Checkbox("Enable Stage Rendering", &bgEnabled))
				bgRenderer.SetEnabled(bgEnabled);

			bool showDebug = bgRenderer.IsShowingDebugOverlay();
			if (ImGui::Checkbox("Show Debug Overlay", &showDebug))
				bgRenderer.SetShowDebugOverlay(showDebug);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Bounding boxes, camera position, screen center");

			bool parallaxEnabled = bgRenderer.IsParallaxEnabled();
			if (ImGui::Checkbox("Enable Parallax", &parallaxEnabled))
				bgRenderer.SetParallaxEnabled(parallaxEnabled);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Disable to see objects at world positions without parallax effect");

			if (currentBgFile && currentBgFile->IsLoaded())
			{
				ImGui::Separator();
				ImGui::TextDisabled("Loaded: %zu objects", currentBgFile->GetObjects().size());
				ImGui::TextDisabled("Camera: (%.0f, %.0f)", bgCamera.panLastX, bgCamera.panLastY);
			}

			ImGui::EndMenu();
		}
		DrawRenderMenu();

		if (ImGui::BeginMenu("Windows"))
		{
			auto* view = getActiveView();
			bool hasView = (view != nullptr);
			bool isPatEditor = hasView && view->isPatEditor();

			// HA6 Editor panes (only show when not in PAT editor mode)
			if (!isPatEditor && hasView) {
				if (view->getMainPane()) {
					std::string label = view->getMainPane()->isVisible ? "Hide Animation Panel" : "Show Animation Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getMainPane()->isVisible = !view->getMainPane()->isVisible;
					}
					// Pattern search bar toggle
					std::string searchLabel = view->getMainPane()->showPatternSearchBar ? "Hide Pattern Search Bar" : "Show Pattern Search Bar";
					if (ImGui::MenuItem(searchLabel.c_str())) {
						view->getMainPane()->showPatternSearchBar = !view->getMainPane()->showPatternSearchBar;
					}
				}
				if (view->getRightPane()) {
					std::string label = view->getRightPane()->isVisible ? "Hide Attack Panel" : "Show Attack Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getRightPane()->isVisible = !view->getRightPane()->isVisible;
					}
				}
				if (view->getBoxPane()) {
					std::string label = view->getBoxPane()->isVisible ? "Hide Hitbox Panel" : "Show Hitbox Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getBoxPane()->isVisible = !view->getBoxPane()->isVisible;
					}
				}
				ImGui::Separator();
			}

			// PAT Editor panes (only show when in PAT editor mode)
			if (isPatEditor && hasView) {
				if (view->getPartSetPane()) {
					std::string label = view->getPartSetPane()->isVisible ? "Hide PartSet Panel" : "Show PartSet Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getPartSetPane()->isVisible = !view->getPartSetPane()->isVisible;
					}
				}
				if (view->getPartPane()) {
					std::string label = view->getPartPane()->isVisible ? "Hide Part Panel" : "Show Part Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getPartPane()->isVisible = !view->getPartPane()->isVisible;
					}
				}
				if (view->getShapePane()) {
					std::string label = view->getShapePane()->isVisible ? "Hide Shape Panel" : "Show Shape Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getShapePane()->isVisible = !view->getShapePane()->isVisible;
					}
				}
				if (view->getTexturePane()) {
					std::string label = view->getTexturePane()->isVisible ? "Hide Texture Panel" : "Show Texture Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getTexturePane()->isVisible = !view->getTexturePane()->isVisible;
					}
				}
				if (view->getToolPane()) {
					std::string label = view->getToolPane()->isVisible ? "Hide Tool Panel" : "Show Tool Panel";
					if (ImGui::MenuItem(label.c_str())) {
						view->getToolPane()->isVisible = !view->getToolPane()->isVisible;
					}
				}
				ImGui::Separator();
			}

			// Global windows
			if (ImGui::MenuItem("Vectors Guide")) vectors.drawWindow = !vectors.drawWindow;
			if (ImGui::MenuItem("Variable references (batch replace)", nullptr, m_varRefs.open)) m_varRefs.open = !m_varRefs.open;
			if (ImGui::MenuItem("Pattern manager", nullptr, m_patMgr.open)) m_patMgr.open = !m_patMgr.open;
			if (ImGui::MenuItem("Notes", nullptr, m_showNotes)) m_showNotes = !m_showNotes;
			if (ImGui::MenuItem("Pattern comparison", nullptr, m_showCompare)) m_showCompare = !m_showCompare;
			if (ImGui::MenuItem("BGM preview", nullptr, m_showBgm)) m_showBgm = !m_showBgm;
			if (ImGui::MenuItem("Stage Browser", nullptr, m_showStageBrowser)) m_showStageBrowser = !m_showStageBrowser;
			if (ImGui::MenuItem("HUD preview / colours", nullptr, m_showHud)) m_showHud = !m_showHud;
			if (ImGui::MenuItem("MBAC (HA4) Inspector", nullptr, ha4ui::showInspector)) ha4ui::showInspector = !ha4ui::showInspector;
			if (ImGui::MenuItem("Game Link (MBAACC)", nullptr, gamelink::showPanel)) gamelink::showPanel = !gamelink::showPanel;
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
				if (ImGui::IsItemHovered() && active->frameData.ownFile() >= 0) {
					ImGui::SetTooltip("Saving writes the patterns that came from %s plus every\n"
					                  "pattern you edited. %d unedited pattern(s) inherited from the\n"
					                  "other files of %s are left in those files.",
					                  ha6Name.c_str(), active->frameData.inheritedPatternCount(), txtName.c_str());
				}
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
