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
				AtDisplay(&frame.AT, frameData, currState.pattern);
				if(ImGui::Button("Copy AT")) {
					currState.copied->at = frame.AT;
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste AT")) {
					frame.AT = currState.copied->at;
					frameData->mark_modified(currState.pattern);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Effects"))
			{
				EfDisplay(&frame.EF, &currState.copied->efSingle, frameData, currState.pattern);
				if(ImGui::Button("Copy all")) {
					CopyVectorContents<Frame_EF>(currState.copied->efGroup, frame.EF);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste all")) {
					CopyVectorContents<Frame_EF>(frame.EF, currState.copied->efGroup);
					frameData->mark_modified(currState.pattern);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Add copy")) {
					frame.EF.push_back(currState.copied->efSingle);
					frameData->mark_modified(currState.pattern);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Conditions"))
			{
				IfDisplay(&frame.IF, &currState.copied->ifSingle, frameData, currState.pattern);
				if(ImGui::Button("Copy all")) {
					CopyVectorContents<Frame_IF>(currState.copied->ifGroup, frame.IF);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Paste all")) {
					CopyVectorContents<Frame_IF>(frame.IF, currState.copied->ifGroup);
					frameData->mark_modified(currState.pattern);
				}
				ImGui::SameLine(0,20.f);
				if(ImGui::Button("Add copy")) {
					frame.IF.push_back(currState.copied->ifSingle);
					frameData->mark_modified(currState.pattern);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
	}
	ImGui::End();
}

