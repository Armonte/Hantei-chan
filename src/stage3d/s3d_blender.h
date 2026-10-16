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
// Candidate blender.exe paths, newest version first, from two install layouts:
//  - `foundationDirNames`: children of C:/Program Files/Blender Foundation named "Blender X.Y" (installer)
//  - `appsDirNames`: children of C:/apps named "blender-X.Y.Z-windows-x64" (portable zip)
// Versions compare numerically per component (5.2.2 > 5.2 > 4.10 > 4.5); on a tie the portable build comes first.
// C:/dev/blender40/blender-4.0.2-windows-x64 is always last.
std::vector<std::string> BlenderCandidates(const std::vector<std::string>& foundationDirNames,
                                           const std::vector<std::string>& appsDirNames = {});
// First candidate that exists on disk, "" when none.
std::string DetectBlender();

// Files "changed on disk" watches for a stage loaded from `modelPath`: the add-on's export always writes bg.fbx.bin into
// the stage folder (also for an ASCII bg.fbx stage, DFCI / UNIB), so that comes first; the loaded file second when it
// is a different file. Forward slashes.
std::vector<std::string> BlenderWatchPaths(const std::string& modelPath);
// Sub-folders of `bgDir` (names, sorted) holding a stage model: bg.fbx.bin (fbxex or UNIEL enciphered) or bg.fbx.
std::vector<std::string> ScanStageFolders(const std::string& bgDir);

}  // namespace s3d
#endif
