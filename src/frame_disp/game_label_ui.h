#ifndef GAME_LABEL_UI_H_GUARD
#define GAME_LABEL_UI_H_GUARD

// EF/IF parameter UI for games with their own label tables (UNI2/UNIB/UNIST/MBTL,
// src/game_labels.h). MBAACC keeps the smart UI in effects/* and frame_disp_if.h.

#include "frame_disp_common.h"
#include "../game_labels.h"
#include "../i18n.h"
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

// Labelled parameters as single fields; unlabelled ones only when non-zero.
inline void DrawGameLabelParams(const GameLabelRow* row, int* p, int count, const std::function<void()>& markModified)
{
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)
	for (int k = 0; k < count; ++k) {
		const char* label = (row && k < 12) ? row->params[k] : nullptr;
		if (!label && p[k] == 0) continue;
		char id[160];
		if (label) snprintf(id, sizeof(id), "p%d: %s##gp%d", k + 1, TXT(label), k);
		else       snprintf(id, sizeof(id), "p%d##gp%d", k + 1, k);
		im::SetNextItemWidth(width);
		if (im::InputInt(id, &p[k], 0, 0)) markModified();
	}
	if (row && row->status) {
		im::TextDisabled(TXT("Source: %s"), row->status);
	}
}

// Effect body for a game with its own labels.
inline void DrawGameEffectUI(Frame_EF& ef, FrameData* frameData, Ha6Game game, const std::function<void()>& markModified)
{
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::EF, ef.type);
	im::SetNextItemWidth(width);
	if (im::InputInt(LBL("Number"), &ef.number, 0, 0)) markModified();
	const bool spawnsPattern = ef.type == 1 || ef.type == 101 || ef.type == 11 || ef.type == 111;
	if (ef.type == 6) {
		im::SameLine(); im::TextDisabled("%s", GameLabelUiTitle(game, GameLabelKind::EF6, ef.number).c_str());
		const GameLabelRow* sub = FindGameLabel(game, GameLabelKind::EF6, ef.number);
		DrawGameLabelParams(sub, ef.parameters, 12, markModified);
		return;
	}
	if (spawnsPattern && frameData && ef.number >= 0 && ef.number < frameData->get_sequence_count()) {
		im::SameLine(); im::TextDisabled("[%s]", frameData->GetDecoratedName(ef.number).c_str());
	}
	if (!row) im::TextDisabled(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	DrawGameLabelParams(row, ef.parameters, 12, markModified);
}

// Condition body for a game with its own labels.
inline void DrawGameConditionUI(Frame_IF& cond, Ha6Game game, const std::function<void()>& markModified)
{
	const GameLabelRow* row = FindGameLabel(game, GameLabelKind::IF, cond.type);
	if (!row) im::TextDisabled(TXT("No handler for this type in %s: raw parameters."), Ha6GameName(game));
	DrawGameLabelParams(row, cond.parameters, kMaxIfParams, markModified);
}

#endif /* GAME_LABEL_UI_H_GUARD */
