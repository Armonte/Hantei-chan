#include <algorithm>
#include "main_pane.h"
#include "pattern_disp.h"
#include "frame_disp.h"
#include "misc.h"
#include <imgui.h>
#include "imsearch.h"
#include "pattern_search.h"
#include "i18n.h"
#include "ui/ui_style.h"
#include "ui/compact_inspector.h"
#include "fpanim_play.h"
#include "paste_notice.h"

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
	ui::CompactInspectorScope compactScope(gSettings.compactUi);
	namespace im = ImGui;
	im::Begin(windowName("Left Pane").c_str(),0);
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
			im::TextColored(modifiedCount > 0 ? modifiedColor : idleColor, TXT("Modified patterns: %d"), modifiedCount);
			auto curSeq = frameData->get_sequence(currState.pattern);
			if(curSeq && curSeq->modified)
			{
				im::SameLine();
				im::TextColored(modifiedColor, TXT("[Modified]"));
			}
		}

		// UNI2/MBTL: play this pattern in the running game (fpan-im playpattern).
		fpanimplay::DrawPlayButton(frameData, currState.pattern);

		// Update current pattern's decorated name in case it was modified
		decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);

		// Pattern search bar (above pattern dropdown)
		if(showPatternSearchBar)
		{
			if(ImSearch::BeginSearch(kPatternSearchFlags))
			{
				ImSearch::SearchBar(TXT("Search pattern names..."));
				
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

		{	// the combo takes what the label, the ID field and the search button leave, so a longer (Japanese) label never pushes them out
			const ImGuiStyle &sty = im::GetStyle();
			const float others = im::CalcTextSize(LBL("Pattern"), nullptr, true).x + sty.ItemInnerSpacing.x * 2 + 20.f + ui::Px(45.f)
				+ im::CalcTextSize(TXT("ID")).x + 10.f + im::GetFrameHeight() + sty.FramePadding.x * 2;
			im::SetNextItemWidth(std::max(80.f, im::GetContentRegionAvail().x - others));
		}
		if (im::BeginCombo(LBL("Pattern"), decoratedNames[currState.pattern].c_str(), ImGuiComboFlags_HeightLargest))
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
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					im::SetItemDefaultFocus();
			}
			im::EndCombo();
		}
		im::SameLine(0, 20.f);
		im::SetNextItemWidth(ui::Px(45));
		if(im::InputInt(LBL("ID##Pattern"), &currState.pattern, 0, 0)) {
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
			im::SetTooltip(showPatternSearchBar ? TXT("Hide search bar") : TXT("Show search bar"));
		}
		auto seq = frameData->get_sequence(currState.pattern);
		if(seq)
		{
			int nframes = seq->frames.size() - 1;
			auto patternClipboard = [&]() {
				auto button = [](const char* label) { return gSettings.compactUi ? im::SmallButton(label) : im::Button(label); };
				if (button(LBL("Copy pattern"))) currState.copied->pattern = *seq;
				if (gSettings.compactUi) i18n::SameLineFit(i18n::ButtonWidth(LBL("Paste pattern")));
				else im::SameLine(0,20.f);
				if (button(LBL("Paste pattern"))) {
					Sequence pasted; pasted = currState.copied->pattern;
					currState.pasteNotice = FormatConvertNotice(frameData->conformPasted(pasted));
					*seq = std::move(pasted);
					frameData->notes.archivePattern(currState.pattern);
					frameData->mark_modified(currState.pattern); markModified();
					decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
					nframes = seq->frames.size() - 1;
				}
			};
			if(nframes >= 0)
			{			
				float spacing = im::GetStyle().ItemInnerSpacing.x;
				im::SetNextItemWidth(im::GetWindowWidth() - ui::Px(160.f));
				// On manual frame navigation, drop any activeSpawns left by a
				// timeline scrub: the render path prefers activeSpawns when
				// non-empty, so stale entries kept showing the scrubbed tick's
				// spawns instead of the newly selected frame's.
				if (im::SliderInt("##frameSlider", &currState.frame, 0, nframes)) {
					// Sync ticks when slider changes
					currState.InspectState(frameData,currState.frame);
					currState.activeSpawns.clear();
				}
				im::SameLine();
				im::PushButtonRepeat(true);
				if(im::ArrowButton("##left", ImGuiDir_Left)) {
					currState.frame--;
					// Sync ticks when manually seeking
					currState.InspectState(frameData,currState.frame);
					currState.activeSpawns.clear();
				}
				im::SameLine(0.0f, spacing);
				if(im::ArrowButton("##right", ImGuiDir_Right)) {
					currState.frame++;
					// Sync ticks when manually seeking
					currState.InspectState(frameData,currState.frame);
					currState.activeSpawns.clear();
				}
				im::PopButtonRepeat();
				im::SameLine();
				im::Text("%d/%d", currState.frame, nframes);

				if(currState.frame < 0)
					currState.frame = 0;
				else if(currState.frame > nframes)
					currState.frame = nframes;

				if(im::Button(LBL("Animate")))
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
				im::Text(TXT("This pattern has no frames."));
				if(im::Button(LBL("Add frame")))
				{
					seq->frames.push_back({});
					currState.frame = 0;
					frameData->mark_modified(currState.pattern);
					markModified();
				}
				
			}

			if (gSettings.compactUi) {
				i18n::SameLineFit(i18n::ButtonWidth(LBL("Copy pattern")));
				patternClipboard();
			}
			const float edgePadding=ui::Em(.6f);
			if(gSettings.compactUi) {
				im::SetNextWindowContentSize(ImVec2(std::max(ui::Em(32),im::GetContentRegionAvail().x-2*edgePadding-im::GetStyle().ScrollbarSize),0));
				im::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(edgePadding,0));
			}
			im::BeginChild("FrameInfo", {0, 0}, gSettings.compactUi ? ImGuiChildFlags_AlwaysUseWindowPadding : 0, gSettings.compactUi ? ImGuiWindowFlags_HorizontalScrollbar : 0);
			if(gSettings.compactUi) im::PopStyleVar();
			// Keep this view's scroll position across tab switches (#73; the
			// Right Pane already does the same).
			const bool leftViewChanged=RestorePaneScroll(currState.leftPaneScrollY);

			bool pastePatternStack=false;
			auto drawPatternStack=[&]() {
				if(im::Button(LBL("Push pattern copy"))) {
					patCopyStack.push_back(SequenceWId{currState.pattern, *seq});
				}
				im::SameLine(0,20.f);
				if(im::Button(LBL("Pop all and paste"))) {
					pastePatternStack = true;
				}
				im::SameLine(0,20.f);
				im::Text(TXT("%zu copies"), patCopyStack.size());
				if(!patCopyStack.empty()) {
					im::SameLine();
					if(im::SmallButton(LBL("Drop last"))) patCopyStack.pop_back();
					im::SameLine();
					if(im::SmallButton(LBL("Clear"))) patCopyStack.clear();
				}
				i18n::TextDisabledWrapped("%s", TXT("Windows > Pattern manager: multi-select, paste slots, move with references."));
			};
			if (im::TreeNode(LBL("Pattern data")))
			{
				// Strings are already stored as UTF-8 in memory
				nameEditBuffer = seq->name;
				if(im::InputText(LBL("Pattern name"), &nameEditBuffer))
				{
					seq->name = nameEditBuffer;
					frameData->mark_modified(currState.pattern);
					markModified();
					decoratedNames[currState.pattern] = frameData->GetDecoratedName(currState.pattern);
				}
				if (gSettings.compactUi) PatternDisplay(seq, frameData, currState.pattern);
				// Pattern note (issue #58), kept beside the HA6, not in it.
				if (!gSettings.compactUi || im::TreeNode(LBL("Pattern note"))) {
					const std::string key = Ha6Notes::PatternKey(currState.pattern);
					const std::string* note = frameData->notes.get(key);
					noteEditBuffer = note ? *note : std::string();
					if(im::InputTextMultiline(LBL("Pattern note"), &noteEditBuffer, ImVec2(0, im::GetTextLineHeight() * 3)))
						frameData->notes.set(key, noteEditBuffer);
					if (gSettings.compactUi) im::TreePop();
				}
				if (!gSettings.compactUi) PatternDisplay(seq, frameData, currState.pattern);
				if (drawScriptRefs) drawScriptRefs(frameData, currState.pattern);

				if (!gSettings.compactUi) patternClipboard();

				if (!gSettings.compactUi) drawPatternStack();

				im::TreePop();
				im::Separator();
			}

			if(pastePatternStack) {
				PopCopies();RegenerateNames();nframes=(int)seq->frames.size()-1;
				currState.frame=std::clamp(currState.frame,0,std::max(0,nframes));
				currState.InspectState(frameData,currState.frame);currState.activeSpawns.clear();pastePatternStack=false;
			}
			if(nframes >= 0)
			{
				// Frame-list mutations are deferred to the end of this block:
				// inserting/erasing reallocates seq->frames and would leave
				// `frame` dangling for the rest of the draw.
				enum class KeyframeOp { None, Append, Insert, Delete, AppendNextSprite } keyframeOp = KeyframeOp::None;
				Frame &frame = seq->frames[currState.frame];
				// D13: what the last paste converted.
				if(!currState.pasteNotice.empty())
				{
					im::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.45f, 0.0f, 1.0f));
					im::TextWrapped("%s", currState.pasteNotice.c_str());
					im::PopStyleColor();
					if(im::SmallButton(LBL("Dismiss##pasteNotice"))) currState.pasteNotice.clear();
				}
				// D17: a BaseData PFLG template backs this pattern in game.
				const Sequence* tmpl = frameData->templateFor(currState.pattern);
				if(tmpl)
				{
					auto ticks = [](const Sequence& q) { int t = 0; for (const auto& f : q.frames) t += std::max(1, f.AF.duration); return t; };
					im::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
					im::TextWrapped("%s", TXT("BaseData template pattern: in game the frame count, timing, state (AS), attack (AT), effects, conditions and layer transforms come from BaseData. Only each layer's sprite, .pat flag and offset, the hurt boxes, and the attack boxes on frames where the template attacks, come from this file. Those other fields are greyed out."));
					im::PopStyleColor();
					im::TextDisabled(TXT("Template: %d frame(s), %d tick(s). This pattern: %d frame(s), %d tick(s)."),
						(int)tmpl->frames.size(), ticks(*tmpl), (int)seq->frames.size(), ticks(*seq));
					if(currState.frame >= (int)tmpl->frames.size())
						im::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", TXT("This frame is past the template's last frame: the game never shows it."));
				}
				if (currState.scriptOpenTrees) im::SetNextItemOpen(currState.scriptOpenTrees == 1);
				if(im::TreeNode(LBL("State data")))
				{
					im::BeginDisabled(tmpl != nullptr);
					AsDisplay(&frame.AS, frameData, currState.pattern, [this]() { markModified(); });
					if(im::Button(LBL("Copy AS"))) {
						currState.copied->as = frame.AS;
					}
					im::SameLine(0,20.f);
					if(im::Button(LBL("Paste AS"))) {
						frame.AS = currState.copied->as;
						frameData->mark_modified(currState.pattern);
						markModified();
					}
					im::EndDisabled();
					im::TreePop();
					im::Separator();
				}
				if (currState.scriptOpenTrees) im::SetNextItemOpen(true);
				if (im::TreeNode(LBL("Animation data")))
				{
					AfDisplay(&frame.AF, currState.selectedLayer, frameData, currState.pattern, [this]() { markModified(); }, nullptr, tmpl != nullptr);
					im::TreePop();
					im::Separator();
				}
				if (frame.ha6.nExtra > 0 && im::TreeNode(LBL("Other tags (kept as loaded)")))
				{
					// Tags the UNI2/MBTL loaders read that no editor field covers
					// (rare: no shipped file uses them). Saved back unchanged.
					static const struct { const char* tag; const char* what; } kInfo[] = {
						{"AFAN", "layer Z rotation as an int (same slot as AFAZ)"},
						{"ASV1", "read, ignored"}, {"ASVA", "AS velocity (2 words)"}, {"ASVC", "AS velocity (2 words)"},
						{"ASAT", "AS +32"}, {"ASKV", "read, ignored"}, {"ASSS", "stance (same field as ASS1/ASS2)"},
						{"ASDF", "read, ignored"}, {"ASCL", "movement flags (2 words)"}, {"ASSE", "movement flags (2 words)"},
						{"ASDE", "AS +36"}, {"ASF2", "read, ignored"}, {"ASF3", "read, ignored"},
						{"ATAB", "AT +66"}, {"ATBG", "AT +70 (guard related, Hit_ResolveHitstop)"}, {"ATGE", "AT +32/+34"},
						{"ATKZ", "read, ignored"}, {"ATGS", "read, ignored"}, {"ATF2", "read, ignored"},
						{"HRFF", "box flag byte: box (attack if < 0), value"},
					};
					for (int i = 0; i < frame.ha6.nExtra && i < Ha6FrameEnc::kMaxExtra; ++i) {
						const Ha6ExtraTag& x = frame.ha6.extra[i];
						std::string line(x.tag, 4);
						for (int k = 0; k < x.nwords; ++k) line += " " + std::to_string(x.w[k]);
						const char* what = "";
						for (const auto& in : kInfo) if (!memcmp(in.tag, x.tag, 4)) what = in.what;
						im::Text("%s", line.c_str());
						im::SameLine(); im::TextDisabled("%s", what);
					}
					im::TreePop();
				}
				if (im::TreeNode(LBL("Tools")))
				{
					im::Checkbox(LBL("Make copy current frame"), &copyThisFrame);
					
					if(im::Button(LBL("Append frame")))
						keyframeOp = KeyframeOp::Append;

					i18n::SameLineFit(i18n::ButtonWidth(LBL("Insert frame")));
					if(im::Button(LBL("Insert frame")))
						keyframeOp = KeyframeOp::Insert;

					i18n::SameLineFit(i18n::ButtonWidth(LBL("Delete frame")));
					if(im::Button(LBL("Delete frame")))
						keyframeOp = KeyframeOp::Delete;

					i18n::SameLineFit(i18n::ButtonWidth(LBL("Copy frame")));
					if(im::Button(LBL("Copy frame")))
					{
						currState.copied->frame = frame;
					}
					i18n::SameLineFit(i18n::ButtonWidth(LBL("Paste frame")));
					if(im::Button(LBL("Paste frame")))
					{
						frame = currState.copied->frame;
						frameData->notes.archiveState(currState.pattern, currState.frame);
						currState.pasteNotice = FormatConvertNotice(frameData->conformPasted(frame));
						frameData->mark_modified(currState.pattern);
						markModified();
					}

					i18n::SameLineFit(i18n::ButtonWidth(LBL("Range tool")));
					if(im::Button(LBL("Range tool")))
					{
						ranges[0] = 0;
						ranges[1] = 0;
						rangeWindow = !rangeWindow;
					}

					// Sprite numbering (issue #45)
					if(im::Button(LBL("Append frame, sprite +1")))
						keyframeOp = KeyframeOp::AppendNextSprite;
					if(im::IsItemHovered())
						im::SetTooltip(TXT("Append a copy of the last frame with its layer 0 sprite number + 1,\nand select it."));
					im::SameLine(0,20.f);
					if(im::Button(LBL("Number sprites from here")))
					{
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const int L = std::clamp(currState.selectedLayer, 0, (int)frame.AF.layers.size() - 1);
						const int base = frame.AF.layers[L].spriteId;
						if (base >= 0) {
							for(int i = currState.frame + 1; i < (int)seq->frames.size(); i++)
							{
								auto& layers = seq->frames[i].AF.layers;
								if ((int)layers.size() <= L) { Layer_Type e; e.spriteId = -1; layers.resize(L + 1, e); }
								layers[L].spriteId = base + (i - currState.frame);
							}
							frameData->mark_modified(currState.pattern);
							markModified();
						}
					}
					if(im::IsItemHovered())
						im::SetTooltip(TXT("Set the selected layer of every later frame to this frame's sprite + 1, + 2, ..."));

					im::Separator();

					// Range paste controls
					im::Text(TXT("Range Paste:"));
					im::InputInt2(LBL("Frame range"), ranges);

					// Clamp ranges
					const int maxFrame = seq->frames.size() - 1;
					if(ranges[0] < 0) ranges[0] = 0;
					if(ranges[1] < 0) ranges[1] = 0;
					if(ranges[0] > maxFrame) ranges[0] = maxFrame;
					if(ranges[1] > maxFrame) ranges[1] = maxFrame;

					// D16: the layer tools act on the selected layer (MBTL keeps the
					// body on layer 1) or on every layer of this frame.
					const int rangeLayer = std::clamp(currState.selectedLayer, 0, std::max(0, (int)frame.AF.layers.size() - 1));
					{
						char lb[96];
						snprintf(lb, sizeof lb, TXT("Selected layer (%d)"), rangeLayer);
						im::Text(TXT("Layer tools apply to:"));
						im::SameLine();
						if (im::RadioButton(lb, !currState.rangeAllLayers)) currState.rangeAllLayers = false;
						im::SameLine();
						if (im::RadioButton(LBL("All layers"), currState.rangeAllLayers)) currState.rangeAllLayers = true;
					}
					auto pasteLayers = [&](LayerPaste what) {
						if (frame.AF.layers.empty()) frame.AF.layers.push_back({});
						const Frame src = frame; // the range may include this frame
						PasteLayerRange(*seq, src, ranges[0], ranges[1], currState.rangeAllLayers ? -1 : rangeLayer, what);
						frameData->mark_modified(currState.pattern);
						markModified();
					};

					// Animation Properties
					im::Text(TXT("Animation:"));
					if(im::Button(LBL("Paste sprite & duration")))
						pasteLayers(LayerPaste::SpriteDuration);

					if(im::Button(LBL("Paste jump & interpolation")))
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

					if(im::Button(LBL("Paste priority & loops")))
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
					im::Text(TXT("Transforms:"));
					if(im::Button(LBL("Paste offset (X/Y)")))
						pasteLayers(LayerPaste::Offset);
					if(im::Button(LBL("Paste rotation")))
						pasteLayers(LayerPaste::Rotation);
					if(im::Button(LBL("Paste scale")))
						pasteLayers(LayerPaste::Scale);
					if(im::Button(LBL("Paste color & blend")))
						pasteLayers(LayerPaste::ColorBlend);
					if(im::Button(LBL("Paste all transforms")))
						pasteLayers(LayerPaste::AllTransforms);

					im::Separator();

					// Copy/paste multiple frames
					if(im::Button(LBL("Copy frames in range")))
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
					if(im::Button(LBL("Paste frames at position")))
					{
						int insertPos = ranges[0];
						if(insertPos >= 0 && insertPos <= seq->frames.size() && !currState.copied->frames.empty())
						{
							// Manually copy frames with cross-allocator assignment
							Ha6ConvertReport conv;
							frameData->notes.insert(currState.pattern, -1, false, insertPos, (int)currState.copied->frames.size(), (int)seq->frames.size());
							for (size_t i = 0; i < currState.copied->frames.size(); i++) {
								Frame temp;
								temp = currState.copied->frames[i];
								conv.add(frameData->conformPasted(temp));
								seq->frames.insert(seq->frames.begin() + insertPos + i, temp);
							}
							currState.pasteNotice = FormatConvertNotice(conv);
							frameData->mark_modified(currState.pattern);
							markModified();
						}
					}

					if(gSettings.compactUi) {im::Separator();drawPatternStack();}

					im::TreePop();
					im::Separator();
				}

			// Range tool popup window
			if(rangeWindow)
			{
				im::SetNextWindowSize(ImVec2{400, 300}, ImGuiCond_FirstUseEver);
				im::Begin(LBL("Range tool"), &rangeWindow);

				im::InputInt2(LBL("Frame range"), ranges);

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
				if(im::Begin(LBL("Search Pattern Names"), &showPatternSearch, ImGuiWindowFlags_NoCollapse))
				{
					if(ImSearch::BeginSearch(kPatternSearchFlags))
					{
						ImSearch::SearchBar(TXT("Search pattern names..."));

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
					if (copyThisFrame) frameData->notes.cloneState(currState.pattern, at, (int)seq->frames.size());
					seq->frames.push_back(std::move(newFrame));
					break;
				}
				case KeyframeOp::Insert:
				{
					// Copy first: insert(pos, {}) picked the initializer_list
					// overload and inserted nothing.
					Frame newFrame = copyThisFrame ? seq->frames[at] : Frame{};
					frameData->notes.insert(currState.pattern, -1, false, at, 1, (int)seq->frames.size());
					if (copyThisFrame) frameData->notes.cloneState(currState.pattern, at + 1, at);
					seq->frames.insert(seq->frames.begin() + at, std::move(newFrame));
					break;
				}
				case KeyframeOp::Delete:
					frameData->notes.erase(currState.pattern, -1, false, at, (int)seq->frames.size());
					seq->frames.erase(seq->frames.begin() + at);
					break;
				case KeyframeOp::AppendNextSprite:
				{
					Frame newFrame = seq->frames.back();
					frameData->notes.cloneState(currState.pattern, (int)seq->frames.size() - 1, (int)seq->frames.size());
					if (newFrame.AF.layers.empty()) newFrame.AF.layers.push_back({});
					if (newFrame.AF.layers[0].spriteId >= 0) newFrame.AF.layers[0].spriteId += 1;
					seq->frames.push_back(std::move(newFrame));
					currState.frame = (int)seq->frames.size() - 1;
					break;
				}
				default:
					break;
				}

				const int count = (int)seq->frames.size();
				if(currState.frame >= count)
					currState.frame = count - 1;
				if(currState.frame < 0)
					currState.frame = 0;
				currState.InspectState(frameData,currState.frame);
				currState.activeSpawns.clear();
				frameData->mark_modified(currState.pattern);
				markModified();
			}
			}

			if(pastePatternStack) {
				PopCopies();RegenerateNames();nframes=(int)seq->frames.size()-1;
				currState.frame=std::clamp(currState.frame,0,std::max(0,nframes));
				currState.InspectState(frameData,currState.frame);currState.activeSpawns.clear();pastePatternStack=false;
			}

			if(!leftViewChanged) currState.leftPaneScrollY = im::GetScrollY();
			im::EndChild();
		}
	}
	else
		im::Text(TXT("Load some data first."));

	//im::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / im::GetIO().Framerate, im::GetIO().Framerate);
	im::End();
}

void MainPane::DrawPatternList()
{
	namespace im=ImGui;
	if(gSettings.showPatternList && frameData->m_loaded && decoratedNames) {
		im::SetNextWindowSize(ImVec2(ui::Em(22),ui::Em(30)),ImGuiCond_FirstUseEver);
		const bool wasOpen=gSettings.showPatternList;
		if(im::Begin(windowName("Pattern list").c_str(),&gSettings.showPatternList)) {
				const int count = frameData->get_sequence_count();
				// The selection follows currState.pattern both ways: a click here sets it, and a change anywhere else (combo,
				// ID field, search, shortcuts) moves the selection and scrolls the row into view once. The scroll is held
				// back while a popup is open (the Pattern combo's own keyboard navigation) and while this list has the
				// keyboard nav focus, and a click never scrolls (the row is already where the mouse is).
				const bool popupOpen = im::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
				if (im::BeginChild("##patternList", ImVec2(0, 0), ImGuiChildFlags_Borders))
				{
					const bool listNav = im::IsWindowFocused() && im::GetIO().NavVisible;
					const bool follow = patternListShown != currState.pattern && !popupOpen && !listNav
						&& currState.pattern >= 0 && currState.pattern < count;
					ImGuiListClipper clip;
					clip.Begin(count);
					if (follow) clip.IncludeItemByIndex(currState.pattern);
					while (clip.Step())
						for (int n = clip.DisplayStart; n < clip.DisplayEnd; n++)
						{
							const bool sel = currState.pattern == n;
							im::PushID(n);
							if (im::Selectable(frameData->GetDecoratedName(n).c_str(), sel))
							{
								currState.pattern = n;   // same as picking it in the Pattern combo
								currState.frame = 0;
								currState.InspectState(frameData,0);
								currState.activeSpawns.clear();
								patternListShown = n;
							}
							im::PopID();
							if (sel && follow)
							{
								// only when the row is not fully in view, so a visible selection does not jump
								const float top = im::GetWindowPos().y, bottom = top + im::GetWindowHeight();
								if (im::GetItemRectMin().y < top || im::GetItemRectMax().y > bottom)
									im::SetScrollHereY(0.3f);
								patternListShown = currState.pattern;
							}
						}
				}
				im::EndChild();
		}
		im::End();
		if(wasOpen!=gSettings.showPatternList) im::MarkIniSettingsDirty();
	}

}

void MainPane::PopCopies()
{
	for(auto &pat : patCopyStack)
	{
		*frameData->get_sequence(pat.id) = pat.seq;
		frameData->notes.archivePattern(pat.id);
		frameData->mark_modified(pat.id);
		markModified();
	}
	patCopyStack.clear();
}

