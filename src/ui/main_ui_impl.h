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
		}

		if(state.animating)
		{
			state.animeSeq = state.pattern;

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
					state.frame = GetNextFrame(true);
					duration = 0;

					// Bounds check
					if(state.frame < 0 || state.frame >= seq->frames.size())
					{
						state.animating = false;
						state.frame = 0;
					}
				}
			}

			duration++;
			state.currentTick++;  // Increment tick counter for spawned pattern synchronization
		}

		auto &frame =  seq->frames[state.frame];

		// Check current frame for spawn effects (during animation)
		if(state.animating && state.vizSettings.showSpawnedPatterns && !frame.EF.empty())
		{
			// Parse spawn effects in current frame
			auto spawns = ParseSpawnedPatterns(frame.EF, state.frame, state.pattern);

			// Create active spawn instances for each spawn effect
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
				instance.tintColor = spawnInfo.tintColor;
				instance.alpha = state.vizSettings.spawnedOpacity;

				state.activeSpawns.push_back(instance);
			}
		}

		// Clean up finished spawns (remove non-looping patterns that have ended)
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

						// Determine which frameData to use
						FrameData* sourceData = spawn.usesEffectHA6 && effectCharacter
							? &effectCharacter->frameData
							: &active->frameData;

						auto spawnSeq = sourceData->get_sequence(spawn.patternId);
						if (!spawnSeq || spawnSeq->frames.empty()) return true; // Remove invalid

						// Calculate total duration
						int totalDuration = 0;
						for (auto& f : spawnSeq->frames) {
							totalDuration += (f.AF.duration > 0 ? f.AF.duration : 1);
						}

						// Check if looping
						bool isLooping = !spawnSeq->frames.empty() && spawnSeq->frames.back().AF.aniType == 2;

						// Keep if looping, remove if finished
						int elapsedTicks = state.currentTick - spawn.spawnTick;
						return !isLooping && elapsedTicks >= totalDuration;
					}),
				state.activeSpawns.end()
			);
		}

		// Render using flat AF fields (gonp original)
		state.spriteId = frame.AF.spriteId;
		render.GenerateHitboxVertices(frame.hitboxes);
		render.offsetX = (frame.AF.offset_x)*1;
		render.offsetY = (frame.AF.offset_y)*1;
		render.SetImageColor(frame.AF.rgba);
		render.rotX = frame.AF.rotation[0];
		render.rotY = frame.AF.rotation[1];
		render.rotZ = frame.AF.rotation[2];
		render.AFRT = frame.AF.AFRT;
		render.scaleX = frame.AF.scale[0];
		render.scaleY = frame.AF.scale[1];

		switch (frame.AF.blend_mode)
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


#endif /* UI_MAIN_UI_IMPL_H_GUARD */
