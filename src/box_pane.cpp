#include "box_pane.h"
#include "mv_script.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

constexpr int boxLimit = 33;

// List MBTL move-script spawns (mv_script.h) for the current pattern.
// Resolved entries get a visibility toggle for their spawn-tree entry;
// unresolved code names are shown greyed so the data isn't silently dropped.
static void DrawScriptSpawnList(FrameData* frameData, FrameState& currState)
{
	namespace im = ImGui;

	const MvScriptIndex* mvIndex = MvScriptIndex::Lookup(frameData);
	const std::vector<MvScriptSpawn>* spawns =
		mvIndex ? mvIndex->spawnsForPattern(currState.pattern) : nullptr;
	if(!spawns || spawns->empty())
		return;

	im::Separator();
	if(!im::CollapsingHeader("Script Spawns"))
		return;

	im::TextDisabled("Spawns parsed from move scripts (chrXXX_mv_*.txt)");

	int id = 0;
	for(const auto& ss : *spawns) {
		im::PushID(id++);

		// Visibility toggle bound to the matching spawn-tree entry
		// (present once the spawn tree has been built for this pattern).
		SpawnedPatternInfo* treeEntry = nullptr;
		for(auto& sp : currState.spawnedPatterns) {
			if(sp.isScriptSpawn && sp.scriptSource == ss.source &&
			   (ss.isImpactEffect ? sp.isPresetEffect : sp.patternId == ss.patternId)) {
				treeEntry = &sp;
				break;
			}
		}

		// Frame gate info ("@ move start" when the script has no frame-ID gate)
		char gateBuf[48];
		if(ss.frameIdRef >= 0)
			snprintf(gateBuf, sizeof(gateBuf), "@ frame ID %d", ss.frameIdRef);
		else
			snprintf(gateBuf, sizeof(gateBuf), "@ move start");

		if(ss.isImpactEffect) {
			if(treeEntry) {
				im::Checkbox("##scriptvis", &treeEntry->visible);
				im::SameLine();
			}
			im::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f),
				"impact effect %s  offset (%d, %d) [script]", gateBuf, ss.offsetX, ss.offsetY);
		} else if(ss.patternId >= 0 && frameData->get_sequence(ss.patternId)) {
			if(treeEntry) {
				im::Checkbox("##scriptvis", &treeEntry->visible);
				im::SameLine();
			}
			std::string name = frameData->GetDecoratedName(ss.patternId);
			im::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f),
				"%s  %s  offset (%d, %d) [script]", name.c_str(), gateBuf, ss.offsetX, ss.offsetY);
		} else {
			const char* code = !ss.patternCode.empty() ? ss.patternCode.c_str() :
				(!ss.mvName.empty() ? ss.mvName.c_str() : "?");
			im::TextDisabled("unresolved: %s", code);
		}
		if(im::IsItemHovered() && !ss.source.empty())
			im::SetTooltip("%s", ss.source.c_str());
		im::PopID();
	}
}

BoxPane::BoxPane(Render* render, FrameData *frameData, FrameState &state):
DrawWindow(render, frameData, state),
currentBox(0), highlight(false), showManualControls(false)
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
		if(frames.size()>0 && currState.frame >= 0 && currState.frame < (int)frames.size())
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
	BoxDragWorld(x / render->scale, y / render->scale);
}

void BoxPane::BoxDragWorld(float dx, float dy)
{
	auto seq = frameData->get_sequence(currState.pattern);
	if(seq)
	{
		auto &frames = seq->frames;
		if(frames.size()>0 && currState.frame >= 0 && currState.frame < (int)frames.size())
		{
			Hitbox &box = frames[currState.frame].hitboxes[currentBox];
			dragxy[0] += dx;
			dragxy[1] += dy;

			box.xy[2] = dragxy[0];
			box.xy[3] = dragxy[1];

			markModified();
		}
	}
}

void BoxPane::Draw()
{
	namespace im = ImGui;
	im::Begin(windowName("Box Pane").c_str(),0);


	if(frameData->get_sequence(currState.pattern) && frameData->get_sequence(currState.pattern)->frames.size() > 0)
	{
		auto &frames = frameData->get_sequence(currState.pattern)->frames;
		// Self-heal a stale frame index before indexing (see main_frame.cpp).
		if (currState.frame < 0) currState.frame = 0;
		if (currState.frame >= (int)frames.size()) currState.frame = (int)frames.size() - 1;
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

			// Calculate box layout dimensions
			const float smallBoxSize = 12.0f;
			const float boxSpacing = 2.0f;
			const float minGroupSpacing = 8.0f;
			const float labelPadding = 4.0f;  // Padding between label end and next box group

			// Calculate text widths
			float colW = im::CalcTextSize("Col").x;
			float hurtW = im::CalcTextSize("Hurt").x;
			float spc12W = im::CalcTextSize("Spc1-2").x;
			float clashW = im::CalcTextSize("Clash").x;
			float projW = im::CalcTextSize("Proj").x;
			float spc516W = im::CalcTextSize("Spc5-16").x;

			// Calculate X positions ensuring labels don't overlap
			// Each position is max(previous_boxes_end + minSpacing, previous_label_end + padding)
			float startX = ImGui::GetCursorScreenPos().x;
			float colX = 0;
			float colBoxEnd = colX + smallBoxSize;

			float hurtX = std::max(colBoxEnd + minGroupSpacing, colX + colW + labelPadding);
			float hurtBoxEnd = hurtX + (8 * smallBoxSize + 7 * boxSpacing);

			float spc12X = std::max(hurtBoxEnd + minGroupSpacing, hurtX + hurtW + labelPadding);
			float spc12BoxEnd = spc12X + (2 * smallBoxSize + 1 * boxSpacing);

			float clashX = std::max(spc12BoxEnd + minGroupSpacing, spc12X + spc12W + labelPadding);
			float clashBoxEnd = clashX + smallBoxSize;

			float projX = std::max(clashBoxEnd + minGroupSpacing, clashX + clashW + labelPadding);
			float projBoxEnd = projX + smallBoxSize;

			float spc516X = std::max(projBoxEnd + minGroupSpacing, projX + projW + labelPadding);
			float spc516BoxEnd = spc516X + (12 * smallBoxSize + 11 * boxSpacing);

			float atkX = std::max(spc516BoxEnd + minGroupSpacing, spc516X + spc516W + labelPadding);

			// Draw labels on first line, aligned with first box of each category
			im::SetCursorPosX(im::GetCursorPosX() + colX);
			im::Text("Col");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (hurtX - colX - colW));
			im::Text("Hurt");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (spc12X - hurtX - hurtW));
			im::Text("Spc1-2");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (clashX - spc12X - spc12W));
			im::Text("Clash");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (projX - clashX - clashW));
			im::Text("Proj");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (spc516X - projX - projW));
			im::Text("Spc5-16");
			im::SameLine(0, 0);
			im::SetCursorPosX(im::GetCursorPosX() + (atkX - spc516X - spc516W));
			im::Text("Atk");

			// Move cursor down with minimal spacing (2px gap between labels and boxes)
			ImVec2 labelEnd = im::GetCursorScreenPos();
			im::SetCursorScreenPos(ImVec2(startX, labelEnd.y + 2.0f));

			// Draw boxes on second line at calculated positions
			float xPos = startX + colX;
			DrawBoxes(0, 1, whiteColor, xPos);
			xPos = startX + hurtX;
			DrawBoxes(1, 8, greenColor, xPos);
			xPos = startX + spc12X;
			DrawBoxes(9, 2, blueColor, xPos);
			xPos = startX + clashX;
			DrawBoxes(11, 1, yellowColor, xPos);
			xPos = startX + projX;
			DrawBoxes(12, 1, cyanColor, xPos);
			xPos = startX + spc516X;
			DrawBoxes(13, 12, purpleColor, xPos);
			xPos = startX + atkX;
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

			im::SameLine(0,20.f);
			im::Checkbox("Highlight selected", &highlight);
			im::SameLine(0,20.f);
			if(im::Button("Delete selected"))
			{
				boxes.erase(currentBox);
				frameData->mark_modified(currState.pattern);
				markModified();
			}
			im::SameLine(0,20.f);
			im::Checkbox("Show manual box controls", &showManualControls);
			im::SameLine(0,20.f);
			im::Checkbox("Position tool", &positionTool);
			if(im::IsItemHovered())
				im::SetTooltip("Drag the handles in the viewport to move animation layers and\n"
					"spawn/preset effects on this keyframe. Alt: precision, Shift: axis lock,\n"
					"Esc: cancel. Each drag is one undo step.");

			if(highlight)
				render->highLightN = currentBox;
			else
				render->highLightN = -1;
			if(showManualControls)
			{
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
		} // End Box Controls

		// Draw spawn timeline
		im::Separator();
		DrawSpawnTimeline();

		// Move-script spawns for this pattern (MBTL chrXXX_mv_*.txt)
		DrawScriptSpawnList(frameData, currState);
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

	if(!currState.vizSettings.showSpawnedPatterns) {
		return;
	}

	if(!im::CollapsingHeader("Spawn Timeline")) {
		return;
	}

	auto mainSeq = frameData->get_sequence(currState.pattern);
	if(!mainSeq || mainSeq->frames.empty()) return;
	const int mainFrameCount = (int)mainSeq->frames.size();

	// Everything below reads the cached tick simulator: the same actors the
	// viewport draws, with lifetimes from the simulated runtime flow.
	auto& sim = currState.BindPreviewSim(frameData, effectFrameData);
	const int settled = sim.settledTick();
	const int rootEnd = sim.rootEndTick();
	int maxTimelineTick;
	if (settled >= 0) {
		maxTimelineTick = std::max(settled + 1, 60);
	} else {
		// Something loops forever (root or a child): show a few root loops.
		int period = FindLoopPeriod(frameData, currState.pattern);
		maxTimelineTick = std::clamp(period * 3, 240, sim.horizon());
	}
	sim.ensureSimulatedTo(maxTimelineTick);
	const auto& track = sim.rootFrameTrack();

	// Runtime IF assumption (global)
	{
		int mode = currState.previewOptions.defaultIfAssumption == preview::IfAssume::True ? 1 : 0;
		const char* modes[] = {"Assume false (authored flow)", "Assume true (branch once per frame visit)"};
		im::SetNextItemWidth(260.f);
		if (im::Combo("Runtime IF conditions", &mode, modes, 2)) {
			currState.previewOptions.defaultIfAssumption = mode ? preview::IfAssume::True : preview::IfAssume::False;
		}
		if (im::IsItemHovered())
			im::SetTooltip("Conditions the preview cannot evaluate (input, hit, distance, landing, ...).\n"
			               "Loop counters (IF 9/10), IF 37, IF 55 and parent-pattern checks are always simulated.\n"
			               "Per-condition overrides are in 'Runtime conditions' below.");
	}

	im::Text("Frame: %d / %d  |  Tick: %d  |  Root ends: %s  |  Settles: %s", currState.frame, mainFrameCount - 1,
	         currState.currentTick,
	         rootEnd >= 0 ? std::to_string(rootEnd).c_str() : "loops",
	         settled >= 0 ? std::to_string(settled).c_str() : "never (looping actor)");

	// Tick scrubber: seeking is O(checkpoint interval) in the simulator.
	int scrubTick = std::min(currState.currentTick, maxTimelineTick);
	if (im::SliderInt("##tickscrub", &scrubTick, 0, maxTimelineTick, "Tick %d")) {
		currState.animating = false;
		currState.currentTick = scrubTick;
		if (scrubTick < (int)track.size())
			currState.frame = std::clamp(track[scrubTick], 0, mainFrameCount - 1);
	}

	im::Separator();

	const float rowHeight = 20.0f;
	const float labelWidth = 120.0f;
	const float avail = std::max(200.0f, im::GetContentRegionAvail().x - labelWidth - 8.0f);
	const float tickWidth = std::clamp(avail / (float)std::max(1, maxTimelineTick), 0.25f, 4.0f);

	ImDrawList* drawList = im::GetWindowDrawList();
	ImVec2 startPos = im::GetCursorScreenPos();
	float yPos = startPos.y;

	// Main pattern row
	im::Text("Main");
	im::SameLine(labelWidth);
	ImVec2 timelineStart = im::GetCursorScreenPos();
	{
		ImVec2 barMin(timelineStart.x, yPos);
		int liveEnd = rootEnd >= 0 ? rootEnd : maxTimelineTick;
		drawList->AddRectFilled(barMin, ImVec2(timelineStart.x + liveEnd * tickWidth, yPos + rowHeight), IM_COL32(80, 80, 255, 200));
		if (rootEnd >= 0 && rootEnd < maxTimelineTick)
			drawList->AddRectFilled(ImVec2(timelineStart.x + rootEnd * tickWidth, yPos),
			                        ImVec2(timelineStart.x + maxTimelineTick * tickWidth, yPos + rowHeight), IM_COL32(60, 60, 90, 160));
		drawList->AddRect(barMin, ImVec2(timelineStart.x + maxTimelineTick * tickWidth, yPos + rowHeight), IM_COL32(255, 255, 255, 255));
		// Exact frame entries from the simulated root track
		for (int t = 1; t < (int)track.size() && t <= maxTimelineTick; t++) {
			if (track[t] != track[t - 1]) {
				float fx = timelineStart.x + t * tickWidth;
				drawList->AddLine(ImVec2(fx, yPos), ImVec2(fx, yPos + rowHeight), IM_COL32(200, 200, 255, 110), 1.0f);
			}
		}
	}
	float currentX = timelineStart.x + currState.currentTick * tickWidth;
	drawList->AddLine(ImVec2(currentX, yPos), ImVec2(currentX, yPos + rowHeight), IM_COL32(255, 255, 0, 255), 2.0f);

	yPos += rowHeight + 4;
	im::SetCursorScreenPos(ImVec2(startPos.x, yPos));

	// Group every simulated actor under its spawn-tree entry (right pane).
	struct Row {
		const SpawnedPatternInfo* entry = nullptr;
		int pattern = -1;
		bool effect = false, preset = false, script = false;
		int depth = 1;
		std::vector<const preview::SpawnRecord*> recs;
	};
	std::vector<Row> rows;
	auto rowFor = [&](const preview::SpawnRecord& r) -> Row& {
		const SpawnedPatternInfo* e = FindSpawnTreeEntry(currState.spawnedPatterns, r.srcPattern, r.srcFrame,
			r.srcEffectIndex, r.effectHa6, r.isScript, r.pattern);
		for (auto& row : rows) {
			if (e ? row.entry == e
			      : (!row.entry && row.pattern == r.pattern && row.effect == r.effectHa6 &&
			         row.preset == r.isPreset && row.depth == r.depth))
				return row;
		}
		Row row;
		row.entry = e; row.pattern = r.pattern; row.effect = r.effectHa6;
		row.preset = r.isPreset; row.script = r.isScript; row.depth = r.depth;
		rows.push_back(row);
		return rows.back();
	};
	for (const auto& r : sim.spawnRecords()) {
		if (r.id == 0 || r.spawnTick > maxTimelineTick) continue;
		rowFor(r).recs.push_back(&r);
	}
	// Tree order first, then unlisted rows by first spawn.
	auto treeIndex = [&](const Row& row) -> size_t {
		return row.entry ? (size_t)(row.entry - currState.spawnedPatterns.data()) : (size_t)-1;
	};
	std::stable_sort(rows.begin(), rows.end(), [&](const Row& x, const Row& y) {
		size_t ix = treeIndex(x), iy = treeIndex(y);
		if (ix != iy) return ix < iy;
		return x.recs.front()->spawnTick < y.recs.front()->spawnTick;
	});

	for (const auto& row : rows) {
		FrameData* sourceData = row.effect ? effectFrameData : frameData;
		std::string patternName;
		if (row.preset && row.script) {
			patternName = "Impact FX";
		} else if (row.preset) {
			extern const char* GetPresetEffectName(int);
			patternName = std::string(GetPresetEffectName(row.pattern)) + " [" + std::to_string(row.pattern) + "]";
		} else {
			patternName = sourceData ? sourceData->GetDecoratedName(row.pattern) : std::to_string(row.pattern);
		}
		const int depth = row.entry ? row.entry->depth + 1 : row.depth;

		im::SetCursorScreenPos(ImVec2(startPos.x + (depth - 1) * 10.0f, yPos));
		std::string label = std::string(depth, '>') + " " + patternName;
		if (label.length() > 14) label = label.substr(0, 14) + "..";
		if (!row.entry) label += " *";
		if (row.script) im::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "%s", label.c_str());
		else im::Text("%s", label.c_str());
		if (im::IsItemHovered()) {
			std::string tip = patternName + (row.effect ? " (effect.ha6)" : "") +
				"\n" + std::to_string(row.recs.size()) + " instance(s)";
			if (!row.entry) tip += "\n* not in the spawn tree (random pick, pattern chain or nested repeat)";
			if (row.script && row.entry && !row.entry->scriptSource.empty()) tip += "\n" + row.entry->scriptSource;
			im::SetTooltip("%s", tip.c_str());
		}
		im::SameLine(labelWidth);

		const glm::vec4 tint = row.entry ? row.entry->tintColor : glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
		for (size_t k = 0; k < row.recs.size(); k++) {
			const auto* r = row.recs[k];
			float sx = timelineStart.x + r->spawnTick * tickWidth;
			if (row.preset) {
				ImU32 markerColor = IM_COL32(255, 128, 0, 255);
				drawList->AddLine(ImVec2(sx, yPos), ImVec2(sx, yPos + rowHeight), markerColor, 3.0f);
				drawList->AddCircleFilled(ImVec2(sx, yPos + rowHeight / 2), 4.0f, markerColor);
				continue;
			}
			int endTick = r->deathTick >= 0 ? std::min(r->deathTick, maxTimelineTick) : maxTimelineTick;
			float ex = timelineStart.x + std::max(endTick, r->spawnTick + 1) * tickWidth;
			ImU32 barColor = IM_COL32(tint.r * 255, tint.g * 255, tint.b * 255, k == 0 ? 180 : 120);
			drawList->AddRectFilled(ImVec2(sx, yPos), ImVec2(ex, yPos + rowHeight), barColor);
			drawList->AddRect(ImVec2(sx, yPos), ImVec2(ex, yPos + rowHeight), IM_COL32(255, 255, 255, 200));
			drawList->AddCircleFilled(ImVec2(sx, yPos + rowHeight / 2), k == 0 ? 3.0f : 2.0f, IM_COL32(255, 255, 255, 255));
			if (r->deathTick < 0 || r->deathTick > maxTimelineTick) {
				// Still alive at the end of the timeline
				drawList->AddTriangleFilled(ImVec2(ex - 5, yPos + 3), ImVec2(ex - 5, yPos + rowHeight - 3),
				                            ImVec2(ex, yPos + rowHeight / 2), IM_COL32(255, 255, 255, 200));
			}
		}
		if (row.entry && !row.preset && row.entry->isRecursive) {
			im::SameLine();
			im::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "REC");
		}

		yPos += rowHeight + 2;
		im::SetCursorScreenPos(ImVec2(startPos.x, yPos));
	}

	// Global current tick line
	drawList->AddLine(ImVec2(currentX, startPos.y), ImVec2(currentX, yPos), IM_COL32(255, 255, 0, 128), 1.0f);
	im::SetCursorScreenPos(ImVec2(startPos.x, yPos + 10));

	// Runtime conditions met so far: per-IF assumption overrides.
	std::vector<const preview::SimEvent*> conds;
	for (const auto& e : sim.events()) {
		if (e.kind != preview::EventKind::IfUnmodeled && e.kind != preview::EventKind::IfAssumedTrue) continue;
		bool dup = false;
		for (auto* c : conds) if (c->ifKey == e.ifKey) { dup = true; break; }
		if (!dup) conds.push_back(&e);
	}
	char header[64];
	snprintf(header, sizeof(header), "Runtime conditions (%d)###simconds", (int)conds.size());
	if (!conds.empty() && im::TreeNode(header)) {
		im::TextDisabled("Not modeled by the preview; choose how each one resolves.");
		for (auto* c : conds) {
			im::PushID(c->ifKey.pattern * 131 + c->ifKey.frame * 7 + c->ifKey.index + (c->ifKey.effectHa6 ? 1000000 : 0));
			auto it = currState.previewOptions.ifOverrides.find(c->ifKey);
			int v = it == currState.previewOptions.ifOverrides.end() ? 0 : (int)it->second;
			const char* opts[] = {"Default", "False", "True"};
			im::SetNextItemWidth(80.f);
			if (im::Combo("##assume", &v, opts, 3)) {
				if (v == 0) currState.previewOptions.ifOverrides.erase(c->ifKey);
				else currState.previewOptions.ifOverrides[c->ifKey] = (preview::IfAssume)v;
			}
			im::SameLine();
			const char* act = c->a == 1 ? "jump to frame" : c->a == 2 ? "queue pattern" : c->a == 3 ? "destroy" : "?";
			if (c->a == 3)
				im::Text("IF %d  %spat %d fr %d #%d -> %s", c->ifType, c->ifKey.effectHa6 ? "[fx] " : "",
				         c->ifKey.pattern, c->ifKey.frame, c->ifKey.index, act);
			else
				im::Text("IF %d  %spat %d fr %d #%d -> %s %d", c->ifType, c->ifKey.effectHa6 ? "[fx] " : "",
				         c->ifKey.pattern, c->ifKey.frame, c->ifKey.index, act, c->b);
			im::PopID();
		}
		im::TreePop();
	}
}