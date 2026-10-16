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

} // namespace sqedit

#endif
