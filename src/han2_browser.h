#ifndef HAN2_BROWSER_H_GUARD
#define HAN2_BROWSER_H_GUARD

// PAC archive browser (RBO *.PAC, GOF2 data0x.dat): mounts several archives, lists their entries, opens characters,
// extracts entries. Replaces PACNyx+'s archive tree. docs/formats/pacnyx_parity.md section A/B.

#include "han2_character.h"
#include <string>

namespace han2ui {

extern bool showBrowser;

struct OpenRequest {
	std::string stem;           // character file name without extension
	han2::ReadFn read;          // reads files from the mounted archives (later entries in the mount list override earlier ones)
	std::string origin;
};

// Mount an archive (error text when it is not a PAC).
std::string AddArchive(const std::string &path);
// Draws the window; returns true when the user asked to open a character.
bool DrawBrowser(OpenRequest &req, std::string &message);

} // namespace han2ui

#endif
