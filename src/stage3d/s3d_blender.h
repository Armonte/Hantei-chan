#ifndef S3D_BLENDER_H_GUARD
#define S3D_BLENDER_H_GUARD
// 3D stage view <-> Blender link: "Open in Blender" starts Blender with the MBTL_fbx_json add-on
// (github.com/Armonte/MBTL_fbx_json) importing the current stage through its mbtl.hantei_open operator; the add-on's export
// writes bg.fbx.bin back into the stage folder and "Reload from Blender" re-reads it. Pure helpers here (unit-tested); the
// launch itself lives in s3d_view.cpp.
#include <string>
#include <vector>

namespace s3d {

// Python for `blender --python-expr`: enables the add-on and imports `stagePath` (bg.fbx.bin or bg.fbx). Backslashes become
// forward slashes (a raw string cannot end with one). Empty + err when the path cannot be embedded (quotes, control chars).
std::string BlenderImportExpr(const std::string& stagePath, std::string& err);
// Full command line for CreateProcess: "<blender>" --python-expr "<expr>". Empty + err on failure.
std::string BlenderCommandLine(const std::string& blenderExe, const std::string& stagePath, std::string& err);
// Candidate blender.exe paths, newest first: every "Blender X.Y" folder name in `foundationDirNames` (children of
// C:/Program Files/Blender Foundation) sorted by version descending, then C:/dev/blender40/blender-4.0.2-windows-x64.
std::vector<std::string> BlenderCandidates(const std::vector<std::string>& foundationDirNames);
// First candidate that exists on disk, "" when none.
std::string DetectBlender();

}  // namespace s3d
#endif
