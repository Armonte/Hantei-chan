#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"
#include "imgui_utils.h"
#include "project_manager.h"
#include "preset_effects.h"
#include "version.h"
#include "framestate.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>

MainFrame::MainFrame(ContextGl *context_):
context(context_)
{
	LoadSettings();
}

MainFrame::~MainFrame()
{
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

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

void MainFrame::DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character)
{
	if (!state.vizSettings.showPresetEffects) return;

	// Get ImGui draw list for overlay
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();

	// Iterate through active spawns or spawned patterns
	bool useActiveSpawns = state.animating && !state.activeSpawns.empty();
	size_t count = useActiveSpawns ? state.activeSpawns.size() : state.spawnedPatterns.size();

	for (size_t i = 0; i < count; i++) {
		// Get spawn info
		bool isPreset;
		int offsetX, offsetY, presetNumber;
		int spawnFrame, spawnTick;

		if (useActiveSpawns) {
			auto& spawn = state.activeSpawns[i];
			if (!spawn.isPresetEffect) continue;
			isPreset = true;
			offsetX = spawn.offsetX;
			offsetY = spawn.offsetY;
			presetNumber = spawn.patternId;
			spawnTick = spawn.spawnTick;

			// Check if we're on the spawn tick (unless showing on all frames)
			if (!state.vizSettings.presetEffectsAllFrames && state.currentTick != spawnTick) {
				continue;
			}
		} else {
			auto& spawn = state.spawnedPatterns[i];
			if (!spawn.isPresetEffect || !spawn.visible) continue;
			isPreset = true;
			offsetX = spawn.offsetX;
			offsetY = spawn.offsetY;
			presetNumber = spawn.patternId;
			spawnFrame = spawn.parentFrame;

			// Check if we're on the spawn frame (unless showing on all frames)
			if (!state.vizSettings.presetEffectsAllFrames && state.frame != spawnFrame) {
				continue;
			}
		}

		// Convert game coordinates to screen coordinates
		// offsetX/offsetY are in game space units (same as render.x/y)
		// render.x/y formula: (character->renderX + clientRect.x/2) / render.scale
		// So to go back: screenX = character->renderX + clientRect.x/2 + offsetX * render.scale
		float screenX = character->renderX + clientRect.x / 2 + offsetX * render.scale;
		float screenY = character->renderY + clientRect.y / 2 + offsetY * render.scale;

		// Draw crosshair
		float size = 15.0f;
		ImU32 color = IM_COL32(255, 128, 0, 255);  // Orange
		float thickness = 2.0f;

		ImVec2 center(screenX, screenY);

		// Horizontal line
		drawList->AddLine(
			ImVec2(center.x - size, center.y),
			ImVec2(center.x + size, center.y),
			color, thickness);

		// Vertical line
		drawList->AddLine(
			ImVec2(center.x, center.y - size),
			ImVec2(center.x, center.y + size),
			color, thickness);

		// Center circle
		drawList->AddCircleFilled(center, 3.0f, color);

		// Label with preset name (if labels enabled)
		if (state.vizSettings.showLabels) {
			const char* presetName = GetPresetEffectName(presetNumber);
			char label[64];
			snprintf(label, sizeof(label), "%s [%d]", presetName, presetNumber);
			drawList->AddText(
				ImVec2(center.x + size + 5, center.y - 8),
				color, label);
		}
	}
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);

	auto* active = getActiveCharacter();
	if (active) {
		render.x = (active->renderX + clientRect.x/2) / render.scale;
		render.y = (active->renderY + clientRect.y/2) / render.scale;
	}

	auto* view = getActiveView();

	// Check if we need to draw with spawned patterns
	bool hasSpawnedPatterns = false;
	if (view && active) {
		auto& state = view->getState();
		hasSpawnedPatterns = state.vizSettings.showSpawnedPatterns && !state.spawnedPatterns.empty();
	}

	if (hasSpawnedPatterns && view && active) {
		// Draw everything as layers (main + spawned) in Z-order
		auto& state = view->getState();

		// First draw grid lines only
		render.DrawGridLines();

		// Get current main pattern sequence
		auto mainSeq = active->frameData.get_sequence(state.pattern);
		if (!mainSeq || mainSeq->frames.empty()) return;
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
			mainLayer.AFRT = mainFrame.AF.AFRT;
			mainLayer.blendMode = mainLayer_data.blend_mode;
			mainLayer.zPriority = mainFrame.AF.priority;
			mainLayer.alpha = mainLayer_data.rgba[3];  // Apply frame alpha
			mainLayer.tintColor = glm::vec4(mainLayer_data.rgba[0], mainLayer_data.rgba[1], mainLayer_data.rgba[2], 1.0f);  // Apply frame RGB
			mainLayer.isSpawned = false;
			mainLayer.hitboxes = (layerIndex == 0) ? mainFrame.hitboxes : BoxList();  // Only layer 0 gets hitboxes
			mainLayer.sourceCG = &active->cg;  // Main pattern uses character CG
			mainLayer.usePat = mainLayer_data.usePat;  // Copy PAT rendering flag from layer data
			mainLayer.sourceParts = &active->parts;  // Main pattern uses character Parts
			render.AddLayer(mainLayer);
		}

		// Save reference to main pattern's layer 0 for inherited rotation (spawns may need it)
		const auto& mainLayer0Data = mainFrame.AF.layers[0];

		// Build render layers for spawned patterns
		// Use activeSpawns during animation OR when seeking (handles looping correctly)
		// When seeking, activeSpawns is populated by SimulateSpawnsToTick in box_pane.cpp
		// IMPORTANT: Use activeSpawns if it has entries (either from animation or seeking)
		bool useActiveSpawns = state.animating || !state.activeSpawns.empty();
		auto& spawnsToRender = useActiveSpawns ?
			reinterpret_cast<std::vector<ActiveSpawnInstance>&>(state.activeSpawns) :
			reinterpret_cast<std::vector<ActiveSpawnInstance>&>(state.spawnedPatterns);

		for (size_t i = 0; i < (useActiveSpawns ? state.activeSpawns.size() : state.spawnedPatterns.size()); i++) {
			// Get spawn info from appropriate source
			ActiveSpawnInstance spawnInfo;

			if (useActiveSpawns) {
				spawnInfo = state.activeSpawns[i];
			} else {
				// Convert SpawnedPatternInfo to ActiveSpawnInstance for rendering
				auto& staticSpawn = state.spawnedPatterns[i];
				if (!staticSpawn.visible) continue;

				spawnInfo.spawnTick = staticSpawn.spawnTick;
				spawnInfo.patternId = staticSpawn.patternId;
				spawnInfo.usesEffectHA6 = staticSpawn.usesEffectHA6;
				spawnInfo.isPresetEffect = staticSpawn.isPresetEffect;
				spawnInfo.offsetX = staticSpawn.offsetX;
				spawnInfo.offsetY = staticSpawn.offsetY;
				spawnInfo.flagset1 = staticSpawn.flagset1;
				spawnInfo.flagset2 = staticSpawn.flagset2;
				spawnInfo.angle = staticSpawn.angle;
				spawnInfo.projVarDecrease = staticSpawn.projVarDecrease;
				spawnInfo.tintColor = state.vizSettings.enableTint ? staticSpawn.tintColor : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
				spawnInfo.alpha = staticSpawn.alpha;
			}

			// Check if this is a preset effect (Effect Type 3)
			if (spawnInfo.isPresetEffect) {
				// Render crosshair marker for preset effects (not pattern-based)
				if (state.vizSettings.showPresetEffects) {
					// TODO: Draw crosshair at (spawnInfo.offsetX, spawnInfo.offsetY)
					// Preset number: spawnInfo.patternId
					// For now, skip rendering (will be implemented below)
				}
				continue;  // Skip pattern loading for preset effects
			}

			// Determine which character to pull pattern from
			FrameData* sourceFrameData;
			CG* sourceCG;
			Parts* sourceParts;

			if (spawnInfo.usesEffectHA6) {
				// Type 8: Effect spawn - ALWAYS use effectCharacter if loaded
				// effectCharacter contains either MBAACC's effect.ha6/effect.cg or UNI's effect.ha6/sys_effect.pat
				if (active->effectCharacter) {
					// Use effect.ha6 data (both MBAACC and UNI)
					sourceFrameData = &active->effectCharacter->frameData;
					sourceCG = &active->effectCharacter->cg;
					sourceParts = &active->effectCharacter->parts;
				} else {
					// Fallback: no effect.ha6 loaded - use main character
					static bool warned = false;
					if (!warned) {
						printf("[Warning] Type 8 spawn detected but no effect.ha6 loaded for %s - using main character as fallback\n",
							active->getName().c_str());
						warned = true;
					}
					sourceFrameData = &active->frameData;
					sourceCG = &active->cg;
					sourceParts = &active->parts;
				}
			} else {
				// Type 1/11/101/111/1000: Pull from main character
				sourceFrameData = &active->frameData;
				sourceCG = &active->cg;
				sourceParts = &active->parts;
			}

			// Get the spawned pattern's sequence from appropriate source
			auto spawnedSeq = sourceFrameData->get_sequence(spawnInfo.patternId);
			if (!spawnedSeq || spawnedSeq->frames.empty()) continue;

			// Calculate which frame to display
			// When animating with activeSpawns: use currentFrame (respects aniType 2 loops/jumps)
			// When paused with spawnedPatterns: calculate from ticks (for seeking)
			int localFrame = 0;

			if (useActiveSpawns) {
				// Use currentFrame directly (advanced by animation logic in main_ui_impl.h)
				localFrame = spawnInfo.currentFrame;

				// Skip if frame is invalid (pattern has ended)
				if (localFrame < 0 || localFrame >= spawnedSeq->frames.size()) {
					continue;
				}
			} else {
				// Static spawn from spawnedPatterns - calculate frame from ticks for seeking
				// Check if current frame (via simulation) matches the spawn frame
				
				// Get main pattern to check if it loops
				auto mainSeq = active->frameData.get_sequence(state.pattern);
				bool mainPatternLoops = false;
				int mainPatternLoopPeriod = 0;
				
				if (mainSeq && !mainSeq->frames.empty()) {
					auto& lastFrame = mainSeq->frames.back();
					mainPatternLoops = (lastFrame.AF.aniType == 2);
					
					// Calculate loop period if looping
					if (mainPatternLoops) {
						mainPatternLoopPeriod = FindLoopPeriod(&active->frameData, state.pattern);
						if (mainPatternLoopPeriod == 0) {
							// Fallback: calculate total duration
							for (int i = 0; i < mainSeq->frames.size(); i++) {
								mainPatternLoopPeriod += mainSeq->frames[i].AF.duration;
							}
							if (mainPatternLoopPeriod == 0) {
								mainPatternLoopPeriod = mainSeq->frames.size() * 10; // Fallback estimate
							}
						}
					}
				}
				
				// Get the current frame via simulation
				int currentFrame = SimulateAnimationFlow(&active->frameData, state.pattern, state.currentTick);
				
				// Get the spawn frame from the static spawn info
				// We need to find which frame in the pattern spawned this
				// The spawn tree records absoluteSpawnFrame, but we need the relative frame
				int spawnFrame = -1;
				if (mainSeq) {
					// Find the frame that has this spawn effect
					for (int i = 0; i < mainSeq->frames.size(); i++) {
						if (!mainSeq->frames[i].EF.empty()) {
							auto frameSpawns = ParseSpawnedPatterns(mainSeq->frames[i].EF, i, state.pattern);
							for (const auto& fs : frameSpawns) {
								if (fs.patternId == spawnInfo.patternId && 
								    fs.usesEffectHA6 == spawnInfo.usesEffectHA6 &&
								    fs.effectType == static_cast<int>(spawnInfo.isPresetEffect ? 3 : 1)) {
									spawnFrame = i;
									break;
								}
							}
							if (spawnFrame >= 0) break;
						}
					}
				}
				
				// Check if we're at the spawn frame (including loop iterations)
				bool isAtSpawnFrame = false;
				int effectiveSpawnTick = spawnInfo.spawnTick;
				
				if (spawnFrame >= 0) {
					// Check if current frame matches spawn frame
					isAtSpawnFrame = (currentFrame == spawnFrame);
					
					// If looping, also check if we're at the spawn frame in a loop iteration
					if (mainPatternLoops && mainPatternLoopPeriod > 0 && !isAtSpawnFrame) {
						// Check if tick modulo loop period puts us at spawn frame
						int tickInLoop = state.currentTick % mainPatternLoopPeriod;
						int spawnTickInLoop = spawnInfo.spawnTick % mainPatternLoopPeriod;
						int frameAtSpawnTickInLoop = SimulateAnimationFlow(&active->frameData, state.pattern, spawnTickInLoop);
						
						if (frameAtSpawnTickInLoop == spawnFrame) {
							// Check if current tick in loop matches spawn tick in loop
							int frameAtCurrentTickInLoop = SimulateAnimationFlow(&active->frameData, state.pattern, tickInLoop);
							isAtSpawnFrame = (frameAtCurrentTickInLoop == spawnFrame);
							
							if (isAtSpawnFrame) {
								// Calculate effective spawn tick for this loop iteration
								effectiveSpawnTick = (state.currentTick / mainPatternLoopPeriod) * mainPatternLoopPeriod + spawnTickInLoop;
							}
						}
					}
				} else {
					// Fallback: use tick-based matching
					if (mainPatternLoops && mainPatternLoopPeriod > 0) {
						int tickInLoop = state.currentTick % mainPatternLoopPeriod;
						isAtSpawnFrame = (tickInLoop == spawnInfo.spawnTick % mainPatternLoopPeriod);
						effectiveSpawnTick = (state.currentTick / mainPatternLoopPeriod) * mainPatternLoopPeriod + (spawnInfo.spawnTick % mainPatternLoopPeriod);
					} else {
						isAtSpawnFrame = (state.currentTick == spawnInfo.spawnTick);
					}
				}
				
				int elapsedTicks = state.currentTick - effectiveSpawnTick;

				// Check if spawned pattern should be visible
				if (!isAtSpawnFrame && elapsedTicks < 0) {
					// Before spawn tick - don't show
					continue;
				}

				// Calculate total pattern duration and check for looping
				int totalDuration = 0;
				for (int i = 0; i < spawnedSeq->frames.size(); i++) {
					int dur = spawnedSeq->frames[i].AF.duration;
					// Safety: treat duration 0 as 1 to avoid infinite loops
					totalDuration += (dur > 0 ? dur : 1);
				}

				// Check if pattern should loop
				bool isLooping = false;
				if (!spawnedSeq->frames.empty()) {
					auto& lastFrame = spawnedSeq->frames.back();
					isLooping = (lastFrame.AF.aniType == 2);
				}

				// Check if pattern has ended (non-looping)
				if (!isLooping && elapsedTicks >= totalDuration) {
					// Pattern finished, don't show
					continue;
				}

				// Handle looping by wrapping elapsed ticks
				int effectiveTicks = isLooping ? (elapsedTicks % totalDuration) : elapsedTicks;
				if (effectiveTicks < 0) effectiveTicks = 0; // Safety

				// Use flow simulation to find frame (handles loops/jumps properly)
				localFrame = SimulateAnimationFlow(sourceFrameData, spawnInfo.patternId, effectiveTicks);
			}

			auto& spawnedFrame = spawnedSeq->frames[localFrame];

			// Ensure spawned frame has at least one layer
			if (spawnedFrame.AF.layers.empty()) {
				spawnedFrame.AF.layers.push_back({});
			}

			// Loop through all layers in spawned frame (UNI multi-layer support)
			for (size_t spawnLayerIndex = 0; spawnLayerIndex < spawnedFrame.AF.layers.size(); spawnLayerIndex++) {
				const auto& spawnedLayer_data = spawnedFrame.AF.layers[spawnLayerIndex];

				// Create render layer with all frame data
				RenderLayer layer;
				layer.spriteId = spawnedLayer_data.spriteId;
				layer.spawnOffsetX = spawnInfo.offsetX;
				layer.spawnOffsetY = spawnInfo.offsetY;
				layer.frameOffsetX = spawnedLayer_data.offset_x;
				layer.frameOffsetY = spawnedLayer_data.offset_y;
				layer.scaleX = spawnedLayer_data.scale[0];
				layer.scaleY = spawnedLayer_data.scale[1];
				layer.rotX = spawnedLayer_data.rotation[0];
				layer.rotY = spawnedLayer_data.rotation[1];
				layer.rotZ = spawnedLayer_data.rotation[2];
				layer.AFRT = spawnedFrame.AF.AFRT;

				// Apply spawn rotation parameter (angle)
				// Rotation format: 0=0°, 2500=90°, 5000=180°, 10000=360°
				float spawnRotation = spawnInfo.angle / 10000.0f;
				layer.rotZ += spawnRotation;

				// Apply flip facing flag (bit 11 of flagset1)
				if (spawnInfo.flagset1 & (1 << 11)) {
					layer.scaleX *= -1.0f;
				}

				// Apply inherit parent rotation flag (bit 8 of flagset1)
				// Inherit from parent pattern's layer 0 (mainLayer0Data)
				if (spawnInfo.flagset1 & (1 << 8)) {
					layer.rotX += mainLayer0Data.rotation[0];
					layer.rotY += mainLayer0Data.rotation[1];
					layer.rotZ += mainLayer0Data.rotation[2];
				}

				layer.blendMode = spawnedLayer_data.blend_mode;
				// Use persistent z-priority from spawn instance (ZP=0 means "keep current")
				// When animating, use currentZPriority (updated only when frame has non-zero ZP)
				// When paused/seeking, use frame's priority directly
				layer.zPriority = useActiveSpawns ? spawnInfo.currentZPriority : spawnedFrame.AF.priority;
				// Apply frame RGBA, then visualization alpha
				layer.alpha = spawnedLayer_data.rgba[3] * spawnInfo.alpha * state.vizSettings.spawnedOpacity;
				// Multiply frame RGB with visualization tint
				layer.tintColor = glm::vec4(
					spawnedLayer_data.rgba[0] * spawnInfo.tintColor.r,
					spawnedLayer_data.rgba[1] * spawnInfo.tintColor.g,
					spawnedLayer_data.rgba[2] * spawnInfo.tintColor.b,
					1.0f);
				layer.isSpawned = true;
				layer.hitboxes = (spawnLayerIndex == 0) ? spawnedFrame.hitboxes : BoxList();  // Only layer 0 gets hitboxes
				layer.sourceCG = sourceCG;  // Use appropriate CG (character or effect.ha6)
				layer.usePat = spawnedLayer_data.usePat;  // Copy PAT rendering flag
				layer.sourceParts = sourceParts;  // Use appropriate Parts (character or effect.pat)
				layer.spawnFlagset1 = spawnInfo.flagset1;
				layer.spawnFlagset2 = spawnInfo.flagset2;

				render.AddLayer(layer);
			}
		}

		// Sort all layers (including main) by Z-priority before drawing
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
			mainLayer.AFRT = mainFrame.AF.AFRT;
			mainLayer.blendMode = mainLayer_data.blend_mode;
			mainLayer.zPriority = mainFrame.AF.priority;
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

void MainFrame::newProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::New) {
		m_projectCloseAction = ProjectCloseAction::New;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	// Clear all views and characters
	views.clear();
	characters.clear();
	activeViewIndex = -1;
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);
	render.SetParts(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
}

void MainFrame::openProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::Open) {
		m_projectCloseAction = ProjectCloseAction::Open;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	std::string path = FileDialog(fileType::HPROJ, false);
	if (path.empty()) {
		return;
	}

	int loadedTheme;
	float loadedZoom;
	bool loadedSmooth;
	float loadedColor[3];

	if (ProjectManager::LoadProject(path, characters, views, activeViewIndex, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor))
	{
		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view
		if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
			setActiveView(activeViewIndex);
		}

		// Effect loading is now automatic per-character in CharacterInstance::loadFromTxt()

		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		// Show error popup
		ImGui::OpenPopup("Project Load Error");
	}
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
		ImGui::OpenPopup("Project Save Error");
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
		ImGui::OpenPopup("Project Save Error");
	}
}

void MainFrame::closeProject()
{
	// Check for unsaved changes (only if not already handling a close action)
	if (m_projectCloseAction != ProjectCloseAction::Close) {
		m_projectCloseAction = ProjectCloseAction::Close;
		if (!tryCloseProject()) {
			return; // Dialog will handle the action
		}
	}
	m_projectCloseAction = ProjectCloseAction::None;

	// Clear all views and characters
	views.clear();
	characters.clear();
	activeViewIndex = -1;

	// Clear the render system
	render.DontDraw();
	render.ClearTexture();
	render.SetCg(nullptr);
	render.SetParts(nullptr);

	ProjectManager::ClearCurrentProjectPath();
	m_projectModified = false;
	updateWindowTitle();
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
	// Remove if already exists
	auto it = std::find(gSettings.recentProjects.begin(), gSettings.recentProjects.end(), path);
	if (it != gSettings.recentProjects.end()) {
		gSettings.recentProjects.erase(it);
	}

	// Add to front
	gSettings.recentProjects.insert(gSettings.recentProjects.begin(), path);

	// Keep only last 10
	if (gSettings.recentProjects.size() > 10) {
		gSettings.recentProjects.resize(10);
	}
}

void MainFrame::openRecentProject(const std::string& path)
{
	// Check for unsaved changes
	m_projectCloseAction = ProjectCloseAction::Open;
	if (!tryCloseProject()) {
		return; // Dialog will handle the action
	}
	m_projectCloseAction = ProjectCloseAction::None;

	int loadedTheme;
	float loadedZoom;
	bool loadedSmooth;
	float loadedColor[3];

	if (ProjectManager::LoadProject(path, characters, views, activeViewIndex, &render,
	                                &loadedTheme, &loadedZoom, &loadedSmooth, loadedColor))
	{
		// Apply loaded UI state
		LoadTheme(loadedTheme);
		SetZoom(loadedZoom);
		smoothRender = loadedSmooth;
		ChangeClearColor(loadedColor[0], loadedColor[1], loadedColor[2]);

		// Set active view
		if (activeViewIndex >= 0 && activeViewIndex < views.size()) {
			setActiveView(activeViewIndex);
		}

		// Effect loading is now automatic per-character in CharacterInstance::loadFromTxt()

		ProjectManager::SetCurrentProjectPath(path);
		m_projectModified = false;
		addRecentProject(path);
		updateWindowTitle();
	} else {
		// Show error popup and remove from recent
		ImGui::OpenPopup("Project Load Error");
		auto it = std::find(gSettings.recentProjects.begin(), gSettings.recentProjects.end(), path);
		if (it != gSettings.recentProjects.end()) {
			gSettings.recentProjects.erase(it);
		}
	}
}

