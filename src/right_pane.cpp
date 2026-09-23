#include "frame_disp/frame_disp_common.h"
#include "right_pane.h"
#include "frame_disp.h"
#include <imgui.h>

void DrawRecordNote(Ha6Notes& notes, const std::string& key);

void RightPane::Draw()
{
	// Track view changes to restore scroll position when switching tabs
	static FrameState* lastViewState = nullptr;
	bool viewJustChanged = (lastViewState != &currState);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	ImGui::Begin("Right Pane", 0);
	ImGui::PopStyleVar();

	// Restore scroll position when switching views
	if (viewJustChanged) {
		ImGui::SetScrollY(currState.rightPaneScrollY);
		lastViewState = &currState;
	}

	auto seq = frameData->get_sequence(currState.pattern);
	if(seq)
	{
		int nframes = seq->frames.size() - 1;
		if(nframes >= 0)
		{
			Frame &frame = seq->frames[currState.frame];
			// The attack record (ATST) is written only for frames that have an
			// attack box; everything else is "null" attack data to the game, no
			// matter what the fields below say. Show which one this frame is (#79).
			const bool atSaved = frame.hitboxes.lower_bound(25) != frame.hitboxes.end();
			const char* atLabel = atSaved ? "Attack data###AttackData"
			                              : "Attack data (null: no attack box)###AttackData";
			if (!atSaved) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			const bool atOpen = ImGui::TreeNode(atLabel);
			if (!atSaved) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(atSaved
					? "This frame has an attack box, so its attack data is saved."
					: "No attack box on this frame: the attack data is not saved,\n"
					  "and the game treats the frame as having no attack properties.\n"
					  "Add an attack box (25+) to make it active.");
			if (atOpen)
			{
				AtDisplay(&frame.AT, frameData, currState.pattern, [this]() { markModified(); });
				if(ImGui::Button("Copy AT")) {
					currState.copied->at = frame.AT;
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste AT")) {
					frame.AT = currState.copied->at;
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Reset AT")) {
					frame.AT = Frame_AT{};
					frame.AT.correction = 100; // what a freshly parsed ATST starts from
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reset every attack field to its default.");
				ImGui::TreePop();
				ImGui::Separator();
			}
			// Record annotations (issue #58): shown under each effect/condition
			// header; right-click the header to add or edit one.
			const int notePattern = currState.pattern, noteFrame = currState.frame;
			CurrentRecordNoteHook().draw = [this, notePattern, noteFrame](bool isEffect, int index, int type) {
				DrawRecordNote(frameData->notes, Ha6Notes::RecordKey(notePattern, noteFrame, isEffect, index, type));
			};
			if(ImGui::TreeNode("Effects"))
			{
				EfDisplay(&frame.EF, &currState.copied->efSingle, frameData, currState.pattern, [this]() { markModified(); }, &currState.copied->efGroup);
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Conditions"))
			{
				IfDisplay(&frame.IF, &currState.copied->ifSingle, frameData, currState.pattern, [this]() { markModified(); }, &currState.copied->ifGroup);
				ImGui::TreePop();
				ImGui::Separator();
			}
			CurrentRecordNoteHook().draw = nullptr;

			// Spawned Patterns Visualization
			if(ImGui::TreeNode("Spawned Patterns Visualization"))
			{
				auto& vizSettings = currState.vizSettings;

				// Master toggle
				if(ImGui::Checkbox("Show spawned patterns", &vizSettings.showSpawnedPatterns)) {
					// Update visualization when toggled
				}

				if(vizSettings.showSpawnedPatterns)
				{
					ImGui::Checkbox("Auto-detect from effects", &vizSettings.autoDetect);
					ImGui::Checkbox("Show offset lines", &vizSettings.showOffsetLines);
					ImGui::Checkbox("Show pattern labels", &vizSettings.showLabels);
					ImGui::Checkbox("Show preset effects (Type 3)", &vizSettings.showPresetEffects);

					// Indented option for preset effects
					if (vizSettings.showPresetEffects) {
						ImGui::Indent();
						ImGui::Checkbox("Show on all frames", &vizSettings.presetEffectsAllFrames);
						ImGui::Unindent();
					}

					ImGui::SliderFloat("Opacity", &vizSettings.spawnedOpacity, 0.0f, 1.0f, "%.2f");
					ImGui::Checkbox("Enable color tint", &vizSettings.enableTint);

					ImGui::Separator();
					ImGui::Text("Animation:");
					if(ImGui::RadioButton("Sync with main", vizSettings.animateWithMain)) {
						vizSettings.animateWithMain = true;
					}
					ImGui::SameLine();
					if(ImGui::RadioButton("Independent", !vizSettings.animateWithMain)) {
						vizSettings.animateWithMain = false;
					}

					ImGui::Separator();

					// Build recursive spawn tree for entire pattern
					if(vizSettings.autoDetect)
					{
						// Track when pattern changes to rebuild spawn tree
						static int lastPattern = -1;

						auto seq = frameData->get_sequence(currState.pattern);
						bool patternChanged = (lastPattern != currState.pattern);
						bool patternModified = seq && seq->modified;
						bool forceRebuild = currState.forceSpawnTreeRebuild;

						// Rebuild spawn tree when pattern changes OR modified OR forced by undo/redo
						if (patternChanged || patternModified || forceRebuild) {
							currState.spawnedPatterns.clear();
							lastPattern = currState.pattern;
							currState.forceSpawnTreeRebuild = false;

							// Build full recursive spawn tree
							std::set<int> visitedPatterns;
							BuildSpawnTreeRecursive(
								frameData,                // Main character frame data
								effectFrameData,          // Effect.ha6 frame data (can be null)
								currState.pattern,        // Root pattern to analyze
								false,                    // Root pattern is from main character
								-1,                       // No parent (root level)
								0,                        // Depth 0
								0,                        // Starts at absolute frame 0
								0,                        // Starts at tick 0
								0,                        // Accumulated offset X (starts at 0)
								0,                        // Accumulated offset Y (starts at 0)
								currState.spawnedPatterns,
								visitedPatterns);
						}

						// Display spawned patterns as hierarchical tree
						if(!currState.spawnedPatterns.empty())
						{
							ImGui::Text("Total spawned pattern(s): %d", (int)currState.spawnedPatterns.size());
							ImGui::Separator();

							// Display only root-level spawns (parentSpawnIndex == -1)
							// Children will be displayed recursively
							int displayNumber = 1;
							for(size_t i = 0; i < currState.spawnedPatterns.size(); i++)
							{
								const auto& sp = currState.spawnedPatterns[i];
								// Skip Effect Type 3 (preset effects) - they don't spawn patterns
								if(sp.isPresetEffect) continue;
								if(sp.parentSpawnIndex == -1)
								{
									DisplaySpawnNode(static_cast<int>(i), displayNumber++);
								}
							}
						}
						else
						{
							ImGui::TextDisabled("No spawned patterns found");
						}
					}
					else if(!vizSettings.autoDetect)
					{
						ImGui::TextDisabled("Auto-detect disabled");
					}
				}

				ImGui::TreePop();
				ImGui::Separator();
			}
		}
	}

	// Save scroll position before ending
	currState.rightPaneScrollY = ImGui::GetScrollY();

	ImGui::End();
}

// Recursively display a spawn node and its children
void RightPane::DisplaySpawnNode(int spawnIndex, int displayNumber)
{
	if(spawnIndex < 0 || spawnIndex >= currState.spawnedPatterns.size()) {
		return;
	}

	const auto& sp = currState.spawnedPatterns[spawnIndex];

	// Skip Effect Type 3 (preset effects) - they don't spawn patterns
	if(sp.isPresetEffect) return;

	// Get pattern name
	FrameData* sourceData = sp.usesEffectHA6 ? effectFrameData : frameData;
	std::string patternName = sourceData ? sourceData->GetDecoratedName(sp.patternId) : std::to_string(sp.patternId);

	ImGui::PushID(spawnIndex);

	// Color code by depth
	ImVec4 depthColor = ImVec4(sp.tintColor.r, sp.tintColor.g, sp.tintColor.b, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, depthColor);

	// Build tree node label with hierarchy info
	bool hasChildren = !sp.childSpawnIndices.empty();
	const char* recursiveMarker = sp.isRecursive ? " [RECURSIVE]" : "";

	bool nodeOpen = ImGui::TreeNode("##spawned", "%d. Pattern %s @ frame %d%s%s",
		displayNumber,
		patternName.c_str(),
		sp.absoluteSpawnFrame,
		hasChildren ? " ↓" : "",
		recursiveMarker);

	ImGui::PopStyleColor();

	if(nodeOpen)
	{
		// Display spawn details
		ImGui::Text("Depth: %d", sp.depth);
		ImGui::Text("Spawned by: Effect %d (type %d%s)",
			sp.effectIndex,
			sp.effectType,
			sp.usesEffectHA6 ? " - effect.ha6" : "");
		ImGui::Text("Parent frame: %d", sp.parentFrame);
		ImGui::Text("Absolute spawn frame: %d", sp.absoluteSpawnFrame);
		ImGui::Text("Pattern frames: %d", sp.patternFrameCount);

		if(sp.lifetime < 9999) {
			ImGui::Text("Lifetime: %d frames", sp.lifetime);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Lifetime: Looping");
		}

		ImGui::Text("Offset: (%d, %d)", sp.offsetX, sp.offsetY);

		if(sp.randomRange > 0) {
			ImGui::Text("Random Range: %d", sp.randomRange);
		}

		if(sp.angle != 0) {
			float degrees = (sp.angle / 10000.0f) * 360.0f;
			ImGui::Text("Angle: %d (%.1f°)", sp.angle, degrees);
		}

		// Show key flags
		if(sp.flagset1 & 0x4) {
			ImGui::BulletText("Follows parent");
		}
		if(sp.flagset1 & 0x10) {
			ImGui::BulletText("Camera relative");
		}
		if(sp.flagset2 & 0x100) {
			ImGui::BulletText("Relative to opponent");
		}

		// Recursively display children
		if(hasChildren)
		{
			ImGui::Separator();
			ImGui::Text("Children (%d):", (int)sp.childSpawnIndices.size());
			ImGui::Indent();

			int childDisplayNum = 1;
			for(int childIdx : sp.childSpawnIndices)
			{
				DisplaySpawnNode(childIdx, childDisplayNum++);
			}

			ImGui::Unindent();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}


// A note under a record header: its text (like a code comment) and a context
// menu on the header to edit or remove it.
void DrawRecordNote(Ha6Notes& notes, const std::string& key)
{
	const std::string* note = notes.get(key);
	static char buf[1024];
	ImGui::PushID(key.c_str());
	if (ImGui::BeginPopupContextItem("##notectx")) {
		if (ImGui::IsWindowAppearing()) snprintf(buf, sizeof(buf), "%s", note ? note->c_str() : "");
		ImGui::TextDisabled("Note (saved beside the HA6, not in it)");
		ImGui::InputTextMultiline("##note", buf, sizeof(buf), ImVec2(360, 80));
		if (ImGui::Button("Save note")) { notes.set(key, buf); ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (note && ImGui::Button("Remove note")) { notes.set(key, ""); ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	} else if (ImGui::IsItemHovered() && !note) {
		ImGui::SetItemTooltip("Right-click to add a note");
	}
	if (note) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.65f, 0.35f, 1.0f));
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(("// " + *note).c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}
	ImGui::PopID();
}
