#ifndef FRAME_DISP_AS_H_GUARD
#define FRAME_DISP_AS_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"
#include "../i18n.h"

// ============================================================================
// Action State (AS) Display
// ============================================================================
// Displays and edits Frame_AS data (movement, physics, cancels, state)
//
// Frame_AS controls:
// - Movement vectors (speed, acceleration, max speed)
// - Physics flags (momentum, ground tech, guard points)
// - State properties (standing/airborne/crouching)
// - Cancel conditions (normals, specials)
// - Invincibility frames
// - Sinewave movement
// ============================================================================

inline void AsDisplay(Frame_AS *as, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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

	unsigned int flagIndex = -1;
	if (gSettings.compactUi) {
		if (ui::BeginInspectorTable("##axis_movement",6)) {
			im::TableSetupColumn("##label",ImGuiTableColumnFlags_WidthFixed,ui::Em(3.2f));
			for(int c=1;c<6;++c) im::TableSetupColumn("##value",ImGuiTableColumnFlags_WidthStretch);
			ui::InspectorCell(0,0);ui::RowLabel("Flags");
			ui::InspectorCell(0,1);im::SetCursorPosX(im::GetCursorPosX()+ui::LabelTextW("X")+im::GetStyle().ItemInnerSpacing.x);if(ui::AxisFlags(&as->movementFlags,0)) markModified();ui::InspectorItem("move.x.flags");
			ui::InspectorCell(0,2);ui::RowLabel("Accel");
			ui::InspectorCell(0,3);im::SetCursorPosX(im::GetCursorPosX()+ui::LabelTextW("Y")+im::GetStyle().ItemInnerSpacing.x);if(ui::AxisFlags(&as->movementFlags,1)) markModified();ui::InspectorItem("move.y.flags");
			ui::InspectorCell(0,4);ui::RowLabel("Accel");
			ui::InspectorCell(0,5);ui::RowLabel("Max X");
			ui::InspectorCell(1,0);ui::RowLabel("Speed");
			ui::InspectorCell(1,1);ui::AxisLabel(0);if(im::InputInt("##speedX",&as->speed[0],0,0)) markModified();ui::InspectorItem("move.x.speed");
			ui::InspectorCell(1,2);if(im::InputInt("##accelX",&as->accel[0],0,0)) markModified();ui::InspectorItem("move.x.accel");
			ui::InspectorCell(1,3);ui::AxisLabel(1);if(im::InputInt("##speedY",&as->speed[1],0,0)) markModified();ui::InspectorItem("move.y.speed");
			ui::InspectorCell(1,4);if(im::InputInt("##accelY",&as->accel[1],0,0)) markModified();ui::InspectorItem("move.y.accel");
			ui::InspectorCell(1,5);if(im::InputInt("##maxSpeedX",&as->maxSpeedX,0,0)) markModified();
			im::EndTable();
		}
	} else {
		if(BitField("Movement Flags", &as->movementFlags, &flagIndex, 8)) markModified();
		switch(flagIndex) {
			case 0: Tooltip("Set Y");break;case 4: Tooltip("Set X");break;
			case 1: Tooltip("Add Y");break;case 5: Tooltip("Add X");break;
		}
	}

	// Label-first form (ui_style.h LabelForm): one label column for every section of this display.
	const bool uniAs = frameData && frameData->usesUniFormat();
	const float lw = ui::LabelColW({LBL("Speed"), LBL("Max X speed"), LBL("Accel"), LBL("Number of hits"),
		LBL("Player can move"), LBL("State"), LBL("Invincibility"), LBL("Counterhit"), LBL("Cancel normal"),
		LBL("Cancel special"), LBL("Sinewave"), LBL("Phases"), uniAs ? LBL("ASCF") : ""});
	if(!gSettings.compactUi && ui::BeginLabelForm("##as_move", lw)) {
		if(im::InputInt2(ui::FormRow(LBL("Speed")), as->speed)) {
			markModified();
		}
		if(im::InputInt(ui::FormRow(LBL("Max X speed")), &as->maxSpeedX, 0, 0)) {
			markModified();
		}
		if(im::InputInt2(ui::FormRow(LBL("Accel")), as->accel)) {
			markModified();
		}
		ui::EndLabelForm();
	}

	ui::FormSection(TXT("Flags"));
	flagIndex = -1;
	if(BitField("Flagset 1", &as->statusFlags[0], &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Vector influences other animations (dash momentum)"); break;
		case 1: Tooltip("Force clean vector (kill dash momentum)"); break;
		case 2: Tooltip("Don't transition to walking"); break;
		case 4: Tooltip("Can ground tech"); break;
		case 5: Tooltip("Unknown"); break;
		case 8: Tooltip("Guard point high and mid"); break;
		case 9: Tooltip("Guard point air blockable"); break;
		case 12: Tooltip("Unknown"); break;
		case 31: Tooltip("Vector initialization only at the beginning (?)"); break;
	}

	im::Dummy(ImVec2(0,ui::Em(.2f)));
	flagIndex = -1;
	if(BitField("Flagset 2", &as->statusFlags[1], &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Can always EX cancel"); break;
		case 2: Tooltip("Can only jump cancel"); break;
		case 31: Tooltip("Can't block"); break;
	}

	ui::FormSection(TXT("State"));
	if (gSettings.compactUi) {
		if(ui::InlineInt("Hits",&as->hitsNumber,ui::Em(2.5f))) markModified();
		im::SameLine(0,ui::Em(1));ui::RowLabel("Can Move");im::SameLine();
		if(im::Checkbox("##canMove",&as->canMove)) markModified();
		if (ui::BeginInspectorTable("##state_options",5)) {
			const char* names[]={"Stance","Invincibility","Counterhit","Cancel normal","Cancel special"};
			for(const char* name:names) {im::TableNextColumn();im::TextUnformatted(TXT(name));}
			im::TableNextColumn();im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##stance",&as->stanceState,stateList,IM_ARRAYSIZE(stateList))) markModified();
			im::TableNextColumn();im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##invul",&as->invincibility,invulList,IM_ARRAYSIZE(invulList))) markModified();
			im::TableNextColumn();im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##counter",&as->counterType,counterList,IM_ARRAYSIZE(counterList))) markModified();
			im::TableNextColumn();im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##normal",&as->cancelNormal,cancelList,IM_ARRAYSIZE(cancelList))) markModified();
			im::TableNextColumn();im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##special",&as->cancelSpecial,cancelList,IM_ARRAYSIZE(cancelList))) markModified();
			im::EndTable();
		}
	} else {
	if(ui::BeginLabelForm("##as_state", lw)) {
		if(im::InputInt(ui::FormRow(LBL("Number of hits")), &as->hitsNumber, 0, 0)) {
			markModified();
		}
		if(im::Checkbox(ui::FormRow(LBL("Player can move")), &as->canMove)) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("State")), &as->stanceState, stateList, IM_ARRAYSIZE(stateList))) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("Invincibility")), &as->invincibility, invulList, IM_ARRAYSIZE(invulList))) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("Counterhit")), &as->counterType, counterList, IM_ARRAYSIZE(counterList))) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("Cancel normal")), &as->cancelNormal, cancelList, IM_ARRAYSIZE(cancelList))) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("Cancel special")), &as->cancelSpecial, cancelList, IM_ARRAYSIZE(cancelList))) {
			markModified();
		}
		ui::EndLabelForm();
	}
	}

	if(im::CollapsingHeader(LBL("Sine flags"))) {
		flagIndex = -1;
		if(BitField("Flags", &as->sineFlags, &flagIndex, 8)) {
			markModified();
		}
		switch (flagIndex)
		{
			case 0: Tooltip("Use Y"); break;
			case 4: Tooltip("Use X"); break;
		}
		if(ui::BeginLabelForm("##as_sine", lw)) {
			ui::FormRow(LBL("Sinewave"));
			im::SetNextItemWidth(im::GetContentRegionAvail().x - ui::LabelTextW("(?)") - im::GetStyle().ItemSpacing.x);
			if(im::InputInt4(ui::HiddenId(LBL("Sinewave")), as->sineParameters)) {
				markModified();
			}
			im::SameLine();
			im::TextDisabled("(?)");
			if(im::IsItemHovered())
				Tooltip("Sine parameters:\nX dist, Y dist\nX frequency, Y frequency");
			if(im::InputFloat2(ui::FormRow(LBL("Phases")), as->sinePhases)) {
				markModified();
			}
			ui::EndLabelForm();
		}
	}
	if(uniAs && ui::BeginLabelForm("##as_uni", lw)) {
		if(im::InputInt(ui::FormRow(LBL("ASCF")), &as->ascf, 0, 0)) {
			markModified();
		}
		if(im::IsItemHovered())
			Tooltip("ASCF (UNI2/MBTL, AS +28): one int; used in a few UNI2/MBTL patterns.");
		ui::EndLabelForm();
	}
}

#endif /* FRAME_DISP_AS_H_GUARD */

