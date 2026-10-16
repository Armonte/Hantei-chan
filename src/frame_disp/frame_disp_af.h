#ifndef FRAME_DISP_AF_H_GUARD
#define FRAME_DISP_AF_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"
#include <cstring> // for memcpy
#include "../i18n.h"

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

inline void AfDisplay(Frame_AF *af, int &selectedLayer, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr, Frame_AF *clipboard = nullptr, bool templateLocked = false)
{
	// templateLocked (D17): a BaseData template supplies everything but each
	// layer's sprite, .pat flag and offset in game, so the rest is greyed out.
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

	const float width = ui::Px(50.f);   // follows the font size (ui_style.h)

	// ============================================================================
	// Multi-Layer UI (UNI AFGX support)
	// ============================================================================
	bool hasMultipleLayers = af->layers.size() > 1;

	// Layer management section
	const bool uni = frameData && frameData->usesUniFormat();
	const Ha6Game game = frameData ? frameData->game() : Ha6Game::MBAACC;
	im::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));  // Blue header
	if (hasMultipleLayers || uni) {
		im::Text(TXT("%s AFGX layers - Layer %d/%d"), Ha6GameName(game), selectedLayer + 1, (int)af->layers.size());
	} else {
		im::Text(TXT("Single Layer (MBAACC AFGP)"));
	}
	im::PopStyleColor();
	if (uni && im::IsItemHovered())
		Tooltip("UNI2 reads 5 AFGX layers, MBTL 3 (Han6_LoadFrameAF ignores higher ids).\n"
			"Layers are drawn 0 first (bottom) to last (top) within the same draw bucket.\n"
			"Unused layers have sprite -1; the game skips them.");

	im::SameLine(0, 20.f);
	// D16: the game reads 5 AFGX layers (UNI2) or 3 (MBTL/UNIB); more are ignored.
	const int layerCap = uni && frameData ? frameData->gameLayerCount() : 0;
	const bool atCap = layerCap > 0 && (int)af->layers.size() >= layerCap;
	im::BeginDisabled(atCap);
	const bool addLayer = im::SmallButton(LBL("Add Layer"));
	im::EndDisabled();
	if (atCap && im::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		im::SetTooltip(TXT("%s reads %d layers; more would be ignored."), Ha6GameName(game), layerCap);
	if (addLayer) {
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
		if (im::SmallButton(LBL("Delete Layer"))) {
			if (af->layers.size() > 1) {
				// D16: in UNI files the other layers keep their ids (and draw order).
				selectedLayer = DeleteLayerKeepIds(*af, selectedLayer, uni);
				markModified();
			}
		}
		if (im::IsItemHovered()) {
			if (uni) Tooltip("Clear the current layer (sprite -1). Only the last layer is removed, so the other layers keep their ids.");
			else Tooltip("Delete current layer (reverts to AFGP if only 1 layer remains)");
		}
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

		// AFPL: draw bucket of the layer (UNI2/MBTL).
		im::SameLine(0, 20.f);
		im::SetNextItemWidth(width * 3);
		static const char* const kBuckets[] = {
			"0: with object", "1: top (403)", "2: front (338)", "3: just in front", "4: just behind" };
		int& pl = af->layers[selectedLayer].priority;
		if (pl >= 0 && pl <= 4) {
			if (i18n::Combo(LBL("Draw bucket (AFPL)"), &pl, kBuckets, IM_ARRAYSIZE(kBuckets)))
				markModified();
		} else if (im::InputInt(LBL("Draw bucket (AFPL)"), &pl, 0, 0)) {
			markModified();
		}
		if (im::IsItemHovered())
			Tooltip("AFPL picks the render bucket the layer is drawn in (Han6Object_Draw):\n"
				"0 = the object's priority + 256 (default)\n"
				"1 = bucket 403, 2 = bucket 338 (drawn over the characters)\n"
				"3 = object + 258 (just in front of it), 4 = object + 254 (just behind)\n"
				"Buckets are drawn in ascending order.");
	}

	im::Separator();

	// Get reference to the layer we're editing
	Layer_Type& layer = af->layers[selectedLayer];

	// Sprite and .pat (per-layer properties)
	im::SetNextItemWidth(width*3);
	if(im::InputInt(LBL("Sprite"), &layer.spriteId)) {
		markModified();
	}
	im::SameLine(0, 20.f);
	if(im::Checkbox(LBL("Use .pat"), &layer.usePat)) {
		markModified();
	}

	if(im::SmallButton(LBL("Copy animation"))) {
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
	im::BeginDisabled(templateLocked);
	if(im::SmallButton(LBL("Paste animation"))) {
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

	if(i18n::Combo(LBL("Animation"), &af->aniType, animationList, IM_ARRAYSIZE(animationList))) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt(LBL("Go to"), &af->jump, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt(LBL("Landing frame"), &af->landJump, 0, 0)) {
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
		if(im::SmallButton(LBL("Set All"))) {
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
		if(im::SmallButton(LBL("Set Range"))) {
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

				im::Text(TXT("Set landing frame for range"));
				im::Separator();
				im::InputInt2(LBL("Frame range"), landingFrameRange);
				im::InputInt(LBL("Landing frame value"), &landingFrameToolValue);

				if(im::Button(LBL("Apply"), ImVec2(120, 0))) {
					for(int i = landingFrameRange[0]; i <= landingFrameRange[1] && i >= 0 && i < seq->frames.size(); i++) {
						seq->frames[i].AF.landJump = landingFrameToolValue;
					}
					frameData->mark_modified(patternIndex);
					markModified();
					im::CloseCurrentPopup();
				}
				im::SameLine();
				if(im::Button(LBL("Cancel"), ImVec2(120, 0))) {
					im::CloseCurrentPopup();
				}
			}
			im::EndPopup();
		}
	}

	im::SetNextItemWidth(width);
	if(im::InputInt(LBL("Z-Priority"), &af->priority, 0, 0)) {
		markModified();
	}
	im::SetNextItemWidth(width);
	if(im::InputInt(LBL("Loop N times"), &af->loopCount, 0, 0)) {
		markModified();
	}
	im::SameLine(0,20); im::SetNextItemWidth(width);
	if(im::InputInt(LBL("End of loop"), &af->loopEnd, 0, 0)) {
		markModified();
	}
	if(im::InputInt(LBL("Duration"), &af->duration, 1, 0)) {
		markModified();
	}
	im::EndDisabled(); // templateLocked (timing)

	im::Separator();

	// Copy/paste buttons for transform section
	static Layer_Type copiedTransformLayer = {};
	static bool copiedTransformAFRT = false;
	static int copiedTransformInterpolationType = 0;
	im::BeginDisabled(templateLocked);
	if(im::SmallButton(LBL("Copy transforms"))) {
		copiedTransformLayer.offset_x = layer.offset_x;
		copiedTransformLayer.offset_y = layer.offset_y;
		copiedTransformLayer.blend_mode = layer.blend_mode;
		memcpy(copiedTransformLayer.rgba, layer.rgba, sizeof(layer.rgba));
		memcpy(copiedTransformLayer.rotation, layer.rotation, sizeof(layer.rotation));
		memcpy(copiedTransformLayer.scale, layer.scale, sizeof(layer.scale));
		// D16: UNI2/MBTL keep AFRT per layer, MBAACC per frame.
		copiedTransformAFRT = uni ? layer.afrt : af->AFRT;
		copiedTransformInterpolationType = af->interpolationType;
	}
	if(im::IsItemHovered()) Tooltip("Copy offset, rotation, scale, color, blend mode, and interpolation");
	im::SameLine();
	if(im::SmallButton(LBL("Paste transforms"))) {
		layer.offset_x = copiedTransformLayer.offset_x;
		layer.offset_y = copiedTransformLayer.offset_y;
		layer.blend_mode = copiedTransformLayer.blend_mode;
		memcpy(layer.rgba, copiedTransformLayer.rgba, sizeof(layer.rgba));
		memcpy(layer.rotation, copiedTransformLayer.rotation, sizeof(layer.rotation));
		memcpy(layer.scale, copiedTransformLayer.scale, sizeof(layer.scale));
		if (uni) layer.afrt = copiedTransformAFRT; else af->AFRT = copiedTransformAFRT;
		af->interpolationType = copiedTransformInterpolationType;
		markModified();
	}
	if(im::IsItemHovered()) Tooltip("Paste offset, rotation, scale, color, blend mode, and interpolation");

	if(i18n::Combo(LBL("Interpolation"), &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList))) {
		markModified();
	}
	im::EndDisabled(); // templateLocked

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

	im::BeginDisabled(templateLocked);
	// MBAA blend numbering, confirmed in MBAA.exe: HA6_ParseAnimationFrameChunks stores AFAL int0 at
	// frame+0x1B; SpriteAnimation_Render 0x407462 maps 0 -> preset 0 (Sprite_EmitTransformedQuad: preset 1 with
	// alpha forced to 255), 2 -> preset 2, 3 -> preset 3, anything else -> preset 1. g_BlendPresetTable 0x54CC50:
	// 1 = ADD (src alpha, inv src alpha), 2 = ADD (src alpha, one), 3 = REVSUBTRACT (src alpha, one).
	// So stored 1 Normal, 2 Additive, 3 Subtractive are right (0 = Normal at full alpha); Render::SetBlendingMode
	// uses the same mapping.
	int mode = layer.blend_mode-1;
	if(mode < 1)
		mode = 0;
	if (i18n::Combo(LBL("Blend Mode"), &mode, "Normal\0Additive\0Subtractive\0"))
	{
		layer.blend_mode=mode+1;
		markModified();
	}
	if (uni && im::IsItemHovered())
		Tooltip("AFAL mode (UNI2/MBTL D3D blend table, D3D_SetBlendModeFromTable):\n"
			"1 = normal (src alpha, inv src alpha)\n"
			"2 = additive (src alpha, one)\n"
			"3 = subtractive (reverse subtract, src alpha, one)\n"
			"No AFAL = normal with the alpha forced to 255.");
	if(im::ColorEdit4(LBL("Color"), layer.rgba)) {
		markModified();
	}

	im::DragFloat3(LBL("Rot XYZ"), layer.rotation, 0.005);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	im::DragFloat2(LBL("Scale"), layer.scale, 0.1);
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	if (uni) {
		// UNI2/MBTL store AFRT per layer (Han6_LoadFrameAF, layer +32).
		if(im::Checkbox(LBL("Rotation keeps scale set by EF (AFRT, this layer)"), &layer.afrt)) {
			markModified();
		}
	} else if(im::Checkbox(LBL("Rotation keeps scale set by EF"), &af->AFRT)) {
		markModified();
	}
	if (uni) {
		im::Separator();
		im::TextDisabled(TXT("Script references (UNI/MBTL)"));
		im::SetNextItemWidth(width);
		if (im::InputInt(LBL("Frame ID (AFID)"), &af->frameId, 0, 0))
			markModified();
		if (im::IsItemHovered())
			Tooltip("AFID: frame id the Squirrel move scripts look up (frame +248, int16).");
		im::SameLine(0, 20.f);
		if (im::Checkbox(LBL("AFJH"), &af->afjh))
			markModified();
		if (im::IsItemHovered())
			Tooltip("AFJH: frame flag byte (+262); set on the first frame of most patterns.");
		// AFPA is read as two int16 (frame +250 and +252), not four bytes.
		int16_t pa[2];
		memcpy(pa, af->param, 4);
		int pai[2] = { pa[0], pa[1] };
		im::SetNextItemWidth(width * 2.5f);
		if (im::InputInt2(LBL("Params (AFPA)"), pai)) {
			pa[0] = (int16_t)pai[0]; pa[1] = (int16_t)pai[1];
			memcpy(af->param, pa, 4);
			markModified();
		}
		if (im::IsItemHovered())
			Tooltip("AFPA: two int16 values for the move scripts (frame +250 / +252).");
	}
	if(clipboard) {
		im::SameLine(0,20.f);
		if(im::Button(LBL("Copy AF"))) {
			*clipboard = *af;
		}
		if(im::IsItemHovered()) Tooltip("Copy all animation data (sprite, timing, transforms, colors)");
		im::SameLine(0,20.f);
		if(im::Button(LBL("Paste AF"))) {
			*af = *clipboard;
			markModified();
		}
		if(im::IsItemHovered()) Tooltip("Paste all animation data (sprite, timing, transforms, colors)");
	}
	im::EndDisabled(); // templateLocked

}

#endif /* FRAME_DISP_AF_H_GUARD */

