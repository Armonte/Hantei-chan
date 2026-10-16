#ifndef SQUIRREL_FBMOD_LINK_H_GUARD
#define SQUIRREL_FBMOD_LINK_H_GUARD

// Client side of the fbmod in-game pipe (Squirrel hot reload). fbmod serves a message-mode named pipe
// \\.\pipe\fbmod-<pid> and \\.\pipe\fbmod-<exe stem lowercase> (fbmod-uni2, fbmod-mbtl). One request per
// connection: write the ASCII request, read one reply (<= 4096 bytes), close.
//   ping            -> "OK fbmod <game> session=<name> mods=<windows path of fbmod\mods>"
//   reload chrNNN   -> "OK reloaded chrNNN (...)" or "ERROR <reason>"   (scripts only)
//   reloadchara chrNNN       -> engine training rebuild (the training menu's character switch, ~1-3 s, resets
//                               positions/meter): "OK engine rebuild chrNNN: <picks>; ha6 root XXXXXXXX -> YYYYYYYY;
//                               back in battle after N ms" ("ERROR ... NOT back in battle ..." when it did not return)
//   reloadchara chrNNN live  -> in-place reload of HA6/CG/pat/palettes + scripts, keeps state: "OK ..." / "ERROR ..."
//   reloadstd                -> shared battle scripts (script/Battle_Init, Battle_Std + every btl_*.txt, data/_combase)
//                               re-run and data/VectorTable, BattleInfo, BoundEff reparsed, then the engine rebuild:
//                               "OK reloadstd: ... | engine rebuild: ...; back in battle after N ms"
//   reloadstd live           -> the same, then sc + cmd + mv of every loaded character in place: "OK reloadstd live: ..."
//   Outside a training battle both are refused: "ERROR reloadchara refused in session <s>: ...",
//   "ERROR not in a battle (start training first)", "ERROR timeout: no battle frame within 5 s ...".
// Calls block (a reload reply waits for the next battle frame, up to ~5 s): run them off the UI thread.
// Win32 only; elsewhere every call fails with "not supported".

#include <cstdint>
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

// The parts of a reloadchara reply the panel shows (platform independent, unit tested).
struct FbmodReloadInfo {
	bool engine = false;        // an engine rebuild reply ("engine rebuild chrNNN: ...")
	bool refused = false;       // refused/timed out because no training battle is running
	bool hasRoots = false;      // "ha6 root X -> Y" present
	uint32_t rootBefore = 0, rootAfter = 0;
	bool backInBattle = false;  // the rebuild reached the training battle again
	long ms = -1;               // "after N ms" (-1: absent)
	bool stdReload = false;     // a reloadstd reply (shared scripts)
};
FbmodReloadInfo FbmodParseReloadReply(const std::string& reply);
// The request for a character reload: "reloadchara <chr>" (engine rebuild) or "reloadchara <chr> live".
std::string FbmodReloadRequest(const std::string& chr, bool live);
// A game-relative path ("script/btl_MvFunc.txt", "data/VectorTable.txt") of a shared battle file: every script/*.txt,
// and data/_combase.txt, VectorTable.txt, BattleInfo.txt, BoundEff.txt. The game loads these once per process, so a
// save needs "reloadstd" instead of a character reload.
bool FbmodIsSharedFile(const std::string& rel);
// "reloadstd" (engine rebuild of every character) or "reloadstd live".
std::string FbmodStdReloadRequest(bool live);
// The fbmod pipe of a script-panel game profile: "mbtl" -> "fbmod-mbtl", anything else -> "fbmod-uni2" (fbmod runs
// on UNI2 1.42 and MBTL only). Both games take the same requests (reloadchara, reloadstd [live]).
std::string FbmodPipeForGame(const std::string& game);
// Where a saved file goes under the fbmod mods folder: the same game-relative path (data/chrNNN/..., script/...,
// data/VectorTable.txt). Both games read their archives as data/... and script/..., so no per-game prefix. "./" and
// backslashes are normalised; "" for a path that leaves the folder ("..") or is absolute.
std::string FbmodModsTarget(const std::string& modsDir, const std::string& rel);

} // namespace sqedit

#endif
