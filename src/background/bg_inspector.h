// ImGui panels for the stage view: game flavour / RNG seed, side-file
// metadata (BgList.ini, bgNNInfo.txt, bgNNlight.txt), and the object / frame
// / event-record editor with save. Drawn inside the host's
// "Background Inspector" window.
#ifndef BG_INSPECTOR_H_GUARD
#define BG_INSPECTOR_H_GUARD

#include <string>

namespace bg {

class File;
class Renderer;

struct InspectorResult {
	std::string openPath;   // non-empty: host should open this stage (sibling variant)
};

InspectorResult DrawInspector(File& file, Renderer& renderer);

} // namespace bg

#endif // BG_INSPECTOR_H_GUARD
