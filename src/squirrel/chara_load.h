#ifndef SQUIRREL_CHARA_LOAD_H_GUARD
#define SQUIRREL_CHARA_LOAD_H_GUARD

// Runs one character's scripts the way MBTL loads them (docs/squirrel/RE_MBTL_LOAD.md) in a VM that already ran the battle
// VM setup (Battle_Init, Battle_Std, data/_combase.txt), and reports what the engine would find. Shared by sqtool and the
// Script panel.
//
// Order (Chara_LoadBattleScripts 0x607650), variant 0: <chr>_sc_0 (engine reads and deletes root Battle_Status), then the
// character is pushed on the object stack, <chr>_cmd_0 (engine reads root <CHR>_CommandTable as an array), <chr>_mv_0
// (engine reads root <CHR>_MoveTable), <chr>_com_0.

#include "sq_api.h"

#include <squirrel.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace sqedit {

// Runs a game-relative script file in v; false + error text/line on a compile or runtime error.
using RunScriptFn = std::function<bool(const std::string& rel, std::string* error, std::string* errorFile, int* errorLine)>;

// Installs the observer that records the developers' btl_debError_CharacterMake messages (silenced in release builds;
// the original still runs). Call once after the battle VM setup.
void InstallDevWarningObserver(HSQUIRRELVM v);

// chrDirRel: "data/chr001". ctx.objectPushed is managed here; ctx.patternByCode must be set by the caller.
// readFile: raw bytes of a game-relative file (the missing-pattern check reads the move's own Init code).
CharaLoadResult LoadCharacterScripts(HSQUIRRELVM v, const std::string& chrDirRel, LoadContext& ctx, const RunScriptFn& run,
	const std::function<bool(const std::string& rel)>& exists, const std::function<std::string(const std::string& rel)>& readFile);

} // namespace sqedit

#endif
