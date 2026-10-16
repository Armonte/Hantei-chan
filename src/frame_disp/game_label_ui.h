#ifndef GAME_LABEL_UI_H_GUARD
#define GAME_LABEL_UI_H_GUARD

// EF/IF parameter UI for games with their own label tables (UNI2/UNIB/UNIST/MBTL,
// src/game_labels.h). MBAACC keeps the smart UI in effects/* and frame_disp_if.h.

#include "frame_disp_common.h"
#include "../game_labels.h"
#include "../i18n.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>

// "<id>: <name>" or "<EF|EF6|IF> <id> (unknown in <game>)", translated.
inline std::string GameLabelUiTitle(Ha6Game g, GameLabelKind k, int id)
{
	char buf[256];
	if (const GameLabelRow* r = FindGameLabel(g, k, id))
		snprintf(buf, sizeof(buf), "%d: %s", id, TXT(r->name));
	else
		snprintf(buf, sizeof(buf), TXT("%s %d (unknown in %s)"),
			k == GameLabelKind::EF ? "EF" : k == GameLabelKind::EF6 ? "EF6" : "IF", id, Ha6GameName(g));
	return buf;
}

// Labelled parameters as label-first rows ("p1: name | field", ui::BeginLabelForm); unlabelled ones only when non-zero.
inline void DrawGameLabelParams(const GameLabelRow* row, int* p, int count, const std::function<void()>& markModified)
{
	char ids[16][160];
	int shown[16], n = 0;
	float lw = 0.f;
	for (int k = 0; k < count && k < 16; ++k) {
		const char* label = (row && k < 12) ? row->params[k] : nullptr;
		if (!label && p[k] == 0) continue;
		if (label) snprintf(ids[n], sizeof(ids[n]), "p%d: %s##gp%d", k + 1, TXT(label), k);
		else       snprintf(ids[n], sizeof(ids[n]), "p%d##gp%d", k + 1, k);
		lw = std::max(lw, ui::LabelColW({ids[n]}));
		shown[n++] = k;
	}
	if (n && ui::BeginLabelForm("##gameparams", lw)) {
		for (int i = 0; i < n; ++i)
			if (im::InputInt(ui::FormRow(ids[i]), &p[shown[i]], 0, 0)) markModified();
		ui::EndLabelForm();
	}
	if (row && row->status) {
		im::TextDisabled(TXT("Source: %s"), row->status);
	}
}

// Effect body for a game with its own labels.
inline void DrawGameEffectUI(Frame_EF& ef, FrameData* frameData, Ha6Game game, const std::function<void()>& markModified)
{
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::EF, ef.type);
	const bool spawnsPattern = ef.type == 1 || ef.type == 101 || ef.type == 11 || ef.type == 111;
	// the Number row keeps its sub-type / spawned pattern name after the field
	std::string trail;
	if (ef.type == 6) trail = GameLabelUiTitle(game, GameLabelKind::EF6, ef.number);
	else if (spawnsPattern && frameData && ef.number >= 0 && ef.number < frameData->get_sequence_count())
		trail = "[" + frameData->GetDecoratedName(ef.number) + "]";
	if (ui::BeginLabelForm("##gamenumber", ui::LabelColW({LBL("Number")}))) {
		const char* rid = ui::FormRow(LBL("Number"));
		if (!trail.empty()) im::SetNextItemWidth(ui::FieldW());
		if (im::InputInt(rid, &ef.number, 0, 0)) markModified();
		if (!trail.empty()) {
			im::SameLine();
			im::PushTextWrapPos(0.f);
			im::TextDisabled("%s", trail.c_str());
			im::PopTextWrapPos();
		}
		ui::EndLabelForm();
	}
	if (ef.type == 6) {
		const GameLabelRow* sub = FindGameLabel(game, GameLabelKind::EF6, ef.number);
		DrawGameLabelParams(sub, ef.parameters, 12, markModified);
		return;
	}
	if (!row) i18n::TextDisabledWrapped(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	DrawGameLabelParams(row, ef.parameters, 12, markModified);
}

// Condition body for a game with its own labels.
inline void DrawGameConditionUI(Frame_IF& cond, Ha6Game game, const std::function<void()>& markModified)
{
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::IF, cond.type);
	if (!row) i18n::TextDisabledWrapped(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	DrawGameLabelParams(row, cond.parameters, kMaxIfParams, markModified);
}

#endif /* GAME_LABEL_UI_H_GUARD */
