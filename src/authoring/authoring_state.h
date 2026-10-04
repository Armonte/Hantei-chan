#ifndef AUTHORING_STATE_H_GUARD
#define AUTHORING_STATE_H_GUARD
// [authoring] Internal state shared by the Authoring window's translation units (authoring_window.cpp: header, Setup,
// Setups, the edit sink; authoring_tuning_ui.cpp: Tuning; authoring_live_ui.cpp: Live + Log). UI thread only.
#include "authoring_model.h"
#include "authoring_policy.h"
#include "authoring_window.h"
#include "edit_sink.h"
#include "game_launcher.h"
#include "../game_link.h"
#include "../tag_tuning/tag_history.h"
#include "../tag_tuning/tag_sidecar.h"

#include <imgui.h>

#include <cstdint>
#include <string>
#include <vector>

namespace authoring {

inline const ImVec4 kColWarn(1.0f, 0.62f, 0.25f, 1.0f), kColOk(0.45f, 0.95f, 0.45f, 1.0f), kColOver(0.55f, 0.8f, 1.0f, 1.0f),
                    kColBad(1.0f, 0.4f, 0.4f, 1.0f), kColExp(0.95f, 0.35f, 0.05f, 1.0f), kColDim(0.6f, 0.6f, 0.6f, 1.0f),
                    kColMoon(0.95f, 0.8f, 0.35f, 1.0f), kColMbac(0.75f, 0.6f, 1.0f, 1.0f);

struct Checkpoint { std::string label, time; tagtune::SidecarWorkspace::Snapshot snap; };
struct AbSlot { bool set = false; std::string label, time; tagtune::SidecarWorkspace::Snapshot snap; };

class WindowSink;

struct AuthoringState {
	bool loaded = false;               // settings read
	// ---- settings (hanteichan_authoring.ini) ----
	std::string gameDir;               // the game folder
	std::string tagRootOverride;       // a sidecar tree elsewhere (tests / captures); "" = <gameDir>\povertycaster\tag
	std::string mbacDir;               // MBAC 02_extracted (reference values)
	bool openChars = true, reReadFirst = true, liveApply = true, editShipped = false;
	// ---- the sidecar tree ----
	tagtune::SidecarWorkspace ws;
	std::string wsRoot;
	tagtune::TagHistory history;
	std::vector<Checkpoint> checkpoints;
	AbSlot ab[2];
	int abActive = -1;
	uint64_t lastPollMs = 0;
	std::string status;                // one line: the last save / apply / action result
	std::vector<tagtune::Warning> blocked;
	uint16_t applySeq = 0;
	std::string lastApply;             // the game's answer to the last ApplyTuning
	// ---- the setup ----
	Setup setup;
	SetupLibrary lib;
	char saveName[64] = "";
	uint16_t loadSeq = 0;
	uint64_t loadSentMs = 0;
	Setup loadRequested;
	std::string loadResult;
	bool openAfterReady = false;       // a launch: open the characters when the setup reaches Ready
	bool wantStagePick = false;
	std::vector<std::string> charWarnings;   // "data\shiki_1.txt missing"
	// ---- the game ----
	GameLauncher launcher;
	bool crashDismissed = false;
	bool unlocked = false;             // session lock: files unlocked (saves only)
	uint32_t attachConfirmPid = 0;
	// ---- follow ----
	bool follow = false;
	int followTeam = 0;
	bool followSwitchTab = true;
	int lastFollowSlot = -1, lastFollowPattern = -1, lastFollowFrame = -1;
	// ---- UI ----
	int tab = 0;                       // 0 Setup, 1 Setups, 2 Tuning, 3 HUD, 4 Live, 5 Log
	int requestTab = -1;
	std::string tuningView;            // "global" or "<file>:<moon>" (the character tab)
	int forceViewFrames = 0;           // > 0: select tuningView (startup / Other...)
	int layer = -2;                    // -2 = the pick's moon, -1 = all moons ([char] file), 0..2 moon file
	bool focused = false;
	bool maximize = false;
	// ---- the edit gesture ----
	bool gestureOpen = false;
	std::string gestureLabel;
	tagtune::SidecarWorkspace::Snapshot gestureBefore;
	bool gestureDirty = false;
	uint64_t lastGestureSaveMs = 0;
	// ---- log ----
	char logFilter[96] = "";
	bool logTagOnly = true, logProblemsOnly = false, logFollow = true;
	std::vector<std::string> gameLog;
	uint64_t gameLogSize = 0, lastLogMs = 0;
	// ---- the roster in use ----
	std::vector<RosterChar> roster;
	bool rosterFromGame = false;
};

AuthoringState& St();
gamelink::Client& Link();
EditSink& Sink();
LinkPolicy Policy(const gamelink::Snapshot& s);

std::string TagRoot();                       // the sidecar tree in use
void EnsureWorkspace();                      // open / reopen the tree when the root changed
void SaveAndApply(const std::string& why);   // save the dirty documents, then ApplyTuning (+ QueryAfter)
void TakeCheckpoint(const std::string& label);
std::string NowText();
std::string MoonText(int m);                 // "Crescent"
std::string CharLabel(const std::string& file);   // "Shiki (shiki)" from the roster
const RosterChar* RosterFor(int chara);
int PaletteCount(const std::string& file1);  // from <gameDir>\data\<file1>.pal (cached)
std::string DataTxt(const std::string& file, int moon);        // <gameDir>\data\<file>_<moon>.txt
std::string CommandsTxt(const std::string& file, int moon);    // <gameDir>\data\<file>_<moon>_c.txt

// the tabs
void DrawTuningTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p);
void DrawLiveTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p);
void DrawLogTab(const gamelink::Snapshot& s);
void FollowTick(HostContext& host, const gamelink::Snapshot& s);
// A compact frame-data strip + text for a pattern of an open character.
void DrawMoveInline(HostContext& host, const std::string& txtPath, int pattern, const char* label);

} // namespace authoring

#endif
