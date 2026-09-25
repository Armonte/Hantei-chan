#ifndef GAME_LINK_H_GUARD
#define GAME_LINK_H_GUARD
// Game Link — Hantei-chan <-> a running MBAA.exe with PovertyCaster's pchost.dll (PCHOST_MBAACC_LINK=1).
//
// The client connects to \\.\pipe\povertycaster-link-<MBAA pid> (PovertyCaster's pc-ipc dev-link endpoint) and
// speaks the fixed-width protocol in game_link_proto.h: reload the characters, pick a character per slot, and
// read what every slot is playing (pattern / frame / position / TAG state). Protocol + design:
// docs/HANTEI_GAME_LINK.md.
//
// NEVER BLOCKS THE UI: every pipe operation and every file stat runs on one worker thread. The UI thread only
// enqueues commands and copies the latest snapshot out (both under a mutex).
//
// Auto-reload on save: the UI hands the worker the files the open characters were loaded from (HA6s, .pat, .txt,
// _c.txt) each frame; the worker polls their write times and, once a change has settled, sends one Reload for the
// game slots whose data-file name matches (SlotMaskForFile).
//
// Stages (docs/HANTEI_STAGE_LINK.md): SetStage / ReloadStage / QueryStage, and the same auto-reload for stage files —
// a saved bgNN.dat / bgNNInfo.txt / bgNNlight.txt sends ReloadStage when bgNN is the stage the game shows; a saved
// BgList.ini sends ReloadStage with kFlagStageList (StageFileMatch).
//
// TARGET PROCESS: by default the client finds MBAA.exe by name and tries each pid. SetTargetPid() (or the env var
// HANTEI_GAME_LINK_PID, read once at construction) pins it to one pid and disables that discovery — for scripted
// runs next to other game instances.
#include "game_link_proto.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gamelink {

struct WatchedFile {
	std::string path;   // absolute path on disk
	std::string key;    // what it belongs to: the character's .txt stem ("akiha_0"), or the file's own stem
};

enum class StageFileKind : uint8_t { Dat, Info, Light, List };
struct WatchedStageFile {
	std::string path;
	StageFileKind kind = StageFileKind::Dat;
};

struct Snapshot {
	bool wantConnected = false;
	bool connected = false;
	uint32_t pid = 0;
	std::string pipe;
	std::string status;          // one line: what the link is doing / why it is not connected
	bool haveState = false;
	wire::State state{};
	uint32_t stateAgeMs = 0;     // since the last LinkState arrived
	std::string lastReply;       // "op #seq: status — message"
	uint32_t gameReloads = 0;    // the game's own reload counter (from the last reply/state)
	bool autoReload = false;
	size_t watched = 0;
	std::string lastChange;      // last save the watcher saw, and what it did about it
	// stages
	bool haveStage = false;
	bool stageUnsupported = false;   // the DLL answered QueryStage with Unknown (predates the stage ops)
	wire::Stage stage{};
	bool autoReloadStage = false;
	size_t watchedStage = 0;
	uint32_t targetPid = 0;          // 0 = discover MBAA.exe by name
	// [link-tag] QueryTag (game_link_proto.h Tag; PovertyCaster mbaacc/link-tag)
	bool haveTag = false;
	bool tagUnsupported = false;     // the DLL answered QueryTag with Unknown (not implemented yet)
	wire::Tag tag{};
};

// Does saving `path` concern the stage the game shows? Stage files are matched by their stage stem, case-
// insensitively: "bg28.dat", "bg28Info.txt", "bg28light.txt" and MBAC's "bg28_s.dat" all belong to "bg28". A
// BgList.ini concerns every stage (*needsList = true). False when the game shows no stage (loaded < 1).
bool StageFileMatches(const std::string& path, const wire::Stage& st, bool* needsList = nullptr);
// "C:\x\Bg\bg28Info.txt" -> "bg28" (lower case), "" for BgList.ini or an unrelated name.
std::string StageStemOf(const std::string& path);

// The slots (bit i = game slot i) whose loaded data-file name matches this file. `key` is a WatchedFile::key or a
// path; it is reduced to a lower-case stem without directory or extension, and matched against each live slot's
// file name as "equal, or <slot>_<anything>". The LONGEST matching slot name wins, so "kohaku_m..." does not also
// hit a "kohaku" slot. 0 = no live slot uses it.
uint8_t SlotMaskForFile(const std::string& key, const wire::State& st);

// Every running MBAA.exe (any path). The link tries each until one serves the pipe.
std::vector<uint32_t> FindGamePids();

class Client {
public:
	Client();
	~Client();
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	void Connect();
	void Disconnect();

	// Each returns the sequence number the reply will echo.
	uint16_t Ping();
	uint16_t Reload(uint8_t slotMask, uint8_t flags = 0);
	uint16_t SetChar(int slot, int chara, int moon, int palette, uint8_t flags);
	uint16_t SetStage(int stageId, uint8_t flags = 0);
	uint16_t ReloadStage(uint8_t flags = 0);
	// Like SetStage, but if the link is not up yet it connects and sends the command as soon as it is (a stage
	// browser's "show in game" should not have to know the link state). Dropped by Disconnect().
	uint16_t SetStageWhenConnected(int stageId, uint8_t flags = 0);

	void SetTargetPid(uint32_t pid);   // 0 = discover by process name (default)

	// [tag-panel] Poll QueryTag with the state (stops by itself once the DLL answers Unknown).
	void SetTagQuery(bool on);
	// [tag-panel] Ask the reload gate without reloading: SetChar(slot 0, keep chara/moon/palette, no reload flag). The
	// DLL checks its arguments, then the gate (session first), and answers RefusedSession / RefusedRecording /
	// RefusedScene / ... or Ok ("held until the next reload": a keep-everything pick, a no-op). Returns the seq.
	uint16_t ProbeGate();
	// The reply with this seq, if it has arrived (non-blocking; recent replies only).
	bool PeekReply(uint16_t seq, wire::Reply& out) const;

	void SetPollHz(int hz);   // LinkState polling while connected; 0 = off
	void SetAutoReload(bool on);
	void SetWatchedFiles(std::vector<WatchedFile> files);
	void SetAutoReloadStage(bool on);
	void SetWatchedStageFiles(std::vector<WatchedStageFile> files);

	Snapshot Get() const;
	std::vector<std::string> RecentLog() const;   // newest last, capped

	// For the command-line tool: wait (on the caller's thread) until the reply with `seq` arrives.
	bool WaitReply(uint16_t seq, int timeoutMs, wire::Reply& out);
	bool WaitState(int timeoutMs, wire::State& out);   // a state newer than the call
	bool WaitStage(int timeoutMs, wire::Stage& out);   // a stage snapshot newer than the call

private:
	void run();
	void note(const std::string& line);
	bool tryOpen();
	void closePipe(const char* why);
	void pump();
	void handleMessage(const wire::Header& h, const uint8_t* body);
	void watchTick();
	void stageWatchTick(uint64_t now);
	uint16_t queueCommand(wire::Command c);

	mutable std::mutex m_mx;
	std::deque<wire::Command> m_out;
	std::deque<wire::Command> m_onConnect;   // SetStageWhenConnected, flushed by tryOpen
	std::vector<WatchedStageFile> m_stageWatch;
	uint32_t m_stageSerial = 0;
	Snapshot m_snap;
	std::vector<std::string> m_log;
	std::vector<WatchedFile> m_watch;
	std::vector<wire::Reply> m_replies;   // recent, for WaitReply
	uint32_t m_stateSerial = 0;
	uint16_t m_seq = 0;
	int m_pollHz = 10;
	bool m_tagQuery = false;

	std::atomic<bool> m_quit{false};
	std::thread m_thread;

	// worker-only
	void* m_pipe = nullptr;   // HANDLE
	std::vector<uint8_t> m_rx;
	uint64_t m_lastPollMs = 0, m_lastStateMs = 0, m_nextOpenMs = 0, m_lastWatchMs = 0, m_lastStageWatchMs = 0;
	struct FileStamp { std::string path, key; uint64_t time = 0; uint64_t size = 0; uint64_t changedMs = 0; bool pending = false; };
	std::vector<FileStamp> m_stamps;
	struct StageStamp { std::string path; StageFileKind kind; uint64_t time = 0, size = 0, changedMs = 0; bool pending = false; };
	std::vector<StageStamp> m_stageStamps;
};

} // namespace gamelink

#endif
