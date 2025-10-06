#include "right_pane.h"
#include "frame_disp.h"
#include <imgui.h>

void RightPane::Draw()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	ImGui::Begin("Right Pane", 0);
	ImGui::PopStyleVar();
	auto seq = frameData->get_sequence(currState.pattern);
	if(seq)
	{
		int nframes = seq->frames.size() - 1;
		if(nframes >= 0)
		{
			Frame &frame = seq->frames[currState.frame];
			if (ImGui::TreeNode("Attack data"))
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
				ImGui::TreePop();
				ImGui::Separator();
			}
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

