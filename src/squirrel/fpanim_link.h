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

// ---- saves from the main editor (HA6, .pat, .cg, .pal), UNI2 / MBTL ----
// The improvement mod folder the main editor saves into: "<gameDir>/UNI2-IM/Mods" (uni2) or "<gameDir>/MBTL-IM/Mods" (mbtl),
// forward slashes; "" for another game or an empty gameDir. fpan-im's VFS mounts it (core imCompatLayers), so a file there
// replaces the archived one with the same game-relative path.
std::string FpanimImModsRoot(const std::string& game, const std::string& gameDir);
// The game-relative path of a saved file: the part from its last "data/" or "script/" folder on
// ("C:/tmp/hantei_archive/uni2_d/data/chr003/chr003.ha6" -> "data/chr003/chr003.ha6"); "" when there is none.
std::string FpanimGameKey(const std::string& path);
// The character folder of a game-relative key, lowercased: "data/<folder>/<file>" where <file> starts with <folder>
// (chr003/chr003.ha6, chr000/chr000_mv_0.txt, and custom-named UNI2-IM characters such as Okita/Okita.ha6 or
// Okita/Okita_0.txt) and the folder is a name fpan-im accepts (a-z 0-9 _, at most 32). "" for anything else
// (shared files, data/_csel/..., deeper paths). fpan-im finds the loaded character by that folder in its resource paths.
std::string FpanimCharaFolder(const std::string& key);
// True when the saved file already sits inside an improvement mod's own folders, which fpan-im rescans before every
// reload: <...>/UNI2-IM/Mods/... or <...>/UNI2-IM/Packs/<pack>/... (MBTL-IM likewise). Saving there needs no copy.
bool FpanimInImFolder(const std::string& game, const std::string& savedPath);
// What a save from the main editor does. Pure: no file is touched.
struct FpanimSavePlan {
	std::string key;        // game-relative path ("" -> not a game file: no mod-folder copy, no reload)
	std::string modsDst;    // where the file goes in the IM mod folder ("" = not copied: setting off, no game folder, bad key)
	bool alreadyInMods = false; // the saved file is already in UNI2-IM/MBTL-IM Mods or a Pack (CG manager "Save to mod folder", or a pack being edited in place)
	std::string chr;        // the character folder (FpanimCharaFolder): "chr003", or a custom name such as "okita"
	bool shared = false;    // a shared battle file (FpanimIsSharedFile): reloadstd
	std::string request;    // "reloadchara chr003 [live]" / "reloadstd [live]"; "" = no reload (hot reload off, or neither kind)
	std::string pipe;       // FpanimPipeForGame(game)
};
// game: "uni2" / "mbtl" (anything else: no mod folder, no reload). savedPath: the file just written (UTF-8, either slash).
// toMods: the "UNI2/MBTL: save into the IM mod folder" setting of that game. hotReload / live: the shared Hot reload toggle
// and Reload mode (Script panel and main editor).
FpanimSavePlan FpanimPlanSave(const std::string& game, const std::string& gameDir, const std::string& savedPath, bool toMods,
                              bool hotReload, bool live);

} // namespace sqedit

#endif
