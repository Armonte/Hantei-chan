#include "main_pane.h"
#include "pattern_disp.h"
#include "frame_disp.h"
#include <imgui.h>	

MainPane::MainPane(Render* render, FrameData *framedata, FrameState &fs) : DrawWindow(render, framedata, fs),
decoratedNames(nullptr)
{
	
}

void MainPane::RegenerateNames()
{
	delete[] decoratedNames;
	
	if(frameData && frameData->m_loaded)
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

void MainPane::Draw()
{	
	namespace im = ImGui;
	im::Begin("Left Pane",0);
	if(frameData->m_loaded)
	{
		if (im::BeginCombo("Pattern", decoratedNames[currState.pattern].c_str(), ImGuiComboFlags_HeightLargest))
		{
			auto count = frameData->get_sequence_count();
			for (int n = 0; n < count; n++)
			{
				const bool is_selected = (currState.pattern == n);
				if (im::Selectable(decoratedNames[n].c_str(), is_selected))
				{
					currState.pattern = n;
					currState.frame = 0;
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					im::SetItemDefaultFocus();
			}
			im::EndCombo();
		}
		auto seq = frameData->get_sequence(currState.pattern);
		if(seq)
		{
			int nframes = seq->frames.size() - 1;
			if(nframes >= 0)
			{			
				float spacing = im::GetStyle().ItemInnerSpacing.x;
				im::SetNextItemWidth(im::GetWindowWidth() - 160.f);
				im::SliderInt("##frameSlider", &currState.frame, 0, nframes);
				im::SameLine();
				im::PushButtonRepeat(true);
				if(im::ArrowButton("##left", ImGuiDir_Left))
					currState.frame--;
				im::SameLine(0.0f, spacing);
				if(im::ArrowButton("##right", ImGuiDir_Right))
					currState.frame++;
				im::PopButtonRepeat();
				im::SameLine();
				im::Text("%d/%d", currState.frame+1, nframes+1);

				if(currState.frame < 0)
					currState.frame = 0;
				else if(currState.frame > nframes)
					currState.frame = nframes;
			}
			else
			{
				im::Text("This pattern has no frames.");
				if(im::Button("Add frame"))
				{
					seq->frames.push_back({});
					currState.frame = 0;
				}
				
			}

			im::BeginChild("FrameInfo", {0, im::GetWindowSize().y-im::GetFrameHeight()*4}, false, ImGuiWindowFlags_HorizontalScrollbar);
			if (im::TreeNode("Pattern data"))
			{
				if(im::InputText("Pattern name", &seq->name))
				{
					decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
				}
				PatternDisplay(seq);
				im::TreePop();
				im::Separator();
			}
			if(nframes >= 0)
			{
				Frame &frame = seq->frames[currState.frame];
				if(im::TreeNode("State data"))
				{
					AsDisplay(&frame.AS);
					im::TreePop();
					im::Separator();
				}
				if (im::TreeNode("Animation data"))
				{
					AfDisplay(&frame.AF);
					im::TreePop();
					im::Separator();
				}
				if (im::TreeNode("Tools"))
				{
					im::Checkbox("Make copy current frame", &copyThisFrame);
					
					if(im::Button("Append frame"))
					{
						if(copyThisFrame)
							seq->frames.push_back(frame);
						else
							seq->frames.push_back({});
					}

					im::SameLine(0,20.f); 
					if(im::Button("Insert frame"))
					{
						if(copyThisFrame)
							seq->frames.insert(seq->frames.begin()+currState.frame, frame);
						else
							seq->frames.insert(seq->frames.begin()+currState.frame, {});
					}

					im::SameLine(0,20.f);
					if(im::Button("Delete frame"))
					{
						seq->frames.erase(seq->frames.begin()+currState.frame);
						if(currState.frame >= seq->frames.size())
							currState.frame--;
					}

					im::TreePop();
					im::Separator();
				}
			}
			im::EndChild();
		}
	}

	im::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / im::GetIO().Framerate, im::GetIO().Framerate);
	im::End();
}

