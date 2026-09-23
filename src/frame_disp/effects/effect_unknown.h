#ifndef EFFECT_UNKNOWN_H_GUARD
#define EFFECT_UNKNOWN_H_GUARD

// ============================================================================
// Effect Types 7, 30, 257: System / special
// ============================================================================
// Type 7:   System effect (round-call / KO banners, particles). MBAA Effect7_SpawnSystemEffect 0x455540
// Type 30:  Object behaviour params. MBAA Effect30_SetObjectBehaviorParams 0x45EF20
// Type 257: Not dispatched (lone data typo in Arc pattern 124)
//
// Types 1000 and 10002 used to live here; 1000 now uses the EF1 UI and 10002
// the EF2 UI (see frame_disp_ef.h). Source: docs/tag_research/MBAA_NAME_AUDIT.md 2.1.
// ============================================================================

static inline void DrawEffectUnknown(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	switch(effect.type) {
		case 7: // System effect
		{
			const char* const sysEffects[] = {
				"253: Round-call banner",
				"254: System fx 1002",
				"255: System fx 1000",
				"256: KO banner",
				"258: System fx 1004",
				"1000: Particles (1502)",
				"1001: Particles (1504)",
				"1002: Particles (1505)",
			};
			if(ShowComboWithManual("System effect", &no, sysEffects, IM_ARRAYSIZE(sysEffects), width*2, width)) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("Spawned into the system effect pool (SysEffect_SpawnById).\nConfidence: medium.");

			im::SetNextItemWidth(width);
			if(im::InputInt("Position X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Position Y", &p[1], 0, 0)) markModified();

			if(no == 256) {
				im::SetNextItemWidth(width);
				if(im::InputInt("KO variant (p3 % 10)", &p[2], 0, 0)) markModified();
			}
			break;
		}

		case 30: // Object behaviour params
		{
			const char* const ef30Modes[] = {
				"0: Set behaviour words",
				"1: Set tracking param",
			};
			if(ShowComboWithManual("Mode", &no, ef30Modes, IM_ARRAYSIZE(ef30Modes), width*2, width)) {
				markModified();
			}
			im::TextDisabled("Same block EF6 No 200 sets (object tracking behaviour)");
			if(no == 0) {
				im::SetNextItemWidth(width);
				if(im::InputInt("Value A (+0x30)", &p[0], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Value B (+0x32)", &p[1], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Value C (+0x2E)", &p[2], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Only non-zero values are written.\nExact meaning of each word unknown (?)");
			} else if(no == 1) {
				im::SetNextItemWidth(width);
				if(im::InputInt("Param index", &p[0], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Value", &p[1], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("word[+0x36 + 2*index] = value\n(the EF6 No 200 p2..p6 words)");
			} else {
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) markModified();
			}
			break;
		}

		case 257: // Not dispatched
		{
			im::Text("Not dispatched by MBAA (Arc pattern 124 data typo) - no effect");
			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}
	}
}

#endif /* EFFECT_UNKNOWN_H_GUARD */
