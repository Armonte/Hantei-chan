#ifndef UI_EDITOR_TOOLS_IMPL_H_GUARD
#define UI_EDITOR_TOOLS_IMPL_H_GUARD

// ============================================================================
// Editing tools: shortcut dispatch, undo/redo, J/K/L transport, box-drag
// transactions and the viewport position tool.
// ============================================================================
// Included at the end of main_frame.cpp (like main_ui_impl.h).

// ---------------------------------------------------------------------------
// Shortcuts
// ---------------------------------------------------------------------------

bool MainFrame::HandleKeys(uint64_t vkey, bool isRepeat, bool imguiWantsKeyboard, bool imguiTextInput)
{
	// A non-text ImGui owner (slider being dragged, modal popup) keeps the
	// keyboard entirely. A focused text field keeps everything except the
	// bindings marked duringTextInput (Save), so its native Ctrl+Z works.
	// Key capture for the Keyboard shortcuts window (issue #9): the next
	// non-modifier key becomes the chord; Esc cancels.
	if (m_keyCapture >= 0) {
		if (vkey == VK_SHIFT || vkey == VK_CONTROL || vkey == VK_MENU || vkey == VK_LSHIFT ||
		    vkey == VK_RSHIFT || vkey == VK_LCONTROL || vkey == VK_RCONTROL || vkey == VK_LWIN || vkey == VK_RWIN)
			return true;
		if (vkey != VK_ESCAPE) {
			ShortcutChord c;
			c.key = (uint32_t)vkey;
			if (GetKeyState(VK_CONTROL) & 0x8000) c.modifiers |= shortcutCtrl;
			if (GetKeyState(VK_SHIFT) & 0x8000) c.modifiers |= shortcutShift;
			if (GetKeyState(VK_MENU) & 0x8000) c.modifiers |= shortcutAlt;
			shortcuts.registry().setBindingChord((size_t)m_keyCapture, c);
			gSettings.keyBindings = shortcuts.registry().serializeOverrides();
			ImGui::MarkIniSettingsDirty();
		}
		m_keyCapture = -1;
		return true;
	}

	bool timelineResizing=false;
	for(auto& v:views) if(v->getBoxPane() && v->getBoxPane()->DurationResizeActive()) timelineResizing=true;
	if(timelineResizing && vkey==VK_ESCAPE) { CancelViewportGestures(); return true; }
	if (imguiWantsKeyboard && !imguiTextInput)
		return false;

	ShortcutChord chord;
	chord.key = (uint32_t)vkey;
	if (GetKeyState(VK_CONTROL) & 0x8000) chord.modifiers |= shortcutCtrl;
	if (GetKeyState(VK_SHIFT) & 0x8000) chord.modifiers |= shortcutShift;
	if (GetKeyState(VK_MENU) & 0x8000) chord.modifiers |= shortcutAlt;

	// A NUMERIC field being typed in (InputInt/InputFloat, Ctrl+click on a drag): Q / E may step keyframes from it
	// (issue #61); a text field (names) keeps every letter.
	bool numericField = false;
	if (imguiTextInput && ImGui::GetCurrentContext()) {
		if (const ImGuiInputTextState* ts = ImGui::GetInputTextState(ImGui::GetActiveID()))
			numericField = (ts->Flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsScientific |
			                             ImGuiInputTextFlags_CharsHexadecimal)) != 0;
	}
	auto input=shortcuts.input(imguiTextInput,numericField,isRepeat); input.mouseDrag |= timelineResizing;
	const ShortcutBinding* binding = shortcuts.resolve(chord, input);
	if (!binding)
		return false;

	// A focused workspace with its own history (e.g. the _c command editor)
	// gets first refusal on every action routed while it owns focus.
	if (shortcuts.dispatchToContext(binding->action))
		return true;

	// Keyframe step from inside a text field (issue #61): done inside the next
	// UI frame (commit the field, step, re-focus it). The key's character
	// (e.g. '*') must not reach the field.
	if (imguiTextInput && (binding->action == ShortcutAction::nextKeyframe ||
	                       binding->action == ShortcutAction::previousKeyframe)) {
		m_textNavDir = binding->action == ShortcutAction::nextKeyframe ? 1 : -1;
		s_swallowChar = vkey == VK_MULTIPLY ? L'*' : vkey == VK_DIVIDE ? L'/'
			: (vkey >= 'A' && vkey <= 'Z') ? (wchar_t)(vkey - 'A' + L'a') : 0;
		return true;
	}
	const bool tickStep=binding->action==ShortcutAction::stepTickBackward || binding->action==ShortcutAction::stepTickForward;
	if(!tickStep) m_scrubHeldDir=0;
	if(tickStep && isRepeat) return true; // Editor owns held timing, not OS autorepeat.
	const bool handled=RunShortcut(binding->action);
	if(tickStep && handled) {
		m_scrubHeldView=getShortcutView();m_scrubHeldChord=chord;
		m_scrubHeldDir=binding->action==ShortcutAction::stepTickBackward?-1:1;
		m_scrubNextTime=ImGui::GetTime()+gSettings.scrubHoldDelayMs/1000.;
	}
	return handled;
}

bool MainFrame::RunShortcut(ShortcutAction action)
{
	// The active tab of the window that owns focus (main or detached).
	auto* view = getShortcutView();

	switch (action) {
	case ShortcutAction::undo:
	case ShortcutAction::redo:
		// The command workspace owns its own history; never let its focus
		// fall through to the character's undo stack.
		if (shortcuts.focused() == ShortcutContext::commands || shortcuts.focused() == ShortcutContext::script)
			return false;
		if (m_posDrag.active) EndPositionDrag(false);
		return PerformUndoRedo(view, action == ShortcutAction::redo);

	case ShortcutAction::save: {
		// Ctrl+S saves the character being edited, and with a project open also
		// writes the .hproj. It used to save only the .hproj when a project was
		// loaded, so character edits were silently left unsaved (issue #80).
		bool did = false;
		// The focused window's character (main or detached).
		auto* active = view ? view->getCharacter() : nullptr;
		// A character with no file yet is skipped when a project is open (the
		// project save still runs); without a project the error explains it.
		if (active && (!active->getTopHA6Path().empty() || active->isPatDocument() || !ProjectManager::HasCurrentProject())) {
			saveCharacter(active);
			did = true;
		}
		if (ProjectManager::HasCurrentProject()) {
			saveProject();
			did = true;
		}
		return did;
	}
	case ShortcutAction::saveProjectAs:
		saveProjectAs();
		return true;
	case ShortcutAction::openProject:
		openProject();
		return true;
	case ShortcutAction::newProject:
		newProject();
		return true;
	case ShortcutAction::nextView:
	case ShortcutAction::previousView: {
		// Cycle the tabs of the focused window (main or detached).
		const uint64_t hostId = view ? m_session.owner(view->getId()).value_or(WorkspaceSession::MainHost)
		                             : WorkspaceSession::MainHost;
		const WorkspaceSession::Host* host = m_session.host(hostId);
		if (!host || host->tabs.empty()) return false;
		const size_t n = host->tabs.size();
		auto it = std::find(host->tabs.begin(), host->tabs.end(), host->active);
		const size_t cur = it == host->tabs.end() ? 0 : (size_t)(it - host->tabs.begin());
		const size_t next = it == host->tabs.end() ? 0
			: action == ShortcutAction::nextView ? (cur + 1) % n : (cur + n - 1) % n;
		const int index = findViewIndexById(host->tabs[next]);
		if (index >= 0) setActiveView(index);
		return true;
	}
	case ShortcutAction::reopenClosedView:
		return reopenClosedTab();
	case ShortcutAction::closeView:
		if (view) {
			tryCloseView(findViewIndexById(view->getId()));
			return true;
		}
		return false;

	case ShortcutAction::previousPattern: if (!view) return false; AdvancePattern(view, -1); return true;
	case ShortcutAction::nextPattern:     if (!view) return false; AdvancePattern(view, 1);  return true;
	case ShortcutAction::previousKeyframe:if (!view) return false; AdvanceFrame(view, -1);   return true;
	case ShortcutAction::nextKeyframe:    if (!view) return false; AdvanceFrame(view, 1);    return true;
	case ShortcutAction::previousBox:
		if (view && view->getBoxPane()) view->getBoxPane()->AdvanceBox(-1);
		return view != nullptr;
	case ShortcutAction::nextBox:
		if (view && view->getBoxPane()) view->getBoxPane()->AdvanceBox(+1);
		return view != nullptr;

	case ShortcutAction::nudgeLayerLeft:      return NudgeLayer(view, -1, 0);
	case ShortcutAction::nudgeLayerRight:     return NudgeLayer(view, +1, 0);
	case ShortcutAction::nudgeLayerUp:        return NudgeLayer(view, 0, -1);
	case ShortcutAction::nudgeLayerDown:      return NudgeLayer(view, 0, +1);
	case ShortcutAction::nudgeLayerLeftFast:  return NudgeLayer(view, -10, 0);
	case ShortcutAction::nudgeLayerRightFast: return NudgeLayer(view, +10, 0);
	case ShortcutAction::nudgeLayerUpFast:    return NudgeLayer(view, 0, -10);
	case ShortcutAction::nudgeLayerDownFast:  return NudgeLayer(view, 0, +10);
	case ShortcutAction::toggleSpawnPreview:
		if (!view) return false;
		view->getState().vizSettings.showSpawnedPatterns = !view->getState().vizSettings.showSpawnedPatterns;
		view->getState().forceSpawnTreeRebuild = true;
		return true;
	case ShortcutAction::previousOpponentPreview: return view && CycleOpponent(view,-1);
	case ShortcutAction::nextOpponentPreview: return view && CycleOpponent(view,1);
	case ShortcutAction::previousPositionTarget:
	case ShortcutAction::nextPositionTarget:
		if(!view) return false;
		return CyclePositionTarget(action==ShortcutAction::nextPositionTarget?1:-1,0,0,false,view);
	case ShortcutAction::togglePositionSolo:
		return view && TogglePositionSolo(view);
	case ShortcutAction::openOpponentPreviewSettings:
		if(!view) return false;
		m_opponentSettingsView=view->getId(); m_showOpponent=!m_showOpponent; return true;

	// J/K/L: J plays in reverse, K stops (or resumes forward when stopped),
	// J/L step; Shift+J/L run continuous transport.
	case ShortcutAction::playReverse:
		if (!view || !view->getCharacter()) return false;
		view->getState().animating = false;
		m_reverseView = view;
		return true;
	case ShortcutAction::playForward:
		if (!view || !view->getCharacter()) return false;
		if (m_reverseView == view) m_reverseView = nullptr;
		if (!view->getState().animating) {
			auto& st = view->getState();
			st.animating = true;
			st.animeSeq = st.pattern;
			st.previousFrame = -1;
		}
		return true;
	case ShortcutAction::togglePlayback:
		if (!view || !view->getCharacter()) return false;
		if (view->getState().animating || m_reverseView == view) {
			StopTransport(view);
		} else {
			auto& st = view->getState();
			st.animating = true;
			st.animeSeq = st.pattern;
			st.previousFrame = -1;
		}
		return true;
	case ShortcutAction::stepTickBackward:
		if (!view || !view->getCharacter()) return false;
		m_reverseView=nullptr;
		StepTick(view, -1);
		return true;
	case ShortcutAction::stepTickForward:
		if (!view || !view->getCharacter()) return false;
		m_reverseView=nullptr;
		StepTick(view, +1);
		return true;

	case ShortcutAction::cancelGesture:
		for(auto& v:views) if(v->getBoxPane() && v->getBoxPane()->DurationResizeActive()) { v->getBoxPane()->EndDurationResize(true); return true; }
		if (m_posDrag.active) {
			EndPositionDrag(true);
			return true;
		}
		if (m_uvDrag.active) {
			EndUvDrag(true);
			return true;
		}
		return false;

	// View toggles (sosfiro F / L / H)
	case ShortcutAction::toggleHideUi:
		m_hideUi = !m_hideUi;
		return true;
	case ShortcutAction::toggleAxis:
		m_showAxis = !m_showAxis;
		return true;
	case ShortcutAction::toggleBoxes:
		m_showBoxes = !m_showBoxes;
		return true;
	case ShortcutAction::togglePartHighlight:
		if (!view || !view->isPatEditor() || !view->getPartSetPane()) return false;
		view->getPartSetPane()->UpdateHighligth(!view->getPartSetPane()->activeHighligth);
		gSettings.patHighlight = view->getPartSetPane()->activeHighligth;
		ImGui::MarkIniSettingsDirty();
		return true;
	case ShortcutAction::toggleTextureView:
		if (!view || !view->isPatEditor()) return false;
		TogglePatRenderMode(view, RenderMode::TEXTURE_VIEW);
		return true;
	case ShortcutAction::toggleUvView:
		if (!view || !view->isPatEditor()) return false;
		TogglePatRenderMode(view, RenderMode::UV_SETTING_VIEW);
		return true;

	case ShortcutAction::toggleOnionSkin:
		if (!view || !view->getCharacter() || view->isPatEditor()) return false;
		view->onion().enabled = !view->onion().enabled;
		markProjectModified();
		return true;
	case ShortcutAction::exportFramePng:
		if (!view || !view->getCharacter()) return false;
		m_exportViewId = view->getId();
		m_exportRequest = ExportRequest::quickFrame;
		return true;
	case ShortcutAction::exportSequencePng:
		if (!view || !view->getCharacter()) return false;
		m_exportViewId = view->getId();
		m_showExportWindow = true;
		return true;
	case ShortcutAction::detachView: {
		if (!view) return false;
		const auto owner = m_session.owner(view->getId()).value_or(WorkspaceSession::MainHost);
		if (owner != WorkspaceSession::MainHost) {
			MoveViewToHost(view->getId(), WorkspaceSession::MainHost, std::nullopt);
			return true;
		}
		const ImVec2 p = ImGui::GetMainViewport()->Pos;
		return DetachViewToNewHost(view->getId(), ImVec2(p.x + 140.f, p.y + 120.f)) != 0;
	}

	default:
		return false;
	}
}

// Ctrl+arrows: move the selected layer of the current keyframe (AF offset X/Y).
bool MainFrame::NudgeLayer(CharacterView* view, int dx, int dy)
{
	if (!view || view->isStageView()) return false;
	if (view->isPatEditor()) return PatNudge(view, dx, dy);   // PAT tab: the selected part (sosfiro Ctrl+arrows)
	CharacterInstance* character = view->getCharacter();
	if (!character) return false;
	auto& st = view->getState();
	if (st.animating) return false;
	Sequence* seq = character->frameData.get_sequence(st.pattern);
	if (!seq || st.frame < 0 || st.frame >= (int)seq->frames.size()) return false;
	auto& layers = seq->frames[st.frame].AF.layers;
	if (layers.empty()) layers.push_back({});
	const int li = std::clamp(st.selectedLayer, 0, (int)layers.size() - 1);
	layers[li].offset_x += dx;
	layers[li].offset_y += dy;
	character->frameData.mark_modified(st.pattern);
	character->markModified();
	character->undoManager.markModified();
	return true;
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

bool MainFrame::isLiveCharacter(const CharacterInstance* character) const
{
	if (!character) return false;
	for (const auto& c : characters)
		if (c.get() == character) return true;
	return false;
}

bool MainFrame::PerformUndoRedo(CharacterView* view, bool redo)
{
	auto* active = view ? view->getCharacter() : nullptr;
	if (!view || !active || view->isStageView())
		return false;

	// Close any viewport gesture first so it is a complete step.
	if (m_boxDragCharacter) EndBoxDrag();
	if (m_uvDrag.active) EndUvDrag(false);

	auto& undo = active->undoManager;
	const UndoManager::Entry* entry = redo ? undo.redo() : undo.undo();
	if (entry)
		RefreshViewsAfterHistory(view, active, entry);

	if (undo.isClean()) active->clearModified();
	else active->markModified();
	return true;
}

void MainFrame::RefreshViewsAfterHistory(CharacterView* activeView, CharacterInstance* character, const UndoManager::Entry* entry)
{
	const int count = character->frameData.get_sequence_count();

	// Navigate the active view to the change when it is not already showing
	// one of the affected patterns (e.g. undoing a cross-pattern paste).
	if (activeView && activeView->getCharacter() == character && entry) {
		auto& st = activeView->getState();
		bool showingChange = false;
		for (const auto& d : entry->changes)
			if (d.index == st.pattern) { showingChange = true; break; }
		if (!showingChange && entry->focusPattern >= 0 && entry->focusPattern < count) {
			st.pattern = entry->focusPattern;
			st.frame = entry->focusFrame >= 0 ? entry->focusFrame : 0;
		} else if (showingChange && entry->focusPattern == st.pattern && entry->focusFrame >= 0) {
			st.frame = entry->focusFrame;
		}
	}

	for (auto& v : views) {
		if (v->getCharacter() != character) continue;
		auto& st = v->getState();
		if (st.pattern >= count) st.pattern = count > 0 ? count - 1 : 0;
		Sequence* seq = character->frameData.get_sequence(st.pattern);
		const int frames = seq ? (int)seq->frames.size() : 0;
		if (st.frame >= frames) st.frame = frames > 0 ? frames - 1 : 0;
		if (st.frame < 0) st.frame = 0;
		st.currentTick = frames > 0 ? CalculateTickFromFrame(&character->frameData, st.pattern, st.frame) : 0;
		st.spriteId = -1;
		st.activeSpawns.clear();
		st.forceSpawnTreeRebuild = true;
		if (v->getMainPane()) v->getMainPane()->RegenerateNames();
	}
}

// ---------------------------------------------------------------------------
// Box drawing transaction
// ---------------------------------------------------------------------------

void MainFrame::EndBoxDrag()
{
	CharacterInstance* c = m_boxDragCharacter;
	m_boxDragCharacter = nullptr;
	if (!isLiveCharacter(c)) return;
	if (c->undoManager.commitTransaction())
		c->markModified();
}

// ---------------------------------------------------------------------------
// J/K/L transport
// ---------------------------------------------------------------------------

void MainFrame::StopTransport(CharacterView* view)
{
	if (!view) return;
	view->getState().animating = false;
	if (m_reverseView == view) m_reverseView = nullptr;
}

void MainFrame::StepTick(CharacterView* view, int dir)
{
	if (!view) return;
	CharacterInstance* character = view->getCharacter();
	if (!character) return;
	auto& st = view->getState();
	st.animating = false;
	if (m_reverseView == view && dir > 0) m_reverseView = nullptr;

	Sequence* seq = character->frameData.get_sequence(st.pattern);
	if (!seq || seq->frames.empty()) return;

	int tick = st.currentTick + dir;
	if (tick < 0) tick = 0;
	FrameData* effectData = character->effectCharacter ? &character->effectCharacter->frameData : nullptr;
	if(st.absoluteScrub) { st.ScrubTimeline(&character->frameData,effectData,st.AuthoredTick(&character->frameData)+dir); return; }
	auto& sim = st.BindPreviewSim(&character->frameData, effectData);
	if (tick > sim.horizon()) tick = sim.horizon();
	sim.ensureSimulatedTo(tick);
	const auto& track = sim.rootFrameTrack();
	st.SeekTimelineTick(&character->frameData,effectData,tick,"keyboard step");
	st.frame = std::clamp(st.frame, 0, (int)seq->frames.size() - 1);
}

void MainFrame::UpdateTransport()
{
	if(m_scrubHeldDir) {
		const auto& io=ImGui::GetIO();
		unsigned mods=((GetAsyncKeyState(VK_CONTROL)&0x8000)?shortcutCtrl:0)|((GetAsyncKeyState(VK_SHIFT)&0x8000)?shortcutShift:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?shortcutAlt:0);
		if(m_scrubHeldView!=getShortcutView() || io.AppFocusLost || io.WantTextInput || ImGui::IsAnyItemActive() ||
			!(GetAsyncKeyState(m_scrubHeldChord.key)&0x8000) || mods!=m_scrubHeldChord.modifiers) m_scrubHeldDir=0;
		else if(ImGui::GetTime()>=m_scrubNextTime) {
			StepTick(m_scrubHeldView,m_scrubHeldDir);
			m_scrubNextTime=ImGui::GetTime()+gSettings.scrubRepeatMs/1000.;
		}
	}
	for(auto& v:views) if(auto* box=v->getBoxPane(); box && box->DurationResizeActive()) {
		if(!box->isVisible || v.get()!=getToolView() || ImGui::GetIO().AppFocusLost) box->EndDurationResize(true);
	}
	if (!m_reverseView) return;
	// Reverse playback belongs to one view; switching tabs or closing it stops it.
	if (m_reverseView != getShortcutView()) {
		m_reverseView = nullptr;
		return;
	}
	auto& st = m_reverseView->getState();
	auto position=[&]() {return st.absoluteScrub?st.AuthoredTick(&m_reverseView->getCharacter()->frameData):st.currentTick;};
	if (st.animating || position() <= 0) {
		// Forward playback took over, or we reached the start.
		m_reverseView = nullptr;
		return;
	}
	StepTick(m_reverseView, -1);
	m_reverseView = position() > 0 ? m_reverseView : nullptr;
}

// ---------------------------------------------------------------------------
// Viewport position tool
// ---------------------------------------------------------------------------
// Handles for the current keyframe's animation layers (AF offset X/Y) and for
// position-type effects (spawn 1/101/1000, 8/108, random 11/111 base offset,
// preset 3). Layer handles sit where the renderer places the layer origin
// (scale and XYZ rotation applied, AFRT order respected) and a screen drag is
// mapped back through the inverse of that 2x2 transform. Effect handles use
// the preview simulator's spawn placement (preview::ResolveSpawnPlacement):
// facing propagation + bit-11 toggle mirror only the effect's own X, PAT
// owner frames halve the offset, and the camera / screen-edge / opponent /
// fixed positioning flags are honoured, so a handle sits exactly where the
// viewport draws the spawned actor. A drag is one undo transaction; Escape
// restores the pre-drag values.

static const char* PositionEffectName(int type)
{
	switch (type) {
	case 1:    return TXT("Spawn pattern");
	case 101:  return TXT("Spawn relative pattern");
	case 1000: return TXT("Spawn pattern once");
	case 8:    return TXT("Spawn actor (effect.ha6)");
	case 108:  return TXT("Spawn relative actor (effect.ha6)");
	case 3:    return TXT("Preset effect");
	case 11:   return TXT("Spawn random pattern");
	case 111:  return TXT("Spawn random relative pattern");
	default:   return TXT("Effect");
	}
}

std::vector<MainFrame::PositionTarget> MainFrame::CollectPositionTargets(CharacterView* view)
{
	std::vector<PositionTarget> out;
	if(!view) view=getActiveView();
	auto* active = view?view->getCharacter():nullptr;
	if (!view || !active || view->isStageView() || view->isPatEditor()) return out;
	auto* box = view->getBoxPane();
	if (!box || !box->positionTool) return out;

	auto& st = view->getState();
	Sequence* seq = active->frameData.get_sequence(st.pattern);
	if (!seq || st.frame < 0 || st.frame >= (int)seq->frames.size()) return out;
	Frame& frame = seq->frames[st.frame];

	const float s = view->getZoom();
	const float baseX = view->camera().panX + SurfaceFor(view).size.x / 2;
	const float baseY = view->camera().panY + SurfaceFor(view).size.y / 2;
	constexpr float tau = glm::pi<float>() * 2.f;

	for (int i = 0; i < (int)frame.AF.layers.size(); ++i) {
		const auto& L = frame.AF.layers[i];
		if (L.spriteId < 0 && !L.usePat) continue;
		glm::mat4 m(1.f);
		m = glm::scale(m, glm::vec3(L.scale[0], L.scale[1], 1.f));
		if (frame.AF.AFRT && !L.usePat) {
			m = glm::rotate(m, L.rotation[0] * tau, glm::vec3(1, 0, 0));
			m = glm::rotate(m, L.rotation[1] * tau, glm::vec3(0, 1, 0));
			m = glm::rotate(m, L.rotation[2] * tau, glm::vec3(0, 0, 1));
		} else {
			m = glm::rotate(m, L.rotation[2] * tau, glm::vec3(0, 0, 1));
			m = glm::rotate(m, L.rotation[1] * tau, glm::vec3(0, 1, 0));
			m = glm::rotate(m, L.rotation[0] * tau, glm::vec3(1, 0, 0));
		}
		PositionTarget t;
		t.kind = PositionTarget::Layer;
		t.index = i;
		t.m[0] = m[0][0] * s; t.m[1] = m[0][1] * s;   // column 0
		t.m[2] = m[1][0] * s; t.m[3] = m[1][1] * s;   // column 1
		const float det = t.m[0] * t.m[3] - t.m[2] * t.m[1];
		t.invertible = std::fabs(det) > 1e-4f * s * s;
		t.screen = ImVec2(baseX + t.m[0] * L.offset_x + t.m[2] * L.offset_y,
		                  baseY + t.m[1] * L.offset_x + t.m[3] * L.offset_y);
		t.label = std::string(TXT("Layer")) + " " + std::to_string(i) + (L.usePat ? " (PAT " : TXT(" (sprite ")) +
			std::to_string(L.spriteId) + ")";
		t.color = IM_COL32(80, 220, 255, 255);
		out.push_back(std::move(t));
	}

	// The keyframe's owner is the root actor: world origin, preview facing.
	FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
	const preview::Options& simOpt = st.BindPreviewSim(&active->frameData, effectData).options();
	preview::SimActor owner;
	owner.isRoot = true;
	owner.facingLeft = simOpt.rootFacingLeft;
	const bool ownerUsesPat = !frame.AF.layers.empty() && frame.AF.layers[0].usePat;

	for (int i = 0; i < (int)frame.EF.size(); ++i) {
		const Frame_EF& ef = frame.EF[i];
		int xp, yp;
		if (!preview::SpawnOffsetSlots(ef, xp, yp)) continue;
		int fs1, fs2;
		preview::SpawnFlagsets(ef, fs1, fs2);
		const preview::SpawnPlacement pl = preview::ResolveSpawnPlacement(simOpt, owner, ownerUsesPat, fs1, fs2);
		PositionTarget t;
		t.kind = PositionTarget::Effect;
		t.index = i;
		t.xParam = xp; t.yParam = yp;
		t.m[0] = pl.kx * s; t.m[1] = 0; t.m[2] = 0; t.m[3] = pl.ky * s;
		t.invertible = std::fabs(pl.kx) > 1e-4f && std::fabs(pl.ky) > 1e-4f;
		t.screen = ImVec2(baseX + (pl.baseX + pl.kx * ef.parameters[xp]) * s,
		                  baseY + (pl.baseY + pl.ky * ef.parameters[yp]) * s);
		t.label = "EF " + std::to_string(i) + ": " + PositionEffectName(ef.type) + " " + std::to_string(ef.number);
		if (pl.facingLeft) t.label += TXT(" (faces left)");
		if (pl.scale != 1.f) t.label += TXT(" (PAT owner: half offset)");
		t.color = ef.type == 3 ? IM_COL32(255, 150, 40, 255) : IM_COL32(255, 210, 60, 255);
		out.push_back(std::move(t));
	}
	preview::TickState ts;
	st.BindPreviewSim(&active->frameData,effectData).getStateAt(st.PreviewTick(&active->frameData),ts);
	const auto* root=ts.root();
	if(root) for(const auto& actor:ts.actors) {
		if(actor.isRoot || actor.isPreset || actor.isScript || actor.srcEffectHa6 || actor.parentId!=root->id ||
			actor.srcPattern!=st.pattern || actor.srcFrame==st.frame) continue;
		auto* source=active->frameData.get_sequence(actor.srcPattern);
		if(!source || actor.srcFrame<0 || actor.srcFrame>=int(source->frames.size())) continue;
		const auto& sf=source->frames[actor.srcFrame];
		if(actor.srcEffectIndex<0 || actor.srcEffectIndex>=int(sf.EF.size())) continue;
		const auto& ef=sf.EF[actor.srcEffectIndex]; int xp,yp;
		if(!preview::SpawnOffsetSlots(ef,xp,yp)) continue;
		// Use the active instance's resolved facing/owner scale, not the later state's AF.
		PositionTarget t; t.kind=PositionTarget::PersistentEffect; t.index=actor.srcEffectIndex;
		t.sourcePattern=actor.srcPattern; t.sourceFrame=actor.srcFrame; t.instance=actor.id;
		t.xParam=xp; t.yParam=yp;
		t.m[0]=(actor.facingLeft?-1.f:1.f)*actor.offsetScale*s; t.m[1]=t.m[2]=0; t.m[3]=actor.offsetScale*s;
		t.invertible=std::fabs(t.m[0]*t.m[3])>1e-6f;
		t.screen=ImVec2(baseX+(actor.x-root->x)*s,baseY+(actor.y-root->y)*s);
		t.label="Spawn "+std::to_string(actor.id)+" -> pattern "+std::to_string(t.sourcePattern)+" state "+std::to_string(t.sourceFrame)+" EF "+std::to_string(t.index);
		t.color=IM_COL32(255,190,70,255); out.push_back(std::move(t));
	}
	if(SurfaceFor(view).opponentReady && SurfaceFor(view).opponentVersion==active->frameData.dataVersion &&
		SurfaceFor(view).pattern==st.pattern && SurfaceFor(view).frame==st.frame && SurfaceFor(view).tick==st.PreviewTick(&active->frameData))
		out.push_back(SurfaceFor(view).opponentTarget);
	return out;
}

bool MainFrame::PositionSpriteHit(int kind,int index,float x,float y,const CharacterView* view) const
{
 if(!view) view=getActiveView();
 const auto* surface=FindSurface(view);
 if(!view || !view->getCharacter() || !surface || surface->pattern!=view->getState().pattern ||
 surface->frame!=view->getState().frame || surface->tick!=view->getState().PreviewTick(&view->getCharacter()->frameData)) return false;
 for(const auto& p:surface->footprints)
  if(p.kind==kind && p.index==index && p.hit(x,y)) return true;
 return false;
}

positionpick::Key MainFrame::PositionKey(const PositionTarget& t,const CharacterView* view) const
{
	if(!view) view=getActiveView();
	if(!view) return {};
	const auto& st=view->getState();
	return {int(t.kind),t.index,t.instance,t.sourcePattern>=0?t.sourcePattern:st.pattern,t.sourceFrame>=0?t.sourceFrame:st.frame};
}

std::vector<positionpick::Key> MainFrame::PositionHits(const std::vector<PositionTarget>& targets,float x,float y,bool sprites,CharacterView* view) const
{
	const auto* surface=FindSurface(view); if(!surface) return {};
	struct Hit { positionpick::Key key; int rank; float distance; bool sprite; };
	std::vector<Hit> hits;
	for(const auto& t:targets) {
		const float dx=t.screen.x-x,dy=t.screen.y-y,d2=dx*dx+dy*dy;
		const bool sprite=sprites && PositionSpriteHit(t.kind,t.instance>=0?t.instance:t.index,x,y,view);
		if(!sprite && d2>81.f) continue;
		int rank=-1;
		if(sprite) for(int j=0;j<int(surface->footprints.size());++j) {
			const auto& p=surface->footprints[j];
			if(p.kind==t.kind && p.index==(t.instance>=0?t.instance:t.index) && p.hit(x,y)) rank=j;
		}
		hits.push_back({PositionKey(t,view),rank,d2,sprite});
	}
	std::stable_sort(hits.begin(),hits.end(),[](const Hit& a,const Hit& b) {
		if(a.sprite!=b.sprite) return a.sprite;
		return a.sprite?a.rank>b.rank:a.distance<b.distance;
	});
	std::vector<positionpick::Key> keys; for(const auto& hit:hits) keys.push_back(hit.key); return keys;
}

positionpick::Key MainFrame::PickPosition(const std::vector<PositionTarget>& targets,float x,float y,bool sprites,int direction,CharacterView* view)
{
	if(!view) view=getActiveView();
	if(!view || !view->getCharacter()) return {};
	auto& st=view->getState(); auto& pick=st.positionSelection;
	pick.sync(st.pattern,st.frame,st.PreviewTick(&view->getCharacter()->frameData),view->getCharacter()->frameData.dataVersion);
	const auto before=pick.selected;
	const auto result=pick.choose(PositionHits(targets,x,y,sprites,view),direction);
	if(!(before==pick.selected)) HC_LOG_DEBUG("position pick view={} pattern={} state={} tick={} version={} xy={:.1f},{:.1f} direction={} candidates={} kind={} index={} instance={} source={}/{}",(unsigned long long)view->getId(),st.pattern,st.frame,pick.tick,(unsigned long long)pick.version,x,y,direction,pick.candidates.size(),pick.selected.kind,pick.selected.index,pick.selected.instance,pick.selected.pattern,pick.selected.state);
	return result;
}

bool MainFrame::CyclePositionTarget(int direction,int x,int y,bool underCursor,CharacterView* view)
{
	if(!view) view=getActiveView();
	if(!view || m_posDrag.active) return false;
	auto targets=CollectPositionTargets(view); if(targets.empty()) return false;
	if(underCursor) { PickPosition(targets,float(x),float(y),true,direction,view); return true; }
	auto& st=view->getState(); auto& pick=st.positionSelection;
	pick.sync(st.pattern,st.frame,st.PreviewTick(&view->getCharacter()->frameData),view->getCharacter()->frameData.dataVersion);
	const auto keys=pick.candidates; if(keys.empty()) return false;
	pick.choose(keys,direction); return true;
}

bool MainFrame::TogglePositionSolo(CharacterView* view)
{
	if(!view) view=getActiveView();
	if(!view || m_posDrag.active || CollectPositionTargets(view).empty()) return false;
	auto& st=view->getState(); auto& pick=st.positionSelection;
	pick.sync(st.pattern,st.frame,st.PreviewTick(&view->getCharacter()->frameData),view->getCharacter()->frameData.dataVersion);
	if(pick.solo.kind<0) {
		bool visible=false;
		for(const auto& p:SurfaceFor(view).footprints) if(p.kind==pick.selected.kind && p.index==(pick.selected.instance>=0?pick.selected.instance:pick.selected.index)) visible=true;
		if(!visible) return false;
	}
	return pick.toggleSolo();
}

void MainFrame::DrawPositionSettings()
{
	for(auto& v:views) {
		auto& s=v->getState(); const bool raise=s.positionSettingsRequested;
		if(raise) { s.positionSettingsOpen=true; s.positionSettingsRequested=false; }
		if(!s.positionSettingsOpen) continue;
		if(raise) { ImGui::SetNextWindowCollapsed(false,ImGuiCond_Always); ImGui::SetNextWindowFocus(); }
		const auto name="Position tool settings###position_settings_"+std::to_string(v->getId());
		if(ImGui::Begin(name.c_str(),&s.positionSettingsOpen,ImGuiWindowFlags_AlwaysAutoResize)) {
			if(raise && !gStartup.headless) if(auto hwnd=static_cast<HWND>(ImGui::GetWindowViewport()->PlatformHandle)) {
				if(IsIconic(hwnd)) ShowWindow(hwnd,SW_RESTORE);
				SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); SetForegroundWindow(hwnd);
			}
			if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) shortcuts.claimFocus(ShortcutContext::characterView,v->getId());
			ImGui::TextUnformatted(v->getCharacter()->getName().c_str());
			bool changed=ImGui::Checkbox("Highlight gizmo hover without Ctrl/Alt",&s.positionHandleHover);
			changed |= ImGui::Checkbox("Highlight sprite while picking / dragging",&s.positionHighlight);
			if(changed) markProjectModified();
			ImGui::TextDisabled("Settings belong to this view and are saved with the project.");
			{
				bool solo=s.positionSelection.solo.kind>=0;
				if(ImGui::Checkbox("Solo selected target",&solo)) TogglePositionSolo(v.get());
			}
			ImGui::TextDisabled("Solo is preview-only. Alt+S toggles it; navigation restores all.");
		}
		ImGui::End();
	}
}

void MainFrame::DrawPositionTool(CharacterView* view,ImDrawList* drawList,bool surfaceHovered)
{
 if(!view) view=getActiveView();
 if(!view) return;
 auto& surface=SurfaceFor(view);
	m_posHoverKind = m_posHoverIndex = -1;
	m_posHoverInstance=-1;
	auto targets = CollectPositionTargets(view);
	if (targets.empty()) {
		if (m_posDrag.active && m_posDrag.viewId==view->getId()) EndPositionDrag(true);
		view->getState().positionSelection.clear();
		return;
	}
	auto& st=view->getState(); auto& pick=st.positionSelection;
	st.positionFeedbackCount=0;
	if(!m_posDrag.active) pick.sync(view->getState().pattern,view->getState().frame,view->getState().PreviewTick(&view->getCharacter()->frameData),view->getCharacter()->frameData.dataVersion);

	// Targets and footprints use surface-local pixels. Only feedback is
	// translated to desktop coordinates; picking never consumes that translation.
	const ImVec2 vpPos = surface.desktopOrigin;
	for (auto& t : targets) { t.screen.x += vpPos.x; t.screen.y += vpPos.y; }
	ImDrawList* dl = drawList?drawList:ImGui::GetBackgroundDrawList();
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mouse = io.MousePos;

	// Hover and click use the same ordered candidate set and stable view-local key.
	if (surfaceHovered || (!drawList && !io.WantCaptureMouse) || (m_posDrag.active && m_posDrag.viewId==view->getId())) {
		if(!m_posDrag.active && (io.KeyCtrl || io.KeyAlt || st.positionHandleHover)) { const auto key=PickPosition(CollectPositionTargets(view),mouse.x-vpPos.x,mouse.y-vpPos.y,io.KeyCtrl || io.KeyAlt,0,view);
			m_posHoverKind=key.kind; m_posHoverIndex=key.index; m_posHoverInstance=key.instance; }
	}

	for (const auto& t : targets) {
		if(pick.solo.kind>=0 && !(pick.solo==PositionKey(t,view))) continue;
		const bool dragging = m_posDrag.active && m_posDrag.viewId==view->getId() && m_posDrag.target.kind == t.kind && m_posDrag.target.index == t.index && m_posDrag.target.instance==t.instance;
		const bool hovered = (m_posHoverKind == t.kind && m_posHoverIndex == t.index && m_posHoverInstance==t.instance);
		const bool feedback=hovered || dragging;
		if(feedback && st.positionHighlight) for(const auto& p:SurfaceFor(view).footprints) if(p.kind==t.kind && p.index==(t.instance>=0?t.instance:t.index)) {
			++st.positionFeedbackCount;
			ImVec2 q[4]; for(int j=0;j<4;++j) q[j]=ImVec2(p.quad[j].x+vpPos.x,p.quad[j].y+vpPos.y);
			dl->AddConvexPolyFilled(q,p.triangle?3:4,IM_COL32(70,150,255,55)); dl->AddPolyline(q,p.triangle?3:4,t.color,ImDrawFlags_Closed,2.f);
		}
		const float r = (dragging || hovered) ? 7.f : 5.f;
		const ImU32 shadow = IM_COL32(0, 0, 0, 200);
		if (t.kind == PositionTarget::Layer) {
			dl->AddCircle(t.screen, r + 1.f, shadow, 20, 3.f);
			dl->AddCircle(t.screen, r, t.color, 20, 2.f);
		} else {
			dl->AddRect(ImVec2(t.screen.x - r - 1, t.screen.y - r - 1), ImVec2(t.screen.x + r + 1, t.screen.y + r + 1), shadow, 0, 0, 3.f);
			dl->AddRect(ImVec2(t.screen.x - r, t.screen.y - r), ImVec2(t.screen.x + r, t.screen.y + r), t.color, 0, 0, 2.f);
		}
		dl->AddLine(ImVec2(t.screen.x - 2, t.screen.y), ImVec2(t.screen.x + 3, t.screen.y), t.color);
		dl->AddLine(ImVec2(t.screen.x, t.screen.y - 2), ImVec2(t.screen.x, t.screen.y + 3), t.color);
		if (dragging || hovered) {
			auto* active = view->getCharacter();
			std::string text = t.label;
			if (active && view) {
				const int sourceFrame=t.sourceFrame>=0?t.sourceFrame:view->getState().frame;
				Sequence* seq = active->frameData.get_sequence(t.sourcePattern>=0?t.sourcePattern:view->getState().pattern);
				if (seq && sourceFrame>=0 && sourceFrame < (int)seq->frames.size()) {
					Frame& f = seq->frames[sourceFrame];
					int x = 0, y = 0;
					if (t.kind == PositionTarget::Layer && t.index < (int)f.AF.layers.size()) {
						x = f.AF.layers[t.index].offset_x; y = f.AF.layers[t.index].offset_y;
					} else if (t.kind != PositionTarget::Layer && t.index < (int)f.EF.size()) {
						x = f.EF[t.index].parameters[t.xParam]; y = f.EF[t.index].parameters[t.yParam];
					}
					text += "  X " + std::to_string(x) + "  Y " + std::to_string(y);
				}
			}
			if (!t.invertible) text += TXT("  (degenerate transform: locked)");
			const ImVec2 p(t.screen.x + 12, t.screen.y - 18);
			dl->AddText(ImVec2(p.x + 1, p.y + 1), shadow, text.c_str());
			dl->AddText(p, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
	ImVec2 canvasMin=surface.canvasMin,canvasMax=surface.canvasMax;
	if(pick.solo.kind>=0 && !io.KeyCtrl && !io.KeyAlt)
		dl->AddText(ImVec2(canvasMin.x+ui::Px(8.f),canvasMax.y-ImGui::GetTextLineHeight()-ui::Px(8.f)),IM_COL32(255,220,100,255),"Solo preview | Alt+S: show all");
	if(pick.candidates.size()>1 && (io.KeyCtrl || io.KeyAlt)) {
		ImGui::SetNextWindowViewport(surface.viewport);
		ImGui::SetNextWindowPos(ImVec2(canvasMax.x-ui::Px(8.f),canvasMax.y-ui::Px(8.f)),ImGuiCond_Always,ImVec2(1,1));
		ImGui::SetNextWindowBgAlpha(.9f);
		const std::string name="Position targets###position_targets_"+std::to_string(view->getId());
		if(ImGui::Begin(name.c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoFocusOnAppearing)) {
			if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) shortcuts.claimFocus(ShortcutContext::characterView,view->getId());
			ImGui::TextDisabled("Ctrl/Alt + wheel to cycle");
			ImGui::SameLine(); ImGui::BeginDisabled(m_posDrag.active);
			if(ImGui::SmallButton(pick.solo.kind>=0?"Show all":"Solo")) TogglePositionSolo(view);
			ImGui::EndDisabled();
			ImGui::BeginChild("candidates",ImVec2(std::max(120.f,std::min(ui::Px(360.f),canvasMax.x-canvasMin.x-ui::Px(32.f))),std::max(24.f,std::min(ImGui::GetTextLineHeightWithSpacing()*std::min<size_t>(6,pick.candidates.size()),canvasMax.y-canvasMin.y-ui::Px(80.f)))),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
			for(size_t i=0;i<pick.candidates.size();++i) for(const auto& t:targets) if(PositionKey(t,view)==pick.candidates[i]) {
				ImGui::PushID(int(i));
				std::string label=t.label+" [P"+std::to_string(pick.candidates[i].pattern)+" S"+std::to_string(pick.candidates[i].state)+"]";
				ImGui::BeginDisabled(m_posDrag.active);
				if(ImGui::Selectable(label.c_str(),pick.selected==pick.candidates[i])) pick.select(pick.candidates[i]);
				ImGui::EndDisabled(); ImGui::PopID(); break;
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}

void MainFrame::LeftClick(int x,int y) { BeginPositionDrag(getActiveView(),x,y); }

void MainFrame::BeginPositionDrag(CharacterView* view,int x,int y)
{
	if (m_posDrag.active) return;
	if(!view) view=getActiveView();
	auto* active = view?view->getCharacter():nullptr;
	if (!view || !active) return;
	auto targets = CollectPositionTargets(view);
	const PositionTarget* hit = nullptr;
	const auto key=PickPosition(targets,float(x),float(y),((GetKeyState(VK_CONTROL)|GetKeyState(VK_MENU))&0x8000)!=0,0,view);
	for (const auto& t : targets) {
		if(PositionKey(t,view)==key) { hit=&t; break; }
	}
	if (!hit || !hit->invertible) {
		HC_LOG_DEBUG("position miss x={} y={} targets={} ready={} version={}/{}",x,y,targets.size(),SurfaceFor(view).opponentReady,(unsigned long long)SurfaceFor(view).opponentVersion,(unsigned long long)active->frameData.dataVersion);
		return;
	}

	auto& st = view->getState();
	const int sourcePattern=hit->sourcePattern>=0?hit->sourcePattern:st.pattern;
	const int sourceFrame=hit->sourceFrame>=0?hit->sourceFrame:st.frame;
	Sequence* seq = active->frameData.get_sequence(sourcePattern);
	if (!seq || sourceFrame < 0 || sourceFrame >= (int)seq->frames.size()) return;
	Frame& f = seq->frames[sourceFrame];

	m_posDrag = PositionDrag{};
	m_posDrag.target = *hit;
	m_posDrag.viewId=view->getId();
	m_posDrag.character = active;
	m_posDrag.pattern = sourcePattern;
	m_posDrag.frame = sourceFrame;
	m_posDrag.viewPattern=st.pattern; m_posDrag.viewFrame=st.frame; m_posDrag.viewTick=st.PreviewTick(&active->frameData);
	if (hit->kind == PositionTarget::Layer) {
		m_posDrag.startX = f.AF.layers[hit->index].offset_x;
		m_posDrag.startY = f.AF.layers[hit->index].offset_y;
	} else {
		m_posDrag.startX = f.EF[hit->index].parameters[hit->xParam];
		m_posDrag.startY = f.EF[hit->index].parameters[hit->yParam];
	}
	st.animating = false;
	StopTransport(view);
	active->undoManager.beginTransaction((std::string(TXT("Move")) + " " + hit->label).c_str());
	m_posDrag.active = true;
	HC_LOG_DEBUG("position begin view={} pattern={} state={} source={}/{} kind={} index={} instance={} runtime_tick={} preview_tick={} pointer={},{} handle={:.3f},{:.3f} footprints={}",(unsigned long long)view->getId(),st.pattern,st.frame,sourcePattern,sourceFrame,int(hit->kind),hit->index,hit->instance,st.currentTick,st.PreviewTick(&active->frameData),x,y,hit->screen.x,hit->screen.y,SurfaceFor(view).footprints.size());
}

bool MainFrame::BeginPositionRotation(int x,int y,CharacterView* view)
{
	m_positionPressClaimed=m_posDrag.active;
	if(m_posDrag.active) return false;
	if(!view) view=getActiveView();
	BeginPositionDrag(view,x,y); if(!m_posDrag.active) return false;
	m_positionPressClaimed=true;
	auto* seq=m_posDrag.character->frameData.get_sequence(m_posDrag.pattern);
	auto& f=seq->frames[m_posDrag.frame]; const auto& t=m_posDrag.target;
	if(t.kind!=PositionTarget::Layer) {
		const int type=f.EF[t.index].type;
		if(t.rotationParam<0 || (t.kind!=PositionTarget::OpponentPose && type!=1 && type!=101 && type!=1000 && type!=8 && type!=108)) { EndPositionDrag(true); return false; }
	}
	m_posDrag.rotating=true; m_posDrag.pointerX=float(x); m_posDrag.pointerY=float(y);
	m_posDrag.startRotation=t.kind==PositionTarget::Layer?f.AF.layers[t.index].rotation[2]:f.EF[t.index].parameters[t.rotationParam]/10000.f;
	return true;
}

void MainFrame::PositionDragBy(int dx, int dy)
{
	if (!m_posDrag.active) return;
	CharacterInstance* c = m_posDrag.character;
	auto* view = findViewById(m_posDrag.viewId);
	if (!isLiveCharacter(c) || !view || view->getCharacter() != c ||
	    view->getState().pattern != m_posDrag.viewPattern || view->getState().frame != m_posDrag.viewFrame || view->getState().PreviewTick(&c->frameData)!=m_posDrag.viewTick) {
		EndPositionDrag(true);
		return;
	}
	Sequence* seq = c->frameData.get_sequence(m_posDrag.pattern);
	if (!seq || m_posDrag.frame >= (int)seq->frames.size()) { EndPositionDrag(true); return; }
	Frame& f = seq->frames[m_posDrag.frame];
	if(m_posDrag.rotating) {
		auto& d=m_posDrag; const auto& t=d.target;
		float ax=d.pointerX-t.screen.x, ay=d.pointerY-t.screen.y;
		d.pointerX+=dx; d.pointerY+=dy;
		float bx=d.pointerX-t.screen.x, by=d.pointerY-t.screen.y;
		if(ax*ax+ay*ay<16.f || bx*bx+by*by<16.f) return;
		float da=std::atan2(ax*by-ay*bx,ax*bx+ay*by)/(2.f*glm::pi<float>());
		if(GetKeyState(VK_MENU)&0x8000) da*=.15f;
		if(t.rotationSign!=0.f) da*=t.rotationSign;
		else if(t.kind!=PositionTarget::Layer && t.m[0]<0.f) da=-da;
		d.angleDelta+=da; float turns=d.startRotation+d.angleDelta;
		if(GetKeyState(VK_SHIFT)&0x8000) turns=std::round(turns*24.f)/24.f;
		if(t.kind==PositionTarget::Layer) f.AF.layers[t.index].rotation[2]=turns;
		else { f.EF[t.index].parameters[t.rotationParam]=int(std::lround(turns*10000.f)); view->getState().forceSpawnTreeRebuild=true; }
		c->frameData.mark_modified(d.pattern); c->markModified(); c->undoManager.markModified(); return;
	}

	const float precision = (GetKeyState(VK_MENU) & 0x8000) ? 0.15f : 1.f;
	m_posDrag.totalDX += dx * precision;
	m_posDrag.totalDY += dy * precision;
	float sx = m_posDrag.totalDX, sy = m_posDrag.totalDY;
	if (GetKeyState(VK_SHIFT) & 0x8000) {   // lock to the dominant screen axis
		if (std::fabs(sx) >= std::fabs(sy)) sy = 0; else sx = 0;
	}

	// Authored delta = M^-1 * screen delta.
	const auto& t = m_posDrag.target;
	const float det = t.m[0] * t.m[3] - t.m[2] * t.m[1];
	if (std::fabs(det) < 1e-8f) return;
	const float ax = ( t.m[3] * sx - t.m[2] * sy) / det;
	const float ay = (-t.m[1] * sx + t.m[0] * sy) / det;
	const int nx = m_posDrag.startX + (int)std::lround(ax);
	const int ny = m_posDrag.startY + (int)std::lround(ay);

	int* px = nullptr; int* py = nullptr;
	if (t.kind == PositionTarget::Layer) {
		if (t.index >= (int)f.AF.layers.size()) { EndPositionDrag(true); return; }
		px = &f.AF.layers[t.index].offset_x; py = &f.AF.layers[t.index].offset_y;
	} else {
		if (t.index >= (int)f.EF.size()) { EndPositionDrag(true); return; }
		px = &f.EF[t.index].parameters[t.xParam]; py = &f.EF[t.index].parameters[t.yParam];
	}
	if (*px == nx && *py == ny) return;
	*px = nx; *py = ny;
	c->frameData.mark_modified(m_posDrag.pattern);
	c->markModified();
	c->undoManager.markModified();
	if (t.kind != PositionTarget::Layer) view->getState().forceSpawnTreeRebuild = true;
}

void MainFrame::EndPositionDrag(bool cancel)
{
	if (!m_posDrag.active) return;
	CharacterInstance* c = m_posDrag.character;
	auto* ownerView=findViewById(m_posDrag.viewId);
	const uint64_t ownerId=m_posDrag.viewId;
	const bool effect = m_posDrag.target.kind != PositionTarget::Layer;
	m_posDrag = PositionDrag{};
	if (!isLiveCharacter(c)) return;
	if (cancel) {
		c->undoManager.cancelTransaction();
	} else {
		c->undoManager.commitTransaction();
	}
	if (c->undoManager.isClean()) c->clearModified();
	else c->markModified();
	// This known local transaction changes values, not target identity. External
	// edits/undo still invalidate selection and Solo through the normal version gate.
	if(ownerView && ownerView->getId()==ownerId && ownerView->getCharacter()==c) ownerView->getState().positionSelection.version=c->frameData.dataVersion;
	if (effect) {
		for (auto& v : views)
			if (v->getCharacter() == c) v->getState().forceSpawnTreeRebuild = true;
	}
}

void MainFrame::CancelViewportGestures()
{
	for(auto& v:views) if(v->getBoxPane()) v->getBoxPane()->EndDurationResize(true);
	if (m_boxDragCharacter) EndBoxDrag();
	if (m_posDrag.active) EndPositionDrag(true);
	if (m_uvDrag.active) EndUvDrag(false);
	shortcuts.setMouseDrag(false);
}

#endif /* UI_EDITOR_TOOLS_IMPL_H_GUARD */
