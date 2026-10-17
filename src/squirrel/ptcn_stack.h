#ifndef SQUIRREL_PTCN_STACK_H_GUARD
#define SQUIRREL_PTCN_STACK_H_GUARD

// PTCN (pattern code name) -> first pattern index of a project .txt's [DataFile] HA6 stack, loaded the way the editor and
// the game stack it (files after the project's own file only fill empty slots). Used for BMvEff.GetPatternNum at load time
// (docs/squirrel/RE_MBTL_LOAD.md). Built into sqtool and the editor (needs FrameData).

#include <map>
#include <string>

namespace sqedit {

std::map<std::string, int> LoadPatternCodes(const std::string& txtPathUtf8);

} // namespace sqedit

#endif
