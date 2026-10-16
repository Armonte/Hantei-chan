#ifndef SQUIRREL_FBMOD_LINK_H_GUARD
#define SQUIRREL_FBMOD_LINK_H_GUARD

// Client side of the fbmod in-game pipe (Squirrel hot reload). fbmod serves a message-mode named pipe
// \\.\pipe\fbmod-<pid> and \\.\pipe\fbmod-<exe stem lowercase> (fbmod-uni2, fbmod-mbtl). One request per
// connection: write the ASCII request, read one reply (<= 4096 bytes), close.
//   ping            -> "OK fbmod <game> session=<name> mods=<windows path of fbmod\mods>"
//   reload chrNNN   -> "OK reloaded chrNNN (...)" or "ERROR <reason>"
// Calls block (a reload reply waits for the next battle frame, up to ~5 s): run them off the UI thread.
// Win32 only; elsewhere every call fails with "not supported".

#include <string>

namespace sqedit {

struct FbmodReply {
	bool connected = false; // false: no pipe by that name (game not running / fbmod not loaded)
	bool ok = false;        // reply starts with "OK"
	std::string text;       // reply text, or the transport error
};

// pipeName: "fbmod-uni2" or a full "\\.\pipe\..." path.
FbmodReply FbmodRequest(const std::string& pipeName, const std::string& request, unsigned timeoutMs = 8000);
// The mods= value of a ping reply ("" when absent). The path runs to the end of the reply (it may hold spaces).
std::string FbmodModsDir(const std::string& pingReply);

} // namespace sqedit

#endif
