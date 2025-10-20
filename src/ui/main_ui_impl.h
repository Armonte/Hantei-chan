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
								tryLoadEffectCharacter(characters.back().get());
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
								tryLoadEffectCharacter(characters.back().get());
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
								tryLoadEffectCharacter(characters.back().get());
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
						render.SetParts(nullptr);
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
					render.SetParts(nullptr);
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
		// Begin undo frame - save snapshot BEFORE any modifications
		auto* character = view->getCharacter();
		if (character && character->undoManager.isEnabled()) {
			int patternIndex = view->getState().pattern;
			Sequence* seq = character->frameData.get_sequence(patternIndex);
			if (seq) {
				character->undoManager.beginFrame(patternIndex, *seq);
			}
		}

		// Set effectFrameData on all panes if effect.ha6 is loaded
		if (effectCharacter) {
			if (view->getMainPane()) view->getMainPane()->setEffectFrameData(&effectCharacter->frameData);
			if (view->getRightPane()) view->getRightPane()->setEffectFrameData(&effectCharacter->frameData);
			if (view->getBoxPane()) view->getBoxPane()->setEffectFrameData(&effectCharacter->frameData);
		}

		if (view->getMainPane()) view->getMainPane()->Draw();
		if (view->getRightPane()) view->getRightPane()->Draw();
		if (view->getBoxPane()) view->getBoxPane()->Draw();

		// Draw PatEditor panes if this is a PAT editor view
		if (view->isPatEditor()) {
			if (view->getPartSetPane()) view->getPartSetPane()->Draw();
			if (view->getPartPane()) view->getPartPane()->Draw();
			if (view->getShapePane()) view->getShapePane()->Draw();
			if (view->getTexturePane()) view->getTexturePane()->Draw();
			if (view->getToolPane()) view->getToolPane()->Draw();
		}

		// End undo frame - commit snapshot if anything was modified
		if (character) {
			character->undoManager.endFrame();
		}
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
				printf("[ANIM START] Resetting visit counts (play pressed on frame %d)\n", state.frame);
				state.frameVisitCounts.clear();
				state.activeSpawns.clear();  // Also clear any stale spawns from previous run
				state.lastSpawnCreationFrame = -1;
			}

			// Initialize visit count for starting frame if needed
			if (state.frameVisitCounts.empty()) {
				state.frameVisitCounts[state.frame] = 1;

				// Check for spawns on frame 0 at animation start
				if (state.vizSettings.showSpawnedPatterns && state.frame >= 0 && state.frame < seq->frames.size()) {
					auto& frame0 = seq->frames[state.frame];
					if (!frame0.EF.empty()) {
						// DEBUG: Log frame 0 spawn creation
						printf("[SPAWN CREATE - ANIM START] Frame %d, Tick %d, Pattern %d\n", state.frame, state.currentTick, state.pattern);

						state.lastSpawnCreationFrame = state.frame;
						auto spawns = ParseSpawnedPatterns(frame0.EF, state.frame, state.pattern);

						// DEBUG: Log how many spawns
						printf("[SPAWN CREATE - ANIM START] Parsed %d spawn(s)\n", (int)spawns.size());

						for(const auto& spawnInfo : spawns) {
							// DEBUG: Log which pattern is being spawned
							printf("[SPAWN CREATE - ANIM START]   -> Spawning pattern %d at tick %d\n", spawnInfo.patternId, state.currentTick);

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
							instance.tintColor = state.vizSettings.enableTint ? spawnInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
							instance.alpha = state.vizSettings.spawnedOpacity;
							instance.currentFrame = 0;
							instance.frameDuration = 0;
							instance.previousFrame = 0;

							if (!instance.isPresetEffect) {
								FrameData* sourceData = instance.usesEffectHA6 && effectCharacter
									? &effectCharacter->frameData
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
												FrameData* childSourceData = childInstance.usesEffectHA6 && effectCharacter
													? &effectCharacter->frameData
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

						// Detect loop: if we jumped backwards to frame 0, reset visit counts for new playthrough
						if (state.frame == 0 && oldFrame > state.frame) {
							// DEBUG: Log loop detection
							printf("[LOOP DETECTED] Resetting visit counts (jumped from frame %d to 0)\n", oldFrame);
							state.frameVisitCounts.clear();
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
						if (state.vizSettings.showSpawnedPatterns && state.frame >= 0 && state.frame < seq->frames.size()) {
							auto& newFrame = seq->frames[state.frame];
							bool isFirstVisit = (state.frameVisitCounts[state.frame] == 1);
							bool shouldCreateSpawns = !newFrame.EF.empty() && isFirstVisit &&
							                          (state.lastSpawnCreationFrame != state.frame);

							if(shouldCreateSpawns)
							{
								// DEBUG: Log spawn creation
								printf("[SPAWN CREATE] Frame %d, Tick %d, Pattern %d\n", state.frame, state.currentTick, state.pattern);

								// Mark that this frame has created spawns
								state.lastSpawnCreationFrame = state.frame;
								// Parse spawn effects in current frame
								auto spawns = ParseSpawnedPatterns(newFrame.EF, state.frame, state.pattern);

								// DEBUG: Log how many spawns were parsed
								printf("[SPAWN CREATE] Parsed %d spawn(s) from frame %d\n", (int)spawns.size(), state.frame);

								// Create active spawn instances for each spawn effect
								for(const auto& spawnInfo : spawns) {
									// DEBUG: Log which pattern is being spawned
									printf("[SPAWN CREATE]   -> Spawning pattern %d at tick %d\n", spawnInfo.patternId, state.currentTick);

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
									instance.tintColor = state.vizSettings.enableTint ? spawnInfo.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
									instance.alpha = state.vizSettings.spawnedOpacity;

									// Initialize animation state for frame-by-frame advancement
									instance.currentFrame = 0;
									instance.frameDuration = 0;
									instance.previousFrame = 0;

									// Initialize loop counter from the spawned pattern's first frame
									if (!instance.isPresetEffect) {
										FrameData* sourceData = instance.usesEffectHA6 && effectCharacter
											? &effectCharacter->frameData
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
														FrameData* childSourceData = childInstance.usesEffectHA6 && effectCharacter
															? &effectCharacter->frameData
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

			// Advance all active spawned patterns frame-by-frame (same logic as main pattern)
			for (auto& spawn : state.activeSpawns) {
				// Skip preset effects (they don't have frames)
				if (spawn.isPresetEffect) continue;

				// Get the spawned pattern's sequence
				FrameData* sourceData = spawn.usesEffectHA6 && effectCharacter
					? &effectCharacter->frameData
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
					FrameData* sourceData = spawn.usesEffectHA6 && effectCharacter
						? &effectCharacter->frameData
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
									FrameData* childSourceData = childInstance.usesEffectHA6 && effectCharacter
										? &effectCharacter->frameData
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
													FrameData* grandchildSourceData = grandchildInstance.usesEffectHA6 && effectCharacter
														? &effectCharacter->frameData
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
		render.SwitchImage(state.spriteId);

		if(frame.AF.loopCount>0)
			loopCounter = frame.AF.loopCount;
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
		case 'Z':
			// Ctrl+Z: undo
			if (auto* active = getActiveCharacter()) {
				auto* view = getActiveView();
				if (view) {
					int patternIndex = view->getState().pattern;
					Sequence* currentSeq = active->frameData.get_sequence(patternIndex);
					if (currentSeq) {
						// Make a copy of current sequence BEFORE calling undo
						Sequence currentSeqCopy = *currentSeq;

						active->undoManager.setEnabled(false);
						SequenceSnapshot* snapshot = active->undoManager.undo(patternIndex, currentSeqCopy);
						if (snapshot) {
							Sequence* targetSeq = active->frameData.get_sequence(snapshot->patternIndex);
							if (targetSeq) {
								*targetSeq = snapshot->sequence;
								
								// Clear pending snapshot to prevent committing stale state
								active->undoManager.clearPending();

								// Check if we're at clean state
								if (active->undoManager.isAtCleanState()) {
									// Clear both character and pattern modified flags
									active->clearModified();
									targetSeq->modified = false;
								} else {
									// Mark both character and pattern as modified
									active->markModified();
									active->frameData.mark_modified(snapshot->patternIndex);
								}

								// Validate frame index after undo
								auto& state = view->getState();
								int frameCount = targetSeq->frames.size();
								if (state.frame >= frameCount) {
									state.frame = frameCount > 0 ? frameCount - 1 : 0;
								}

								// Force sprite reload by resetting spriteId
								state.spriteId = -1;

								// Force spawn tree rebuild on next frame
								state.forceSpawnTreeRebuild = true;

								// Refresh main pane to update pattern names
								if (view->getMainPane()) {
									view->getMainPane()->RegenerateNames();
								}
							}
						}
						active->undoManager.setEnabled(true);
					}
				}
				return true;
			}
			break;
		case 'Y':
			// Ctrl+Y: redo
			if (auto* active = getActiveCharacter()) {
				auto* view = getActiveView();
				if (view) {
					int patternIndex = view->getState().pattern;
					Sequence* currentSeq = active->frameData.get_sequence(patternIndex);
					if (currentSeq) {
						// Make a copy of current sequence BEFORE calling redo
						Sequence currentSeqCopy = *currentSeq;

						active->undoManager.setEnabled(false);
						SequenceSnapshot* snapshot = active->undoManager.redo(patternIndex, currentSeqCopy);
						if (snapshot) {
							Sequence* targetSeq = active->frameData.get_sequence(snapshot->patternIndex);
							if (targetSeq) {
								*targetSeq = snapshot->sequence;
								
								// Clear pending snapshot to prevent committing stale state
								active->undoManager.clearPending();

								// Check if we're at clean state
								if (active->undoManager.isAtCleanState()) {
									// Clear both character and pattern modified flags
									active->clearModified();
									targetSeq->modified = false;
								} else {
									// Mark both character and pattern as modified
									active->markModified();
									active->frameData.mark_modified(snapshot->patternIndex);
								}

								// Validate frame index after redo
								auto& state = view->getState();
								int frameCount = targetSeq->frames.size();
								if (state.frame >= frameCount) {
									state.frame = frameCount > 0 ? frameCount - 1 : 0;
								}

								// Force sprite reload by resetting spriteId
								state.spriteId = -1;

								// Force spawn tree rebuild on next frame
								state.forceSpawnTreeRebuild = true;

								// Refresh main pane to update pattern names
								if (view->getMainPane()) {
									view->getMainPane()->RegenerateNames();
								}
							}
						}
						active->undoManager.setEnabled(true);
					}
				}
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
	zoom_idx = level;  // Keep global zoom_idx for saving to settings
	
	// Also update active view's zoom
	auto* view = getActiveView();
	if (view) {
		view->setZoom(level);
	}
}

void MainFrame::HandleMouseWheel(bool isIncrease)
{
	auto* view = getActiveView();
	if (!view) return;
	
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
