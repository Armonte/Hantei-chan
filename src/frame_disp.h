#include <imgui.h>
#include "framedata.h"

inline void FrameDisplay(Frame *frame)
{
	float width = 50.f;
	//ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Sprite", &frame->AF.spriteId);
	
	ImGui::Checkbox("Use .pat", &frame->AF.usePat); ImGui::SameLine(0.f, 20);
	ImGui::Checkbox("Interpolate", &frame->AF.interpolation);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Jump to", &frame->AF.jump, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Landing frame", &frame->AF.landJump, 0, 0);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Priority", &frame->AF.priority, 0, 0); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Loop N times", &frame->AF.loopCount, 0, 0); ImGui::SameLine(0,20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("End of loop", &frame->AF.loopEnd, 0, 0);

	

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("X", &frame->AF.offset_x, 0, 0);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Y", &frame->AF.offset_y, 0, 0);

	ImGui::InputInt("Duration", &frame->AF.duration, 0, 0);

	ImGui::InputInt("AniFlag", &frame->AF.aniFlag, 0, 0);

	ImGui::InputInt("Blend Mode", &frame->AF.blend_mode, 0, 0);
	ImGui::ColorEdit4("Color", frame->AF.rgba);

	ImGui::InputFloat3("Rot XYZ", frame->AF.rotation); 
	ImGui::InputFloat2("Scale", frame->AF.scale);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Boxes", &frame->nHitbox, 0, 0);
	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("FSNE", &frame->FSNE, 0, 0);
	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("FSNI", &frame->FSNI, 0, 0);

}