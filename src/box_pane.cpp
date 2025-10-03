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
					finalColor = IM_COL32(255, 255, 255, 255);
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