#ifndef FRAME_DISP_AF_H_GUARD
#define FRAME_DISP_AF_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"
#include <cstring> // for memcpy

// ============================================================================
// Animation Frame (AF) Display  
// ============================================================================
// Displays and edits Frame_AF data (sprite, animation, transforms)
//
// Frame_AF controls:
// - Sprite selection and .pat usage
// - Animation control (duration, loops, jumps, landing frames)
// - Z-priority
// - Visual transforms (position, rotation, scale, color, blend mode)
// - Interpolation
// ============================================================================

inline void AfDisplay(Frame_AF *af, int &selectedLayer, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr, Frame_AF *clipboard = nullptr)
{
	// Helper lambda to mark both frameData and character as modified
	auto markModified = [&]() {
		if (frameData && patternIndex >= 0) {
			frameData->mark_modified(patternIndex);
		}
		if (onModified) {
			onModified();
		}
	};

	// Ensure at least one layer exists for MBAACC compatibility
	if (af->layers.empty()) {
		af->layers.push_back({});
	}

	// Clamp selectedLayer to valid range
	if (selectedLayer < 0 || selectedLayer >= (int)af->layers.size()) {
		selectedLayer = 0;
	}

	static Layer_Type copiedAnimationLayer = {};
	static Frame_AF copiedAnimationFrame = {};

	constexpr float width = 50.f;

	// ============================================================================
	// Multi-Layer UI (UNI AFGX support)
	// ============================================================================
	bool hasMultipleLayers = af->layers.size() > 1;

	// Layer management section
	im::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));  // Blue header
	if (hasMultipleLayers) {
		im::Text("Multi-Layer (UNI AFGX) - Layer %d/%d", selectedLayer + 1, (int)af->layers.size());
	} else {
		im::Text("Single Layer (MBAACC AFGP)");
	}
	im::PopStyleColor();

	im::SameLine(0, 20.f);
	if (im::SmallButton("Add Layer")) {
		// Add new layer with default values
		Layer_Type newLayer = {};
		newLayer.spriteId = -1;
		newLayer.usePat = false;
		newLayer.rgba[0] = newLayer.rgba[1] = newLayer.rgba[2] = newLayer.rgba[3] = 1.0f;
		newLayer.scale[0] = newLayer.scale[1] = 1.0f;
		newLayer.priority = 0;
		af->layers.push_back(newLayer);
		selectedLayer = af->layers.size() - 1;  // Select newly added layer
		markModified();
	}
	if (im::IsItemHovered()) Tooltip("Add a new layer (creates UNI AFGX multi-layer format)");

	// Only show delete button if we have more than 1 layer
	if (hasMultipleLayers) {
		im::SameLine();
		if (im::SmallButton("Delete Layer")) {
			if (af->layers.size() > 1) {
				af->layers.erase(af->layers.begin() + selectedLayer);
				// Clamp selectedLayer after deletion
				if (selectedLayer >= (int)af->layers.size()) {
					selectedLayer = af->layers.size() - 1;
				}
				markModified();
			}
		}
		if (im::IsItemHovered()) Tooltip("Delete current layer (reverts to AFGP if only 1 layer remains)");
	}

	// Layer selector dropdown (only show if we have multiple layers)
	if (hasMultipleLayers) {
		im::SetNextItemWidth(width * 2);
		if (im::Combo("##LayerSelector", &selectedLayer,
			[](void* data, int idx) -> const char* {
				static char buf[32];
				snprintf(buf, sizeof(buf), "Layer %d", idx);
				return buf;
			}, nullptr, af->layers.size())) {
			// Layer selection changed - no need to mark modified
		}
		if (im::IsItemHovered()) Tooltip("Select which layer to edit");

		// Layer priority (UNI AFPL tag) - only shown for multi-layer
		im::SameLine(0, 20.f);
		im::SetNextItemWidth(width);
		if (im::InputInt("Priority", &af->layers[selectedLayer].priority, 0, 0)) {
			markModified();
		}
		if (im::IsItemHovered()) Tooltip("Layer Z-priority (UNI AFPL tag) - higher renders on top");
	}

	im::Separator();

	// Get reference to the layer we're editing
	Layer_Type& layer = af->layers[selectedLayer];

	// Sprite and .pat (per-layer properties)
	im::SetNextItemWidth(width*3);
	if(im::InputInt("Sprite", &layer.spriteId)) {
		markModified();
	}
	im::SameLine(0, 20.f);
	if(im::Checkbox("Use .pat", &layer.usePat)) {
		markModified();
	}

	if(im::SmallButton("Copy animation")) {
		copiedAnimationLayer.spriteId = layer.spriteId;
		copiedAnimationLayer.usePat = layer.usePat;
		copiedAnimationFrame.duration = af->duration;
		copiedAnimationFrame.aniType = af->aniType;
		copiedAnimationFrame.aniFlag = af->aniFlag;
		copiedAnimationFrame.jump = af->jump;
		copiedAnimationFrame.landJump = af->landJump;
		copiedAnimationFrame.priority = af->priority;
		copiedAnimationFrame.loopCount = af->loopCount;
		copiedAnimationFrame.loopEnd = af->loopEnd;
	}
	if(im::IsItemHovered()) Tooltip("Copy sprite, duration, jumps, priority, and loops");
	im::SameLine();
	if(im::SmallButton("Paste animation")) {
		layer.spriteId = copiedAnimationLayer.spriteId;
		layer.usePat = copiedAnimationLayer.usePat;
		af->duration = copiedAnimationFrame.duration;
		af->aniType = copiedAnimationFrame.aniType;
		af->aniFlag = copiedAnimationFrame.aniFlag;
		af->jump = copiedAnimationFrame.jump;
		af->landJump = copiedAnimationFrame.landJump;
		af->priority = copiedAnimationFrame.priority;
		af->loopCount = copiedAnimationFrame.loopCount;
		af->loopEnd = copiedAnimationFrame.loopEnd;
		markModified();
	}
	if(im::IsItemHovered()) Tooltip("Paste sprite, duration, jumps, priority, and loops");

	im::Separator();

	unsigned int flagIndex = -1;
	if(BitField("Animation flags", &af->aniFlag, &flagIndex, 4)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Landing frame: Land to pattern"); break;
		case 1: Tooltip("Loop: Decrement loops and go to end if 0"); break;
		case 2: Tooltip("Go to: Use relative offset"); break;
		case 3: Tooltip("End of loop: Use relative offset"); break;
	}

	if(im::Combo("Animation", &af->aniType, animationList, IM_ARRAYSIZE(animationList))) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Go to", &af->jump, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Landing frame", &af->landJump, 0, 0)) {
		markModified();
	}

	// Landing frame tools
	if(frameData && patternIndex >= 0) {
		static int landingFrameToolValue = 0;
		static int landingFrameRange[2] = {0, 0};

		im::SetNextItemWidth(width);
		im::InputInt("##LandingValue", &landingFrameToolValue, 0, 0);
		if(im::IsItemHovered()) Tooltip("Landing frame value to set");

		im::SameLine();
		if(im::SmallButton("Set All")) {
			auto seq = frameData->get_sequence(patternIndex);
			if(seq) {
				for(int i = 0; i < seq->frames.size(); i++) {
					seq->frames[i].AF.landJump = landingFrameToolValue;
				}
				frameData->mark_modified(patternIndex);
				markModified();
			}
		}
		if(im::IsItemHovered()) Tooltip("Apply landing frame to all frames in pattern");

		im::SameLine();
		if(im::SmallButton("Set Range")) {
			im::OpenPopup("LandingFrameRange");
		}
		if(im::IsItemHovered()) Tooltip("Apply landing frame to a range of frames");

		// Range popup
		if(im::BeginPopup("LandingFrameRange")) {
			auto seq = frameData->get_sequence(patternIndex);
			if(seq) {
				const int maxFrame = seq->frames.size() - 1;

				// Clamp range values
				if(landingFrameRange[0] < 0) landingFrameRange[0] = 0;
				if(landingFrameRange[1] < 0) landingFrameRange[1] = 0;
				if(landingFrameRange[0] > maxFrame) landingFrameRange[0] = maxFrame;
				if(landingFrameRange[1] > maxFrame) landingFrameRange[1] = maxFrame;

				im::Text("Set landing frame for range");
				im::Separator();
				im::InputInt2("Frame range", landingFrameRange);
				im::InputInt("Landing frame value", &landingFrameToolValue);

				if(im::Button("Apply", ImVec2(120, 0))) {
					for(int i = landingFrameRange[0]; i <= landingFrameRange[1] && i >= 0 && i < seq->frames.size(); i++) {
						seq->frames[i].AF.landJump = landingFrameToolValue;
					}
					frameData->mark_modified(patternIndex);
					markModified();
					im::CloseCurrentPopup();
				}
				im::SameLine();
				if(im::Button("Cancel", ImVec2(120, 0))) {
					im::CloseCurrentPopup();
				}
			}
			im::EndPopup();
		}
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Z-Priority", &af->priority, 0, 0)) {
		markModified();
	}
	im::SetNextItemWidth(width);
	if(im::InputInt("Loop N times", &af->loopCount, 0, 0)) {
		markModified();
	}
	im::SameLine(0,20); im::SetNextItemWidth(width);
	if(im::InputInt("End of loop", &af->loopEnd, 0, 0)) {
		markModified();
	}
	if(im::InputInt("Duration", &af->duration, 1, 0)) {
		markModified();
	}

	im::Separator();

	// Copy/paste buttons for transform section
	static Layer_Type copiedTransformLayer = {};
	static bool copiedTransformAFRT = false;
	static int copiedTransformInterpolationType = 0;
	if(im::SmallButton("Copy transforms")) {
		copiedTransformLayer.offset_x = layer.offset_x;
		copiedTransformLayer.offset_y = layer.offset_y;
		copiedTransformLayer.blend_mode = layer.blend_mode;
		memcpy(copiedTransformLayer.rgba, layer.rgba, sizeof(layer.rgba));
		memcpy(copiedTransformLayer.rotation, layer.rotation, sizeof(layer.rotation));
		memcpy(copiedTransformLayer.scale, layer.scale, sizeof(layer.scale));
		copiedTransformAFRT = af->AFRT;
		copiedTransformInterpolationType = af->interpolationType;
	}
	if(im::IsItemHovered()) Tooltip("Copy offset, rotation, scale, color, blend mode, and interpolation");
	im::SameLine();
	if(im::SmallButton("Paste transforms")) {
		layer.offset_x = copiedTransformLayer.offset_x;
		layer.offset_y = copiedTransformLayer.offset_y;
		layer.blend_mode = copiedTransformLayer.blend_mode;
		memcpy(layer.rgba, copiedTransformLayer.rgba, sizeof(layer.rgba));
		memcpy(layer.rotation, copiedTransformLayer.rotation, sizeof(layer.rotation));
		memcpy(layer.scale, copiedTransformLayer.scale, sizeof(layer.scale));
		af->AFRT = copiedTransformAFRT;
		af->interpolationType = copiedTransformInterpolationType;
		markModified();
	}
	if(im::IsItemHovered()) Tooltip("Paste offset, rotation, scale, color, blend mode, and interpolation");

	if(im::Combo("Interpolation", &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList))) {
		markModified();
	}

	im::SetNextItemWidth(width);
	im::DragInt("X", &layer.offset_x);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	im::SameLine();
	im::SetNextItemWidth(width);
	im::DragInt("Y", &layer.offset_y);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}

	int mode = layer.blend_mode-1;
	if(mode < 1)
		mode = 0;
	if (im::Combo("Blend Mode", &mode, "Normal\0Additive\0Subtractive\0"))
	{
		layer.blend_mode=mode+1;
		markModified();
	}
	if(im::ColorEdit4("Color", layer.rgba)) {
		markModified();
	}

	im::DragFloat3("Rot XYZ", layer.rotation, 0.005);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	im::DragFloat2("Scale", layer.scale, 0.1);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	if(im::Checkbox("Rotation keeps scale set by EF", &af->AFRT)) {
		markModified();
	}
	if(clipboard) {
		im::SameLine(0,20.f);
		if(im::Button("Copy AF")) {
			*clipboard = *af;
		}
		if(im::IsItemHovered()) Tooltip("Copy all animation data (sprite, timing, transforms, colors)");
		im::SameLine(0,20.f);
		if(im::Button("Paste AF")) {
			*af = *clipboard;
			markModified();
		}
		if(im::IsItemHovered()) Tooltip("Paste all animation data (sprite, timing, transforms, colors)");
	}

}

#endif /* FRAME_DISP_AF_H_GUARD */

