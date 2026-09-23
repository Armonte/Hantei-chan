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

	if (imguiWantsKeyboard && !imguiTextInput)
		return false;

	ShortcutChord chord;
	chord.key = (uint32_t)vkey;
	if (GetKeyState(VK_CONTROL) & 0x8000) chord.modifiers |= shortcutCtrl;
	if (GetKeyState(VK_SHIFT) & 0x8000) chord.modifiers |= shortcutShift;
	if (GetKeyState(VK_MENU) & 0x8000) chord.modifiers |= shortcutAlt;

	const ShortcutBinding* binding = shortcuts.resolve(chord, imguiTextInput, isRepeat);
	if (!binding)
		return false;

	// A focused workspace with its own history (e.g. the _c command editor)
	// gets first refusal on every action routed while it owns focus.
	if (shortcuts.dispatchToContext(binding->action))
		return true;
	return RunShortcut(binding->action);
}

bool MainFrame::RunShortcut(ShortcutAction action)
{
	auto* view = getActiveView();

	switch (action) {
	case ShortcutAction::undo:
	case ShortcutAction::redo:
		// The command workspace owns its own history; never let its focus
		// fall through to the character's undo stack.
		if (shortcuts.focused() == ShortcutContext::commands)
			return false;
		if (m_posDrag.active) EndPositionDrag(false);
		return PerformUndoRedo(action == ShortcutAction::redo);

	case ShortcutAction::save: {
		// Ctrl+S saves the character being edited, and with a project open also
		// writes the .hproj. It used to save only the .hproj when a project was
		// loaded, so character edits were silently left unsaved (issue #80).
		bool did = false;
		auto* active = getActiveCharacter();
		// A character with no file yet is skipped when a project is open (the
		// project save still runs); without a project the error explains it.
		if (active && (!active->getTopHA6Path().empty() || !ProjectManager::HasCurrentProject())) {
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
		if (!views.empty()) {
			setActiveView((activeViewIndex + 1) % (int)views.size());
			return true;
		}
		return false;
	case ShortcutAction::previousView:
		if (!views.empty()) {
			setActiveView((activeViewIndex - 1 + (int)views.size()) % (int)views.size());
			return true;
		}
		return false;
	case ShortcutAction::closeView:
		if (activeViewIndex >= 0 && activeViewIndex < (int)views.size()) {
			tryCloseView(activeViewIndex);
			return true;
		}
		return false;

	case ShortcutAction::previousPattern: if (!view) return false; AdvancePattern(-1); return true;
	case ShortcutAction::nextPattern:     if (!view) return false; AdvancePattern(1);  return true;
	case ShortcutAction::previousKeyframe:if (!view) return false; AdvanceFrame(-1);   return true;
	case ShortcutAction::nextKeyframe:    if (!view) return false; AdvanceFrame(1);    return true;
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

	// J/K/L: J plays in reverse, K stops (or resumes forward when stopped),
	// L plays forward. Shift+J / Shift+L step one tick (auto-repeat allowed).
	case ShortcutAction::playReverse:
		if (!view || !getActiveCharacter()) return false;
		view->getState().animating = false;
		m_reverseView = view;
		return true;
	case ShortcutAction::playForward:
		if (!view || !getActiveCharacter()) return false;
		if (m_reverseView == view) m_reverseView = nullptr;
		if (!view->getState().animating) {
			auto& st = view->getState();
			st.animating = true;
			st.animeSeq = st.pattern;
			st.previousFrame = -1;
		}
		return true;
	case ShortcutAction::togglePlayback:
		if (!view || !getActiveCharacter()) return false;
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
		if (!view || !getActiveCharacter()) return false;
		StepTick(view, -1);
		return true;
	case ShortcutAction::stepTickForward:
		if (!view || !getActiveCharacter()) return false;
		StepTick(view, +1);
		return true;

	case ShortcutAction::cancelGesture:
		if (m_posDrag.active) {
			EndPositionDrag(true);
			return true;
		}
		return false;

	default:
		return false;
	}
}

// Ctrl+arrows: move the selected layer of the current keyframe (AF offset X/Y).
bool MainFrame::NudgeLayer(CharacterView* view, int dx, int dy)
{
	if (!view || view->isStageView() || view->isPatEditor()) return false;
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

bool MainFrame::PerformUndoRedo(bool redo)
{
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active || view->isStageView())
		return false;

	// Close any viewport gesture first so it is a complete step.
	if (m_boxDragCharacter) EndBoxDrag();

	auto& undo = active->undoManager;
	const UndoManager::Entry* entry = redo ? undo.redo() : undo.undo();
	if (entry)
		RefreshViewsAfterHistory(active, entry);

	if (undo.isClean()) active->clearModified();
	else active->markModified();
	return true;
}

void MainFrame::RefreshViewsAfterHistory(CharacterInstance* character, const UndoManager::Entry* entry)
{
	auto* activeView = getActiveView();
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
	auto& sim = st.BindPreviewSim(&character->frameData, effectData);
	if (tick > sim.horizon()) tick = sim.horizon();
	sim.ensureSimulatedTo(tick);
	const auto& track = sim.rootFrameTrack();
	st.currentTick = tick;
	if (tick < (int)track.size()) st.frame = track[tick];
	st.frame = std::clamp(st.frame, 0, (int)seq->frames.size() - 1);
}

void MainFrame::UpdateTransport()
{
	if (!m_reverseView) return;
	// Reverse playback belongs to one view; switching tabs or closing it stops it.
	if (m_reverseView != getActiveView()) {
		m_reverseView = nullptr;
		return;
	}
	auto& st = m_reverseView->getState();
	if (st.animating || st.currentTick <= 0) {
		// Forward playback took over, or we reached the start.
		m_reverseView = nullptr;
		return;
	}
	StepTick(m_reverseView, -1);
	m_reverseView = st.currentTick > 0 ? getActiveView() : nullptr;
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
	case 1:    return "Spawn pattern";
	case 101:  return "Spawn relative pattern";
	case 1000: return "Spawn pattern once";
	case 8:    return "Spawn actor (effect.ha6)";
	case 108:  return "Spawn relative actor (effect.ha6)";
	case 3:    return "Preset effect";
	case 11:   return "Spawn random pattern";
	case 111:  return "Spawn random relative pattern";
	default:   return "Effect";
	}
}

std::vector<MainFrame::PositionTarget> MainFrame::CollectPositionTargets()
{
	std::vector<PositionTarget> out;
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active || view->isStageView() || view->isPatEditor()) return out;
	auto* box = view->getBoxPane();
	if (!box || !box->positionTool) return out;

	auto& st = view->getState();
	Sequence* seq = active->frameData.get_sequence(st.pattern);
	if (!seq || st.frame < 0 || st.frame >= (int)seq->frames.size()) return out;
	Frame& frame = seq->frames[st.frame];

	const float s = render.scale;
	const float baseX = active->renderX + clientRect.x / 2;
	const float baseY = active->renderY + clientRect.y / 2;
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
		t.label = "Layer " + std::to_string(i) + (L.usePat ? " (PAT " : " (sprite ") +
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
		if (pl.facingLeft) t.label += " (faces left)";
		if (pl.scale != 1.f) t.label += " (PAT owner: half offset)";
		t.color = ef.type == 3 ? IM_COL32(255, 150, 40, 255) : IM_COL32(255, 210, 60, 255);
		out.push_back(std::move(t));
	}
	return out;
}

void MainFrame::DrawPositionTool()
{
	m_posHoverKind = m_posHoverIndex = -1;
	auto targets = CollectPositionTargets();
	if (targets.empty()) {
		if (m_posDrag.active) EndPositionDrag(true);
		return;
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mouse = io.MousePos;
	constexpr float pickRadius = 9.f;

	// Hover: nearest handle under the cursor (only when ImGui is not using the mouse).
	float best = pickRadius * pickRadius;
	if (!io.WantCaptureMouse || m_posDrag.active) {
		for (const auto& t : targets) {
			const float dx = t.screen.x - mouse.x, dy = t.screen.y - mouse.y;
			const float d2 = dx * dx + dy * dy;
			if (d2 <= best) { best = d2; m_posHoverKind = t.kind; m_posHoverIndex = t.index; }
		}
	}

	for (const auto& t : targets) {
		const bool dragging = m_posDrag.active && m_posDrag.target.kind == t.kind && m_posDrag.target.index == t.index;
		const bool hovered = m_posHoverKind == t.kind && m_posHoverIndex == t.index;
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
			auto* active = getActiveCharacter();
			auto* view = getActiveView();
			std::string text = t.label;
			if (active && view) {
				Sequence* seq = active->frameData.get_sequence(view->getState().pattern);
				if (seq && view->getState().frame < (int)seq->frames.size()) {
					Frame& f = seq->frames[view->getState().frame];
					int x = 0, y = 0;
					if (t.kind == PositionTarget::Layer && t.index < (int)f.AF.layers.size()) {
						x = f.AF.layers[t.index].offset_x; y = f.AF.layers[t.index].offset_y;
					} else if (t.kind == PositionTarget::Effect && t.index < (int)f.EF.size()) {
						x = f.EF[t.index].parameters[t.xParam]; y = f.EF[t.index].parameters[t.yParam];
					}
					text += "  X " + std::to_string(x) + "  Y " + std::to_string(y);
				}
			}
			if (!t.invertible) text += "  (degenerate transform: locked)";
			const ImVec2 p(t.screen.x + 12, t.screen.y - 18);
			dl->AddText(ImVec2(p.x + 1, p.y + 1), shadow, text.c_str());
			dl->AddText(p, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
}

void MainFrame::LeftClick(int x, int y)
{
	if (m_posDrag.active) return;
	auto* view = getActiveView();
	auto* active = getActiveCharacter();
	if (!view || !active) return;
	auto targets = CollectPositionTargets();
	constexpr float pickRadius = 9.f;
	float best = pickRadius * pickRadius;
	const PositionTarget* hit = nullptr;
	for (const auto& t : targets) {
		const float dx = t.screen.x - x, dy = t.screen.y - y;
		const float d2 = dx * dx + dy * dy;
		if (d2 <= best) { best = d2; hit = &t; }
	}
	if (!hit || !hit->invertible) return;

	auto& st = view->getState();
	Sequence* seq = active->frameData.get_sequence(st.pattern);
	if (!seq || st.frame < 0 || st.frame >= (int)seq->frames.size()) return;
	Frame& f = seq->frames[st.frame];

	m_posDrag = PositionDrag{};
	m_posDrag.target = *hit;
	m_posDrag.character = active;
	m_posDrag.pattern = st.pattern;
	m_posDrag.frame = st.frame;
	if (hit->kind == PositionTarget::Layer) {
		m_posDrag.startX = f.AF.layers[hit->index].offset_x;
		m_posDrag.startY = f.AF.layers[hit->index].offset_y;
	} else {
		m_posDrag.startX = f.EF[hit->index].parameters[hit->xParam];
		m_posDrag.startY = f.EF[hit->index].parameters[hit->yParam];
	}
	st.animating = false;
	StopTransport(view);
	active->undoManager.beginTransaction(("Move " + hit->label).c_str());
	m_posDrag.active = true;
}

void MainFrame::PositionDragBy(int dx, int dy)
{
	if (!m_posDrag.active) return;
	CharacterInstance* c = m_posDrag.character;
	auto* view = getActiveView();
	if (!isLiveCharacter(c) || !view || view->getCharacter() != c ||
	    view->getState().pattern != m_posDrag.pattern || view->getState().frame != m_posDrag.frame) {
		EndPositionDrag(true);
		return;
	}
	Sequence* seq = c->frameData.get_sequence(m_posDrag.pattern);
	if (!seq || m_posDrag.frame >= (int)seq->frames.size()) { EndPositionDrag(true); return; }
	Frame& f = seq->frames[m_posDrag.frame];

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
	if (t.kind == PositionTarget::Effect) view->getState().forceSpawnTreeRebuild = true;
}

void MainFrame::EndPositionDrag(bool cancel)
{
	if (!m_posDrag.active) return;
	CharacterInstance* c = m_posDrag.character;
	const bool effect = m_posDrag.target.kind == PositionTarget::Effect;
	m_posDrag = PositionDrag{};
	if (!isLiveCharacter(c)) return;
	if (cancel) {
		c->undoManager.cancelTransaction();
	} else {
		c->undoManager.commitTransaction();
	}
	if (c->undoManager.isClean()) c->clearModified();
	else c->markModified();
	if (effect) {
		for (auto& v : views)
			if (v->getCharacter() == c) v->getState().forceSpawnTreeRebuild = true;
	}
}

void MainFrame::CancelViewportGestures()
{
	if (m_boxDragCharacter) EndBoxDrag();
	if (m_posDrag.active) EndPositionDrag(true);
}

#endif /* UI_EDITOR_TOOLS_IMPL_H_GUARD */
