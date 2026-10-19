#include "process.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <tlhelp32.h>
#else
#include <csignal>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace portable {

#ifdef _WIN32

namespace {

// The process handle and the stdout pipe read end travel together through the
// single void* Process::pipeRead exposes (the header keeps the type opaque).
struct WinHandles {
	HANDLE process = nullptr;
	HANDLE rd = nullptr;
};

// QueryPerformanceCounter is the only Win32 source with sub-millisecond
// resolution; the frequency is fixed per boot.
uint64_t PerfUs()
{
	static const uint64_t freq = [] {
		LARGE_INTEGER f{};
		QueryPerformanceFrequency(&f);
		return f.QuadPart > 0 ? (uint64_t)f.QuadPart : 1ull;
	}();
	LARGE_INTEGER c{};
	QueryPerformanceCounter(&c);
	return freq ? (uint64_t)c.QuadPart * 1000000ull / freq : 0;
}

std::string ImageOf(uint32_t pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return {};
	wchar_t buf[MAX_PATH * 2];
	DWORD n = MAX_PATH * 2;
	std::string out;
	if (QueryFullProcessImageNameW(h, 0, buf, &n)) {
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, nullptr, 0, nullptr, nullptr);
		out.resize(bytes > 0 ? bytes : 0);
		if (bytes > 0) WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, out.data(), bytes, nullptr, nullptr);
	}
	CloseHandle(h);
	return out;
}

}  // namespace

uint64_t NowUs() { return PerfUs(); }
uint64_t NowMs() { return GetTickCount64(); }

std::string LocalTimeString()
{
	SYSTEMTIME st{};
	GetLocalTime(&st);
	char buf[32];
	std::snprintf(buf, sizeof buf, "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)st.wYear, (unsigned)st.wMonth,
	              (unsigned)st.wDay, (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
	return buf;
}

void SleepMs(unsigned ms) { Sleep(ms); }

uint32_t CurrentPid() { return (uint32_t)GetCurrentProcessId(); }

std::vector<ProcessEntry> FindProcessesByName(const std::string& exeName)
{
	std::vector<ProcessEntry> out;
	if (exeName.empty()) return out;
	std::string want = exeName;
	for (char& c : want) c = (char)std::tolower((unsigned char)c);
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return out;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof pe;
	for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
		// Process32FirstW/NextW give the base name; compare case-insensitively,
		// the same way the _wcsicmp in game_launcher.cpp did.
		const int n = (int)wcslen(pe.szExeFile);
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, n, nullptr, 0, nullptr, nullptr);
		std::string name(bytes > 0 ? bytes : 0, '\0');
		if (bytes > 0) WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, n, name.data(), bytes, nullptr, nullptr);
		std::string lower = name;
		for (char& c : lower) c = (char)std::tolower((unsigned char)c);
		if (lower != want) continue;
		ProcessEntry e;
		e.pid = (uint32_t)pe.th32ProcessID;
		e.exe = name;
		e.image = ImageOf(e.pid);
		out.push_back(e);
	}
	CloseHandle(snap);
	return out;
}

bool ProcessAlive(uint32_t pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return false;
	DWORD code = 0;
	const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
	CloseHandle(h);
	return alive;
}

bool KillProcess(uint32_t pid)
{
	HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!h) return false;
	const BOOL ok = TerminateProcess(h, 1);
	CloseHandle(h);
	return ok != 0;
}

namespace {

std::wstring Widen(const std::string& s)
{
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

// A NUL-separated, NUL-terminated "KEY=VALUE" block plus a copy of the parent
// environment, which is what CreateProcessW wants with
// CREATE_UNICODE_ENVIRONMENT (game_launcher.cpp's BuildEnvBlock).
std::vector<wchar_t> BuildEnvBlock(const std::vector<std::pair<std::string, std::string>>& vars)
{
	std::vector<wchar_t> block;
	wchar_t* parent = GetEnvironmentStringsW();
	for (const wchar_t* p = parent; p && *p; p += std::wcslen(p) + 1) {
		std::wstring entry(p);
		const size_t eq = entry.find(L'=');
		bool override_ = false;
		for (const auto& kv : vars) {
			if (eq == std::wstring::npos) break;
			if (entry.compare(0, eq, Widen(kv.first)) == 0) { override_ = true; break; }
		}
		if (override_) continue;
		block.insert(block.end(), entry.begin(), entry.end());
		block.push_back(L'\0');
	}
	FreeEnvironmentStringsW(parent);
	for (const auto& kv : vars) {
		const std::wstring e = Widen(kv.first + "=" + kv.second);
		block.insert(block.end(), e.begin(), e.end());
		block.push_back(L'\0');
	}
	block.push_back(L'\0');
	return block;
}

}  // namespace

bool Spawn(const SpawnDesc& desc, Process& out, std::string* error)
{
	out = Process();
	if (desc.exe.empty()) {
		if (error) *error = "no program to start";
		return false;
	}
	SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE };
	HANDLE rd = nullptr, wr = nullptr;
	if (desc.captureStdout) {
		if (!CreatePipe(&rd, &wr, &sa, 0)) {
			if (error) *error = "CreatePipe failed (" + std::to_string(GetLastError()) + ")";
			return false;
		}
		// The read end must not be inherited by the child, or the pipe never
		// reports EOF when the child exits.
		SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
	}
	STARTUPINFOW si{};
	si.cb = sizeof si;
	if (desc.captureStdout) {
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = wr;
		si.hStdError = wr;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	}
	PROCESS_INFORMATION pi{};
	const std::wstring exe = Widen(desc.exe);
	std::wstring cmd;
	if (desc.args.empty()) {
		cmd = L"\"" + exe + L"\"";
	} else {
		for (size_t i = 0; i < desc.args.size(); ++i) {
			if (i) cmd += L' ';
			cmd += L"\"" + Widen(desc.args[i]) + L"\"";
		}
	}
	std::vector<wchar_t> env = BuildEnvBlock(desc.env);
	const std::wstring dir = desc.workingDir.empty() ? std::wstring() : Widen(desc.workingDir);
	DWORD flags = CREATE_UNICODE_ENVIRONMENT | (desc.hidden ? CREATE_NO_WINDOW : 0);
	std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
	cmdBuf.push_back(L'\0');
	const BOOL ok = CreateProcessW(exe.c_str(), cmdBuf.data(), nullptr, nullptr, TRUE, flags, env.data(),
	                               dir.empty() ? nullptr : dir.c_str(), &si, &pi);
	if (wr) CloseHandle(wr);
	if (!ok) {
		if (rd) CloseHandle(rd);
		if (error) *error = "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
		return false;
	}
	CloseHandle(pi.hThread);
	out.pid = (uint32_t)pi.dwProcessId;
	out.pipeRead = new WinHandles{ pi.hProcess, rd };   // caller closes with CloseProcess
	return true;
}

bool WaitForExit(const Process& p, unsigned timeoutMs, int* exitCode)
{
	if (!p.pipeRead) return false;
	WinHandles* h = (WinHandles*)p.pipeRead;
	const DWORD wait = WaitForSingleObject(h->process, timeoutMs ? timeoutMs : INFINITE);
	if (wait != WAIT_OBJECT_0) return false;
	DWORD code = 0;
	GetExitCodeProcess(h->process, &code);
	if (exitCode) *exitCode = (int)code;
	return true;
}

std::string ReadAvailable(const Process& p)
{
	if (!p.pipeRead) return std::string();
	WinHandles* h = (WinHandles*)p.pipeRead;
	std::string out;
	for (;;) {
		DWORD avail = 0;
		if (!PeekNamedPipe(h->rd, nullptr, 0, nullptr, &avail, nullptr) || !avail) break;
		char buf[4096];
		DWORD rd = 0;
		if (!ReadFile(h->rd, buf, avail < sizeof buf ? avail : (DWORD)sizeof buf, &rd, nullptr) || !rd) break;
		out.append(buf, rd);
	}
	return out;
}

void CloseProcess(Process& p)
{
	WinHandles* h = (WinHandles*)p.pipeRead;
	if (h) {
		if (h->process) CloseHandle(h->process);
		if (h->rd) CloseHandle(h->rd);
		delete h;
	}
	p = Process();
}

#else  /* POSIX */

namespace {

uint64_t ClockUs(clockid_t id)
{
	struct timespec ts{};
	clock_gettime(id, &ts);
	return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

std::string Lower(std::string s)
{
	for (char& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

}  // namespace

uint64_t NowUs() { return ClockUs(CLOCK_MONOTONIC); }
uint64_t NowMs() { return NowUs() / 1000ull; }

std::string LocalTimeString()
{
	const std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_r(&t, &tm);
	char buf[32];
	std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
	return buf;
}

void SleepMs(unsigned ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

uint32_t CurrentPid() { return (uint32_t)getpid(); }

std::vector<ProcessEntry> FindProcessesByName(const std::string& exeName)
{
	std::vector<ProcessEntry> out;
	if (exeName.empty()) return out;
	const std::string want = Lower(exeName);
	std::error_code ec;
	for (const fs::directory_entry& e : fs::directory_iterator("/proc", fs::directory_options::skip_permission_denied, ec)) {
		const std::string name = e.path().filename().string();
		if (name.empty() || !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) continue;
		std::error_code e2;
		// /proc/<pid>/exe is the running image (it follows replace-on-update).
		const fs::path exe = fs::read_symlink(e.path() / "exe", e2);
		const std::string base = e2 ? std::string() : exe.filename().string();
		if (Lower(base) != want) continue;
		ProcessEntry pe;
		pe.pid = (uint32_t)std::strtoul(name.c_str(), nullptr, 10);
		pe.exe = base;
		pe.image = exe.string();
		out.push_back(pe);
	}
	return out;
}

bool ProcessAlive(uint32_t pid)
{
	if (pid == 0) return false;
	if (kill((pid_t)pid, 0) == 0) return true;
	return errno == EPERM;   // exists, owned by another user
}

bool KillProcess(uint32_t pid)
{
	return kill((pid_t)pid, SIGTERM) == 0;
}

bool Spawn(const SpawnDesc& desc, Process& out, std::string* error)
{
	out = Process();
	if (desc.exe.empty()) {
		if (error) *error = "no program to start";
		return false;
	}
	int fds[2] = { -1, -1 };
	if (desc.captureStdout && pipe(fds) != 0) {
		if (error) *error = std::string("pipe failed: ") + std::strerror(errno);
		return false;
	}
	// The environment is flattened before the fork: between fork and exec only
	// async-signal-safe calls are allowed, and building a vector is not one.
	std::vector<std::string> envStorage;
	std::vector<char*> env;
	for (char** e = environ; e && *e; ++e) envStorage.push_back(*e);
	for (const auto& kv : desc.env) {
		const std::string entry = kv.first + "=" + kv.second;
		bool replaced = false;
		for (std::string& s : envStorage) {
			if (s.compare(0, kv.first.size(), kv.first) == 0 && s.size() > kv.first.size() && s[kv.first.size()] == '=') {
				s = entry;
				replaced = true;
				break;
			}
		}
		if (!replaced) envStorage.push_back(entry);
	}
	for (std::string& s : envStorage) env.push_back(s.data());
	env.push_back(nullptr);
	std::vector<char*> argv;
	argv.push_back((char*)(desc.args.empty() ? desc.exe.c_str() : desc.args[0].c_str()));
	for (size_t i = desc.args.empty() ? 0 : 1; i < desc.args.size(); ++i) argv.push_back((char*)desc.args[i].c_str());
	argv.push_back(nullptr);

	const pid_t pid = fork();
	if (pid < 0) {
		if (fds[0] >= 0) { close(fds[0]); close(fds[1]); }
		if (error) *error = std::string("fork failed: ") + std::strerror(errno);
		return false;
	}
	if (pid == 0) {
		// Child. Only async-signal-safe calls below this line.
		if (fds[1] >= 0) {
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[0]);
			close(fds[1]);
		} else if (desc.hidden) {
			const int devnull = open("/dev/null", O_RDWR);
			if (devnull >= 0) { dup2(devnull, STDIN_FILENO); dup2(devnull, STDOUT_FILENO); if (devnull > 2) close(devnull); }
		}
		if (!desc.workingDir.empty()) chdir(desc.workingDir.c_str());
		execvp(desc.exe.c_str(), argv.data());
		_exit(127);
	}
	if (fds[0] >= 0) close(fds[1]);
	out.pid = (uint32_t)pid;
	out.pipeRead = fds[0] >= 0 ? new int(fds[0]) : nullptr;
	return true;
}

bool WaitForExit(const Process& p, unsigned timeoutMs, int* exitCode)
{
	if (p.pid == 0) return false;
	const uint64_t deadline = timeoutMs ? NowMs() + timeoutMs : 0;
	for (;;) {
		int status = 0;
		const pid_t r = waitpid((pid_t)p.pid, &status, WNOHANG);
		if (r == (pid_t)p.pid) {
			if (exitCode) *exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
			return true;
		}
		if (r < 0 && errno != EINTR) return false;
		if (timeoutMs && NowMs() >= deadline) return false;
		SleepMs(5);
	}
}

std::string ReadAvailable(const Process& p)
{
	if (!p.pipeRead) return std::string();
	const int fd = *(const int*)p.pipeRead;
	std::string out;
	for (;;) {
		struct pollfd fds{ fd, POLLIN, 0 };
		if (poll(&fds, 1, 0) <= 0 || !(fds.revents & POLLIN)) break;
		char buf[4096];
		const ssize_t n = read(fd, buf, sizeof buf);
		if (n <= 0) break;
		out.append(buf, (size_t)n);
	}
	return out;
}

void CloseProcess(Process& p)
{
	if (p.pipeRead) {
		close(*(int*)p.pipeRead);
		delete (int*)p.pipeRead;
	}
	p = Process();
}

#endif  /* _WIN32 */

}  // namespace portable
