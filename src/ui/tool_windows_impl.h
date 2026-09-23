#ifndef UI_TOOL_WINDOWS_IMPL_H_GUARD
#define UI_TOOL_WINDOWS_IMPL_H_GUARD

// ============================================================================
// Tool windows opened from the Tools menu: variable batch replace (#75).
// Included at the end of main_frame.cpp.
// ============================================================================

#include "../var_refs.h"

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

#endif /* UI_TOOL_WINDOWS_IMPL_H_GUARD */
