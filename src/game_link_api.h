#ifndef GAME_LINK_API_H_GUARD
#define GAME_LINK_API_H_GUARD
// A small, stable Game Link API for OTHER editor windows (the stage browser on feat/stage, and anything else that
// wants to drive the running game without owning the Game Link window). It talks to the same shared client as the
// Game Link window (game_link_panel.cpp SharedClient), so there is still one pipe per editor.
//
// Offline only: the game refuses every stage op in netplay / replay / recording (docs/HANTEI_STAGE_LINK.md); the
// refusal comes back as the reply and shows up in the Game Link window's log.
#include <cstdint>
#include <string>

// Switch the running game to stage `stageId` (the BgList.ini Bg_NNN index, 1..99) and restart that stage's music.
// Connects first if the link is not up (the switch is sent as soon as it is). Returns the command's sequence number
// (the reply echoes it), 0 if stageId is out of range.
uint16_t GameLink_ShowStageInGame(int stageId);

// Re-read the stage the game shows from disk (bgNN.dat + bgNNInfo.txt; BgList.ini too when rereadList). 0 when the
// link is not connected.
uint16_t GameLink_ReloadStageInGame(bool rereadList = false);

struct GameLinkStageInfo {
	bool connected = false;
	bool haveStage = false;     // false against a pchost.dll that predates the stage ops
	bool allowed = false;       // a stage op would be accepted right now (offline battle)
	int loaded = -1;            // the stage on screen
	int bgm = -1;
	std::string dataFile;       // "bg28"
	std::string lastReply;      // the link's most recent reply line
};
GameLinkStageInfo GameLink_GetStageInfo();

#endif
