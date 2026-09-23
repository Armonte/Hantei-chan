#ifndef UI_TOOL_WINDOWS_IMPL_H_GUARD
#define UI_TOOL_WINDOWS_IMPL_H_GUARD

// ============================================================================
// Tool windows opened from the Tools menu: variable batch replace (#75).
// Included at the end of main_frame.cpp.
// ============================================================================

#include "../var_refs.h"
#include "../pattern_refs.h"

// Commit a tool edit as one undo step and flag the character dirty.
void MainFrame::markToolEdit(CharacterInstance* character)
{
	if (!character) return;
	character->markModified();
	character->undoManager.markModified();
	for (auto& v : views) {
		if (v->getCharacter() != character) continue;
		v->getState().forceSpawnTreeRebuild = true;
		if (v->getMainPane()) v->getMainPane()->RegenerateNames();
	}
}

void MainFrame::navigateActiveView(int pattern, int frame)
{
	auto* view = getActiveView();
	auto* character = getActiveCharacter();
	if (!view || !character) return;
	auto& st = view->getState();
	st.animating = false;
	st.pattern = pattern;
	st.frame = frame < 0 ? 0 : frame;
	Sequence* seq = character->frameData.get_sequence(pattern);
	const int frames = seq ? (int)seq->frames.size() : 0;
	if (st.frame >= frames) st.frame = frames > 0 ? frames - 1 : 0;
	st.currentTick = frames > 0 ? CalculateTickFromFrame(&character->frameData, st.pattern, st.frame) : 0;
	st.activeSpawns.clear();
	if (view->getMainPane()) view->getMainPane()->RegenerateNames();
}

void MainFrame::drawVarRefsWindow()
{
	if (!m_varRefs.open) return;
	auto* character = getActiveCharacter();
	ImGui::SetNextWindowSize(ImVec2(620, 460), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Variable references", &m_varRefs.open)) { ImGui::End(); return; }
	if (!character) {
		ImGui::TextDisabled("Open a character first.");
		ImGui::End();
		return;
	}
	auto& w = m_varRefs;
	ImGui::TextWrapped("Find every effect/condition parameter that names a variable ID, and rename it in one step.");
	ImGui::CheckboxFlags("Variables (EF2/105, IF25, IF31, IF38)", &w.categories, varrefs::catVariable);
	ImGui::CheckboxFlags("Projectile variables (EF1/101/1000/11/111, EF2/100-101, IF2, IF3, IF24)", &w.categories, varrefs::catProjectile);
	ImGui::CheckboxFlags("Spawn-once guard (EF1000 p6)", &w.categories, varrefs::catOnceGuard);
	ImGui::SetNextItemWidth(90);
	ImGui::InputInt("Variable ID", &w.from, 0, 0);
	ImGui::SameLine();
	if (ImGui::Button("Find")) {
		w.results = varrefs::Find(character->frameData, w.from, w.categories);
		w.searched = true;
		w.status.clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("List all IDs")) {
		w.results = varrefs::Find(character->frameData, -1, w.categories);
		w.searched = true;
		w.status.clear();
	}
	ImGui::SetNextItemWidth(90);
	ImGui::InputInt("Replace with", &w.to, 0, 0);
	ImGui::SameLine();
	if (ImGui::Button("Replace all")) {
		int skipped = 0;
		const int n = varrefs::Replace(character->frameData, w.from, w.to, w.categories, &skipped);
		if (n > 0) markToolEdit(character);
		w.status = "Replaced " + std::to_string(n) + " reference(s) to " + std::to_string(w.from) +
			" with " + std::to_string(w.to) + " (one undo step).";
		if (skipped) w.status += " Skipped " + std::to_string(skipped) + " value(s) that cannot hold that ID (negative in a tens place, or 0 where 0 means none).";
		w.results = varrefs::Find(character->frameData, w.to, w.categories);
	}
	if (!w.status.empty()) ImGui::TextWrapped("%s", w.status.c_str());
	ImGui::Separator();
	if (w.searched) {
		ImGui::Text("%zu reference(s). Click one to go to it.", w.results.size());
		if (ImGui::BeginChild("##varrefs", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
			ImGuiListClipper clip;
			clip.Begin((int)w.results.size());
			while (clip.Step()) {
				for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
					const auto& r = w.results[i];
					ImGui::PushID(i);
					if (ImGui::Selectable(varrefs::Describe(r).c_str()))
						navigateActiveView(r.pattern, r.frame);
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

// ---------------------------------------------------------------------------
// Pattern manager (#42, #44): select patterns, copy/paste them (also across
// tabs and instances), move them with reference remapping, list callers.
// ---------------------------------------------------------------------------

void MainFrame::drawPatternManagerWindow()
{
	auto& w = m_patMgr;
	if (!w.open) return;
	auto* character = getActiveCharacter();
	auto* view = getActiveView();
	ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Pattern manager", &w.open)) { ImGui::End(); return; }
	if (!character || !view || view->isStageView()) {
		ImGui::TextDisabled("Open a character first.");
		ImGui::End();
		return;
	}
	FrameData& fd = character->frameData;
	const int count = fd.get_sequence_count();
	if (w.character != character) { w.selection.clear(); w.character = character; w.lastClicked = -1; }
	w.selection.erase(std::remove_if(w.selection.begin(), w.selection.end(),
		[&](int p) { return p < 0 || p >= count; }), w.selection.end());
	auto isSelected = [&](int p) { return std::find(w.selection.begin(), w.selection.end(), p) != w.selection.end(); };

	// ---- left: pattern list ----
	ImGui::BeginChild("##patlist", ImVec2(340, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##filter", "Filter by number or name", w.filter, sizeof(w.filter));
	ImGui::Checkbox("Hide empty slots", &w.hideEmpty);
	ImGui::TextDisabled("Click, Ctrl+click, Shift+click. Order = click order.");
	std::vector<int> rows;
	std::string f = w.filter;
	for (auto& c : f) c = (char)tolower((unsigned char)c);
	for (int p = 0; p < count; ++p) {
		if (w.hideEmpty && patrefs::IsEmptySlot(fd, p)) continue;
		if (!f.empty()) {
			std::string name = fd.GetDecoratedName(p);
			for (auto& c : name) c = (char)tolower((unsigned char)c);
			if (name.find(f) == std::string::npos) continue;
		}
		rows.push_back(p);
	}
	if (ImGui::BeginChild("##rows")) {
		ImGuiListClipper clip;
		clip.Begin((int)rows.size());
		while (clip.Step()) {
			for (int r = clip.DisplayStart; r < clip.DisplayEnd; ++r) {
				const int p = rows[r];
				const bool sel = isSelected(p);
				std::string label = fd.GetDecoratedName(p);
				if (sel) {
					const int order = (int)(std::find(w.selection.begin(), w.selection.end(), p) - w.selection.begin()) + 1;
					label = "[" + std::to_string(order) + "] " + label;
				}
				ImGui::PushID(p);
				if (ImGui::Selectable(label.c_str(), sel)) {
					const ImGuiIO& io = ImGui::GetIO();
					if (io.KeyShift && w.lastClicked >= 0) {
						const int a = std::min(w.lastClicked, p), b = std::max(w.lastClicked, p);
						for (int q : rows) if (q >= a && q <= b && !isSelected(q)) w.selection.push_back(q);
					} else if (io.KeyCtrl) {
						if (sel) w.selection.erase(std::find(w.selection.begin(), w.selection.end(), p));
						else w.selection.push_back(p);
					} else {
						w.selection.assign(1, p);
					}
					w.lastClicked = p;
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) navigateActiveView(p, 0);
				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();
	ImGui::EndChild();
	ImGui::SameLine();

	// ---- right: operations ----
	ImGui::BeginChild("##ops");
	ImGui::Text("%zu selected", w.selection.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear selection")) w.selection.clear();
	ImGui::SameLine();
	if (ImGui::SmallButton("Select current")) { w.selection.assign(1, view->getState().pattern); w.lastClicked = view->getState().pattern; }

	CopyData* clip = view->getState().copied;
	ImGui::SeparatorText("Clipboard");
	if (ImGui::Button("Copy selected") && !w.selection.empty()) {
		clip->patterns.clear();
		clip->patternIds.clear();
		for (int p : w.selection) {
			Sequence_T<LinearAllocator> t;
			t = *fd.get_sequence(p);
			clip->patterns.push_back(t);
			clip->patternIds.push_back(p);
		}
		snprintf(clip->patternSource, sizeof(clip->patternSource), "%s", character->getName().c_str());
		w.status = "Copied " + std::to_string(w.selection.size()) + " pattern(s).";
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%zu pattern(s) from %s", clip->patterns.size(),
		clip->patternSource[0] ? clip->patternSource : "-");

	static const char* placements[] = {
		"Next empty slots from target", "Consecutive from target (overwrite)", "Original ids (overwrite)" };
	ImGui::SetNextItemWidth(260);
	ImGui::Combo("Placement", &w.placement, placements, IM_ARRAYSIZE(placements));
	ImGui::SetNextItemWidth(90);
	ImGui::InputInt("Target slot", &w.target, 0, 0);
	ImGui::SameLine();
	if (ImGui::SmallButton("= current pattern")) w.target = view->getState().pattern;
	w.target = std::clamp(w.target, 0, std::max(0, count - 1));
	ImGui::Checkbox("Remap references between the pasted patterns", &w.remapPaste);
	ImGui::Checkbox("Move: update every reference in the character", &w.remapMove);

	const auto how = (patrefs::Placement)w.placement;
	// Preview for paste
	std::vector<int> clipIds(clip->patternIds.begin(), clip->patternIds.end());
	const auto pasteSlots = patrefs::PlanSlots(fd, (int)clip->patterns.size(), w.target, how, clipIds);
	int pasteOverwrites = 0, pasteDropped = 0;
	for (int s : pasteSlots) { if (s < 0) ++pasteDropped; else if (!patrefs::IsEmptySlot(fd, s)) ++pasteOverwrites; }
	ImGui::BeginDisabled(clip->patterns.empty());
	if (ImGui::Button("Paste")) {
		std::vector<Sequence> pats(clip->patterns.size());
		for (size_t i = 0; i < pats.size(); ++i) pats[i] = clip->patterns[i];
		const int refs = patrefs::PastePatterns(fd, pats, clipIds, pasteSlots, w.remapPaste);
		markToolEdit(character);
		w.refsFor = -1;
		w.status = "Pasted " + std::to_string(pats.size() - pasteDropped) + " pattern(s), " +
			std::to_string(refs) + " reference(s) remapped. One undo step.";
		for (int s : pasteSlots) if (s >= 0) { navigateActiveView(s, 0); break; }
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%d overwrite(s)%s", pasteOverwrites, pasteDropped ? ", some past the last slot" : "");

	ImGui::SeparatorText("Selected patterns");
	const auto moveSlots = patrefs::PlanSlots(fd, (int)w.selection.size(), w.target, how, w.selection);
	int moveOverwrites = 0;
	for (int s : moveSlots)
		if (s >= 0 && !patrefs::IsEmptySlot(fd, s) && std::find(w.selection.begin(), w.selection.end(), s) == w.selection.end())
			++moveOverwrites;
	ImGui::BeginDisabled(w.selection.empty() || how == patrefs::Placement::OriginalIds);
	if (ImGui::Button("Move selected to target")) {
		std::vector<int> from, to;
		for (size_t i = 0; i < w.selection.size(); ++i) if (moveSlots[i] >= 0) { from.push_back(w.selection[i]); to.push_back(moveSlots[i]); }
		const int refs = patrefs::MovePatterns(fd, from, to, w.remapMove);
		markToolEdit(character);
		w.refsFor = -1;
		w.status = "Moved " + std::to_string(from.size()) + " pattern(s), " + std::to_string(refs) +
			" reference(s) updated. One undo step.";
		w.selection = to;
		if (!to.empty()) navigateActiveView(to.front(), 0);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%d non-selected pattern(s) would be overwritten", moveOverwrites);
	ImGui::BeginDisabled(w.selection.empty());
	if (ImGui::Button("Clear selected patterns")) ImGui::OpenPopup("Clear patterns?");
	ImGui::EndDisabled();
	if (ImGui::BeginPopupModal("Clear patterns?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Empty %zu pattern slot(s)? (Undo restores them.)", w.selection.size());
		if (ImGui::Button("Clear")) {
			for (int p : w.selection) { *fd.get_sequence(p) = Sequence{}; fd.mark_modified(p); }
			markToolEdit(character);
			w.refsFor = -1;
			w.status = "Cleared " + std::to_string(w.selection.size()) + " pattern(s).";
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (!w.status.empty()) ImGui::TextWrapped("%s", w.status.c_str());

	ImGui::SeparatorText("References to the first selected pattern");
	if (!w.selection.empty()) {
		const int target = w.selection.front();
		const bool refresh = ImGui::SmallButton("Refresh");
		ImGui::SameLine();
		if (refresh || w.refsFor != target || w.refsVersion != fd.dataVersion) {
			w.refs = patrefs::Find(fd, target);
			w.refsFor = target;
			w.refsVersion = fd.dataVersion;
		}
		ImGui::Text("%zu reference(s) to pattern %d", w.refs.size(), target);
		if (ImGui::BeginChild("##refs", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
			for (size_t i = 0; i < w.refs.size(); ++i) {
				ImGui::PushID((int)i);
				if (ImGui::Selectable(patrefs::Describe(w.refs[i]).c_str()))
					navigateActiveView(w.refs[i].pattern, w.refs[i].frame);
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
	ImGui::End();
}

// ---------------------------------------------------------------------------
// Notes (#58): every annotation of the active character.
// ---------------------------------------------------------------------------

void MainFrame::drawNotesWindow()
{
	if (!m_showNotes) return;
	auto* character = getActiveCharacter();
	ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Notes", &m_showNotes)) { ImGui::End(); return; }
	if (!character) { ImGui::TextDisabled("Open a character first."); ImGui::End(); return; }
	FrameData& fd = character->frameData;
	ImGui::TextWrapped("Notes on patterns (Pattern data > Pattern note) and on effects/conditions "
	                   "(right-click a record header). Saved with the character to %s.",
	                   character->getTopHA6Path().empty() ? "<ha6>.notes.json"
	                   : Ha6Notes::PathFor(character->getTopHA6Path()).c_str());
	if (!character->notesError().empty())
		ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Notes file not loaded (it will not be overwritten): %s",
		                   character->notesError().c_str());
	ImGui::Text("%zu note(s)%s", fd.notes.notes.size(), fd.notes.dirty ? " (unsaved)" : "");
	if (ImGui::BeginChild("##notes", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
		for (const auto& kv : fd.notes.notes) {
			int p, f, idx, type; bool isEf;
			if (!Ha6Notes::ParseKey(kv.first, &p, &f, &isEf, &idx, &type)) continue;
			std::string where = "Pattern " + std::to_string(p);
			bool matched = fd.get_sequence(p) != nullptr;
			if (f >= 0) {
				where += " frame " + std::to_string(f) + (isEf ? " EF#" : " IF#") + std::to_string(idx) +
					" (type " + std::to_string(type) + ")";
				Sequence* seq = fd.get_sequence(p);
				matched = seq && f < (int)seq->frames.size() &&
					(isEf ? idx < (int)seq->frames[f].EF.size() && seq->frames[f].EF[idx].type == type
					      : idx < (int)seq->frames[f].IF.size() && seq->frames[f].IF[idx].type == type);
			}
			if (!matched) where += "  [unmatched: record moved, deleted or retyped]";
			ImGui::PushID(kv.first.c_str());
			if (ImGui::Selectable(where.c_str())) navigateActiveView(p, f < 0 ? 0 : f);
			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", kv.second.c_str());
			ImGui::PopTextWrapPos();
			ImGui::Unindent();
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

#endif /* UI_TOOL_WINDOWS_IMPL_H_GUARD */
