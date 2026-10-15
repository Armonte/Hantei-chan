#ifndef STARTUP_ARGS_H_GUARD
#define STARTUP_ARGS_H_GUARD

// Command-line startup actions (parsed in main.cpp, run by MainFrame):
//   --open <file>        open a character: .txt, .ha6, MBAC .DAT (by content) or .hproj
//   --pattern N / --frame N   select pattern / frame in the opened view
//   --palette N          palette number
//   --zoom F             view zoom
//   --game auto|mbaacc|uni2|unist|unib|mbtl   default HA6 dialect for files with no per-file choice (View > Game format)
//   --no-pups            ignore PUPS (draw with <cg>.pal only)
//   --compare N          overlay pattern N of the same character (pattern comparison)
//   --tool NAME          open a tool window: hud, bgm, compare, patterns, notes, vars, keys
//   --cg <file>          load a CG bank into the opened character
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
//   --ui-script <file>   run a scripted UI session: one command per line (# = comment): wait N | waitidle | capture <png> | quit | openany <path> |
//                        pattern N | frame N | viewframe N | zoomview N | framenodes MASK (hold open: 1 Attack data, 2 Effects, 4 Conditions) | copypattern N | pastepattern N | gameformat G | layer N | switchview N |
//                        and every abrowser::ScriptCommand (browse, source, filter, type, select, open, key, ...)
//   --quit               quit after the actions even without --capture
// Headless render / export (Hantei_Docs startup args doc, "Headless export"):
//   --headless           hidden window + GL context (never shown, no taskbar entry, no ini written, no detached
//                        OS windows); the view renders into its FBO; exit code 0 = every PNG written
//   --size WxH           the view's render target size (default: the window's client size)
//   --export-png <x.png> a path ending in .png is a single view shot (= --capture-view); otherwise a folder (above)
//   --center X,Y         world point at the view centre   --bg R,G,B (0-255) view clear colour
//   --layer L[,L..]      draw only these AF layers (UNI multi-layer AFGX; MBAACC frames have layer 0)
//   --batch <file>       one shot per line, `;`-separated: open X; pattern N; frame N; tick N; zoom Z;
//                        center X,Y; bg R,G,B; size WxH; layer L; boxes 0|1; spawns 0|1; export out.png
//                        (settings carry over to the next line; `open` of an already open file reuses its view)
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
	std::string openAny;   // --open-any a|b|c: each file goes through File > Open... (header-based routing), for scripted checks
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
	std::string uiScript;   // --ui-script <file>: scripted UI actions (archive browser, captures) without any real mouse / keyboard; see startup_args.cpp
	std::string cg;              // --cg <file>: load this CG bank into the active character (frame 3)
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
	bool hideBoxes = false;     // --hide-boxes: start with the hitbox overlay off (game-vs-editor comparison shots)
	bool hideSpawns = false;    // --hide-spawns: start with spawned-pattern preview off (isolate a pattern's own parts)
	bool exportFitEach = false;
	std::string saveProject;
	std::string stats;           // UTF-8
	int postKey = 0;
	double sceneMs = 0, onionSimMs = 0, onionTotalMs = 0;
	int statFrames = 0, onionSamples = 0;
	bool quit = false;

	// Headless render / export (startup_args.cpp RunBatch)
	bool headless = false;
	int exitCode = 0;
	int sizeW = 0, sizeH = 0;         // --size: view render target size override (0 = client size)
	bool hasCenter = false; float centerX = 0.f, centerY = 0.f;
	bool hasBg = false; float bg[3] = {0, 0, 0};
	unsigned layerMask = ~0u;          // --layer: AF layers drawn (bit per layer index)
	std::string batch;                 // --batch <file> (UTF-8)
};

// Parses one wave-2 option; advances i past its value. False if not ours.
bool ParseWave2StartupArg(const char* arg, const wchar_t* next, int& i);
extern StartupArgs gStartup;

#endif
