#ifndef ARCHIVE_BROWSER_H_GUARD
#define ARCHIVE_BROWSER_H_GUARD

// ONE browser for every French-Bread archive and game folder (docs/ux/archive_audit_2026-10-04.md).
//   * Open an archive (.PAC, GOF2 data0x.dat, GOF1 .p, MBAC / MB / PB .p) or a game folder (GOF1 / GOF2 / RBO / MBAACC ...): the game is detected, its archives
//     are mounted (later archives override earlier ones, as the game's own loader does) and listed together with the loose files.
//   * Selecting an entry previews it in place (images, CG banks with palettes and frame list, EX3, characters with a thumbnail and facts, text, hex): nothing is extracted.
//   * Double click / Enter opens the entry in the proper editor straight from the archive; Save writes back into the archive with a .bak.
//   * Extract is an optional action (selected entries or everything, to a folder you choose).
// Decoding, thumbnails and archive reads run on one worker thread; the UI thread only uploads textures and draws.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class CharacterInstance;

namespace abrowser {

enum class Game { None, RBO, GOF2, GOF1, MBAACC, MB, PB2K1, Generic, UNI2, MBTL, DFCI, UNIST, UNI, MBAC, REACT, QOH99, QOH98, ROSA, LILIAN, DMP };
const char *GameName(Game g);

struct GameInfo {
	Game game = Game::None;
	std::string root;                    // the folder the user picked
	std::string dataDir;                 // folder that holds the archives / loose character files
	std::vector<std::string> archives;   // UTF-8 paths, game priority order (later overrides earlier)
};
// Looks at file names only (no archive is opened). Game::None when the folder holds nothing recognisable.
GameInfo DetectGame(const std::string &dir);

extern bool show;                        // the dockable window
// Opens a game folder, an archive or a loose file's folder in the browser (and shows the window). Returns an error text or empty.
std::string OpenPath(const std::string &path);
// File dialogs + OpenPath.
void OpenArchiveDialog();
void OpenGameFolderDialog(Game want = Game::None);   // Game::None = auto-detect
// The last browsed folder / archive (restored on the next start). Empty = none.
const std::string &LastBrowsed();
const std::vector<std::string> &RecentBrowsed();

// What the browser asks MainFrame to do with the entry the user opened.
struct OpenRequest {
	enum Kind { None, Han2Stem, Gof1Entry, LooseFile } kind = None;
	std::string stem;                                      // Han2Stem: character file name without extension
	std::function<bool(const std::string &, std::vector<uint8_t> &)> read;   // Han2Stem: reads the character's files from the mounted archives (priority order)
	std::string archivePath, entryName, origin;            // Gof1Entry / write-back target
	std::string path;                                      // LooseFile: a file on disk (loose, or a managed working copy of an archive entry)
};
// Draws the window; true when the user opened something (req filled).
bool Draw(OpenRequest &req);

// ---- writing back -----------------------------------------------------------------------------------------------------------------------------------
struct EntryEdit { std::string name; std::vector<uint8_t> data; };   // name as stored (CP932 for PAC / GOF1)
// Replaces / adds entries in an archive file in place: <archive>.bak is kept (the first time), the rebuilt archive is written next to it and swapped in.
// Any mounted copy of the archive in the browser is re-read. Works for PAC (RBO / GOF2), GOF1 .p and every fbarc kind.
bool ReplaceEntries(const std::string &archivePath, const std::vector<EntryEdit> &edits, std::string *err);

// Characters opened out of an archive: Save goes through these (all keep <archive>.bak the first time and re-read the mounted archive).
// Writes the edited character back into the archive it was opened from (PAC: .DT2 / .DAT and the changed GOF2 .PAT / .CHP; GOF1: the entry).
bool SaveCharacterIntoArchive(CharacterInstance &ch, const std::string &archivePath, std::string *err);
// A managed working copy of an archive entry (see OpenRequest::LooseFile) was saved: write its bytes back into the archive. *hadOrigin = false when the
// file is not a working copy (nothing to do).
bool SaveEntryBytesIntoArchive(const std::string &loosePath, std::string *err, bool *hadOrigin);
bool SwapRebuiltArchive(const std::string &archivePath, const std::string &rebuiltPath, std::string *err);

// ---- scripted UI (tests / screenshots without the real mouse) ----------------------------------------------------------------------------------------
// Commands (MainFrame's --ui-script runner forwards them): browse <path> | source <substring> | filter <text> | type <name|all> | select <substring> | next | prev
//   | open | key <Up|Down|Enter|Home|End|PageDown|PageUp> | frame <n> | palette <n> | zoom <fit|1|2|4> | thumbs <on|off>
bool ScriptCommand(const std::string &cmd, const std::string &arg, std::string *msg);
// Welcome screen (shown when no character is open): game buttons, recent folders / files. Returns a file the user picked to open ('' = none).
std::string DrawWelcome(const std::vector<std::string> &recentFiles);
bool Busy();          // loads / previews / thumbnails still running

} // namespace abrowser

#endif
