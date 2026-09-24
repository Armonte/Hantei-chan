#ifndef UI_VIEW_RENDER_IMPL_H_GUARD
#define UI_VIEW_RENDER_IMPL_H_GUARD

// ============================================================================
// Per-view scene rendering (docs/HANTEI_WAVE2.md §2-§4)
// ============================================================================
// Included from main_frame.cpp.
//
// Every character view renders into its own RenderTarget with its own camera
// (CharacterView::camera()). A pass is fully described by Render::PassParams
// plus SceneOptions; nothing is borrowed from, or written back to, another
// view. The main window shows the main host's active view by blitting its
// target to the back buffer; detached windows show their active view's target
// through ImGui::Image (ui/workspace_hosts_impl.h); PNG export renders into a
// private target (ui/png_export_impl.h).
//
// Spawned actors, onion-skin samples and export ticks all come from the view's
// cached preview simulator (preview_sim.h, getStateAt).

#include <chrono>
#include <climits>

namespace viewrender {

// How one actor's frame is placed and coloured.
struct ActorStyle {
	float offsetX = 0.f, offsetY = 0.f; // world offset from the root actor
	bool mirrored = false;               // facing differs from the root
	float angleTurns = 0.f;              // actor angle (1 = 360 degrees)
	int zPriority = INT_MIN;             // INT_MIN: the frame's AF priority
	int pups = 0;                        // PUPS of the actor's pattern: <cg>_pN.pal (UNI2/MBTL, #76)
	float alphaMul = 1.f;
	glm::vec3 tint{1.f, 1.f, 1.f};
	bool boxes = true;                   // attach the frame's hitboxes to layer 0
	bool spawned = false;
	int flagset1 = 0, flagset2 = 0;
	int layer0Sprite = INT_MIN;          // editor override for layer 0's sprite
};

// Push every AF layer of `frame` (UNI multi-layer AFGX) as render layers.
static void AddFrameLayers(Render& render, Frame& frame, CG* cg, Parts* parts, const ActorStyle& s)
{
	if (frame.AF.layers.empty())
		frame.AF.layers.push_back({});   // MBAACC frames have one implicit layer
	for (size_t li = 0; li < frame.AF.layers.size(); ++li) {
		const auto& L = frame.AF.layers[li];
		RenderLayer layer;
		layer.spriteId = (li == 0 && s.layer0Sprite != INT_MIN) ? s.layer0Sprite : L.spriteId;
		layer.spawnOffsetX = (int)std::lround(s.offsetX);
		layer.spawnOffsetY = (int)std::lround(s.offsetY);
		layer.frameOffsetX = L.offset_x;
		layer.frameOffsetY = L.offset_y;
		layer.scaleX = L.scale[0];
		layer.scaleY = L.scale[1];
		layer.rotX = L.rotation[0];
		layer.rotY = L.rotation[1];
		layer.rotZ = L.rotation[2];
		// UNI2/MBTL keep AFRT per layer (Han6_LoadFrameAF, layer +32).
		layer.AFRT = frame.AF.AFRT || L.afrt;
		// Actor matrix F * R(angle) (MbaaTransform::ActorMatrix): the renderer
		// applies scale then Z rotation, so mirror via scaleX and add the
		// actor angle (10000 = 360 degrees, clockwise).
		if (s.mirrored) layer.scaleX *= -1.0f;
		layer.rotZ += s.angleTurns;
		layer.blendMode = L.blend_mode;
		// Object priority, then the layer's AFPL draw bucket (UNI2/MBTL;
		// AFPL 0 = object + 256 keeps MBAACC's relative order).
		layer.zPriority = LayerDrawBucket(L.priority, s.zPriority == INT_MIN ? frame.AF.priority : s.zPriority);
		layer.pups = s.pups;
		layer.alpha = L.rgba[3] * s.alphaMul;
		layer.tintColor = glm::vec4(L.rgba[0] * s.tint.r, L.rgba[1] * s.tint.g, L.rgba[2] * s.tint.b, 1.0f);
		layer.isSpawned = s.spawned;
		if (li == 0 && s.boxes) layer.hitboxes = frame.hitboxes;
		layer.sourceCG = cg;
		layer.usePat = L.usePat;
		layer.sourceParts = parts;
		layer.spawnFlagset1 = s.flagset1;
		layer.spawnFlagset2 = s.flagset2;
		render.AddLayer(layer);
	}
}

static double MsSince(std::chrono::steady_clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace viewrender

void MainFrame::DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character,
	ImDrawList* drawList, ImVec2 origin, float zoom)
{
	if (!state.vizSettings.showPresetEffects || !state.vizSettings.showSpawnedPatterns) return;
	if (!drawList) return;

	// Preset effects (EF3) and script impact markers are one-tick actors in
	// the preview simulation. "All frames" shows every one the pattern fires.
	FrameData* effectData = character->effectCharacter ? &character->effectCharacter->frameData : nullptr;
	auto& sim = state.BindPreviewSim(&character->frameData, effectData);
	preview::TickState ts;
	sim.getStateAt(state.currentTick, ts);
	const preview::SimActor* root = ts.root();
	const float rootX = root ? root->x : 0.f, rootY = root ? root->y : 0.f;

	struct Marker { float x, y; int preset; };
	std::vector<Marker> markers;
	if (state.vizSettings.presetEffectsAllFrames) {
		sim.ensureSimulatedTo(std::max(state.currentTick, sim.settledTick()));
		for (const auto& r : sim.spawnRecords())
			if (r.isPreset) markers.push_back({r.x - rootX, r.y - rootY, r.pattern});
	} else {
		for (const auto& a : ts.actors)
			if (a.isPreset) markers.push_back({a.x - rootX, a.y - rootY, a.pattern});
	}

	for (const auto& m : markers) {
		const ImVec2 center(origin.x + m.x * zoom, origin.y + m.y * zoom);
		const float size = 15.0f;
		const ImU32 color = IM_COL32(255, 128, 0, 255);  // Orange
		const float thickness = 2.0f;
		drawList->AddLine(ImVec2(center.x - size, center.y), ImVec2(center.x + size, center.y), color, thickness);
		drawList->AddLine(ImVec2(center.x, center.y - size), ImVec2(center.x, center.y + size), color, thickness);
		drawList->AddCircleFilled(center, 3.0f, color);
		if (state.vizSettings.showLabels) {
			const char* presetName = m.preset >= 0 ? GetPresetEffectName(m.preset) : "Impact FX";
			char label[64];
			snprintf(label, sizeof(label), "%s [%d]", presetName, m.preset);
			drawList->AddText(ImVec2(center.x + size + 5, center.y - 8), color, label);
		}
	}
}

// Add the root frame and (optionally) the spawned actors of `ts` as layers.
// rootFrameIndex selects the root's frame (the editor's selected frame for the
// live view, the simulated frame for onion/export samples).
void MainFrame::AddSimulatedActorLayers(CharacterView* view, const preview::TickState& ts,
	int rootFrameIndex, int layer0Sprite, bool includeSpawns, bool boxes,
	float alphaMul, const glm::vec3* sampleTint)
{
	CharacterInstance* active = view->getCharacter();
	auto& state = view->getState();
	Sequence* mainSeq = active->frameData.get_sequence(state.pattern);
	if (!mainSeq || mainSeq->frames.empty()) return;
	rootFrameIndex = std::clamp(rootFrameIndex, 0, (int)mainSeq->frames.size() - 1);
	Frame& mainFrame = mainSeq->frames[rootFrameIndex];

	viewrender::ActorStyle rootStyle;
	rootStyle.layer0Sprite = layer0Sprite;
	rootStyle.boxes = boxes;
	rootStyle.alphaMul = alphaMul;
	rootStyle.pups = mainSeq->pups;
	if (sampleTint) rootStyle.tint = *sampleTint;
	viewrender::AddFrameLayers(render, mainFrame, &active->cg, &active->parts, rootStyle);

	if (!includeSpawns) return;
	const preview::SimActor* simRoot = ts.root();
	const float rootX = simRoot ? simRoot->x : 0.f;
	const float rootY = simRoot ? simRoot->y : 0.f;
	const bool rootFacingLeft = simRoot ? simRoot->facingLeft : false;

	for (const auto& actor : ts.actors) {
		if (actor.isRoot || actor.isPreset) continue;  // presets: DrawPresetEffectMarkers

		// Per-entry visibility / alpha / tint from the spawn tree (right pane)
		const SpawnedPatternInfo* entry = FindSpawnTreeEntry(state.spawnedPatterns,
			actor.srcPattern, actor.srcFrame, actor.srcEffectIndex, actor.effectHa6,
			actor.isScript, actor.pattern);
		if (entry && !entry->visible) continue;
		const float vizAlpha = entry ? entry->alpha : 1.0f;
		const glm::vec4 vizTint = (state.vizSettings.enableTint && entry) ? entry->tintColor : glm::vec4(1.0f);

		FrameData* sourceFrameData = &active->frameData;
		CG* sourceCG = &active->cg;
		Parts* sourceParts = &active->parts;
		if (actor.effectHa6 && active->effectCharacter) {
			sourceFrameData = &active->effectCharacter->frameData;
			sourceCG = &active->effectCharacter->cg;
			sourceParts = &active->effectCharacter->parts;
		}
		Sequence* spawnedSeq = sourceFrameData->get_sequence(actor.pattern);
		if (!spawnedSeq || spawnedSeq->frames.empty()) continue;
		if (actor.frame < 0 || actor.frame >= (int)spawnedSeq->frames.size()) continue;

		viewrender::ActorStyle s;
		s.offsetX = actor.x - rootX;
		s.offsetY = actor.y - rootY;
		s.mirrored = actor.facingLeft != rootFacingLeft;
		s.angleTurns = actor.angleTurns();
		s.zPriority = actor.zPriority;          // sticky: AF priority 0 keeps the previous value
		s.alphaMul = vizAlpha * state.vizSettings.spawnedOpacity * alphaMul;
		s.tint = glm::vec3(vizTint.r, vizTint.g, vizTint.b);
		if (sampleTint) s.tint *= *sampleTint;
		s.boxes = boxes;
		s.spawned = true;
		s.pups = spawnedSeq->pups;
		s.flagset1 = actor.flagset1;
		s.flagset2 = actor.flagset2;
		viewrender::AddFrameLayers(render, spawnedSeq->frames[actor.frame], sourceCG, sourceParts, s);
	}
}

// Ticks sampled by the onion skin around `tick`, farthest first, split into
// past (before) and future (after) lists. Keyframe mode steps by root frame
// entries instead of fixed tick spacing.
static void CollectOnionTicks(preview::PreviewSim& sim, const OnionSkinSettings& o, int tick,
	std::vector<int>& past, std::vector<int>& future)
{
	past.clear(); future.clear();
	const int before = std::clamp(o.before, 0, 16), after = std::clamp(o.after, 0, 16);
	const int settled = sim.settledTick();
	const int last = settled >= 0 ? settled : sim.horizon();
	if (!o.keyframesOnly) {
		const int step = std::max(1, o.spacing);
		for (int k = before; k >= 1; --k) if (tick - k * step >= 0) past.push_back(tick - k * step);
		for (int k = after; k >= 1; --k) if (tick + k * step <= last) future.push_back(tick + k * step);
		return;
	}
	// Keyframe mode: the tick each neighbouring root frame visit starts at.
	sim.ensureSimulatedTo(std::min(last, tick + 64 * std::max(1, after)));
	const auto& track = sim.rootFrameTrack();
	if (track.empty()) return;
	auto frameAt = [&](int t) { return t < (int)track.size() ? track[t] : track.back(); };
	int t = std::min(tick, (int)track.size() - 1);
	while (t > 0 && frameAt(t - 1) == frameAt(tick)) --t;   // start of the current visit
	std::vector<int> starts;
	for (int k = 0; k < before && t > 0; ++k) {
		int s = t - 1;
		while (s > 0 && frameAt(s - 1) == frameAt(t - 1)) --s;
		starts.push_back(s);
		t = s;
	}
	past.assign(starts.rbegin(), starts.rend());
	std::vector<int> ahead;
	int u = std::min(tick, (int)track.size() - 1);
	for (int k = 0; k < after; ++k) {
		const int cur = frameAt(u);
		while (u + 1 < (int)track.size() && frameAt(u + 1) == cur) ++u;
		if (u + 1 >= (int)track.size() || u + 1 > last) break;
		++u;
		ahead.push_back(u);
	}
	future.assign(ahead.rbegin(), ahead.rend());
}

void MainFrame::DrawOnionSkin(CharacterView* view, int tick)
{
	const OnionSkinSettings& o = view->onion();
	CharacterInstance* active = view->getCharacter();
	auto& state = view->getState();
	const auto t0 = std::chrono::steady_clock::now();

	FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
	auto& sim = state.BindPreviewSim(&active->frameData, effectData);
	std::vector<int> past, future;
	CollectOnionTicks(sim, o, tick, past, future);

	double simMs = 0.0;
	auto drawSample = [&](int t, int distance, const glm::vec3& tint) {
		const auto s0 = std::chrono::steady_clock::now();
		sim.getStateAt(t, m_onionState);
		simMs += viewrender::MsSince(s0);
		const preview::SimActor* root = m_onionState.root();
		if (!root) return;
		float alpha = std::clamp(o.alpha, 0.f, 1.f);
		for (int k = 1; k < distance; ++k) alpha *= std::clamp(o.falloff, 0.f, 1.f);
		render.ClearLayers();
		AddSimulatedActorLayers(view, m_onionState, root->frame, INT_MIN,
			o.includeSpawns && state.vizSettings.showSpawnedPatterns, false, alpha, &tint);
		Sequence* seq = active->frameData.get_sequence(state.pattern);
		const int prio = (seq && root->frame >= 0 && root->frame < (int)seq->frames.size())
			? seq->frames[root->frame].AF.priority : 0;
		render.SortLayersByZPriority(prio);
		render.DrawLayers();
	};
	// Farthest samples first so nearer ones draw on top; all behind the
	// current frame.
	for (size_t i = 0; i < past.size(); ++i) drawSample(past[i], (int)(past.size() - i), o.pastTint);
	for (size_t i = 0; i < future.size(); ++i) drawSample(future[i], (int)(future.size() - i), o.futureTint);

	m_onionStats.samples = (int)(past.size() + future.size());
	m_onionStats.simMs = simMs;
	m_onionStats.totalMs = viewrender::MsSince(t0);
}

void MainFrame::DrawCharacterScene(CharacterView* view, const Render::PassParams& pass, const SceneOptions& opt)
{
	CharacterInstance* active = view ? view->getCharacter() : nullptr;
	if (!active) return;
	render.BeginPass(pass);
	auto& state = view->getState();

	if (opt.grid) {
		// The PAT editor's origin cross / UV rectangle live in the grid line
		// buffer (RenderUpdate); every other view draws the plain grid.
		if (!view->isPatEditor()) render.ResetGridLines();
		render.DrawGridLines();
	}

	Sequence* mainSeq = active->frameData.get_sequence(state.pattern);
	if (!mainSeq || mainSeq->frames.empty()) return;
	// Self-heal a stale frame index (pattern shrunk by undo/reload while the
	// Main Pane's clamp didn't run, e.g. when that pane is hidden).
	if (state.frame < 0) state.frame = 0;
	if (state.frame >= (int)mainSeq->frames.size()) state.frame = (int)mainSeq->frames.size() - 1;

	const bool spawns = opt.spawns < 0 ? state.vizSettings.showSpawnedPatterns : opt.spawns != 0;
	const int tick = opt.tick >= 0 ? opt.tick : state.currentTick;

	if (opt.onion && view->onion().enabled && !view->isPatEditor())
		DrawOnionSkin(view, tick);
	else if (view == getActiveView())
		m_onionStats = OnionStats{};

	// Spawned actors at the tick come from the cached tick simulator
	// (preview_sim.h). Positions are world coordinates with the root at the
	// origin; facing is the engine's resolved child facing and the actor angle
	// already includes inherited rotation.
	FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
	preview::TickState& ts = m_sceneState;
	ts.actors.clear();
	ts.tick = -1;
	if (spawns || opt.tick >= 0) {
		auto& sim = state.BindPreviewSim(&active->frameData, effectData);
		sim.getStateAt(tick, ts);
	}
	int rootFrame = state.frame;
	// The main window's view keeps the editor's sprite id for layer 0
	// (RenderUpdate refreshes it every frame); other views read the layer.
	int layer0Sprite = view == getActiveView() ? state.spriteId : INT_MIN;
	if (opt.tick >= 0) {
		// Export / explicit tick: the root frame is the simulated one.
		const preview::SimActor* root = ts.root();
		rootFrame = root ? root->frame : 0;
		layer0Sprite = INT_MIN;
	}

	render.ClearLayers();
	AddSimulatedActorLayers(view, ts, rootFrame, layer0Sprite, spawns, opt.boxes, 1.f, nullptr);
	// Pattern comparison overlay (issue #63): drawn over the tool windows'
	// target view (the tab worked in last, main or detached) at its live
	// tick, not over onion/export tick samples.
	if (opt.tick < 0 && view == getToolView())
		AddCompareLayers(view, active);
	rootFrame = std::clamp(rootFrame, 0, (int)mainSeq->frames.size() - 1);
	render.SortLayersByZPriority(mainSeq->frames[rootFrame].AF.priority);
	render.DrawLayers();
}

void MainFrame::DrawMainViewScene(CharacterView* view, int width, int height)
{
	Render::PassParams pass;
	pass.width = width;
	pass.height = height;

	if (view->isStageView()) {
		// bgCamera.panLastX/Y is the screen anchor (where world (0, 0) sits)
		// in u4ick's model; the pass origin translates bg sprites, grid and
		// reference markers together when the user pans.
		pass.zoom = view->getZoom();
		pass.originX = bgCamera.panLastX * pass.zoom;
		pass.originY = bgCamera.panLastY * pass.zoom;
		render.BeginPass(pass);
		// Stage back half (band 0, render priority 10) goes under the grid;
		// the front half (weather + band 1) goes over it.
		bgRenderer.Render(bgCamera, width, height, bg::Pass::Back);
		render.ResetGridLines();
		render.DrawGridLines();
		bgRenderer.Render(bgCamera, width, height, bg::Pass::Front);

		// u4ick's stage boundary rects (MonoForm.cs:431-435). They live at
		// the LIVE camera position (movingPoint, = bgCamera.panX/panY), at
		// parallax=256 implicitly: during a drag they slide with the cursor
		// at 1:1 while parallax layers shift relative to them.
		if (bgRenderer.IsShowingDebugOverlay()) {
			const float z = bgCamera.zoom;
			const ImVec2 vp = ImGui::GetMainViewport()->Pos;   // desktop coords with viewports
			const float px = vp.x + (bgCamera.panX - bg::STAGE_CENTER_X) * z;
			const float py = vp.y + (bgCamera.panY - bg::STAGE_FLOOR_Y) * z;
			auto* dl = ImGui::GetBackgroundDrawList();
			dl->AddLine(ImVec2(px - 401*z, py + 224*z), ImVec2(px + 656*z, py + 224*z),
			            IM_COL32(255, 255, 0, 255), 1.0f);
			dl->AddRect(ImVec2(px - 401*z, py - 538*z), ImVec2(px + 656*z, py + 272*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
			dl->AddRect(ImVec2(px - 401*z, py + 272*z), ImVec2(px + 656*z, py + 280*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
		}
		return;
	}

	const ViewCamera& cam = view->camera();
	pass.zoom = cam.zoom;
	pass.originX = cam.panX + width / 2.f;
	pass.originY = cam.panY + height / 2.f;
	render.BeginPass(pass);
	// A loaded stage only renders in its own stage tab; for character tabs
	// the background renderer is disabled and these passes are no-ops.
	bgRenderer.Render(bgCamera, width, height, bg::Pass::Back);
	DrawCharacterScene(view, pass, SceneOptions{});
	bgRenderer.Render(bgCamera, width, height, bg::Pass::Front);

	if (view->getCharacter())
		DrawPresetEffectMarkers(view->getState(), view->getCharacter(), ImGui::GetBackgroundDrawList(),
			ImVec2(ImGui::GetMainViewport()->Pos.x + pass.originX, ImGui::GetMainViewport()->Pos.y + pass.originY),
			pass.zoom);
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	ProcessPendingExport();

	// Dev hook: HANTEI_STAGE_PREVIEW=<stage.dat> opens that stage on the
	// first frame and captures the stage view to C:/dev/bg_dump.png ~2 s
	// later (used to eyeball renderer changes without the file dialog).
	static bool stageEnvChecked = false;
	if (!stageEnvChecked) {
		stageEnvChecked = true;
		if (const char* p = std::getenv("HANTEI_STAGE_PREVIEW")) {
			loadStageFile(p);
			bgRenderer.RequestDebugDump(120);
		}
	}

	// Tick background animation (once per frame, regardless of which view
	// is shown).
	bgRenderer.Update();
	// Stage-view smooth zoom-to-cursor: ease the stage tab's zoom toward the
	// target and re-pin the cursor's world anchor each frame so the point
	// under the cursor stays put as the scale changes.
	if (bgZoomAnimating) {
		auto* zv = getActiveView();
		if (!zv || !zv->isStageView()) {
			bgZoomAnimating = false;  // left the stage tab - abandon
		} else {
			float z = zv->getZoom();
			z += (bgZoomTarget - z) * 0.30f;
			const float d = bgZoomTarget - z;
			if (d < 0.004f && d > -0.004f) {
				z = bgZoomTarget;
				bgZoomAnimating = false;
			}
			const float s = z > 0.0f ? z : 1.0f;
			const float zpx = bgZoomAnchorScrnX / s - bgZoomAnchorWorldX;
			const float zpy = bgZoomAnchorScrnY / s - bgZoomAnchorWorldY;
			bgCamera.SetPan(zpx, zpy);
			zoom_idx = z;
			zv->setZoom(z);
			zv->setStageRenderXY(zpx, zpy);
		}
	}
	if (auto* sv = getActiveView(); sv && sv->isStageView())
		bgCamera.zoom = sv->getZoom();
	// Ease panLast -> panX after a drag so the parallax delta decays
	// smoothly instead of snapping (no-op while dragging or settled).
	bgCamera.Settle();

	const int width = std::max(1, (int)clientRect.x);
	const int height = std::max(1, (int)clientRect.y);
	CharacterView* view = getActiveView();
	RenderTarget* target = view ? &view->renderTarget() : nullptr;
	if (target && target->ensure(width, height, false)) {
		{
			ScopedTargetBinding bind(*target);
			bind.clear(clearColor[0], clearColor[1], clearColor[2], 1.f, true);
			const auto t0 = std::chrono::steady_clock::now();
			DrawMainViewScene(view, width, height);
			m_lastSceneMs = viewrender::MsSince(t0);
		}
		target->blitToDefault(width, height);
	} else {
		glViewport(0, 0, width, height);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	RenderDetachedViewTargets();
}

#endif /* UI_VIEW_RENDER_IMPL_H_GUARD */
