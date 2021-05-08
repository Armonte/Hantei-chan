#include "main_pane.h"
#include "pattern_disp.h"
#include "frame_disp.h"
#include <imgui.h>	

MainPane::MainPane(Render* render_):
frameData(nullptr),
currentPattern(-1),
currFrame(0),
decoratedNames(nullptr),
render(render_)
{
	
}

void MainPane::RegenerateNames()
{
	delete[] decoratedNames;
	
	if(frameData)
	{
		decoratedNames = new std::string[frameData->get_sequence_count()];
		int count = frameData->get_sequence_count();

		for(int i = 0; i < count; i++)
		{
			decoratedNames[i] = frameData->GetDecoratedName(i);
		}
	}
	else
		decoratedNames = nullptr;
}

void MainPane::SetFrameData(FrameData *frameData_)
{
	if(frameData_)
		currentPattern = 0;
	else
		currentPattern = -1;

	frameData = frameData_;

	RegenerateNames();
}


void MainPane::Draw()
{	
	ImGui::Begin("Left Pane",0 , ImGuiWindowFlags_NoMove );
	if(frameData->m_loaded)
	{
		if (ImGui::BeginCombo("Pattern", decoratedNames[currentPattern].c_str(), ImGuiComboFlags_HeightLargest))
		{
			auto count = frameData->get_sequence_count();
			for (int n = 0; n < count; n++)
			{
				//std::string &&name = std::to_string(n) + " " + frameData->GetName(n); 
				const bool is_selected = (currentPattern == n);
				if (ImGui::Selectable(decoratedNames[n].c_str(), is_selected))
				{
					currentPattern = n;
					currFrame = 0;
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		auto seq = frameData->get_sequence(currentPattern);
		if(seq)
		{
			if (ImGui::TreeNode("Pattern data"))
			{
				PatternDisplay(seq);
				ImGui::TreePop();
				ImGui::Separator();
			}

			int nframes = seq->frames.size() - 1;
			if(nframes >= 0)
			{			
				float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
				ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 160.f);
				ImGui::SliderInt("##frameSlider", &currFrame, 0, nframes);
				ImGui::SameLine();
				ImGui::PushButtonRepeat(true);
				if (ImGui::ArrowButton("##left", ImGuiDir_Left) && currFrame > 0) {currFrame--;}
				ImGui::SameLine(0.0f, spacing);
				if (ImGui::ArrowButton("##right", ImGuiDir_Right) && currFrame < nframes) {currFrame++;}
				ImGui::PopButtonRepeat();
				ImGui::SameLine();
				ImGui::Text("%d/%d", currFrame+1, nframes+1);

				Frame &frame = seq->frames[currFrame];
				if(frame.AT)
					AtDisplay(frame.AT);
				if (ImGui::TreeNode("Animation data"))
				{
					AfDisplay(&frame.AF);
					ImGui::TreePop();
					ImGui::Separator();
				}
				constexpr float width = 50.f;
				ImGui::SetNextItemWidth(width);
				ImGui::InputInt("Boxes", &frame.nHitbox, 0, 0);
				ImGui::SetNextItemWidth(width);
				ImGui::InputInt("FSNE", &frame.FSNE, 0, 0);
				ImGui::SetNextItemWidth(width);
				ImGui::InputInt("FSNI", &frame.FSNI, 0, 0);
			}
			else
			{
				ImGui::Text("This pattern has no frames.");
			}


		}
		else
		{
			ImGui::Text("This pattern is empty.");
		}
	}
	else
		render->DontDraw();




	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::End();

	ForceUpdate();
}

void MainPane::ForceUpdate()
{
	Sequence *seq;
	if((seq = frameData->get_sequence(currentPattern)) &&
		seq->frames.size() > 0)
	{
		auto &frame =  seq->frames[currFrame];
		spriteId = frame.AF.spriteId;
		render->GenerateHitboxVertices(frame.hitboxes, frame.nHitbox);
		render->offsetX = (frame.AF.offset_x)*1;
		render->offsetY = (frame.AF.offset_y)*1;
		render->SetImageColor(frame.AF.rgba);
		render->rotX = frame.AF.rotation[0];
		render->rotY = frame.AF.rotation[1];
		render->rotZ = frame.AF.rotation[2];
		render->scaleX = frame.AF.scale[0];
		render->scaleY = frame.AF.scale[1];
		
		switch (frame.AF.blend_mode)
		{
		case 2:
			render->blendingMode = Render::additive;
			break;
		case 3:
			render->blendingMode = Render::substractive;
			break;
		default:
			render->blendingMode = Render::normal;
			break;
		}
	}
	else
	{
		spriteId = -1;
		
		render->DontDraw();
	}
	render->SwitchImage(spriteId);
}

void MainPane::AdvancePattern(int dir)
{
	currentPattern+= dir;
	if(currentPattern < 0)
		currentPattern = 0;
	else if(currentPattern >= frameData->get_sequence_count())
		currentPattern = frameData->get_sequence_count()-1;
	currFrame = 0;
}

void MainPane::AdvanceFrame(int dir)
{
	auto seq = frameData->get_sequence(currentPattern);
	currFrame += dir;
	if(currFrame < 0)
		currFrame = 0;
	else if(seq && currFrame >= seq->frames.size())
		currFrame = seq->frames.size()-1;
}