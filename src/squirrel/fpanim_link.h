#ifndef SQUIRREL_FPANIM_LINK_H_GUARD
#define SQUIRREL_FPANIM_LINK_H_GUARD

// Client side of the fpan-im in-game pipe (Squirrel hot reload). fpan-im serves a message-mode named pipe
// \\.\pipe\fpan-im-<pid> and \\.\pipe\fpan-im-<exe stem lowercase> (fpan-im-uni2, fpan-im-mbtl). One request per
// connection: write the ASCII request, read one reply (<= 4096 bytes), close.
//   ping            -> "OK fpan-im <game> session=<name> mods=<windows path of fpan-im\mods>"
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

struct FpanimReply {
	bool connected = false; // false: no pipe by that name (game not running / fpan-im not loaded)
	bool ok = false;        // reply starts with "OK"
	std::string text;       // reply text, or the transport error
};

// pipeName: "fpan-im-uni2" or a full "\\.\pipe\..." path.
FpanimReply FpanimRequest(const std::string& pipeName, const std::string& request, unsigned timeoutMs = 8000);
// The mods= value of a ping reply ("" when absent). The path runs to the end of the reply (it may hold spaces).
std::string FpanimModsDir(const std::string& pingReply);

// The parts of a reloadchara reply the panel shows (platform independent, unit tested).
struct FpanimReloadInfo {
	bool engine = false;        // an engine rebuild reply ("engine rebuild chrNNN: ...")
	bool refused = false;       // refused/timed out because no training battle is running
	bool hasRoots = false;      // "ha6 root X -> Y" present
	uint32_t rootBefore = 0, rootAfter = 0;
	bool backInBattle = false;  // the rebuild reached the training battle again
	long ms = -1;               // "after N ms" (-1: absent)
	bool stdReload = false;     // a reloadstd reply (shared scripts)
};
FpanimReloadInfo FpanimParseReloadReply(const std::string& reply);
// The request for a character reload: "reloadchara <chr>" (engine rebuild) or "reloadchara <chr> live".
std::string FpanimReloadRequest(const std::string& chr, bool live);
// A game-relative path ("script/btl_MvFunc.txt", "data/VectorTable.txt") of a shared battle file: every script/*.txt,
// and data/_combase.txt, VectorTable.txt, BattleInfo.txt, BoundEff.txt. The game loads these once per process, so a
// save needs "reloadstd" instead of a character reload.
bool FpanimIsSharedFile(const std::string& rel);
// "reloadstd" (engine rebuild of every character) or "reloadstd live".
std::string FpanimStdReloadRequest(bool live);
// The fpan-im pipe of a script-panel game profile: "mbtl" -> "fpan-im-mbtl", anything else -> "fpan-im-uni2" (fpan-im runs
// on UNI2 1.42 and MBTL only). Both games take the same requests (reloadchara, reloadstd [live]).
std::string FpanimPipeForGame(const std::string& game);
// The old name of an fpan-im pipe ("fpan-im-uni2" -> "fbmod-uni2", full "\\.\pipe\..." paths kept full), "" for any
// other name. The project was called fbmod until 2026-10-16; FpanimRequest retries there once when the new name is
// absent, so an older game-side build still answers. Remove after the next release.
std::string FpanimLegacyPipeName(const std::string& pipeName);
// Where a saved file goes under the fpan-im mods folder: the same game-relative path (data/chrNNN/..., script/...,
// data/VectorTable.txt). Both games read their archives as data/... and script/..., so no per-game prefix. "./" and
// backslashes are normalised; "" for a path that leaves the folder ("..") or is absolute.
std::string FpanimModsTarget(const std::string& modsDir, const std::string& rel);

} // namespace sqedit

#endif
