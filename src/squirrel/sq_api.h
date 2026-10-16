#ifndef SQUIRREL_SQ_API_H_GUARD
#define SQUIRREL_SQ_API_H_GUARD

// Installs a game's Sqrat API table (docs/squirrel/api/<game>_api.json, extracted from the IDB) into a Squirrel 2.2.4 VM:
// tables, constants, natives and the engine's classes. Shared by sqtool and the Script panel's VM.
//
// Classes are real Squirrel classes with exactly the fields the exe binds (Sqrat_Class_Var*, typed) and its methods, so a
// misspelled field or a new member fails as it does in the game. A native whose wrapper pushes a class instance returns a
// new instance of that class. Natives are stubs (their bodies live in the exe); the stub function is supplied by the caller
// and reads its tag with StubTag(), then pushes the return value with PushStubReturn().

#include <squirrel.h>

#include <json/json.hpp>

#include <functional>
#include <string>

namespace sqedit {

struct ApiInstallStats {
	int tables = 0, natives = 0, consts = 0, classes = 0, fields = 0, methods = 0, skipped = 0;
};

ApiInstallStats InstallSqratApi(HSQUIRRELVM v, const nlohmann::json& api, SQFUNCTION stub);

// Inside a stub: its tag "<ret>|<Qualified.Name>" (ret "instance:<Class>" for class returns).
std::string StubTag(HSQUIRRELVM v);
// Pushes the declared return type's value (0, 0.0, "", a new instance of the class, an empty array); returns the count.
SQInteger PushStubReturn(HSQUIRRELVM v, const std::string& tag);

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
	std::function<void(int datatype, const std::string& code, const std::string& move)> onMissingPattern;
};

// The context the traced natives of VM v answer from (nullptr = none: they fall back to type defaults).
void SetLoadContext(HSQUIRRELVM v, const LoadContext* ctx);
// Inside a stub: handles a native with traced load-time behaviour (GetPatternNum, CheckHan6Data, GetCharaNo, GetMvStatus,
// GetPlayerSide, VsScreen_GetCharaNo, IsTrainingBattle, GetRapidBeatType, GetVector_FromAngle). Returns -1 when not one of them.
SQInteger CallTracedNative(HSQUIRRELVM v, const std::string& tag);

} // namespace sqedit

#endif
