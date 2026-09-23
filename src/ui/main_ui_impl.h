#ifndef UI_MAIN_UI_IMPL_H_GUARD
#define UI_MAIN_UI_IMPL_H_GUARD

// ============================================================================
// Main UI Drawing Implementation
// ============================================================================
// Contains the MainFrame::DrawUi() member function implementation
//
// This file is included at the end of main_frame.cpp to keep the UI drawing
// separate from the main file for better organization.
//
// UI Components:
// - Character tabs (switch between loaded characters)
// - Timeline display (frame-by-frame visualization)
// - Inspector panels (frame data editing)
// - Pattern list (sequence management)
// - Box display (hitboxes, hurtboxes, collision boxes)
// - Status bars and info panels
// ============================================================================

void MainFrame::DrawUi()
{
	ImGuiID errorPopupId = ImGui::GetID("Loading Error");
	shortcuts.beginFrame();
	UpdateTransport();
	

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
					std::string tabId = view->getDisplayName() + "###view_" + std::to_string((uintptr_t)view);
					if (ImGui::BeginTabItem(tabId.c_str(), nullptr, flags)) {
						if (activeViewIndex != (int)i) {
							setActiveView(i);
						}
						ImGui::EndTabItem();
					}
					continue;
				}

				// Use stable ID (pointer) for ImGui, display name can change
				std::string tabId = view->getDisplayName() + "###view_" + std::to_string((uintptr_t)view);
				if (ImGui::BeginTabItem(tabId.c_str(), &open, flags)) {
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
				if (ImGui::MenuItem("Load Pattern (.pat)...")) {
					std::string path = FileDialog(fileType::PAT, false);
					if (!path.empty()) {
						createPatEditorView(path);
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

		// Force rebuild if needsDockRebuild flag is set (e.g., when opening PatEditor)
		if (needsDockRebuild) {
			ImGui::DockBuilderRemoveNode(dockspaceID);
			needsDockRebuild = false;
		}

		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, clientRect);

			ImGuiID toSplit = dockspaceID;
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Left, 0.30f, nullptr, &toSplit);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Right, 0.45f, nullptr, &toSplit);
			ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Down, 0.25f, nullptr, &toSplit);
			ImGuiID dock_pat_right_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Right, 0.60f, nullptr, &toSplit);

			ImGuiID texture_dock = ImGui::DockBuilderSplitNode(dock_down_id, ImGuiDir_Right, 0.50f, nullptr, &dock_down_id);
			ImGuiID part_dock = ImGui::DockBuilderSplitNode(dock_left_id, ImGuiDir_Down, 0.50f, nullptr, &dock_left_id);

			// Normal HA6 editing panes
			ImGui::DockBuilderDockWindow("Left Pane", dock_left_id);
			ImGui::DockBuilderDockWindow("Right Pane", dock_right_id);
 			ImGui::DockBuilderDockWindow("Box Pane", dock_down_id);

			// PatEditor panes
			ImGui::DockBuilderDockWindow("PartSet Pane", dock_left_id);
			ImGui::DockBuilderDockWindow("Part Pane", part_dock);
			ImGui::DockBuilderDockWindow("Tool Pane", dock_pat_right_id);
			ImGui::DockBuilderDockWindow("Shape Pane", dock_down_id);
			ImGui::DockBuilderDockWindow("Texture Pane", texture_dock);

			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_NoDockingInCentralNode |
			ImGuiDockNodeFlags_AutoHideTabBar 
			//ImGuiDockNodeFlags_NoSplit
		); 
	ImGui::End();

	// Project actions (new/open/recent/close) requested from menus, shortcuts
	// or the unsaved-changes dialog run here, outside any menu or popup, so
	// they never tear down views/characters mid-draw.
	processDeferredProjectAction();

	// Error popups requested from inside menus/popups are opened here, at the
	// same ID-stack level as the BeginPopupModal calls below.
	if (m_pendingErrorPopup) {
		ImGui::OpenPopup(m_pendingErrorPopup);
		m_pendingErrorPopup = nullptr;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Save Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The file could not be saved. The original file on disk was left unchanged\n"
			"and the character is still marked as modified.\n\n");
		if (!m_errorDetail.empty()) {
			ImGui::TextUnformatted(m_errorDetail.c_str());
			ImGui::Text("\n");
		}
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

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
					if (saveCharacter(character)) {
						closeView(pendingCloseViewIndex);
					}
					// On failure the view stays open and the Save Error popup
					// (queued by saveCharacter) takes over from this one.
					pendingCloseViewIndex = -1;
					ImGui::CloseCurrentPopup();
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
		if (!m_errorDetail.empty()) {
			ImGui::TextUnformatted(m_errorDetail.c_str());
			ImGui::Text("\n");
		}
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
		if (!m_errorDetail.empty()) {
			ImGui::TextUnformatted(m_errorDetail.c_str());
			ImGui::Text("\n");
		}
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
			// Save modified characters as well as the project file; the old
			// code only saved the .hproj and then discarded character edits.
			bool ok = saveAllModifiedCharacters();
			if (ok && m_projectModified) {
				saveProject();
				ok = !m_projectModified;
			}
			m_pendingProjectClose = false;
			ImGui::CloseCurrentPopup();
			if (ok) {
				// Run the pending action (with its path) next, outside this popup
				requestProjectAction(m_projectCloseAction, m_pendingProjectPath, true);
			}
			// On failure the action is cancelled; the save error popup explains why
			// (a cancelled Save As dialog simply cancels).
			m_projectCloseAction = ProjectCloseAction::None;
			m_pendingProjectPath.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
			m_pendingProjectClose = false;
			ImGui::CloseCurrentPopup();

			// Run the pending action without saving, outside this popup
			requestProjectAction(m_projectCloseAction, m_pendingProjectPath, true);
			m_projectCloseAction = ProjectCloseAction::None;
			m_pendingProjectPath.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			m_pendingProjectClose = false;
			m_projectCloseAction = ProjectCloseAction::None;
			m_pendingProjectPath.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Only draw panes if we have an active view
	auto* view = getActiveView();
	if (view) {
		// Undo: make sure the committed baseline exists before any pane can
		// edit, and remember where the user is so undo can navigate back.
		auto* character = view->getCharacter();
		if (character) {
			character->undoManager.noteFocus(view->getState().pattern, view->getState().frame);
			character->undoManager.ensureBaseline();
		}

		// This view's surface owns focus-scoped shortcuts for the next key
		// messages (a later claim in the frame, e.g. a command workspace, wins).
		shortcuts.claimFocus(view->isStageView() ? ShortcutContext::stageView
			: view->isPatEditor() ? ShortcutContext::patEditor
			: ShortcutContext::characterView, (uint64_t)(uintptr_t)view);

		// Set effectFrameData on all panes if effect.ha6 is loaded (per-character)
		if (character && character->effectCharacter) {
			if (view->getMainPane()) view->getMainPane()->setEffectFrameData(&character->effectCharacter->frameData);
			if (view->getRightPane()) view->getRightPane()->setEffectFrameData(&character->effectCharacter->frameData);
			if (view->getBoxPane()) view->getBoxPane()->setEffectFrameData(&character->effectCharacter->frameData);
		}

		// Draw HA6 editor panes (only if visible and not in PAT editor mode)
		if (!view->isPatEditor()) {
			if (view->getMainPane() && view->getMainPane()->isVisible) view->getMainPane()->Draw();
			if (view->getRightPane() && view->getRightPane()->isVisible) view->getRightPane()->Draw();
			if (view->getBoxPane() && view->getBoxPane()->isVisible) view->getBoxPane()->Draw();
			ha4ui::DrawInspector(character, view->getState());   // MBAC .DAT only
		}

		// Draw PatEditor panes if this is a PAT editor view (only if visible)
		if (view->isPatEditor()) {
			if (view->getPartSetPane() && view->getPartSetPane()->isVisible) view->getPartSetPane()->Draw();
			if (view->getPartPane() && view->getPartPane()->isVisible) view->getPartPane()->Draw();
			if (view->getShapePane() && view->getShapePane()->isVisible) view->getShapePane()->Draw();
			if (view->getTexturePane() && view->getTexturePane()->isVisible) view->getTexturePane()->Draw();
			if (view->getToolPane() && view->getToolPane()->isVisible) view->getToolPane()->Draw();
		}

		// Undo: a step ends when no widget is active, no mouse button is held
		// and no explicit transaction (box draw, position drag) is open. Every
		// edit made during the gesture - on any pattern - becomes one step.
		if (character) {
			const ImGuiIO& io = ImGui::GetIO();
			const bool gesture = ImGui::IsAnyItemActive() ||
				io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2];
			auto& undo = character->undoManager;
			undo.endFrame(gesture);
			// Tab '*' follows the history: undoing back to the saved
			// revision clears it, any other committed revision sets it.
			if (!gesture && !undo.inTransaction()) {
				if (undo.isClean()) character->clearModified();
				else character->markModified();
			}
		}
	}
	aboutWindow.Draw();
	vectors.Draw();

	// Background (stage) Inspector — shows the currently loaded stage's
	// objects, lets you scrub through frames, and exposes editable fields.
	// Only visible when a stage is loaded.
	if (currentBgFile && currentBgFile->IsLoaded())
	{
		// FirstUseEver = land at a sensible default the first time the user
		// sees the window, but let them move it freely after that (and the
		// position persists in imgui's .ini between runs, like every other
		// pane in the editor).
		ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Background Inspector", nullptr, 0);

		ImGui::Text("File: %s", currentBgFile->GetFilename().c_str());
		auto& objects = currentBgFile->GetObjects();
		ImGui::Text("Objects: %zu", objects.size());

		// Diagnostic readout — exposes the exact values feeding the GL transform
		// so we can compare numerically to u4ick's bgmaketool. The expected
		// screen position for any sprite is: (offset + render.x) * render.scale
		// where render.x = bgCamera.panLastX (mirrored each frame from the
		// camera's stable pose).
		ImGui::Separator();
		ImGui::TextDisabled("--- Diagnostics ---");
		ImGui::Text("bgCamera pan=(%.1f, %.1f) panLast=(%.1f, %.1f) dragging=%d",
		            bgCamera.panX, bgCamera.panY,
		            bgCamera.panLastX, bgCamera.panLastY,
		            (int)bgCamera.dragging);
		// render.x / render.y are `int` (defined in render.h); the previous
		// %.1f format was undefined behavior on x64 and was reporting
		// garbage zeros that made it look like render.x wasn't being
		// mirrored from bgCamera. Cast explicitly so we see the real value.
		ImGui::Text("render scale=%.2f x=%d y=%d",
		            render.scale, render.x, render.y);
		ImGui::Text("viewport clientRect=(%.0f, %.0f)", clientRect.x, clientRect.y);
		if (!objects.empty() && !objects[0].frames.empty()) {
			const auto& obj0 = objects[0];
			const auto& f0 = obj0.frames[obj0.currentFrame];
			float sx = bgCamera.ScreenX((float)f0.offsetX, obj0.parallax);
			float sy = bgCamera.ScreenY((float)f0.offsetY, obj0.parallax);
			float screenPx = sx + bgCamera.panLastX;
			float screenPy = sy + bgCamera.panLastY;
			ImGui::Text("obj[0].f[%d/%d] sprId=%d offset=(%d, %d)",
			            obj0.currentFrame, (int)obj0.frames.size() - 1,
			            f0.spriteId, f0.offsetX, f0.offsetY);
			ImGui::Text("  -> screen=(%.1f, %.1f)", screenPx, screenPy);

			// Y-flicker diagnostic: dump sprite dimensions AND content
			// origin (bounds_x1, bounds_y1) for every frame in obj[0]'s
			// animation. If offsetY varies per frame, the bounded
			// content position shifts even though width/height are fixed —
			// that explains the user-visible flicker.
			if (auto* cg = currentBgFile->GetCG()) {
				ImGui::Text("Sprite dims (per frame):");
				for (size_t fi = 0; fi < obj0.frames.size(); ++fi) {
					int sid = obj0.frames[fi].spriteId;
					// spriteId is raw: >=10000 is a CG sprite (index sid-10000),
					// <10000 is a PAT pattern (no CG texture to show here).
					int cgIdx = sid >= 10000 ? sid - 10000 : -1;
					ImageData* img = cgIdx >= 0 ? cg->draw_texture(cgIdx, false, false) : nullptr;
					int w = img ? img->width : -1;
					int h = img ? img->height : -1;
					int ox = img ? img->offsetX : -1;
					int oy = img ? img->offsetY : -1;
					delete img;
					ImGui::Text("  [%zu] spr=%d  %dx%d  origin=(%d, %d)",
					            fi, sid, w, h, ox, oy);
				}
			}
		}
		if (ImGui::Button("Center View on obj[0]") && !objects.empty() && !objects[0].frames.empty()) {
			// Pan so obj[0]'s sprite top-left lands at viewport center —
			// simplest possible 'where IS the sprite supposed to be' test.
			const auto& f0 = objects[0].frames[0];
			float wantScreenX = clientRect.x * 0.5f;
			float wantScreenY = clientRect.y * 0.5f;
			float newPanX = wantScreenX / render.scale - (float)f0.offsetX;
			float newPanY = wantScreenY / render.scale - (float)f0.offsetY;
			bgCamera.SetPan(newPanX, newPanY);
			if (auto* v = getActiveView()) v->setStageRenderXY(newPanX, newPanY);
		}
		if (ImGui::Button("Reset Pan (0, 0)")) {
			bgCamera.SetPan(0.0f, 0.0f);
			if (auto* v = getActiveView()) v->setStageRenderXY(0.0f, 0.0f);
		}
		// (Layer ordering is now handled by depth test in the new bg
		// renderer; toggle removed.)
		// Manual pan entry so we can test specific values against u4ick's
		// observed layout pixel-for-pixel.
		static float manualPanX = 0.0f, manualPanY = 0.0f;
		manualPanX = bgCamera.panLastX;
		manualPanY = bgCamera.panLastY;
		ImGui::PushItemWidth(100);
		ImGui::InputFloat("##manualPanX", &manualPanX);
		ImGui::SameLine();
		ImGui::InputFloat("##manualPanY", &manualPanY);
		ImGui::SameLine();
		if (ImGui::Button("Apply Pan")) {
			bgCamera.SetPan(manualPanX, manualPanY);
			if (auto* v = getActiveView()) v->setStageRenderXY(manualPanX, manualPanY);
		}
		ImGui::PopItemWidth();

		ImGui::Separator();

		bool bgPaused = bgRenderer.IsPaused();
		if (ImGui::Button(bgPaused ? "Play" : "Pause")) bgRenderer.SetPaused(!bgPaused);
		ImGui::SameLine();
		ImGui::TextDisabled("Animation: %s", bgPaused ? "PAUSED" : "Playing");
		// Animation advancement happens via bgRenderer.Update() in DrawBack;
		// don't double-tick here.

		bool bgParallax = bgRenderer.IsParallaxEnabled();
		if (ImGui::Checkbox("Parallax", &bgParallax)) bgRenderer.SetParallaxEnabled(bgParallax);
		ImGui::SameLine();
		bool bgOverlay = bgRenderer.IsShowingDebugOverlay();
		if (ImGui::Checkbox("Stage rects", &bgOverlay)) bgRenderer.SetShowDebugOverlay(bgOverlay);
		ImGui::TextDisabled("Left-drag to pan, mouse-wheel to zoom.");

		ImGui::Separator();

		static int selectedObjIndex = 0;
		if (selectedObjIndex >= (int)objects.size()) selectedObjIndex = 0;
		bgRenderer.SetSelectedObject(selectedObjIndex);

		ImGui::Text("Objects (draw order: low layer first):");
		if (ImGui::SmallButton("Show All")) {
			for (auto& o : objects) o.visible = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Solo Selected")) {
			for (size_t i = 0; i < objects.size(); ++i)
				objects[i].visible = ((int)i == selectedObjIndex);
		}
		for (size_t i = 0; i < objects.size(); ++i)
		{
			ImGui::PushID((int)i);
			ImGui::Checkbox("##vis", &objects[i].visible);
			ImGui::SameLine();
			char label[64];
			snprintf(label, sizeof(label), "obj_%zu (layer=%d  parallax=%d)",
			         i, objects[i].layer, objects[i].parallax);
			if (ImGui::Selectable(label, selectedObjIndex == (int)i))
				selectedObjIndex = (int)i;
			ImGui::PopID();
		}

		ImGui::Separator();

		if (selectedObjIndex >= 0 && selectedObjIndex < (int)objects.size())
		{
			auto& obj = objects[selectedObjIndex];
			ImGui::Text("Object %d Properties:", selectedObjIndex);
			ImGui::Text("  Name: %s", obj.name.c_str());
			ImGui::PushItemWidth(120);
			ImGui::InputInt("Layer (draw order)", &obj.layer);
			ImGui::PopItemWidth();
			ImGui::Text("  Parallax: %d", obj.parallax);
			ImGui::Text("  Frames: %zu", obj.frames.size());
			ImGui::Text("  Current Frame: %d / %d", obj.currentFrame, (int)obj.frames.size() - 1);

			if (ImGui::Button("<< Back")) currentBgFile->StepObjectBackward(selectedObjIndex);
			ImGui::SameLine();
			if (ImGui::Button("Forward >>")) currentBgFile->StepObjectForward(selectedObjIndex);

			ImGui::Separator();
			ImGui::Text("Current Frame Data (Editable):");

			if (obj.currentFrame >= 0 && obj.currentFrame < (int)obj.frames.size())
			{
				auto& frame = obj.frames[obj.currentFrame];

				ImGui::PushItemWidth(120);
				ImGui::InputScalar("Sprite ID",      ImGuiDataType_S16, &frame.spriteId);
				ImGui::InputScalar("Offset X",       ImGuiDataType_S16, &frame.offsetX);
				ImGui::InputScalar("Offset Y",       ImGuiDataType_S16, &frame.offsetY);
				ImGui::InputScalar("Duration",       ImGuiDataType_S16, &frame.duration);
				ImGui::InputScalar("Blend Mode",     ImGuiDataType_U8,  &frame.blendMode);
				ImGui::InputScalar("Opacity",        ImGuiDataType_U8,  &frame.opacity);
				ImGui::InputScalar("Animation Type", ImGuiDataType_U8,  &frame.aniType);
				ImGui::InputScalar("Jump Frame",     ImGuiDataType_U8,  &frame.jumpFrame);
				ImGui::PopItemWidth();

				ImGui::Text("World: (%d, %d)", frame.offsetX, frame.offsetY);

				ImGui::Separator();
				ImGui::Text("All Frames:");
				size_t shown = std::min<size_t>(obj.frames.size(), 20);
				for (size_t f = 0; f < shown; ++f)
				{
					const auto& fr = obj.frames[f];
					ImGui::Text("  [%zu] spriteId=%d offset=(%d,%d) aniType=%d",
					            f, fr.spriteId, fr.offsetX, fr.offsetY, fr.aniType);
				}
				if (obj.frames.size() > shown)
					ImGui::TextDisabled("  ... (%zu more frames)", obj.frames.size() - shown);
			}
		}

		ImGui::End();
	}

	DrawPositionTool();
	shortcuts.endFrame();
	RenderUpdate();
}

// Create ActiveSpawnInstances for script-declared spawns (mv_script) whose
// spawnTick matches `tick`, so they appear during LIVE playback and not only
// in the paused/seek views. Reads the spawn-tree entries so visibility
// toggles and (once implemented) frame-ID timing are honored automatically.
static void CreateScriptSpawnInstances(FrameState& state, CharacterInstance* active, int tick)
{
	if (!active) return;
	for (const auto& sp : state.spawnedPatterns) {
		if (!sp.isScriptSpawn || !sp.visible || sp.patternId < 0) continue;
		if (sp.spawnTick != tick) continue;
		auto spawnSeq = active->frameData.get_sequence(sp.patternId);
		if (!spawnSeq || spawnSeq->frames.empty()) continue;

		ActiveSpawnInstance instance;
		instance.spawnTick = tick;
		instance.patternId = sp.patternId;
		instance.usesEffectHA6 = sp.usesEffectHA6;
		instance.isPresetEffect = false;
		instance.offsetX = sp.offsetX;
		instance.offsetY = sp.offsetY;
		instance.parentFrame = sp.parentFrame;
		instance.tintColor = state.vizSettings.enableTint ? sp.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		instance.alpha = 1.0f;
		instance.currentFrame = 0;
		instance.frameDuration = 0;
		instance.previousFrame = 0;
		instance.loopCounter = spawnSeq->frames[0].AF.loopCount;
		state.activeSpawns.push_back(instance);
	}
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
		// Handle animation playback
		static int duration = 0;
		static int loopCounter = 0;

		if(state.animeSeq != state.pattern)
		{
			duration = 0;
			loopCounter = 0;
			// Clear active spawns when pattern changes
			state.activeSpawns.clear();
			state.frameVisitCounts.clear();
			state.lastSpawnCreationFrame = -1;
		}

		// Track previous animation state to detect play button press (static per-view)
		static bool wasAnimating = false;

		if(state.animating)
		{
			state.animeSeq = state.pattern;

			// Detect animation start: transition from paused (false) to playing (true)
			bool animationJustStarted = !wasAnimating && state.animating;

			// Reset visit tracking whenever animation starts (play button pressed)
			// This ensures every playthrough is fresh, regardless of where user seeked to
			if (animationJustStarted) {
				state.frameVisitCounts.clear();
				state.activeSpawns.clear();  // Also clear any stale spawns from previous run
				state.lastSpawnCreationFrame = -1;
			}

			// Initialize visit count for starting frame if needed
			if (state.frameVisitCounts.empty()) {
				state.frameVisitCounts[state.frame] = 1;

				// Script-declared spawns (mv_script) that fire at the current tick
				if (state.vizSettings.showSpawnedPatterns) {
					CreateScriptSpawnInstances(state, active, state.currentTick);
				}

				// Check for spawns on frame 0 at animation start
				if (state.vizSettings.showSpawnedPatterns && state.frame >= 0 && state.frame < seq->frames.size()) {
					auto& frame0 = seq->frames[state.frame];
					if (!frame0.EF.empty()) {
						state.lastSpawnCreationFrame = state.frame;
						auto spawns = ParseSpawnedPatterns(frame0.EF, state.frame, state.pattern);

						for(const auto& spawnInfo : spawns) {
							ActiveSpawnInstance instance;
							instance.spawnTick = state.currentTick;
							instance.patternId = spawnInfo.patternId;
							instance.usesEffectHA6 = spawnInfo.usesEffectHA6;
							instance.isPresetEffect = spawnInfo.isPresetEffect;
							instance.offsetX = spawnInfo.offsetX;
							instance.offsetY = spawnInfo.offsetY;
							instance.flagset1 = spawnInfo.flagset1;
							instance.flagset2 = spawnInfo.flagset2;
							instance.angle = spawnInfo.angle;
							instance.projVarDecrease = spawnInfo.projVarDecrease;
							instance.parentFrame = spawnInfo.parentFrame;
							instance.tintColor = state.vizSettings.enableTint ? spawnInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
							// Visualization opacity is applied once at render time
							// (main_frame layer.alpha multiply) — pre-baking it here
							// squared it for spawns with no static entry.
							instance.alpha = 1.0f;
							instance.currentFrame = 0;
							instance.frameDuration = 0;
							instance.previousFrame = 0;

							if (!instance.isPresetEffect) {
								FrameData* sourceData = instance.usesEffectHA6 && active->effectCharacter
									? &active->effectCharacter->frameData
									: &active->frameData;

								auto spawnSeq = sourceData->get_sequence(instance.patternId);
								if (spawnSeq && !spawnSeq->frames.empty()) {
									instance.loopCounter = spawnSeq->frames[0].AF.loopCount;

									if (!spawnSeq->frames[0].EF.empty()) {
										auto frame0Spawns = ParseSpawnedPatterns(spawnSeq->frames[0].EF, 0, instance.patternId);
										for (const auto& childInfo : frame0Spawns) {
											ActiveSpawnInstance childInstance;
											childInstance.spawnTick = state.currentTick;
											childInstance.patternId = childInfo.patternId;
											childInstance.usesEffectHA6 = childInfo.usesEffectHA6;
											childInstance.isPresetEffect = childInfo.isPresetEffect;

											if (instance.usesEffectHA6 && childInfo.effectType != 8) {
												childInstance.usesEffectHA6 = true;
											}

											childInstance.offsetX = instance.offsetX + childInfo.offsetX;
											childInstance.offsetY = instance.offsetY + childInfo.offsetY;
											childInstance.flagset1 = childInfo.flagset1;
											childInstance.flagset2 = childInfo.flagset2;
											childInstance.angle = childInfo.angle;
											childInstance.projVarDecrease = childInfo.projVarDecrease;
											childInstance.tintColor = state.vizSettings.enableTint ? childInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
											childInstance.alpha = state.vizSettings.spawnedOpacity;
											childInstance.currentFrame = 0;
											childInstance.frameDuration = 0;
											childInstance.previousFrame = 0;

											if (!childInstance.isPresetEffect) {
												FrameData* childSourceData = childInstance.usesEffectHA6 && active->effectCharacter
													? &active->effectCharacter->frameData
													: &active->frameData;
												auto childSeq = childSourceData->get_sequence(childInstance.patternId);
												if (childSeq && !childSeq->frames.empty()) {
													childInstance.loopCounter = childSeq->frames[0].AF.loopCount;
												}
											}

											state.activeSpawns.push_back(childInstance);
										}
									}
								}
							}

							state.activeSpawns.push_back(instance);
						}
					}
				}
			}

			// Lambda to calculate next frame - can be called with or without side effects
			auto GetNextFrame = [&](bool decreaseLoopCounter) {
				if (seq && !seq->frames.empty())
				{
					auto& af = seq->frames[state.frame].AF;
					if (af.aniType == 1) {
						// Sequential advance - stop at end
						if (state.frame + 1 >= seq->frames.size()) {
							return 0;
						}
						else {
							return state.frame + 1;
						}
					}
					else if (af.aniType == 2)
					{
						// Jump/loop logic
						if ((af.aniFlag & 0x2) && loopCounter < 0)
						{
							// Loop count exhausted - use loopEnd
							if (af.aniFlag & 0x8) {
								return state.frame + af.loopEnd;
							}
							else {
								return af.loopEnd;
							}
						}
						else
						{
							// Decrement loop counter if needed
							if (af.aniFlag & 0x2 && decreaseLoopCounter) {
								--loopCounter;
							}
							// Jump to next frame
							if (af.aniFlag & 0x4) {
								return state.frame + af.jump;
							}
							else {
								return af.jump;
							}
						}
					}
					else {
						// aniType 0 or other - stop animation
						return 0;
					}
				}
				return state.frame;
			};

			if(duration >= seq->frames[state.frame].AF.duration)
			{
				if(seq && !seq->frames.empty())
				{
					int oldFrame = state.frame;
					state.frame = GetNextFrame(true);
					duration = 0;

						// Update previousFrame only if we successfully advanced
						if (state.frame != oldFrame) {
							state.previousFrame = oldFrame;

							// Detect loop: check if we jumped backwards based on actual animation data
							// This happens when aniType == 2 and jump points to an earlier frame
							bool isLooping = false;
							if (oldFrame >= 0 && oldFrame < seq->frames.size()) {
								auto& oldFrameData = seq->frames[oldFrame].AF;
								// Loop detected if: aniType is 2 (jump) and we jumped to an earlier frame
								// OR if we jumped backwards (which indicates a loop in the animation flow)
								if (oldFrameData.aniType == 2) {
									// Check if the jump target is earlier than current frame (loop back)
									int jumpTarget = oldFrameData.jump;
									if ((oldFrameData.aniFlag & 0x4) == 0) {
										// Absolute jump (not relative)
										isLooping = (jumpTarget < oldFrame);
									} else {
										// Relative jump
										isLooping = ((oldFrame + jumpTarget) < oldFrame);
									}
								}
								// Also detect if frame number decreased (backward jump occurred)
								if (!isLooping) {
									isLooping = (state.frame < oldFrame);
								}
							}
							
							if (isLooping) {
								// DEBUG: Log loop detection with animation data
								if (oldFrame >= 0 && oldFrame < seq->frames.size()) {
									auto& oldFrameData = seq->frames[oldFrame].AF;
									printf("[LOOP DETECTED] Frame %d (aniType=%d, jump=%d, aniFlag=0x%x) -> Frame %d (tick: %d)\n", 
										oldFrame, oldFrameData.aniType, oldFrameData.jump, oldFrameData.aniFlag, state.frame, state.currentTick);
								} else {
									printf("[LOOP DETECTED] Jumped from frame %d to %d (tick: %d)\n", oldFrame, state.frame, state.currentTick);
								}
								// Don't clear visit counts - we want to track how many times we've visited each frame
								// But reset lastSpawnCreationFrame so spawns can be created again on loop iterations
								state.lastSpawnCreationFrame = -1;
							}

						// Increment visit count for the new frame we're entering
						state.frameVisitCounts[state.frame]++;

						// DEBUG: Log frame transitions (only for significant changes)
						static int lastLoggedFrame = -1;
						if (state.frame != lastLoggedFrame && (state.frame % 10 == 0 || state.frame < 5)) {
							printf("[FRAME ADVANCE] %d -> %d (visit count: %d, tick: %d)\n",
								oldFrame, state.frame, state.frameVisitCounts[state.frame], state.currentTick);
							lastLoggedFrame = state.frame;
						}

						// Check for spawns immediately when entering a new frame (BEFORE incrementing tick)
						// Create spawns every time we enter a frame with spawn effects (including loop iterations)
						if (state.vizSettings.showSpawnedPatterns && state.frame >= 0 && state.frame < seq->frames.size()) {
							auto& newFrame = seq->frames[state.frame];
							// Allow spawns to be created on every visit to a frame with spawn effects
							// This ensures spawns appear on loop iterations
							bool shouldCreateSpawns = !newFrame.EF.empty() &&
							                          (state.lastSpawnCreationFrame != state.frame);

							if(shouldCreateSpawns)
							{
								// DEBUG: Log spawn creation
								// Mark that this frame has created spawns
								state.lastSpawnCreationFrame = state.frame;
								// Parse spawn effects in current frame
								auto spawns = ParseSpawnedPatterns(newFrame.EF, state.frame, state.pattern);

								// Create active spawn instances for each spawn effect
								for(const auto& spawnInfo : spawns) {
									ActiveSpawnInstance instance;
									instance.spawnTick = state.currentTick;  // Use current tick BEFORE incrementing
									instance.patternId = spawnInfo.patternId;
									instance.usesEffectHA6 = spawnInfo.usesEffectHA6;
									instance.isPresetEffect = spawnInfo.isPresetEffect;
									instance.offsetX = spawnInfo.offsetX;
									instance.offsetY = spawnInfo.offsetY;
									instance.flagset1 = spawnInfo.flagset1;
									instance.flagset2 = spawnInfo.flagset2;
									instance.angle = spawnInfo.angle;
									instance.projVarDecrease = spawnInfo.projVarDecrease;
									instance.parentFrame = spawnInfo.parentFrame;
									instance.tintColor = state.vizSettings.enableTint ? spawnInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
									// Applied once at render time; pre-baking squared it.
									instance.alpha = 1.0f;

									// Initialize animation state for frame-by-frame advancement
									instance.currentFrame = 0;
									instance.frameDuration = 0;
									instance.previousFrame = 0;

									// Initialize loop counter from the spawned pattern's first frame
									if (!instance.isPresetEffect) {
										FrameData* sourceData = instance.usesEffectHA6 && active->effectCharacter
											? &active->effectCharacter->frameData
											: &active->frameData;

										auto spawnSeq = sourceData->get_sequence(instance.patternId);
										if (spawnSeq && !spawnSeq->frames.empty()) {
											instance.loopCounter = spawnSeq->frames[0].AF.loopCount;

											// Check frame 0 for nested spawn effects immediately
											if (!spawnSeq->frames[0].EF.empty()) {
												auto frame0Spawns = ParseSpawnedPatterns(spawnSeq->frames[0].EF, 0, instance.patternId);

												for (const auto& childInfo : frame0Spawns) {
													ActiveSpawnInstance childInstance;
													childInstance.spawnTick = state.currentTick;
													childInstance.patternId = childInfo.patternId;
													childInstance.usesEffectHA6 = childInfo.usesEffectHA6;
													childInstance.isPresetEffect = childInfo.isPresetEffect;

													if (instance.usesEffectHA6 && childInfo.effectType != 8) {
														childInstance.usesEffectHA6 = true;
													}

													childInstance.offsetX = instance.offsetX + childInfo.offsetX;
													childInstance.offsetY = instance.offsetY + childInfo.offsetY;
													childInstance.flagset1 = childInfo.flagset1;
													childInstance.flagset2 = childInfo.flagset2;
													childInstance.angle = childInfo.angle;
													childInstance.projVarDecrease = childInfo.projVarDecrease;
													childInstance.tintColor = state.vizSettings.enableTint ? childInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
													childInstance.alpha = state.vizSettings.spawnedOpacity;
													childInstance.currentFrame = 0;
													childInstance.frameDuration = 0;
													childInstance.previousFrame = 0;

													if (!childInstance.isPresetEffect) {
														FrameData* childSourceData = childInstance.usesEffectHA6 && active->effectCharacter
															? &active->effectCharacter->frameData
															: &active->frameData;
														auto childSeq = childSourceData->get_sequence(childInstance.patternId);
														if (childSeq && !childSeq->frames.empty()) {
															childInstance.loopCounter = childSeq->frames[0].AF.loopCount;
														}
													}

													state.activeSpawns.push_back(childInstance);
												}
											}
										}
									}

									state.activeSpawns.push_back(instance);
								}
							}
						}
					}

					// Bounds check
					if(state.frame < 0 || state.frame >= seq->frames.size())
					{
						state.animating = false;
						state.frame = 0;
						state.previousFrame = -1;  // Reset on animation end
						state.frameVisitCounts.clear();  // Clear frame visit tracking on animation end
						state.lastSpawnCreationFrame = -1;
					}
				}
			}

			duration++;
			state.currentTick++;  // Increment tick counter for spawned pattern synchronization

			// Script-declared spawns (mv_script) that fire at this tick — the
			// ha6 EF path above only creates spawns on frame entry, so script
			// spawns would otherwise never appear during live playback.
			if (state.vizSettings.showSpawnedPatterns) {
				CreateScriptSpawnInstances(state, active, state.currentTick);
			}

			// Advance all active spawned patterns frame-by-frame (same logic as main pattern)
			for (auto& spawn : state.activeSpawns) {
				// Skip preset effects (they don't have frames)
				if (spawn.isPresetEffect) continue;

				// Get the spawned pattern's sequence
				FrameData* sourceData = spawn.usesEffectHA6 && active->effectCharacter
					? &active->effectCharacter->frameData
					: &active->frameData;

				auto spawnSeq = sourceData->get_sequence(spawn.patternId);
				if (!spawnSeq || spawnSeq->frames.empty()) continue;

				// Increment frame duration
				spawn.frameDuration++;

				// Check if we need to advance to next frame
				if (spawn.currentFrame < spawnSeq->frames.size()) {
					auto& currentSpawnFrame = spawnSeq->frames[spawn.currentFrame];

					if (spawn.frameDuration >= currentSpawnFrame.AF.duration) {
						// Time to advance - use same logic as GetNextFrame
						int nextFrame = spawn.currentFrame;  // Default: stay on current frame

						if (currentSpawnFrame.AF.aniType == 1) {
							// Sequential advance - stop at end
							if (spawn.currentFrame + 1 >= spawnSeq->frames.size()) {
								nextFrame = -1;  // Mark for removal
							} else {
								nextFrame = spawn.currentFrame + 1;
							}
						}
						else if (currentSpawnFrame.AF.aniType == 2) {
							// Jump/loop logic (same as main pattern)
							if ((currentSpawnFrame.AF.aniFlag & 0x2) && spawn.loopCounter < 0) {
								// Loop count exhausted - use loopEnd
								if (currentSpawnFrame.AF.aniFlag & 0x8) {
									nextFrame = spawn.currentFrame + currentSpawnFrame.AF.loopEnd;
								} else {
									nextFrame = currentSpawnFrame.AF.loopEnd;
								}
							} else {
								// Decrement loop counter if needed
								if (currentSpawnFrame.AF.aniFlag & 0x2) {
									--spawn.loopCounter;
								}
								// Jump to next frame
								if (currentSpawnFrame.AF.aniFlag & 0x4) {
									nextFrame = spawn.currentFrame + currentSpawnFrame.AF.jump;
								} else {
									nextFrame = currentSpawnFrame.AF.jump;
								}
							}
						}
						else {
							// aniType 0 or other - stop animation
							nextFrame = -1;  // Mark for removal
						}

						// Apply the frame change
						if (nextFrame < 0 || nextFrame >= spawnSeq->frames.size()) {
							// Pattern ended - mark for removal by setting currentFrame to invalid
							spawn.currentFrame = -1;
						} else {
							spawn.currentFrame = nextFrame;
							spawn.frameDuration = 0;

							// Update loop counter from new frame if it has one
							if (spawnSeq->frames[spawn.currentFrame].AF.loopCount > 0) {
								spawn.loopCounter = spawnSeq->frames[spawn.currentFrame].AF.loopCount;
							}
						}
					}
				}
			}
		}

		// Create nested spawns (spawned patterns spawning child patterns)
		// Check for spawns that entered a new frame and create their child spawns
		if (state.animating && state.vizSettings.showSpawnedPatterns) {
			// We need to iterate by index (not reference) because we'll be adding to the vector
			size_t numSpawns = state.activeSpawns.size();
			for (size_t i = 0; i < numSpawns; i++) {
				auto& spawn = state.activeSpawns[i];

				// Skip if not a pattern (preset effects don't spawn children)
				if (spawn.isPresetEffect) continue;

				// Skip if pattern ended
				if (spawn.currentFrame < 0) continue;

				// Check if this spawn entered a new frame
				if (spawn.currentFrame != spawn.previousFrame) {
					// Get the spawned pattern's sequence
					FrameData* sourceData = spawn.usesEffectHA6 && active->effectCharacter
						? &active->effectCharacter->frameData
						: &active->frameData;

					auto spawnSeq = sourceData->get_sequence(spawn.patternId);
					if (spawnSeq && spawn.currentFrame < spawnSeq->frames.size()) {
						auto& spawnedFrame = spawnSeq->frames[spawn.currentFrame];

						// Check if this frame has spawn effects
						if (!spawnedFrame.EF.empty()) {
							// Parse child spawn effects
							auto childSpawns = ParseSpawnedPatterns(spawnedFrame.EF, spawn.currentFrame, spawn.patternId);

							// Create active instances for each child spawn
							for (const auto& childInfo : childSpawns) {
								ActiveSpawnInstance childInstance;
								childInstance.spawnTick = state.currentTick;
								childInstance.patternId = childInfo.patternId;
								childInstance.usesEffectHA6 = childInfo.usesEffectHA6;
								childInstance.isPresetEffect = childInfo.isPresetEffect;

								// Apply inheritance: if parent uses effect.ha6, children should too (unless they're type 8)
								if (spawn.usesEffectHA6 && childInfo.effectType != 8) {
									childInstance.usesEffectHA6 = true;
								}

								// Accumulate offsets from parent
								childInstance.offsetX = spawn.offsetX + childInfo.offsetX;
								childInstance.offsetY = spawn.offsetY + childInfo.offsetY;

								childInstance.flagset1 = childInfo.flagset1;
								childInstance.flagset2 = childInfo.flagset2;
								childInstance.angle = childInfo.angle;
								childInstance.projVarDecrease = childInfo.projVarDecrease;
								childInstance.tintColor = state.vizSettings.enableTint ? childInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
								childInstance.alpha = state.vizSettings.spawnedOpacity;

								// Initialize animation state
								childInstance.currentFrame = 0;
								childInstance.frameDuration = 0;
								childInstance.previousFrame = 0;  // Set to 0 so frame 0 isn't checked again

								// Initialize loop counter from first frame
								if (!childInstance.isPresetEffect) {
									FrameData* childSourceData = childInstance.usesEffectHA6 && active->effectCharacter
										? &active->effectCharacter->frameData
										: &active->frameData;

									auto childSeq = childSourceData->get_sequence(childInstance.patternId);
									if (childSeq && !childSeq->frames.empty()) {
										childInstance.loopCounter = childSeq->frames[0].AF.loopCount;

										// Check frame 0 for nested spawn effects (before frame advancement)
										if (!childSeq->frames[0].EF.empty()) {
											auto grandchildSpawns = ParseSpawnedPatterns(childSeq->frames[0].EF, 0, childInstance.patternId);
											for (const auto& grandchildInfo : grandchildSpawns) {
												ActiveSpawnInstance grandchildInstance;
												grandchildInstance.spawnTick = state.currentTick;
												grandchildInstance.patternId = grandchildInfo.patternId;
												grandchildInstance.usesEffectHA6 = grandchildInfo.usesEffectHA6;
												grandchildInstance.isPresetEffect = grandchildInfo.isPresetEffect;

												// Apply inheritance: if parent uses effect.ha6, children should too (unless they're type 8)
												if (childInstance.usesEffectHA6 && grandchildInfo.effectType != 8) {
													grandchildInstance.usesEffectHA6 = true;
												}

												grandchildInstance.offsetX = childInstance.offsetX + grandchildInfo.offsetX;
												grandchildInstance.offsetY = childInstance.offsetY + grandchildInfo.offsetY;
												grandchildInstance.flagset1 = grandchildInfo.flagset1;
												grandchildInstance.flagset2 = grandchildInfo.flagset2;
												grandchildInstance.angle = grandchildInfo.angle;
												grandchildInstance.projVarDecrease = grandchildInfo.projVarDecrease;
												grandchildInstance.tintColor = state.vizSettings.enableTint ? grandchildInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
												grandchildInstance.alpha = state.vizSettings.spawnedOpacity;
												grandchildInstance.currentFrame = 0;
												grandchildInstance.frameDuration = 0;
												grandchildInstance.previousFrame = 0;

												if (!grandchildInstance.isPresetEffect) {
													FrameData* grandchildSourceData = grandchildInstance.usesEffectHA6 && active->effectCharacter
														? &active->effectCharacter->frameData
														: &active->frameData;
													auto grandchildSeq = grandchildSourceData->get_sequence(grandchildInstance.patternId);
													if (grandchildSeq && !grandchildSeq->frames.empty()) {
														grandchildInstance.loopCounter = grandchildSeq->frames[0].AF.loopCount;
													}
												}

												state.activeSpawns.push_back(grandchildInstance);
											}
										}
									}
								}

								state.activeSpawns.push_back(childInstance);
							}
						}
					}

					// Update previousFrame tracker
					spawn.previousFrame = spawn.currentFrame;
				}
			}
		}

		// Update animation state tracker for next frame
		wasAnimating = state.animating;

		auto &frame =  seq->frames[state.frame];

		// Clean up finished spawns
		if(state.animating) {
			state.activeSpawns.erase(
				std::remove_if(state.activeSpawns.begin(), state.activeSpawns.end(),
					[&](const ActiveSpawnInstance& spawn) {
						// Preset effects (Type 3) are instant - remove immediately after spawn tick
						if (spawn.isPresetEffect) {
							int elapsedTicks = state.currentTick - spawn.spawnTick;
							// Remove after spawn tick (instant effect)
							return elapsedTicks > 0;
						}

						// Remove if pattern animation has ended (marked with currentFrame == -1)
						return spawn.currentFrame < 0;
					}),
				state.activeSpawns.end()
			);
		}

		// Render using layer[0] data for MBAACC compatibility
		// Ensure frame has at least one layer
		if (frame.AF.layers.empty()) {
			frame.AF.layers.push_back({});
		}
		const auto& layer = frame.AF.layers[0];

		state.spriteId = layer.spriteId;
		render.usePat = layer.usePat;  // Enable Parts rendering if usePat is true

		// Set Parts rendering pattern indices (partSet IDs)
		if (layer.usePat) {
			render.curPattern = layer.spriteId;  // spriteId is the partSet index when usePat=true
			render.curNextPattern = layer.spriteId;  // Same for now (no interpolation)
			render.curInterp = 0.0f;
		}

		// Generate geometry for PatEditor or standard views
		if (view->isPatEditor() && layer.usePat) {
			// For PatEditor, try different rendering modes in order:
			// 1. UV rectangle (TEXTURE_VIEW / UV_SETTING_VIEW)
			// 2. Part origin cross (DEFAULT mode)
			// 3. Hitboxes (fallback)
			if (!render.GenerateUVRectangleVertices()) {
				if (!render.GeneratePartCenterVertices()) {
					render.GenerateHitboxVertices(frame.hitboxes);
				}
			}
		} else {
			// For non-PatEditor views, just show hitboxes
			render.GenerateHitboxVertices(frame.hitboxes);
		}
		render.offsetX = (layer.offset_x)*1;
		render.offsetY = (layer.offset_y)*1;
		
		// Debug layer color for PAT rendering
		static bool debugLayerColor = true;
		if (debugLayerColor && layer.usePat) {
			printf("[RenderUpdate] layer.rgba for PAT: (%.3f, %.3f, %.3f, %.3f)\n",
				layer.rgba[0], layer.rgba[1], layer.rgba[2], layer.rgba[3]);
			debugLayerColor = false;
		}
		
		render.SetImageColor(const_cast<float*>(layer.rgba));
		render.rotX = layer.rotation[0];
		render.rotY = layer.rotation[1];
		render.rotZ = layer.rotation[2];
		render.AFRT = frame.AF.AFRT;
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

		// Only switch CG sprite texture if NOT using Parts rendering
		// (Parts have their own textures and shouldn't interfere with CG sprite texture)
		if (!layer.usePat) {
			render.SwitchImage(state.spriteId);
		}

		if(frame.AF.loopCount>0)
			loopCounter = frame.AF.loopCount;
	}
	else
	{
		state.spriteId = -1;
		render.DontDraw();
	}
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
	state.currentTick = 0;  // Reset tick when changing pattern
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

	// Sync ticks when seeking with keyboard
	state.currentTick = CalculateTickFromFrame(&active->frameData, state.pattern, state.frame);
}

void MainFrame::UpdateBackProj(float x, float y)
{
	render.UpdateProj(x, y);
	glViewport(0, 0, x, y);
}

void MainFrame::HandleMouseDown(bool dragRight, bool dragLeft)
{
	auto* view = getActiveView();
	// Stage tab pans with LEFT-mouse drag, matching the character tabs'
	// pan convention (the editor is consistent: left-drag = pan
	// everywhere). u4ick used right-mouse, but in-editor consistency
	// wins over matching bgmaketool's input scheme.
	if (view && view->isStageView() && dragLeft)
		bgCamera.BeginDrag();
}

void MainFrame::HandleMouseUp(bool dragRight, bool dragLeft)
{
	if (dragRight && m_boxDragCharacter) EndBoxDrag();
	if (dragLeft && m_posDrag.active) EndPositionDrag(false);
	auto* view = getActiveView();
	if (view && view->isStageView() && dragLeft) {
		bgCamera.EndDrag();
		// panX is the true final camera position; panLast is still
		// easing toward it via Settle(). Persist panX so a tab switch
		// restores the settled position, not a mid-ease one.
		view->setStageRenderXY(bgCamera.panX, bgCamera.panY);
	}
}

void MainFrame::HandleMouseDrag(int x_, int y_, bool dragRight, bool dragLeft)
{
	auto* view = getActiveView();
	if (!view) return;

	// Stage tab: left-drag pans the bg camera. We accumulate the live
	// delta into bgCamera.pan while panLast stays put — that's u4ick's
	// movingPoint / movingPoint_last pair, and it's what makes the
	// parallax preview kick in for the duration of the drag (sprites
	// with parallax > 256 shift faster than the camera, < 256 slower).
	// On mouse-up HandleMouseUp calls EndDrag and Settle() eases the
	// parallax delta back to zero.
	if (view->isStageView()) {
		if (dragLeft) {
			bgCamera.panX += x_ / render.scale;
			bgCamera.panY += y_ / render.scale;
		}
		return;
	}

	auto* active = getActiveCharacter();
	if (!active) return;

	// A position-tool drag owns the left button (no panning while it runs).
	if (dragLeft && m_posDrag.active)
	{
		PositionDragBy(x_, y_);
		return;
	}

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
	if (!view) return;
	// Stage tab uses RMB for camera pan (handled in HandleMouseDown/Drag);
	// no box-start here.
	if (view->isStageView()) return;

	auto* active = getActiveCharacter();
	if (!active) return;

	auto* boxPane = view->getBoxPane();
	if (!boxPane) return;

	// The whole right-drag (start + every drag update) is one undo step,
	// closed in HandleMouseUp / CancelViewportGestures.
	if (m_boxDragCharacter) EndBoxDrag();
	active->undoManager.beginTransaction("Draw box");
	m_boxDragCharacter = active;

	boxPane->BoxStart((x_ - active->renderX - clientRect.x/2)/render.scale,
	                  (y_ - active->renderY - clientRect.y/2)/render.scale);
}

// HandleKeys() and the shortcut/undo/transport/position-tool handlers live
// in ui/editor_tools_impl.h.

void MainFrame::ChangeClearColor(float r, float g, float b)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
}

void MainFrame::SetZoom(float level)
{
	render.scale = level;
	zoom_idx = level;  // Keep global zoom_idx for saving to settings
	
	// Also update active view's zoom
	auto* view = getActiveView();
	if (view) {
		view->setZoom(level);
	}
}

void MainFrame::HandleMouseWheel(bool isIncrease, int mouseX, int mouseY)
{
	auto* view = getActiveView();
	if (!view) return;

	if (view->isStageView())
	{
		// Smooth zoom centered on the cursor. screen = (world + pan) *
		// scale, so the world point currently under the cursor is
		// world = cursor/scale - pan. We capture that point and pin it
		// under the cursor while DrawBack eases render.scale -> target.
		float s = render.scale > 0.0f ? render.scale : 1.0f;
		bgZoomAnchorWorldX = mouseX / s - bgCamera.panLastX;
		bgZoomAnchorWorldY = mouseY / s - bgCamera.panLastY;
		bgZoomAnchorScrnX  = (float)mouseX;
		bgZoomAnchorScrnY  = (float)mouseY;

		if (!bgZoomAnimating) bgZoomTarget = render.scale;
		bgZoomTarget *= isIncrease ? 1.15f : (1.0f / 1.15f);
		if (bgZoomTarget > 20.0f)  bgZoomTarget = 20.0f;
		if (bgZoomTarget < 0.25f)  bgZoomTarget = 0.25f;
		bgZoomAnimating = true;
		return;
	}

	// Character / PAT views — unchanged stepped zoom.
	float currentZoom = view->getZoom();
	float newZoomVal = currentZoom;

	if (isIncrease)
	{
		newZoomVal += 0.25f;  // Smaller increment for smoother zooming
		if (newZoomVal > 20.0f) newZoomVal = 20.0f;  // Match slider max
	}
	else
	{
		newZoomVal -= 0.25f;
		if (newZoomVal < 0.25f) newZoomVal = 0.25f;  // Match slider min
	}
	SetZoom(newZoomVal);
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


#endif /* UI_MAIN_UI_IMPL_H_GUARD */
