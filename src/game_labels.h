#ifndef GAME_LABELS_H_GUARD
#define GAME_LABELS_H_GUARD

// Per-game effect (EF), EF6 sub-number and condition (IF) labels.
// MBAACC keeps its own tables and smart UI (framedata_labels.h, frame_disp/effects/*).
// UNI2/UNIB/UNIST/MBTL files use the French-Bread engine tables in game_labels_fb.inc,
// taken from the Mizuumi FPANModding pages and the uni2.exe/MBTL.exe handlers
// (docs/uni_re/labels/LABELS.md). UNIB and UNIST use the UNI2 rows (not checked against
// their executables).

#include <string>
#include "framedata.h"

struct GameLabelRow {
	int id;
	const char* name;
	const char* status;          // "wiki+IDA", "IDA" or "IDA-corrected"
	const char* params[12];      // p1..p12 (p1 = parameters[0]); nullptr = no known meaning
};

enum class GameLabelKind { EF, EF6, IF };

// True when the game has its own tables (every non-MBAACC HA6 dialect).
inline bool GameHasOwnLabels(Ha6Game g);
// The row for `id`, or nullptr when the game has no handler for it (or uses MBAACC tables).
inline const GameLabelRow* FindGameLabel(Ha6Game g, GameLabelKind k, int id);
// "<id>: <name>", or "EF <id> (unknown in <game>)" / "EF6 <id> ..." / "IF <id> ...".
inline std::string GameLabelTitle(Ha6Game g, GameLabelKind k, int id);
// Rows in table order, ascending (for pickers and tests).
inline int GameLabelCount(Ha6Game g, GameLabelKind k);
inline const GameLabelRow* GameLabelAt(Ha6Game g, GameLabelKind k, int index);

#include <cstdio>
#include "game_labels_fb.inc"


namespace game_labels_detail {
template<size_t N> inline int Count(const GameLabelRow (&)[N]) { return (int)N; }

inline bool Table(Ha6Game g, GameLabelKind k, const GameLabelRow** rows, int* n)
{
	if (!GameHasOwnLabels(g)) return false;
	switch (k) {
	case GameLabelKind::EF:  *rows = kFbEf;  *n = Count(kFbEf);  break;
	case GameLabelKind::EF6: *rows = kFbEf6; *n = Count(kFbEf6); break;
	default:                 *rows = kFbIf;  *n = Count(kFbIf);  break;
	}
	return true;
}

inline const char* KindPrefix(GameLabelKind k)
{
	return k == GameLabelKind::EF ? "EF" : k == GameLabelKind::EF6 ? "EF6" : "IF";
}
}

inline bool GameHasOwnLabels(Ha6Game g)
{
	return g == Ha6Game::UNI || g == Ha6Game::MBTL || g == Ha6Game::UNIB || g == Ha6Game::UNIST;
}

inline const GameLabelRow* FindGameLabel(Ha6Game g, GameLabelKind k, int id)
{
	const GameLabelRow* rows; int n;
	if (!game_labels_detail::Table(g, k, &rows, &n)) return nullptr;
	for (int i = 0; i < n; ++i)
		if (rows[i].id == id) return &rows[i];
	return nullptr;
}

inline std::string GameLabelTitle(Ha6Game g, GameLabelKind k, int id)
{
	char buf[160];
	if (const GameLabelRow* r = FindGameLabel(g, k, id))
		std::snprintf(buf, sizeof(buf), "%d: %s", id, r->name);
	else
		std::snprintf(buf, sizeof(buf), "%s %d (unknown in %s)", game_labels_detail::KindPrefix(k), id, Ha6GameName(g));
	return buf;
}

inline int GameLabelCount(Ha6Game g, GameLabelKind k)
{
	const GameLabelRow* rows; int n;
	return game_labels_detail::Table(g, k, &rows, &n) ? n : 0;
}

inline const GameLabelRow* GameLabelAt(Ha6Game g, GameLabelKind k, int index)
{
	const GameLabelRow* rows; int n;
	if (!game_labels_detail::Table(g, k, &rows, &n) || index < 0 || index >= n) return nullptr;
	return &rows[index];
}

#endif /* GAME_LABELS_H_GUARD */
