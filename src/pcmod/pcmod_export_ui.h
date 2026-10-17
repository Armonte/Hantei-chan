#ifndef PCMOD_EXPORT_UI_H_GUARD
#define PCMOD_EXPORT_UI_H_GUARD
// "Export as pcmod package..." (Hantei_Docs/pcmod_export.md): ImGui front end over pcmod_package.h. Lists the files with
// their hashes, auto-detects sim (data\) vs cosmetic (grp\ ...), warns when a file replaces one the MBAACC install has,
// and writes <out>\<id>\pcmod.json + files + <out>\<id>-<version>.zip.
#include <string>

namespace pcmodui {
// The menu item. `descriptorUtf8` = the active character's .txt ("" = start empty and add files / a folder).
void MenuItem(const std::string& descriptorUtf8);
void Open(const std::string& descriptorUtf8);
void Draw();
// --ui-script: `pcmod open [chara.txt] | add <file> | addfolder <dir> | set <field> <value> | export | status`.
bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg);
}
#endif
