#ifndef HMOD_UI_H_GUARD
#define HMOD_UI_H_GUARD
// The "Mod Project" panel (docs/modmanager/AUTHORING_DESIGN.md): open / create a hmod.json project, see every working file's
// change status against vanilla, check conflicts and export (pcmod for MBAACC, UNI2-IM / MBTL-IM mod folders for UNI2 / MBTL).
#include <string>

namespace hmodui {
void MenuItem();
void Open();
void Draw();
// --ui-script: `hmod open <dir> | new <dir> <id> <game> | gamedir <dir> | index | status | check [pkg] | export [pkg] | out <dir> | mods <0|1> | zip <0|1>`.
bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg);
}
#endif
