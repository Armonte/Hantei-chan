#ifndef S3D_VIEW_H_GUARD
#define S3D_VIEW_H_GUARD
// "3D Stage" dockable window: renders a French-Bread 3D stage (UNI2 / UNI / MBTL bg.fbx.bin, BgList.txt camera and fog) the
// way the game's DefaultShader_Easy draws it (docs/stage3d/SCOPE.md §3). Off-screen at the stage's TargetW x TargetH (1280x720),
// shown scaled to the window, exportable 1:1 for side-by-side checks against game captures.
#include <string>

namespace stage3dview {

extern bool show;
void MenuItem();
void BlenderPrefsItems();   // Preferences: Blender executable for "Open in Blender" (+ auto-detect)
void Draw();   // call once per frame from the ImGui build phase (renders into its own FBO, restores GL state)
// --ui-script: `stage3d open <bg folder> <bgNNN>` | `stage3d frame <n>` | `stage3d cam <x> <y> <zoom>` | `stage3d capture <png>`
// | `stage3d set <key> <value>` (lift, contrast, cull, fog, fov) | `stage3d status`
// | `stage3d blender [launch|launch-bg]` (print / run the Open in Blender command; launch-bg adds --background) | `stage3d reload` (= Reload from Blender)
// | `stage3d stages [bg folder]` (stage folders with bg.fbx.bin or bg.fbx); `status` also prints model=, watch= (the files
// "changed on disk" polls, bg.fbx.bin first) and changed_on_disk=0|1
// True when a watched model file changed since the last load (polls the disk now). --ui-script `waits3dchange <s>`.
bool ChangedOnDiskNow();
bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg);

}  // namespace stage3dview
#endif
