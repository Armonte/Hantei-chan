// [authoring] launcher — see game_launcher.h.
#include "game_launcher.h"
#include "authoring_model.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace authoring {

namespace fs = std::filesystem;

namespace {
uint64_t NowMs() { return GetTickCount64(); }
std::wstring W(const std::string& s) { return fs::u8path(s).wstring(); }
std::string U8(const std::wstring& s) { return fs::path(s).u8string(); }
bool Is(const std::string& p) { std::error_code ec; return fs::exists(fs::u8path(p), ec); }
std::string LowerPath(std::string s)
{
	for (char& c : s) { if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); if (c == '/') c = '\\'; }
	while (!s.empty() && s.back() == '\\') s.pop_back();
	return s;
}
std::string ImageOf(uint32_t pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return {};
	wchar_t buf[MAX_PATH * 2];
	DWORD n = MAX_PATH * 2;
	std::string out;
	if (QueryFullProcessImageNameW(h, 0, buf, &n)) out = U8(std::wstring(buf, n));
	CloseHandle(h);
	return out;
}
} // namespace

const char* LaunchPhaseName(LaunchPhase p)
{
	switch (p) {
	case LaunchPhase::Idle: return "idle";
	case LaunchPhase::Starting: return "starting pc_inject";
	case LaunchPhase::WaitingForGame: return "waiting for MBAA.exe";
	case LaunchPhase::Running: return "running";
	case LaunchPhase::Exited: return "exited";
	case LaunchPhase::Failed: return "failed";
	}
	return "?";
}

GameDirCheck CheckGameDir(const std::string& dir)
{
	GameDirCheck c;
	if (dir.empty()) { c.problems.push_back("no game folder set"); return c; }
	c.exe = Is(dir + "\\MBAA.exe");
	c.pchost = Is(dir + "\\pchost.dll");
	c.inject = Is(dir + "\\pc_inject.exe");
	c.looseData = Is(dir + "\\data\\sion_0.txt") || Is(dir + "\\data\\shiki_0.txt");
	c.packed = Is(dir + "\\0002.p");
	c.tagDir = Is(dir + "\\povertycaster\\tag");
	c.legacyIni = Is(dir + "\\tag_tuning.ini");
	if (!c.exe) c.problems.push_back("MBAA.exe not found in the folder");
	if (!c.pchost) c.problems.push_back("pchost.dll not found (PovertyCaster)");
	if (!c.inject) c.problems.push_back("pc_inject.exe not found (PovertyCaster)");
	if (!c.looseData)
		c.notes.push_back(c.packed ? "0002.p present: live reload needs extracted data (HANTEI_GAME_LINK §5)"
		                           : "no loose data\\ found: live reload needs extracted data");
	if (!c.tagDir) c.notes.push_back("no povertycaster\\tag\\ yet: create it or migrate tag_tuning.ini");
	return c;
}

std::vector<RunningGame> ListRunningGames()
{
	std::vector<RunningGame> out;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return out;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof pe;
	for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
		if (_wcsicmp(pe.szExeFile, L"MBAA.exe") != 0) continue;
		RunningGame g;
		g.pid = pe.th32ProcessID;
		g.image = ImageOf(g.pid);
		g.dir = g.image.empty() ? std::string() : fs::u8path(g.image).parent_path().u8string();
		out.push_back(g);
	}
	CloseHandle(snap);
	return out;
}

std::vector<std::string> TailFile(const std::string& path, size_t n, uint64_t* sizeOut)
{
	std::vector<std::string> lines;
	HANDLE h = CreateFileW(W(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return lines;
	LARGE_INTEGER sz{};
	GetFileSizeEx(h, &sz);
	if (sizeOut) *sizeOut = (uint64_t)sz.QuadPart;
	const int64_t want = std::min<int64_t>(sz.QuadPart, (int64_t)(n * 400 + 4096));
	LARGE_INTEGER pos;
	pos.QuadPart = sz.QuadPart - want;
	SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
	std::string buf((size_t)want, '\0');
	DWORD rd = 0;
	ReadFile(h, buf.data(), (DWORD)want, &rd, nullptr);
	CloseHandle(h);
	buf.resize(rd);
	size_t p = 0;
	if (pos.QuadPart > 0) { const size_t nl = buf.find('\n'); p = nl == std::string::npos ? buf.size() : nl + 1; }
	while (p < buf.size()) {
		size_t nl = buf.find('\n', p);
		if (nl == std::string::npos) nl = buf.size();
		std::string l = buf.substr(p, nl - p);
		if (!l.empty() && l.back() == '\r') l.pop_back();
		lines.push_back(l);
		p = nl + 1;
	}
	if (lines.size() > n) lines.erase(lines.begin(), lines.end() - (ptrdiff_t)n);
	return lines;
}

GameLauncher::GameLauncher() = default;

GameLauncher::~GameLauncher()
{
	m_quit = true;
	if (m_thread.joinable()) m_thread.join();
	if (m_watch.joinable()) m_watch.join();
}

void GameLauncher::note(const std::string& s)
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_log.push_back(s);
	if (m_log.size() > 400) m_log.erase(m_log.begin(), m_log.begin() + 100);
}

LaunchState GameLauncher::Get() const { std::lock_guard<std::mutex> lk(m_mx); return m_st; }
std::vector<std::string> GameLauncher::Log() const { std::lock_guard<std::mutex> lk(m_mx); return m_log; }
bool GameLauncher::Owns(uint32_t pid) const { std::lock_guard<std::mutex> lk(m_mx); return pid && pid == m_ownPid; }

void GameLauncher::Forget()
{
	std::lock_guard<std::mutex> lk(m_mx);
	if (m_st.phase == LaunchPhase::Exited || m_st.phase == LaunchPhase::Failed) { m_st = LaunchState{}; m_ownPid = 0; }
}

bool GameLauncher::Launch(const std::string& gameDir, const std::vector<std::pair<std::string, std::string>>& vars, std::string* why)
{
	{
		std::lock_guard<std::mutex> lk(m_mx);
		if (m_st.phase == LaunchPhase::Starting || m_st.phase == LaunchPhase::WaitingForGame || m_st.phase == LaunchPhase::Running) {
			if (why) *why = "a launch is already running";
			return false;
		}
	}
	const GameDirCheck c = CheckGameDir(gameDir);
	if (!c.CanLaunch()) { if (why) *why = c.problems.front(); return false; }
	if (m_thread.joinable()) m_thread.join();
	if (m_watch.joinable()) m_watch.join();
	{
		std::lock_guard<std::mutex> lk(m_mx);
		m_st = LaunchState{};
		m_st.phase = LaunchPhase::Starting;
		m_st.gameDir = gameDir;
		m_st.startedMs = NowMs();
		for (const auto& kv : vars) if (kv.first == "PCHOST_MBAACC_AUTHORING_SETUP") m_st.setupHex = kv.second;
		m_ownPid = 0;
	}
	m_thread = std::thread([this, gameDir, vars] { worker(gameDir, vars); });
	return true;
}

void GameLauncher::worker(std::string gameDir, std::vector<std::pair<std::string, std::string>> vars)
{
	// 1. who is running already
	std::vector<uint32_t> before;
	for (const RunningGame& g : ListRunningGames()) before.push_back(g.pid);
	// 2. pc_inject with the environment, stdout captured
	SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE };
	HANDLE rd = nullptr, wr = nullptr;
	CreatePipe(&rd, &wr, &sa, 0);
	SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
	STARTUPINFOW si{};
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = wr;
	si.hStdError = wr;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	PROCESS_INFORMATION pi{};
	wchar_t* parent = GetEnvironmentStringsW();
	const std::wstring env = BuildEnvBlock(vars, parent);
	FreeEnvironmentStringsW(parent);
	const std::wstring exe = W(gameDir + "\\pc_inject.exe");
	std::wstring cmd = L"\"" + exe + L"\" MBAA.exe pchost.dll";
	std::string envLine;
	for (const auto& kv : vars) envLine += " " + kv.first + "=" + (kv.second.size() > 24 ? kv.second.substr(0, 24) + "..." : kv.second);
	note("launch: " + gameDir + "\\pc_inject.exe MBAA.exe pchost.dll" + envLine);
	const BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
	                               (LPVOID)env.data(), W(gameDir).c_str(), &si, &pi);
	CloseHandle(wr);
	if (!ok) {
		const DWORD e = GetLastError();
		CloseHandle(rd);
		std::lock_guard<std::mutex> lk(m_mx);
		m_st.phase = LaunchPhase::Failed;
		m_st.message = "CreateProcess pc_inject.exe failed (error " + std::to_string(e) + ")";
		m_log.push_back(m_st.message);
		return;
	}
	{
		std::lock_guard<std::mutex> lk(m_mx);
		m_st.injectPid = pi.dwProcessId;
		m_st.phase = LaunchPhase::WaitingForGame;
		m_st.message = "pc_inject pid " + std::to_string(pi.dwProcessId);
	}
	// 3. read pc_inject's output while looking for the new MBAA.exe of this folder
	const std::string want = LowerPath(gameDir + "\\MBAA.exe");
	const uint64_t deadline = NowMs() + 15000;
	uint32_t game = 0;
	std::string partial;
	bool injectAlive = true;
	while (!m_quit) {
		DWORD avail = 0;
		while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail) {
			char b[1024];
			DWORD got = 0;
			if (!ReadFile(rd, b, std::min<DWORD>(avail, sizeof b), &got, nullptr) || !got) break;
			partial.append(b, got);
			size_t nl;
			while ((nl = partial.find('\n')) != std::string::npos) {
				std::string l = partial.substr(0, nl);
				if (!l.empty() && l.back() == '\r') l.pop_back();
				note("pc_inject: " + l);
				partial.erase(0, nl + 1);
			}
		}
		if (!game) {
			for (const RunningGame& g : ListRunningGames())
				if (std::find(before.begin(), before.end(), g.pid) == before.end() && LowerPath(g.image) == want) { game = g.pid; break; }
			if (game) {
				std::lock_guard<std::mutex> lk(m_mx);
				m_st.gamePid = game;
				m_ownPid = game;
				m_st.phase = LaunchPhase::Running;
				m_st.message = "MBAA.exe pid " + std::to_string(game);
				m_log.push_back("game started: " + m_st.message);
			} else if (NowMs() > deadline) {
				std::lock_guard<std::mutex> lk(m_mx);
				m_st.phase = LaunchPhase::Failed;
				m_st.message = "no new MBAA.exe in " + gameDir + " within 15 s";
				m_log.push_back(m_st.message);
				break;
			}
		}
		if (injectAlive && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
			DWORD code = 0;
			GetExitCodeProcess(pi.hProcess, &code);
			note("pc_inject exited with " + std::to_string(code));
			injectAlive = false;
			if (!game && code != 0) {
				// pc_inject failed before the game appeared; give the game a moment anyway
				Sleep(500);
				bool found = false;
				for (const RunningGame& g : ListRunningGames())
					found = found || (std::find(before.begin(), before.end(), g.pid) == before.end() && LowerPath(g.image) == want);
				if (!found) {
					std::lock_guard<std::mutex> lk(m_mx);
					m_st.phase = LaunchPhase::Failed;
					m_st.message = "pc_inject.exe exited with " + std::to_string(code) + " before MBAA.exe started";
					break;
				}
			}
		}
		if (game && !injectAlive) break;
		Sleep(50);
	}
	CloseHandle(rd);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	if (game) watch(game, gameDir);
}

void GameLauncher::Watch(uint32_t pid, const std::string& gameDir)
{
	if (m_watch.joinable()) { m_quit = true; m_watch.join(); m_quit = false; }
	{
		std::lock_guard<std::mutex> lk(m_mx);
		if (m_st.phase == LaunchPhase::Running && m_st.gamePid == pid) return;
		m_st = LaunchState{};
		m_st.phase = LaunchPhase::Running;
		m_st.gamePid = pid;
		m_st.gameDir = gameDir;
		m_st.message = "attached to MBAA.exe pid " + std::to_string(pid);
		m_st.startedMs = NowMs();
	}
	m_watch = std::thread([this, pid, gameDir] { watch(pid, gameDir); });
}

void GameLauncher::watch(uint32_t pid, std::string gameDir)
{
	HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return;
	while (!m_quit && WaitForSingleObject(h, 100) == WAIT_TIMEOUT) {}
	if (m_quit) { CloseHandle(h); return; }
	DWORD code = 0;
	GetExitCodeProcess(h, &code);
	CloseHandle(h);
	const std::vector<std::string> tail = TailFile(gameDir + "\\pchost_authoring.log", 20);
	std::lock_guard<std::mutex> lk(m_mx);
	if (m_st.gamePid != pid) return;
	m_st.phase = LaunchPhase::Exited;
	m_st.exitCode = code;
	m_st.crashed = code != 0 && !m_killed;
	m_killed = false;
	m_st.exitedMs = NowMs();
	char b[96];
	std::snprintf(b, sizeof b, "MBAA.exe pid %u exited with 0x%08X%s", (unsigned)pid, (unsigned)code,
	              code >= 0xC0000000u ? " (crash)" : code ? " (error)" : "");
	m_st.message = b;
	if (!m_st.crashed && code) m_st.message += " - closed from Hantei-chan";
	m_st.lastLog = tail;
	m_log.push_back(m_st.message);
}

bool GameLauncher::KillOwn()
{
	uint32_t pid;
	{ std::lock_guard<std::mutex> lk(m_mx); pid = m_ownPid; }
	if (!pid) return false;
	HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!h) return false;
	{ std::lock_guard<std::mutex> lk(m_mx); m_killed = true; }
	const BOOL ok = TerminateProcess(h, 1);
	CloseHandle(h);
	note("terminated own MBAA.exe pid " + std::to_string(pid));
	return ok != 0;
}

} // namespace authoring
