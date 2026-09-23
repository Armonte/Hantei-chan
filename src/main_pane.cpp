#include "main_pane.h"
#include "pattern_disp.h"
#include "frame_disp.h"
#include "misc.h"
#include <imgui.h>
#include "imsearch.h"	

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

		// Always one status line, so the widgets below never move when patterns
		// become modified (users drive the pane with position-based macros, #82).
		{
			const ImVec4 modifiedColor(1.0f, 0.7f, 0.0f, 1.0f);
			const ImVec4 idleColor = im::GetStyleColorVec4(ImGuiCol_TextDisabled);
			im::TextColored(modifiedCount > 0 ? modifiedColor : idleColor, "Modified patterns: %d", modifiedCount);
			auto curSeq = frameData->get_sequence(currState.pattern);
			if(curSeq && curSeq->modified)
			{
				im::SameLine();
				im::TextColored(modifiedColor, "[Modified]");
			}
		}

		// Update current pattern's decorated name in case it was modified
		decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);

		// Pattern search bar (above pattern dropdown)
		if(showPatternSearchBar)
		{
			if(ImSearch::BeginSearch())
			{
				ImSearch::SearchBar("Search pattern names...");
				
				// Only show searchable items when there's an active search query
				const char* query = ImSearch::GetUserQuery();
				if(query && strlen(query) > 0)
				{
					auto count = frameData->get_sequence_count();
					for(int n = 0; n < count; n++)
					{
						auto patternSeq = frameData->get_sequence(n);
						if(!patternSeq) continue;

						std::string displayName = frameData->GetDecoratedName(n);
						ImSearch::SearchableItem(displayName.c_str(),
							[&, n](const char* name)
							{
								const bool isSelected = (currState.pattern == n);
								if(im::Selectable(name, isSelected))
								{
									currState.pattern = n;
									currState.frame = 0;
									currState.currentTick = 0;
									// Update decorated name
									decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
									// Clear search query to hide the list
									ImSearch::SetUserQuery("");
								}
							});
					}
				}
				ImSearch::EndSearch();
			}
		}

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
					currState.currentTick = 0;  // Reset tick when changing pattern
					// Debug: Print pattern name
					auto seq = frameData->get_sequence(n);
					if(seq) {
						printf("[Pattern Switch] Pattern %d: '%s' (UTF-8 bytes: ", n, seq->name.c_str());
						for(size_t i = 0; i < seq->name.length() && i < 64; i++) {
							printf("%02x ", (unsigned char)seq->name[i]);
						}
						printf(")\n");
					}
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					im::SetItemDefaultFocus();
			}
			im::EndCombo();
		}
		im::SameLine(0, 20.f);
		im::SetNextItemWidth(45);
		if(im::InputInt("ID##Pattern", &currState.pattern, 0, 0)) {
			// Clamp to valid range
			int count = frameData->get_sequence_count();
			if(currState.pattern < 0) currState.pattern = 0;
			if(currState.pattern >= count) currState.pattern = count - 1;
			currState.frame = 0;
			currState.currentTick = 0;  // Reset tick when changing pattern
			// Update decorated name
			decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
		}
		im::SameLine(0, 10.f);
		// Use Unicode magnifying glass character (U+1F50D) or fallback to text
		const char* searchIcon = u8"🔍";  // UTF-8 encoded magnifying glass emoji
		if(im::Button(searchIcon))
		{
			showPatternSearchBar = !showPatternSearchBar;
		}
		if(im::IsItemHovered())
		{
			im::SetTooltip(showPatternSearchBar ? "Hide search bar" : "Show search bar");
		}
		auto seq = frameData->get_sequence(currState.pattern);
		if(seq)
		{
			int nframes = seq->frames.size() - 1;
			if(nframes >= 0)
			{			
				float spacing = im::GetStyle().ItemInnerSpacing.x;
				im::SetNextItemWidth(im::GetWindowWidth() - 160.f);
				// On manual frame navigation, drop any activeSpawns left by a
				// timeline scrub: the render path prefers activeSpawns when
				// non-empty, so stale entries kept showing the scrubbed tick's
				// spawns instead of the newly selected frame's.
				if (im::SliderInt("##frameSlider", &currState.frame, 0, nframes)) {
					// Sync ticks when slider changes
					currState.currentTick = CalculateTickFromFrame(frameData, currState.pattern, currState.frame);
					currState.activeSpawns.clear();
				}
				im::SameLine();
				im::PushButtonRepeat(true);
				if(im::ArrowButton("##left", ImGuiDir_Left)) {
					currState.frame--;
					// Sync ticks when manually seeking
					currState.currentTick = CalculateTickFromFrame(frameData, currState.pattern, currState.frame);
					currState.activeSpawns.clear();
				}
				im::SameLine(0.0f, spacing);
				if(im::ArrowButton("##right", ImGuiDir_Right)) {
					currState.frame++;
					// Sync ticks when manually seeking
					currState.currentTick = CalculateTickFromFrame(frameData, currState.pattern, currState.frame);
					currState.activeSpawns.clear();
				}
				im::PopButtonRepeat();
				im::SameLine();
				im::Text("%d/%d", currState.frame, nframes);

				if(currState.frame < 0)
					currState.frame = 0;
				else if(currState.frame > nframes)
					currState.frame = nframes;

				if(im::Button("Animate"))
				{
					currState.animating = !currState.animating;
					currState.animeSeq = currState.pattern;
					if (currState.animating) {
						// Reset tick counter and clear active spawns when starting animation
						currState.currentTick = 0;
						currState.previousFrame = -1;
						currState.activeSpawns.clear();
					}
				}
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

			im::BeginChild("FrameInfo", {0, 0}, false);

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
				// Frame-list mutations are deferred to the end of this block:
				// inserting/erasing reallocates seq->frames and would leave
				// `frame` dangling for the rest of the draw.
				enum class KeyframeOp { None, Append, Insert, Delete } keyframeOp = KeyframeOp::None;
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
					im::TreePop();
					im::Separator();
				}
				if (im::TreeNode("Tools"))
				{
					im::Checkbox("Make copy current frame", &copyThisFrame);
					
					if(im::Button("Append frame"))
						keyframeOp = KeyframeOp::Append;

					im::SameLine(0,20.f);
					if(im::Button("Insert frame"))
						keyframeOp = KeyframeOp::Insert;

					im::SameLine(0,20.f);
					if(im::Button("Delete frame"))
						keyframeOp = KeyframeOp::Delete;

					im::SameLine(0,20.f);
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

					im::SameLine(0,20.f);
					if(im::Button("Range tool"))
					{
						ranges[0] = 0;
						ranges[1] = 0;
						rangeWindow = !rangeWindow;
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
						// Ensure source frame has at least one layer
						if (frame.AF.layers.empty()) {
							frame.AF.layers.push_back({});
						}
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							// Ensure destination frame has at least one layer
							if (seq->frames[i].AF.layers.empty()) {
								seq->frames[i].AF.layers.push_back({});
							}
							seq->frames[i].AF.layers[0].spriteId = srcLayer.spriteId;
							seq->frames[i].AF.layers[0].usePat = srcLayer.usePat;
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
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							if (seq->frames[i].AF.layers.empty()) seq->frames[i].AF.layers.push_back({});
							seq->frames[i].AF.layers[0].offset_x = srcLayer.offset_x;
							seq->frames[i].AF.layers[0].offset_y = srcLayer.offset_y;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste rotation"))
					{
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							if (seq->frames[i].AF.layers.empty()) seq->frames[i].AF.layers.push_back({});
							memcpy(seq->frames[i].AF.layers[0].rotation, srcLayer.rotation, sizeof(float)*3);
							seq->frames[i].AF.AFRT = frame.AF.AFRT;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste scale"))
					{
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							if (seq->frames[i].AF.layers.empty()) seq->frames[i].AF.layers.push_back({});
							memcpy(seq->frames[i].AF.layers[0].scale, srcLayer.scale, sizeof(float)*2);
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste color & blend"))
					{
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							if (seq->frames[i].AF.layers.empty()) seq->frames[i].AF.layers.push_back({});
							memcpy(seq->frames[i].AF.layers[0].rgba, srcLayer.rgba, sizeof(float)*4);
							seq->frames[i].AF.layers[0].blend_mode = srcLayer.blend_mode;
						}
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					if(im::Button("Paste all transforms"))
					{
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const auto& srcLayer = frame.AF.layers[0];

						for(int i = ranges[0]; i <= ranges[1] && i >= 0 && i < seq->frames.size(); i++)
						{
							if (seq->frames[i].AF.layers.empty()) seq->frames[i].AF.layers.push_back({});
							seq->frames[i].AF.layers[0].offset_x = srcLayer.offset_x;
							seq->frames[i].AF.layers[0].offset_y = srcLayer.offset_y;
							memcpy(seq->frames[i].AF.layers[0].rotation, srcLayer.rotation, sizeof(float)*3);
							memcpy(seq->frames[i].AF.layers[0].scale, srcLayer.scale, sizeof(float)*2);
							memcpy(seq->frames[i].AF.layers[0].rgba, srcLayer.rgba, sizeof(float)*4);
							seq->frames[i].AF.layers[0].blend_mode = srcLayer.blend_mode;
							seq->frames[i].AF.AFRT = frame.AF.AFRT;
							seq->frames[i].AF.priority = frame.AF.priority;
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

			// Range tool popup window
			if(rangeWindow)
			{
				im::SetNextWindowSize(ImVec2{400, 300}, ImGuiCond_FirstUseEver);
				im::Begin("Range tool", &rangeWindow);

				im::InputInt2("Frame range", ranges);

				// Clamp ranges
				const int maxFrame = seq->frames.size() - 1;
				if(ranges[0] < 0) ranges[0] = 0;
				if(ranges[1] < 0) ranges[1] = 0;
				if(ranges[0] > maxFrame) ranges[0] = maxFrame;
				if(ranges[1] > maxFrame) ranges[1] = maxFrame;

				im::End();
			}

			// Pattern name search popup
			if(showPatternSearch)
			{
				im::SetNextWindowSize(ImVec2{500, 400}, ImGuiCond_FirstUseEver);
				im::SetNextWindowPos(im::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
				if(im::Begin("Search Pattern Names", &showPatternSearch, ImGuiWindowFlags_NoCollapse))
				{
					if(ImSearch::BeginSearch())
					{
						ImSearch::SearchBar("Search pattern names...");

						auto count = frameData->get_sequence_count();
						for(int n = 0; n < count; n++)
						{
							auto patternSeq = frameData->get_sequence(n);
							if(!patternSeq) continue;

							std::string displayName = frameData->GetDecoratedName(n);
							ImSearch::SearchableItem(displayName.c_str(),
								[&, n](const char* name)
								{
									const bool isSelected = (currState.pattern == n);
									if(im::Selectable(name, isSelected))
									{
										currState.pattern = n;
										currState.frame = 0;
										currState.currentTick = 0;
										showPatternSearch = false;
										// Update decorated name
										decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
									}
								});
						}
						ImSearch::EndSearch();
					}
				}
				im::End();
			}

			// Apply the deferred keyframe op. `frame` is not used past this
			// point; the frame index is re-validated after the mutation.
			if(keyframeOp != KeyframeOp::None)
			{
				const int at = currState.frame;
				switch(keyframeOp)
				{
				case KeyframeOp::Append:
				{
					Frame newFrame = copyThisFrame ? seq->frames[at] : Frame{};
					seq->frames.push_back(std::move(newFrame));
					break;
				}
				case KeyframeOp::Insert:
				{
					// Copy first: insert(pos, {}) picked the initializer_list
					// overload and inserted nothing.
					Frame newFrame = copyThisFrame ? seq->frames[at] : Frame{};
					seq->frames.insert(seq->frames.begin() + at, std::move(newFrame));
					break;
				}
				case KeyframeOp::Delete:
					seq->frames.erase(seq->frames.begin() + at);
					break;
				default:
					break;
				}

				const int count = (int)seq->frames.size();
				if(currState.frame >= count)
					currState.frame = count - 1;
				if(currState.frame < 0)
					currState.frame = 0;
				currState.currentTick = count > 0
					? CalculateTickFromFrame(frameData, currState.pattern, currState.frame) : 0;
				currState.activeSpawns.clear();
				frameData->mark_modified(currState.pattern);
				markModified();
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

