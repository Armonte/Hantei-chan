#ifndef GAME_LABEL_UI_H_GUARD
#define GAME_LABEL_UI_H_GUARD

// EF/IF parameter UI for games with their own label tables (UNI2/UNIB/UNIST/MBTL,
// src/game_labels.h). MBAACC keeps the smart UI in effects/* and frame_disp_if.h.

#include "frame_disp_common.h"
#include "../game_labels.h"
#include "../i18n.h"
#include "effects/effect_form.h"
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

// The parameter rows a label row shows: labelled ones, and unlabelled ones only when non-zero. Fills `ids` with the
// row labels ("p1: name##gpN") and `shown` with the parameter indexes; returns the count.
inline int GameLabelParamRows(const GameLabelRow* row, const int* p, int count, char (*ids)[384], int* shown)
{
	int n = 0;
	for (int k = 0; k < count && k < 16; ++k) {
		const char* label = (row && k < 12) ? row->params[k] : nullptr;
		if (!label && p[k] == 0) continue;
		if (label) snprintf(ids[n], sizeof(ids[n]), "p%d: %s##gp%d", k + 1, TXT(label), k);
		else       snprintf(ids[n], sizeof(ids[n]), "p%d##gp%d", k + 1, k);
		shown[n++] = k;
	}
	return n;
}

// Label column a condition's parameter rows need (capped like ui::LabelColW), so frame_disp_if.h can give the Type row
// and the parameter rows of one condition the same column.
inline float GameConditionLabelColW(const Frame_IF& cond, Ha6Game game)
{
	char ids[16][384];
	int shown[16];
	const int n = GameLabelParamRows(FindGameLabel(game, GameLabelKind::IF, cond.type), cond.parameters, kMaxIfParams, ids, shown);
	float lw = 0.f;
	for (int i = 0; i < n; ++i) lw = std::max(lw, ui::LabelColW({ids[i]}));
	return lw;
}

// Labelled parameters as label-first rows ("p1: name | field"); unlabelled ones only when non-zero.
// labelW > 0: a ui::BeginLabelForm table with that label column (conditions, sharing the Type row's column);
// labelW == 0: effects/effect_form.h EfRow rows, the label column every effect row of the pane shares.
inline void DrawGameLabelParams(const GameLabelRow* row, int* p, int count, const std::function<void()>& markModified, float labelW = 0.f)
{
	char ids[16][384];
	int shown[16];
	const int n = GameLabelParamRows(row, p, count, ids, shown);
	if (labelW > 0.f) {
		if (n && ui::BeginLabelForm("##gameparams", labelW)) {
			for (int i = 0; i < n; ++i)
				if (im::InputInt(ui::FormRow(ids[i]), &p[shown[i]], 0, 0)) markModified();
			ui::EndLabelForm();
		}
	} else {
		for (int i = 0; i < n; ++i)
			if (im::InputInt(EfRow(ids[i]), &p[shown[i]], 0, 0)) markModified();
	}
	if (row && row->status) {
		im::TextDisabled(TXT("Source: %s"), row->status);
	}
}

// Effect body for a game with its own labels: Number and parameters on the effect form's label column (the Type row's).
inline void DrawGameEffectUI(Frame_EF& ef, FrameData* frameData, Ha6Game game, const std::function<void()>& markModified)
{
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::EF, ef.type);
	const bool spawnsPattern = ef.type == 1 || ef.type == 101 || ef.type == 11 || ef.type == 111;
	// the Number row keeps its sub-type / spawned pattern name after the field
	std::string trail;
	if (ef.type == 6) trail = GameLabelUiTitle(game, GameLabelKind::EF6, ef.number);
	else if (spawnsPattern && frameData && ef.number >= 0 && ef.number < frameData->get_sequence_count())
		trail = "[" + frameData->GetDecoratedName(ef.number) + "]";
	const char* rid = EfRow(LBL("Number"));
	if (!trail.empty()) im::SetNextItemWidth(ui::FieldW());
	if (im::InputInt(rid, &ef.number, 0, 0)) markModified();
	if (!trail.empty()) {   // after the field when it fits, else under it in the value column
		im::SameLine();
		if (im::GetContentRegionAvail().x >= im::CalcTextSize(trail.c_str()).x) im::TextDisabled("%s", trail.c_str());
		else { im::NewLine(); EfNote(trail.c_str()); }
	}
	if (ef.type == 6) {
		const GameLabelRow* sub = FindGameLabel(game, GameLabelKind::EF6, ef.number);
		DrawGameLabelParams(sub, ef.parameters, 12, markModified);
		return;
	}
	if (!row) i18n::TextDisabledWrapped(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	DrawGameLabelParams(row, ef.parameters, 12, markModified);
}

// Condition body for a game with its own labels. labelW: the condition's label column (GameConditionLabelColW folded
// into the Type row's), so Type and the parameters line up.
inline void DrawGameConditionUI(Frame_IF& cond, Ha6Game game, const std::function<void()>& markModified, float labelW = 0.f)
{
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::IF, cond.type);
	if (!row) i18n::TextDisabledWrapped(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	if (labelW <= 0.f) labelW = GameConditionLabelColW(cond, game);
	DrawGameLabelParams(row, cond.parameters, kMaxIfParams, markModified, std::max(labelW, ui::Em(3.f)));
}

#endif /* GAME_LABEL_UI_H_GUARD */
