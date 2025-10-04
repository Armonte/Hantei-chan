#include "right_pane.h"
#include "frame_disp.h"
#include <imgui.h>

void RightPane::Draw()
{	
	ImGui::Begin("Right Pane", 0);
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
				EfDisplay(&frame.EF, &currState.copied->efSingle, frameData, currState.pattern, [this]() { markModified(); });
				if(ImGui::Button("Copy all")) {
					CopyVectorContents<Frame_EF>(currState.copied->efGroup, frame.EF);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste all")) {
					CopyVectorContents<Frame_EF>(frame.EF, currState.copied->efGroup);
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Add copy")) {
					frame.EF.push_back(currState.copied->efSingle);
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Conditions"))
			{
				IfDisplay(&frame.IF, &currState.copied->ifSingle, frameData, currState.pattern, [this]() { markModified(); });
				if(ImGui::Button("Copy all")) {
					CopyVectorContents<Frame_IF>(currState.copied->ifGroup, frame.IF);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste all")) {
					CopyVectorContents<Frame_IF>(frame.IF, currState.copied->ifGroup);
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Add copy")) {
					frame.IF.push_back(currState.copied->ifSingle);
					frameData->mark_modified(currState.pattern);
					markModified();
				}
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

					// Parse and accumulate spawned patterns from all frames
					if(vizSettings.autoDetect)
					{
						// Track when pattern changes to rebuild spawned patterns list
						static int lastPattern = -1;
						static int lastBuildFrame = -1;

						bool patternChanged = (lastPattern != currState.pattern);

						// Rebuild spawned patterns list when pattern changes
						if (patternChanged) {
							currState.spawnedPatterns.clear();
							lastPattern = currState.pattern;
							lastBuildFrame = -1;
						}

						// Parse current frame's effects and add to accumulated list
						if (!frame.EF.empty()) {
							auto newSpawns = ParseSpawnedPatterns(frame.EF, currState.frame);

							// Add new spawns that aren't already in the list
							for (const auto& newSpawn : newSpawns) {
								bool alreadyExists = false;
								for (const auto& existing : currState.spawnedPatterns) {
									if (existing.parentFrame == newSpawn.parentFrame &&
									    existing.effectIndex == newSpawn.effectIndex) {
										alreadyExists = true;
										break;
									}
								}
								if (!alreadyExists) {
									currState.spawnedPatterns.push_back(newSpawn);
								}
							}
						}

						// Display accumulated spawned patterns
						if(!currState.spawnedPatterns.empty())
						{
							ImGui::Text("Total spawned pattern(s): %d", (int)currState.spawnedPatterns.size());
							ImGui::Separator();

							for(size_t i = 0; i < currState.spawnedPatterns.size(); i++)
							{
								ImGui::PushID(static_cast<int>(i));

								const auto& sp = currState.spawnedPatterns[i];
								std::string patternName = frameData->GetDecoratedName(sp.patternId);

								if(ImGui::TreeNode("##spawned", "%d. Pattern %s (frame %d)", (int)i+1, patternName.c_str(), sp.parentFrame))
								{
									ImGui::Text("Effect Index: %d", sp.effectIndex);
									ImGui::Text("Offset: (%d, %d)", sp.offsetX, sp.offsetY);

									if(sp.randomRange > 0) {
										ImGui::Text("Random Range: %d", sp.randomRange);
									}

									if(sp.angle != 0) {
										float degrees = (sp.angle / 10000.0f) * 360.0f;
										ImGui::Text("Angle: %d (%.1f°)", sp.angle, degrees);
									}

									// Show some key flags
									if(sp.flagset1 & 0x4) {  // bit 2
										ImGui::BulletText("Follows parent");
									}
									if(sp.flagset1 & 0x10) { // bit 4
										ImGui::BulletText("Camera relative");
									}
									if(sp.flagset2 & 0x100) { // bit 8
										ImGui::BulletText("Relative to opponent");
									}

									ImGui::TreePop();
								}

								ImGui::PopID();
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

