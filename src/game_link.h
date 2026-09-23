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
};

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

	void SetPollHz(int hz);   // LinkState polling while connected; 0 = off
	void SetAutoReload(bool on);
	void SetWatchedFiles(std::vector<WatchedFile> files);

	Snapshot Get() const;
	std::vector<std::string> RecentLog() const;   // newest last, capped

	// For the command-line tool: wait (on the caller's thread) until the reply with `seq` arrives.
	bool WaitReply(uint16_t seq, int timeoutMs, wire::Reply& out);
	bool WaitState(int timeoutMs, wire::State& out);   // a state newer than the call

private:
	void run();
	void note(const std::string& line);
	bool tryOpen();
	void closePipe(const char* why);
	void pump();
	void handleMessage(const wire::Header& h, const uint8_t* body);
	void watchTick();
	uint16_t queueCommand(wire::Command c);

	mutable std::mutex m_mx;
	std::deque<wire::Command> m_out;
	Snapshot m_snap;
	std::vector<std::string> m_log;
	std::vector<WatchedFile> m_watch;
	std::vector<wire::Reply> m_replies;   // recent, for WaitReply
	uint32_t m_stateSerial = 0;
	uint16_t m_seq = 0;
	int m_pollHz = 10;

	std::atomic<bool> m_quit{false};
	std::thread m_thread;

	// worker-only
	void* m_pipe = nullptr;   // HANDLE
	std::vector<uint8_t> m_rx;
	uint64_t m_lastPollMs = 0, m_lastStateMs = 0, m_nextOpenMs = 0, m_lastWatchMs = 0;
	struct FileStamp { std::string path, key; uint64_t time = 0; uint64_t size = 0; uint64_t changedMs = 0; bool pending = false; };
	std::vector<FileStamp> m_stamps;
};

} // namespace gamelink

#endif
