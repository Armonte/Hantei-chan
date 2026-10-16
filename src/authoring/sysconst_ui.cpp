#include "sysconst_ui.h"
#include "../ui/ui_style.h"

#include "../i18n.h"
#include "../misc.h"       // sj2utf8
#include "../png_writer.h" // BrowseForFolderUtf8

#include <imgui.h>

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace sysconst {

namespace {

const ImVec4 kChanged(1.0f, 0.75f, 0.3f, 1.0f);

std::string Join(const std::string& dir, const char* name) { return (fs::u8path(dir) / name).u8string(); }

// A small inline text field; returns true with the new text on Enter or focus loss after an edit.
bool EditCell(const char* id, const std::string& value, std::string& out, float width)
{
	char buf[64];
	snprintf(buf, sizeof buf, "%s", value.c_str());
	ImGui::SetNextItemWidth(width);
	ImGui::InputText(id, buf, sizeof buf);
	if (ImGui::IsItemDeactivatedAfterEdit() && value != buf) {
		out = buf;
		return true;
	}
	return false;
}

} // namespace

bool Window::openFolder(const std::string& dataDirUtf8, std::string* error)
{
	m_dir = dataDirUtf8;
	m_hasType = m_type.load(Join(m_dir, "_type.txt"));
	m_hasVec = m_vec.load(Join(m_dir, "vector.txt"));
	if (!m_hasType && !m_hasVec) {
		if (error) *error = std::string(TXT("No _type.txt or vector.txt in")) + " " + dataDirUtf8;
		return false;
	}
	m_status.clear();
	open = true;
	return true;
}

bool Window::save()
{
	bool ok = true;
	if (m_hasType && m_type.dirty()) ok &= m_type.save(Join(m_dir, "_type.txt"));
	if (m_hasVec && m_vec.dirty()) ok &= m_vec.save(Join(m_dir, "vector.txt"));
	m_status = ok ? TXT("Saved.") : TXT("Save failed.");
	m_statusError = !ok;
	return ok;
}

void Window::draw()
{
	if (!open) return;
	ui::FitNextWindow(900, 620);
	if (!ImGui::Begin(LBL("System constants (_type.txt / vector.txt)"), &open)) {
		ImGui::End();
		return;
	}
	if (ImGui::Button(LBL("Open data folder..."))) {
		const std::string d = BrowseForFolderUtf8(m_dir);
		std::string err;
		if (!d.empty() && !openFolder(d, &err)) { m_status = err; m_statusError = true; }
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Compare with vanilla folder..."))) {
		const std::string d = BrowseForFolderUtf8(m_vanillaDir.empty() ? m_dir : m_vanillaDir);
		if (!d.empty()) {
			m_vanillaDir = d;
			const bool t = m_typeBase.load(Join(d, "_type.txt")), v = m_vecBase.load(Join(d, "vector.txt"));
			m_hasBase = t || v;
			if (!m_hasBase) { m_status = TXT("No _type.txt or vector.txt in that folder."); m_statusError = true; }
		}
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!dirty());
	if (ImGui::Button(LBL("Save"))) save();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Checkbox(LBL("Only changed"), &m_onlyChanged);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160);
	char f[64];
	snprintf(f, sizeof f, "%s", m_filter.c_str());
	if (ImGui::InputTextWithHint("##scfilter", TXT("filter"), f, sizeof f)) m_filter = f;
	ImGui::TextDisabled("%s", m_dir.empty() ? TXT("No folder open.") : m_dir.c_str());
	if (m_hasBase) { ImGui::SameLine(); ImGui::TextDisabled("| %s %s", TXT("vanilla:"), m_vanillaDir.c_str()); }
	if (!m_status.empty()) {
		ImGui::SameLine();
		ImGui::TextColored(m_statusError ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.6f, 0.9f, 0.6f, 1), "%s", m_status.c_str());
	}
	if (ImGui::BeginTabBar("##sctabs")) {
		if (m_hasType && ImGui::BeginTabItem(LBL("_type.txt"))) { drawType(); ImGui::EndTabItem(); }
		if (m_hasVec && ImGui::BeginTabItem(LBL("vector.txt"))) { drawVector(); ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S, false) && dirty())
		save();
	ImGui::End();
}

void Window::drawType()
{
	if (!ImGui::BeginTable("##type", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		return;
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn(TXT("Key"), ImGuiTableColumnFlags_WidthFixed, 200);
	ImGui::TableSetupColumn(TXT("Value"), ImGuiTableColumnFlags_WidthFixed, 120);
	ImGui::TableSetupColumn(TXT("Vanilla"), ImGuiTableColumnFlags_WidthFixed, 90);
	ImGui::TableSetupColumn(TXT("Comment"), ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();
	std::string section = "\x01";
	std::string setSec, setKey, setVal;
	for (size_t i = 0; i < m_type.keys().size(); i++) {
		const TypeKey& k = m_type.keys()[i];
		const TypeKey* base = m_hasBase ? m_typeBase.find(k.section, k.key) : nullptr;
		const bool changed = m_hasBase && (!base || base->value != k.value);
		if (m_onlyChanged && !changed) continue;
		if (!m_filter.empty() && k.key.find(m_filter) == std::string::npos && k.section.find(m_filter) == std::string::npos) continue;
		if (k.section != section) {
			section = k.section;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "[%s]", section.c_str());
		}
		ImGui::PushID(static_cast<int>(i));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (changed) ImGui::TextColored(kChanged, "%s", k.key.c_str());
		else ImGui::TextUnformatted(k.key.c_str());
		ImGui::TableNextColumn();
		std::string nv;
		if (EditCell("##v", k.value, nv, -1)) { setSec = k.section; setKey = k.key; setVal = nv; }
		ImGui::TableNextColumn();
		if (m_hasBase) ImGui::TextDisabled("%s", base ? base->value.c_str() : TXT("(new)"));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(sj2utf8(k.comment).c_str());
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (!setKey.empty() && !m_type.setValue(setSec, setKey, setVal)) { m_status = TXT("Edit failed."); m_statusError = true; }
}

void Window::drawVector()
{
	const auto& rows = m_vec.rows();
	// Vanilla row of the same kind and id, for highlighting.
	auto baseRow = [&](size_t r) -> int {
		if (!m_hasBase) return -1;
		const auto& br = m_vecBase.rows();
		const int id = m_vec.idOf(r);
		const VectorFile::Kind kind = rows[r].kind;
		if (kind != VectorFile::Step) {
			for (size_t i = 0; i < br.size(); i++) if (br[i].kind == kind && m_vecBase.idOf(i) == id) return static_cast<int>(i);
			return -1;
		}
		// Step rows: same position under the header with the same No.
		const int headId = m_vec.idOf(static_cast<size_t>(rows[r].owner));
		const size_t nth = r - static_cast<size_t>(rows[r].owner);
		for (size_t i = 0; i < br.size(); i++)
			if (br[i].kind == VectorFile::Header && m_vecBase.idOf(i) == headId)
				return i + nth < br.size() && br[i + nth].kind == VectorFile::Step ? static_cast<int>(i + nth) : -1;
		return -1;
	};
	size_t setRow = 0, setIdx = 0;
	std::string setVal;
	bool doSet = false;
	auto drawRow = [&](size_t r, int ncols) {
		const int b = baseRow(r);
		for (size_t t = 0; t < static_cast<size_t>(ncols); t++) {
			ImGui::TableNextColumn();
			if (t >= rows[r].tok.size()) continue;
			const std::string v = m_vec.field(r, t);
			const bool changed = m_hasBase && (b < 0 || m_vecBase.field(static_cast<size_t>(b), t) != v);
			if (changed) ImGui::PushStyleColor(ImGuiCol_Text, kChanged);
			ImGui::PushID(static_cast<int>(t));
			std::string nv;
			const bool isName = rows[r].kind == VectorFile::Header && t == 1;
			if (isName) ImGui::TextUnformatted(sj2utf8(v).c_str());
			else if (EditCell("##f", v, nv, -1)) { setRow = r; setIdx = t; setVal = nv; doSet = true; }
			ImGui::PopID();
			if (changed) ImGui::PopStyleColor();
			if (changed && b >= 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s %s", TXT("vanilla:"), m_vecBase.field(static_cast<size_t>(b), t).c_str());
		}
	};
	auto rowChanged = [&](size_t r) {
		const int b = baseRow(r);
		if (b < 0) return m_hasBase;
		for (size_t t = 0; t < rows[r].tok.size(); t++) if (m_vecBase.field(static_cast<size_t>(b), t) != m_vec.field(r, t)) return true;
		return false;
	};

	if (ImGui::BeginTabBar("##vtabs")) {
		if (ImGui::BeginTabItem(LBL("Hit vectors"))) {
			if (ImGui::BeginTable("##hv", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
				ImGui::TableSetupScrollFreeze(0, 1);
				for (const char* c : VectorFile::kHeaderFields) ImGui::TableSetupColumn(c, 0, 70);
				ImGui::TableSetupColumn(TXT("Used by"), 0, 60);
				ImGui::TableSetupColumn(TXT("Comment"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();
				for (size_t r = 0; r < rows.size(); r++) {
					if (rows[r].kind == VectorFile::List) continue;
					if (rows[r].kind == VectorFile::Header) {
						bool any = rowChanged(r);
						for (size_t s = r + 1; s < rows.size() && rows[s].kind == VectorFile::Step; s++) any |= rowChanged(s);
						if (m_onlyChanged && !any) continue;
						if (!m_filter.empty() && sj2utf8(m_vec.field(r, 1)).find(m_filter) == std::string::npos && m_vec.field(r, 0) != m_filter) continue;
					} else {
						const size_t h = static_cast<size_t>(rows[r].owner);
						bool any = rowChanged(h);
						for (size_t s = h + 1; s < rows.size() && rows[s].kind == VectorFile::Step; s++) any |= rowChanged(s);
						if (m_onlyChanged && !any) continue;
						if (!m_filter.empty() && sj2utf8(m_vec.field(h, 1)).find(m_filter) == std::string::npos && m_vec.field(h, 0) != m_filter) continue;
					}
					ImGui::PushID(static_cast<int>(r));
					ImGui::TableNextRow();
					if (rows[r].kind == VectorFile::Header) {
						drawRow(r, 7);
						ImGui::TableNextColumn();
						if (vectorUses) ImGui::Text("%d", vectorUses(m_vec.idOf(r)));
					} else {
						ImGui::TableNextColumn();
						ImGui::TextDisabled("  %s", TXT("step"));
						if (ImGui::IsItemHovered()) {
							std::string tip;
							for (const char* c : VectorFile::kStepFields) tip += std::string(c) + "  ";
							ImGui::SetTooltip("%s", tip.c_str());
						}
						// Step fields sit under columns 1..6 (VecNum HitAni GuardAni Time VecTime flag; hover "step").
						drawRow(r, 6);
						ImGui::TableNextColumn();
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", sj2utf8(rows[r].comment).c_str());
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(LBL("Vector list"))) {
			if (ImGui::BeginTable("##vl", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
				ImGui::TableSetupScrollFreeze(0, 1);
				for (const char* c : VectorFile::kListFields) ImGui::TableSetupColumn(c, 0, 80);
				ImGui::TableSetupColumn(TXT("Comment"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();
				for (size_t r = 0; r < rows.size(); r++) {
					if (rows[r].kind != VectorFile::List) continue;
					if (m_onlyChanged && !rowChanged(r)) continue;
					if (!m_filter.empty() && m_vec.field(r, 0) != m_filter && sj2utf8(rows[r].comment).find(m_filter) == std::string::npos) continue;
					ImGui::PushID(static_cast<int>(r));
					ImGui::TableNextRow();
					drawRow(r, 5);
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", sj2utf8(rows[r].comment).c_str());
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	if (doSet && !m_vec.setField(setRow, setIdx, setVal)) { m_status = TXT("Edit failed: one value, no spaces."); m_statusError = true; }
}

} // namespace sysconst
