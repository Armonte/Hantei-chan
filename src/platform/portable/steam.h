#ifndef PORTABLE_STEAM_H_GUARD
#define PORTABLE_STEAM_H_GUARD

// Steam library discovery and the "find the game install" scan.

// Lane LINUX deliverable 2. The parsing is the same on both platforms; only the
// roots differ. It is the code game_table.cpp's SteamApps() already runs, lifted
// so the ~6 files that hard-code the Steam common path (see docs/render/linux.md
// section 4) can share one implementation.

#include <string>
#include <vector>

namespace portable {

// One Steam library folder (a folder that holds steamapps/). Normalised to '/',
// deduplicated, deterministic: the explicit override first, then the platform
// defaults, then whatever libraryfolders.vdf adds.
std::vector<std::string> SteamLibraries();

struct SteamApp {
	std::string name;     // appmanifest_*.acf "name"
	std::string dir;      // .../steamapps/common/<installdir>, '/'-separated
	std::string library;  // the library the app came from
};

// Every installed app steamapps can see, read from appmanifest_*.acf. Missing
// Steam (or a permission error) gives an empty list, which is what the install
// scan wants on a machine with no Steam.
std::vector<SteamApp> SteamApps();

// The install folder of an app whose manifest name contains nameNeedle
// (case-insensitive), or "" when not found. This is the look-up the
// default-path copies do.
std::string SteamAppDir(const std::string& nameNeedle);

}  // namespace portable

#endif /* PORTABLE_STEAM_H_GUARD */
