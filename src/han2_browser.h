#ifndef HAN2_BROWSER_H_GUARD
#define HAN2_BROWSER_H_GUARD

// PAC archive browser (RBO *.PAC, GOF2 data0x.dat): mounts several archives, lists their entries, opens characters,
// extracts entries. Replaces PACNyx+'s archive tree. docs/formats/pacnyx_parity.md section A/B.

#include "han2_character.h"
#include <string>
#include <vector>

namespace han2ui {

extern bool showBrowser;

struct OpenRequest {
	std::string stem;           // character file name without extension
	han2::ReadFn read;          // reads files from the mounted archives (later entries in the mount list override earlier ones)
	std::string origin;
	std::string gof1Archive, gof1Entry;   // set for GOF1 .p entries: archive path + entry name
};

// UI language lives in i18n::language (src/i18n.h); persisted in han2_settings.ini next to the exe.

const char *Tr(const char *en, const char *jp);
void LoadHan2Settings();
void SaveHan2Settings();
// Loading report: one entry per load attempt with a summary and the warnings / errors found (PACNyx DATLoading equivalent).
void PushLoadReport(const std::string &name, const std::string &summary, const std::vector<std::string> &warnings, bool failed);
extern bool showLoadReport;
void DrawLoadReport();
// Working folder: remembered between sessions, listed as a tree in the browser.
const std::string &WorkFolder();
void SetWorkFolder(const std::string &dir);

// Live reload (docs/formats/dmp_live_reload.md): the game folder PovertyCaster runs, remembered between sessions.
const std::string &LiveReloadGameDir();
void SetLiveReloadGameDir(const std::string &dir);

// Mount an archive (error text when it is not a PAC).
std::string AddArchive(const std::string &path);
// Draws the window; returns true when the user asked to open a character.
bool DrawBrowser(OpenRequest &req, std::string &message);

} // namespace han2ui

#endif
