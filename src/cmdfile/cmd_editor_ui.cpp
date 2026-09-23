#include "cmd_editor_ui.h"

#include "cmd_io.h"
#include "cmd_notes.h"
#include "cmd_validate.h"
#include "cmd_workspace.h"
#include "../extension_profile.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <set>

namespace cmdfile {

namespace {

const ImVec4 kErrorColor(1.0f, 0.36f, 0.30f, 1.0f);
const ImVec4 kWarnColor(1.0f, 0.72f, 0.20f, 1.0f);
const ImVec4 kOkColor(0.45f, 0.85f, 0.45f, 1.0f);
const ImVec4 kCommentColor(0.62f, 0.72f, 0.52f, 1.0f);
const ImVec4 kLinkColor(0.40f, 0.78f, 1.0f, 1.0f);

// Flagset bit meanings (bit 0 = rightmost digit). Sources: gHantei_Docs/Command_file(_c.txt).md,
// the Japanese header comments shipped in the _c files, and LoadCharacterCommandFile 0x46ba10.
const char* const kFlags1Bits[7] = {
	"Usable standing", "Usable in the air", "Usable crouching",
	"Cannot be cancelled into (top priority; \"always\" routes still work)",
	"Resets the command buffer", "Follow-up (rekka) input only", "Guard cancel",
};
const char* const kFlags2Bits[8] = {
	"Fires on button release", "Strict input window", "Usable before the round starts",
	"Can cancel into itself", "Guard cancel only (needs Flagset 1 guard cancel)",
	"Whiff cancel only (engine also sets \"cannot be cancelled into\" and \"cancel into itself\")",
	"Can be kara-cancelled into", "Not read by MBAACC",
};

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool Matches(const std::string& haystack, const std::string& needleLower)
{
	return needleLower.empty() || Lower(haystack).find(needleLower) != std::string::npos;
}

std::string BaseName(const std::string& path)
{
	const auto pos = path.find_last_of("\\/");
	return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string TeamSoloText(const std::string& v)
{
	if (v == "0") return "both";
	if (v == "1") return "team only";
	if (v == "2") return "solo only";
	return v;
}

std::string ProjectileText(const std::string& v)
{
	const auto n = ParseInteger(v);
	if (!n) return v;
	if (*n == 0) return "-";
	return "var" + std::to_string(*n / 10) + "<" + std::to_string(*n % 10);
}

void HelpMarker(const char* text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
}

} // namespace

// ---------------------------------------------------------------------------------------

struct CommandFileEditor::View {
	int id = 0;
	Workspace ws;
	bool open = true;
	bool focus = true;
	std::uint64_t notifiedRevision = 0;

	// Selection (command-region line uids, so comments are selectable too).
	std::set<std::uint64_t> selected;
	std::uint64_t primary = 0;
	std::uint64_t anchor = 0;
	std::uint64_t selectedCheck = 0; // ExComRecord uid
	bool scrollToPrimary = false;
	bool scrollToCheck = false;
	int requestTab = -1;

	std::string search;
	bool linkedChecksOnly = false;

	// Inline editing
	std::uint64_t editUid = 0;
	int editField = -1; // command: 0..8, 20 comment, 21 note; check: 100+field, 120 comment, 121 note; 200 comment line
	std::string editBuffer;
	bool editFocus = false;

	std::string status;
	bool statusError = false;
	bool openSavePopup = false;
	bool openExternalPopup = false;
	bool openClosePopup = false;
	std::string lastBackup;

	struct Pending {
		std::function<bool(WorkspaceSnapshot&)> edit;
		std::function<void(View&)> after;
	};
	std::vector<Pending> pending;

	void queue(std::function<bool(WorkspaceSnapshot&)> edit, std::function<void(View&)> after = nullptr)
	{
		pending.push_back({ std::move(edit), std::move(after) });
	}

	void setStatus(const std::string& s, bool error) { status = s; statusError = error; }

	const Document& doc() const { return ws.state.document; }

	void selectOnly(std::uint64_t uid)
	{
		selected.clear();
		if (uid) selected.insert(uid);
		primary = anchor = uid;
	}

	std::vector<std::uint64_t> selectedCommandUids() const
	{
		std::vector<std::uint64_t> out;
		for (const auto& c : doc().commands) if (selected.count(c.uid)) out.push_back(c.uid);
		return out;
	}

	std::vector<std::uint64_t> selectedLinesInOrder() const
	{
		std::vector<std::uint64_t> out;
		for (const auto& l : doc().lines()) if (selected.count(l.uid)) out.push_back(l.uid);
		return out;
	}

	void drawWindow(const std::string& title, const DocumentChanged& onChanged);
	void drawToolbar(const std::vector<Diagnostic>& diagnostics);
	void drawCommandsTab(const std::vector<Diagnostic>& diagnostics);
	void drawCommandTable(const std::vector<Diagnostic>& diagnostics, float height);
	void drawCommandDetail();
	void drawChecksTab();
	void drawCheckDetail();
	void drawTagTab();
	void drawOtherTab();
	void drawProblemsTab(const std::vector<Diagnostic>& diagnostics);
	void drawPopups();
	void handleShortcuts();
	void applyPending();
	void doSave(bool overwrite);

	bool inlineText(std::uint64_t uid, int field, const std::string& shown, std::string& committed, float width = -FLT_MIN);
	void selectByUid(std::uint64_t uid, bool additive, bool range);
	void selectCheck(std::size_t row);

	// Actions
	void insertCommand(bool after);
	void duplicateSelected();
	void commentOutSelected();
	void deleteSelected();
	void moveSelected(int dir);
	void insertComment(bool after);
	void addCheck(bool forSelectedCommand);
	CommandFields defaultCommand() const;
	std::string freeCommandId() const;
};

// ---------------------------------------------------------------------------------------

CommandFileEditor::CommandFileEditor() = default;
CommandFileEditor::~CommandFileEditor() = default;

bool CommandFileEditor::open(const std::string& path, std::string* error)
{
	for (auto& v : m_views) {
		if (Lower(v->ws.path()) == Lower(path)) { v->focus = true; return true; }
	}
	auto view = std::make_unique<View>();
	if (!view->ws.open(path, error)) return false;
	view->id = m_nextId++;
	m_views.push_back(std::move(view));
	return true;
}

bool CommandFileEditor::hasUnsavedChanges() const
{
	return std::any_of(m_views.begin(), m_views.end(), [](const auto& v) { return v->ws.dirty(); });
}

bool CommandFileEditor::empty() const { return m_views.empty(); }

void CommandFileEditor::draw(const DocumentChanged& onChanged)
{
	for (auto& v : m_views) {
		const std::string title = "Command file: " + BaseName(v->ws.path()) + (v->ws.dirty() ? " *" : "") +
			"###cmdfile_" + std::to_string(v->id);
		v->drawWindow(title, onChanged);
	}
	m_views.erase(std::remove_if(m_views.begin(), m_views.end(), [](const auto& v) { return !v->open; }), m_views.end());
}

// ---------------------------------------------------------------------------------------

void CommandFileEditor::View::drawWindow(const std::string& title, const DocumentChanged& onChanged)
{
	ImGui::SetNextWindowSize(ImVec2(1100, 720), ImGuiCond_FirstUseEver);
	if (focus) { ImGui::SetNextWindowFocus(); focus = false; }
	bool keepOpen = true;
	const bool visible = ImGui::Begin(title.c_str(), &keepOpen, ImGuiWindowFlags_NoCollapse);
	if (!keepOpen) {
		if (ws.dirty()) openClosePopup = true;
		else open = false;
	}
	if (visible) {
		const auto diagnostics = Validate(doc(), GetExtensionProfile());
		handleShortcuts();
		drawToolbar(diagnostics);
		if (ImGui::BeginTabBar("##cmdfile_tabs")) {
			auto tab = [&](const char* label, int index) {
				ImGuiTabItemFlags flags = requestTab == index ? ImGuiTabItemFlags_SetSelected : 0;
				return ImGui::BeginTabItem(label, nullptr, flags);
			};
			if (tab("Commands", 0)) { drawCommandsTab(diagnostics); ImGui::EndTabItem(); }
			const std::string checksLabel = "ExComChecks (" + std::to_string(doc().checks.size()) + ")###checks";
			if (tab(checksLabel.c_str(), 1)) { drawChecksTab(); ImGui::EndTabItem(); }
			if (tab("Tag data", 2)) { drawTagTab(); ImGui::EndTabItem(); }
			if (tab("Other sections", 3)) { drawOtherTab(); ImGui::EndTabItem(); }
			const std::size_t errors = CountSeverity(diagnostics, Severity::Error), warnings = CountSeverity(diagnostics, Severity::Warning);
			const std::string problemsLabel = "Problems (" + std::to_string(errors) + "E/" + std::to_string(warnings) + "W)###problems";
			if (errors) ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
			const bool problemsOpen = tab(problemsLabel.c_str(), 4);
			if (errors) ImGui::PopStyleColor();
			if (problemsOpen) { drawProblemsTab(diagnostics); ImGui::EndTabItem(); }
			requestTab = -1;
			ImGui::EndTabBar();
		}
	}
	drawPopups();
	ImGui::End();
	applyPending();
	if (onChanged && ws.revision() != notifiedRevision) {
		notifiedRevision = ws.revision();
		onChanged(ws.path(), doc());
	}
}

void CommandFileEditor::View::drawToolbar(const std::vector<Diagnostic>& diagnostics)
{
	const bool errors = HasErrors(diagnostics);
	ImGui::BeginDisabled(!ws.dirty());
	if (ImGui::Button("Save...")) openSavePopup = true;
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(errors ? "Fix the errors in the Problems tab first (Ctrl+S)" : "Review and save (Ctrl+S)");
	ImGui::SameLine();
	ImGui::BeginDisabled(!ws.dirty());
	if (ImGui::Button("Revert")) { ws.revert(); selectOnly(0); setStatus("Reverted to the saved file (Undo brings the edits back).", false); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!ws.canUndo());
	if (ImGui::Button("Undo")) ws.undo();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!ws.canRedo());
	if (ImGui::Button("Redo")) ws.redo();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::TextDisabled("%s file, profile: %s", doc().dialect == Dialect::MBAC ? "MBAC" : "MBAACC", ExtensionProfileName(GetExtensionProfile()));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Change the profile under Preferences > Extension profile.\nVanilla accepts every shipped file; Extended adds the BOF-only types and rules.");
	ImGui::SameLine();
	if (ws.dirty()) ImGui::TextColored(kWarnColor, "%zu line(s) changed", ws.changedLineCount());
	else ImGui::TextColored(kOkColor, "saved");
	ImGui::TextDisabled("%s", ws.path().c_str());
	if (ws.notesReadOnly()) {
		ImGui::TextColored(kErrorColor, "Notes file problem: %s", ws.notesError().c_str());
	}
	if (!status.empty()) ImGui::TextColored(statusError ? kErrorColor : kOkColor, "%s", status.c_str());
}

// ---------------------------------------------------------------------------------------
// Commands tab

CommandFields CommandFileEditor::View::defaultCommand() const
{
	CommandFields f;
	f.fields = { freeCommandId(), "5A", "00000101", "00000000", "0", "0", "0", "0", "0" };
	return f;
}

std::string CommandFileEditor::View::freeCommandId() const
{
	std::set<long long> used;
	for (const auto& c : doc().commands) if (auto v = ParseInteger(c.fields[CF_Id])) used.insert(*v);
	long long id = 0;
	while (used.count(id)) ++id;
	return std::to_string(id);
}

void CommandFileEditor::View::selectByUid(std::uint64_t uid, bool additive, bool range)
{
	if (range && anchor) {
		const auto& lines = doc().lines();
		const auto a = doc().lineIndexOf(anchor), b = doc().lineIndexOf(uid);
		if (a && b) {
			if (!additive) selected.clear();
			for (std::size_t i = std::min(*a, *b); i <= std::max(*a, *b); ++i) {
				const auto k = doc().kind(i);
				if (k != LineKind::Blank && k != LineKind::End) selected.insert(lines[i].uid);
			}
			primary = uid;
			return;
		}
	}
	if (additive) {
		if (selected.count(uid)) selected.erase(uid); else selected.insert(uid);
		primary = anchor = uid;
		return;
	}
	selectOnly(uid);
	// Linked selection: pick the first ExComCheck of this command.
	const int row = doc().findCommandRow(uid);
	if (row >= 0) {
		const auto linked = doc().checksForCommand(doc().commands[row].fields[CF_Id]);
		if (!linked.empty()) selectedCheck = doc().checks[linked.front()].uid;
	}
}

void CommandFileEditor::View::selectCheck(std::size_t row)
{
	selectedCheck = doc().checks[row].uid;
	if (const auto& id = doc().checks[row].values[XF_CheckNum]) {
		const auto cmds = doc().commandsWithId(*id);
		if (!cmds.empty()) { selectOnly(doc().commands[cmds.front()].uid); scrollToPrimary = true; }
	}
}

bool CommandFileEditor::View::inlineText(std::uint64_t uid, int field, const std::string& shown, std::string& committed, float width)
{
	ImGui::PushID(field);
	bool done = false;
	if (editUid == uid && editField == field) {
		ImGui::SetNextItemWidth(width);
		if (editFocus) { ImGui::SetKeyboardFocusHere(); editFocus = false; }
		const bool enter = ImGui::InputText("##edit", &editBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
			committed = editBuffer;
			done = true;
			editUid = 0; editField = -1;
		} else if (ImGui::IsItemDeactivated()) {
			editUid = 0; editField = -1;
		}
	} else {
		if (shown.empty()) ImGui::TextDisabled("..");
		else ImGui::TextUnformatted(shown.c_str());
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			editUid = uid; editField = field; editFocus = true; editBuffer = shown == "-" ? std::string() : shown;
		}
	}
	ImGui::PopID();
	return done;
}

void CommandFileEditor::View::drawCommandsTab(const std::vector<Diagnostic>& diagnostics)
{
	const bool hasSel = !selected.empty();
	const bool hasCmdSel = !selectedCommandUids().empty();
	if (ImGui::Button("+ Command")) insertCommand(true);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Insert below the selection (or before END) with the lowest free ID");
	ImGui::SameLine();
	if (ImGui::Button("+ Comment")) insertComment(true);
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasCmdSel);
	if (ImGui::Button("Duplicate")) duplicateSelected();
	ImGui::SameLine();
	if (ImGui::Button("Comment out")) commentOutSelected();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSel);
	if (ImGui::Button("Delete")) deleteSelected();
	ImGui::SameLine();
	if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveSelected(-1);
	ImGui::SameLine();
	if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveSelected(1);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(260);
	ImGui::InputTextWithHint("##search", "Search ID, input, pattern, comment, note", &search);
	if (!search.empty()) { ImGui::SameLine(); if (ImGui::SmallButton("x")) search.clear(); }
	ImGui::SameLine();
	ImGui::TextDisabled("%zu commands", doc().commands.size());
	HelpMarker("Double-click a cell to edit it. Drag rows to reorder (drop on the lower half to place below).\n"
		"Ctrl/Shift+click selects several rows. Right-click a row for more actions.\n"
		"Keys: Ctrl+Z / Ctrl+Y undo/redo, Ctrl+S save, Delete removes, Alt+Up/Down moves.");

	const float detailHeight = 250.0f;
	drawCommandTable(diagnostics, std::max(120.0f, ImGui::GetContentRegionAvail().y - detailHeight));
	drawCommandDetail();
}

void CommandFileEditor::View::drawCommandTable(const std::vector<Diagnostic>& diagnostics, float height)
{
	const Document& d = doc();
	const std::string needle = Lower(search);
	std::set<std::uint64_t> problemLines;
	for (const auto& diag : diagnostics) if (diag.uid && diag.severity != Severity::Info) problemLines.insert(diag.uid);
	std::map<long long, int> idCount;
	for (const auto& c : d.commands) if (auto v = ParseInteger(c.fields[CF_Id])) ++idCount[*v];
	std::set<std::uint64_t> linkedLines; // commands linked to the selected check
	if (selectedCheck) {
		const int row = d.findCheckRow(selectedCheck);
		if (row >= 0 && d.checks[row].values[XF_CheckNum])
			for (auto r : d.commandsWithId(*d.checks[row].values[XF_CheckNum])) linkedLines.insert(d.commands[r].uid);
	}

	const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("##commands", 13, flags, ImVec2(0, height))) return;
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 40);
	ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 44);
	ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthFixed, 80);
	ImGui::TableSetupColumn("Flagset 1", ImGuiTableColumnFlags_WidthFixed, 76);
	ImGui::TableSetupColumn("Flagset 2", ImGuiTableColumnFlags_WidthFixed, 76);
	ImGui::TableSetupColumn("Pattern", ImGuiTableColumnFlags_WidthFixed, 52);
	ImGui::TableSetupColumn("Meter", ImGuiTableColumnFlags_WidthFixed, 52);
	ImGui::TableSetupColumn("Team/solo", ImGuiTableColumnFlags_WidthFixed, 66);
	ImGui::TableSetupColumn("Proj. limit", ImGuiTableColumnFlags_WidthFixed, 66);
	ImGui::TableSetupColumn("Dash", ImGuiTableColumnFlags_WidthFixed, 40);
	ImGui::TableSetupColumn("ExCC", ImGuiTableColumnFlags_WidthFixed, 38);
	ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthFixed, 140);
	ImGui::TableHeadersRow();

	const std::size_t regionEnd = d.endLine ? *d.endLine : d.lines().size();
	for (std::size_t i = 0; i < regionEnd; ++i) {
		const LineKind kind = d.kind(i);
		if (kind == LineKind::Blank) continue;
		const Line& line = d.lines()[i];
		const int cmdRow = kind == LineKind::Command ? d.findCommandRow(line.uid) : -1;
		const CommandRecord* cmd = cmdRow >= 0 ? &d.commands[cmdRow] : nullptr;
		const std::string commentUtf8 = cmd ? Cp932ToUtf8(cmd->comment) : Cp932ToUtf8(d.commandRegionText(i));
		const auto noteIt = ws.state.notes.byUid.find(line.uid);
		const std::string note = noteIt != ws.state.notes.byUid.end() ? noteIt->second : std::string();
		if (!needle.empty()) {
			bool hit = Matches(commentUtf8, needle) || Matches(note, needle);
			if (cmd) hit = hit || Matches(cmd->fields[CF_Id], needle) || Matches(cmd->fields[CF_Input], needle) || Matches(cmd->fields[CF_Pattern], needle);
			if (!hit) continue;
		}
		ImGui::PushID(static_cast<int>(line.uid));
		ImGui::TableNextRow();
		const bool isSelected = selected.count(line.uid) != 0;
		if (linkedLines.count(line.uid) && !isSelected)
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(0.15f, 0.35f, 0.55f, 0.45f)));

		// Column 0: line number + selectable row
		ImGui::TableSetColumnIndex(0);
		char label[32];
		snprintf(label, sizeof(label), "%zu", i + 1);
		if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
			const auto& io = ImGui::GetIO();
			selectByUid(line.uid, io.KeyCtrl, io.KeyShift);
		}
		if (scrollToPrimary && line.uid == primary) { ImGui::SetScrollHereY(0.4f); scrollToPrimary = false; }
		if (ImGui::BeginDragDropSource()) {
			const std::uint64_t payload = line.uid;
			ImGui::SetDragDropPayload("HANTEI_CMDLINE", &payload, sizeof(payload));
			const std::size_t n = isSelected ? selected.size() : 1;
			ImGui::Text("Move %zu row(s)", n);
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
			const bool below = ImGui::GetMousePos().y > (min.y + max.y) * 0.5f;
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HANTEI_CMDLINE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect | ImGuiDragDropFlags_AcceptBeforeDelivery)) {
				const float y = below ? max.y : min.y;
				ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, y), ImVec2(min.x + ImGui::GetWindowWidth(), y), IM_COL32(74, 184, 255, 255), 2.0f);
				if (p->IsDelivery()) {
					const std::uint64_t src = *static_cast<const std::uint64_t*>(p->Data);
					std::vector<std::uint64_t> moving = selected.count(src) ? selectedLinesInOrder() : std::vector<std::uint64_t>{ src };
					const std::uint64_t target = line.uid;
					queue([moving, target, below](WorkspaceSnapshot& s) { return s.document.moveLines(moving, target, below); });
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("##rowmenu")) {
			if (!isSelected) selectOnly(line.uid);
			if (ImGui::MenuItem("Insert command above")) insertCommand(false);
			if (ImGui::MenuItem("Insert command below")) insertCommand(true);
			if (ImGui::MenuItem("Insert comment above")) insertComment(false);
			if (ImGui::MenuItem("Insert comment below")) insertComment(true);
			ImGui::Separator();
			if (cmd && ImGui::MenuItem("Duplicate")) duplicateSelected();
			if (cmd && ImGui::MenuItem("Comment out")) commentOutSelected();
			if (kind == LineKind::CommentedCommand && ImGui::MenuItem("Restore as active command")) {
				const auto uid = line.uid;
				queue([uid](WorkspaceSnapshot& s) { return s.document.uncomment(uid); });
			}
			if (cmd && ImGui::MenuItem("Add ExComCheck for this command")) addCheck(true);
			if (ImGui::MenuItem("Edit note")) { editUid = line.uid; editField = 21; editFocus = true; editBuffer = note; }
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) deleteSelected();
			ImGui::EndPopup();
		}
		if (problemLines.count(line.uid)) { ImGui::SameLine(); ImGui::TextColored(kWarnColor, "!"); }

		if (cmd) {
			const std::uint64_t uid = line.uid;
			for (int f = 0; f < CF_Count; ++f) {
				ImGui::TableSetColumnIndex(f + 1);
				std::string shown = cmd->fields[f];
				const bool dupId = f == CF_Id && ParseInteger(shown) && idCount[*ParseInteger(shown)] > 1;
				if (dupId) ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
				std::string value;
				if (inlineText(uid, f, shown, value)) {
					queue([uid, f, value](WorkspaceSnapshot& s) {
						const int row = s.document.findCommandRow(uid);
						return row >= 0 && s.document.setCommandField(row, f, value);
					});
				}
				if (dupId) ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) {
					if (f == CF_TeamSolo) ImGui::SetTooltip("Team/solo: %s\n0 = both, 1 = team only, 2 = solo only\n(read by Command_CheckCmdVars, record byte +36)", TeamSoloText(shown).c_str());
					else if (f == CF_Projectile) ImGui::SetTooltip("Projectile limit (\"tobi\"): %s\nTens digit = projectile variable, ones digit = limit.\nThe move is usable while that variable is below the limit.", ProjectileText(shown).c_str());
					else if (f == CF_AirDash) ImGui::SetTooltip("Air-dash limit: usable while the dash variable is below this value (0 = no limit).");
					else if (dupId) ImGui::SetTooltip("Duplicate ID: MBAACC uses the first definition only.");
				}
			}
			ImGui::TableSetColumnIndex(10);
			const auto linked = d.checksForCommand(cmd->fields[CF_Id]);
			if (!linked.empty()) {
				ImGui::TextColored(kLinkColor, "%zu", linked.size());
				if (ImGui::IsItemClicked()) { selectCheck(linked.front()); requestTab = 1; scrollToCheck = true; }
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%zu ExComCheck row(s), all must pass. Click to open.", linked.size());
			}
			ImGui::TableSetColumnIndex(11);
			std::string newComment;
			if (inlineText(uid, 20, commentUtf8, newComment)) {
				const auto encoded = Utf8ToCp932(newComment);
				if (!encoded) setStatus("That comment has characters CP932 cannot store.", true);
				else queue([uid, c = *encoded](WorkspaceSnapshot& s) {
					const int row = s.document.findCommandRow(uid);
					return row >= 0 && s.document.setCommandComment(row, c);
				});
			}
		} else {
			// Comment, commented-out command or malformed row: show the text across the columns.
			ImGui::TableSetColumnIndex(1);
			if (kind == LineKind::MalformedCommand) {
				ImGui::TextColored(kErrorColor, "%s", Cp932ToUtf8(line.text).c_str());
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("MBAACC reads this line as a command (missing columns = 0).\nComment it out or delete it.");
			} else if (kind == LineKind::CommentedCommand) {
				const auto fields = d.commentedCommand(i);
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				for (int f = 0; f < CF_Count; ++f) {
					ImGui::TableSetColumnIndex(f + 1);
					ImGui::Text(f == 0 ? "// %s" : "%s", fields->fields[f].c_str());
				}
				ImGui::TableSetColumnIndex(11);
				ImGui::TextUnformatted(Cp932ToUtf8(fields->comment).c_str());
				ImGui::PopStyleColor();
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, kCommentColor);
				std::string edited;
				const std::uint64_t uid = line.uid;
				if (inlineText(uid, 200, "// " + commentUtf8, edited, ImGui::GetContentRegionAvail().x + 600)) {
					if (edited.rfind("//", 0) == 0) edited = edited.substr(edited.size() > 2 && edited[2] == ' ' ? 3 : 2);
					const auto encoded = Utf8ToCp932(edited);
					if (!encoded) setStatus("That comment has characters CP932 cannot store.", true);
					else queue([uid, c = *encoded](WorkspaceSnapshot& s) { return s.document.replaceCommentLine(uid, c); });
				}
				ImGui::PopStyleColor();
			}
		}
		ImGui::TableSetColumnIndex(12);
		std::string newNote;
		if (inlineText(line.uid, 21, note.empty() && !(editUid == line.uid && editField == 21) ? std::string() : note, newNote)) {
			if (ws.notesReadOnly()) setStatus("Notes are read-only until the notes file problem is fixed.", true);
			else {
				const auto uid = line.uid;
				queue([uid, newNote](WorkspaceSnapshot& s) {
					if (newNote.empty()) s.notes.byUid.erase(uid); else s.notes.byUid[uid] = newNote;
					return true;
				});
			}
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void CommandFileEditor::View::drawCommandDetail()
{
	ImGui::Separator();
	const Document& d = doc();
	const int row = d.findCommandRow(primary);
	if (row < 0) {
		ImGui::TextDisabled("Select a command to edit its flags and linked ExComChecks.");
		return;
	}
	const CommandRecord& c = d.commands[row];
	const std::uint64_t uid = c.uid;
	ImGui::BeginChild("##detail", ImVec2(0, 0), ImGuiChildFlags_None);
	ImGui::Text("Command %s  %s  -> pattern %s", c.fields[CF_Id].c_str(), c.fields[CF_Input].c_str(), c.fields[CF_Pattern].c_str());
	if (!c.comment.empty()) { ImGui::SameLine(); ImGui::TextColored(kCommentColor, "// %s", Cp932ToUtf8(c.comment).c_str()); }

	auto setField = [&](int f, const std::string& value) {
		queue([uid, f, value](WorkspaceSnapshot& s) {
			const int r = s.document.findCommandRow(uid);
			return r >= 0 && s.document.setCommandField(r, f, value);
		});
	};

	// Flagsets as bit toggles.
	auto flagEditor = [&](int field, const char* const* names, int bitCount, bool hasClass) {
		std::string bits = c.fields[field];
		if (bits.size() != 8) { ImGui::TextColored(kWarnColor, "%s: %s (not 8 digits; edit it in the table)", CommandFieldName(field), bits.c_str()); return; }
		ImGui::TextUnformatted(CommandFieldName(field));
		ImGui::SameLine(110);
		ImGui::PushID(field);
		if (hasClass) {
			static const char* classes[] = { "0 Normal", "1 Special", "2 EX" };
			int cls = bits[0] - '0';
			ImGui::SetNextItemWidth(100);
			if (ImGui::Combo("##class", &cls, classes, 3)) { bits[0] = static_cast<char>('0' + cls); setField(field, bits); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel class (first digit): normal / special / EX");
			ImGui::SameLine();
		}
		for (int bit = bitCount - 1; bit >= 0; --bit) {
			const int ch = 7 - bit;
			bool on = bits[ch] != '0';
			ImGui::PushID(bit);
			if (ImGui::Checkbox("##b", &on)) { bits[ch] = on ? '1' : '0'; setField(field, bits); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("bit %d: %s", bit, names[bit]);
			ImGui::PopID();
			ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::PopID();
	};
	flagEditor(CF_Flags1, kFlags1Bits, 7, true);
	flagEditor(CF_Flags2, kFlags2Bits, 8, false);

	ImGui::TextUnformatted("Team/solo");
	ImGui::SameLine(110);
	{
		static const char* opts[] = { "0: both", "1: team only", "2: solo only" };
		int v = static_cast<int>(ParseInteger(c.fields[CF_TeamSolo]).value_or(-1));
		ImGui::SetNextItemWidth(160);
		if (v >= 0 && v <= 2) {
			if (ImGui::Combo("##teamsolo", &v, opts, 3)) setField(CF_TeamSolo, std::to_string(v));
		} else ImGui::TextColored(kWarnColor, "%s (unknown)", c.fields[CF_TeamSolo].c_str());
		HelpMarker("First of the three trailing columns (the header comment calls it \"counter time\").\n"
			"MBAACC Command_CheckCmdVars: 1 = only with a partner (team), 2 = only without one (solo).\n"
			"In MBAC only Hisui and Kohaku use it.");
	}
	ImGui::SameLine();
	ImGui::Text("   Projectile limit: %s", ProjectileText(c.fields[CF_Projectile]).c_str());
	HelpMarker("\"tobi\" (飛び道具制限). Tens digit = projectile variable (EF6 100/101, EF1 p9),\nones digit = limit; usable while the variable is below the limit.");
	ImGui::SameLine();
	ImGui::Text("   Air-dash limit: %s", c.fields[CF_AirDash].c_str());

	// Linked ExComChecks
	const auto linked = d.checksForCommand(c.fields[CF_Id]);
	ImGui::Text("ExComChecks (all must pass): %zu", linked.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Add check")) addCheck(true);
	for (auto r : linked) {
		const auto& chk = d.checks[r];
		ImGui::PushID(static_cast<int>(chk.uid));
		char label[256];
		snprintf(label, sizeof(label), "%03d  Type %s  %s", chk.index, chk.values[XF_Type].value_or("?").c_str(), SummarizeCheck(chk).c_str());
		if (ImGui::Selectable(label, selectedCheck == chk.uid)) { selectedCheck = chk.uid; }
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { requestTab = 1; scrollToCheck = true; }
		ImGui::PopID();
	}
	ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------
// ExComChecks tab

void CommandFileEditor::View::drawChecksTab()
{
	const Document& d = doc();
	if (ImGui::Button("+ Check")) addCheck(false);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a row below the selected check (or at the end), linked to the selected command");
	ImGui::SameLine();
	const int selRow = d.findCheckRow(selectedCheck);
	ImGui::BeginDisabled(selRow < 0);
	if (ImGui::Button("Duplicate")) {
		ExComFields f;
		f.values = d.checks[selRow].values;
		f.comment = d.checks[selRow].comment;
		const int anchor = selRow;
		queue([anchor, f](WorkspaceSnapshot& s) { return s.document.insertCheck(anchor, true, f) != 0; },
			[anchor](View& v) { if (anchor + 1 < static_cast<int>(v.doc().checks.size())) v.selectedCheck = v.doc().checks[anchor + 1].uid; });
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete")) {
		const std::size_t r = static_cast<std::size_t>(selRow);
		queue([r](WorkspaceSnapshot& s) { return s.document.deleteChecks({ r }); }, [](View& v) { v.selectedCheck = 0; });
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("##cup", ImGuiDir_Up) && selRow > 0) {
		const std::size_t r = static_cast<std::size_t>(selRow);
		queue([r](WorkspaceSnapshot& s) { return s.document.moveCheck(r, r - 1, false); });
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("##cdown", ImGuiDir_Down) && selRow >= 0 && selRow + 1 < static_cast<int>(d.checks.size())) {
		const std::size_t r = static_cast<std::size_t>(selRow);
		queue([r](WorkspaceSnapshot& s) { return s.document.moveCheck(r, r + 1, true); });
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Checkbox("Only checks of the selected command", &linkedChecksOnly);
	HelpMarker("Rows are numbered 000..Num-1; reordering and deleting renumber only the [ExComCheck] section and update Num.\n"
		"Every row whose CheckNum equals a command ID must pass for that command (Command_CheckExComConditions 0x46d3d0).");

	std::string selectedCommandId;
	if (const int r = d.findCommandRow(primary); r >= 0) selectedCommandId = d.commands[r].fields[CF_Id];

	const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	const float height = std::max(120.0f, ImGui::GetContentRegionAvail().y - 230.0f);
	if (ImGui::BeginTable("##checks", 11, flags, ImVec2(0, height))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, 36);
		ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 70);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 40);
		for (int p = 0; p < 5; ++p) ImGui::TableSetupColumn(ExComFieldName(XF_P0 + p), ImGuiTableColumnFlags_WidthFixed, 52);
		ImGui::TableSetupColumn("Meaning", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed, 140);
		ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthFixed, 140);
		ImGui::TableHeadersRow();
		for (std::size_t r = 0; r < d.checks.size(); ++r) {
			const auto& chk = d.checks[r];
			const bool linked = !selectedCommandId.empty() && chk.values[XF_CheckNum] && ParseInteger(*chk.values[XF_CheckNum]) == ParseInteger(selectedCommandId);
			if (linkedChecksOnly && !linked) continue;
			ImGui::PushID(static_cast<int>(chk.uid));
			ImGui::TableNextRow();
			if (linked && selectedCheck != chk.uid)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(0.15f, 0.35f, 0.55f, 0.45f)));
			ImGui::TableSetColumnIndex(0);
			char label[16];
			snprintf(label, sizeof(label), "%03d", chk.index);
			if (ImGui::Selectable(label, selectedCheck == chk.uid, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
				selectCheck(r);
			if (scrollToCheck && selectedCheck == chk.uid) { ImGui::SetScrollHereY(0.4f); scrollToCheck = false; }
			if (ImGui::BeginDragDropSource()) {
				const std::uint64_t payload = chk.uid;
				ImGui::SetDragDropPayload("HANTEI_EXCOM", &payload, sizeof(payload));
				ImGui::Text("Move row %03d", chk.index);
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
				const bool below = ImGui::GetMousePos().y > (min.y + max.y) * 0.5f;
				if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HANTEI_EXCOM", ImGuiDragDropFlags_AcceptNoDrawDefaultRect | ImGuiDragDropFlags_AcceptBeforeDelivery)) {
					const float y = below ? max.y : min.y;
					ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, y), ImVec2(min.x + ImGui::GetWindowWidth(), y), IM_COL32(74, 184, 255, 255), 2.0f);
					if (p->IsDelivery()) {
						const std::uint64_t src = *static_cast<const std::uint64_t*>(p->Data);
						const std::uint64_t dst = chk.uid;
						queue([src, dst, below](WorkspaceSnapshot& s) {
							const int a = s.document.findCheckRow(src), b = s.document.findCheckRow(dst);
							return a >= 0 && b >= 0 && s.document.moveCheck(a, b, below);
						});
					}
				}
				ImGui::EndDragDropTarget();
			}
			const std::uint64_t uid = chk.uid;
			for (int f = XF_CheckNum; f <= XF_P4; ++f) {
				ImGui::TableSetColumnIndex(f + 1);
				std::string value;
				if (inlineText(uid, 100 + f, chk.values[f].value_or("-"), value)) {
					std::optional<std::string> v;
					if (!value.empty() && value != "-") v = value;
					queue([uid, f, v](WorkspaceSnapshot& s) {
						const int row = s.document.findCheckRow(uid);
						return row >= 0 && s.document.setCheckValue(row, f, v);
					});
				}
				if (f == XF_CheckNum && ImGui::IsItemHovered()) ImGui::SetTooltip("Command ID this row gates. Double-click to edit.");
			}
			ImGui::TableSetColumnIndex(8);
			ImGui::TextUnformatted(SummarizeCheck(chk).c_str());
			ImGui::TableSetColumnIndex(9);
			std::string comment;
			if (inlineText(uid, 120, Cp932ToUtf8(chk.comment), comment)) {
				const auto encoded = Utf8ToCp932(comment);
				if (!encoded) setStatus("That comment has characters CP932 cannot store.", true);
				else queue([uid, c = *encoded](WorkspaceSnapshot& s) {
					const int row = s.document.findCheckRow(uid);
					return row >= 0 && s.document.setCheckComment(row, c);
				});
			}
			ImGui::TableSetColumnIndex(10);
			const auto noteIt = ws.state.notes.byUid.find(uid);
			std::string note;
			if (inlineText(uid, 121, noteIt != ws.state.notes.byUid.end() ? noteIt->second : std::string(), note)) {
				if (ws.notesReadOnly()) setStatus("Notes are read-only until the notes file problem is fixed.", true);
				else queue([uid, note](WorkspaceSnapshot& s) { if (note.empty()) s.notes.byUid.erase(uid); else s.notes.byUid[uid] = note; return true; });
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	drawCheckDetail();
}

void CommandFileEditor::View::drawCheckDetail()
{
	ImGui::Separator();
	const Document& d = doc();
	const int row = d.findCheckRow(selectedCheck);
	if (row < 0) { ImGui::TextDisabled("Select a row to edit it with named parameters."); return; }
	const auto& chk = d.checks[row];
	const std::uint64_t uid = chk.uid;
	ImGui::BeginChild("##checkdetail");
	const auto type = chk.values[XF_Type] ? ParseInteger(*chk.values[XF_Type]) : std::nullopt;
	const ExComTypeInfo* info = type ? FindExComType(static_cast<int>(*type)) : nullptr;
	ImGui::Text("Row %03d for command %s", chk.index, chk.values[XF_CheckNum].value_or("?").c_str());
	ImGui::SameLine();
	ImGui::SetNextItemWidth(320);
	const std::string preview = info ? std::to_string(info->type) + ": " + info->name : "Type " + chk.values[XF_Type].value_or("?");
	if (ImGui::BeginCombo("Type", preview.c_str())) {
		for (const auto* t : ExComTypes(GetExtensionProfile())) {
			const std::string name = std::to_string(t->type) + ": " + t->name;
			if (ImGui::Selectable(name.c_str(), info == t)) {
				const std::string v = std::to_string(t->type);
				queue([uid, v](WorkspaceSnapshot& s) { const int r = s.document.findCheckRow(uid); return r >= 0 && s.document.setCheckValue(r, XF_Type, v); });
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t->summary);
		}
		ImGui::EndCombo();
	}
	if (info && info->extendedOnly && !ExtendedProfileEnabled())
		ImGui::TextColored(kErrorColor, "Type %d needs a BOF executable: vanilla MBAACC fails it. Enable the Extended profile or change the type.", info->type);
	if (info) ImGui::TextDisabled("%s", info->summary);
	for (int p = 0; p < 5; ++p) {
		const int f = XF_P0 + p;
		ImGui::PushID(f);
		const bool present = chk.values[f].has_value();
		ImGui::Text("P%d %s", p, info ? info->paramNames[p] : "");
		if (info && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info->paramHelp[p]);
		ImGui::SameLine(220);
		if (present) {
			std::string text = *chk.values[f];
			ImGui::SetNextItemWidth(120);
			if (ImGui::InputText("##v", &text, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
				queue([uid, f, text](WorkspaceSnapshot& s) { const int r = s.document.findCheckRow(uid); return r >= 0 && s.document.setCheckValue(r, f, text); });
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("remove")) queue([uid, f](WorkspaceSnapshot& s) { const int r = s.document.findCheckRow(uid); return r >= 0 && s.document.setCheckValue(r, f, std::nullopt); });
		} else {
			ImGui::TextDisabled("not written");
			ImGui::SameLine();
			if (ImGui::SmallButton("add")) queue([uid, f](WorkspaceSnapshot& s) { const int r = s.document.findCheckRow(uid); return r >= 0 && s.document.setCheckValue(r, f, std::string("0")); });
		}
		ImGui::PopID();
	}
	ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------

void CommandFileEditor::View::drawTagTab()
{
	const Document& d = doc();
	ImGui::SeparatorText("Team/solo column");
	ImGui::TextWrapped("The first of the three trailing command columns. MBAACC reads it in Command_CheckCmdVars: 1 = only usable with a partner (team), 2 = only usable alone (solo). Tag/team modes therefore disable team-only moves for solo play and vice versa.");
	int teamOnly = 0, soloOnly = 0;
	for (const auto& c : d.commands) {
		if (c.fields[CF_TeamSolo] == "1") ++teamOnly;
		else if (c.fields[CF_TeamSolo] == "2") ++soloOnly;
	}
	ImGui::Text("%d team-only, %d solo-only command(s)", teamOnly, soloOnly);
	if (teamOnly + soloOnly > 0 && ImGui::BeginTable("##teamsolo", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("ID"); ImGui::TableSetupColumn("Input"); ImGui::TableSetupColumn("Pattern");
		ImGui::TableSetupColumn("Team/solo"); ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		for (const auto& c : d.commands) {
			if (c.fields[CF_TeamSolo] != "1" && c.fields[CF_TeamSolo] != "2") continue;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(static_cast<int>(c.uid));
			if (ImGui::Selectable(c.fields[CF_Id].c_str(), primary == c.uid, ImGuiSelectableFlags_SpanAllColumns)) { selectOnly(c.uid); scrollToPrimary = true; requestTab = 0; }
			ImGui::PopID();
			ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(c.fields[CF_Input].c_str());
			ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(c.fields[CF_Pattern].c_str());
			ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(TeamSoloText(c.fields[CF_TeamSolo]).c_str());
			ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(Cp932ToUtf8(c.comment).c_str());
		}
		ImGui::EndTable();
	}

	ImGui::SeparatorText("[TeamChangeData] (tag-in / tag-out patterns)");
	if (d.dialect == Dialect::MBAC)
		ImGui::TextWrapped("MBAC: the game reads these two pattern numbers from the compiled <CHR>_C.CT (file offset 0x2280/0x2284); this text file is its source. They are the patterns played when the character tags in and out.");
	else
		ImGui::TextColored(kWarnColor, "MBAACC has no [TeamChangeData] reader: these values are left over from MBAC and are ignored by the game.\nSee docs/tag_research/STATE_COMPARISON_MBAC_vs_MBAACC.md section 5 for the correct pattern numbers.");
	if (!d.teamChange.present) {
		ImGui::TextDisabled("This file has no [TeamChangeData] section.");
		return;
	}
	int in = d.teamChange.tagIn.value_or(0), out = d.teamChange.tagOut.value_or(0);
	ImGui::SetNextItemWidth(120);
	const bool a = ImGui::InputInt("Tag-in pattern", &in, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	const bool b = ImGui::InputInt("Tag-out pattern", &out, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
	if (a || b) queue([in, out](WorkspaceSnapshot& s) { return s.document.setTeamChange(in, out); });
	ImGui::TextDisabled("Line %zu%s", d.teamChange.headerLine + 1, d.teamChange.assignmentForm ? " (key = in, out form)" : " (bare \"in out\" form)");
}

void CommandFileEditor::View::drawOtherTab()
{
	const Document& d = doc();
	if (!d.endLine) { ImGui::TextDisabled("No END line: everything is read as commands."); return; }
	ImGui::TextDisabled("Everything after END, shown as stored. The engine looks these keys up by name (AirJumpNum, Guard, Flags, ...). Read-only here; ExComCheck and TeamChangeData have their own tabs.");
	ImGui::BeginChild("##tail", ImVec2(0, 0), ImGuiChildFlags_Borders);
	for (std::size_t i = *d.endLine; i < d.lines().size(); ++i) {
		const auto k = d.kind(i);
		const std::string text = Cp932ToUtf8(d.lines()[i].text);
		ImGui::TextDisabled("%4zu", i + 1);
		ImGui::SameLine();
		if (k == LineKind::SectionHeader) ImGui::TextColored(kLinkColor, "%s", text.c_str());
		else if (k == LineKind::Comment) ImGui::TextColored(kCommentColor, "%s", text.c_str());
		else ImGui::TextUnformatted(text.c_str());
	}
	ImGui::EndChild();
}

void CommandFileEditor::View::drawProblemsTab(const std::vector<Diagnostic>& diagnostics)
{
	if (diagnostics.empty()) { ImGui::TextColored(kOkColor, "No problems for the %s profile.", ExtensionProfileName(GetExtensionProfile())); return; }
	if (!ImGui::BeginTable("##problems", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) return;
	ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 70);
	ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 50);
	ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();
	int n = 0;
	for (const auto& diag : diagnostics) {
		ImGui::PushID(n++);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		const ImVec4 color = diag.severity == Severity::Error ? kErrorColor : diag.severity == Severity::Warning ? kWarnColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		const char* sev = diag.severity == Severity::Error ? "error" : diag.severity == Severity::Warning ? "warning" : "info";
		if (ImGui::Selectable(sev, false, ImGuiSelectableFlags_SpanAllColumns) && diag.uid) {
			const int crow = doc().findCommandRow(diag.uid);
			const int xrow = doc().findCheckRow(diag.uid);
			if (xrow >= 0) { selectCheck(xrow); requestTab = 1; scrollToCheck = true; }
			else { selectOnly(diag.uid); scrollToPrimary = true; requestTab = 0; }
			(void)crow;
		}
		ImGui::PopStyleColor();
		ImGui::TableSetColumnIndex(1);
		if (diag.line) ImGui::Text("%d", diag.line);
		ImGui::TableSetColumnIndex(2);
		ImGui::TextWrapped("%s", diag.message.c_str());
		ImGui::PopID();
	}
	ImGui::EndTable();
}

// ---------------------------------------------------------------------------------------

void CommandFileEditor::View::doSave(bool overwrite)
{
	const auto result = ws.save(GetExtensionProfile(), overwrite);
	switch (result.status) {
	case SaveStatus::Saved:
		lastBackup = result.backupPath;
		setStatus(result.backupPath.empty() ? "Saved (no changes to the command file)." : "Saved. Previous version backed up to " + result.backupPath, false);
		break;
	case SaveStatus::ExternalChange:
		openExternalPopup = true;
		break;
	default:
		setStatus(result.message, true);
		break;
	}
}

void CommandFileEditor::View::drawPopups()
{
	const std::string saveId = "Save command file?##" + std::to_string(id);
	const std::string extId = "File changed on disk##" + std::to_string(id);
	const std::string closeId = "Discard command file changes?##" + std::to_string(id);
	if (openSavePopup) { ImGui::OpenPopup(saveId.c_str()); openSavePopup = false; }
	if (openExternalPopup) { ImGui::OpenPopup(extId.c_str()); openExternalPopup = false; }
	if (openClosePopup) { ImGui::OpenPopup(closeId.c_str()); openClosePopup = false; }

	if (ImGui::BeginPopupModal(saveId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		const auto diagnostics = Validate(doc(), GetExtensionProfile());
		ImGui::Text("Replace %s", ws.path().c_str());
		ImGui::Text("%zu line(s) changed%s.", ws.changedLineCount(), ws.notesDirty() ? ", notes changed" : "");
		ImGui::Text("Profile: %s", ExtensionProfileName(GetExtensionProfile()));
		const auto warnings = CountSeverity(diagnostics, Severity::Warning);
		if (HasErrors(diagnostics)) ImGui::TextColored(kErrorColor, "%zu error(s) block the save. See the Problems tab.", CountSeverity(diagnostics, Severity::Error));
		else if (warnings) ImGui::TextColored(kWarnColor, "%zu warning(s); saving is allowed.", warnings);
		ImGui::TextWrapped("The current file is copied to .hantei-backups first, then replaced atomically. Only edited lines change; every other byte (CP932 comments, spacing, line endings) is kept.");
		ImGui::BeginDisabled(HasErrors(diagnostics));
		if (ImGui::Button("Back up and save")) { doSave(false); ImGui::CloseCurrentPopup(); }
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal(extId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("%s changed on disk after it was opened here.", ws.path().c_str());
		if (ImGui::Button("Overwrite (the disk version is backed up)")) { doSave(true); ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal(closeId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s has unsaved changes.", BaseName(ws.path()).c_str());
		if (ImGui::Button("Discard and close")) { open = false; ImGui::CloseCurrentPopup(); }
		ImGui::SameLine();
		if (ImGui::Button("Keep editing")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void CommandFileEditor::View::handleShortcuts()
{
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
	const auto& io = ImGui::GetIO();
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { if (ws.dirty()) openSavePopup = true; return; }
	if (io.WantTextInput) return; // text fields keep their own undo
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) ws.redo(); else ws.undo(); }
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ws.redo();
	else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !selected.empty()) deleteSelected();
	else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) moveSelected(-1);
	else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) moveSelected(1);
}

void CommandFileEditor::View::applyPending()
{
	auto items = std::move(pending);
	pending.clear();
	for (auto& p : items) {
		std::string error;
		const bool ok = ws.apply([&](WorkspaceSnapshot& s) {
			const bool r = p.edit(s);
			if (r) PruneNotes(s.document, s.notes);
			return r;
		});
		if (!ok) { setStatus("Edit rejected: the value is not valid there.", true); continue; }
		if (statusError) status.clear();
		if (p.after) p.after(*this);
	}
	// Drop selections that no longer exist.
	for (auto it = selected.begin(); it != selected.end();) {
		if (!doc().lineIndexOf(*it)) it = selected.erase(it); else ++it;
	}
	if (primary && !doc().lineIndexOf(primary)) primary = 0;
	if (selectedCheck && doc().findCheckRow(selectedCheck) < 0) selectedCheck = 0;
}

// ---------------------------------------------------------------------------------------
// Actions

void CommandFileEditor::View::insertCommand(bool after)
{
	const auto lines = selectedLinesInOrder();
	const std::uint64_t anchor = lines.empty() ? 0 : (after ? lines.back() : lines.front());
	const CommandFields f = defaultCommand();
	auto newUid = std::make_shared<std::uint64_t>(0);
	queue([anchor, after, f, newUid](WorkspaceSnapshot& s) { *newUid = s.document.insertCommand(anchor, after, f); return *newUid != 0; },
		[newUid](View& v) { v.selectOnly(*newUid); v.scrollToPrimary = true; v.editUid = *newUid; v.editField = CF_Input; v.editFocus = true; v.editBuffer = "5A"; });
}

void CommandFileEditor::View::duplicateSelected()
{
	const auto uids = selectedCommandUids();
	if (uids.empty()) return;
	auto lastUid = std::make_shared<std::uint64_t>(0);
	queue([uids, lastUid](WorkspaceSnapshot& s) {
		for (auto uid : uids) {
			const int row = s.document.findCommandRow(uid);
			if (row < 0) return false;
			CommandFields f;
			f.fields = s.document.commands[row].fields;
			f.comment = s.document.commands[row].comment;
			std::set<long long> used;
			for (const auto& c : s.document.commands) if (auto v = ParseInteger(c.fields[CF_Id])) used.insert(*v);
			long long id = 0;
			while (used.count(id)) ++id;
			f.fields[CF_Id] = std::to_string(id);
			*lastUid = s.document.insertCommand(uid, true, f);
			if (!*lastUid) return false;
		}
		return true;
	}, [lastUid](View& v) { v.selectOnly(*lastUid); v.scrollToPrimary = true; v.setStatus("Duplicated with the lowest free command ID.", false); });
}

void CommandFileEditor::View::commentOutSelected()
{
	const auto uids = selectedCommandUids();
	if (uids.empty()) return;
	queue([uids](WorkspaceSnapshot& s) { return s.document.commentOut(uids); });
}

void CommandFileEditor::View::deleteSelected()
{
	const auto uids = selectedLinesInOrder();
	if (uids.empty()) return;
	queue([uids](WorkspaceSnapshot& s) { return s.document.deleteLines(uids); }, [](View& v) { v.selectOnly(0); });
}

void CommandFileEditor::View::moveSelected(int dir)
{
	const auto uids = selectedLinesInOrder();
	if (uids.empty()) return;
	const Document& d = doc();
	const std::size_t regionEnd = d.endLine ? *d.endLine : d.lines().size();
	auto first = d.lineIndexOf(uids.front()), last = d.lineIndexOf(uids.back());
	if (!first || !last) return;
	// Neighbouring non-blank line outside the selection.
	std::uint64_t target = 0;
	if (dir < 0) {
		for (std::size_t i = *first; i-- > 0;) if (d.kind(i) != LineKind::Blank) { target = d.lines()[i].uid; break; }
	} else {
		for (std::size_t i = *last + 1; i < regionEnd; ++i) if (d.kind(i) != LineKind::Blank) { target = d.lines()[i].uid; break; }
	}
	if (!target) return;
	queue([uids, target, dir](WorkspaceSnapshot& s) { return s.document.moveLines(uids, target, dir > 0); }, [](View& v) { v.scrollToPrimary = true; });
}

void CommandFileEditor::View::insertComment(bool after)
{
	const auto lines = selectedLinesInOrder();
	const std::uint64_t anchor = lines.empty() ? 0 : (after ? lines.back() : lines.front());
	auto newUid = std::make_shared<std::uint64_t>(0);
	queue([anchor, after, newUid](WorkspaceSnapshot& s) { *newUid = s.document.insertCommentLines(anchor, after, { "" }); return *newUid != 0; },
		[newUid](View& v) { v.selectOnly(*newUid); v.scrollToPrimary = true; v.editUid = *newUid; v.editField = 200; v.editFocus = true; v.editBuffer.clear(); });
}

void CommandFileEditor::View::addCheck(bool forSelectedCommand)
{
	const Document& d = doc();
	std::string commandId;
	if (const int r = d.findCommandRow(primary); r >= 0) commandId = d.commands[r].fields[CF_Id];
	if (commandId.empty() && forSelectedCommand) return;
	if (commandId.empty()) commandId = d.commands.empty() ? "0" : d.commands.front().fields[CF_Id];
	ExComFields f;
	f.values = { commandId, std::string("0"), std::string("0"), std::string("1"), std::nullopt, std::nullopt, std::nullopt };
	int anchor = d.findCheckRow(selectedCheck);
	if (forSelectedCommand) {
		const auto linked = d.checksForCommand(commandId);
		anchor = linked.empty() ? -1 : static_cast<int>(linked.back());
	}
	auto newUid = std::make_shared<std::uint64_t>(0);
	queue([anchor, f, newUid](WorkspaceSnapshot& s) { *newUid = s.document.insertCheck(anchor, true, f); return *newUid != 0; },
		[newUid](View& v) { v.selectedCheck = *newUid; v.requestTab = 1; v.scrollToCheck = true; });
}

} // namespace cmdfile
