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
	SyncWorkspaceSession();
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

		// View tabs of the main window (detached windows draw their own; see
		// ui/workspace_hosts_impl.h). Order and selection come from the
		// workspace session; tabs can be dragged between windows.
		const ImVec2 mainBarMin = ImGui::GetCursorScreenPos();
		if (ImGui::BeginTabBar("##character_tabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
			DrawHostTabs(WorkspaceSession::MainHost);

			// Right-click context menu popup
			if (ImGui::BeginPopup("ViewContextMenu")) {
				DrawViewTabContextItems();
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
		DrawTabBarDropTarget(WorkspaceSession::MainHost,
			ImRect(mainBarMin, ImVec2(mainBarMin.x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y)));

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
		// Undo: remember where the user is so undo can navigate back.
		if (auto* character = view->getCharacter())
			character->undoManager.noteFocus(view->getState().pattern, view->getState().frame);

		// This view's surface owns focus-scoped shortcuts for the next key
		// messages (a later claim in the frame - a focused detached window, a
		// command workspace - wins).
		shortcuts.claimFocus(view->isStageView() ? ShortcutContext::stageView
			: view->isPatEditor() ? ShortcutContext::patEditor
			: ShortcutContext::characterView, view->getId());

		DrawViewPanes(view, std::string());
	}

	// Detached (multi-monitor) windows, their panes and tab moves.
	DrawDetachedHosts();
	ApplyPendingTabActions();
	FinishTabDrag();
	FinishPaneUndoFrame();
	DrawRenderToolWindows();

	aboutWindow.Draw();
	vectors.Draw();
	drawCommandEditor();
	drawGameLink();

	// Background (stage) Inspector — shows the currently loaded stage's
	// objects, lets you scrub through frames, and exposes editable fields.
	// Only visible when a stage is loaded.
	if (currentBgFile && currentBgFile->IsLoaded())
	{
		// FirstUseEver = land at a sensible default the first time the user
		// sees the window, but let them move it freely after that (and the
		// position persists in imgui's .ini between runs, like every other
		// pane in the editor).
		// Relative to the main viewport: with detachable windows enabled,
		// window positions are desktop coordinates, and a fixed (80, 80)
		// could open the inspector as its own OS window off the main one.
		const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
		ImGui::SetNextWindowPos(ImVec2(mainPos.x + 80.0f, mainPos.y + 80.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Background Inspector", nullptr, 0);
		// Editing here is stage editing: route Ctrl+Z/Y/S to the stage.
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			shortcuts.claimFocus(ShortcutContext::stageView, getActiveView() ? getActiveView()->getId() : 0);

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
		ImGui::Text("view zoom=%.2f pass x=%d y=%d",
		            getActiveView() ? getActiveView()->getZoom() : 0.f, render.x, render.y);
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
			const float vz = (getActiveView() && getActiveView()->getZoom() > 0.f) ? getActiveView()->getZoom() : 1.f;
			float newPanX = wantScreenX / vz - (float)f0.offsetX;
			float newPanY = wantScreenY / vz - (float)f0.offsetY;
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

		// Game flavour / seed / side files / object, frame and event-record
		// editing live in bg_inspector.cpp.
		bg::InspectorResult bgRes = bg::DrawInspector(*currentBgFile, bgRenderer);

		ImGui::End();
		if (!bgRes.openPath.empty()) loadStageFile(bgRes.openPath);
	}

	DrawPositionTool();
	shortcuts.endFrame();
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
	AdvanceViewPlayback(view);
	if((seq = active->frameData.get_sequence(state.pattern)) &&
		seq->frames.size() > 0)
	{

		auto &frame =  seq->frames[state.frame];

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

	}
	else
	{
		state.spriteId = -1;
		render.DontDraw();
	}
}

// One game tick of playback for one view (no GL). Playback is driven by the
// preview tick simulator: the root frame comes from the simulated runtime
// flow (engine loop rules, EF re-fire on every frame entry) and spawned
// actors are read from the same simulation when the view renders. When the
// root has ended and every spawned actor is gone, playback wraps to tick 0.
void MainFrame::AdvanceViewPlayback(CharacterView* view)
{
	CharacterInstance* active = view ? view->getCharacter() : nullptr;
	if (!active || view->isStageView()) return;
	auto& state = view->getState();
	Sequence* seq = active->frameData.get_sequence(state.pattern);
	if (!seq || seq->frames.empty()) return;
	state.animeSeq = state.pattern;
	if (state.animating) {
		FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
		auto& sim = state.BindPreviewSim(&active->frameData, effectData);
		int next = state.currentTick + 1;
		const int settled = sim.settledTick();          // cached after the first call
		const int wrapAt = settled >= 0 ? settled : sim.horizon();
		if (next > wrapAt) next = 0;
		sim.ensureSimulatedTo(next);
		const auto& track = sim.rootFrameTrack();
		state.currentTick = next;
		if (next < (int)track.size())
			state.frame = track[next];
	}
	if (state.frame < 0 || state.frame >= (int)seq->frames.size())
		state.frame = std::clamp(state.frame, 0, (int)seq->frames.size() - 1);
}

void MainFrame::AdvancePattern(CharacterView* view, int dir)
{
	auto* active = view ? view->getCharacter() : nullptr;
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

void MainFrame::AdvanceFrame(CharacterView* view, int dir)
{
	auto* active = view ? view->getCharacter() : nullptr;
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
			const float z = view->getZoom() > 0.f ? view->getZoom() : 1.f;
			bgCamera.panX += x_ / z;
			bgCamera.panY += y_ / z;
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
		const float z = view->getZoom() > 0.f ? view->getZoom() : 1.f;
		if (view->getBoxPane()) view->getBoxPane()->BoxDragWorld(x_ / z, y_ / z);
	}
	else if(dragLeft)
	{
		// Pan this view's own camera; the character keeps the last pan as
		// the starting camera for views opened later.
		auto& cam = view->camera();
		cam.panX += x_;
		cam.panY += y_;
		active->renderX = (int)cam.panX;
		active->renderY = (int)cam.panY;
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

	const auto& cam = view->camera();
	boxPane->BoxStart((x_ - cam.panX - clientRect.x/2)/cam.zoom,
	                  (y_ - cam.panY - clientRect.y/2)/cam.zoom);
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
		float s = view->getZoom() > 0.0f ? view->getZoom() : 1.0f;
		bgZoomAnchorWorldX = mouseX / s - bgCamera.panLastX;
		bgZoomAnchorWorldY = mouseY / s - bgCamera.panLastY;
		bgZoomAnchorScrnX  = (float)mouseX;
		bgZoomAnchorScrnY  = (float)mouseY;

		if (!bgZoomAnimating) bgZoomTarget = view->getZoom();
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
	// Detached native windows cannot rely on the main OS window to mask
	// rounded or translucent ImGui window corners.
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
}


#endif /* UI_MAIN_UI_IMPL_H_GUARD */
