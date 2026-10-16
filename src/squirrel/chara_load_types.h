#ifndef SQUIRREL_CHARA_LOAD_TYPES_H_GUARD
#define SQUIRREL_CHARA_LOAD_TYPES_H_GUARD

// Plain data of the character-load check (chara_load.h, sq_api.h) without the Squirrel headers, for the UI side.

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace sqedit {

// Where a script function lives: its source name ("./data/chr001/chr001_mv_0.txt") and line range (1-based, inclusive).
struct SourceRange {
	std::string source;
	int first = 0, last = 0;
	std::string slot;   // "Init", "Init_After", ...
};

// What the engine knows while a character's scripts load (MBTL Chara_LoadBattleScripts 0x607650). Natives whose load-time
// behaviour is traced (docs/squirrel/RE_MBTL_LOAD.md, "Load-time natives") answer from it instead of a type default.
struct LoadContext {
	// The character is on the engine's object stack only while its cmd/mv/com files run (not during the battle VM setup
	// or its sc file): natives that read "the current object" return -1 / 0 without it.
	bool objectPushed = false;
	int charaNo = -1;        // object +5 (BMvTbl.GetCharaNo, MvStatus.CharaNo) and the player data (BMvCore.GetCharaNo)
	int vsCharaNo = -1;      // g_VsScreenCharaNo: the chara-list index the loader set (BMvCore.VsScreen_GetCharaNo)
	int side = 0;            // 0 = player 1, 1 = player 2 (BMvTbl.GetPlayerSide)
	int gameMode = 1, gameModeSub = 0;   // g_GameMode / g_GameModeSub (MvStageStatus.IsTrainingBattle; 0+1 or 3 = training)
	int rapidBeatType = 0;   // root chara +20 (0..3) for BMvTbl.GetRapidBeatType
	// PTCN lookup: first pattern with that code name in the character's HA6 stack (datatype 0) or data/effect.txt's
	// (datatype 1); -1 when absent (CharaObj_FindPatternByCodeName 0x41E770).
	std::function<int(int datatype, const std::string& code)> patternByCode;
	// Called when a PTCN lookup returns -1 (the scripts then carry -1 as the move's pattern: Init_Std's SetPattern(-1)).
	// `move` is the calling script's local `mvname` (the move Battle_Std.MakeMoveTmpl is building), "" when there is none.
	// `inits`: the source ranges of the move's own Init / Init_Before / Init_After (the caller's t[mvname] table), which can set
	// the real pattern after the template's Init_Std called SetPattern(-1).
	std::function<void(int datatype, const std::string& code, const std::string& move, const std::vector<SourceRange>& inits)> onMissingPattern;
};

struct CharaLoadIssue {
	enum Kind { RuntimeError, MissingTable, MissingPattern, DevWarning };
	Kind kind = RuntimeError;
	std::string file;      // game-relative ("data/chr001/chr001_mv_0.txt"); "" when not tied to a file
	int line = 0;          // 1-based, 0 = unknown
	std::string move;      // MissingPattern: the move
	std::string message;
};

struct CharaLoadResult {
	std::vector<CharaLoadIssue> issues;
	std::set<std::string> commandMoves;   // UpdateTable.name of every <CHR>_CommandTable entry
	std::set<std::string> moves;          // keys of the built <CHR>_MoveTable (what the engine can switch to)
	int filesRun = 0;
	bool ok() const;                     // no RuntimeError / MissingTable
};

} // namespace sqedit

#endif
