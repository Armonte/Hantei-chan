#ifndef STARTUP_ARGS_H_GUARD
#define STARTUP_ARGS_H_GUARD

// Command-line startup actions (parsed in main.cpp, run by MainFrame):
//   --open <file>        open a character: .txt, .ha6, MBAC .DAT (by content) or .hproj
//   --pattern N / --frame N   select pattern / frame in the opened view
//   --palette N          palette number
//   --capture <png>      after a few frames, save the window to <png> and quit
//                        (used for automated render checks, e.g. MBAC sprites)
// Wave 2 render checks (docs/HANTEI_WAVE2.md §7):
//   --tick N             set the view's simulation tick (root frame follows)
//   --onion B,A,S[,K]    onion skin: B before, A after, spacing S, K=1 keyframes
//   --detach             move the opened view into a detached window
//   --capture-view <png> save the opened view's own render target
//   --export-png <dir>   export PNGs (UTF-8 path) with the options below, then continue
//   --export-range current|pattern|A:B   (default pattern)
//   --export-scale N  --export-bg transparent|editor  --export-boxes  --export-nospawns
//   --export-crop all|each
//   --save-project <hproj>  save the project (atomic) before quitting
//   --capture-windows <prefix>  save every detached OS window (PrintWindow) as <prefix>_<n>.png
//   --post-key <vk>      frame 12: post WM_KEYDOWN <vk> to the first detached window
//                        (checks shortcut routing through the HWND subclass)
//   --stats <file>       frames 10-19: average scene render / onion-skin cost (CPU ms)
//   --quit               quit after the actions even without --capture
#include <string>
#include <cwchar>

struct StartupArgs {
	std::string open;
	int pattern = -1;
	int frame = -1;
	int palette = -1;
	std::string capture;
	int frameCounter = 0;

	int tick = -1;
	int onionBefore = -1, onionAfter = 0, onionSpacing = 1;
	bool onionKeyframes = false;
	bool detach = false;
	std::string captureView;
	std::string captureWindows;  // UTF-8 prefix
	std::string exportDir;       // UTF-8
	std::string exportRange = "pattern";
	int exportScale = 1;
	bool exportTransparent = true;
	bool exportBoxes = false;
	bool exportSpawns = true;
	bool exportFitEach = false;
	std::string saveProject;
	std::string stats;           // UTF-8
	int postKey = 0;
	double sceneMs = 0, onionSimMs = 0, onionTotalMs = 0;
	int statFrames = 0, onionSamples = 0;
	bool quit = false;
};

// Parses one wave-2 option; advances i past its value. False if not ours.
bool ParseWave2StartupArg(const char* arg, const wchar_t* next, int& i);
extern StartupArgs gStartup;

#endif
