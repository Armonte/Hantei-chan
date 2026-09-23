#ifndef STARTUP_ARGS_H_GUARD
#define STARTUP_ARGS_H_GUARD

// Command-line startup actions (parsed in main.cpp, run by MainFrame):
//   --open <file>        open a character: .txt, .ha6, MBAC .DAT (by content) or .hproj
//   --pattern N / --frame N   select pattern / frame in the opened view
//   --palette N          palette number
//   --capture <png>      after a few frames, save the window to <png> and quit
//                        (used for automated render checks, e.g. MBAC sprites)
//   --game-link N        open the Game Link window, connect, follow game slot N (1-4) and jump the editor to it;
//                        with --capture the shot is taken later (frame 240) so the link has live state
#include <string>

struct StartupArgs {
	std::string open;
	int pattern = -1;
	int frame = -1;
	int palette = -1;
	std::string capture;
	int gameLinkSlot = 0;   // --game-link (1-4); 0 = off
	int frameCounter = 0;
};
extern StartupArgs gStartup;

#endif
