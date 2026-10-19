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

#include "ui_style.h"
#include "../bof_authoring_contract.h"
#include "../mbaacc_typed_hitgrab.h"
#include "../parts/pat_renumber.h"
#include "../app_log.h"
#include "../png_writer.h"

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

static std::string AcpToUtf8Rn(const std::string& s)
{
	if (s.empty()) return s;
	const int n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, 0); MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), &w[0], n);
	return WideToUtf8(w);
}

static std::string FileNameOf(const std::string& p)
{
	const size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

static std::unique_ptr<CharacterInstance> MakePatPlaceholder(const std::string& name)
{
	return MakePatPlaceholderCharacter(name);   // shared with project restore (project_manager.cpp)
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

bool MainFrame::createPatEditorView(const std::string& patPath)
{
	// Standalone PAT entry points share this dispatcher. File extensions cannot
	// distinguish native MBAACC/UNI/MBTL parts from GOF1/GOF2/HAN2 parts.
	// Dialog/--open paths use ACP; browser/drop/script paths use UTF-8. Keep
	// the native Parts loader's ACP contract while sniffing either entry form.
	std::string nativePath = patPath, displayPath = AcpToUtf8Rn(patPath);
	std::ifstream file(patPath, std::ios::binary);
	if (file && displayPath != patPath) {
		bool ambiguous = false;
		try { ambiguous = std::filesystem::exists(std::filesystem::u8path(patPath)); } catch (const std::exception&) {}
		if (ambiguous) {
			requestErrorPopup("Loading Error", TXT("Ambiguous PAT filename: ANSI and UTF-8 interpretations name different existing files. Rename the file to disambiguate."));
			return false;
		}
	}
	if (!file) {
		file.clear();
		try { file.open(std::filesystem::u8path(patPath), std::ios::binary); } catch (const std::exception&) {}
		nativePath = Utf8ToAcp(patPath);
		displayPath = patPath;
	}
	std::vector<uint8_t> head(0x24);
	file.read((char*)head.data(), (std::streamsize)head.size());
	head.resize((size_t)file.gcount());
	const bool native = head.size() >= 12 && memcmp(head.data(), "PAniDataFile", 12) == 0;
	if (!native) {
		file.clear(); file.seekg(0);
		std::vector<uint8_t> all((std::istreambuf_iterator<char>(file)), {});
		if (han2::IsPat(all.data(), all.size())) {
			const size_t before = views.size();
			openHan2File(displayPath); // Keep the existing HAN2 conversion/editing workflow.
			const bool opened = views.size() > before;
			if (opened) addRecentFile(displayPath);
			return opened;
		}
		requestErrorPopup("Loading Error", std::string(TXT("Unrecognized PAT: expected PAniDataFile (MBAACC/UNI/MBTL) or HAN2/GOF parts.\n")) + patPath);
		return false;
	}
	auto character = MakePatPlaceholder(FileNameOf(displayPath));
	if (!character->loadPAT(nativePath)) {
		requestErrorPopup("Loading Error", std::string(TXT("Could not read native PAT (PAniDataFile):\n")) + patPath);
		return false;
	}
	character->makePatDocument();
	addPatDocumentView(std::move(character));
	addRecentFile(displayPath);
	return true;
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
	// Part set renumbering (pat_renumber.h): the .pat and every HA6 that draws it are written together.
	const int rnExtent = 0;
	std::vector<int> rnRemap = patrn::PendingDiskRemap(character->parts, rnExtent);
	patrn::Users rnUsers;
	const bool renumbered = !patrn::IsIdentity(rnRemap);
	if (renumbered) {
		if (normalizePath(target) != normalizePath(character->getPATPath().empty() ? character->parts.filePath : character->getPATPath())) {
			requestErrorPopup("Save Error", TXT("The part sets were renumbered: save the .pat to its own file (Save, not Save As), so the HA6 files that draw it are rewritten with it."));
			return false;
		}
		const std::string u8 = AcpToUtf8Rn(target);
		rnUsers = patrn::FindUsers(u8);
		rnRemap = patrn::PendingDiskRemap(character->parts, patrn::MaxRef(rnUsers) + 1);
		const patrn::Plan plan = patrn::MakePlan(rnUsers, rnRemap);
		std::string msg;
		for (auto& r : plan.refusals) msg += r + "\n";
		// an HA6 that is open in another tab would be saved later with the old ids
		auto openIn = [&](CharacterInstance* c) {
			if (!c || c == character) return;
			for (auto& h : c->getHA6Paths()) for (auto& f : rnUsers.files)
				if (!f.shared && normalizePath(AcpToUtf8Rn(h)) == normalizePath(f.path)) msg += f.path + TXT(" is open in a tab: close it first (its frames would be saved with the old part set ids)") + std::string("\n");
		};
		for (auto& c : characters) { openIn(c.get()); openIn(c->effectCharacter.get()); }   // effect.ha6 draws sys_effect.pat
		if (!msg.empty()) {
			requestErrorPopup("Save Error", std::string(TXT("Not saved: the renumbered part sets cannot be written safely:")) + "\n" + msg);
			return false;
		}
	}
	backupBeforeSave(target, true);
	if (!character->savePATAs(target)) {
		requestErrorPopup("Save Error", std::string(TXT("Could not write the .pat (it may be locked by another program, or the PAT has no data yet): ")) + target);
		return false;
	}
	if (renumbered) {
		struct Ctx { MainFrame* self; } ctx{this};
		const patrn::WriteResult w = patrn::WriteUsers(rnUsers, rnRemap,
			[](const std::string& u8, void* c) { static_cast<Ctx*>(c)->self->backupBeforeSave(Utf8ToAcp(u8), false); }, &ctx);
		std::string rep = std::to_string(w.refs) + TXT(" part set references rewritten in:") + "\n";
		for (auto& f : w.written) rep += "  " + f + "\n";
		for (auto& e : w.errors) rep += std::string(TXT("Error: ")) + e + "\n";
		HC_LOG_INFO("[PatRenumber] saved {}: {} references in {} files, {} errors", target, w.refs, w.written.size(), w.errors.size());
		requestErrorPopup(w.errors.empty() ? "Notice" : "Save Error", rep);
	}
	if (character->isPatDocument()) character->setName(FileNameOf(target));
	{   // a working copy of an archive entry (archive browser): write it back into that archive like a character entry.
		// UNI2 d\ and MBTL archives are read-only (no origin): the copy is kept, and onFileSaved below covers the mod folder.
		std::string err; bool had = false;
		if (!abrowser::SaveEntryBytesIntoArchive(target, &err, &had) && had) {
			requestErrorPopup("Save Error", std::string(TXT("Saved the working copy, but could not write back into the archive: ")) + err);
			return false;
		}
	}
	reloadPatInOtherTabs(character, target);
	updateWindowTitle();
	onFileSaved(character, AcpToUtf8Rn(target));   // UNI2 / MBTL: IM mod folder + fpan-im hot reload
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

// Edit > Preferences: Auto backup, PAT editor, Mouse buttons, 3D stage: Blender (menus), and the same settings in one window.
void MainFrame::DrawBackupPrefsItems()
{
	{
		bool changed = false;
		changed |= ImGui::Checkbox(LBL("Back up character files (HA6, ...) before saving"), &gSettings.backupHa6);
		changed |= ImGui::Checkbox(LBL("Back up .pat files before saving"), &gSettings.backupPat);
		ImGui::SetNextItemWidth(ui::Px(120));
		if (ImGui::InputInt(LBL("Backups kept per file (0 = all)"), &gSettings.backupKeep)) {
			gSettings.backupKeep = std::clamp(gSettings.backupKeep, 0, 999);
			changed = true;
		}
		char folder[512];
		snprintf(folder, sizeof folder, "%s", gSettings.backupFolder.c_str());
		ImGui::SetNextItemWidth(ui::Px(260));
		if (ImGui::InputTextWithHint(LBL("Backup folder"), "backup", folder, sizeof folder)) {
			gSettings.backupFolder = folder;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(LBL("Browse..."))) {
			const std::string picked = BrowseForFolderUtf8(AnsiToUtf8(gSettings.backupFolder));
			if (!picked.empty()) { gSettings.backupFolder = Utf8ToAcp(picked); changed = true; }
		}
		// help in a (?) tooltip instead of two full-width lines
		ui::HelpMarker((std::string(TXT("Empty: a \"backup\" folder next to the file. A relative folder is relative to the file.")) + "\n"
			+ TXT("Every save (menu, Ctrl+S, close prompt) copies the old file to <name>.<date-time>-NN.bak first.")).c_str());
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
		ImGui::SetNextItemWidth(ui::Px(120));
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
		ImGui::SetNextItemWidth(ui::Px(120));
		bool changed = ImGui::Combo(LBL("Pan the view"), &pan, buttons, 3);
		ImGui::SetNextItemWidth(ui::Px(120));
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
		ui::HelpMarker(TXT("The middle button always pans when it does not draw."));
	}
}

// Edit > Preferences > Timeline and keys: opt-in behavior; every default is the 0.1.2 behavior.
void MainFrame::DrawTimelineKeysPrefsItems()
{
	bool changed = false, keys = false;
	changed |= ImGui::Checkbox(LBL("New views scrub absolute (authored) time"), &gSettings.absoluteScrubDefault);
	ui::HelpMarker(TXT("Off: the timeline scrubs the simulated flow (jumps, loops). Each view can still switch with its Simulated scrub box."));
	keys |= ImGui::Checkbox(LBL("J/L step one tick (hold repeats), Shift+J/L play"), &gSettings.jlFrameStep);
	keys |= ImGui::Checkbox(LBL("PgUp/PgDn step states in a character view"), &gSettings.pageKeysStepStates);
	changed |= ImGui::Checkbox(LBL("Pattern list in its own window"), &gSettings.patternListWindow);
	ui::HelpMarker(TXT("Off: the pattern list fills the free space of the pattern pane. On: View > Pattern list toggles a separate window."));
	changed |= ImGui::Checkbox(LBL("Timeline Duration row"), &gSettings.timelineDurationLane);
	if (keys) {
		shortcuts.registry().applyLayoutOptions(gSettings.jlFrameStep, gSettings.pageKeysStepStates);
		gSettings.keyBindings = shortcuts.registry().serializeOverrides();
		m_scrubHeldDir = 0;
	}
	if (changed || keys) ImGui::MarkIniSettingsDirty();
}

void MainFrame::DrawPatPreferences()
{
	if (ImGui::BeginMenu(LBL("Timeline and keys"))) { DrawTimelineKeysPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("Auto backup"))) { DrawBackupPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("PAT editor"))) { DrawPatColourPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("Mouse buttons"))) { DrawMousePrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("3D stage: Blender"))) { stage3dview::BlenderPrefsItems(); ImGui::EndMenu(); }
	if (ImGui::BeginMenu(LBL("UNI2/MBTL: mod folder and hot reload"))) { DrawHotSavePrefsItems(); ImGui::EndMenu(); }
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
	ImGui::SeparatorText(TXT("Timeline and keys"));
	DrawTimelineKeysPrefsItems();
	ImGui::SeparatorText(TXT("Auto backup"));
	DrawBackupPrefsItems();
	ImGui::SeparatorText(TXT("PAT editor"));
	DrawPatColourPrefsItems();
	ImGui::SeparatorText(TXT("Mouse buttons"));
	DrawMousePrefsItems();
	ImGui::SeparatorText(TXT("3D stage: Blender"));
	stage3dview::BlenderPrefsItems();
	ImGui::SeparatorText(TXT("UNI2/MBTL: mod folder and hot reload"));
	DrawHotSavePrefsItems();
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
	ImGui::SetNextItemWidth(ui::Px(90));
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
bool MainFrame::PatScriptCommand(const std::string& command, const std::string& arg, std::string* msg)
{
 const bool detached=gStartup.headless && command.rfind("detached:",0)==0;
 const std::string cmd=detached?command.substr(9):command;
 CharacterView* view=getActiveView();
 if(detached) {
  const auto* host=m_hosts.empty()?nullptr:m_session.host(m_hosts.begin()->first);
  view=host?findViewById(host->active):nullptr;
 }
	CharacterInstance* ch = view ? view->getCharacter() : nullptr;
	if(cmd=="motiontoggle" && view && gStartup.headless) { view->getState().previewOptions.simulateMovement=arg=="on"; view->getState().forceSpawnTreeRebuild=true; return true; }
 if(cmd=="motionfacing" && view && gStartup.headless) { view->getState().previewOptions.rootFacingLeft=arg=="left"; view->getState().forceSpawnTreeRebuild=true; return true; }
 if(cmd=="motioninspect" && view && ch && gStartup.headless) {
  auto& st=view->getState(); preview::TickState ts;
  st.BindPreviewSim(&ch->frameData,ch->effectCharacter?&ch->effectCharacter->frameData:nullptr).getStateAt(st.PreviewTick(&ch->frameData),ts);
  float child=0; bool found=false; for(const auto& a:ts.actors) if(!a.isRoot && a.pattern==412) { child=a.x; found=true; break; }
  if(msg) *msg="motioninspect: root="+std::to_string(ts.root()?ts.root()->x:0)+" child="+std::to_string(child)+" found="+std::to_string(found);
  return true;
 }
 if(cmd=="tabfixture" && view && gStartup.headless) {
  while(views.size()<3) {
   auto v=std::make_unique<CharacterView>(ch,&render); v->setViewNumber(int(views.size()+1));
   views.push_back(std::move(v));
  }
  SyncWorkspaceSession();
  if(arg=="detached") {
   auto ids=m_session.host(WorkspaceSession::MainHost)->tabs;
   auto vp=ImGui::GetMainViewport()->Pos; auto id=DetachViewToNewHost(ids[0],ImVec2(vp.x+140,vp.y+100));
   for(size_t i=1;i<ids.size();++i) MoveViewToHost(ids[i],id,std::nullopt);
  }
  return true;
 }
 if(cmd=="tabinput" && gStartup.headless) {
  const auto hostId=m_hosts.empty()?WorkspaceSession::MainHost:m_hosts.begin()->first;
  const auto* host=m_session.host(hostId); if(!host || host->tabs.size()!=3) return false;
  static ImVec2 anchor{},target{};
  if(arg=="hover" || arg=="down" || arg=="downlast") {
   const auto from=m_tabUi[host->tabs.front()],to=m_tabUi[host->tabs.back()];
   anchor=ImVec2((from.min.x+from.max.x)*.5f,(from.min.y+from.max.y)*.5f);
   target=ImVec2(to.min.x+(to.max.x-to.min.x)*(arg=="downlast"?.75f:.25f),(to.min.y+to.max.y)*.5f);
  }
  auto& io=ImGui::GetIO();
  io.AddMousePosEvent(arg=="hover" || arg=="down" || arg=="downlast"?anchor.x:target.x,arg=="hover" || arg=="down" || arg=="downlast"?anchor.y:target.y);
  if(arg=="down" || arg=="downlast" || arg=="up") io.AddMouseButtonEvent(0,arg!="up");
  return true;
 }
 if(cmd=="tabmoveprobe" && gStartup.headless) {
  const auto id=m_hosts.empty()?WorkspaceSession::MainHost:m_hosts.begin()->first;
  const auto* host=m_session.host(id); if(!host || host->tabs.size()!=3) return false;
  MoveViewToHost(host->tabs.front(),id,2); return true;
 }
 if(cmd=="tabinspect" && gStartup.headless) {
  const auto id=m_hosts.empty()?WorkspaceSession::MainHost:m_hosts.begin()->first;
  const auto* host=m_session.host(id); if(!host) return false;
  if(msg) { *msg="tabinspect: match="+std::to_string(m_tabOrderMatches[id])+" order=";
   for(auto id:host->tabs) *msg+=std::to_string(id)+","; }
  return true;
 }
 if(cmd=="transferprepare" && view && ch && gStartup.headless) {
  OpenStateTransfer(view); auto& q=m_stateTransfer.request;
  q.sourcePattern=149;q.sourceFirst=q.sourceLast=0;q.targetPattern=63;
  q.targetFirst=0;q.targetLast=2;q.groups=statetransfer::Duration|statetransfer::Animation;
  PreviewStateTransfer();
  if(msg) *msg="transferprepare: ready="+std::to_string(m_stateTransfer.ready)+" changes="+std::to_string(m_stateTransfer.plan.changed);
  return true;
 }
 if(cmd=="transferverify" && view && ch && gStartup.headless) {
  auto& w=m_stateTransfer; auto& fd=ch->frameData;
  auto* win=ImGui::FindWindowByName("State property transfer");
  const auto* surface=FindSurface(view);
  const bool owner=w.view==view->getId() && win && surface && win->Viewport->ID==surface->viewport;
  const size_t before=ch->undoManager.undoCount();
  const bool applied=ApplyStateTransfer() && ch->undoManager.undoCount()==before+1 && ch->isModified();
  bool after=true;for(const auto& r:w.plan.rows) after &= statetransfer::Equal(fd.m_sequences[w.plan.request.targetPattern].frames[r.target],r.after);
  PerformUndoRedo(view,false);
  bool undone=true;for(const auto& r:w.plan.rows) undone &= statetransfer::Equal(fd.m_sequences[w.plan.request.targetPattern].frames[r.target],r.before);
  PerformUndoRedo(view,true);
  bool redone=true;for(const auto& r:w.plan.rows) redone &= statetransfer::Equal(fd.m_sequences[w.plan.request.targetPattern].frames[r.target],r.after);
  PreviewStateTransfer();fd.mark_modified(w.request.sourcePattern);
  const size_t count=ch->undoManager.undoCount();const bool stale=!ApplyStateTransfer() && ch->undoManager.undoCount()==count;
  if(msg) *msg="transferverify: owner="+std::to_string(owner)+" apply="+std::to_string(applied && after)+" undo="+std::to_string(undone)+" redo="+std::to_string(redone)+" stale="+std::to_string(stale);
  return true;
 }
 if(cmd=="pattransformfixture" && view && ch && gStartup.headless) {
  // In-memory owner only: preserve the real PAT173 animation and its resources.
  auto& st=view->getState(); auto* owner=ch->frameData.get_sequence(0);
  if(!owner || owner->frames.empty() || !ch->frameData.get_sequence(173)) return false;
  Frame f=owner->frames[0]; f.EF.clear(); f.IF.clear(); f.AF.duration=120; f.AF.aniType=3;
  for(auto& l:f.AF.layers) {l.blend_mode=1; l.rgba[3]=0;}
  Frame_EF e{}; e.type=1; e.number=173; e.parameters[2]=64|(arg=="left"?2048:0);
  f.EF.push_back(e); owner->frames={f}; ch->frameData.mark_modified(0);
  ch->undoManager.markModified(); ch->undoManager.flush();
  st.pattern=0; st.InspectState(&ch->frameData,0); st.vizSettings.showSpawnedPatterns=true;
  return true;
 }
 if(cmd=="patpickfixture" && view && ch && gStartup.headless) {
  auto& parts=ch->parts; if(!parts.loaded || parts.textures.size()!=parts.gfxMeta.size()) return false;
  const int ti=int(parts.textures.size()),ci=int(parts.cutOuts.size()),pi=int(parts.partSets.size());
  std::vector<char> rgba(8*8*4,0); for(int y=0;y<8;++y) for(int x=4;x<8;++x) {
   for(int c=0;c<4;++c) rgba[(y*8+x)*4+c]=char(255);
  }
  auto* tex=new Texture; tex->LoadDirect(rgba.data(),8,8);
  if(arg.find("compressed")!=std::string::npos) {
   // Two-by-two BC1 blocks: left transparent, right opaque. Also exercises
   // replacing a raw CPU image before the old source allocation is released.
   unsigned char blocks[32]{};
   for(int i=0;i<4;++i) { blocks[i*8+2]=blocks[i*8+3]=255; for(int j=4;j<8;++j) blocks[i*8+j]=i%2?0x55:0xff; }
   tex->LoadCompressed(reinterpret_cast<char*>(blocks),8,8,sizeof(blocks),1);
  } else tex->Apply(false,false);
  parts.textures.push_back(tex); PartGfx<> gfx; gfx.id=ti; gfx.w=gfx.h=8; gfx.textureIndex=tex->id; parts.gfxMeta.push_back(gfx);
  CutOut<> cut; cut.id=ci; cut.texture=ti; cut.uv[2]=cut.uv[3]=256; cut.wh[0]=cut.wh[1]=64; cut.xy[0]=cut.xy[1]=32; cut.shapeIndex=-1; parts.cutOuts.push_back(cut);
  PartSet<> ps; ps.partId=pi; PartProperty prop; prop.ppId=ci; prop.propId=0; prop.flip=arg.find("flip")!=std::string::npos?1:0;
  prop.rotation[1]=.07f; prop.rotation[2]=.11f; ps.groups.push_back(prop); parts.partSets.push_back(ps);
  auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); if(!seq || seq->frames.empty()) return false;
  Frame f=seq->frames[0]; f.EF.clear(); f.IF.clear(); f.AF.duration=120; f.AF.aniType=3;
  f.AF.layers.resize(2); for(auto& l:f.AF.layers) {
   l.usePat=true; l.spriteId=pi; l.offset_x=l.offset_y=0; l.scale[0]=l.scale[1]=1;
   l.rotation[0]=l.rotation[1]=0; l.rotation[2]=.04f; for(auto& c:l.rgba) c=1;
  }
  seq->frames={f}; ch->frameData.mark_modified(st.pattern); ch->undoManager.markModified(); ch->undoManager.flush();
  st.InspectState(&ch->frameData,0); view->getBoxPane()->positionTool=true; view->camera().panX=view->camera().panY=0;
  return true;
 }
 if(cmd=="patpicktest" && view && ch && gStartup.headless) {
  const auto& footprints=SurfaceFor(view).footprints;
  ImVec2 covered{},empty{}; bool gotCovered=false,gotEmpty=false;
  int triangles=0;
  for(const auto& p:footprints) if(p.kind==0 && p.index==0 && p.triangle && p.mask) {
   ++triangles;
   float minx=p.quad[0].x,maxx=minx,miny=p.quad[0].y,maxy=miny;
   for(int j=1;j<3;++j) { minx=std::min(minx,p.quad[j].x); maxx=std::max(maxx,p.quad[j].x); miny=std::min(miny,p.quad[j].y); maxy=std::max(maxy,p.quad[j].y); }
   for(int y=int(miny)+2;y<int(maxy)-2;y+=2) for(int x=int(minx)+2;x<int(maxx)-2;x+=2) {
    if(!spritepick::TriangleHit(p.quad,p.uv,p.invW,*p.mask,float(x),float(y),true)) continue;
    if(p.hit(float(x),float(y))) { if(!gotCovered) { covered=ImVec2(float(x),float(y)); gotCovered=true; } }
    else if(!PositionSpriteHit(0,0,float(x),float(y),view)) { empty=ImVec2(float(x),float(y)); gotEmpty=true; }
   }
  }
  const auto targets=CollectPositionTargets(view);
  bool transparent=false,cycle=false,move=false,rotate=false;
  if(gotEmpty) {
   // Stay away from explicit handles, which intentionally ignore texel coverage.
   bool nearHandle=false; for(const auto& t:targets) if(std::hypot(t.screen.x-empty.x,t.screen.y-empty.y)<10) nearHandle=true;
   transparent=!nearHandle && PositionHits(targets,empty.x,empty.y,true,view).empty();
  }
  if(gotCovered) {
   const auto hits=PositionHits(targets,covered.x,covered.y,true,view);
   cycle=hits.size()==2 && hits.front().index==1;
   if(cycle) {
    PickPosition(targets,covered.x,covered.y,true,0,view);
    const auto first=view->getState().positionSelection.selected;
    PickPosition(targets,covered.x,covered.y,true,1,view); cycle &= !(first==view->getState().positionSelection.selected);
    BYTE saved[256],keys[256]; GetKeyboardState(saved); memcpy(keys,saved,sizeof(keys)); keys[VK_CONTROL]|=0x80; SetKeyboardState(keys);
    BeginPositionDrag(view,int(covered.x),int(covered.y)); move=m_posDrag.active && m_posDrag.viewId==view->getId();
    const int index=m_posDrag.target.index;
    if(move) {
     const int original=ch->frameData.get_sequence(view->getState().pattern)->frames[0].AF.layers[index].offset_x;
     PositionDragBy(30,15); EndPositionDrag(false);
     move &= ch->frameData.get_sequence(view->getState().pattern)->frames[0].AF.layers[index].offset_x!=original;
     ch->undoManager.undo(); move &= ch->frameData.get_sequence(view->getState().pattern)->frames[0].AF.layers[index].offset_x==original;
    }
    rotate=BeginPositionRotation(int(covered.x),int(covered.y),view);
    if(rotate) {
     const auto pivot=m_posDrag.target.screen;
     PositionDragBy(int(-(covered.y-pivot.y)-(covered.x-pivot.x)),int((covered.x-pivot.x)-(covered.y-pivot.y)));
     rotate=std::fabs(m_posDrag.angleDelta)>.1f; EndPositionDrag(true);
    }
    SetKeyboardState(saved);
   }
  }
  if(msg) *msg="patpicktest: triangles="+std::to_string(triangles)+" covered="+std::to_string(gotCovered)+" empty="+std::to_string(transparent)+" cycle="+std::to_string(cycle)+" move="+std::to_string(move)+" rotation="+std::to_string(rotate);
  return true;
 }
 if(cmd=="transportcheck" && gStartup.headless && view && ch) {
  auto& st=view->getState();auto* seq=ch->frameData.get_sequence(st.pattern);
  if(!seq || seq->frames.size()<3) return false;
  st.InspectState(&ch->frameData,1,std::min(1,std::max(0,seq->frames[1].AF.duration-1)));
  const int visible=st.AuthoredTick(&ch->frameData);StepTick(view,1);
  bool ok=st.AuthoredTick(&ch->frameData)==visible+1;
  StepTick(view,-1);ok &= st.AuthoredTick(&ch->frameData)==visible;
  view->getBoxPane()->AdvanceBox(3);
  const int box=view->getBoxPane()->SelectedBoxId();AdvanceFrame(view,1);AdvanceFrame(view,-1);
  ok &= box==view->getBoxPane()->SelectedBoxId();
  st.InspectState(&ch->frameData,1,0);RunShortcut(ShortcutAction::togglePlayback);AdvanceViewPlayback(view);
  auto& sim=st.BindPreviewSim(&ch->frameData,ch->effectCharacter?&ch->effectCharacter->frameData:nullptr);
  preview::TickState sample;sim.getStateAt(st.currentTick,sample);
  ok &= sample.root() && (sample.root()->frame==1 || seq->frames[1].AF.duration==1);
  StopTransport(view);
  if(msg) *msg=std::string("transportcheck: ")+(ok?"PASS":"FAIL")+" tick="+std::to_string(st.currentTick)+" state="+std::to_string(st.frame)+" box="+std::to_string(box);
  return true;
 }
 if(cmd=="compactui" && gStartup.headless) {
  if(arg=="list") {gSettings.patternListWindow=true;gSettings.showPatternList=!gSettings.showPatternList;}
  if(arg=="wide") {
   LoadTheme(1);
   SetWindowPos(WindowFromDC(context->dc), nullptr, 0, 0, 1920, 1080, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
  }
  if(arg=="panes") {
   for(auto* w:ImGui::GetCurrentContext()->Windows)
    if(!strcmp(w->Name,"Left Pane") || !strcmp(w->Name,"Right Pane")) {
     auto* node = ImGui::DockBuilderGetNode(w->DockId);
     while(node && node->ParentNode && node->ParentNode->SplitAxis==ImGuiAxis_Y) node=node->ParentNode;
     if(node) ImGui::DockBuilderSetNodeSize(node->ID, ImVec2(!strcmp(w->Name,"Right Pane")?700:620, node->Size.y));
    }
  }
  if(arg=="timelinewide") for(auto* w:ImGui::GetCurrentContext()->Windows) if(!strcmp(w->Name,"Box Pane")) {
   ImGui::SetWindowDock(w,0,ImGuiCond_Always);
   ImGui::SetWindowPos(w,ImVec2(300,60),ImGuiCond_Always);
   ImGui::SetWindowSize(w,ImVec2(1100,940),ImGuiCond_Always);
  }
  if(arg=="0" || arg=="1") { gSettings.compactUi=arg=="1"; ImGui::MarkIniSettingsDirty(); }
  if(arg=="grabfixture" && ch && view) {auto* seq=ch->frameData.get_sequence(view->getState().pattern);if(seq && view->getState().frame>=0 && view->getState().frame<(int)seq->frames.size()) seq->frames[view->getState().frame].AT.hitgrab=1;}
  if(msg) *msg="compactui: enabled="+std::to_string(gSettings.compactUi)+" version="+std::to_string(ch?ch->frameData.dataVersion:0);
  for(auto* w:ImGui::GetCurrentContext()->Windows) {
   if(strstr(w->Name,"FrameInfo") || strstr(w->Name,"Right Pane"))
    if(msg) *msg+=" ["+std::string(w->Name)+" content="+std::to_string(w->ContentSize.y)+" width="+std::to_string(w->Size.x)+"]";
  }
  return true;
 }
 if(cmd=="panelscroll" && view && gStartup.headless) {
  extern int gUiScriptOpenFrameNodes; gUiScriptOpenFrameNodes=7;
  const auto owner=m_session.owner(view->getId()).value_or(WorkspaceSession::MainHost);
  const auto name=owner==WorkspaceSession::MainHost?std::string("Right Pane"):"Right Pane###host"+std::to_string(owner)+"_Right Pane";
  auto* w=ImGui::FindWindowByName(name.c_str()); if(!w) return false;
  ImGui::GetIO().AddMousePosEvent(w->Pos.x+w->Size.x*.5f,w->Pos.y+w->Size.y*.5f);
  if(arg=="wheel") ImGui::GetIO().AddMouseWheelEvent(0,-3);
  if(msg) *msg="panelscroll: y="+std::to_string(w->Scroll.y)+" max="+std::to_string(w->ScrollMax.y)+" target="+std::to_string(w->ScrollTarget.y);
  return true;
 }
 if(cmd=="diagnosticpattern" && view && gStartup.headless) { view->getState().pattern=atoi(arg.c_str()); view->getState().InspectState(&ch->frameData,0); return true; }
	if(cmd=="diagnosticsaveini" && gStartup.headless && !arg.empty()) { ImGui::SaveIniSettingsToDisk(arg.c_str()); return true; }
	if(cmd=="grabpooltest" && view && gStartup.headless) {
  RefreshOpponentRoster(view);
  auto& a=view->getState(); if(a.opponentRoster.size()<2) return false;
  FrameState b; b.opponentPoolInitialized=true; b.opponentCyclePool=a.opponentRoster;
  auto& shared=OpponentCyclePool(a); shared={a.opponentRoster[0]};
  bool global=&OpponentCyclePool(a)==&OpponentCyclePool(b) && OpponentCyclePool(b).size()==1;
  b.opponentSeparatePool=true; b.opponentCyclePool={a.opponentRoster[1]};
  bool separate=OpponentCyclePool(b).front()!=OpponentCyclePool(a).front();
  b.opponentSeparatePool=false;
  bool rejoin=&OpponentCyclePool(a)==&OpponentCyclePool(b);
  a.opponentSeparatePool=true; a.opponentCyclePool={a.opponentRoster[1]};
  ImGui::MarkIniSettingsDirty();
  if(msg) *msg="grabpooltest: shared="+std::to_string(global)+" separate="+std::to_string(separate)+" rejoin="+std::to_string(rejoin);
  return true;
 }
 if(cmd=="grabpoolinspect" && view && gStartup.headless) {
  RefreshOpponentRoster(view); auto& st=view->getState();
  if(msg) *msg="grabpoolinspect: global="+std::to_string(gSettings.grabCyclePool.size())+" separate="+std::to_string(st.opponentSeparatePool)+" effective="+std::to_string(OpponentCyclePool(st).size());
  return true;
 }
 if(cmd=="detachedtoolfixture" && view && ch && gStartup.headless) {
  auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern);
  if(!seq || seq->frames.empty() || seq->frames[0].AF.layers.empty()) return false;
  Frame f=seq->frames[0]; f.EF.clear(); f.IF.clear(); f.AF.layers.resize(1);
  f.AF.layers[0].offset_x=f.AF.layers[0].offset_y=0; f.AF.layers[0].rotation[2]=0;
  f.AF.duration=120; f.AF.aniType=3; seq->frames={f};
  ch->frameData.mark_modified(st.pattern); ch->undoManager.markModified(); ch->undoManager.flush();
  st.InspectState(&ch->frameData,0); view->getBoxPane()->positionTool=true;
  view->camera().panX=view->camera().panY=0;
  return true;
 }
 if(cmd=="detachedtoolinput" && gStartup.headless && !m_hosts.empty()) {
  const auto* host=m_session.host(m_hosts.begin()->first); auto* v=host?findViewById(host->active):nullptr;
  if(!v) return false;
  auto& io=ImGui::GetIO(); auto& st=v->getState();
  if(arg=="play" || arg=="stop") { st.animating=arg=="play"; return true; }
  if(arg=="undo") return PerformUndoRedo(v,false);
  const auto targets=CollectPositionTargets(v); if(targets.empty()) return false;
  static ImVec2 anchor;
  const auto& surface=SurfaceFor(v);
  if(arg=="hover" || arg=="down" || arg=="rdown") anchor=ImVec2(surface.desktopOrigin.x+targets[0].screen.x,surface.desktopOrigin.y+targets[0].screen.y);
  ImVec2 point=anchor;
  if(arg=="move") { point.x+=30; point.y+=20; }
  if(arg=="rmove1") point.x+=30;
  if(arg=="rmove2" || arg=="rup") point.y+=30;
  if(arg=="up") { point.x+=30; point.y+=20; }
  io.AddMousePosEvent(point.x,point.y);
  if(arg=="down" || arg=="up") io.AddMouseButtonEvent(ImGuiMouseButton_Left,arg=="down");
  if(arg=="rdown" || arg=="rup") io.AddMouseButtonEvent(ImGuiMouseButton_Right,arg=="rdown");
  return true;
 }
 if(cmd=="detachedtoolinspect" && gStartup.headless && !m_hosts.empty()) {
  const auto* host=m_session.host(m_hosts.begin()->first); auto* v=host?findViewById(host->active):nullptr; if(!v) return false;
  auto& st=v->getState(); auto* seq=v->getCharacter()->frameData.get_sequence(st.pattern);
  if(!seq || seq->frames.empty()) return false;
  const auto& layer=seq->frames[0].AF.layers[0];
  if(msg) *msg="detachedtoolinspect: x="+std::to_string(layer.offset_x)+" y="+std::to_string(layer.offset_y)+" rotation="+std::to_string(layer.rotation[2])+" tick="+std::to_string(st.currentTick)+" footprints="+std::to_string(SurfaceFor(v).footprints.size())+" active="+std::to_string(m_posDrag.active);
  return true;
 }
	if(cmd=="detachedsurfacefixture" && view && gStartup.headless) {
		if(arg=="keepmain") {
   auto other=std::make_unique<CharacterView>(ch,&render);
   other->getState().pattern=view->getState().pattern;
   other->getState().InspectState(&ch->frameData,0);
   other->getBoxPane()->positionTool=true;
   other->camera().panX=-100; other->camera().panY=0;
   views.push_back(std::move(other));
   SyncWorkspaceSession();
  }
  const auto p=ImGui::GetMainViewport()->Pos;
		const auto id=DetachViewToNewHost(view->getId(),ImVec2(p.x+140,p.y+100));
		if(!id) return false;
		auto& host=m_hosts[id]; host.w=900; host.h=650; host.showPanes=true; host.applyGeometry=true; m_focusHostRequest=id; return true;
	}
	if(cmd=="detachedsurfaceinput" && gStartup.headless && !m_hosts.empty()) {
		const auto id=m_hosts.begin()->first;
		const auto dock=ImHashStr(("hantei_host_dock_"+std::to_string(id)).c_str());
		auto* central=ImGui::DockBuilderGetCentralNode(dock); if(!central) return false;
		const bool move=arg=="panmove" || arg=="boxmove";
		ImGui::GetIO().AddMousePosEvent(central->Pos.x+central->Size.x*.5f+(move?30:0),central->Pos.y+central->Size.y*.5f+(move?20:0));
		if(arg=="wheel") ImGui::GetIO().AddMouseWheelEvent(0,1);
		if(arg=="pandown" || arg=="panup") ImGui::GetIO().AddMouseButtonEvent(gSettings.mousePanButton,arg=="pandown");
		if(arg=="boxdown" || arg=="boxup") ImGui::GetIO().AddMouseButtonEvent(gSettings.mouseDrawButton,arg=="boxdown");
		return true;
	}
	if(cmd=="detachedsurfaceundo" && gStartup.headless && !m_hosts.empty()) {
		const auto* host=m_session.host(m_hosts.begin()->first); auto* v=host?findViewById(host->active):nullptr;
		return v && PerformUndoRedo(v,false);
	}
	if(cmd=="detachedsurfaceinspect" && gStartup.headless && !m_hosts.empty()) {
		const auto* host=m_session.host(m_hosts.begin()->first); auto* v=host?findViewById(host->active):nullptr; if(!v) return false;
		if(msg) *msg="detachedsurfaceinspect: zoom="+std::to_string(v->getZoom())+" pan="+std::to_string(v->camera().panX)+","+std::to_string(v->camera().panY)+" dirty="+std::to_string(!v->getCharacter()->undoManager.isClean());
		return true;
	}
	if(cmd=="absolutescrub" && view && ch) { view->getState().absoluteScrub=true; view->getState().ScrubTimeline(&ch->frameData,ch->effectCharacter?&ch->effectCharacter->frameData:nullptr,atoi(arg.c_str())); return true; }
	if(cmd=="absolutepreviewfixture" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); auto* child=ch->frameData.get_sequence(400);
		if(!seq || seq->frames.empty() || !child || child->frames.empty()) return false;
		Frame start=seq->frames[0],later=start,released=start,body=child->frames[0];
		start.IF.clear(); start.EF.clear(); start.AF.duration=2; start.AF.aniType=2; start.AF.jump=0; start.AF.aniFlag=0;
		later.IF.clear(); later.EF.clear(); later.AF.duration=3; later.AF.aniType=3;
		released.IF.clear(); released.EF.clear(); released.AF.duration=120; released.AF.aniType=3;
		body.IF.clear(); body.EF.clear(); body.AF.duration=120; body.AF.aniType=3;
		Frame_EF spawn{}; spawn.type=1; spawn.number=400; spawn.parameters[0]=150; spawn.parameters[1]=-40; start.EF.push_back(spawn);
		Frame_EF pose{}; pose.type=14; pose.number=26; pose.parameters[0]=70; pose.parameters[1]=-40; pose.parameters[5]=10; start.EF.push_back(pose);
		Frame_EF release=pose; release.parameters[3]=1; release.parameters[4]=36; released.EF.push_back(release);
		seq->frames={start,later,released}; child->frames={body}; ch->frameData.mark_modified(st.pattern); ch->frameData.mark_modified(400); ch->undoManager.markModified(); ch->undoManager.flush();
		st.opponentFollow=true; st.opponentReleaseVectors=true; st.opponentVectorLoadTried=false; st.vizSettings.showSpawnedPatterns=true;
		return true;
	}
	if(cmd=="timelinedurationtest" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); if(!seq || seq->frames.empty()) return false;
		Frame f=seq->frames[0]; f.IF.clear(); f.EF.clear(); f.AF.aniType=1;
		seq->frames={f,f,f}; seq->frames[0].AF.duration=0; seq->frames[1].AF.duration=5; seq->frames[2].AF.duration=4;
		ch->frameData.mark_modified(st.pattern); ch->undoManager.markModified(); ch->undoManager.flush();
		st.InspectState(&ch->frameData,1,4); auto* box=view->getBoxPane();
		box->BeginDurationResize(1,100,4); box->UpdateDurationResize(112);
		bool preview=ch->frameData.get_sequence(st.pattern)->frames[1].AF.duration==5;
		box->EndDurationResize(true); bool cancel=ch->frameData.get_sequence(st.pattern)->frames[1].AF.duration==5;
		box->BeginDurationResize(1,100,4); box->UpdateDurationResize(80); bool committed=box->EndDurationResize(false);
		seq=ch->frameData.get_sequence(st.pattern); bool result=seq->frames[1].AF.duration==1 && seq->frames[0].AF.duration==0 && seq->frames[2].AF.duration==4 && st.frame==1 && st.AuthoredTick(&ch->frameData)==1;
		ch->undoManager.undo(); seq=ch->frameData.get_sequence(st.pattern);
		bool undo=seq->frames[1].AF.duration==5 && st.AuthoredTick(&ch->frameData)==5;
		ch->undoManager.redo(); bool redo=ch->frameData.get_sequence(st.pattern)->frames[1].AF.duration==1;
		box->BeginDurationResize(1,100,4); box->UpdateDurationResize(200); ch->frameData.mark_modified(st.pattern);
		bool stale=!box->EndDurationResize(false) && ch->frameData.get_sequence(st.pattern)->frames[1].AF.duration==1;
		box->BeginDurationResize(1,100,4); box->UpdateDurationResize(108); CancelViewportGestures(); bool route=!box->DurationResizeActive();
		if(msg) *msg="timelinedurationtest: preview="+std::to_string(preview)+" cancel="+std::to_string(cancel)+" commit="+std::to_string(committed&&result)+" undo="+std::to_string(undo)+" redo="+std::to_string(redo)+" stale="+std::to_string(stale)+" cancel_route="+std::to_string(route);
		return true;
	}
	if(cmd=="discoverpathstest" && view && ch && gStartup.headless) {
		FrameData* effects=ch->effectCharacter?&ch->effectCharacter->frameData:nullptr;
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); if(!seq || seq->frames.empty()) return false;
		Frame loop=seq->frames[0],end=loop;
		loop.IF.clear(); loop.EF.clear(); loop.AF.duration=2; loop.AF.aniType=2; loop.AF.aniFlag=0; loop.AF.jump=0; loop.AF.loopCount=0;
		end.IF.clear(); end.EF.clear(); end.AF.duration=3; end.AF.aniType=0;
		seq->frames={loop,end}; ch->frameData.mark_modified(st.pattern); st.previewOptions.horizonTicks=60;
		st.DiscoverPaths(&ch->frameData,effects);
		bool discovery=st.discoveredRoutes.size()==2 && st.discoveredRoutes[1].start==1;
		st.SelectPath(&ch->frameData,effects,1); bool selected=st.frame==1 && st.currentTick==0 && !st.authoredInspection;
		st.absoluteScrub=true; st.ScrubTimeline(&ch->frameData,effects,1); auto& sim=st.BindPreviewSim(&ch->frameData,effects); preview::TickState sample; sim.getStateAt(st.PreviewTick(&ch->frameData),sample);
		bool absolute=sample.root() && sample.root()->frame==0;
		st.SelectPath(&ch->frameData,effects,0); st.playAllRoutes=true; st.playbackLoop=false; st.currentTick=discovery?st.discoveredRoutes.front().lastTick:0; st.animating=true; AdvanceViewPlayback(view);
		bool all=st.routeStart==1 && st.currentTick==0 && st.animating && st.playAllRoutes;
		st.currentTick=st.discoveredRoutes.empty()?0:st.discoveredRoutes.back().lastTick; AdvanceViewPlayback(view); all &= !st.animating && st.playbackFinished;
		st.playbackLoop=true; st.playAllRoutes=true; st.playbackFinished=false; st.animating=true; AdvanceViewPlayback(view); bool wrap=st.routeStart==0 && st.currentTick==0 && st.animating;
		st.animating=false; ch->frameData.mark_modified(st.pattern); st.BindPreviewSim(&ch->frameData,effects);
		bool stale=st.discoveredRoutes.empty() && !st.playAllRoutes;
		st.SelectPath(&ch->frameData,effects,1);
		if(msg) *msg="discoverpathstest: discovery="+std::to_string(discovery)+" selected="+std::to_string(selected)+" absolute="+std::to_string(absolute)+" all="+std::to_string(all)+" loop="+std::to_string(wrap)+" stale="+std::to_string(stale);
		return true;
	}
	if(cmd=="timelinezoom" && view && gStartup.headless) {view->getBoxPane()->SetTimelineZoom((float)atof(arg.c_str()));return true;}
	if(cmd=="timelinecellfixture" && view && ch && gStartup.headless) {
		auto& st=view->getState();auto* seq=ch->frameData.get_sequence(st.pattern);
		if(!seq || seq->frames.empty()) return false;
		Frame sample=seq->frames[0];sample.EF.clear();sample.IF.clear();sample.AF.aniType=1;sample.AF.jump=0;
		seq->frames.assign(3,sample);seq->frames[0].AF.duration=2;seq->frames[1].AF.duration=3;seq->frames[2].AF.duration=4;
		ch->frameData.mark_modified(st.pattern);st.absoluteScrub=true;st.InspectState(&ch->frameData,0);return true;
	}
	if(cmd=="timelineinspect" && view) {
		const auto& st=view->getState(); if(msg) *msg="timelineinspect: loop="+std::to_string(st.playbackLoop)+" authored="+std::to_string(st.authoredInspection)+" tick="+std::to_string(st.currentTick)+" route="+std::to_string(st.routeStart); return true;
	}
	if(cmd=="timelinenavigationtest" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); if(!seq || seq->frames.empty()) return false;
		Frame loop=seq->frames[0],skipped=loop,last=loop;
		loop.IF.clear(); loop.EF.clear(); loop.AF.duration=2; loop.AF.aniType=2; loop.AF.aniFlag=0; loop.AF.jump=0; loop.AF.loopCount=0;
		skipped.IF.clear(); skipped.EF.clear(); skipped.AF.duration=3; skipped.AF.aniType=1;
		last.IF.clear(); last.EF.clear(); last.AF.duration=4; last.AF.aniType=3;
		seq->frames={loop,skipped,last}; ch->frameData.mark_modified(st.pattern);
		st.previewOptions.horizonTicks=60;
		st.SeekTimelineTick(&ch->frameData,nullptr,8,"test A"); const int first=st.frame;
		st.InspectState(&ch->frameData,2,2); bool inspect=st.frame==2 && st.authoredOffset==2 && st.currentTick==8 && st.authoredInspection;
		st.SeekTimelineTick(&ch->frameData,nullptr,4,"test B"); st.SeekTimelineTick(&ch->frameData,nullptr,8,"test A again");
		bool replay=st.frame==first && st.frame==0 && !st.authoredInspection;
		Frame_IF branch{}; branch.type=4; branch.parameters[0]=2; seq->frames[0].IF.push_back(branch); ch->frameData.mark_modified(st.pattern);
		st.previewOptions.defaultIfAssumption=preview::IfAssume::False; st.SeekTimelineTick(&ch->frameData,nullptr,1,"IF false"); bool policy=st.frame==0;
		st.previewOptions.defaultIfAssumption=preview::IfAssume::True; st.SeekTimelineTick(&ch->frameData,nullptr,1,"IF true"); policy &= st.frame==2;
		st.previewOptions.defaultIfAssumption=preview::IfAssume::False;
		st.absoluteScrub=true;
		st.ScrubTimeline(&ch->frameData,nullptr,3); bool absolute=st.frame==1 && st.authoredOffset==1 && st.currentTick==1;
		st.ScrubTimeline(&ch->frameData,nullptr,7); absolute &= st.frame==2 && st.authoredOffset==2;
		st.ScrubTimeline(&ch->frameData,nullptr,3); absolute &= st.frame==1 && st.authoredOffset==1;
		st.playbackLoop=false; st.SeekTimelineTick(&ch->frameData,nullptr,60,"once horizon"); st.animating=true; AdvanceViewPlayback(view);
		bool once=st.currentTick==60 && !st.animating && st.playbackFinished;
		st.playbackLoop=true; st.playbackFinished=false; st.animating=true; AdvanceViewPlayback(view); bool wrap=st.currentTick==0 && st.animating; st.animating=false;
		seq=ch->frameData.get_sequence(st.pattern); last.AF.aniType=1; seq->frames={last}; ch->frameData.mark_modified(st.pattern);
		auto& sim=st.BindPreviewSim(&ch->frameData,nullptr); int end=sim.settledTick(); st.SeekTimelineTick(&ch->frameData,nullptr,end,"once settled"); st.playbackLoop=false; st.animating=true; AdvanceViewPlayback(view);
		once &= end>=0 && st.currentTick==end && !st.animating;
		st.animating=false;
		st.InspectState(&ch->frameData,0,1);
		if(msg) *msg="timelinenavigationtest: inspect="+std::to_string(inspect)+" replay="+std::to_string(replay)+" policy="+std::to_string(policy)+" once="+std::to_string(once)+" loop="+std::to_string(wrap)+" absolute="+std::to_string(absolute);
		return true;
	}
	if(cmd=="opponenteditfixture" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern); if(!seq || seq->frames.empty()) return false;
		Frame start=seq->frames[0],later=start;
		start.IF.clear(); start.EF.clear(); start.AF.duration=1; start.AF.aniType=1;
		later.IF.clear(); later.EF.clear(); later.AF.duration=120; later.AF.aniType=3;
		Frame_EF e{}; e.type=14; e.number=26; e.parameters[0]=100; e.parameters[1]=-40; e.parameters[5]=10;
		if(arg=="native4") { e.type=4; e.parameters[5]=0; e.parameters[7]=10; }
		if(arg=="extended") { e={}; e.type=6; e.number=161; e.parameters[1]=26; e.parameters[2]=10; e.parameters[3]=100; e.parameters[4]=-40; e.parameters[8]=6; }
		const bool release=arg.rfind("release",0)==0;
		if(release) { e={}; e.type=arg=="release4"?4:14; e.number=26; e.parameters[0]=100; e.parameters[1]=-40; e.parameters[3]=1; e.parameters[4]=36; e.parameters[7]=arg=="release4"?10:0; }
		if(arg=="release162") { e={}; e.type=6; e.number=162; e.parameters[0]=26; e.parameters[1]=100; e.parameters[2]=-40; e.parameters[3]=36; }
		start.EF.push_back(e); seq->frames={start,later}; ch->frameData.mark_modified(st.pattern); ch->undoManager.markModified(); ch->undoManager.flush();
		st.opponentEnabled=true; st.opponentFollow=true; st.opponentOnion=false; st.opponentOpacity=1; st.frame=1; st.currentTick=5; st.authoredInspection=false;
		st.opponentReleaseVectors=release; st.opponentReleaseGround=true; st.opponentVectorLoadTried=false;
		if(arg=="releaselanded") st.currentTick=90;
		st.previewOptions.rootFacingLeft=arg=="left" || arg=="releaseleft"; st.forceSpawnTreeRebuild=true;
		return true;
	}
	if(cmd=="positionpixeltest" && view && gStartup.headless) {
		int covered=0,empty=0,mismatch=0;
		for(const auto& p:SurfaceFor(view).footprints) if(p.kind==0 && p.mask) {
			for(int y=0;y<p.mask->height;y+=std::max(1,p.mask->height/64)) for(int x=0;x<p.mask->width;x+=std::max(1,p.mask->width/64)) {
				const float u=((x+.5f)/p.mask->width-p.uv[0].x)/(p.uv[1].x-p.uv[0].x);
				const float v=((y+.5f)/p.mask->height-p.uv[0].y)/(p.uv[3].y-p.uv[0].y);
				if(u<0 || u>=1 || v<0 || v>=1) continue;
				const float sx=p.quad[0].x+u*(p.quad[1].x-p.quad[0].x)+v*(p.quad[3].x-p.quad[0].x);
				const float sy=p.quad[0].y+u*(p.quad[1].y-p.quad[0].y)+v*(p.quad[3].y-p.quad[0].y);
				const bool expected=p.mask->coverage[size_t(y)*p.mask->width+x]!=0;
				if(expected) ++covered; else ++empty;
				if(PositionSpriteHit(p.kind,p.index,sx,sy)!=expected) ++mismatch;
			}
		}
		if(msg) *msg="positionpixeltest: covered="+std::to_string(covered)+" empty="+std::to_string(empty)+" mismatch="+std::to_string(mismatch);
		return true;
	}
	if(cmd=="positionflipfixture" && view && ch && gStartup.headless) {
		auto* seq=ch->frameData.get_sequence(view->getState().pattern); if(!seq) return false;
		auto& f=seq->frames[view->getState().frame]; bool changed=false;
		for(auto& e:f.EF) if(e.type==1 || e.type==101 || e.type==1000) { e.parameters[2]=(e.parameters[2]&~2048)|(arg=="1"?2048:0); changed=true; }
		if(changed) { ch->frameData.mark_modified(view->getState().pattern); ch->undoManager.markModified(); ch->undoManager.flush(); }
		return changed;
	}
	if(cmd=="positionpersistentfixture" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern);
		auto* child=ch->frameData.get_sequence(400); if(!seq || seq->frames.empty() || !child || child->frames.empty()) return false;
		Frame start=seq->frames[0],later=start,body=child->frames[0];
		start.IF.clear(); start.EF.clear(); start.AF.duration=1; start.AF.aniType=1;
		later.IF.clear(); later.EF.clear(); later.AF.duration=120; later.AF.aniType=3;
		body.IF.clear(); body.EF.clear(); body.AF.duration=120; body.AF.aniType=3;
		Frame_EF e{}; e.type=1; e.number=400; e.parameters[0]=100; e.parameters[1]=-80; e.parameters[2]=arg=="flip"?2048:0;
		start.EF.push_back(e); seq->frames={start,later}; child->frames={body};
		ch->frameData.mark_modified(st.pattern); ch->frameData.mark_modified(400); ch->undoManager.markModified(); ch->undoManager.flush();
		st.frame=1; st.currentTick=5; st.forceSpawnTreeRebuild=true; st.vizSettings.showSpawnedPatterns=true;
		return true;
	}
	if(cmd=="positionpickmodifier" && gStartup.headless) {
		ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftCtrl,arg=="ctrl");
		ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftAlt,arg=="alt");
		ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,arg=="ctrl");
		ImGui::GetIO().AddKeyEvent(ImGuiMod_Alt,arg=="alt"); return true;
	}
	if(cmd=="positionwidgetinspect" && view && gStartup.headless) {
		const auto name="Position targets###position_targets_"+std::to_string(view->getId());
		const auto* window=ImGui::FindWindowByName(name.c_str());
		const auto& pick=view->getState().positionSelection;
		if(msg) *msg="positionwidgetinspect: visible="+std::to_string(window && window->WasActive)+" selected="+std::to_string(pick.selected.kind>=0)+" candidates="+std::to_string(pick.candidates.size());
		return true;
	}
	if(cmd=="positionfeedbackprobe" && view && gStartup.headless) {
		view->getState().positionHandleHover=arg=="on";
		const auto targets=CollectPositionTargets(); if(targets.empty()) return false;
		const auto vp=ImGui::GetMainViewport()->Pos;
		ImGui::GetIO().AddMousePosEvent(vp.x+targets.front().screen.x,vp.y+targets.front().screen.y);
		if(msg) *msg="positionfeedbackprobe: requested="+std::to_string(vp.x+targets.front().screen.x)+","+std::to_string(vp.y+targets.front().screen.y);
		return true;
	}
	if(cmd=="positionsolo" && view && gStartup.headless) {
		if(arg=="cycle") return CyclePositionTarget(1,0,0,false);
		if(arg=="scope") { view->getState().InspectState(&ch->frameData,view->getState().frame,1); return true; }
		if(arg=="edit") {
			auto& pick=view->getState().positionSelection; const auto key=pick.solo;
			for(const auto& t:CollectPositionTargets()) if(PositionKey(t)==key) {
				LeftClick(int(t.screen.x),int(t.screen.y)); bool routed=m_posDrag.active && PositionKey(m_posDrag.target)==key;
				if(m_posDrag.active) { PositionDragBy(20,10); EndPositionDrag(true); }
				if(msg) *msg="positionsoloedit: retained="+std::to_string(routed && pick.solo==key);
				return true;
			}
			return false;
		}
		return TogglePositionSolo();
	}
	if(cmd=="positiontoolinspect" && view && gStartup.headless) {
		const auto& st=view->getState();
		const auto name="Position targets###position_targets_"+std::to_string(view->getId());
		auto* window=ImGui::FindWindowByName(name.c_str()); bool anchor=true;
		if(window && window->WasActive) if(auto* host=ImGui::FindWindowByName("Dock Window")) if(auto* central=ImGui::DockBuilderGetCentralNode(host->GetID("Dock Space"))) {
			anchor=std::fabs(window->Pos.x+window->Size.x-(central->Pos.x+central->Size.x-ui::Px(8.f)))<=2 && std::fabs(window->Pos.y+window->Size.y-(central->Pos.y+central->Size.y-ui::Px(8.f)))<=2;
		}
		if(msg) *msg="positiontoolinspect: feedback="+std::to_string(st.positionFeedbackCount>0)+" anchor="+std::to_string(anchor)+" solo="+std::to_string(st.positionSelection.solo.kind>=0)+" footprints="+std::to_string(SurfaceFor(view).footprints.size())+" handle_hover="+std::to_string(st.positionHandleHover)+" highlight="+std::to_string(st.positionHighlight);
		return true;
	}
	if(cmd=="positionconfigopen" && view && gStartup.headless) { view->getState().positionSettingsRequested=true; return true; }
	if(cmd=="positionoverlapfixture" && view && ch && gStartup.headless) {
		auto& st=view->getState(); auto* seq=ch->frameData.get_sequence(st.pattern);
		if(!seq || seq->frames.empty() || seq->frames[0].AF.layers.empty()) return false;
		Frame f=seq->frames[0]; f.EF.clear(); f.IF.clear(); f.AF.duration=120; f.AF.aniType=3;
		f.AF.layers={f.AF.layers[0],f.AF.layers[0]};
		for(auto& layer:f.AF.layers) layer.offset_y=0; // keep the handle inside the actual central canvas, not behind Box Pane
		seq->frames={f}; ch->frameData.mark_modified(st.pattern);
		if(auto* host=ImGui::FindWindowByName("Dock Window")) if(auto* central=ImGui::DockBuilderGetCentralNode(host->GetID("Dock Space")))
			view->camera().panY=central->Pos.y+central->Size.y*.5f-ImGui::GetMainViewport()->Pos.y-clientRect.y*.5f;
		view->getBoxPane()->positionTool=true; st.InspectState(&ch->frameData,0); return true;
	}
	if(cmd=="positionoverlaptest" && view && ch && gStartup.headless) {
		auto targets=CollectPositionTargets(); if(targets.size()!=2) return false;
		ImVec2 point{}; bool covered=false;
		for(const auto& p:SurfaceFor(view).footprints) if(p.kind==PositionTarget::Layer && p.mask) {
			for(int y=0;y<p.mask->height && !covered;++y) for(int x=0;x<p.mask->width;++x) {
				if(!p.mask->coverage[size_t(y)*p.mask->width+x]) continue;
				float u=((x+.5f)/p.mask->width-p.uv[0].x)/(p.uv[1].x-p.uv[0].x),v=((y+.5f)/p.mask->height-p.uv[0].y)/(p.uv[3].y-p.uv[0].y);
				if(u<.05f || u>.95f || v<.05f || v>.95f) continue;
				point=ImVec2(std::round(p.quad[0].x+u*(p.quad[1].x-p.quad[0].x)+v*(p.quad[3].x-p.quad[0].x)),std::round(p.quad[0].y+u*(p.quad[1].y-p.quad[0].y)+v*(p.quad[3].y-p.quad[0].y)));
				if(PositionSpriteHit(PositionTarget::Layer,0,point.x,point.y) && PositionSpriteHit(PositionTarget::Layer,1,point.x,point.y)) { covered=true; break; }
			}
			if(covered) break;
		}
		if(!covered) return false;
		BYTE saved[256],keys[256]; GetKeyboardState(saved); memcpy(keys,saved,sizeof(keys)); keys[VK_CONTROL]|=0x80; SetKeyboardState(keys);
		auto first=PickPosition(targets,point.x,point.y,true); const float zoom=view->getZoom();
		HandleMouseWheel(!gSettings.invertWheelZoom,int(point.x),int(point.y));
		auto& pick=view->getState().positionSelection; auto second=pick.selected;
		bool cycle=!(first==second) && zoom==view->getZoom();
		HandleMouseWheel(gSettings.invertWheelZoom,int(point.x),int(point.y)); cycle &= pick.selected==first;
		pick.select(second); // same route as clicking a candidate-list row
		bool stable=PickPosition(targets,point.x,point.y,true)==second;
		LeftClick(int(point.x),int(point.y)); bool routed=m_posDrag.active && PositionKey(m_posDrag.target)==second;
		bool drag=routed && !CyclePositionTarget(1,int(point.x),int(point.y));
		if(m_posDrag.active) { PositionDragBy(20,10); EndPositionDrag(false); }
		auto* seq=ch->frameData.get_sequence(view->getState().pattern);
		const int changed=seq->frames[0].AF.layers[second.index].offset_x;
		const int other=seq->frames[0].AF.layers[1-second.index].offset_x;
		bool edit=changed!=other;
		ch->undoManager.undo(); seq=ch->frameData.get_sequence(view->getState().pattern); edit &= seq->frames[0].AF.layers[0].offset_x==seq->frames[0].AF.layers[1].offset_x;
		PickPosition(CollectPositionTargets(),point.x,point.y,true); // repopulate after undo/version change
		pick.select(second); bool rotation=BeginPositionRotation(int(point.x),int(point.y)) && PositionKey(m_posDrag.target)==second;
		if(m_posDrag.active) {
			const auto pivot=m_posDrag.target.screen;
			PositionDragBy(int(-(point.y-pivot.y)-(point.x-pivot.x)),int((point.x-pivot.x)-(point.y-pivot.y)));
			rotation &= std::fabs(m_posDrag.angleDelta)>.1f;
			EndPositionDrag(true);
		}
		PickPosition(CollectPositionTargets(),point.x,point.y,true);
		bool key=RunShortcut(ShortcutAction::nextPositionTarget);
		SetKeyboardState(saved);
		if(msg) *msg="positionoverlaptest: coverage="+std::to_string(covered)+" cycle="+std::to_string(cycle)+" stable="+std::to_string(stable)+" source="+std::to_string(routed&&edit)+" drag="+std::to_string(drag)+" shortcut="+std::to_string(key)+" rotation="+std::to_string(rotation);
		return true;
	}
	if(cmd=="positiongesturetest" && view && ch && gStartup.headless) {
		const bool opponentTest=arg.rfind("opponent",0)==0;
		view->getBoxPane()->positionTool=true;
		const auto targets=CollectPositionTargets(view);
		const PositionTarget* hit=nullptr;
		for(const auto& t:targets) if((opponentTest && t.kind==PositionTarget::OpponentPose) || ((arg=="persistent" || arg=="persistentfx") && t.kind==PositionTarget::PersistentEffect) || (arg=="effect" && t.kind==PositionTarget::Effect) || (arg=="layer" && t.kind==PositionTarget::Layer)) { hit=&t; break; }
		if(!hit) return false;
		BYTE saved[256],keys[256]; GetKeyboardState(saved); memcpy(keys,saved,sizeof(keys)); keys[VK_CONTROL]|=0x80; SetKeyboardState(keys);
		ImVec2 point=hit->screen;
		const int pickIndex=hit->instance>=0?hit->instance:hit->index;
		for(const auto& p:SurfaceFor(view).footprints) if(p.kind==hit->kind && p.index==pickIndex) {
			// Pick an actual covered texel; the old bounds centre can be transparent.
			bool found=false;
			if(p.triangle) for(int i=1;i<32 && !found;++i) for(int j=1;j<32-i;++j) {
                const float u=i/32.f,v=j/32.f;
                ImVec2 q(p.quad[0].x+u*(p.quad[1].x-p.quad[0].x)+v*(p.quad[2].x-p.quad[0].x),
                         p.quad[0].y+u*(p.quad[1].y-p.quad[0].y)+v*(p.quad[2].y-p.quad[0].y));
                if(PositionSpriteHit(hit->kind,pickIndex,float(int(q.x)),float(int(q.y)),view)) {point=q; found=true; break;}
            }
			if(!p.triangle && p.mask) for(int y=0;y<p.mask->height && !found;++y) for(int x=0;x<p.mask->width;++x) {
				if(!p.mask->coverage[size_t(y)*p.mask->width+x]) continue;
				const float u=((x+.5f)/p.mask->width-p.uv[0].x)/(p.uv[1].x-p.uv[0].x);
				const float v=((y+.5f)/p.mask->height-p.uv[0].y)/(p.uv[3].y-p.uv[0].y);
				if(u<.02f || u>.98f || v<.02f || v>.98f) continue;
				point=ImVec2(p.quad[0].x+u*(p.quad[1].x-p.quad[0].x)+v*(p.quad[3].x-p.quad[0].x),p.quad[0].y+u*(p.quad[1].y-p.quad[0].y)+v*(p.quad[3].y-p.quad[0].y));
				if(!PositionSpriteHit(hit->kind,pickIndex,float(int(point.x)),float(int(point.y)),view)) continue;
				found=true; break;
			}
			if(found) break;
		}
		const bool footprint=PositionSpriteHit(hit->kind,pickIndex,point.x,point.y,view);
		const bool testMove=arg!="opponent-rotate" && arg!="opponent-undo";
		const bool testRotation=arg!="opponent-move" && arg!="opponent-undo";
		if(testMove) BeginPositionDrag(view,int(point.x),int(point.y));
		bool moved=!testMove || m_posDrag.active;
		if(testMove && moved) { PositionDragBy(20,10); EndPositionDrag(true); }
		bool rotated=!testRotation || BeginPositionRotation(int(point.x),int(point.y),view);
		if(testRotation && rotated) {
			const auto pivot=m_posDrag.target.screen; int dx=int(-(point.y-pivot.y)-(point.x-pivot.x)); int dy=int((point.x-pivot.x)-(point.y-pivot.y));
			Frame_EF before{};
			if(opponentTest) before=ch->frameData.get_sequence(m_posDrag.pattern)->frames[m_posDrag.frame].EF[hit->index];
			PositionDragBy(dx,dy); rotated=std::fabs(m_posDrag.angleDelta)>.1f;
			if(opponentTest) {
				const auto& e=ch->frameData.get_sequence(m_posDrag.pattern)->frames[m_posDrag.frame].EF[hit->index];
				rotated &= e.parameters[hit->rotationParam]!=before.parameters[hit->rotationParam] && m_posDrag.angleDelta*hit->rotationSign>0;
				for(size_t j=0;j<sizeof(e.parameters)/sizeof(int);++j) if(int(j)!=hit->rotationParam) rotated &= e.parameters[j]==before.parameters[j];
			}
			EndPositionDrag(true);
			if(opponentTest) {
				const auto& e=ch->frameData.get_sequence(hit->sourcePattern)->frames[hit->sourceFrame].EF[hit->index];
				for(size_t j=0;j<sizeof(e.parameters)/sizeof(int);++j) rotated &= e.parameters[j]==before.parameters[j];
			}
		}
		if(arg=="persistent" || arg=="persistentfx" || arg=="opponent-undo") {
			const int sp=hit->sourcePattern,sf=hit->sourceFrame,ei=hit->index,xp=hit->xParam;
			auto read=[&]() { return ch->frameData.get_sequence(sp)->frames[sf].EF[ei].parameters[xp]; };
			const int original=read(); BeginPositionDrag(view,int(point.x),int(point.y));
			const auto originalEffect=ch->frameData.get_sequence(sp)->frames[sf].EF[ei];
			auto readY=[&]() { return ch->frameData.get_sequence(sp)->frames[sf].EF[ei].parameters[hit->yParam]; };
			const int originalY=readY();
			const bool routed=m_posDrag.active && m_posDrag.pattern==sp && m_posDrag.frame==sf && sf!=view->getState().frame;
			if(m_posDrag.active) { PositionDragBy(30,15); EndPositionDrag(false); }
			const int changed=read(),changedY=readY(); bool committed=changed==original+int(std::lround(30.f/hit->m[0])) && changedY==originalY+int(std::lround(15.f/hit->m[3]));
			const auto changedEffect=ch->frameData.get_sequence(sp)->frames[sf].EF[ei];
			for(size_t j=0;j<sizeof(originalEffect.parameters)/sizeof(int);++j) if(int(j)!=xp && int(j)!=hit->yParam) committed &= changedEffect.parameters[j]==originalEffect.parameters[j];
			ch->undoManager.undo(); const bool undone=read()==original && readY()==originalY;
			ch->undoManager.redo(); const bool redone=read()==changed && readY()==changedY;
			ch->undoManager.undo();
			HC_LOG_DEBUG("position gesture view={} source={}/{} tick={} route={} commit={} undo={} redo={} delta={},{} expected={},{}",(unsigned long long)view->getId(),sp,sf,view->getState().PreviewTick(&ch->frameData),routed,committed,undone,redone,changed-original,changedY-originalY,int(std::lround(30.f/hit->m[0])),int(std::lround(15.f/hit->m[3])));
   moved &= routed&&committed&&undone&&redone;
			if(msg) *msg="persistent source routing/undo/redo: "+std::to_string(routed&&committed&&undone&&redone);
		}
		SetKeyboardState(saved);
		if(msg) *msg=std::string("positiongesturetest: footprint=")+(footprint?"1":"0")+" move="+(moved?"1":"0")+" rotation="+(rotated?"1":"0")+" point="+std::to_string(point.x)+","+std::to_string(point.y)+" kind="+std::to_string(hit->kind)+" index="+std::to_string(pickIndex);
		return true;
	}
	if(cmd=="opponentonionconfig" && view) { auto& o=view->opponentOnion(); o.before=4; o.after=1; o.spacing=7; o.alpha=.27f; view->onion().enabled=false; return true; }
	if (cmd=="opponentpreview" && view) {
		const int index=atoi(arg.c_str());
		if(index<0 || index>=(int)characters.size()) return false;
		auto& st=view->getState(); st.opponentTargetPath=characters[index]->getTopHA6Path(); st.opponentEnabled=true;
		m_showOpponent=true; return true;
	}
	if (cmd=="opponentinspect" && view) {
		if(msg) *msg="opponentinspect: "+view->getState().opponentStatus;
		return true;
	}
	if(cmd=="opponentroster" && view) {
		RefreshOpponentRoster(view);
		if(msg) *msg="opponentroster: "+std::to_string(view->getState().opponentRoster.size());
		return true;
	}
	if(cmd=="opponentselect" && view) return SelectOpponent(view,arg);
	if(cmd=="opponentcycle" && view) { const bool ok=CycleOpponent(view,atoi(arg.c_str())); if(msg) *msg=ok?"opponentcycle: selected":"opponentcycle: empty pool or unavailable"; return true; }
	if(cmd=="opponentpoolnone" && view) { RefreshOpponentRoster(view); OpponentCyclePool(view->getState()).clear(); ImGui::MarkIniSettingsDirty(); return true; }
	if(cmd=="opponentopacity" && view) { view->getState().opponentOpacity=std::clamp(float(atof(arg.c_str())),0.f,1.f); return true; }
	if(cmd=="opponentonion" && view) { view->getState().opponentOnion=arg=="1"; view->onion().enabled=true; return true; }
	if(cmd=="opponentrelease" && view) { auto& s=view->getState(); s.opponentReleaseVectors=arg=="1"; s.opponentFollow=true; s.opponentVectorLoadTried=false; return true; }
	if(cmd=="opponenttick" && view) { auto& s=view->getState(); s.currentTick=std::clamp(atoi(arg.c_str()),0,1800); return true; }
	if(cmd=="opponentraisetest" && view && gStartup.headless) {
		ImGui::SetWindowCollapsed("Grab preview settings",true,ImGuiCond_Always);
		view->getState().opponentSettingsRequested=true; return true;
	}
	if(cmd=="opponentkeys" && gStartup.headless) {
		m_showKeyBindings=true;
		snprintf(m_shortcutFilter.InputBuf,sizeof(m_shortcutFilter.InputBuf),"Opponent"); m_shortcutFilter.Build();
		int count=0; for(const auto& b:shortcuts.registry().bindings()) if(m_shortcutFilter.PassFilter(b.name)) ++count;
		if(msg) *msg="opponentkeys: visible registry rows="+std::to_string(count);
		return true;
	}
	if(cmd=="opponentsystemkeytest" && view && gStartup.headless) {
		BYTE saved[256],pressed[256]; GetKeyboardState(saved); memcpy(pressed,saved,256); pressed[VK_MENU]=0x80;
		SetKeyboardState(pressed);
		shortcuts.beginFrame(); shortcuts.claimFocus(ShortcutContext::characterView,view->getId()); shortcuts.endFrame();
		auto& io=ImGui::GetIO(); const bool wanted=io.WantCaptureKeyboard; io.WantCaptureKeyboard=false;
		const auto before=view->getState().opponentTargetPath;
		SendMessage(mainWindowHandle,WM_SYSKEYDOWN,0xBE,1u<<29);
		SendMessage(mainWindowHandle,WM_SYSCHAR,L'.',1u<<29);
		io.WantCaptureKeyboard=wanted; SetKeyboardState(saved);
		if(msg) *msg=std::string("opponentsystemkeytest: ")+(before!=view->getState().opponentTargetPath?"cycled":"FAILED");
		return true;
	}
	if(cmd=="opponentreleasefixture" && ch && gStartup.headless) {
		auto* seq=ch->frameData.get_sequence(view->getState().pattern);
		if(!seq || seq->frames.empty()) return false;
		Frame held=seq->frames[0],released=held;
		held.IF.clear(); held.EF.clear(); held.AF.duration=1; held.AF.aniType=1;
		released.IF.clear(); released.EF.clear(); released.AF.duration=120; released.AF.aniType=3;
		Frame_EF pose{}; pose.type=4; pose.number=26; pose.parameters[7]=10; held.EF.push_back(pose);
		Frame_EF release{}; release.type=14; release.number=26; release.parameters[3]=1; release.parameters[4]=36; released.EF.push_back(release);
		seq->frames={held,released}; ch->frameData.mark_modified(view->getState().pattern);
		if(msg) *msg="opponentreleasefixture: in-memory owner only; never saved";
		return true;
	}
	if(cmd=="opponentprojectsave" && gStartup.headless) { ProjectManager::SetCurrentProjectPath(arg); saveProject(); return true; }
	if(cmd=="opponentcontrolsinspect" && view) {
		auto& s=view->getState();
		if(msg) *msg="opponentcontrols: opacity="+std::to_string(s.opponentOpacity)+" onion="+std::to_string(s.opponentOnion)+" pool="+std::to_string(OpponentCyclePool(s).size())+" target="+s.opponentTargetPath+" tick="+std::to_string(s.currentTick)+" onion_spacing="+std::to_string(view->opponentOnion().spacing);
		return true;
	}
	if (cmd == "transformtrace" && view) {
		const auto& s = view->getState(); render.RequestTransformTrace(arg,s.pattern,s.frame,s.PreviewTick(&ch->frameData)); return true;
	}
	if (cmd == "cmdopen") { openCommandEditor(arg); return true; }
	if (cmd == "cmdtab") {
		commandEditor.debugRequestTab(atoi(arg.c_str()));
		if (msg) *msg = "cmdtab: available=" + std::string(commandEditor.empty() ? "0" : "1");
		return true;
	}
	if (cmd == "extensionprofile") {
		SetExtensionProfile(arg == "extended" ? ExtensionProfile::Extended : ExtensionProfile::Vanilla);
		if (msg) *msg = std::string("extensionprofile: ") + (ExtendedProfileEnabled() ? "extended" : "vanilla");
		return true;
	}
	// Read-only test navigation to a real BOF record; the normal pane renders it
	// on subsequent frames. No source records or document state are rewritten.
	if (cmd == "bofsample159" && gStartup.headless && ch) {
		auto& state = view->getState();
		auto* seq = ch->frameData.get_sequence(state.pattern);
		if (seq && state.frame >= 0 && state.frame < (int)seq->frames.size()) {
			Frame_IF record{}; record.type = 159;
			record.parameters[1] = 1; record.parameters[4] = 2;
			record.parameters[5] = 3; record.parameters[6] = 2;
			record.parameters[7] = 1; record.parameters[8] = 77;
			seq->frames[state.frame].IF = {record}; // isolate the disposable panel fixture
			ch->frameData.mark_modified(state.pattern);
			if (msg) *msg = "bofsample159: disposable headless fixture added";
		}
		return true;
	}
	if (cmd == "bofinspect") {
		const int type = atoi(arg.c_str());
		if (ch) for (int pi = 0; pi < ch->frameData.get_sequence_count(); ++pi) {
			auto* seq = ch->frameData.get_sequence(pi);
			if (!seq) continue;
			for (int fi = 0; fi < (int)seq->frames.size(); ++fi) {
				if (arg == "atng" && mbaacc::extended::isTypedHitgrab((unsigned)seq->frames[fi].AT.hitgrab)) {
					view->getState().pattern = pi; view->getState().frame = fi;
					if (msg) *msg = "bofinspect: typed ATNG profile=" + std::to_string(mbaacc::extended::typedHitgrabProfile((unsigned)seq->frames[fi].AT.hitgrab));
					return true;
				}
				auto report = [&](bool valid) {
					view->getState().pattern = pi; view->getState().frame = fi;
					if (msg) *msg = "bofinspect: type=" + std::to_string(type) + " pattern=" + std::to_string(pi) + " state=" + std::to_string(fi) + " valid=" + (valid ? "1" : "0");
				};
				for (const auto& c : seq->frames[fi].IF) if ((type == 158 || type == 159) && c.type == type) {
					report(bof_authoring::ConditionValid(type, c.parameters)); return true;
				}
				for (const auto& e : seq->frames[fi].EF) if (type >= 160 && type <= 162 && e.type == 6 && e.number == type) {
					report(bof_authoring::EffectValid(type, e.parameters)); return true;
				}
			}
		}
		if (msg) *msg = "bofinspect: missing type=" + std::to_string(type);
		return true;
	}
	if (cmd == "sq") {   // Squirrel Script panel (squirrel/script_panel.h scriptCommand)
		if (!scriptPanel) { scriptPanel = std::make_unique<sqedit::ScriptPanel>(); wireScriptPanel(); }
		if (arg != "hide") scriptPanel->setVisible(true);
		const size_t sp = arg.find(' ');
		return scriptPanel->scriptCommand(arg.substr(0, sp), sp == std::string::npos ? "" : arg.substr(sp + 1), msg), true;
	}
	if (cmd == "patopen") { createPatEditorView(arg); return true; }
	// Test entry: enqueue the same file-drop path as WM_DROPFILES, without OS
	// mouse timing. ProcessDroppedFiles runs it through the real dispatcher.
	if (cmd == "dropfile") { m_droppedFiles.push_back(arg); return true; }
	if (cmd == "newpat") { createNewPatView(arg == "mbaacc"); return true; }
	if (cmd == "prefswindow") { m_showPatPrefs = arg != "off"; return true; }
	if ((cmd == "panview" || cmd == "viewpan") && view) { float x = 0, y = 0; sscanf(arg.c_str(), "%f %f", &x, &y); view->camera().panX = x; view->camera().panY = y; return true; }
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
			snprintf(b, sizeof b, "patinfo: doc=%d modified=%d undo=%zu redo=%zu set=%d prop=%d cut=%d uv=%d,%d,%d,%d prop.xy=%d,%d highlight=%d sets=%zu fb=%d editor=%d views=%zu path=%s",
				ch->isPatDocument() ? 1 : 0, ch->isModified() ? 1 : 0, ch->undoManager.undoCount(), ch->undoManager.redoCount(),
				st.partSet, st.partProp, st.partCutOut, co ? co->uv[0] : -1, co ? co->uv[1] : -1, co ? co->uv[2] : -1, co ? co->uv[3] : -1,
				pr ? pr->x : 0, pr ? pr->y : 0, ch->parts.partHighlight, ch->parts.partSets.size(), ch->parts.fbPartModel ? 1 : 0, view->isPatEditor() ? 1 : 0, views.size(), ch->getPATPath().c_str());
		}
		*msg = b;
		return true;
	}
	return false;
}

#endif /* UI_PAT_EDITOR_IMPL_H_GUARD */
