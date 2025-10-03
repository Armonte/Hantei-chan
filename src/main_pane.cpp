#include "main_pane.h"
#include "pattern_disp.h"
#include "frame_disp.h"
#include "misc.h"
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
		// Count and display modified patterns
		int modifiedCount = 0;
		for(int i = 0; i < frameData->get_sequence_count(); i++)
		{
			auto seq = frameData->get_sequence(i);
			if(seq && seq->modified)
				modifiedCount++;
		}

		if(modifiedCount > 0)
		{
			im::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Modified patterns: %d", modifiedCount);
		}

		// Update current pattern's decorated name in case it was modified
		decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);

		if (im::BeginCombo("Pattern", decoratedNames[currState.pattern].c_str(), ImGuiComboFlags_HeightLargest))
		{
			auto count = frameData->get_sequence_count();
			// Regenerate all names when dropdown is open to show current modified status
			for (int n = 0; n < count; n++)
			{
				decoratedNames[n] = frameData->GetDecoratedName(n);
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
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				
			}

			im::BeginChild("FrameInfo", {0, im::GetWindowSize().y-im::GetFrameHeight()*3}, false, ImGuiWindowFlags_HorizontalScrollbar);

			// Show if current pattern is modified
			if(seq->modified)
			{
				im::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "[Modified]");
			}

			if (im::TreeNode("Pattern data"))
			{
				// Strings are already stored as UTF-8 in memory
				nameEditBuffer = seq->name;
				if(im::InputText("Pattern name", &nameEditBuffer))
				{
					seq->name = nameEditBuffer;
					frameData->mark_modified(currState.pattern);
					markModified();
					decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
				}
				PatternDisplay(seq, frameData, currState.pattern);

				if(im::Button("Copy pattern")) {
					currState.copied->pattern = *seq;
				}
				im::SameLine(0,20.f);
				if(im::Button("Paste pattern")) {
					*seq = currState.copied->pattern;
					frameData->mark_modified(currState.pattern);
					markModified();
					decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
					nframes = seq->frames.size() - 1;
				}

				if(im::Button("Push pattern copy")) {
					patCopyStack.push_back(SequenceWId{currState.pattern, *seq});
				}
				im::SameLine(0,20.f);
				if(im::Button("Pop all and paste")) {
					PopCopies();
					RegenerateNames();
					nframes = seq->frames.size() - 1;
				}
				im::SameLine(0,20.f);
				im::Text("%zu copies", patCopyStack.size());

				im::TreePop();
				im::Separator();
			}
			if(nframes >= 0)
			{
				Frame &frame = seq->frames[currState.frame];
				if(im::TreeNode("State data"))
				{
					AsDisplay(&frame.AS, frameData, currState.pattern, [this]() { markModified(); });
					if(im::Button("Copy AS")) {
						currState.copied->as = frame.AS;
					}
					im::SameLine(0,20.f);
					if(im::Button("Paste AS")) {
						frame.AS = currState.copied->as;
						frameData->mark_modified(currState.pattern);
						markModified();
					}
					im::TreePop();
					im::Separator();
				}
				if (im::TreeNode("Animation data"))
				{
					AfDisplay(&frame.AF, currState.selectedLayer, frameData, currState.pattern, [this]() { markModified(); });
					if(im::Button("Copy AF")) {
						currState.copied->af = frame.AF;
					}
					if(im::IsItemHovered()) Tooltip("Copy all animation data (sprite, timing, transforms, colors)");
					im::SameLine(0,20.f);
					if(im::Button("Paste AF")) {
						frame.AF = currState.copied->af;
						frameData->mark_modified(currState.pattern);
						markModified();
					}
					if(im::IsItemHovered()) Tooltip("Paste all animation data (sprite, timing, transforms, colors)");
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
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					im::SameLine(0,20.f);
					if(im::Button("Insert frame"))
					{
						if(copyThisFrame)
							seq->frames.insert(seq->frames.begin()+currState.frame, frame);
						else
							seq->frames.insert(seq->frames.begin()+currState.frame, {});
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					im::SameLine(0,20.f);
					if(im::Button("Delete frame"))
					{
						seq->frames.erase(seq->frames.begin()+currState.frame);
						if(currState.frame >= seq->frames.size())
							currState.frame--;
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Copy frame"))
					{
						currState.copied->frame = frame;
					}
					im::SameLine(0,20.f);
					if(im::Button("Paste frame"))
					{
						frame = currState.copied->frame;
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					im::Separator();

					// Range paste controls
					im::Text("Range Paste:");
					im::InputInt2("Frame range", ranges);

					// Clamp ranges
					const int maxFrame = seq->frames.size() - 1;
					if(ranges[0] < 0) ranges[0] = 0;
					if(ranges[1] < 0) ranges[1] = 0;
					if(ranges[0] > maxFrame) ranges[0] = maxFrame;
					if(ranges[1] > maxFrame) ranges[1] = maxFrame;

					// Animation Properties
					im::Text("Animation:");
					if(im::Button("Paste sprite & duration"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							seq->frames[i].AF.spriteId = frame.AF.spriteId;
							seq->frames[i].AF.usePat = frame.AF.usePat;
							seq->frames[i].AF.duration = frame.AF.duration;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste jump & interpolation"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							seq->frames[i].AF.aniType = frame.AF.aniType;
							seq->frames[i].AF.aniFlag = frame.AF.aniFlag;
							seq->frames[i].AF.jump = frame.AF.jump;
							seq->frames[i].AF.landJump = frame.AF.landJump;
							seq->frames[i].AF.interpolationType = frame.AF.interpolationType;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste priority & loops"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							seq->frames[i].AF.priority = frame.AF.priority;
							seq->frames[i].AF.loopCount = frame.AF.loopCount;
							seq->frames[i].AF.loopEnd = frame.AF.loopEnd;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					im::Separator();

					// Transform Properties
					im::Text("Transforms:");
					if(im::Button("Paste offset (X/Y)"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							seq->frames[i].AF.offset_x = frame.AF.offset_x;
							seq->frames[i].AF.offset_y = frame.AF.offset_y;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste rotation"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							memcpy(seq->frames[i].AF.rotation, frame.AF.rotation, sizeof(float)*3);
							seq->frames[i].AF.AFRT = frame.AF.AFRT;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste scale"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							memcpy(seq->frames[i].AF.scale, frame.AF.scale, sizeof(float)*2);
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste color & blend"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							memcpy(seq->frames[i].AF.rgba, frame.AF.rgba, sizeof(float)*4);
							seq->frames[i].AF.blend_mode = frame.AF.blend_mode;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste all transforms"))
					{
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							seq->frames[i].AF.offset_x = frame.AF.offset_x;
							seq->frames[i].AF.offset_y = frame.AF.offset_y;
							memcpy(seq->frames[i].AF.rotation, frame.AF.rotation, sizeof(float)*3);
							memcpy(seq->frames[i].AF.scale, frame.AF.scale, sizeof(float)*2);
							memcpy(seq->frames[i].AF.rgba, frame.AF.rgba, sizeof(float)*4);
							seq->frames[i].AF.blend_mode = frame.AF.blend_mode;
							seq->frames[i].AF.AFRT = frame.AF.AFRT;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					im::Separator();

					// Copy/paste multiple frames
					if(im::Button("Copy frames in range"))
					{
						currState.copied->frames.clear();
						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							Frame_T<LinearAllocator> temp;
							temp = seq->frames[i];
							currState.copied->frames.push_back(temp);
						}
					}

					im::SameLine();
					if(im::Button("Paste frames at position"))
					{
						int insertPos = ranges[0];
						if(insertPos >= 0 && insertPos <= seq->frames.size() && !currState.copied->frames.empty())
						{
							// Manually copy frames with cross-allocator assignment
							for (size_t i = 0; i < currState.copied->frames.size(); i++) {
								Frame temp;
								temp = currState.copied->frames[i];
								seq->frames.insert(seq->frames.begin() + insertPos + i, temp);
							}
							frameData->mark_modified(currState.pattern);
							markModified();
						}
					}

					im::TreePop();
					im::Separator();
				}
			}
			im::EndChild();
		}
	}
	else
		im::Text("Load some data first.");

	//im::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / im::GetIO().Framerate, im::GetIO().Framerate);
	im::End();
}

void MainPane::PopCopies()
{
	for(auto &pat : patCopyStack)
	{
		*frameData->get_sequence(pat.id) = pat.seq;
		frameData->mark_modified(pat.id);
		markModified();
	}
	patCopyStack.clear();
}

