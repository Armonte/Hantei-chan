#ifndef FRAME_DISP_AT_H_GUARD
#define FRAME_DISP_AT_H_GUARD

#include "frame_disp_common.h"
#include "../uni_vector_table.h"
#include "../cg.h"
#include "../i18n.h"
#include "../extension_profile.h"
#include "../mbaacc_typed_hitgrab.h"

// ============================================================================
// Attack (AT) Display
// ============================================================================
// Displays and edits Frame_AT data (hitboxes, damage, vectors, properties)
//
// Frame_AT controls:
// - Guard/Hit flags (blockability, hit conditions)
// - Damage values (HP, meter, guard damage)
// - Hitstop and untech time
// - Hit/guard vectors (knockback, blockstun)
// - Hit effects and sounds
// - Correction (proration/scaling)
// ============================================================================

inline void AtDisplay(Frame_AT *at, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr, std::function<void()> toolbar = nullptr)
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


	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)
	//Some attack flags mean different things in UNI/MBTL/UNI2 than in MBAACC (issue #74).
	const bool uniFormat = frameData && frameData->usesUniFormat();
	unsigned int flagIndex = -1;
	if (gSettings.compactUi && toolbar) im::BeginGroup();

	if(BitField("Guard Flags", &at->guard_flags, &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Stand blockable"); break;
		case 1: Tooltip("Air blockable"); break;
		case 2: Tooltip("Crouch blockable"); break;
		case 8: Tooltip("Miss if enemy is standing"); break;
		case 9: Tooltip("Miss if enemy is airborne"); break;
		case 10: Tooltip("Miss if enemy is crouching"); break;
		case 11: Tooltip("Miss if enemy is in hitstun (includes blockstun)"); break;
		case 12: Tooltip("Miss if enemy is in blockstun"); break;
		case 13: Tooltip("Miss if OTG"); break;
		case 14: Tooltip("Hit only in hitstun"); break;
		case 15: Tooltip("Can't hit playable character"); break;
	}

	im::Dummy(ImVec2(0,ui::Em(.2f)));
	flagIndex = -1;
	if(BitField("Hit Flags", &at->otherFlags, &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Chip health instead of red health"); break;
		case 1: Tooltip("Can't KO"); break;
		case 2: Tooltip("Make enemy unhittable"); break;
		case 3: Tooltip("Can't be clashed with"); break;
		case 4: Tooltip(uniFormat ?
			"Counter hit vector swap:\nif the attack's vector has a counter hit\nconditional in the vector table, it is used on CH"
			: "Auto super jump cancel"); break;
		case 5: Tooltip("Don't increase combo counter"); break;
		case 6: Tooltip("Shake the screen on hit"); break;
		case 7: Tooltip("Not air techable"); break;
		case 8: Tooltip("Not ground techable (HKD)"); break;
		case 9: Tooltip("Friendly fire"); break;
		case 10: Tooltip("No self hitstop"); break;

		case 12: Tooltip("Lock burst"); break;
		case 13: Tooltip("Can't be shielded"); break;
		case 14: Tooltip("Can't critical"); break;

		case 16: Tooltip("Use custom blockstop"); break;
		case 17: Tooltip("OTG Relaunch"); break;
		case 18: Tooltip("Can't counterhit"); break;
		case 19: Tooltip("Unknown"); break;
		case 20: Tooltip("Use Type 2 Circuit Break"); break;
		case 21: Tooltip("Unknown"); break;
		case 22: Tooltip("Remove 1f of untech"); break;

		case 25: if (uniFormat) Tooltip("Attacker's own hitstop from the ATSA preset\n(Hit_ResolveHitstop, UNI2/MBTL)"); break;
		//Unused or don't exist in melty.
		//case 29: Tooltip("Block enemy blast during Stun?"); break;
	}

	if (gSettings.compactUi && toolbar) {
		im::EndGroup();im::SameLine(0,ui::Em(1));im::BeginGroup();toolbar();im::EndGroup();
	}
	// Label-first form (ui_style.h LabelForm): every section shares one label column sized to the longest label
	// (capped; a longer one wraps and shows in full on hover), and the values fill the rest of the pane.
	const float lw = ui::LabelColW({LBL("Hitstop"), LBL("Cust HS"), LBL("Cust BS"), LBL("Untech time"),
		LBL("Circuit break time"), LBL("Extra gravity"), LBL("Hitgrab (ATNG)"), LBL("Hitgrab"), LBL("Correction %"),
		LBL("Type##Correction"), LBL("VS damage"), LBL("Damage"), LBL("Guard damage"), LBL("Meter gain"),
		LBL("Guard Vector:"), LBL("Hit Vector:"), LBL("Stand"), LBL("Air"), LBL("Crouch"), LBL("Flags"),
		LBL("Hit effect"), LBL("Sound effect"), LBL("Added effect"),
		uniFormat ? LBL("Proration % (ATHH)") : "", uniFormat ? LBL("Min damage (ATAM)") : "",
		uniFormat ? LBL("Starter corr. (ATSH)") : "", uniFormat ? LBL("Self stop (ATSA)") : "",
		uniFormat ? LBL("Hitstun decay (ATC0)") : "", uniFormat ? LBL("Power % (ATVD)") : "",
		uniFormat ? LBL("Hitstop (ATSn form)") : ""});

	auto grabProfile=[&](bool inlineField) {
		if(!ExtendedProfileEnabled() || !frameData || frameData->game()!=Ha6Game::MBAACC || at->hitgrab==0) return;
		int profile=mbaacc::extended::typedHitgrabProfile((unsigned)at->hitgrab);
		if(inlineField) {im::SameLine();ui::RowLabel("P");im::SameLine(0,3);im::SetNextItemWidth(ui::Em(2));}
		const char* id=inlineField?"##grab_profile":ui::FormRow(LBL("Tech profile (0 = ordinary)"));
		if(im::InputInt(id,&profile,0,0) && profile>=0 && profile<=255) {
			at->hitgrab=(int)mbaacc::extended::makeTypedHitgrab((unsigned)profile);markModified();
		}
		im::SameLine(0,3);im::TextDisabled("(?)");
		if(im::IsItemHovered()) im::SetTooltip("Extended Melty/BOF hitgrab tech profile (0–255).\n0: ordinary hitgrab. Nonzero: typed hitgrab profile.\nRequires the matching executable extension; vanilla MBAACC does not support typed profiles.\nUnknown ATNG values are preserved until you edit this field.");
		if(at->hitgrab!=1 && !mbaacc::extended::isTypedHitgrab((unsigned)at->hitgrab) && !inlineField)
			im::TextDisabled("Unknown ATNG 0x%08X preserved; editing profile replaces it.",(unsigned)at->hitgrab);
	};
	// --- Hitstop / untech -------------------------------------------------------------------------------------------
	ui::FormSection(TXT("Hitstop"));
	if(gSettings.compactUi && !uniFormat) {
		if(im::BeginTable("##at_hitstop_compact",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoSavedSettings)) {
			im::TableSetupColumn("##preset",ImGuiTableColumnFlags_WidthStretch,1.1f);
			im::TableSetupColumn("##custom",ImGuiTableColumnFlags_WidthStretch,1.4f);
			im::TableSetupColumn("##grab",ImGuiTableColumnFlags_WidthStretch,1.6f);
			im::TableNextColumn();ui::RowLabel("Hitstop");im::SameLine();im::SetNextItemWidth(ui::Em(6));
			if(i18n::Combo("##hitstop",&at->hitStop,hitStopList,IM_ARRAYSIZE(hitStopList))) markModified();
			im::TableNextColumn();if(ui::InlineInt("Cust HS",&at->hitStopTime,ui::Em(2.6f))) markModified();
			im::SameLine();if(ui::InlineInt("Cust BS",&at->blockStopTime,ui::Em(2.6f))) markModified();
			im::TableNextColumn();bool hg=at->hitgrab!=0;ui::RowLabel("Hitgrab");im::SameLine();
			if(im::Checkbox("##hitgrab",&hg)) {at->hitgrab=hg?1:0;markModified();}
			grabProfile(true);
			im::TableNextColumn();if(ui::InlineInt("Untech time",&at->untechTime,ui::Em(3))) markModified();
			im::TableNextColumn();if(ui::InlineInt("Circuit break time",&at->breakTime,ui::Em(3))) markModified();
			im::TableNextColumn();if(ui::InlineFloat("Extra gravity",&at->extraGravity,ui::Em(3.5f))) markModified();
			im::EndTable();
		}
	} else {
	if(ui::BeginLabelForm("##at_hitstop", lw)) {
		if(i18n::Combo(ui::FormRow(LBL("Hitstop")), &at->hitStop, hitStopList, IM_ARRAYSIZE(hitStopList))) {
			markModified();
		}
		if(im::InputInt(ui::FormRow(LBL("Cust HS")), &at->hitStopTime, 0,0)) {
			markModified();
		}
		if(im::InputInt(ui::FormRow(LBL("Cust BS")), &at->blockStopTime, 0,0)) {
			markModified();
		}
		if(im::InputInt(ui::FormRow(LBL("Untech time")), &at->untechTime, 0,0)) {
			markModified();
		}
		// D14: ATBT (circuit break time) and ATUH (extra gravity) are MBAACC
		// tags. UNI2/MBTL never read ATBT (no case in Han6_LoadFrameAT, uni2.exe
		// 0x4C6CB0 / MBTL 0x4A8CE0) and read ATUH into a local that is dropped,
		// so the UNI writer doesn't emit them: the fields are hidden there.
		if(!uniFormat) {
			if(im::InputInt(ui::FormRow(LBL("Circuit break time")), &at->breakTime, 0,0)) {
				markModified();
			}
			if(im::InputFloat(ui::FormRow(LBL("Extra gravity")), &at->extraGravity, 0,0)) {
				markModified();
			}
		}
		if(uniFormat) {
			// UNI2/MBTL ATNG is a byte with values up to 65 (not a bool).
			if(im::InputInt(ui::FormRow(LBL("Hitgrab (ATNG)")), &at->hitgrab, 0, 0)) {
				markModified();
			}
		} else {
			bool hg = at->hitgrab != 0;
			if(im::Checkbox(ui::FormRow(LBL("Hitgrab")), &hg)) {
				at->hitgrab = hg ? 1 : 0;
				markModified();
			}

		}
		ui::EndLabelForm();
	}	}

	if(!gSettings.compactUi && ui::BeginLabelForm("##at_grab_profile",lw)) {grabProfile(false);ui::EndLabelForm();}

	if(uniFormat) {
		// The low bits as flags (sosfiro's labels); hover a box for its meaning.
		unsigned hgBits = (unsigned)at->hitgrab;
		unsigned hgIndex = -1;
		if(BitField("Hitgrab flags", &hgBits, &hgIndex, 8)) {
			at->hitgrab = (int)hgBits;
			markModified();
		}
		switch (hgIndex)
		{
			case 0: Tooltip("Hitgrab: enabled"); break;
			case 1: Tooltip("Hitgrab: target collision box"); break;
			case 2: Tooltip("Hitgrab: target origin"); break;
			case 3: Tooltip("Hitgrab: ignore the target's invulnerability timer"); break;
		}
	}

	// --- Damage -----------------------------------------------------------------------------------------------------
	// D14: UNI2/MBTL keep only the high words of ATVV (damage +60, meter
	// +64); the low words (MBAACC VS damage, guard damage) are never read and
	// the UNI writer saves ATAT/ATCA. Those two fields are hidden there.
	ui::FormSection(TXT("Damage"));
	if(gSettings.compactUi && !uniFormat) {
		if(im::BeginTable("##at_damage_compact",4,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoSavedSettings)) {
			const char* labels[]={"Damage","VS damage","Guard damage","Meter gain"};
			int* values[]={&at->damage,&at->red_damage,&at->guard_damage,&at->meter_gain};
			for(const char* label:labels) im::TableSetupColumn(label,ImGuiTableColumnFlags_WidthStretch,im::CalcTextSize(TXT(label)).x+ui::Em(4));
			for(int i=0;i<4;++i) {im::TableNextColumn();if(ui::InlineInt(labels[i],values[i],ui::Em(3.5f))) markModified();}
			im::EndTable();
		}
	} else if(ui::BeginLabelForm("##at_damage", lw)) {
		if(im::InputInt(ui::FormRow(LBL("Damage")), &at->damage, 0, 0)) {
			markModified();
		}
		if(!uniFormat) {
			if(im::InputInt(ui::FormRow(LBL("VS damage")), &at->red_damage, 0, 0)) {
				markModified();
			}
			if(im::InputInt(ui::FormRow(LBL("Guard damage")), &at->guard_damage, 0, 0)) {
				markModified();
			}
		}
		if(im::InputInt(ui::FormRow(LBL("Meter gain")), &at->meter_gain, 0, 0)) {
			markModified();
		}
		ui::EndLabelForm();
	}
	if(uniFormat) {
		i18n::TextDisabledWrapped("%s", TXT("VS damage, guard damage, circuit break time and extra gravity are MBAACC-only: this game does not read them."));
	}

	// --- Proration --------------------------------------------------------------------------------------------------
	ui::FormSection(TXT("Proration"));
	if(gSettings.compactUi && !uniFormat) {
		if(ui::BeginInspectorTable("##at_proration_compact",2)) {
			im::TableNextColumn();if(ui::InlineInt("Correction %",&at->correction,ui::Em(4))) markModified();
			im::TableNextColumn();ui::RowLabel("Type");im::SameLine();im::SetNextItemWidth(ui::Em(7));
			if(i18n::Combo("##correction",&at->correction_type,"Normal\0Multiplicative\0Subtractive\0")) markModified();
			im::EndTable();
		}
	} else if(ui::BeginLabelForm("##at_proration", lw)) {
		if(im::InputInt(ui::FormRow(LBL("Correction %")), &at->correction, 0, 0)) {
			markModified();
		}
		if(i18n::Combo(ui::FormRow(LBL("Type##Correction")), &at->correction_type, "Normal\0Multiplicative\0Subtractive\0")) {
			markModified();
		}
		ui::EndLabelForm();
	}

	// --- Vectors ----------------------------------------------------------------------------------------------------
	ui::FormSection(TXT("Vectors"));
	const univec::Names* uniVectors = uniFormat && frameData ? univec::ForFile(frameData->m_sourcePath) : nullptr;
	static bool guardVectorManual = false;
	static bool hitVectorManual = false;
	static const char* const vectorLabels[] = {"Stand", "Air", "Crouch"};
	// One block per vector (guard, hit): a "Manual" toggle row, Stand / Air / Crouch rows, then the three flag combos.
	auto vectorRows = [&](const char* title, const char* manualLabel, bool& manual, const char* manualId, int* vec, int* flags,
		int idBase, const char* flagId) {
		ui::FormRow(title);
		if(im::Checkbox(LBL(manualLabel), &manual)) {
		}
		if(im::IsItemHovered()) {
			Tooltip("Enable manual vector entry");
		}
		if(manual) {
			// Manual mode - show InputInt3
			ui::FormRow("");
			if(im::InputInt3(manualId, vec)) {
				markModified();
			}
		} else {
			// Dropdown mode - Stand, Air, Crouch with dropdowns
			for(int i = 0; i < 3; i++)
			{
				im::PushID(idBase+i);
				const char* rowId = ui::FormRow(LBL(vectorLabels[i]));
				if(uniFormat) {
					// UNI2/MBTL: ids index data/VectorTable.txt, not MBAACC's list.
					im::SetNextItemWidth(ui::FieldW());
					if(im::InputInt(rowId, &vec[i], 0, 0)) markModified();
					im::SameLine();
					const std::string* nm = nullptr;
					if(uniVectors) { auto it = uniVectors->find(vec[i]); if(it != uniVectors->end()) nm = &it->second; }
					if(nm) im::TextDisabled("%s", nm->c_str());
					else if(uniVectors) im::TextDisabled("%s", TXT("(not in VectorTable.txt)"));
					else im::TextDisabled("%s", TXT("(VectorTable.txt not found)"));
				} else if(ShowComboWithManual(rowId, &vec[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), im::GetContentRegionAvail().x, width)) {
					markModified();
				}
				im::PopID();
			}
		}
		// Vector flags: three combos sharing the row
		ui::FormRow(LBL("Flags"));
		const float flagW = (im::GetContentRegionAvail().x - im::GetStyle().ItemSpacing.x * 2.f) / 3.f;
		for(int i = 0; i < 3; i++)
		{
			im::SetNextItemWidth(flagW);
			if(i > 0)
				im::SameLine();
			im::PushID(i);
			if(i18n::Combo(flagId, &flags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags))) {
				markModified();
			}
			im::PopID();
		}
	};
	if (gSettings.compactUi && !uniFormat) {
		auto compactVector = [&](const char* title, bool& manual, int* vec, int* flags) {
			im::PushID(title);
			if(im::BeginTable("##vector",4,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)) {
				im::TableSetupColumn("##label",ImGuiTableColumnFlags_WidthFixed,ui::Em(7.5f));
				for(int i=0;i<3;++i) im::TableSetupColumn(vectorLabels[i],ImGuiTableColumnFlags_WidthStretch);
				im::TableNextColumn();
				for(int i=0;i<3;++i) {im::TableNextColumn();im::TextUnformatted(TXT(vectorLabels[i]));}
				im::TableNextColumn();ui::RowLabel(title);im::SameLine();im::Checkbox("##manual",&manual);
				if(im::IsItemHovered()) Tooltip("Enable manual vector entry");
				for(int i=0;i<3;++i) {
					im::TableNextColumn();im::PushID(i);im::SetNextItemWidth(-FLT_MIN);
					if(manual) {if(im::InputInt("##value",&vec[i],0,0)) markModified();}
					else if(ShowComboWithManual("##value",&vec[i],hitVectorList,IM_ARRAYSIZE(hitVectorList),im::GetContentRegionAvail().x,width)) markModified();
					im::PopID();
				}
				im::TableNextColumn();ui::RowLabel("Flags");
				for(int i=0;i<3;++i) {im::TableNextColumn();im::PushID(i);im::SetNextItemWidth(-FLT_MIN);if(i18n::Combo("##flags",&flags[i],vectorFlags,IM_ARRAYSIZE(vectorFlags))) markModified();im::PopID();}
				im::EndTable();
			}
			im::PopID();
		};
		compactVector("Guard Vector:",guardVectorManual,at->guardVector,at->gVFlags);
		compactVector("Hit Vector:",hitVectorManual,at->hitVector,at->hVFlags);
	} else if(ui::BeginLabelForm("##at_vectors", lw)) {
		// Guard Vector
		vectorRows(LBL("Guard Vector:"), "Manual##gv", guardVectorManual, "##guardVec", at->guardVector, at->gVFlags, 100, "##GFLAG");
		// Hit Vector
		vectorRows(LBL("Hit Vector:"), "Manual##hv", hitVectorManual, "##hitVec", at->hitVector, at->hVFlags, 200, "##HFLAG");
		ui::EndLabelForm();
	}

	// --- Effects ----------------------------------------------------------------------------------------------------
	ui::FormSection(TXT("Effects"));
	// A list combo followed by its raw "ID" field (any ID, also ones the list does not name; sosfiro: raw added effect ID).
	auto comboWithId = [&](const char* rowId, int* v, const char* const* items, int count, const char* idLabel) {
		const char* idText = TXT("ID");
		const float idW = ui::FieldW() * 0.8f;
		const float comboW = im::GetContentRegionAvail().x - idW - ui::LabelTextW(idText) - im::GetStyle().ItemSpacing.x * 2.f;
		im::SetNextItemWidth(std::max(comboW, ui::FieldW()));
		if(i18n::Combo(rowId, v, items, count)) {
			markModified();
		}
		im::SameLine();
		im::TextUnformatted(idText);
		im::SameLine();
		im::SetNextItemWidth(-FLT_MIN);
		if(im::InputInt(ui::HiddenId(LBL(idLabel)), v, 0, 0)) {
			markModified();
		}
	};
	if(gSettings.compactUi && !uniFormat) {
		if(im::BeginTable("##at_effects_compact",3,ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoSavedSettings)) {
			im::TableSetupColumn("##label",ImGuiTableColumnFlags_WidthFixed,ui::Em(7.5f));
			im::TableSetupColumn("##effect",ImGuiTableColumnFlags_WidthStretch);
			im::TableSetupColumn("##sound",ImGuiTableColumnFlags_WidthFixed,ui::Em(10));
			im::TableNextColumn();ui::RowLabel("Hit effect");im::TableNextColumn();comboWithId("##hit",&at->hitEffect,hitEffectList,IM_ARRAYSIZE(hitEffectList),"ID##Hit effect");
			im::TableNextColumn();if(ui::InlineInt("Sound effect",&at->soundEffect,ui::Em(3))) markModified();
			im::TableNextColumn();ui::RowLabel("Added effect");im::TableNextColumn();comboWithId("##added",&at->addedEffect,addedEffectList,IM_ARRAYSIZE(addedEffectList),"ID##Added effect");
			im::EndTable();
		}
	} else if(ui::BeginLabelForm("##at_effects", lw)) {
		comboWithId(ui::FormRow(LBL("Hit effect")), &at->hitEffect, hitEffectList, IM_ARRAYSIZE(hitEffectList), "ID##Hit effect");
		if(im::InputInt(ui::FormRow(LBL("Sound effect")), &at->soundEffect, 0, 0)) {
			markModified();
		}
		comboWithId(ui::FormRow(LBL("Added effect")), &at->addedEffect, addedEffectList, IM_ARRAYSIZE(addedEffectList), "ID##Added effect");
		ui::EndLabelForm();
	}

	if(uniFormat) {
		// UNI2/MBTL AT fields. Offsets are the game's 76-byte AT record
		// (Han6_LoadFrameAT). In this dialect "Damage" is ATAT and "Meter
		// gain" is ATCA; "VS damage" and "Guard damage" exist only in
		// MBAACC's ATVV and are not saved.
		ui::FormSection(TXT("UNI/MBTL attack data"));
		auto intField = [&](const char* label, int* v, const char* tip) {
			if(im::InputInt(ui::FormRow(LBL(label)), v, 0, 0)) markModified();
			if(im::IsItemHovered()) Tooltip(tip);
		};
		if(ui::BeginLabelForm("##at_uni", lw)) {
			intField("Proration % (ATHH)", &at->damageProration, "ATHH (+48): damage proration for the rest of the combo.\n100 or absent = none.");
			intField("Min damage (ATAM)", &at->minDamage, "ATAM (+62): minimum damage.");
			intField("Starter corr. (ATSH)", &at->starterCorrection, "ATSH (+46): correction when this attack starts a combo.");
			intField("Self stop (ATSA)", &at->addHitStun, "ATSA (+43): hitstop preset for the attacker itself,\nused when hit flag 25 is set (Hit_ResolveHitstop).");
			if(im::InputInt3(ui::FormRow(LBL("Hitstun decay (ATC0)")), at->hitStunDecay)) markModified();
			if(im::IsItemHovered())
				Tooltip("ATC0 (+71, +73, +72): hitstun decay values.\n"
					"The first defaults to 100 and the third to 50 when 0.");
			intField("ATRF", &at->atrf, "ATRF (+47, byte). Observed 25/100/200 in UNI2.");
			intField("ATBC", &at->atbc, "ATBC (+52, word). Observed 30 in UNI2.");
			intField("Power % (ATVD)", &at->atvd, "ATVD (+68): attack power % override. When > 0 it replaces the\n"
				"attacker's damage rate (Hit_ComputeDamage_ATVDRateOverride, MBTL).");
			static const char* const kLegacy[] = {"none", "ATS1", "ATS2", "ATS3", "ATS4", "ATS5", "ATS6"};
			int legacy = (at->hitStopLegacy >= 0 && at->hitStopLegacy <= 6) ? at->hitStopLegacy : 0;
			if(i18n::Combo(ui::FormRow(LBL("Hitstop (ATSn form)")), &legacy, kLegacy, IM_ARRAYSIZE(kLegacy))) {
				at->hitStopLegacy = legacy;
				markModified();
			}
			if(im::IsItemHovered())
				Tooltip("ATS1..ATS6 set the same hitstop field as ATSP (\"Hitstop\" above).\n"
					"The game reads them first, so an ATSP in the same block wins.\n"
					"Kept as loaded (UNI2 chr006/016/017/021/026).");
			ui::EndLabelForm();
		}
	}
}

#endif /* FRAME_DISP_AT_H_GUARD */
