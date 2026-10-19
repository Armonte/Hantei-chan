#ifndef PORTABLE_PROCESS_H_GUARD
#define PORTABLE_PROCESS_H_GUARD

// Portable process launch, discovery and timing.
//
// Lane LINUX deliverable 2. Windows bodies are copied from
// authoring/game_launcher.cpp, fpanim_install.cpp, game_link.cpp and the
// GetTickCount64/Sleep sites; the POSIX bodies use posix_spawn and /proc, so a
// caller changes its include and nothing else. No src/ caller uses these yet
// (docs/render/linux.md section 7 has the P5 switch-over list).

#include <cstdint>
#include <string>
#include <vector>

namespace portable {

// ---- clocks -----------------------------------------------------------------------------------------------------

// Monotonic milliseconds, the meaning GetTickCount64 had. Never goes backwards.
uint64_t NowMs();
// Monotonic microseconds, for the spawn-to-ready timings game_link_cli prints.
uint64_t NowUs();
// Wall-clock "YYYY-MM-DD HH:MM:SS" in local time, what GetLocalTime produced for
// the log lines.
std::string LocalTimeString();
// Sleep that also builds on Windows without windows.h in the header.
void SleepMs(unsigned ms);

// ---- process discovery ------------------------------------------------------------------------------------------

struct ProcessEntry {
	uint32_t pid = 0;
	std::string exe;     // base name, e.g. "MBAA.exe"
	std::string image;   // full path of the image when the OS reports one
};

// Every process whose base name is `exeName` (case-insensitive), like the
// CreateToolhelp32Snapshot + PROCESSENTRY32W loop in game_link.cpp. On Linux the
// name comes from /proc/<pid>/exe and /proc/<pid>/cmdline.
std::vector<ProcessEntry> FindProcessesByName(const std::string& exeName);
// True while pid is a live process; OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)
// in the Windows body, kill(pid, 0) on POSIX.
bool ProcessAlive(uint32_t pid);
// The current process id (GetCurrentProcessId / getpid).
uint32_t CurrentPid();
// Terminate, with SIGTERM then SIGKILL on POSIX (TerminateProcess on Windows).
bool KillProcess(uint32_t pid);

// ---- launch -----------------------------------------------------------------------------------------------------

struct SpawnDesc {
	std::string exe;                      // absolute path of the program
	std::vector<std::string> args;        // argv[0] is exe unless the caller sets args[0]
	std::string workingDir;               // "" = leave the parent's
	std::vector<std::pair<std::string, std::string>> env;   // added/overridden
	bool captureStdout = false;           // give the child a pipe the parent reads
	bool hidden = true;                   // CREATE_NO_WINDOW / no controlling tty
};

struct Process {
	uint32_t pid = 0;
	void* pipeRead = nullptr;             // read end of the child's stdout+stderr when captureStdout
};

// Start a child. Returns false and fills `error` when the OS refused; the child
// not exiting yet is not an error. On POSIX `exe` may be a PATH lookup name.
bool Spawn(const SpawnDesc& desc, Process& out, std::string* error = nullptr);
// Wait for exit, up to timeoutMs (0 = forever). Returns true when the child
// exited and puts its status in exitCode (POSIX: WEXITSTATUS, or 128+signal when
// it was signalled).
bool WaitForExit(const Process& p, unsigned timeoutMs, int* exitCode);
// Non-blocking pipe read; returns the bytes available now ("" at EOF).
std::string ReadAvailable(const Process& p);
void CloseProcess(Process& p);

}  // namespace portable

#endif /* PORTABLE_PROCESS_H_GUARD */
