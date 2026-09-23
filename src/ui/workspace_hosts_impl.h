#ifndef UI_WORKSPACE_HOSTS_IMPL_H_GUARD
#define UI_WORKSPACE_HOSTS_IMPL_H_GUARD

// ============================================================================
// Detachable multi-monitor windows (GitHub issue #3, docs/HANTEI_WAVE2.md §5)
// ============================================================================
// Included from main_frame.cpp.
//
// Model: WorkspaceSession (ported from Gonptechan EX) owns which window
// ("host") every view tab lives in, the tab order and each host's active tab.
// Host 0 is the main window; every other host is an ImGui window with the
// NoAutoMerge viewport flag, i.e. always its own native OS window, which the
// user can move to another monitor.
//
// Rendering: each view draws into its own RenderTarget with its own camera
// (ui/view_render_impl.h). A detached window shows its active view's target
// with ImGui::Image; there is no swapping of global render state (EX swapped
// clientRect / render.scale / renderX/Y around every detached draw and
// restored them by hand).
//
// Layout: each detached window can dock the active view's Left/Right/Box
// panes into its own dockspace (window names are namespaced per host, so the
// arrangement belongs to the window and follows whichever tab is active).
// Geometry, tabs, active tab and the pane toggle are saved in the project file
// (atomic write, one pass); the dock split sizes persist in imgui.ini under
// the host's stable id.

#include <imgui_internal.h>

static const char* kViewTabPayload = "HANTEI_VIEW_TAB";

CharacterView* MainFrame::findViewById(uint64_t id)
{
	if (id == 0) return nullptr;
	for (auto& v : views)
		if (v->getId() == id) return v.get();
	return nullptr;
}

int MainFrame::findViewIndexById(uint64_t id) const
{
	for (size_t i = 0; i < views.size(); ++i)
		if (views[i]->getId() == id) return (int)i;
	return -1;
}

// The view keyboard shortcuts act on: the active tab of whichever window
// (main or detached) owned focus in the last UI frame.
CharacterView* MainFrame::getShortcutView()
{
	if (CharacterView* v = findViewById(shortcuts.focusedViewId()))
		return v;
	return getActiveView();
}

CharacterView* MainFrame::getToolView()
{
	if (CharacterView* v = findViewById(m_toolViewId))
		return v;
	if (CharacterView* v = getActiveView())
		return v;
	for (uint64_t hostId : m_session.hostIds())
		if (const WorkspaceSession::Host* h = m_session.host(hostId))
			if (CharacterView* v = findViewById(h->active))
				return v;
	return nullptr;
}

// Called once per UI frame after the detached hosts are drawn (they set
// m_focusedHostId).
void MainFrame::NoteToolViewFocus()
{
	if (m_focusedHostId != WorkspaceSession::MainHost) {
		if (const WorkspaceSession::Host* h = m_session.host(m_focusedHostId))
			if (findViewById(h->active)) m_toolViewId = h->active;
		return;
	}
	CharacterView* mainView = getActiveView();
	if (!mainView) return;
	// Work in the main window: its dock space (tab bar, docked panes) has
	// keyboard focus, or the canvas was clicked outside every ImGui window.
	const ImGuiContext& g = *GImGui;
	bool mainWork = false;
	if (g.NavWindow) {
		const ImGuiWindow* root = g.NavWindow->RootWindowDockTree ? g.NavWindow->RootWindowDockTree : g.NavWindow->RootWindow;
		mainWork = root && (!strcmp(root->Name, "Dock Window") || !strcmp(root->Name, "Left Pane") ||
		                    !strcmp(root->Name, "Right Pane") || !strcmp(root->Name, "Box Pane"));
	}
	const ImGuiIO& io = ImGui::GetIO();
	if (!mainWork && !io.WantCaptureMouse && (io.MouseClicked[0] || io.MouseClicked[1] || io.MouseClicked[2]) &&
	    g.MouseViewport == ImGui::GetMainViewport())
		mainWork = true;
	if (mainWork || !findViewById(m_toolViewId))
		m_toolViewId = mainView->getId();
}

static bool IsDetachableView(const CharacterView* view)
{
	// Stage tabs share the single background renderer and PAT-editor tabs
	// have their own pane set; both stay in the main window.
	return view && view->getCharacter() && !view->isStageView() && !view->isPatEditor();
}

// Keep the session in step with `views` (views are created and destroyed by
// many code paths) and make activeViewIndex follow the main window's tab.
void MainFrame::SyncWorkspaceSession()
{
	// Drop closed views.
	for (uint64_t hostId : m_session.hostIds()) {
		const WorkspaceSession::Host* host = m_session.host(hostId);
		if (!host) continue;
		const std::vector<uint64_t> tabs = host->tabs;
		for (uint64_t id : tabs)
			if (!findViewById(id)) m_session.close(id);
	}
	// New views open in the main window.
	for (auto& v : views)
		if (!m_session.contains(v->getId()))
			m_session.add(v->getId(), WorkspaceSession::MainHost, false);
	// Forget windows whose last tab left.
	for (auto it = m_hosts.begin(); it != m_hosts.end();) {
		if (!m_session.host(it->first)) it = m_hosts.erase(it);
		else ++it;
	}

	const WorkspaceSession::Host* mainHost = m_session.host(WorkspaceSession::MainHost);
	CharacterView* current = getActiveView();
	if (current && m_session.owner(current->getId()).value_or(1) == WorkspaceSession::MainHost) {
		if (mainHost->active != current->getId())
			m_session.select(WorkspaceSession::MainHost, current->getId());
		return;
	}
	// The active view left the main window (or none is set): show the main
	// window's own active tab.
	const int index = mainHost && mainHost->active ? findViewIndexById(mainHost->active) : -1;
	if (index >= 0) {
		setActiveView(index);
	} else if (activeViewIndex != -1) {
		activeViewIndex = -1;
		render.DontDraw();
	}
}

uint64_t MainFrame::DetachViewToNewHost(uint64_t viewId, ImVec2 screenPos)
{
	CharacterView* view = findViewById(viewId);
	if (!IsDetachableView(view)) return 0;
	const uint64_t hostId = m_nextHostId++;
	if (!m_session.detach(viewId, hostId)) return 0;
	HostWindow& hw = m_hosts[hostId];
	hw.id = hostId;
	// Keep the main-window size for the new window's surface when there is
	// room; the position is desktop coordinates (viewports enabled).
	hw.w = std::clamp(clientRect.x * 0.6f, 640.f, 1400.f);
	hw.h = std::clamp(clientRect.y * 0.7f, 480.f, 1000.f);
	hw.x = screenPos.x - 80.f;
	hw.y = screenPos.y - 12.f;
	hw.applyGeometry = true;
	hw.showPanes = true;
	SyncWorkspaceSession();
	markProjectModified();
	return hostId;
}

void MainFrame::MoveViewToHost(uint64_t viewId, uint64_t hostId, std::optional<size_t> index)
{
	CharacterView* view = findViewById(viewId);
	if (!view) return;
	if (hostId != WorkspaceSession::MainHost && !IsDetachableView(view)) return;
	const auto owner = m_session.owner(viewId);
	if (!owner) return;
	if (*owner == hostId && index)
		m_session.reorder(hostId, viewId, *index);
	else
		m_session.move(viewId, hostId, index, true);
	if (hostId == WorkspaceSession::MainHost) {
		const int i = findViewIndexById(viewId);
		if (i >= 0) setActiveView(i);
	}
	SyncWorkspaceSession();
	markProjectModified();
}

// One tab of a host's tab bar: close button, context menu, and the
// drag-and-drop source/target used to reorder, move and detach tabs.
// Returns true if ImGui shows the tab as selected this frame.
bool MainFrame::DrawViewTabItem(uint64_t hostId, CharacterView* view, size_t position, bool forceSelect)
{
	CharacterInstance* character = view->getCharacter();
	ImGuiTabItemFlags flags = (character && character->isModified()) ? ImGuiTabItemFlags_UnsavedDocument : 0;
	if (forceSelect) flags |= ImGuiTabItemFlags_SetSelected;
	const std::string tabId = view->getDisplayName() + "###view_" + std::to_string(view->getId());
	bool open = true;
	const int index = findViewIndexById(view->getId());
	const bool waitingForClose = pendingCloseViewIndex == index;
	const bool selected = ImGui::BeginTabItem(tabId.c_str(), waitingForClose ? nullptr : &open, flags);
	if (selected) ImGui::EndTabItem();

	// Drag source: reorder within the bar, drop on another window's bar, or
	// release anywhere else to move the tab into a new window.
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
		const uint64_t id = view->getId();
		ImGui::SetDragDropPayload(kViewTabPayload, &id, sizeof(id));
		ImGui::TextUnformatted(view->getDisplayName().c_str());
		if (IsDetachableView(view))
			ImGui::TextDisabled("Drop on a tab bar to move, anywhere else for a new window");
		else
			ImGui::TextDisabled("Stage and PAT-editor tabs stay in the main window");
		ImGui::EndDragDropSource();
		if (!m_tabDrag.active) {
			m_tabDrag.active = true;
			m_tabDrag.viewId = id;
			m_tabDrag.start = ImGui::GetIO().MouseClickedPos[0];
		}
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kViewTabPayload)) {
			uint64_t dragged = 0;
			memcpy(&dragged, p->Data, sizeof(dragged));
			// Insert before this tab, or after it when released on its right half.
			const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
			const size_t slot = ImGui::GetIO().MousePos.x > (mn.x + mx.x) * 0.5f ? position + 1 : position;
			m_tabDrag.dropped = true;
			if (dragged != view->getId()) m_pendingTabMove = {dragged, hostId, slot, true};
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		contextMenuViewIndex = index;
		ImGui::OpenPopup("ViewContextMenu");
	}
	if (!open && index >= 0) m_pendingTabClose = index;
	return selected;
}

// Draw every tab of a host and reconcile ImGui's selection with the session:
// a programmatic change (new view, move, Ctrl+Tab) is pushed to ImGui with
// SetSelected; a click on a tab is pulled into the session.
void MainFrame::DrawHostTabs(uint64_t hostId)
{
	const WorkspaceSession::Host* host = m_session.host(hostId);
	if (!host) return;
	const std::vector<uint64_t> tabs = host->tabs;
	const uint64_t sessionActive = host->active;
	uint64_t& shown = m_tabShown[hostId];      // what ImGui showed last frame
	bool& pending = m_tabPushPending[hostId];  // SetSelected issued, not shown yet
	// ImGui applies SetSelected one frame late, so while a push is pending a
	// mismatch is not a user click.
	const bool force = shown != sessionActive;
	if (force) pending = true;
	uint64_t selectedNow = 0;
	for (size_t i = 0; i < tabs.size(); ++i) {
		CharacterView* v = findViewById(tabs[i]);
		if (!v) continue;
		if (DrawViewTabItem(hostId, v, i, force && tabs[i] == sessionActive)) selectedNow = tabs[i];
	}
	if (selectedNow == sessionActive) {
		pending = false;
	} else if (!pending && selectedNow) {
		// The user clicked another tab.
		m_session.select(hostId, selectedNow);
		if (hostId == WorkspaceSession::MainHost) {
			const int index = findViewIndexById(selectedNow);
			if (index >= 0) setActiveView(index);
		}
	}
	shown = selectedNow;
}

// Apply tab moves/closes queued while drawing tab bars (never mutate the
// session or `views` in the middle of a tab bar).
void MainFrame::ApplyPendingTabActions()
{
	if (m_pendingTabMove.valid) {
		const auto m = m_pendingTabMove;
		m_pendingTabMove = PendingTabMove{};
		MoveViewToHost(m.viewId, m.hostId, m.index);
	}
	if (m_pendingTabClose >= 0) {
		const int index = m_pendingTabClose;
		m_pendingTabClose = -1;
		tryCloseView(index);
	}
}

// Context-menu items shared by every tab bar (inside BeginPopup).
void MainFrame::DrawViewTabContextItems()
{
	if (contextMenuViewIndex < 0 || contextMenuViewIndex >= (int)views.size()) return;
	CharacterView* v = views[contextMenuViewIndex].get();
	if (ImGui::MenuItem("New View of Character", nullptr, false, v->getCharacter() != nullptr))
		createViewForCharacter(v->getCharacter());
	ImGui::Separator();
	const uint64_t owner = m_session.owner(v->getId()).value_or(WorkspaceSession::MainHost);
	const bool detachable = IsDetachableView(v);
	if (ImGui::MenuItem("Move to New Window", shortcuts.registry().label(ShortcutAction::detachView).c_str(), false, detachable)) {
		const ImVec2 p = ImGui::GetMainViewport()->Pos;
		DetachViewToNewHost(v->getId(), ImVec2(p.x + 140.f, p.y + 120.f));
	}
	if (owner != WorkspaceSession::MainHost && ImGui::MenuItem("Move to Main Window"))
		MoveViewToHost(v->getId(), WorkspaceSession::MainHost, std::nullopt);
	for (const auto& [hostId, hw] : m_hosts) {
		if (hostId == owner) continue;
		const std::string label = "Move to Window " + std::to_string(hostId);
		if (ImGui::MenuItem(label.c_str(), nullptr, false, detachable))
			MoveViewToHost(v->getId(), hostId, std::nullopt);
	}
}

// Whole-bar drop target: a tab released on the empty part of a bar goes last.
void MainFrame::DrawTabBarDropTarget(uint64_t hostId, const ImRect& barRect)
{
	const ImGuiPayload* payload = ImGui::GetDragDropPayload();
	if (!payload || !payload->IsDataType(kViewTabPayload)) return;
	if (ImGui::BeginDragDropTargetCustom(barRect, ImGui::GetID("##tabbar_drop"))) {
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kViewTabPayload)) {
			uint64_t dragged = 0;
			memcpy(&dragged, p->Data, sizeof(dragged));
			m_tabDrag.dropped = true;
			m_pendingTabMove = {dragged, hostId, std::nullopt, true};
		}
		ImGui::EndDragDropTarget();
	}
}

// A tab released away from every tab bar becomes its own window.
void MainFrame::FinishTabDrag()
{
	if (!m_tabDrag.active) return;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const float dx = mouse.x - m_tabDrag.start.x, dy = mouse.y - m_tabDrag.start.y;
	if (!m_tabDrag.dropped && dx * dx + dy * dy > 40.f * 40.f)
		DetachViewToNewHost(m_tabDrag.viewId, mouse);
	m_tabDrag = TabDrag{};
}

// Draw one view's editing panes. `ns` namespaces the pane window names
// (empty = the main window's "Left Pane"/"Right Pane"/"Box Pane").
void MainFrame::DrawViewPanes(CharacterView* view, const std::string& ns)
{
	auto* character = view->getCharacter();
	if (character) {
		character->undoManager.ensureBaseline();
		if (character->effectCharacter) {
			if (view->getMainPane()) view->getMainPane()->setEffectFrameData(&character->effectCharacter->frameData);
			if (view->getRightPane()) view->getRightPane()->setEffectFrameData(&character->effectCharacter->frameData);
			if (view->getBoxPane()) view->getBoxPane()->setEffectFrameData(&character->effectCharacter->frameData);
		}
		if (std::find(m_paneCharacters.begin(), m_paneCharacters.end(), character) == m_paneCharacters.end())
			m_paneCharacters.push_back(character);
	}
	if (view->getMainPane()) view->getMainPane()->setWindowNamespace(ns);
	if (view->getRightPane()) view->getRightPane()->setWindowNamespace(ns);
	if (view->getBoxPane()) view->getBoxPane()->setWindowNamespace(ns);

	if (!view->isPatEditor()) {
		if (view->getMainPane() && view->getMainPane()->isVisible) view->getMainPane()->Draw();
		if (view->getRightPane() && view->getRightPane()->isVisible) view->getRightPane()->Draw();
		if (view->getBoxPane() && view->getBoxPane()->isVisible) view->getBoxPane()->Draw();
		if (ns.empty()) ha4ui::DrawInspector(character, view->getState());   // MBAC .DAT only
	} else {
		if (view->getPartSetPane() && view->getPartSetPane()->isVisible) view->getPartSetPane()->Draw();
		if (view->getPartPane() && view->getPartPane()->isVisible) view->getPartPane()->Draw();
		if (view->getShapePane() && view->getShapePane()->isVisible) view->getShapePane()->Draw();
		if (view->getTexturePane() && view->getTexturePane()->isVisible) view->getTexturePane()->Draw();
		if (view->getToolPane() && view->getToolPane()->isVisible) view->getToolPane()->Draw();
	}
}

// Undo: a step ends when no widget is active, no mouse button is held and no
// explicit transaction (box draw, position drag) is open. Every edit made
// during the gesture - on any pattern, from any window - becomes one step.
void MainFrame::FinishPaneUndoFrame()
{
	const ImGuiIO& io = ImGui::GetIO();
	const bool gesture = ImGui::IsAnyItemActive() ||
		io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2];
	for (CharacterInstance* character : m_paneCharacters) {
		if (!isLiveCharacter(character)) continue;
		auto& undo = character->undoManager;
		undo.endFrame(gesture);
		// Tab '*' follows the history: undoing back to the saved revision
		// clears it, any other committed revision sets it.
		if (!gesture && !undo.inTransaction()) {
			// Unsaved notes (issue #58) keep the tab dirty too.
			if (undo.isClean() && !character->frameData.notes.dirty) character->clearModified();
			else character->markModified();
		}
	}
	m_paneCharacters.clear();
}

void MainFrame::DrawOnionSkinControls(CharacterView* view)
{
	if (!view || !view->getCharacter() || view->isStageView() || view->isPatEditor()) {
		ImGui::TextDisabled("Onion skin needs a character tab.");
		return;
	}
	auto& o = view->onion();
	bool changed = false;
	changed |= ImGui::Checkbox("Onion skin", &o.enabled);
	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", shortcuts.registry().label(ShortcutAction::toggleOnionSkin).c_str());
	ImGui::SetNextItemWidth(120);
	changed |= ImGui::SliderInt("Before", &o.before, 0, 16);
	ImGui::SetNextItemWidth(120);
	changed |= ImGui::SliderInt("After", &o.after, 0, 16);
	changed |= ImGui::Checkbox("Keyframes (root frame entries)", &o.keyframesOnly);
	if (!o.keyframesOnly) {
		ImGui::SetNextItemWidth(120);
		changed |= ImGui::SliderInt("Spacing (ticks)", &o.spacing, 1, 60);
	}
	changed |= ImGui::Checkbox("Include spawned actors", &o.includeSpawns);
	ImGui::SetNextItemWidth(120);
	changed |= ImGui::SliderFloat("Opacity", &o.alpha, 0.05f, 1.f, "%.2f");
	ImGui::SetNextItemWidth(120);
	changed |= ImGui::SliderFloat("Falloff", &o.falloff, 0.1f, 1.f, "%.2f");
	changed |= ImGui::ColorEdit3("Past tint", &o.pastTint.x, ImGuiColorEditFlags_NoInputs);
	ImGui::SameLine();
	changed |= ImGui::ColorEdit3("Future tint", &o.futureTint.x, ImGuiColorEditFlags_NoInputs);
	if (o.enabled && m_onionStats.samples > 0)
		ImGui::TextDisabled("%d samples: %.3f ms simulation, %.3f ms total (CPU)",
			m_onionStats.samples, m_onionStats.simMs, m_onionStats.totalMs);
	if (changed) markProjectModified();
}

// The image area of a detached window: draws the view's target and handles
// pan (left drag), zoom to cursor (wheel) and box drawing (right drag).
void MainFrame::DrawDetachedViewSurface(uint64_t hostId, CharacterView* view, ImVec2 mn, ImVec2 mx)
{
	const int w = std::max(1, (int)(mx.x - mn.x));
	const int h = std::max(1, (int)(mx.y - mn.y));
	if (w < 8 || h < 8) return;
	view->surfaceWidth = w;
	view->surfaceHeight = h;
	view->surfaceVisible = true;
	RenderTarget& target = view->renderTarget();
	if (!target.ensure(w, h, false)) return;   // texture must exist for the image

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddImage((ImTextureID)(intptr_t)target.texture(), mn, ImVec2(mn.x + w, mn.y + h), ImVec2(0, 1), ImVec2(1, 0));

	ImGui::SetCursorScreenPos(mn);
	ImGui::InvisibleButton("##view_surface", ImVec2((float)w, (float)h),
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const ImGuiIO& io = ImGui::GetIO();
	auto& cam = view->camera();
	const ImVec2 origin(mn.x + cam.panX + w / 2.f, mn.y + cam.panY + h / 2.f);

	if (ImGui::IsItemHovered() && io.MouseWheel != 0.f) {
		// Zoom around the cursor: keep the world point under it fixed.
		const float z0 = cam.zoom;
		const float z1 = std::clamp(z0 + (((io.MouseWheel > 0) != gSettings.invertWheelZoom) ? 0.25f : -0.25f), 0.25f, 20.f);
		const float wx = (io.MousePos.x - origin.x) / z0, wy = (io.MousePos.y - origin.y) / z0;
		cam.zoom = z1;
		cam.panX = io.MousePos.x - wx * z1 - mn.x - w / 2.f;
		cam.panY = io.MousePos.y - wy * z1 - mn.y - h / 2.f;
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
		cam.panX += io.MouseDelta.x;
		cam.panY += io.MouseDelta.y;
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f)) {
		cam.panX += io.MouseDelta.x;
		cam.panY += io.MouseDelta.y;
	}
	// Right drag draws the selected box, one undo step (as in the main window).
	CharacterInstance* character = view->getCharacter();
	BoxPane* boxPane = view->getBoxPane();
	if (character && boxPane) {
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			if (m_boxDragCharacter) EndBoxDrag();
			character->undoManager.beginTransaction("Draw box");
			m_boxDragCharacter = character;
			m_hostBoxDragView = view->getId();
			boxPane->BoxStart((int)((io.MousePos.x - origin.x) / cam.zoom), (int)((io.MousePos.y - origin.y) / cam.zoom));
		} else if (m_hostBoxDragView == view->getId() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			if (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)
				boxPane->BoxDragWorld(io.MouseDelta.x / cam.zoom, io.MouseDelta.y / cam.zoom);
		} else if (m_hostBoxDragView == view->getId()) {
			m_hostBoxDragView = 0;
			if (m_boxDragCharacter) EndBoxDrag();
		}
	}

	dl->PushClipRect(mn, ImVec2(mn.x + w, mn.y + h), true);
	if (character)
		DrawPresetEffectMarkers(view->getState(), character, dl,
			ImVec2(mn.x + cam.panX + w / 2.f, mn.y + cam.panY + h / 2.f), cam.zoom);
	dl->PopClipRect();
	(void)hostId;
}

// Toolbar of a detached window for its active view.
void MainFrame::DrawDetachedHostToolbar(HostWindow& hw, CharacterView* view)
{
	auto& st = view->getState();
	CharacterInstance* character = view->getCharacter();
	Sequence* seq = character ? character->frameData.get_sequence(st.pattern) : nullptr;
	const int frames = seq ? (int)seq->frames.size() : 0;
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Pattern %d  Frame %d/%d  Tick %d", st.pattern, st.frame, std::max(0, frames - 1), st.currentTick);
	ImGui::SameLine();
	if (ImGui::SmallButton(st.animating ? "Pause" : "Play")) {
		st.animating = !st.animating;
		st.animeSeq = st.pattern;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("<")) StepTick(view, -1);
	ImGui::SameLine();
	if (ImGui::SmallButton(">")) StepTick(view, +1);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90);
	float z = view->getZoom();
	if (ImGui::SliderFloat("##zoom", &z, 0.25f, 20.f, "zoom %.2f")) view->setZoom(z);
	ImGui::SameLine();
	if (ImGui::SmallButton("Center")) { view->camera().panX = 0.f; view->camera().panY = 150.f; }
	ImGui::SameLine();
	if (ImGui::Checkbox("Onion", &view->onion().enabled)) markProjectModified();
	ImGui::SameLine();
	if (ImGui::SmallButton("...##onion")) ImGui::OpenPopup("OnionPopup");
	if (ImGui::BeginPopup("OnionPopup")) { DrawOnionSkinControls(view); ImGui::EndPopup(); }
	ImGui::SameLine();
	if (ImGui::Checkbox("Panes", &hw.showPanes)) markProjectModified();
	ImGui::SameLine();
	if (ImGui::SmallButton("Export PNG...")) { m_exportViewId = view->getId(); m_showExportWindow = true; }
	ImGui::SameLine();
	if (ImGui::SmallButton("To main window"))
		MoveViewToHost(view->getId(), WorkspaceSession::MainHost, std::nullopt);
}

void MainFrame::DrawDetachedHosts()
{
	m_focusedHostId = WorkspaceSession::MainHost;
	std::vector<uint64_t> ids;
	for (const auto& entry : m_hosts) ids.push_back(entry.first);

	for (uint64_t hostId : ids) {
		const WorkspaceSession::Host* sh = m_session.host(hostId);
		if (!sh || sh->tabs.empty()) continue;
		HostWindow& hw = m_hosts[hostId];

		// Always its own native window (never merged back into the main
		// viewport), so it can sit on another monitor.
		ImGuiWindowClass wc;
		wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
		ImGui::SetNextWindowClass(&wc);
		if (hw.applyGeometry) {
			ImGui::SetNextWindowPos(ImVec2(hw.x, hw.y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(hw.w, hw.h), ImGuiCond_Always);
			hw.applyGeometry = false;
		}
		ImGui::SetNextWindowSizeConstraints(ImVec2(320, 240), ImVec2(FLT_MAX, FLT_MAX));
		CharacterView* activeView = findViewById(sh->active);
		const std::string title = (activeView ? activeView->getDisplayName() : std::string("Window")) +
			" - Window " + std::to_string(hostId) + "###hantei_host_" + std::to_string(hostId);
		bool open = true;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
		const bool visible = ImGui::Begin(title.c_str(), &open,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();
		const ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
		if (pos.x != hw.x || pos.y != hw.y || size.x != hw.w || size.y != hw.h) {
			hw.x = pos.x; hw.y = pos.y; hw.w = size.x; hw.h = size.y;
		}
		if (m_focusHostRequest == hostId) {
			ImGui::SetWindowFocus();
			m_focusHostRequest = 0;
		}
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy);

		// Pane windows dock into this window's own dockspace, namespaced by
		// host so the arrangement belongs to the window.
		const std::string ns = "host" + std::to_string(hostId);
		const ImGuiID dockId = ImHashStr(("hantei_host_dock_" + std::to_string(hostId)).c_str());
		if (visible) {
			// Tab bar (no ImGui reordering: order is the session's).
			const ImVec2 barMin = ImGui::GetCursorScreenPos();
			if (ImGui::BeginTabBar("##host_tabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
				DrawHostTabs(hostId);
				if (ImGui::BeginPopup("ViewContextMenu")) { DrawViewTabContextItems(); ImGui::EndPopup(); }
				ImGui::EndTabBar();
			}
			const ImRect barRect(barMin, ImVec2(barMin.x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y));
			DrawTabBarDropTarget(hostId, barRect);

			activeView = sh ? findViewById(sh->active) : nullptr;
			if (activeView) {
				DrawDetachedHostToolbar(hw, activeView);
				const ImVec2 avail = ImGui::GetContentRegionAvail();
				ImVec2 surfMin = ImGui::GetCursorScreenPos();
				ImVec2 surfMax(surfMin.x + avail.x, surfMin.y + avail.y);
				if (hw.showPanes) {
					if (!ImGui::DockBuilderGetNode(dockId)) {
						ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
						ImGui::DockBuilderSetNodeSize(dockId, avail);
						ImGuiID center = dockId;
						const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.28f, nullptr, &center);
						const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.38f, nullptr, &center);
						const ImGuiID down = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);
						ImGui::DockBuilderDockWindow(("Left Pane###" + ns + "_Left Pane").c_str(), left);
						ImGui::DockBuilderDockWindow(("Right Pane###" + ns + "_Right Pane").c_str(), right);
						ImGui::DockBuilderDockWindow(("Box Pane###" + ns + "_Box Pane").c_str(), down);
						ImGui::DockBuilderFinish(dockId);
					}
					ImGui::DockSpace(dockId, avail, ImGuiDockNodeFlags_PassthruCentralNode |
						ImGuiDockNodeFlags_NoDockingInCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);
					if (ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockId)) {
						surfMin = central->Pos;
						surfMax = ImVec2(central->Pos.x + central->Size.x, central->Pos.y + central->Size.y);
					}
				} else if (ImGui::DockBuilderGetNode(dockId)) {
					// Keep the saved layout alive while the panes are hidden.
					ImGui::DockSpace(dockId, ImVec2(1, 1), ImGuiDockNodeFlags_KeepAliveOnly);
				}
				DrawDetachedViewSurface(hostId, activeView, surfMin, surfMax);
			}
			DrawTabBarDropTarget(hostId, ImGui::GetCurrentWindow()->Rect());
		}
		ImGui::End();

		if (visible && activeView && hw.showPanes && m_session.host(hostId))
			DrawViewPanes(activeView, ns);

		if (focused && activeView) {
			m_focusedHostId = hostId;
			shortcuts.claimFocus(ShortcutContext::characterView, activeView->getId());
		}
		if (!open) {
			// Closing a window returns its tabs to the main window; closing
			// tabs (and their unsaved-changes prompt) stays a per-tab action.
			const WorkspaceSession::Host* h = m_session.host(hostId);
			const std::vector<uint64_t> tabs = h ? h->tabs : std::vector<uint64_t>{};
			for (uint64_t id : tabs) m_session.move(id, WorkspaceSession::MainHost, std::nullopt, false);
			SyncWorkspaceSession();
			markProjectModified();
		}
	}
}

// Render the active view of every visible detached window into its target
// (called from DrawBack, after the main window's pass).
void MainFrame::RenderDetachedViewTargets()
{
	for (const auto& entry : m_hosts) {
		const WorkspaceSession::Host* sh = m_session.host(entry.first);
		CharacterView* view = sh ? findViewById(sh->active) : nullptr;
		if (!view || !view->surfaceVisible || !view->getCharacter()) continue;
		view->surfaceVisible = false;
		RenderTarget& target = view->renderTarget();
		if (!target.ensure(view->surfaceWidth, view->surfaceHeight, false)) continue;
		ScopedTargetBinding bind(target);
		bind.clear(clearColor[0], clearColor[1], clearColor[2], 1.f, true);
		Render::PassParams pass;
		pass.width = target.width();
		pass.height = target.height();
		pass.zoom = view->camera().zoom;
		pass.originX = view->camera().panX + pass.width / 2.f;
		pass.originY = view->camera().panY + pass.height / 2.f;
		DrawCharacterScene(view, pass, SceneOptions{});
	}
}

// "View" menu: onion skin, PNG export and the window workspace.
void MainFrame::DrawRenderMenu()
{
	if (!ImGui::BeginMenu("View")) return;
	CharacterView* view = getActiveView();
	if (ImGui::BeginMenu("Onion skin", view && view->getCharacter() && !view->isPatEditor())) {
		DrawOnionSkinControls(view);
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Export PNG...", shortcuts.registry().label(ShortcutAction::exportSequencePng).c_str(),
		m_showExportWindow, view && view->getCharacter() != nullptr)) {
		m_showExportWindow = !m_showExportWindow;
		if (view) m_exportViewId = view->getId();
	}
	if (ImGui::MenuItem("Export current frame", shortcuts.registry().label(ShortcutAction::exportFramePng).c_str(),
		false, view && view->getCharacter() != nullptr)) {
		m_exportViewId = view->getId();
		m_exportRequest = ExportRequest::quickFrame;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Move tab to new window", shortcuts.registry().label(ShortcutAction::detachView).c_str(),
		false, IsDetachableView(view))) {
		const ImVec2 p = ImGui::GetMainViewport()->Pos;
		DetachViewToNewHost(view->getId(), ImVec2(p.x + 140.f, p.y + 120.f));
	}
	if (!m_hosts.empty()) {
		for (const auto& [hostId, hw] : m_hosts) {
			const WorkspaceSession::Host* h = m_session.host(hostId);
			CharacterView* v = h ? findViewById(h->active) : nullptr;
			const std::string label = "Window " + std::to_string(hostId) + ": " +
				(v ? v->getDisplayName() : std::string("(empty)")) +
				(h && h->tabs.size() > 1 ? " +" + std::to_string(h->tabs.size() - 1) : std::string());
			if (ImGui::MenuItem(label.c_str())) m_focusHostRequest = hostId;
		}
		if (ImGui::MenuItem("Return all tabs to the main window")) {
			for (uint64_t hostId : m_session.hostIds()) {
				if (hostId == WorkspaceSession::MainHost) continue;
				const WorkspaceSession::Host* h = m_session.host(hostId);
				const std::vector<uint64_t> tabs = h ? h->tabs : std::vector<uint64_t>{};
				for (uint64_t id : tabs) m_session.move(id, WorkspaceSession::MainHost, std::nullopt, false);
			}
			SyncWorkspaceSession();
			markProjectModified();
		}
	}
	ImGui::Separator();
	bool detachable = gSettings.detachableWindows;
	if (ImGui::MenuItem("Native detached windows (restart)", nullptr, &detachable)) {
		gSettings.detachableWindows = detachable;
		ImGui::MarkIniSettingsDirty();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Detached tabs open as their own OS windows that can move to other monitors.\n"
			"Off: they stay inside the main window. Takes effect after a restart.%s",
			WorkspaceViewports::IsEnabled() ? "" : "\n(Currently off.)");
	ImGui::EndMenu();
}

// ---- Project persistence ---------------------------------------------------

std::string MainFrame::SerializeWorkspace() const
{
	nlohmann::json ws;
	ws["version"] = 1;
	ws["next_host_id"] = m_nextHostId;
	nlohmann::json hosts = nlohmann::json::array();
	for (uint64_t hostId : m_session.hostIds()) {
		const WorkspaceSession::Host* h = m_session.host(hostId);
		if (!h) continue;
		nlohmann::json hj;
		hj["id"] = hostId;
		hj["tabs"] = h->tabs;
		hj["active"] = h->active;
		if (hostId != WorkspaceSession::MainHost) {
			auto it = m_hosts.find(hostId);
			if (it != m_hosts.end()) {
				hj["x"] = it->second.x; hj["y"] = it->second.y;
				hj["w"] = it->second.w; hj["h"] = it->second.h;
				hj["panes"] = it->second.showPanes;
			}
		}
		hosts.push_back(hj);
	}
	ws["hosts"] = hosts;
	return ws.dump();
}

void MainFrame::RestoreWorkspace(const std::string& text)
{
	m_session.clear();
	m_hosts.clear();
	m_nextHostId = 1;
	nlohmann::json ws;
	try { if (!text.empty()) ws = nlohmann::json::parse(text); } catch (...) { ws = nlohmann::json(); }
	if (ws.is_object() && ws.value("version", 0) == 1 && ws.contains("hosts") && ws["hosts"].is_array()) {
		m_nextHostId = std::max<uint64_t>(1, ws.value("next_host_id", (uint64_t)1));
		for (const auto& hj : ws["hosts"]) {
			const uint64_t hostId = hj.value("id", (uint64_t)0);
			if (!hj.contains("tabs") || !hj["tabs"].is_array()) continue;
			for (const auto& t : hj["tabs"]) {
				const uint64_t id = t.get<uint64_t>();
				CharacterView* v = findViewById(id);
				if (!v || m_session.contains(id)) continue;
				if (hostId != WorkspaceSession::MainHost && !IsDetachableView(v)) {
					m_session.add(id, WorkspaceSession::MainHost, false);
					continue;
				}
				m_session.add(id, hostId, false);
			}
			const uint64_t active = hj.value("active", (uint64_t)0);
			if (m_session.owner(active).value_or(~0ull) == hostId) m_session.select(hostId, active);
			if (hostId != WorkspaceSession::MainHost && m_session.host(hostId)) {
				HostWindow& hw = m_hosts[hostId];
				hw.id = hostId;
				hw.x = hj.value("x", 120.f); hw.y = hj.value("y", 120.f);
				hw.w = std::max(320.f, hj.value("w", 960.f)); hw.h = std::max(240.f, hj.value("h", 720.f));
				hw.showPanes = hj.value("panes", true);
				hw.applyGeometry = true;
				m_nextHostId = std::max(m_nextHostId, hostId + 1);
			}
		}
	}
	SyncWorkspaceSession();
	// The main window keeps showing the project's active view when it lives
	// there; otherwise SyncWorkspaceSession picked the main host's tab.
	if (CharacterView* v = getActiveView())
		if (m_session.owner(v->getId()).value_or(1) == WorkspaceSession::MainHost)
			m_session.select(WorkspaceSession::MainHost, v->getId());
}

// Detached-window keyboard hook (workspace_viewports.cpp subclass).
static bool DetachedWindowKeyHook(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CHAR) {
		// Same rule as the main WndProc: drop the character of a key that
		// already acted as a shortcut while a text field had focus.
		if (MainFrame::s_swallowChar && (wchar_t)wParam == MainFrame::s_swallowChar) {
			MainFrame::s_swallowChar = 0;
			return true;
		}
		return false;
	}
	MainFrame* mf = (MainFrame*)GetWindowLongPtr(mainWindowHandle, GWLP_USERDATA);
	if (!mf || !ImGui::GetCurrentContext()) return false;
	const ImGuiIO& io = ImGui::GetIO();
	return mf->HandleKeys(wParam, (lParam & (1 << 30)) != 0, io.WantCaptureKeyboard, io.WantTextInput);
}

WorkspaceViewports::KeyHook MainFrame::DetachedKeyHook()
{
	return DetachedWindowKeyHook;
}

#endif /* UI_WORKSPACE_HOSTS_IMPL_H_GUARD */
