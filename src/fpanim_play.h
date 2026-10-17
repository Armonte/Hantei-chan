#ifndef FPANIM_PLAY_H_GUARD
#define FPANIM_PLAY_H_GUARD

// "Play in game" for UNI2 / MBTL characters: send the current pattern to the running game through fpan-im's pipe
// (`playpattern 1 <pattern> [loop]`: P1 enters the pattern through the engine's own move change, training battle only;
// with Loop it re-enters at every return to neutral until `playpattern stop`).
// Saves go into the IM mod folder and hot reload the character (ui/main_frame_hotsave.cpp). Play waits for a save's
// reload still in flight, and reloads the character itself (`reloadchara <chr>`, `... live` in live mode) only when it
// was saved into the mod folder since its last successful reload (hot reload off, or the reload failed), so one edit
// costs one reload. Pipe calls run on a worker thread.

#include <string>

class FrameData;

namespace fpanimplay {

// The active file (MainFrame::currentFilePath); the character folder (chrNNN) is taken from it.
void SetCurrentFile(const std::string& path);
// "chr005" from ".../data/chr005/chr005.ha6" (the last chrNNN path part), "" when there is none. Unit tested.
std::string CharaFolderFromPath(const std::string& path);
// The playpattern request for P1 ("playpattern 1 <pattern>", "... loop"); the loop's stop request.
std::string PlayRequest(int pattern, bool loop = false);
std::string StopRequest();
// true for the HA6 dialects fpan-im serves (UNI2, MBTL).
bool GameSupported(const FrameData* fd);
// Button + F5 shortcut + last reply, drawn in the pattern pane.
void DrawPlayButton(FrameData* fd, int pattern);

// --ui-script: playingame [loop|noloop] (press the button) | playstop | playloop on|off. true when handled.
bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg);
// A play (or its wait for a save's reload) is in flight; the last reply.
bool Busy();
std::string LastReply();

// Hot-save hand-off, UI thread (implemented in ui/main_frame_hotsave.cpp).
bool HotSaveBusy();                          // a save's hot reload is in flight
bool HotSaveStale(const std::string& chr);   // saved into the mod folder since its last successful reload
void HotSaveMarkReloaded(const std::string& chr);

} // namespace fpanimplay

#endif
