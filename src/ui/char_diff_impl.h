#ifndef UI_CHAR_DIFF_IMPL_H_GUARD
#define UI_CHAR_DIFF_IMPL_H_GUARD

// ============================================================================
// Character diff (issue #131): Tools > Character diff (compare with...)
// ============================================================================
// The tool character is compared with another file of the same character: the vanilla one in a game archive (.p) or a game
// folder (the same .txt / .ha6 file name is looked up and unpacked to a private folder), or any character file. The model
// comparison lives in char_diff.cpp; this file loads the other side, keeps the result current, marks the pattern lists
// (chardiff::Marks, read by MainPane), opens the other side in a read-only tab that follows the navigation, and draws the
// window. Included from main_frame.cpp.
#include "../char_diff.h"
#include "../app_log.h"

namespace {
// Loads the other side: a .txt stack, a .ha6 on its own, or (any character file) whatever File > Open would recognise.
std::unique_ptr<CharacterInstance> LoadDiffSide(const std::string& path, bool anyFormat, std::string* err)
{
	auto ch = std::make_unique<CharacterInstance>();
	std::string ext = std::filesystem::u8path(path).extension().u8string();
	for (auto& c : ext) c = (char)tolower((unsigned char)c);
	bool ok;
	if (anyFormat) {
		std::string fmt;
		ok = abrowser::LoadCharacterFile(*ch, path, &fmt, err);
	} else if (ext == ".txt") {
		ok = ch->loadFromTxt(path);
		if (!ok && err) *err = "could not load " + path;
	} else {
		ok = ch->loadHA6(path, false);
		if (!ok && err) *err = "could not load " + path;
	}
	if (!ok) return nullptr;
	return ch;
}

ImVec4 DiffKindColor(chardiff::Kind k)
{
	const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	ImVec4 c;
	if (!chardiff::KindColor(k, bg.x * 0.3f + bg.y * 0.59f + bg.z * 0.11f > 0.5f, &c.x)) return ImGui::GetStyleColorVec4(ImGuiCol_Text);
	return c;
}
const char* DiffKindLabel(chardiff::Kind k)
{
	switch (k) {
	case chardiff::Kind::Changed: return TXT("changed");
	case chardiff::Kind::Added: return TXT("added");
	case chardiff::Kind::Removed: return TXT("removed");
	default: return TXT("same");
	}
}
const char* DiffTr(const char* s) { return TXT(s); }
} // namespace

void MainFrame::NavigateView(CharacterView* view, int pattern, int frame)
{
	if (!view || !view->getCharacter()) return;
	FrameData& fd = view->getCharacter()->frameData;
	auto& st = view->getState();
	const bool newPattern = st.pattern != pattern;
	st.animating = false;
	st.pattern = std::clamp(pattern, 0, std::max(0, fd.get_sequence_count() - 1));
	Sequence* seq = fd.get_sequence(st.pattern);
	const int frames = seq ? (int)seq->frames.size() : 0;
	st.frame = std::clamp(frame, 0, std::max(0, frames - 1));
	st.currentTick = frames > 0 ? CalculateTickFromFrame(&fd, st.pattern, st.frame) : 0;
	if (newPattern) st.activeSpawns.clear();
}

void MainFrame::CharDiffClear()
{
	auto& d = m_diff;
	d.mine = nullptr; d.mineViewId = 0;
	d.otherPath.clear(); d.foundIn.clear(); d.otherLabel.clear();
	d.other.clear(); d.otherChar.reset();
	d.sideChar = nullptr; d.sideViewId = 0;   // an open side tab stays open (still a read-only copy)
	d.result = chardiff::Result();
	d.syncPattern = d.syncFrame = d.sideSetPattern = d.sideSetFrame = -1;
	chardiff::Marks().kinds = nullptr;
	chardiff::Marks().mine = chardiff::Marks().other = nullptr;
}

bool MainFrame::CharDiffLoad(CharacterInstance* mine, uint64_t mineViewId, const std::string& source, std::string* msg)
{
	namespace fs = std::filesystem;
	auto fail = [&](const std::string& why) { if (msg) *msg = why; HC_LOG_WARN("character diff: {}", why); return false; };
	if (!mine) return fail("no character open");
	if (source.empty()) return fail("choose an archive, a game folder or a character file to compare with");
	std::error_code ec;
	const fs::path sp = fs::u8path(source);
	if (!fs::exists(sp, ec)) return fail("not found: " + source);
	// An archive or a game folder: look for the same file name there. Anything else is a character file loaded as it is.
	bool lookup = fs::is_directory(sp, ec);
	std::string ext = sp.extension().u8string();
	for (auto& c : ext) c = (char)tolower((unsigned char)c);
	if (!lookup) {
		std::vector<uint8_t> head(64);
		std::ifstream f(sp, std::ios::binary);
		f.read((char*)head.data(), (std::streamsize)head.size());
		head.resize((size_t)f.gcount());
		lookup = fbarc::Detect(head.data(), head.size(), ext) != fbarc::Kind::Unknown || ext == ".p" || sp.filename().u8string() == "MBTL.exe";
	}
	const bool hasTxt = !mine->getTxtPath().empty();
	const std::string myFile = hasTxt ? mine->getTxtPath() : mine->getTopHA6Path();
	std::string otherPath, foundIn, err;
	std::unique_ptr<CharacterInstance> other;
	if (lookup) {
		const std::string want = fs::u8path(myFile).filename().u8string();
		std::string stem = sp.filename().u8string();
		for (auto& c : stem) if (!isalnum((unsigned char)c)) c = '_';
		const std::string stageRoot = (fs::temp_directory_path(ec) / "hantei_diff" / fs::u8path(stem)).u8string();
		otherPath = abrowser::StageCharacterByName(source, want, stageRoot, &foundIn, &err);
		if (otherPath.empty()) return fail(err);
		other = LoadDiffSide(otherPath, false, &err);
	} else {
		otherPath = foundIn = source;
		other = LoadDiffSide(otherPath, true, &err);
	}
	if (!other) return fail(err.empty() ? "could not load " + otherPath : err);

	CharDiffClear();
	auto& d = m_diff;
	d.mine = mine;
	d.mineViewId = mineViewId;
	d.source = source;
	d.otherPath = otherPath;
	d.foundIn = foundIn;
	d.otherLabel = fs::u8path(otherPath).filename().u8string();
	if (foundIn != otherPath) d.otherLabel += " (" + fs::u8path(foundIn).filename().u8string() + ")";
	d.other = other->frameData.m_sequences;
	d.otherChar = std::move(other);
	snprintf(d.sourceBuf, sizeof d.sourceBuf, "%s", source.c_str());
	if (gSettings.diffSource != source) { gSettings.diffSource = source; ImGui::MarkIniSettingsDirty(); }
	CharDiffRefresh();
	HC_LOG_INFO("character diff: {} vs {} (from {}): {} changed, {} added, {} removed, {:.1f} ms", mine->getName(), otherPath, foundIn,
		d.result.changed, d.result.added, d.result.removed, d.result.ms);
	if (msg) *msg = CharDiffSummary(0);
	return true;
}

void MainFrame::CharDiffRefresh()
{
	auto& d = m_diff;
	if (!d.mine) return;
	if (d.sideChar && isLiveCharacter(d.sideChar)) {   // the side tab may have been edited (it is a copy, but the UI does not stop edits)
		d.other = d.sideChar->frameData.m_sequences;
		d.sideVersion = d.sideChar->frameData.dataVersion;
	}
	d.result = chardiff::Compare(d.mine->frameData.m_sequences, d.other);
	d.mineVersion = d.mine->frameData.dataVersion;
	d.mineRevision = d.mine->undoManager.currentRevision();
	d.lastRefresh = ImGui::GetTime();
	char b[160];
	snprintf(b, sizeof b, TXT("%d changed, %d added, %d removed (%d frames differ), %.1f ms"),
		d.result.changed, d.result.added, d.result.removed, d.result.framesDiffering, d.result.ms);
	d.status = b;
	d.statusError = false;
}

std::string MainFrame::CharDiffSummary(int maxRows) const
{
	const auto& d = m_diff;
	if (!d.mine || !d.result.valid) return "no diff";
	char b[512];
	snprintf(b, sizeof b, "changed=%d added=%d removed=%d frames=%d ms=%.1f mine=%s other=%s from=%s", d.result.changed, d.result.added, d.result.removed,
		d.result.framesDiffering, d.result.ms, d.mine->getName().c_str(), d.otherPath.c_str(), d.foundIn.c_str());
	std::string s = b;
	int n = 0;
	for (const auto& p : d.result.patterns) {
		if (n++ >= maxRows) break;
		const Sequence* seq = p.pattern < (int)d.mine->frameData.m_sequences.size() ? &d.mine->frameData.m_sequences[p.pattern] : nullptr;
		if (p.kind == chardiff::Kind::Removed && p.pattern < (int)d.other.size()) seq = &d.other[p.pattern];
		snprintf(b, sizeof b, "\n  %d %s \"%s\": %s", p.pattern, chardiff::KindName(p.kind), seq ? seq->name.c_str() : "", chardiff::Describe(p).c_str());
		s += b;
	}
	if ((int)d.result.patterns.size() > maxRows && maxRows > 0) s += "\n  ... " + std::to_string(d.result.patterns.size() - maxRows) + " more";
	return s;
}

bool MainFrame::CharDiffOpenSideBySide(std::string* msg)
{
	auto& d = m_diff;
	if (!d.mine) { if (msg) *msg = "no diff"; return false; }
	CharacterView* mineView = findViewById(d.mineViewId);
	if (d.sideChar && isLiveCharacter(d.sideChar) && findViewById(d.sideViewId)) { if (msg) *msg = "already open"; return true; }
	std::unique_ptr<CharacterInstance> ch = std::move(d.otherChar);
	std::string err;
	if (!ch) ch = LoadDiffSide(d.otherPath, d.otherPath == d.foundIn, &err);
	if (!ch) { if (msg) *msg = err; return false; }
	ch->compareCopy = true;
	ch->setName(ch->getName() + " [" + (d.foundIn != d.otherPath ? std::filesystem::u8path(d.foundIn).filename().u8string() : std::string(TXT("reference"))) + "]");
	d.sideChar = ch.get();
	characters.push_back(std::move(ch));
	createViewForCharacter(d.sideChar);
	CharacterView* side = views.back().get();
	d.sideViewId = side->getId();
	d.sideVersion = d.sideChar->frameData.dataVersion;
	if (mineView) {
		NavigateView(side, mineView->getState().pattern, mineView->getState().frame);
		d.syncPattern = mineView->getState().pattern; d.syncFrame = mineView->getState().frame;
		d.sideSetPattern = side->getState().pattern; d.sideSetFrame = side->getState().frame;
	}
	// Its own window next to the main one (headless runs keep it as a tab: no OS windows), then back to the edited tab.
	SyncWorkspaceSession();
	if (!gStartup.headless) {
		const ImVec2 p = ImGui::GetMainViewport()->Pos, sz = ImGui::GetMainViewport()->Size;
		DetachViewToNewHost(d.sideViewId, ImVec2(p.x + sz.x * 0.5f, p.y + 80.f));
	}
	if (mineView) { const int i = findViewIndexById(d.mineViewId); if (i >= 0) setActiveView(i); }
	markProjectModified();
	if (msg) *msg = "opened " + d.sideChar->getName();
	return true;
}

void MainFrame::UpdateCharDiff()
{
	auto& d = m_diff;
	auto& marks = chardiff::Marks();
	if (d.mine && !isLiveCharacter(d.mine)) CharDiffClear();
	if (d.sideChar && !isLiveCharacter(d.sideChar)) {   // side tab closed: keep comparing against the last snapshot
		d.sideChar = nullptr; d.sideViewId = 0;
	}
	if (!d.mine || !d.result.valid) { marks.kinds = nullptr; marks.mine = marks.other = nullptr; return; }
	// Auto refresh after edits (data version moved), not while a mouse button is held (drags edit every frame).
	const bool stale = d.mine->frameData.dataVersion != d.mineVersion || d.mine->undoManager.currentRevision() != d.mineRevision
		|| (d.sideChar && d.sideChar->frameData.dataVersion != d.sideVersion);
	if (stale && d.autoRefresh && !ImGui::IsAnyMouseDown() && ImGui::GetTime() - d.lastRefresh > 0.25) CharDiffRefresh();
	marks.mine = &d.mine->frameData;
	marks.other = d.sideChar ? &d.sideChar->frameData : nullptr;
	marks.kinds = &d.result.kindOf;
	// Side by side: whichever tab the user moved, the other one follows.
	CharacterView* mv = findViewById(d.mineViewId);
	CharacterView* sv = d.sideChar ? findViewById(d.sideViewId) : nullptr;
	if (d.sync && mv && sv) {
		auto& a = mv->getState();
		auto& b = sv->getState();
		if (a.pattern != d.syncPattern || a.frame != d.syncFrame) {
			NavigateView(sv, a.pattern, a.frame);
			if (a.animating) b.currentTick = a.currentTick;
		} else if (b.pattern != d.sideSetPattern || b.frame != d.sideSetFrame) {
			NavigateView(mv, b.pattern, b.frame);
		}
		d.syncPattern = a.pattern; d.syncFrame = a.frame;
		d.sideSetPattern = b.pattern; d.sideSetFrame = b.frame;
		if (m_compare.character == d.sideChar) m_compare.pattern = a.pattern;
	}
}

void MainFrame::DrawCharDiffWindow()
{
	auto& d = m_diff;
	if (!d.open) return;
	ui::FitNextWindow(620, 640);
	if (!ImGui::Begin(LBL("Character diff"), &d.open)) { ImGui::End(); return; }
	CharacterView* toolView = getToolView();
	CharacterInstance* toolChar = toolView ? toolView->getCharacter() : nullptr;
	if (toolChar && toolChar->compareCopy && d.mine) { toolChar = d.mine; toolView = findViewById(d.mineViewId); }   // the side tab is focused: keep the edited one

	// ---- source -------------------------------------------------------------------------------------------------
	ImGui::SeparatorText(TXT("Compare with"));
	if (!d.sourceBuf[0] && !gSettings.diffSource.empty()) snprintf(d.sourceBuf, sizeof d.sourceBuf, "%s", gSettings.diffSource.c_str());
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##diffsource", TXT("game archive (.p), game folder or character file"), d.sourceBuf, sizeof d.sourceBuf);
	if (ImGui::Button(LBL("Archive / file..."))) {
		const std::string p = FileDialog(fileType::OPENANY);
		if (!p.empty()) snprintf(d.sourceBuf, sizeof d.sourceBuf, "%s", p.c_str());
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Game folder..."))) {
		const std::string p = BrowseForFolderUtf8(d.sourceBuf);
		if (!p.empty()) snprintf(d.sourceBuf, sizeof d.sourceBuf, "%s", p.c_str());
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!toolChar || !d.sourceBuf[0]);
	if (ImGui::Button(LBL("Compare"))) {
		std::string msg;
		if (!CharDiffLoad(toolChar, toolView ? toolView->getId() : 0, d.sourceBuf, &msg)) { d.status = i18n::TrDetail(msg); d.statusError = true; }
	}
	ImGui::EndDisabled();
	i18n::TextDisabledWrapped("%s", TXT("An archive or a game folder: the file with the same name as the open character (its .txt, else its .ha6) is "
		"unpacked to a private folder and compared. Any other file is compared as it is."));
	if (!toolChar) ImGui::TextDisabled("%s", TXT("Open a character first."));
	else if (!d.mine) ImGui::Text(TXT("Character: %s"), toolChar->getName().c_str());

	if (!d.status.empty()) {
		if (d.statusError) ImGui::TextColored(DiffKindColor(chardiff::Kind::Removed), "%s", d.status.c_str());
	}
	if (!d.mine || !d.result.valid) { ImGui::End(); return; }

	// ---- result ---------------------------------------------------------------------------------------------------
	ImGui::SeparatorText(TXT("Result"));
	ImGui::Text(TXT("%s  vs  %s"), d.mine->getName().c_str(), d.otherLabel.c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", d.otherPath.c_str(), d.foundIn.c_str());
	ImGui::TextColored(DiffKindColor(chardiff::Kind::Changed), TXT("%d changed"), d.result.changed);
	ImGui::SameLine(); ImGui::TextColored(DiffKindColor(chardiff::Kind::Added), TXT("%d added"), d.result.added);
	ImGui::SameLine(); ImGui::TextColored(DiffKindColor(chardiff::Kind::Removed), TXT("%d removed"), d.result.removed);
	ImGui::SameLine(); ImGui::TextDisabled(TXT("(%d frames differ, %.1f ms)"), d.result.framesDiffering, d.result.ms);
	const bool stale = d.mine->frameData.dataVersion != d.mineVersion || d.mine->undoManager.currentRevision() != d.mineRevision;
	if (ImGui::Button(LBL("Refresh"))) CharDiffRefresh();
	if (stale) { ImGui::SameLine(); ImGui::TextColored(DiffKindColor(chardiff::Kind::Changed), "%s", TXT("(out of date)")); }
	ImGui::SameLine();
	ImGui::Checkbox(LBL("Refresh after edits"), &d.autoRefresh);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Clear"))) { CharDiffClear(); d.status.clear(); ImGui::End(); return; }
	ImGui::Checkbox(LBL("Show only changed patterns"), &chardiff::Marks().onlyChanged);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Pattern lists (left pane, Pattern combo, search) list only the differing patterns."));
	const bool sideOpen = d.sideChar && findViewById(d.sideViewId);
	if (ImGui::Button(sideOpen ? LBL("Side by side: open") : LBL("Open side by side"))) {
		std::string msg;
		if (!CharDiffOpenSideBySide(&msg)) { d.status = i18n::TrDetail(msg); d.statusError = true; }
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Opens the other side in its own (read-only) tab, in a window of its own, following this tab's pattern and frame."));
	ImGui::SameLine();
	ImGui::Checkbox(LBL("Follow navigation"), &d.sync);
	ImGui::SameLine();
	ImGui::BeginDisabled(!sideOpen);
	if (ImGui::Button(LBL("Overlay"))) {
		m_compare.character = d.sideChar; m_compare.enabled = true; m_compare.followTick = true; m_compare.sim.reset();
		if (CharacterView* mv = findViewById(d.mineViewId)) m_compare.pattern = mv->getState().pattern;
		m_showCompare = true;
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", TXT("Draws the other side tinted over this tab (Pattern comparison window). Needs the side-by-side tab."));

	// ---- summary list -----------------------------------------------------------------------------------------------
	CharacterView* mv = findViewById(d.mineViewId);
	const int curPattern = mv ? mv->getState().pattern : -1;
	const int curFrame = mv ? mv->getState().frame : 0;
	ImGui::SetNextItemWidth(ui::Px(200));
	ImGui::InputTextWithHint("##difffilter", TXT("filter (name or ID)"), d.filter, sizeof d.filter);
	std::vector<int> rows;
	rows.reserve(d.result.patterns.size());
	const auto& mineSeqs = d.mine->frameData.m_sequences;
	auto nameOf = [&](const chardiff::PatternDiff& p) -> const std::string& {
		static const std::string none;
		if (p.kind == chardiff::Kind::Removed) return p.pattern < (int)d.other.size() ? d.other[p.pattern].name : none;
		return p.pattern < (int)mineSeqs.size() ? mineSeqs[p.pattern].name : none;
	};
	for (int i = 0; i < (int)d.result.patterns.size(); i++) {
		if (d.filter[0]) {
			const auto& p = d.result.patterns[i];
			std::string hay = std::to_string(p.pattern) + " " + nameOf(p);
			std::string needle = d.filter;
			for (auto& c : hay) c = (char)tolower((unsigned char)c);
			for (auto& c : needle) c = (char)tolower((unsigned char)c);
			if (hay.find(needle) == std::string::npos) continue;
		}
		rows.push_back(i);
	}
	const float listH = std::max(ui::Px(120), ImGui::GetContentRegionAvail().y * 0.45f);
	if (ImGui::BeginTable("##diffrows", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable,
		ImVec2(0, listH))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(TXT("Pattern"), ImGuiTableColumnFlags_WidthFixed, ui::Px(52));
		ImGui::TableSetupColumn(TXT("Name"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn(TXT("Status"), ImGuiTableColumnFlags_WidthFixed, ui::Px(64));
		ImGui::TableSetupColumn(TXT("What differs"), ImGuiTableColumnFlags_WidthStretch, 1.6f);
		ImGui::TableHeadersRow();
		ImGuiListClipper clip;
		clip.Begin((int)rows.size());
		while (clip.Step())
			for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
				const auto& p = d.result.patterns[rows[r]];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(p.pattern);
				char id[16]; snprintf(id, sizeof id, "%03d", p.pattern);
				if (ImGui::Selectable(id, p.pattern == curPattern, ImGuiSelectableFlags_SpanAllColumns)) {
					if (mv) {
						NavigateView(mv, p.pattern, p.frames.empty() ? 0 : p.frames.front().frame);
						if (mv->getMainPane()) mv->getMainPane()->RegenerateNames();
					}
				}
				ImGui::PopID();
				ImGui::TableNextColumn(); ImGui::TextUnformatted(nameOf(p).c_str());
				ImGui::TableNextColumn(); ImGui::TextColored(DiffKindColor(p.kind), "%s", DiffKindLabel(p.kind));
				ImGui::TableNextColumn(); ImGui::TextUnformatted(chardiff::Describe(p, DiffTr).c_str());
			}
		ImGui::EndTable();
	}

	// ---- the selected pattern / frame -------------------------------------------------------------------------------
	if (curPattern < 0) { ImGui::End(); return; }
	const Sequence* a = curPattern < (int)mineSeqs.size() ? &mineSeqs[curPattern] : nullptr;
	const Sequence* b = curPattern < (int)d.other.size() ? &d.other[curPattern] : nullptr;
	const chardiff::PatternDiff* pd = d.result.find(curPattern);
	char hdr[160];
	snprintf(hdr, sizeof hdr, TXT("Pattern %d, frame %d"), curPattern, curFrame);
	ImGui::SeparatorText(hdr);
	if (!pd) { ImGui::TextDisabled("%s", TXT("This pattern is the same on both sides.")); ImGui::End(); return; }
	ImGui::TextColored(DiffKindColor(pd->kind), "%s", DiffKindLabel(pd->kind));
	ImGui::SameLine();
	ImGui::TextUnformatted(chardiff::Describe(*pd, DiffTr).c_str());
	if (!pd->frames.empty()) {   // jump to a differing frame
		ImGui::TextUnformatted(TXT("Frames:"));
		for (const auto& f : pd->frames) {
			ImGui::SameLine();
			char lb[24]; snprintf(lb, sizeof lb, "%d##df%d", f.frame, f.frame);
			const bool cur = f.frame == curFrame;
			if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			if (ImGui::SmallButton(lb) && mv) NavigateView(mv, curPattern, f.frame);
			if (cur) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", chardiff::AreaNames(f.areas, DiffTr).c_str());
		}
	}
	std::vector<chardiff::FieldDiff> fields = chardiff::HeaderFields(a, b);
	const Frame* fa = a && curFrame < (int)a->frames.size() ? &a->frames[curFrame] : nullptr;
	const Frame* fb = b && curFrame < (int)b->frames.size() ? &b->frames[curFrame] : nullptr;
	if (fa || fb) {
		auto ff = chardiff::FrameFields(fa, fb);
		if (!fa || !fb) {
			chardiff::FieldDiff m; m.area = chardiff::aFrameCount; m.field = TXT("frame");
			m.mine = fa ? TXT("present") : "-"; m.other = fb ? TXT("present") : "-";
			fields.push_back(m);
		}
		fields.insert(fields.end(), ff.begin(), ff.end());
	}
	if (fields.empty()) { ImGui::TextDisabled("%s", TXT("This frame is the same on both sides.")); ImGui::End(); return; }
	if (ImGui::BeginTable("##difffields", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(TXT("Area"), ImGuiTableColumnFlags_WidthFixed, ui::Px(52));
		ImGui::TableSetupColumn(TXT("Field"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
		ImGui::TableSetupColumn(TXT("This file"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn(TXT("Other"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();
		for (const auto& f : fields) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TextDisabled("%s", chardiff::AreaNames(f.area, DiffTr).c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(f.field.c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(DiffKindColor(chardiff::Kind::Changed), "%s", f.mine.c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(f.other.c_str());
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

// --ui-script: diffwith <archive|folder|file> | diffrefresh | diffside | diffstatus | difffields <pattern> <frame> | diffonly on|off | diffclear
bool MainFrame::CharDiffScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	auto& d = m_diff;
	if (cmd == "diffwith") {
		CharacterView* v = getActiveView();
		d.open = true;
		if (!CharDiffLoad(v ? v->getCharacter() : nullptr, v ? v->getId() : 0, arg, msg)) { if (msg) *msg = "FAILED " + *msg; return true; }
		if (msg) *msg = CharDiffSummary(25);
		return true;
	}
	if (cmd == "diffrefresh") { CharDiffRefresh(); if (msg) *msg = CharDiffSummary(25); return true; }
	if (cmd == "diffside") { std::string m; const bool ok = CharDiffOpenSideBySide(&m); if (msg) *msg = (ok ? "" : "FAILED ") + m + " views=" + std::to_string(views.size()); return true; }
	if (cmd == "diffonly") { chardiff::Marks().onlyChanged = arg != "off" && arg != "0"; if (msg) *msg = chardiff::Marks().onlyChanged ? "on" : "off"; return true; }
	if (cmd == "diffclear") { CharDiffClear(); if (msg) *msg = "cleared"; return true; }
	if (cmd == "diffstatus") {
		CharacterView* mv = findViewById(d.mineViewId);
		CharacterView* sv = d.sideChar ? findViewById(d.sideViewId) : nullptr;
		char b[256];
		snprintf(b, sizeof b, "mine=%d/%d side=%d/%d stale=%d ", mv ? mv->getState().pattern : -1, mv ? mv->getState().frame : -1,
			sv ? sv->getState().pattern : -1, sv ? sv->getState().frame : -1, d.mine && d.mine->frameData.dataVersion != d.mineVersion ? 1 : 0);
		if (msg) *msg = b + CharDiffSummary(0);
		return true;
	}
	if (cmd == "difffields") {
		int p = 0, f = 0;
		if (!d.mine || sscanf(arg.c_str(), "%d %d", &p, &f) < 1) { if (msg) *msg = "FAILED no diff / usage: difffields <pattern> [frame]"; return true; }
		const auto& ms = d.mine->frameData.m_sequences;
		const Sequence* a = p >= 0 && p < (int)ms.size() ? &ms[p] : nullptr;
		const Sequence* b = p >= 0 && p < (int)d.other.size() ? &d.other[p] : nullptr;
		auto rows = chardiff::HeaderFields(a, b);
		const Frame* fa = a && f >= 0 && f < (int)a->frames.size() ? &a->frames[f] : nullptr;
		const Frame* fb = b && f >= 0 && f < (int)b->frames.size() ? &b->frames[f] : nullptr;
		if (fa || fb) { auto ff = chardiff::FrameFields(fa, fb); rows.insert(rows.end(), ff.begin(), ff.end()); }
		std::string s = std::to_string(rows.size()) + " fields";
		int n = 0;
		for (const auto& r : rows) {
			if (n++ >= 40) { s += "\n  ..."; break; }
			s += "\n  [" + chardiff::AreaNames(r.area) + "] " + r.field + ": " + r.mine + " | " + r.other;
		}
		if (msg) *msg = s;
		return true;
	}
	return false;
}

#endif
