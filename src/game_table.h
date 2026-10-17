#ifndef GAME_TABLE_H_GUARD
#define GAME_TABLE_H_GUARD

// THE list of French-Bread titles Hantei-chan opens: one row per game (id, names, family, install-detection hints, archive types, browse support).
// The start screen, File > Open Game Folder, install auto-detection and docs/ux/start_screen_games.md are all driven from this table:
// adding a game is one row here plus (if its folder looks new) a rule in abrowser::DetectGame.
#include "archive_browser.h"
#include <string>
#include <vector>

namespace abrowser {

enum class Family { Hantei6Modern, Hantei6, Hantei4, Han2, Gof1Pb, QoH, Other };
// What "Open <game>" gives you.
enum class Support {
	Characters,   // archives mounted, characters listed with thumbnails, double click opens the editor
	CharactersLoose,   // same, from the game's loose files (no archive to mount)
	FilesOnly     // folder / archives browsed with inline previews; characters of this title have no editor yet (see `note`)
};

struct GameDef {
	Game id;
	const char *shortName;                 // "MBTL": the button reads "Open MBTL"
	const char *name, *nameJa;             // full title
	Family family;
	const char *loader;                    // which loader opens a character
	const char *archives;                  // container / archive types
	std::vector<const char *> steamNames;  // substrings of the Steam app name (matched against appmanifest "name")
	std::vector<const char *> commonPaths; // folders to try (Windows paths)
	Support support;
	const char *note;                      // what is missing, if anything
};

const std::vector<GameDef> &Games();
const GameDef *FindGame(Game g);
const char *FamilyName(Family f, bool ja);

// Where a game is installed on this machine.
struct Install {
	std::string path;      // empty = not found
	std::string how;       // "remembered", "Steam", "common path"
};
// Result of the last scan (empty until RescanInstalls finished). Thread: UI.
const Install &InstallOf(Game g);
bool ScanDone();
// Runs on the browser worker: Steam libraryfolders.vdf + appmanifests, common paths, the remembered folder per game. Each candidate must DetectGame() as the row's game.
void RescanInstalls();
// Remembered folder (han2_settings.ini [browser] Dir_<id>).
std::string RememberedDir(Game g);
void RememberDir(Game g, const std::string &dir);

} // namespace abrowser
#endif
