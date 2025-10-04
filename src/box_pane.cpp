#include "box_pane.h"
#include <imgui.h>
#include <imgui_internal.h>

constexpr int boxLimit = 33; 

BoxPane::BoxPane(Render* render, FrameData *frameData, FrameState &state):
DrawWindow(render, frameData, state),
currentBox(0), highlight(false)
{
	//Init box names
	for(int i = 0; i < boxLimit; i++)
	{
		if(i==0)
			boxNameList[i] = "Collision box";
		else if (i >= 1 && i <= 8)
			boxNameList[i] = "Hurtbox " + std::to_string(i);
		else if(i >=9 && i <= 10)
			boxNameList[i] = "Special box " + std::to_string(i-8);
		else if(i == 11)
			boxNameList[i] = "Clash box";
		else if(i == 12)
			boxNameList[i] = "Projectile box";
		else if(i>12 && i<=24)
			boxNameList[i] = "Special box " + std::to_string(i-8);
		else
			boxNameList[i] = "Attack box " + std::to_string(i-24);
	}
}

void BoxPane::BoxStart(int x, int y)
{
	auto seq = frameData->get_sequence(currState.pattern);
	if(seq)
	{
		auto &frames = seq->frames;
		if(frames.size()>0)
		{
			Hitbox &box = frames[currState.frame].hitboxes[currentBox];
			box.xy[0] = box.xy[2] = x;
			box.xy[1] = box.xy[3] = y;

			dragxy[0] = box.xy[0];
			dragxy[1] = box.xy[1];

			markModified();
		}
	}
}

void BoxPane::BoxDrag(int x, int y)
{
	auto seq = frameData->get_sequence(currState.pattern);
	if(seq)
	{
		auto &frames = seq->frames;
		if(frames.size()>0)
		{
			Hitbox &box = frames[currState.frame].hitboxes[currentBox];
			dragxy[0] += x/render->scale;
			dragxy[1] += y/render->scale;

			box.xy[2] = dragxy[0];
			box.xy[3] = dragxy[1];

			markModified();
		}
	}
}

void BoxPane::Draw()
{
	namespace im = ImGui;
	im::Begin("Box Pane",0);


	if(frameData->get_sequence(currState.pattern) && frameData->get_sequence(currState.pattern)->frames.size() > 0)
	{
		auto &frames = frameData->get_sequence(currState.pattern)->frames;
		BoxList &boxes = frames[currState.frame].hitboxes;

		// Box Controls section (collapsible)
		if(im::CollapsingHeader("Box Controls", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Helper lambda to check if a box exists (has non-zero coordinates)
			auto boxExists = [&boxes](int idx) -> bool {
				if(boxes.count(idx) == 0) return false;
				const auto& box = boxes.at(idx);
				return !(box.xy[0] == 0 && box.xy[1] == 0 && box.xy[2] == 0 && box.xy[3] == 0);
			};

			// Color definitions (matching render.cpp)
			const ImU32 whiteColor = IM_COL32(255, 255, 255, 255);
			const ImU32 greenColor = IM_COL32(51, 255, 51, 255);
			const ImU32 blueColor = IM_COL32(0, 0, 255, 255);
			const ImU32 yellowColor = IM_COL32(255, 255, 0, 255);
			const ImU32 cyanColor = IM_COL32(0, 255, 255, 255);
			const ImU32 purpleColor = IM_COL32(128, 0, 255, 255);
			const ImU32 redColor = IM_COL32(255, 51, 51, 255);

			// Helper to draw colored box buttons
			auto DrawBoxes = [&](int startIdx, int count, ImU32 color, float& xPos) {
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				const ImVec2 smallBoxSize(12, 12);
				const float spacing = 2.0f;

				ImVec2 pos = ImVec2(xPos, window->DC.CursorPos.y);
				for(int i = 0; i < count; i++) {
					int boxIdx = startIdx + i;

					ImGui::PushID(boxIdx);
					ImVec2 boxMin = pos;
					ImVec2 boxMax = ImVec2(pos.x + smallBoxSize.x, pos.y + smallBoxSize.y);

					bool hovered = ImGui::IsMouseHoveringRect(boxMin, boxMax);
					bool clicked = hovered && ImGui::IsMouseClicked(0);

					if(clicked) {
						currentBox = boxIdx;
					}

					// Determine color
					ImU32 finalColor = color;
					if(boxIdx == currentBox) {
						// Selected box = #f07fd7 (matches highlight feature in viewer)
						finalColor = IM_COL32(0xf0, 0x7f, 0xd7, 255);
					} else if(!boxExists(boxIdx)) {
						ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
						c.x *= 0.3f; c.y *= 0.3f; c.z *= 0.3f;
						finalColor = ImGui::ColorConvertFloat4ToU32(c);
					}

					window->DrawList->AddRectFilled(boxMin, boxMax, finalColor);
					window->DrawList->AddRect(boxMin, boxMax, IM_COL32(128, 128, 128, 255));

					if(hovered) {
						ImGui::SetTooltip("%s", boxNameList[boxIdx].c_str());
					}

					pos.x += smallBoxSize.x + spacing;
					ImGui::PopID();
				}

				xPos = pos.x + 8.0f; // Extra spacing between groups
			};

			// Draw labels on first line
			im::Text("Col"); im::SameLine();
			im::Text("  Hurt"); im::SameLine();
			im::Text("       Spc1-2"); im::SameLine();
			im::Text(" Clash"); im::SameLine();
			im::Text("Proj"); im::SameLine();
			im::Text("   Spc5-16"); im::SameLine();
			im::Text("                          Atk");

			// Draw boxes on second line
			float xPos = ImGui::GetCursorScreenPos().x;
			DrawBoxes(0, 1, whiteColor, xPos);
			DrawBoxes(1, 8, greenColor, xPos);
			DrawBoxes(9, 2, blueColor, xPos);
			DrawBoxes(11, 1, yellowColor, xPos);
			DrawBoxes(12, 1, cyanColor, xPos);
			DrawBoxes(13, 12, purpleColor, xPos);
			DrawBoxes(25, 8, redColor, xPos);

			// Advance cursor past the boxes
			ImGui::GetCurrentWindow()->DC.CursorPos.y += 12 + ImGui::GetStyle().ItemSpacing.y;

			im::Separator();

			// Navigation arrows
			im::PushButtonRepeat(true);
			if(im::ArrowButton("##left", ImGuiDir_Left))
				AdvanceBox(-1);
			im::SameLine(0,im::GetStyle().ItemInnerSpacing.x);
			if(im::ArrowButton("##right", ImGuiDir_Right))
				AdvanceBox(+1);
			im::PopButtonRepeat();

			im::SameLine(0,20.f);
			if(im::Button("Copy all"))
			{
				// Manually copy map for cross-allocator support
				currState.copied->boxes.clear();
				for (const auto& pair : boxes) {
					currState.copied->boxes[pair.first] = pair.second;
				}
			}
			im::SameLine(0,20.f);
			if(im::Button("Paste all"))
			{
				// Manually copy map for cross-allocator support
				boxes.clear();
				for (const auto& pair : currState.copied->boxes) {
					boxes[pair.first] = pair.second;
				}
				frameData->mark_modified(currState.pattern);
				markModified();
			}

			im::SameLine(0,20.f);
			if(im::Button("Copy params"))
			{
				currState.copied->box = boxes[currentBox];
			}
			im::SameLine(0,20.f);
			if(im::Button("Paste params"))
			{
				boxes[currentBox] = currState.copied->box;
				frameData->mark_modified(currState.pattern);
				markModified();
			}

			im::Checkbox("Highlight selected", &highlight);
			im::SameLine(0,20.f);
			if(im::Button("Delete selected"))
			{
				boxes.erase(currentBox);
				frameData->mark_modified(currState.pattern);
				markModified();
			}

			if(highlight)
				render->highLightN = currentBox;
			else
				render->highLightN = -1;
		

			const int step = 1;
			if(im::InputScalarN("Top left", ImGuiDataType_S32, boxes[currentBox].xy, 2, &step, NULL, "%d", 0))
			{
				frameData->mark_modified(currState.pattern);
				markModified();
			}
			if(im::InputScalarN("Bottom right", ImGuiDataType_S32, boxes[currentBox].xy+2, 2, &step, NULL, "%d", 0))
			{
				frameData->mark_modified(currState.pattern);
				markModified();
			}
		} // End Box Controls

		// Draw spawn timeline
		im::Separator();
		DrawSpawnTimeline();
	}


	im::End();
}

void BoxPane::AdvanceBox(int dir)
{
	currentBox += dir;
	if(currentBox < 0)
		currentBox = boxLimit-1;
	else if(currentBox >= boxLimit)
		currentBox = 0;
}

void BoxPane::DrawSpawnTimeline()
{
	namespace im = ImGui;

	if(!currState.vizSettings.showSpawnedPatterns || currState.spawnedPatterns.empty()) {
		return;
	}

	if(!im::CollapsingHeader("Spawn Timeline")) {
		return;
	}

	// Get main pattern frame count
	auto mainSeq = frameData->get_sequence(currState.pattern);
	if(!mainSeq || mainSeq->frames.empty()) return;

	int mainFrameCount = mainSeq->frames.size();

	// Calculate total timeline extent (max spawn frame + max lifetime)
	int timelineEnd = mainFrameCount;
	for(const auto& sp : currState.spawnedPatterns) {
		int spawnEnd = sp.absoluteSpawnFrame + (sp.lifetime < 9999 ? sp.lifetime : sp.patternFrameCount);
		timelineEnd = std::max(timelineEnd, spawnEnd);
	}

	// Timeline settings
	const float rowHeight = 20.0f;
	const float frameWidth = 3.0f;
	const float labelWidth = 100.0f;
	const float timelineWidth = std::min(600.0f, frameWidth * timelineEnd);

	im::Text("Frame: %d / %d  |  Tick: %d", currState.frame, mainFrameCount - 1, currState.currentTick);

	// Add tick scrubber
	int maxTick = timelineEnd * 10;  // Rough estimate
	if (im::SliderInt("##tickscrub", &currState.currentTick, 0, maxTick, "Tick %d")) {
		// User scrubbed - update to non-animating mode and sync frame
		currState.animating = false;
		currState.frame = CalculateFrameFromTick(frameData, currState.pattern, currState.currentTick);
	}

	im::Separator();

	// Draw main pattern row
	ImDrawList* drawList = im::GetWindowDrawList();
	ImVec2 startPos = im::GetCursorScreenPos();
	float yPos = startPos.y;

	// Main pattern label
	im::Text("Main");
	im::SameLine(labelWidth);

	ImVec2 timelineStart = im::GetCursorScreenPos();

	// Main pattern timeline bar
	ImVec2 barMin = ImVec2(timelineStart.x, yPos);
	ImVec2 barMax = ImVec2(timelineStart.x + mainFrameCount * frameWidth, yPos + rowHeight);
	drawList->AddRectFilled(barMin, barMax, IM_COL32(100, 100, 255, 180));
	drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255));

	// Current frame indicator on main
	float currentX = timelineStart.x + currState.frame * frameWidth;
	drawList->AddLine(
		ImVec2(currentX, yPos),
		ImVec2(currentX, yPos + rowHeight),
		IM_COL32(255, 255, 0, 255),
		2.0f
	);

	yPos += rowHeight + 4;
	im::SetCursorScreenPos(ImVec2(startPos.x, yPos));

	// Draw spawned pattern rows
	for(size_t i = 0; i < currState.spawnedPatterns.size(); i++) {
		const auto& sp = currState.spawnedPatterns[i];

		// Get pattern name
		FrameData* sourceData = sp.usesEffectHA6 ? effectFrameData : frameData;
		std::string patternName = sourceData ? sourceData->GetDecoratedName(sp.patternId) : std::to_string(sp.patternId);

		// Indent based on depth
		float indent = sp.depth * 10.0f;

		// Label with depth indicator
		im::SetCursorScreenPos(ImVec2(startPos.x + indent, yPos));
		std::string label = std::string(sp.depth, '>') + " " + patternName;
		if(label.length() > 12) {
			label = label.substr(0, 12) + "..";
		}
		im::Text("%s", label.c_str());
		im::SameLine(labelWidth);

		// Timeline bar
		ImVec2 spawnBarMin = ImVec2(
			timelineStart.x + sp.absoluteSpawnFrame * frameWidth,
			yPos
		);
		int barLength = sp.lifetime < 9999 ? sp.lifetime : sp.patternFrameCount;
		ImVec2 spawnBarMax = ImVec2(
			timelineStart.x + (sp.absoluteSpawnFrame + barLength) * frameWidth,
			yPos + rowHeight
		);

		// Color from tint
		ImU32 barColor = IM_COL32(
			sp.tintColor.r * 255,
			sp.tintColor.g * 255,
			sp.tintColor.b * 255,
			180
		);

		drawList->AddRectFilled(spawnBarMin, spawnBarMax, barColor);
		drawList->AddRect(spawnBarMin, spawnBarMax, IM_COL32(255, 255, 255, 255));

		// Mark spawn point
		drawList->AddCircleFilled(
			ImVec2(timelineStart.x + sp.absoluteSpawnFrame * frameWidth, yPos + rowHeight/2),
			3.0f,
			IM_COL32(255, 255, 255, 255)
		);

		// Show if looping
		if(sp.lifetime >= 9999) {
			im::SameLine();
			im::TextColored(ImVec4(1, 1, 0.5f, 1), "Loop");
		}

		// Show if recursive
		if(sp.isRecursive) {
			im::SameLine();
			im::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "REC");
		}

		yPos += rowHeight + 2;
		im::SetCursorScreenPos(ImVec2(startPos.x, yPos));
	}

	// Draw global current frame line
	drawList->AddLine(
		ImVec2(currentX, startPos.y),
		ImVec2(currentX, yPos),
		IM_COL32(255, 255, 0, 128),
		1.0f
	);

	// Advance cursor past timeline
	im::SetCursorScreenPos(ImVec2(startPos.x, yPos + 10));
}