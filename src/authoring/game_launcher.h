#ifndef AUTHORING_GAME_LAUNCHER_H_GUARD
#define AUTHORING_GAME_LAUNCHER_H_GUARD
// [authoring] Launch MBAA.exe with PovertyCaster for Authoring Mode, or find a running one (docs/HANTEI_AUTHORING_MODE.md
// §5.2, §3.7, §8.9 / §8.10):
//   1. snapshot the running MBAA.exe pids (+ image paths);
//   2. CreateProcessW <gamedir>\pc_inject.exe MBAA.exe pchost.dll, cwd <gamedir>, the parent environment + the §3.7
//      variables; its stdout / stderr go to the launcher log;
//   3. the new pid = the MBAA.exe whose image is <gamedir>\MBAA.exe and which was not in the snapshot (<= 15 s);
//   4. watch the pid: when it exits, report the exit code and the last lines of <gamedir>\pchost_authoring.log so
//      the UI can offer "relaunch with the same setup".
// Everything runs on a worker thread; the UI polls Get(). Only the pid this launcher started is ever terminated.
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace authoring {

struct GameDirCheck {
	bool exe = false, pchost = false, inject = false, looseData = false, packed = false, tagDir = false, legacyIni = false;
	std::vector<std::string> problems;   // blocking: launch disabled
	std::vector<std::string> notes;      // warnings
	bool CanLaunch() const { return problems.empty(); }
};
GameDirCheck CheckGameDir(const std::string& gameDir);

struct RunningGame { uint32_t pid = 0; std::string image; std::string dir; };
std::vector<RunningGame> ListRunningGames();

enum class LaunchPhase : uint8_t { Idle, Starting, WaitingForGame, Running, Exited, Failed };
const char* LaunchPhaseName(LaunchPhase p);

struct LaunchState {
	LaunchPhase phase = LaunchPhase::Idle;
	uint32_t injectPid = 0;
	uint32_t gamePid = 0;
	uint32_t exitCode = 0;
	bool crashed = false;            // exit code looks like an exception (>= 0xC0000000) or non-zero
	std::string message;
	std::string gameDir;
	std::string setupHex;            // what it was launched with (relaunch)
	std::vector<std::string> lastLog;   // the last pchost_authoring.log lines at exit
	uint64_t startedMs = 0, exitedMs = 0;
};

class GameLauncher {
public:
	GameLauncher();
	~GameLauncher();
	// vars = LaunchEnv(setup). False when a launch is already running or the dir fails the checks.
	bool Launch(const std::string& gameDir, const std::vector<std::pair<std::string, std::string>>& vars, std::string* why);
	// Watch a game this launcher did not start (Attach): exit reporting only, never terminated.
	void Watch(uint32_t pid, const std::string& gameDir);
	// Terminate the game this launcher started (never another pid). False if there is none.
	bool KillOwn();
	void Forget();                    // back to Idle (after the UI handled an exit)
	LaunchState Get() const;
	std::vector<std::string> Log() const;   // pc_inject stdout + launcher notes (capped)
	bool Owns(uint32_t pid) const;

private:
	void worker(std::string gameDir, std::vector<std::pair<std::string, std::string>> vars);
	void watch(uint32_t pid, std::string gameDir);
	void note(const std::string& s);
	mutable std::mutex m_mx;
	LaunchState m_st;
	std::vector<std::string> m_log;
	std::thread m_thread, m_watch;
	std::atomic<bool> m_quit{false};
	uint32_t m_ownPid = 0;
	bool m_killed = false;   // KillOwn: the exit is not a crash
};

// The last `n` lines of a text file that another process is writing (shared read). Empty when unreadable.
std::vector<std::string> TailFile(const std::string& path, size_t n, uint64_t* sizeOut = nullptr);

} // namespace authoring

#endif
