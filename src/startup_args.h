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
//   --tool tag           open the Tag / Team window (experimental); --tool gamelink opens the Game Link window
//   --tag-ini <path>     the tag_tuning.ini it edits (default: next to the linked MBAA.exe)
//   --tag-char <file>    the [char.<file>] it shows   --tag-tab Global|Character|Assists|Live|Raw
//   --tag-apply          press Apply to game once (frame 150; checks the gate / validation from a script)
//   --tag-link           connect the Game Link for it (with --capture the shot is taken at frame 240, as --game-link)
//   --tool authoring     [authoring] open the Authoring workspace (docs/HANTEI_AUTHORING_MODE.md); --tool tag opens it on
//                        its Tuning tab (the old Tag / Team panel: Experimental: Authoring > Legacy, or --tag-ini)
//   --authoring-tab Setup|Setups|Tuning|HUD|Live|Log   --authoring-setup <128 hex>   --authoring-game <dir>
//   --authoring-tag-root <povertycaster\tag dir>   --authoring-char <file>   --authoring-layer all|c|f|h
//   --authoring-view global   --authoring-link (connect; the capture waits to frame 240)   --authoring-load (Load in game)
//   --authoring-pid <n> (link only to that pid)   --authoring-unlock (session: files unlocked)   --authoring-ab 1 (an A/B pair for a capture; nothing is written)
//   --game-link N        open the Game Link window, connect, follow game slot N (1-4) and jump the editor to it;
//                        with --capture the shot is taken later (frame 240) so the link has live state
#include <string>
#include <cwchar>
#include "authoring/authoring_window.h"

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
	std::string tagIni, tagChar, tagTab;   // --tag-ini / --tag-char / --tag-tab
	bool tagLink = false;                  // --tag-link
	bool tagApply = false;                 // --tag-apply
	authoring::StartupOptions authoring;   // --authoring-* ([authoring])
	std::string capture;
	int gameLinkSlot = 0;   // --game-link (1-4); 0 = off
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
