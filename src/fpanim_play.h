#ifndef FPANIM_PLAY_H_GUARD
#define FPANIM_PLAY_H_GUARD

// "Play in game" for UNI2 / MBTL characters: send the current pattern to the running game through fpan-im's pipe
// (`playpattern 1 <pattern>`: P1 enters the pattern through the engine's own move change, training battle only).
// When script hot reload is on (Settings sqHotReload), the character is reloaded first (`reloadchara <chr>`, or
// `... live` in live mode) so the saved edit is what plays. Pipe calls run on a worker thread.

#include <string>

class FrameData;

namespace fpanimplay {

// The active file (MainFrame::currentFilePath); the character folder (chrNNN) is taken from it.
void SetCurrentFile(const std::string& path);
// "chr005" from ".../data/chr005/chr005.ha6" (the last chrNNN path part), "" when there is none. Unit tested.
std::string CharaFolderFromPath(const std::string& path);
// The playpattern request for P1 ("playpattern 1 <pattern>").
std::string PlayRequest(int pattern);
// true for the HA6 dialects fpan-im serves (UNI2, MBTL).
bool GameSupported(const FrameData* fd);
// Button + F5 shortcut + last reply, drawn in the pattern pane.
void DrawPlayButton(FrameData* fd, int pattern);

} // namespace fpanimplay

#endif
