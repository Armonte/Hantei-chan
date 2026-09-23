#ifndef STARTUP_ARGS_H_GUARD
#define STARTUP_ARGS_H_GUARD

// Command-line startup actions (parsed in main.cpp, run by MainFrame):
//   --open <file>        open a character: .txt, .ha6, MBAC .DAT (by content) or .hproj
//   --pattern N / --frame N   select pattern / frame in the opened view
//   --palette N          palette number
//   --zoom F             view zoom
//   --game auto|mbaacc|uni|mbtl   HA6 dialect override (View > Game format)
//   --no-pups            ignore PUPS (draw with <cg>.pal only)
//   --compare N          overlay pattern N of the same character (pattern comparison)
//   --tool NAME          open a tool window: hud, bgm, compare, patterns, notes, vars, keys
//   --capture <png>      after a few frames, save the window to <png> and quit
//                        (used for automated render checks, e.g. MBAC sprites)
#include <string>

struct StartupArgs {
	std::string open;
	int pattern = -1;
	int frame = -1;
	int palette = -1;
	float zoom = 0.f;
	std::string game;
	bool noPups = false;   // --no-pups: draw every pattern with <cg>.pal
	int compare = -1;
	std::string tool;
	std::string capture;
	int frameCounter = 0;
};
extern StartupArgs gStartup;

#endif
