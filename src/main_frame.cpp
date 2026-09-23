#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"
#include "imgui_utils.h"
#include "project_manager.h"
#include "preset_effects.h"
#include "version.h"
#include "framestate.h"
#include "misc.h"
#include "background/bg_inspector.h"
#include "extension_profile.h"
#include "framedata_ha4.h"
#include "ha4_character.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

wchar_t MainFrame::s_swallowChar = 0;

MainFrame::MainFrame(ContextGl *context_):
context(context_)
{
	LoadSettings();
	// User key bindings (issue #9); unknown or stale entries are ignored.
	for (const auto& line : gSettings.keyBindings)
		shortcuts.registry().applyOverride(line);
	// Hand the background renderer/camera to the GL Render so its Draw()
	// loop calls into bgRenderer at the right point (behind the character).
	render.SetBackgroundRenderer(&bgRenderer, &bgCamera);
	// The bg renderer draws PAT-pattern stage objects through Render's
	// Parts pipeline (sprite-id < 10000) — give it the back-reference.
	bgRenderer.SetHostRender(&render);
	// Stage edits keep their own history (bg::File), separate from the
	// character undo stack: while a stage view owns focus, Ctrl+Z / Ctrl+Y
	// go to the stage and Ctrl+S saves the stage file.
	shortcuts.setContextHandler(ShortcutContext::stageView, [this](ShortcutAction a) {
		if (!currentBgFile) return false;
		switch (a) {
		case ShortcutAction::undo: currentBgFile->Undo(); return true;
		case ShortcutAction::redo: currentBgFile->Redo(); return true;
		case ShortcutAction::save:
			if (currentBgFile->Save(currentBgFile->GetFilename().c_str())) currentBgFile->ClearDirty();
			return true;
		default: return false;
		}
	});
}

MainFrame::~MainFrame()
{
	clearStage();
	ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
}

void MainFrame::LoadSettings()
{
	LoadTheme(gSettings.theme);
	SetZoom(gSettings.zoomLevel);
	smoothRender = gSettings.bilinear;
	memcpy(clearColor, gSettings.color, sizeof(float)*3);
}

void MainFrame::Draw()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	DrawUi();
	DrawBack();
	ImGui::Render();

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	ProcessStartupArgs();

	// Multi-viewport disabled in main.cpp for now; UpdatePlatformWindows is
	// a no-op without the flag but the guarded read-of-IO is also fine to
	// skip while we sort out the Inspector interaction bug.
	// ImGuiIO& io = ImGui::GetIO();
	// if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) { ... }

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

void MainFrame::DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character)
{
	if (!state.vizSettings.showPresetEffects || !state.vizSettings.showSpawnedPatterns) return;

	// Get ImGui draw list for overlay
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();

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
		// Convert game coordinates to screen coordinates
		// render.x/y formula: (character->renderX + clientRect.x/2) / render.scale
		float screenX = character->renderX + clientRect.x / 2 + m.x * render.scale;
		float screenY = character->renderY + clientRect.y / 2 + m.y * render.scale;

		// Draw crosshair
		float size = 15.0f;
		ImU32 color = IM_COL32(255, 128, 0, 255);  // Orange
		float thickness = 2.0f;
		ImVec2 center(screenX, screenY);
		drawList->AddLine(ImVec2(center.x - size, center.y), ImVec2(center.x + size, center.y), color, thickness);
		drawList->AddLine(ImVec2(center.x, center.y - size), ImVec2(center.x, center.y + size), color, thickness);
		drawList->AddCircleFilled(center, 3.0f, color);

		// Label with preset name (if labels enabled)
		if (state.vizSettings.showLabels) {
			const char* presetName = m.preset >= 0 ? GetPresetEffectName(m.preset) : "Impact FX";
			char label[64];
			snprintf(label, sizeof(label), "%s [%d]", presetName, m.preset);
			drawList->AddText(ImVec2(center.x + size + 5, center.y - 8), color, label);
		}
	}
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);

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

	// Tick background animation (once per frame, regardless of which draw
	// path we take below).
	bgRenderer.Update();
	// Sync editor zoom -> bg camera zoom so mouse-wheel zoom (which writes
	// render.scale) drives the bg projection too, and so the drag delta
	// math (which divides by render.scale to convert screen-px to world-px)
	// matches what the bg's projection will scale back up to screen-px.
	// Stage-view smooth zoom-to-cursor: ease render.scale toward the
	// target and re-pin the cursor's world anchor each frame so the
	// point under the cursor stays put as the scale changes.
	if (bgZoomAnimating) {
		auto* zv = getActiveView();
		if (!zv || !zv->isStageView()) {
			bgZoomAnimating = false;  // left the stage tab — abandon
		} else {
			render.scale += (bgZoomTarget - render.scale) * 0.30f;
			float d = bgZoomTarget - render.scale;
			if (d < 0.004f && d > -0.004f) {
				render.scale = bgZoomTarget;
				bgZoomAnimating = false;
			}
			float s = render.scale > 0.0f ? render.scale : 1.0f;
			float zpx = bgZoomAnchorScrnX / s - bgZoomAnchorWorldX;
			float zpy = bgZoomAnchorScrnY / s - bgZoomAnchorWorldY;
			bgCamera.SetPan(zpx, zpy);
			zoom_idx = render.scale;
			zv->setZoom(render.scale);
			zv->setStageRenderXY(zpx, zpy);
		}
	}

	bgCamera.zoom = render.scale;
	// Ease panLast -> panX after a drag so the parallax delta decays
	// smoothly instead of snapping (no-op while dragging or settled).
	bgCamera.Settle();
	// Stage back half (band 0, render priority 10) goes under the
	// characters; the front half (weather + band 1 "in front of characters",
	// objhdr+21) is drawn by this guard on every exit path, after them.
	bgRenderer.Render(bgCamera, (int)clientRect.x, (int)clientRect.y, bg::Pass::Back);
	struct StageFrontPass {
		MainFrame* mf;
		~StageFrontPass() {
			mf->bgRenderer.Render(mf->bgCamera, (int)clientRect.x,
			                      (int)clientRect.y, bg::Pass::Front);
		}
	} stageFrontPass{this};

	auto* active = getActiveCharacter();
	auto* view = getActiveView();

	if (active) {
		render.x = (active->renderX + clientRect.x/2) / render.scale;
		render.y = (active->renderY + clientRect.y/2) / render.scale;
	}
	else if (view && view->isStageView()) {
		// bgCamera.panLastX/Y is the screen anchor (where world (0, 0) sits)
		// in u4ick's model. Mirror it into render.x/y so the GL transform
		// translates everything (bg sprites + grid + reference markers)
		// together when the user pans.
		render.x = bgCamera.panLastX;
		render.y = bgCamera.panLastY;
		render.DrawGridLines();

		// u4ick's stage boundary rects (MonoForm.cs:431-435). They live at
		// the LIVE camera position (movingPoint, = bgCamera.panX/panY), at
		// parallax=256 implicitly — so during a right-drag they slide with
		// the cursor at 1:1 while parallax layers shift WRT them at their
		// own rates. On release, panLast catches up to panX and the
		// parallax delta collapses back to zero, so layers snap into the
		// common-frame view aligned to the rects.
		if (bgRenderer.IsShowingDebugOverlay()) {
			float z = bgCamera.zoom;
			// Shift by STAGE_CENTER_X / STAGE_FLOOR_Y so the rects land on
			// the grid lines — same shift the bg sprites get in bg_renderer.
			float px = (bgCamera.panX - bg::STAGE_CENTER_X) * z;
			float py = (bgCamera.panY - bg::STAGE_FLOOR_Y) * z;
			auto* dl = ImGui::GetBackgroundDrawList();
			// Yellow ground line: (panX-401, panY+224) span 1057 (h=0).
			dl->AddLine(ImVec2(px - 401*z, py + 224*z),
			            ImVec2(px + 656*z, py + 224*z),
			            IM_COL32(255, 255, 0, 255), 1.0f);
			// Purple full playfield: 1057x810 from (panX-401, panY-538).
			dl->AddRect(ImVec2(px - 401*z, py - 538*z),
			            ImVec2(px + 656*z, py + 272*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
			// Purple thin band: 1057x8 just below the ground line.
			dl->AddRect(ImVec2(px - 401*z, py + 272*z),
			            ImVec2(px + 656*z, py + 280*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
		}
		return; // DrawBackground above already drew the stage.
	}

	// Check if we need to draw with spawned patterns
	bool hasSpawnedPatterns = false;
	if (view && active) {
		auto& state = view->getState();
		hasSpawnedPatterns = state.vizSettings.showSpawnedPatterns;
	}

	if (hasSpawnedPatterns && view && active) {
		// Draw everything as layers (main + spawned) in Z-order
		auto& state = view->getState();

		// First draw grid lines only
		render.DrawGridLines();

		// Get current main pattern sequence
		auto mainSeq = active->frameData.get_sequence(state.pattern);
		if (!mainSeq || mainSeq->frames.empty()) return;
		// Self-heal a stale frame index (pattern shrunk by undo/reload while
		// the Main Pane's clamp didn't run, e.g. when that pane is hidden).
		if (state.frame < 0) state.frame = 0;
		if (state.frame >= (int)mainSeq->frames.size()) state.frame = (int)mainSeq->frames.size() - 1;
		auto& mainFrame = mainSeq->frames[state.frame];

		render.ClearLayers();

		// Add main pattern layers (support UNI multi-layer AFGX)
		if (mainFrame.AF.layers.empty()) {
			mainFrame.AF.layers.push_back({});  // Ensure at least one layer exists for MBAACC
		}

		// Loop through all layers in the frame (UNI multi-layer support)
		for (size_t layerIndex = 0; layerIndex < mainFrame.AF.layers.size(); layerIndex++) {
			const auto& mainLayer_data = mainFrame.AF.layers[layerIndex];

			RenderLayer mainLayer;
			// Layer 0: use UI-selected sprite (state.spriteId) for editor compatibility
			// Layers 1+: use sprite from layer data (for UNI multi-layer)
			mainLayer.spriteId = (layerIndex == 0) ? state.spriteId : mainLayer_data.spriteId;
			mainLayer.spawnOffsetX = 0;
			mainLayer.spawnOffsetY = 0;
			mainLayer.frameOffsetX = mainLayer_data.offset_x;
			mainLayer.frameOffsetY = mainLayer_data.offset_y;
			mainLayer.scaleX = mainLayer_data.scale[0];
			mainLayer.scaleY = mainLayer_data.scale[1];
			mainLayer.rotX = mainLayer_data.rotation[0];
			mainLayer.rotY = mainLayer_data.rotation[1];
			mainLayer.rotZ = mainLayer_data.rotation[2];
			mainLayer.AFRT = mainFrame.AF.AFRT || mainLayer_data.afrt;
			mainLayer.blendMode = mainLayer_data.blend_mode;
			mainLayer.zPriority = LayerDrawBucket(mainLayer_data.priority, mainFrame.AF.priority);
			mainLayer.pups = mainSeq->pups;
			mainLayer.alpha = mainLayer_data.rgba[3];  // Apply frame alpha
			mainLayer.tintColor = glm::vec4(mainLayer_data.rgba[0], mainLayer_data.rgba[1], mainLayer_data.rgba[2], 1.0f);  // Apply frame RGB
			mainLayer.isSpawned = false;
			mainLayer.hitboxes = (layerIndex == 0) ? mainFrame.hitboxes : BoxList();  // Only layer 0 gets hitboxes
			mainLayer.sourceCG = &active->cg;  // Main pattern uses character CG
			mainLayer.usePat = mainLayer_data.usePat;  // Copy PAT rendering flag from layer data
			mainLayer.sourceParts = &active->parts;  // Main pattern uses character Parts
			render.AddLayer(mainLayer);
		}

		// Spawned actors at the current tick, from the cached tick simulator
		// (preview_sim.h). Positions are world coordinates with the root at
		// the origin; facing is the engine's resolved child facing (parent
		// facing, bit-11 toggle, own X mirrored before the parent position is
		// added) and the actor angle already includes inherited rotation.
		FrameData* effectFrameDataPtr = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
		auto& sim = state.BindPreviewSim(&active->frameData, effectFrameDataPtr);
		preview::TickState simState;
		sim.getStateAt(state.currentTick, simState);
		const preview::SimActor* simRoot = simState.root();
		const float rootX = simRoot ? simRoot->x : 0.f;
		const float rootY = simRoot ? simRoot->y : 0.f;
		const bool rootFacingLeft = simRoot ? simRoot->facingLeft : false;

		for (const auto& actor : simState.actors) {
			if (actor.isRoot || actor.isPreset) continue;  // presets: DrawPresetEffectMarkers

			// Per-entry visibility / alpha / tint from the spawn tree (right pane)
			const SpawnedPatternInfo* entry = FindSpawnTreeEntry(state.spawnedPatterns,
				actor.srcPattern, actor.srcFrame, actor.srcEffectIndex, actor.effectHa6,
				actor.isScript, actor.pattern);
			if (entry && !entry->visible) continue;
			const float vizAlpha = entry ? entry->alpha : 1.0f;
			const glm::vec4 vizTint = (state.vizSettings.enableTint && entry) ? entry->tintColor : glm::vec4(1.0f);

			// Data source: effect.ha6 actors use the effect character when loaded
			FrameData* sourceFrameData = &active->frameData;
			CG* sourceCG = &active->cg;
			Parts* sourceParts = &active->parts;
			if (actor.effectHa6 && active->effectCharacter) {
				sourceFrameData = &active->effectCharacter->frameData;
				sourceCG = &active->effectCharacter->cg;
				sourceParts = &active->effectCharacter->parts;
			}

			auto spawnedSeq = sourceFrameData->get_sequence(actor.pattern);
			if (!spawnedSeq || spawnedSeq->frames.empty()) continue;
			if (actor.frame < 0 || actor.frame >= (int)spawnedSeq->frames.size()) continue;
			auto& spawnedFrame = spawnedSeq->frames[actor.frame];

			// Ensure spawned frame has at least one layer
			if (spawnedFrame.AF.layers.empty()) {
				spawnedFrame.AF.layers.push_back({});
			}

			const bool mirrored = actor.facingLeft != rootFacingLeft;

			// Loop through all layers in spawned frame (UNI multi-layer support)
			for (size_t spawnLayerIndex = 0; spawnLayerIndex < spawnedFrame.AF.layers.size(); spawnLayerIndex++) {
				const auto& spawnedLayer_data = spawnedFrame.AF.layers[spawnLayerIndex];

				RenderLayer layer;
				layer.spriteId = spawnedLayer_data.spriteId;
				layer.spawnOffsetX = (int)std::lround(actor.x - rootX);
				layer.spawnOffsetY = (int)std::lround(actor.y - rootY);
				layer.frameOffsetX = spawnedLayer_data.offset_x;
				layer.frameOffsetY = spawnedLayer_data.offset_y;
				layer.scaleX = spawnedLayer_data.scale[0];
				layer.scaleY = spawnedLayer_data.scale[1];
				layer.rotX = spawnedLayer_data.rotation[0];
				layer.rotY = spawnedLayer_data.rotation[1];
				layer.rotZ = spawnedLayer_data.rotation[2];
				layer.AFRT = spawnedFrame.AF.AFRT || spawnedLayer_data.afrt;
				layer.pups = spawnedSeq->pups;

				// Actor matrix F * R(angle) (MbaaTransform::ActorMatrix): the
				// renderer applies scale then Z rotation, so mirror via scaleX
				// and add the actor angle (10000 = 360 degrees, clockwise).
				if (mirrored) layer.scaleX *= -1.0f;
				layer.rotZ += actor.angleTurns();

				layer.blendMode = spawnedLayer_data.blend_mode;
				// Sticky Z priority (AF priority 0 keeps the previous value),
				// then the layer's AFPL bucket (UNI/MBTL).
				layer.zPriority = LayerDrawBucket(spawnedLayer_data.priority, actor.zPriority);
				// Apply frame RGBA, then visualization alpha
				layer.alpha = spawnedLayer_data.rgba[3] * vizAlpha * state.vizSettings.spawnedOpacity;
				// Multiply frame RGB with visualization tint
				layer.tintColor = glm::vec4(
					spawnedLayer_data.rgba[0] * vizTint.r,
					spawnedLayer_data.rgba[1] * vizTint.g,
					spawnedLayer_data.rgba[2] * vizTint.b,
					1.0f);
				layer.isSpawned = true;
				layer.hitboxes = (spawnLayerIndex == 0) ? spawnedFrame.hitboxes : BoxList();  // Only layer 0 gets hitboxes
				layer.sourceCG = sourceCG;
				layer.usePat = spawnedLayer_data.usePat;
				layer.sourceParts = sourceParts;
				layer.spawnFlagset1 = actor.flagset1;
				layer.spawnFlagset2 = actor.flagset2;

				render.AddLayer(layer);
			}
		}

		// Sort all layers (including main) by Z-priority before drawing
		AddCompareLayers(view, active);  // pattern comparison overlay (#63)
		render.SortLayersByZPriority(mainFrame.AF.priority);

		// Draw all layers in Z-order
		render.DrawLayers();

		// Draw preset effect crosshairs (Effect Type 3) as overlay
		DrawPresetEffectMarkers(state, active);
	}
	else if (view && active) {
		// Normal draw without spawned patterns - use multi-layer rendering for UNI support
		auto& state = view->getState();

		// First draw grid lines only
		render.DrawGridLines();

		// Get current main pattern sequence
		auto mainSeq = active->frameData.get_sequence(state.pattern);
		if (!mainSeq || mainSeq->frames.empty()) return;
		// Self-heal a stale frame index (pattern shrunk by undo/reload while
		// the Main Pane's clamp didn't run, e.g. when that pane is hidden).
		if (state.frame < 0) state.frame = 0;
		if (state.frame >= (int)mainSeq->frames.size()) state.frame = (int)mainSeq->frames.size() - 1;
		auto& mainFrame = mainSeq->frames[state.frame];

		render.ClearLayers();

		// Add main pattern layers (support UNI multi-layer AFGX)
		if (mainFrame.AF.layers.empty()) {
			mainFrame.AF.layers.push_back({});  // Ensure at least one layer exists for MBAACC
		}

		// Loop through all layers in the frame (UNI multi-layer support)
		for (size_t layerIndex = 0; layerIndex < mainFrame.AF.layers.size(); layerIndex++) {
			const auto& mainLayer_data = mainFrame.AF.layers[layerIndex];

			RenderLayer mainLayer;
			// Layer 0: use UI-selected sprite (state.spriteId) for editor compatibility
			// Layers 1+: use sprite from layer data (for UNI multi-layer)
			mainLayer.spriteId = (layerIndex == 0) ? state.spriteId : mainLayer_data.spriteId;
			mainLayer.spawnOffsetX = 0;
			mainLayer.spawnOffsetY = 0;
			mainLayer.frameOffsetX = mainLayer_data.offset_x;
			mainLayer.frameOffsetY = mainLayer_data.offset_y;
			mainLayer.scaleX = mainLayer_data.scale[0];
			mainLayer.scaleY = mainLayer_data.scale[1];
			mainLayer.rotX = mainLayer_data.rotation[0];
			mainLayer.rotY = mainLayer_data.rotation[1];
			mainLayer.rotZ = mainLayer_data.rotation[2];
			mainLayer.AFRT = mainFrame.AF.AFRT || mainLayer_data.afrt;
			mainLayer.blendMode = mainLayer_data.blend_mode;
			mainLayer.zPriority = LayerDrawBucket(mainLayer_data.priority, mainFrame.AF.priority);
			mainLayer.pups = mainSeq->pups;
			mainLayer.alpha = mainLayer_data.rgba[3];  // Apply frame alpha
			mainLayer.tintColor = glm::vec4(mainLayer_data.rgba[0], mainLayer_data.rgba[1], mainLayer_data.rgba[2], 1.0f);  // Apply frame RGB
			mainLayer.isSpawned = false;
			mainLayer.hitboxes = (layerIndex == 0) ? mainFrame.hitboxes : BoxList();  // Only layer 0 gets hitboxes
			mainLayer.sourceCG = &active->cg;  // Main pattern uses character CG
			mainLayer.usePat = mainLayer_data.usePat;  // Copy PAT rendering flag from layer data
			mainLayer.sourceParts = &active->parts;  // Main pattern uses character Parts
			render.AddLayer(mainLayer);
		}

		// Sort and draw all layers
		AddCompareLayers(view, active);  // pattern comparison overlay (#63)
		render.SortLayersByZPriority(mainFrame.AF.priority);
		render.DrawLayers();

		// Draw preset effect crosshairs
		DrawPresetEffectMarkers(state, active);
	}
}


// ============================================================================
// UI Implementation - extracted to ui/main_ui_impl.h for better organization
// ============================================================================
#include "ui/main_ui_impl.h"

// ============================================================================
// Menu Implementation - extracted to ui/main_menu_impl.h
// ============================================================================
#include "ui/main_menu_impl.h"

// ============================================================================
// Editing tools (shortcuts, undo/redo, J/K/L, box-drag undo, position tool)
// ============================================================================
#include "ui/editor_tools_impl.h"
#include "ui/tool_windows_impl.h"


void MainFrame::WarmStyle()
{
	ImVec4* colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.95f, 0.91f, 0.85f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.98f, 0.96f, 0.93f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.95f, 1.00f, 0.62f, 1.00f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 1.00f, 0.81f, 1.00f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.82f, 0.73f, 0.64f, 0.81f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.97f, 0.65f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(1.00f, 0.96f, 0.87f, 0.99f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.98f, 0.94f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.72f, 0.66f, 0.48f, 0.99f);
	colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.55f, 0.53f, 0.32f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.74f, 1.00f, 0.53f, 0.25f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.77f, 0.41f, 0.96f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.47f, 0.00f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.74f, 0.57f, 0.33f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.94f, 0.75f, 0.36f, 0.42f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.75f, 0.01f, 0.61f);
	colors[ImGuiCol_Separator]              = ImVec4(0.38f, 0.34f, 0.25f, 0.66f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.76f, 0.70f, 0.59f, 0.98f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.32f, 0.32f, 0.32f, 0.45f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.41f, 0.80f, 1.00f, 0.84f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.61f, 0.23f, 1.00f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.79f, 0.74f, 0.64f, 0.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.64f, 0.06f, 0.85f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.69f, 0.40f, 0.12f, 0.31f);
	colors[ImGuiCol_TabUnfocused]           = ImVec4(0.93f, 0.92f, 0.92f, 0.98f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.91f, 0.87f, 0.74f, 1.00f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.98f, 0.35f, 0.22f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.58f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.40f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.69f, 0.53f, 0.32f, 0.30f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.69f, 0.58f, 0.44f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.70f, 0.62f, 0.42f, 0.40f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.98f, 0.89f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.98f, 0.94f, 0.95f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.97f, 0.98f, 0.80f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	ChangeClearColor(0.324f, 0.409f, 0.185f);
}

// Multi-character/view support helper methods

CharacterView* MainFrame::getActiveView()
{
	if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
		return views[activeViewIndex].get();
	}
	return nullptr;
}

const CharacterView* MainFrame::getActiveView() const
{
	if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
		return views[activeViewIndex].get();
	}
	return nullptr;
}

CharacterInstance* MainFrame::getActiveCharacter()
{
	auto* view = getActiveView();
	return view ? view->getCharacter() : nullptr;
}

const CharacterInstance* MainFrame::getActiveCharacter() const
{
	auto* view = getActiveView();
	return view ? view->getCharacter() : nullptr;
}

void MainFrame::setActiveView(int index)
{
	if (index >= 0 && index < views.size()) {
		activeViewIndex = index;

		// Update render state to use this view's character and settings
		auto* view = getActiveView();
		if (view) {
			auto* character = view->getCharacter();
			if (character) {
				// Update CG reference (this resets curImageId)
				render.SetCg(&character->cg);

				// Set Parts if this view has them loaded (PAT editor or character with PAT)
				// SetParts internally handles texture clearing when switching between Parts/CG
				if (character->parts.loaded) {
					render.SetParts(&character->parts);
				} else {
					render.SetParts(nullptr);  // Clear Parts for non-PAT views
				}
			}

			// Restore this view's zoom level
			float viewZoom = view->getZoom();
			render.scale = viewZoom;
			zoom_idx = viewZoom;  // Update UI slider

			// Point the background renderer at this view's stage (if any).
			// currentBgFile mirrors the active tab's stage so the Stage menu
			// and Background Inspector keep working transparently.
			if (view->isStageView()) {
				currentBgFile = view->getStageFile();
				bgRenderer.SetFile(currentBgFile);
				bgRenderer.SetEnabled(true);
				// Apply stored pan from the view into bgCamera (the source
				// of truth) and mirror into render.x/y so the very first
				// DrawBackground call this frame uses correct coords.
				bgCamera.SetPan(view->getStageRenderX(),
				                view->getStageRenderY());
				render.x = bgCamera.panLastX;
				render.y = bgCamera.panLastY;
			} else {
				currentBgFile = nullptr;
				bgRenderer.SetFile(nullptr);
				bgRenderer.SetEnabled(false);
			}
		}
	}
}

CharacterInstance* MainFrame::findCharacterByPath(const std::string& path)
{
	for (auto& character : characters) {
		if (character->getTxtPath() == path || character->getTopHA6Path() == path) {
			return character.get();
		}
	}
	return nullptr;
}

void MainFrame::createViewForCharacter(CharacterInstance* character)
{
	if (!character) return;

	// Find the lowest available view number for this character
	int viewNumber = 0;
	bool foundNumber = false;
	while (!foundNumber) {
		foundNumber = true;
		for (auto& view : views) {
			if (view->getCharacter() == character && view->getViewNumber() == viewNumber) {
				viewNumber++;
				foundNumber = false;
				break;
			}
		}
	}

	auto view = std::make_unique<CharacterView>(character, &render);
	view->setViewNumber(viewNumber);
	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
}

bool MainFrame::reopenClosedTab()
{
	while (!m_closedTabs.empty()) {
		ClosedTab t = m_closedTabs.back();
		m_closedTabs.pop_back();
		CharacterInstance* character = findCharacterByPath(t.path);
		if (!character) {
			auto loaded = std::make_unique<CharacterInstance>();
			const bool ok = t.isTxt ? loaded->loadFromTxt(t.path) : loaded->loadHA6(t.path, false);
			if (!ok) continue;
			character = loaded.get();
			characters.push_back(std::move(loaded));
		}
		createViewForCharacter(character);
		markProjectModified();
		if (auto* view = getActiveView()) {
			auto& st = view->getState();
			st.pattern = t.pattern;
			st.frame = t.frame;
			Sequence* seq = character->frameData.get_sequence(st.pattern);
			if (!seq) st.pattern = 0;
			seq = character->frameData.get_sequence(st.pattern);
			const int frames = seq ? (int)seq->frames.size() : 0;
			if (st.frame >= frames) st.frame = frames > 0 ? frames - 1 : 0;
			st.currentTick = frames > 0 ? CalculateTickFromFrame(&character->frameData, st.pattern, st.frame) : 0;
			if (view->getMainPane()) view->getMainPane()->RegenerateNames();
		}
		return true;
	}
	return false;
}

void MainFrame::createPatEditorView(const std::string& patPath)
{
	// Create a dummy character for PAT editing
	auto character = std::make_unique<CharacterInstance>();
	character->frameData.initEmpty();

	// Extract filename from path for display name
	size_t lastSlash = patPath.find_last_of("/\\");
	std::string filename = (lastSlash != std::string::npos) ? patPath.substr(lastSlash + 1) : patPath;
	character->setName(filename);

	// Create dummy frame with usePat layer
	auto seq = character->frameData.get_sequence(0);
	if (seq) {
		auto frame = &seq->frames.emplace_back();
		// Ensure frame has at least one layer
		if (frame->AF.layers.empty()) {
			frame->AF.layers.push_back({});
		}
		frame->AF.layers[0].usePat = true;
		frame->AF.layers[0].spriteId = 0;  // Initialize to first partSet for .pat rendering
	}

	// Load the PAT file
	if (!character->loadPAT(patPath)) {
		// Failed to load - don't create the view
		return;
	}

	// Set up Parts rendering
	render.SetParts(&character->parts);

	// Add character to list and create PatEditor view
	characters.push_back(std::move(character));
	auto view = std::make_unique<CharacterView>(characters.back().get(), &render);
	view->setPatEditor(true);
	view->setViewNumber(0);

	// Refresh panes AFTER setting isPatEditor flag to create PatEditor panes
	// This will automatically hide HA6 editor panes and show PAT editor panes
	view->refreshPanes(&render);

	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
	markProjectModified();

	// Force dock layout rebuild to show PatEditor panes
	needsDockRebuild = true;
}

int MainFrame::countViewsForCharacter(CharacterInstance* character)
{
	int count = 0;
	for (auto& view : views) {
		if (view->getCharacter() == character) {
			count++;
		}
	}
	return count;
}

void MainFrame::closeView(int index)
{
	if (index >= 0 && index < views.size()) {
		auto* view = views[index].get();
		auto* character = view->getCharacter();

		// Remember it for Reopen closed tab (issue #61). Only views whose
		// character can be loaded again from a file.
		if (character && !view->isStageView() && !view->isPatEditor()) {
			ClosedTab t;
			t.path = !character->getTxtPath().empty() ? character->getTxtPath() : character->getTopHA6Path();
			t.isTxt = !character->getTxtPath().empty();
			t.pattern = view->getState().pattern;
			t.frame = view->getState().frame;
			if (!t.path.empty()) {
				m_closedTabs.push_back(t);
				if (m_closedTabs.size() > 10) m_closedTabs.erase(m_closedTabs.begin());
			}
		}

		// Remove the view
		views.erase(views.begin() + index);
		markProjectModified();

		// If this was the last view for this character, remove the character too
		if (character && countViewsForCharacter(character) == 0) {
			for (size_t i = 0; i < characters.size(); i++) {
				if (characters[i].get() == character) {
					characters.erase(characters.begin() + i);
					break;
				}
			}
		}

		// Update active index
		if (views.empty()) {
			activeViewIndex = -1;

			// Clear the render system
			render.DontDraw();
			render.ClearTexture();
			render.SetCg(nullptr);
			render.SetParts(nullptr);
		} else {
			// Select previous view, or first if we closed the first one
			if (activeViewIndex >= views.size()) {
				activeViewIndex = views.size() - 1;
			}
			setActiveView(activeViewIndex);
		}
	}
}

bool MainFrame::tryCloseView(int index)
{
	if (index < 0 || index >= views.size()) {
		return false;
	}

	auto& view = views[index];
	auto* character = view->getCharacter();

	// Only prompt if this is the last view and character is modified
	if (character && character->isModified() && countViewsForCharacter(character) == 1) {
		// Show unsaved changes dialog
		pendingCloseViewIndex = index;
		shouldOpenUnsavedDialog = true;
		return false; // Not closed yet, user needs to confirm
	}

	closeView(index);
	return true;
}

// ============================================================================
// Project Management
// ============================================================================

void MainFrame::markProjectModified()
{
	m_projectModified = true;
	updateWindowTitle();
}

void MainFrame::requestErrorPopup(const char* popupName, const std::string& detail)
{
	m_pendingErrorPopup = popupName;
	m_errorDetail = detail;
}

bool MainFrame::saveCharacter(CharacterInstance* character)
{
	if (!character) return false;
	if (character->save()) return true;
	const std::string& path = character->getTopHA6Path();
	if (character->frameData.isHA4() && !ha4::LastSaveError().empty()) {
		requestErrorPopup("Save Error", "MBAC .DAT not saved: " + ha4::LastSaveError());
		return false;
	}
	requestErrorPopup("Save Error", path.empty()
		? "Character '" + character->getName() + "' has no HA6 file to save to. Use Save Character As."
		: "Could not write " + path);
	return false;
}

bool MainFrame::saveCharacterAs(CharacterInstance* character, const std::string& path)
{
	if (!character) return false;
	if (character->saveAs(path)) return true;
	requestErrorPopup("Save Error", "Could not write " + path);
	return false;
}

bool MainFrame::saveAllModifiedCharacters()
{
	for (auto& character : characters) {
		if (character->isModified() && !saveCharacter(character.get())) {
			return false;
		}
	}
	return true;
}

void MainFrame::clearProjectState()
{
	views.clear();
	characters.clear();
	activeViewIndex = -1;
	pendingCloseViewIndex = -1;

	// Clear the render system
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);
	render.SetParts(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
}

void MainFrame::requestProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed)
{
	m_deferredProjectAction = action;
	m_deferredProjectPath = path;
	m_deferredProjectConfirmed = confirmed;
}

void MainFrame::processDeferredProjectAction()
{
	if (m_deferredProjectAction == ProjectCloseAction::None) {
		return;
	}
	ProjectCloseAction action = m_deferredProjectAction;
	std::string path = std::move(m_deferredProjectPath);
	bool confirmed = m_deferredProjectConfirmed;
	m_deferredProjectAction = ProjectCloseAction::None;
	m_deferredProjectPath.clear();
	m_deferredProjectConfirmed = false;
	runProjectAction(action, path, confirmed);
}

void MainFrame::runProjectAction(ProjectCloseAction action, const std::string& path, bool confirmed)
{
	// Ask about unsaved changes first; the dialog re-queues this action
	// (with the same path) as confirmed.
	if (!confirmed) {
		m_projectCloseAction = action;
		m_pendingProjectPath = path;
		if (!tryCloseProject()) {
			return;
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;
	m_pendingProjectPath.clear();

	switch (action) {
		case ProjectCloseAction::New:
		case ProjectCloseAction::Close:
			clearProjectState();
			break;
		case ProjectCloseAction::Open:
			if (path.empty()) {
				std::string chosen = FileDialog(fileType::HPROJ, false);
				if (!chosen.empty()) {
					loadProjectFromPath(chosen, false);
				}
			} else {
				loadProjectFromPath(path, true);
			}
			break;
		default:
			break;
	}
}

void MainFrame::loadProjectFromPath(const std::string& path, bool isRecent)
{
	int loadedTheme = style_idx;
	float loadedZoom = zoom_idx;
	bool loadedSmooth = smoothRender;
	float loadedColor[3] = { clearColor[0], clearColor[1], clearColor[2] };
	std::vector<std::string> failedCharacters;

	// LoadProject builds the new project off to the side and only replaces
	// characters/views when the file parsed; on failure the current project
	// is left untouched.
	std::vector<std::unique_ptr<CharacterInstance>> newCharacters;
	std::vector<std::unique_ptr<CharacterView>> newViews;
	int newActiveView = -1;
	if (ProjectManager::LoadProject(path, newCharacters, newViews, newActiveView, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor,
	                                &failedCharacters))
	{
		// Drop render references into the old project before it is destroyed.
		render.DontDraw();
		render.ClearTexture();
		render.SetCg(nullptr);
		render.SetParts(nullptr);
		pendingCloseViewIndex = -1;

		views = std::move(newViews);
		characters = std::move(newCharacters);
		activeViewIndex = newActiveView;

		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view
		if (activeViewIndex >= 0 && activeViewIndex < (int)views.size()) {
			setActiveView(activeViewIndex);
		}

		// Effect loading is now automatic per-character in CharacterInstance::loadFromTxt()

		ProjectManager::SetCurrentProjectPath(path);
		// A partially loaded project must not look clean: saving it would drop
		// the missing characters from the .hproj.
		m_projectModified = !failedCharacters.empty();
		addRecentProject(path);
		updateWindowTitle();

		if (!failedCharacters.empty()) {
			std::string detail = "These characters could not be loaded and were skipped:\n";
			for (const auto& f : failedCharacters) {
				detail += "  " + f + "\n";
			}
			detail += "Saving the project now would remove them from it.";
			requestErrorPopup("Project Load Error", detail);
		}
	} else {
		requestErrorPopup("Project Load Error", path);
		if (isRecent) {
			// Use normalized path comparison to find and remove the entry
			std::string normalizedPath = normalizePath(path);
			auto it = std::find_if(gSettings.recentProjects.begin(), gSettings.recentProjects.end(),
				[&normalizedPath](const std::string& existing) {
					return normalizePath(existing) == normalizedPath;
				});
			if (it != gSettings.recentProjects.end()) {
				gSettings.recentProjects.erase(it);
			}
		}
	}
}

void MainFrame::newProject()
{
	requestProjectAction(ProjectCloseAction::New, std::string(), false);
}

void MainFrame::openProject()
{
	requestProjectAction(ProjectCloseAction::Open, std::string(), false);
}

void MainFrame::saveProject()
{
	if (!ProjectManager::HasCurrentProject()) {
		saveProjectAs();
		return;
	}

	if (ProjectManager::SaveProject(ProjectManager::GetCurrentProjectPath(),
	                                characters, views, activeViewIndex,
	                                style_idx, zoom_idx, smoothRender, clearColor))
	{
		m_projectModified = false;
		updateWindowTitle();
	} else {
		requestErrorPopup("Project Save Error", ProjectManager::GetCurrentProjectPath());
	}
}

void MainFrame::saveProjectAs()
{
	std::string path = FileDialog(fileType::HPROJ, true);
	if (path.empty()) {
		return;
	}

	// Ensure .hproj extension
	if (path.find(".hproj") == std::string::npos) {
		path += ".hproj";
	}

	if (ProjectManager::SaveProject(path, characters, views, activeViewIndex,
	                                style_idx, zoom_idx, smoothRender, clearColor))
	{
		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		requestErrorPopup("Project Save Error", path);
	}
}

void MainFrame::closeProject()
{
	requestProjectAction(ProjectCloseAction::Close, std::string(), false);
}

void MainFrame::updateWindowTitle()
{
	// Build version string with build number and git hash
	std::wstring title = L"gonptéchan v" VERSION_WITH_COMMIT_W;

	if (ProjectManager::HasCurrentProject()) {
		const std::string& projectPath = ProjectManager::GetCurrentProjectPath();

		// Extract filename from path
		size_t lastSlash = projectPath.find_last_of("/\\");
		std::string filename = (lastSlash != std::string::npos)
			? projectPath.substr(lastSlash + 1)
			: projectPath;

		// Convert to wide string
		std::wstring wFilename(filename.begin(), filename.end());

		title = wFilename + L" - " + title;

		if (m_projectModified) {
			title = L"*" + title;
		}
	}

#ifdef _WIN32
	SetWindowTextW(mainWindowHandle, title.c_str());
#else
	// For GLFW, convert to UTF-8
	std::string utf8Title;
	for (wchar_t wc : title) {
		if (wc < 0x80) {
			utf8Title += static_cast<char>(wc);
		} else if (wc < 0x800) {
			utf8Title += static_cast<char>(0xC0 | (wc >> 6));
			utf8Title += static_cast<char>(0x80 | (wc & 0x3F));
		} else {
			utf8Title += static_cast<char>(0xE0 | (wc >> 12));
			utf8Title += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
			utf8Title += static_cast<char>(0x80 | (wc & 0x3F));
		}
	}
	glfwSetWindowTitle(mainWindowHandle, utf8Title.c_str());
#endif
}

bool MainFrame::tryCloseProject()
{
	if (m_projectModified || std::any_of(characters.begin(), characters.end(),
	                                     [](const auto& c) { return c->isModified(); }))
	{
		m_pendingProjectClose = true;
		shouldOpenUnsavedProjectDialog = true;
		return false; // Don't close yet, wait for user response
	}

	return true; // No unsaved changes, ok to close
}

void MainFrame::addRecentProject(const std::string& path)
{
	if (path.empty()) {
		return;
	}

	std::string normalizedPath = normalizePath(path);

	// Remove if already exists (compare normalized paths)
	auto it = std::find_if(gSettings.recentProjects.begin(), gSettings.recentProjects.end(),
		[&normalizedPath](const std::string& existing) {
			return normalizePath(existing) == normalizedPath;
		});
	
	if (it != gSettings.recentProjects.end()) {
		gSettings.recentProjects.erase(it);
	}

	// Add to front (store normalized path for consistency)
	// Use forward slashes as the canonical format for storage
	gSettings.recentProjects.insert(gSettings.recentProjects.begin(), normalizedPath);

	// Keep only last 10
	if (gSettings.recentProjects.size() > 10) {
		gSettings.recentProjects.resize(10);
	}
}

void MainFrame::openRecentProject(const std::string& path)
{
	// Deferred: the caller iterates gSettings.recentProjects, which a failed
	// load modifies. The path is carried through the unsaved-changes prompt.
	requestProjectAction(ProjectCloseAction::Open, path, false);
}


// ---------------------------------------------------------------------------
// Background (stage) load / clear. Each loaded stage becomes its own view
// (tab) so it can be docked / undocked alongside character tabs and live in
// its own viewport. currentBgFile mirrors the active tab's stage file so
// existing menu items and the inspector keep working without knowing about
// the view abstraction.

void MainFrame::loadStageFile(const std::string& path)
{
	auto file = std::make_unique<bg::File>();
	if (!file->Load(path.c_str()))
		return;

	std::string displayName = path;
	auto slash = displayName.find_last_of("/\\");
	if (slash != std::string::npos) displayName = displayName.substr(slash + 1);

	auto view = std::make_unique<CharacterView>(nullptr, &render);
	view->setStageFile(std::move(file), displayName);

	// Default the camera so world (0, 0) lands at u4ick's proportional
	// character-feet anchor: 401/1280 across and 538/720 down of the
	// viewport. That's where his SaveRender output puts character feet,
	// and matches the 'fire at bottom near feet' layout the user sees in
	// his tool. Diagnostics (Screenshot 2026-05) confirmed the underlying
	// math is correct end-to-end — the only choice was where to put the
	// initial anchor, and (0, 0) only matches u4ick's *uninteracted*
	// default (everything off-screen until you drag).
	float clientW = clientRect.x > 0 ? clientRect.x : 1280.0f;
	float clientH = clientRect.y > 0 ? clientRect.y : 720.0f;
	float defaultPanX = clientW * (401.0f / 1280.0f);
	float defaultPanY = clientH * (538.0f / 720.0f);
	view->setStageRenderXY(defaultPanX, defaultPanY);
	view->setStageRenderInit(true);
	bgCamera.SetPan(defaultPanX, defaultPanY);

	views.push_back(std::move(view));
	setActiveView((int)views.size() - 1);
}

void MainFrame::clearStage()
{
	// Drop the active stage view (if any) and detach the renderer.
	bgRenderer.SetFile(nullptr);
	bgRenderer.SetEnabled(false);
	currentBgFile = nullptr;

	if (activeViewIndex >= 0 && activeViewIndex < (int)views.size()) {
		auto* view = views[activeViewIndex].get();
		if (view && view->isStageView()) {
			views.erase(views.begin() + activeViewIndex);
			if (views.empty())
				activeViewIndex = -1;
			else if (activeViewIndex >= (int)views.size())
				setActiveView((int)views.size() - 1);
			else
				setActiveView(activeViewIndex);
		}
	}
}
