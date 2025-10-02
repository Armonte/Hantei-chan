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
				AtDisplay(&frame.AT);
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Effects"))
			{
				EfDisplay(&frame.EF);
				ImGui::TreePop();
				ImGui::Separator();
			}
			if(ImGui::TreeNode("Conditions"))
			{
				IfDisplay(&frame.IF, frameData);
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
	}
	ImGui::End();
}

