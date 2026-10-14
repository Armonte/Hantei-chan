#ifndef UI_PAT_EDITOR_IMPL_H_GUARD
#define UI_PAT_EDITOR_IMPL_H_GUARD

// ============================================================================
// PAT editor: file safety and editing comfort (sosfiro parity, docs/audit_2026_10_14/SOSFIRO_PARITY.md)
// ============================================================================
// Included at the end of main_frame.cpp (like main_ui_impl.h).
//  - Auto backup before every save (HA6 / character files and .pat), Preferences toggle, cap and folder
//  - PAT documents: Save writes the .pat, New PAT, reload of a shared .pat in the other tabs after a save
//  - per-frame selection / highlight sync of PAT tabs (stale indices after undo, reload or deletes)
//  - viewport: right-drag edits the selected cut-out's UV rectangle, outline of the selected part
//  - keyboard: part set / part navigation, Ctrl+arrow nudges, H / T / U
//  - Tools: Fill missing layers, Scale all layers

static BackupOptions BackupOptionsFor(bool pat)
{
	BackupOptions o;
	o.enabled = pat ? gSettings.backupPat : gSettings.backupHa6;
	o.keep = gSettings.backupKeep;
	o.folder = gSettings.backupFolder;
	return o;
}

void MainFrame::backupBeforeSave(const std::string& path, bool pat)
{
	std::string err;
	if (!BackupBeforeSave(path, BackupOptionsFor(pat), nullptr, &err))
		requestErrorPopup("Backup", std::string(TXT("The automatic backup before saving failed (the file was still saved): ")) + err);
}

// Folder picker results are UTF-8; editor file paths are in the ANSI code page (file_backup.h).
static std::string Utf8ToAcp(const std::string& s)
{
	if (s.empty()) return s;
	const int wn = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(wn, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], wn);
	const int an = WideCharToMultiByte(CP_ACP, 0, w.data(), wn, nullptr, 0, nullptr, nullptr);
	std::string a(an, 0);
	WideCharToMultiByte(CP_ACP, 0, w.data(), wn, &a[0], an, nullptr, nullptr);
	return a;
}

static std::string FileNameOf(const std::string& p)
{
	const size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

// The placeholder character of a PAT tab: one pattern with one frame whose layer 0 draws the selected part set.
static std::unique_ptr<CharacterInstance> MakePatPlaceholder(const std::string& name)
{
	auto character = std::make_unique<CharacterInstance>();
	character->frameData.initEmpty();
	character->setName(name);
	if (auto seq = character->frameData.get_sequence(0)) {
		auto frame = &seq->frames.emplace_back();
		if (frame->AF.layers.empty()) frame->AF.layers.push_back({});
		frame->AF.layers[0].usePat = true;
		frame->AF.layers[0].spriteId = 0;   // the first part set
	}
	return character;
}

void MainFrame::addPatDocumentView(std::unique_ptr<CharacterInstance> character)
{
	render.SetParts(&character->parts);
	characters.push_back(std::move(character));
	auto view = std::make_unique<CharacterView>(characters.back().get(), &render);
	view->setPatEditor(true);
	view->setViewNumber(0);
	// Panes AFTER the PAT flag: refreshPanes creates the PAT panes and hides the HA6 ones.
	view->refreshPanes(&render);
	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
	markProjectModified();
	needsDockRebuild = true;   // dock the PAT panes
}

void MainFrame::createPatEditorView(const std::string& patPath)
{
	auto character = MakePatPlaceholder(FileNameOf(patPath));
	if (!character->loadPAT(patPath)) {
		requestErrorPopup("Loading Error", patPath);
		return;
	}
	character->makePatDocument();
	addPatDocumentView(std::move(character));
}

void MainFrame::createNewPatView(bool mbaaccFormat)
{
	auto character = MakePatPlaceholder(TXT("New PAT"));
	character->newPatDocument(mbaaccFormat);
	addPatDocumentView(std::move(character));
}

// Save a character's .pat. `path` empty = its own .pat; a New PAT without one asks where (Save As).
bool MainFrame::savePatOf(CharacterInstance* character, const std::string& path)
{
	if (!character || !character->parts.loaded) return false;
	std::string target = path.empty() ? character->getPATPath() : path;
	if (target.empty()) {
		target = FileDialog(fileType::PAT, true);
		if (target.empty()) return false;
	}
	backupBeforeSave(target, true);
	if (!character->savePATAs(target)) {
		requestErrorPopup("Save Error", std::string(TXT("Could not write the .pat (it may be locked by another program, or the PAT has no data yet): ")) + target);
		return false;
	}
	if (character->isPatDocument()) character->setName(FileNameOf(target));
	reloadPatInOtherTabs(character, target);
	updateWindowTitle();
	return true;
}

// sosfiro's ReloadPatInTabs: every other character that draws this .pat (HA6 tabs, effect characters, another PAT tab
// on the same file) reloads it, so what they show is what was saved. A PAT tab with unsaved edits of its own keeps them.
void MainFrame::reloadPatInOtherTabs(CharacterInstance* saver, const std::string& path)
{
	const std::string key = normalizePath(path);
	auto reload = [&](CharacterInstance* c) {
		if (!c || c == saver || !c->parts.loaded || c->parts.fbPartModel) return;
		const std::string own = !c->getPATPath().empty() ? c->getPATPath() : c->parts.filePath;
		if (own.empty() || normalizePath(own) != key) return;
		if (c->isPatDocument() && c->isModified()) return;
		if (!c->loadPAT(path)) return;
		if (c->isPatDocument()) c->undoManager.reset();   // the reloaded file is the new baseline
		refreshPatPanes(c);
	};
	for (auto& c : characters) {
		reload(c.get());
		reload(c->effectCharacter.get());
	}
	if (auto* v = getActiveView())
		if (auto* c = v->getCharacter()) if (c->parts.loaded) render.SetParts(&c->parts);
}

// Selection indices of a PAT view inside the model (they go stale after undo, reload, a delete or another view's
// edit), and the animator's part sets inside the list.
static void ClampPatState(FrameState& st, Parts& p)
{
	auto clampTo = [](int& v, size_t n) { v = n == 0 ? 0 : std::clamp(v, 0, (int)n - 1); };
	clampTo(st.partSet, p.partSets.size());
	const auto* set = p.GetPartSet(st.partSet);
	clampTo(st.partProp, set ? set->groups.size() : 0);
	clampTo(st.partCutOut, p.cutOuts.size());
	clampTo(st.partShape, p.shapes.size());
	clampTo(st.partGraph, p.gfxMeta.size());
	clampTo(st.framePatEditor, st.animationSequence.frames.size());
	for (auto& f : st.animationSequence.frames)
		if (!f.AF.layers.empty() && (f.AF.layers[0].spriteId < 0 || f.AF.layers[0].spriteId >= (int)p.partSets.size()))
			f.AF.layers[0].spriteId = 0;
}

void MainFrame::refreshPatPanes(CharacterInstance* character)
{
	for (auto& v : views) {
		if (v->getCharacter() != character || !v->isPatEditor()) continue;
		ClampPatState(v->getState(), character->parts);
		if (auto* p = v->getPartSetPane()) { p->RegeneratePartSetNames(); p->RegeneratePartPropNames(); p->UpdateHighligth(p->activeHighligth); }
		if (auto* p = v->getPartPane()) p->RegeneratePartCutOutsNames();
		if (auto* p = v->getShapePane()) p->RegenerateShapesNames();
		if (auto* p = v->getTexturePane()) p->RegenerateTexturesNames();
		if (auto* p = v->getToolPane()) p->RegeneratePartsetNames();
	}
}

// Once per UI frame for a PAT view, before its panes draw.
void MainFrame::SyncPatView(CharacterView* view)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor() || !ch->parts.loaded) return;
	// Undo / redo restored the model: rebuild the panes' name lists (the channel cannot reach the panes).
	if (auto* channel = ch->patDocumentUndo())
		if (!channel->onRestored) channel->onRestored = [this, ch] { refreshPatPanes(ch); };
	auto& st = view->getState();
	ClampPatState(st, ch->parts);
	if (auto* pane = view->getPartSetPane()) {
		const auto* prop = ch->parts.GetPartProp(st.partSet, st.partProp);
		ch->parts.partHighlight = pane->activeHighligth && prop ? prop->propId : -1;
		ch->parts.SetHighlightOpacity(pane->highlightOpacity);
	}
	memcpy(render.patCenterColor, gSettings.patCenterColor, sizeof render.patCenterColor);
	memcpy(render.patUvRectColor, gSettings.patUvRectColor, sizeof render.patUvRectColor);
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

// Up / Down: part set, Left / Right (Q / E, Num/ / Num*): part (sosfiro main_frame.cpp AdvancePattern / AdvanceFrame).
bool MainFrame::PatNavigate(CharacterView* view, int dSet, int dProp)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor() || !ch->parts.loaded) return false;
	auto& st = view->getState();
	if (st.animating) return true;
	const int nSets = (int)ch->parts.partSets.size();
	if (dSet && nSets > 0) {
		const int next = std::clamp(st.partSet + dSet, 0, nSets - 1);
		if (auto* pane = view->getPartSetPane()) pane->UpdateRenderFrame(next);
		else { st.partSet = next; st.partProp = 0; }
	}
	if (dProp) {
		const auto* set = ch->parts.GetPartSet(st.partSet);
		const int n = set ? (int)set->groups.size() : 0;
		st.partProp = n > 0 ? std::clamp(st.partProp + dProp, 0, n - 1) : 0;
		if (auto* pane = view->getPartSetPane()) pane->UpdateHighligth(pane->activeHighligth);
	}
	return true;
}

// Ctrl+arrows: move the selected part 1 px (Ctrl+Shift: 10 px); in the UV view the cut-out's UV rectangle.
bool MainFrame::PatNudge(CharacterView* view, int dx, int dy)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor() || !ch->parts.loaded) return false;
	auto& st = view->getState();
	if (st.animating) return false;
	if (st.renderMode == RenderMode::UV_SETTING_VIEW) {
		auto* co = ch->parts.GetCutOut(st.partCutOut);
		if (!co) return false;
		co->uv[0] = std::clamp(co->uv[0] + dx, 0, 256);
		co->uv[1] = std::clamp(co->uv[1] + dy, 0, 256);
	} else {
		auto* prop = ch->parts.GetPartProp(st.partSet, st.partProp);
		if (!prop) return false;
		prop->x += dx;
		prop->y += dy;
	}
	ch->undoManager.markModified();
	return true;
}

// T / U and the pane buttons: toggle a render mode (a second press returns to the part set).
void MainFrame::TogglePatRenderMode(CharacterView* view, RenderMode mode)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor() || !ch->parts.loaded) return;
	auto& st = view->getState();
	st.renderMode = st.renderMode == mode ? RenderMode::DEFAULT : mode;
	// The placeholder frame of a PAT document shows the part set in the default view (the pane buttons do the same).
	if (ch->isPatDocument() && !st.animating) {
		if (auto* seq = ch->frameData.get_sequence(0)) {
			if (!seq->frames.empty()) {
				auto& layers = seq->frames[0].AF.layers;
				if (layers.empty()) layers.push_back({});
				layers[0].spriteId = st.renderMode != RenderMode::DEFAULT ? 0 : st.partSet;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------

// World point -> pixel of the sheet shown in the UV view. The sheet is drawn from the view origin (Parts::Draw draws
// one part at (0, 0) with the quad 0..w, 0..h; Render::GenerateUVRectangleVertices uses the same placement).
static bool UvSheetPixel(Parts& parts, const FrameState& st, float wx, float wy, float& px, float& py)
{
	const auto* co = parts.GetCutOut(st.partCutOut);
	const auto* g = co ? parts.GetPartGfx(co->texture) : nullptr;
	if (!g || g->w <= 0 || g->h <= 0) return false;
	px = wx;
	py = wy;
	return true;
}

// Right press in a PAT tab: in the UV view it starts editing the selected cut-out's UV rectangle (sosfiro
// PatPartPane::BoxStart); elsewhere it does nothing. It never draws a hitbox into the placeholder frame.
bool MainFrame::BeginUvDrag(CharacterView* view, float wx, float wy)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor()) return false;
	EndUvDrag(false);
	auto& st = view->getState();
	if (st.animating || st.renderMode != RenderMode::UV_SETTING_VIEW || !ch->parts.loaded) return true;
	float px, py;
	if (!UvSheetPixel(ch->parts, st, wx, wy, px, py)) return true;
	auto* pane = view->getPartPane();
	if (!pane) return true;
	ch->undoManager.beginTransaction(TXT("Edit UV rectangle"));
	if (!pane->BoxStart(px, py)) { ch->undoManager.cancelTransaction(); return true; }
	m_uvDrag.active = true;
	m_uvDrag.character = ch;
	m_uvDrag.viewId = view->getId();
	m_uvDrag.wx = wx;
	m_uvDrag.wy = wy;
	return true;
}

void MainFrame::UvDragBy(float dwx, float dwy)
{
	if (!m_uvDrag.active) return;
	CharacterView* view = findViewById(m_uvDrag.viewId);
	if (!view || view->getCharacter() != m_uvDrag.character || !isLiveCharacter(m_uvDrag.character)) { m_uvDrag = UvDrag{}; return; }
	m_uvDrag.wx += dwx;
	m_uvDrag.wy += dwy;
	float px, py;
	if (UvSheetPixel(m_uvDrag.character->parts, view->getState(), m_uvDrag.wx, m_uvDrag.wy, px, py))
		if (auto* pane = view->getPartPane()) pane->BoxDrag(px, py);
	m_uvDrag.character->undoManager.markModified();
}

void MainFrame::EndUvDrag(bool cancel)
{
	if (!m_uvDrag.active) return;
	CharacterInstance* ch = m_uvDrag.character;
	m_uvDrag = UvDrag{};
	if (!isLiveCharacter(ch)) return;
	if (cancel) ch->undoManager.cancelTransaction();
	else ch->undoManager.commitTransaction();
}

// Outline of the selected part (sosfiro drew a square in selectedPartPropColor behind it). Plane parts in the default
// view: the cut-out quad through the part's transform (part_transform.h), drawn over the view.
void MainFrame::DrawPatSelectionOutline(CharacterView* view, ImDrawList* dl, ImVec2 origin, float zoom)
{
	auto* ch = view ? view->getCharacter() : nullptr;
	if (!ch || !view->isPatEditor() || !ch->parts.loaded || !dl) return;
	const auto& st = view->getState();
	if (st.animating) return;
	if (st.renderMode == RenderMode::UV_SETTING_VIEW) {
		// The selected cut-out's UV rectangle on its sheet (drawn from the view origin, 1/256 of the sheet per UV unit).
		// Render::GenerateUVRectangleVertices fills the old box buffer, which the layered renderer no longer draws.
		const auto* cut = ch->parts.GetCutOut(st.partCutOut);
		const auto* g = cut ? ch->parts.GetPartGfx(cut->texture) : nullptr;
		if (!g || g->w <= 0 || g->h <= 0) return;
		const float sx = g->w / 256.f * zoom, sy = g->h / 256.f * zoom;
		const ImVec2 a(origin.x + cut->uv[0] * sx, origin.y + cut->uv[1] * sy);
		const ImVec2 b(a.x + cut->uv[2] * sx, a.y + cut->uv[3] * sy);
		const float* c = gSettings.patUvRectColor;
		dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * 0.25f)));
		dl->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3])), 0.f, 0, 2.0f);
		return;
	}
	if (st.renderMode != RenderMode::DEFAULT || !gSettings.patOutlineSelected) return;
	const auto* prop = ch->parts.GetPartProp(st.partSet, st.partProp);
	if (!prop || prop->ppId < 0 || prop->ppId >= (int)ch->parts.cutOuts.size()) return;
	const auto& co = ch->parts.cutOuts[prop->ppId];
	if (co.wh[0] == 0 && co.wh[1] == 0) return;
	if (const auto* sh = ch->parts.GetShape(co.shapeIndex))
		if (!ch->parts.shapes.empty() && sh->type != ShapeType::PLANE && sh->type != ShapeType::UNK2) return;   // 3D shape: centre cross only
	const glm::mat4 m = partxf::PartMatrix(partxf::Pivot(ch->parts.fbPartModel, (float)prop->x, (float)prop->y, co),
		glm::vec3(prop->rotation[1], prop->rotation[2], prop->rotation[3]), glm::vec2(prop->pras[0], prop->pras[1]),
		glm::vec2(prop->scaleX, prop->scaleY));
	ImVec2 pts[4];
	const float t[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	for (int i = 0; i < 4; i++) {
		const glm::vec2 q = partxf::QuadPoint(co, t[i][0], t[i][1]);
		const glm::vec4 w = m * glm::vec4(q.x, q.y, 0.f, 1.f);
		pts[i] = ImVec2(origin.x + w.x * zoom, origin.y + w.y * zoom);
	}
	const float* c = gSettings.patSelectedColor;
	dl->AddPolyline(pts, 4, ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3])), ImDrawFlags_Closed, 2.0f);
}

// ---------------------------------------------------------------------------
// Menus and windows
// ---------------------------------------------------------------------------

// Edit > Preferences: Auto backup, PAT editor, Mouse buttons (menus), and the same settings in one window.
void MainFrame::DrawBackupPrefsItems()
{
	{
		bool changed = false;
		changed |= ImGui::Checkbox(LBL("Back up character files (HA6, ...) before saving"), &gSettings.backupHa6);
		changed |= ImGui::Checkbox(LBL("Back up .pat files before saving"), &gSettings.backupPat);
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputInt(LBL("Backups kept per file (0 = all)"), &gSettings.backupKeep)) {
			gSettings.backupKeep = std::clamp(gSettings.backupKeep, 0, 999);
			changed = true;
		}
		char folder[512];
		snprintf(folder, sizeof folder, "%s", gSettings.backupFolder.c_str());
		ImGui::SetNextItemWidth(260);
		if (ImGui::InputTextWithHint(LBL("Backup folder"), "backup", folder, sizeof folder)) {
			gSettings.backupFolder = folder;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(LBL("Browse..."))) {
			const std::string picked = BrowseForFolderUtf8(AnsiToUtf8(gSettings.backupFolder));
			if (!picked.empty()) { gSettings.backupFolder = Utf8ToAcp(picked); changed = true; }
		}
		ImGui::TextDisabled("%s", TXT("Empty: a \"backup\" folder next to the file. A relative folder is relative to the file."));
		ImGui::TextDisabled("%s", TXT("Every save (menu, Ctrl+S, close prompt) copies the old file to <name>.<date-time>-NN.bak first."));
		if (changed) ImGui::MarkIniSettingsDirty();
	}
}

void MainFrame::DrawPatColourPrefsItems()
{
	{
		bool changed = false;
		changed |= ImGui::ColorEdit4(LBL("Selected part outline"), gSettings.patSelectedColor, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::Checkbox(LBL("Show##outline"), &gSettings.patOutlineSelected);
		changed |= ImGui::ColorEdit4(LBL("Part centre cross"), gSettings.patCenterColor, ImGuiColorEditFlags_NoInputs);
		changed |= ImGui::ColorEdit4(LBL("UV rectangle"), gSettings.patUvRectColor, ImGuiColorEditFlags_NoInputs);
		ImGui::SetNextItemWidth(120);
		changed |= ImGui::SliderFloat(LBL("Highlight opacity of the other parts"), &gSettings.patHighlightOpacity, 0.f, 1.f, "%.2f");
		if (changed) {
			for (auto& v : views)
				if (auto* p = v->getPartSetPane()) p->highlightOpacity = gSettings.patHighlightOpacity;
			ImGui::MarkIniSettingsDirty();
		}
	}
}

void MainFrame::DrawMousePrefsItems()
{
	{
		// Issue #9: which button pans the view and which draws boxes / UV rectangles.
		const char* buttons[] = {LBL("Left"), LBL("Right"), LBL("Middle")};
		int pan = gSettings.mousePanButton, draw = gSettings.mouseDrawButton;
		ImGui::SetNextItemWidth(120);
		bool changed = ImGui::Combo(LBL("Pan the view"), &pan, buttons, 3);
		ImGui::SetNextItemWidth(120);
		changed |= ImGui::Combo(LBL("Draw boxes / UV rectangle"), &draw, buttons, 3);
		if (changed) {
			if (pan == draw) {   // keep them on different buttons: swap
				if (pan != gSettings.mousePanButton) draw = gSettings.mousePanButton;
				else pan = gSettings.mouseDrawButton;
			}
			gSettings.mousePanButton = pan;
			gSettings.mouseDrawButton = draw;
			ImGui::MarkIniSettingsDirty();
		}
		if (ImGui::SmallButton(LBL("Default (left pans, right draws)"))) {
			gSettings.mousePanButton = 0;
			gSettings.mouseDrawButton = 1;
			ImGui::MarkIniSettingsDirty();
		}
		ImGui::TextDisabled("%s", TXT("The middle button always pans when it does not draw."));
	}
}

void MainFrame::DrawPatPreferences()
{
	if (ImGui::BeginMenu(LBL("Auto backup"))) { DrawBackupPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("PAT editor"))) { DrawPatColourPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("Mouse buttons"))) { DrawMousePrefsItems(); ImGui::EndMenu(); }
	if (ImGui::MenuItem(LBL("Saving and PAT editor settings..."), nullptr, m_showPatPrefs)) m_showPatPrefs = !m_showPatPrefs;
}

void MainFrame::DrawPatPreferencesWindow()
{
	if (!m_showPatPrefs) return;
	const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mainPos.x + 200, mainPos.y + 140), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Saving and PAT editor settings"), &m_showPatPrefs, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
		ImGui::End();
		return;
	}
	ImGui::SeparatorText(TXT("Auto backup"));
	DrawBackupPrefsItems();
	ImGui::SeparatorText(TXT("PAT editor"));
	DrawPatColourPrefsItems();
	ImGui::SeparatorText(TXT("Mouse buttons"));
	DrawMousePrefsItems();
	ImGui::End();
}

// File > New PAT / the "+" tab menu: pick the layout of the new file.
void MainFrame::DrawNewPatPopup()
{
	if (m_openNewPat) {
		ImGui::OpenPopup(LBL("New PAT"));
		m_openNewPat = false;
	}
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(LBL("New PAT"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(TXT("Which game is the new .pat for?"));
		ImGui::Separator();
		if (ImGui::Button(LBL("UNI / UNI2 / MBTL"), ImVec2(200, 0))) { createNewPatView(false); ImGui::CloseCurrentPopup(); }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("PGT2 textures (DXT), 3D shapes (VEST)."));
		ImGui::SameLine();
		if (ImGui::Button(LBL("MBAACC"), ImVec2(200, 0))) { createNewPatView(true); ImGui::CloseCurrentPopup(); }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("PGTX textures (32-bit), flat parts only, MBAACC tag names."));
		if (ImGui::Button(LBL("Cancel"), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// Tools > Layers (sosfiro Tools > Fill missing layers / Scale all layers), on the active character.
void MainFrame::DrawLayerToolsMenu()
{
	CharacterView* view = getActiveView();
	CharacterInstance* ch = view && !view->isStageView() && !view->isPatEditor() ? view->getCharacter() : nullptr;
	if (!ImGui::BeginMenu(LBL("Layers"), ch != nullptr)) return;
	const int count = ch->frameData.gameLayerCount();
	char fill[160];
	snprintf(fill, sizeof fill, TXT("Fill missing layers (every frame gets %d)"), count);
	if (ImGui::MenuItem(fill, nullptr, false, count > 1)) {
		ch->undoManager.beginTransaction(TXT("Fill missing layers"));
		int changed = 0;
		for (int i = 0; i < ch->frameData.get_sequence_count(); i++) {
			Sequence* seq = ch->frameData.get_sequence(i);
			if (!seq) continue;
			bool any = false;
			for (auto& f : seq->frames)
				if ((int)f.AF.layers.size() < count) { f.AF.layers.resize(count); any = true; }
			if (any) { ch->frameData.mark_modified(i); changed++; }
		}
		ch->undoManager.commitTransaction();
		if (changed) ch->markModified();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", TXT("Adds empty layers up to the game's layer count (UNI 5, MBTL 3)."));
	ImGui::Separator();
	ImGui::SetNextItemWidth(90);
	ImGui::InputFloat(LBL("Factor##layerscale"), &m_layerScaleFactor, 0.f, 0.f, "%.3f");
	ImGui::SameLine();
	if (ImGui::Button(LBL("Scale all layers and boxes")) && m_layerScaleFactor > 0.f && m_layerScaleFactor != 1.f) {
		ch->undoManager.beginTransaction(TXT("Scale all layers"));
		const float k = m_layerScaleFactor;
		for (int i = 0; i < ch->frameData.get_sequence_count(); i++) {
			Sequence* seq = ch->frameData.get_sequence(i);
			if (!seq || seq->frames.empty()) continue;
			for (auto& f : seq->frames) {
				for (auto& l : f.AF.layers) { l.scale[0] *= k; l.scale[1] *= k; }
				for (auto& [id, box] : f.hitboxes)
					for (int j = 0; j < 4; j++) box.xy[j] = (int)std::lround(box.xy[j] * k);
			}
			ch->frameData.mark_modified(i);
		}
		ch->undoManager.commitTransaction();
		ch->markModified();
		ImGui::CloseCurrentPopup();
	}
	ImGui::TextDisabled("%s", TXT("Multiplies every layer's scale and every box of every pattern (one undo step)."));
	ImGui::EndMenu();
}

// ---------------------------------------------------------------------------
// --ui-script commands for the PAT editor (tools/ui/shots, docs/audit_2026_10_14/evidence/s2)
// ---------------------------------------------------------------------------
bool MainFrame::PatScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	CharacterView* view = getActiveView();
	CharacterInstance* ch = view ? view->getCharacter() : nullptr;
	if (cmd == "patopen") { createPatEditorView(arg); return true; }
	if (cmd == "newpat") { createNewPatView(arg == "mbaacc"); return true; }
	if (cmd == "prefswindow") { m_showPatPrefs = arg != "off"; return true; }
	if (cmd == "panview" && view) { float x = 0, y = 0; sscanf(arg.c_str(), "%f %f", &x, &y); view->camera().panX = x; view->camera().panY = y; return true; }
	if (cmd == "lang") { i18n::language = arg == "ja" ? 1 : 0; return true; }   // UI language for the shot (not saved)
	if (cmd == "shortcut") {
		for (int a = 0; a < (int)ShortcutAction::count; a++)
			if (arg == ShortcutRegistry::ActionId((ShortcutAction)a)) { RunShortcut((ShortcutAction)a); return true; }
		if (msg) *msg = "unknown action " + arg;
		return true;
	}
	if (cmd == "patsel" && view && ch) {   // patsel <set> [prop] [cutout]
		int set = -1, prop = -1, cut = -1;
		sscanf(arg.c_str(), "%d %d %d", &set, &prop, &cut);
		auto& st = view->getState();
		if (set >= 0) PatNavigate(view, set - st.partSet, 0);
		if (prop >= 0) PatNavigate(view, 0, prop - st.partProp);
		if (cut >= 0) st.partCutOut = cut;
		return true;
	}
	if (cmd == "patmode" && view) {
		const RenderMode m = arg == "uv" ? RenderMode::UV_SETTING_VIEW : arg == "texture" ? RenderMode::TEXTURE_VIEW : RenderMode::DEFAULT;
		if (view->getState().renderMode != m) TogglePatRenderMode(view, m == RenderMode::DEFAULT ? view->getState().renderMode : m);
		return true;
	}
	if (cmd == "uvdrag" && view) {   // uvdrag x0 y0 x1 y1 (world)
		float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		sscanf(arg.c_str(), "%f %f %f %f", &x0, &y0, &x1, &y1);
		BeginUvDrag(view, x0, y0);
		UvDragBy(x1 - x0, y1 - y0);
		EndUvDrag(false);
		return true;
	}
	if (cmd == "patinfo" && msg) {   // one line for the script log
		char b[512];
		if (!view || !ch) snprintf(b, sizeof b, "patinfo: no view");
		else {
			const auto& st = view->getState();
			const auto* co = ch->parts.GetCutOut(st.partCutOut);
			const auto* pr = ch->parts.GetPartProp(st.partSet, st.partProp);
			snprintf(b, sizeof b, "patinfo: doc=%d modified=%d undo=%zu redo=%zu set=%d prop=%d cut=%d uv=%d,%d,%d,%d prop.xy=%d,%d highlight=%d sets=%zu path=%s",
				ch->isPatDocument() ? 1 : 0, ch->isModified() ? 1 : 0, ch->undoManager.undoCount(), ch->undoManager.redoCount(),
				st.partSet, st.partProp, st.partCutOut, co ? co->uv[0] : -1, co ? co->uv[1] : -1, co ? co->uv[2] : -1, co ? co->uv[3] : -1,
				pr ? pr->x : 0, pr ? pr->y : 0, ch->parts.partHighlight, ch->parts.partSets.size(), ch->getPATPath().c_str());
		}
		*msg = b;
		return true;
	}
	return false;
}

#endif /* UI_PAT_EDITOR_IMPL_H_GUARD */
