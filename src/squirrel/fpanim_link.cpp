#include "fpanim_link.h"

#include "../app_log.h"

#include <cctype>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sqedit {

std::string FpanimModsDir(const std::string& pingReply)
{
	const size_t at = pingReply.find("mods=");
	if (at == std::string::npos) return "";
	std::string s = pingReply.substr(at + 5);
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\0')) s.pop_back();
	return s;
}

std::string FpanimReloadRequest(const std::string& chr, bool live)
{
	return "reloadchara " + chr + (live ? " live" : "");
}

bool FpanimIsSharedFile(const std::string& rel)
{
	std::string r = rel;
	for (char& c : r) c = c == '\\' ? '/' : static_cast<char>(tolower(static_cast<unsigned char>(c)));
	if (r.rfind("./", 0) == 0) r.erase(0, 2);
	auto endsTxt = [&](size_t from) { return r.size() > from + 4 && r.compare(r.size() - 4, 4, ".txt") == 0 && r.find('/', from) == std::string::npos; };
	if (r.rfind("script/", 0) == 0) return endsTxt(7);
	for (const char* f : {"data/_combase.txt", "data/vectortable.txt", "data/battleinfo.txt", "data/boundeff.txt"})
		if (r == f) return true;
	return false;
}

std::string FpanimStdReloadRequest(bool live)
{
	return live ? "reloadstd live" : "reloadstd";
}

std::string FpanimPipeForGame(const std::string& game)
{
	return game == "mbtl" ? "fpan-im-mbtl" : "fpan-im-uni2";
}

std::string FpanimLegacyPipeName(const std::string& pipeName)
{
	const std::string prefix = "\\\\.\\pipe\\";
	const bool full = pipeName.rfind(prefix, 0) == 0;
	const std::string bare = full ? pipeName.substr(prefix.size()) : pipeName;
	if (bare.rfind("fpan-im-", 0) != 0) return "";
	return (full ? prefix : "") + "fbmod-" + bare.substr(8);
}

std::string FpanimModsTarget(const std::string& modsDir, const std::string& rel)
{
	std::string r = rel;
	for (char& c : r) if (c == '\\') c = '/';
	while (r.rfind("./", 0) == 0) r.erase(0, 2);
	if (r.empty() || r[0] == '/' || (r.size() > 1 && r[1] == ':')) return "";
	if (r == ".." || r.rfind("../", 0) == 0 || r.find("/../") != std::string::npos) return "";
	std::string m = modsDir;
	for (char& c : m) if (c == '\\') c = '/';
	if (!m.empty() && m.back() != '/') m.push_back('/');
	return m + r;
}

FpanimReloadInfo FpanimParseReloadReply(const std::string& reply)
{
	FpanimReloadInfo info;
	const bool err = reply.rfind("ERROR", 0) == 0;
	info.stdReload = reply.find("reloadstd") != std::string::npos;
	info.engine = reply.find("engine rebuild ") != std::string::npos || reply.find("engine rebuild:") != std::string::npos;
	if (err) {
		for (const char* k : {"refused in session", "not in a battle", "no battle frame within"})
			if (reply.find(k) != std::string::npos) { info.refused = true; break; }
	}
	const size_t r = reply.find("ha6 root ");
	if (r != std::string::npos) {
		unsigned a = 0, b = 0;
		if (sscanf(reply.c_str() + r + 9, "%8X -> %8X", &a, &b) == 2) {
			info.hasRoots = true;
			info.rootBefore = a;
			info.rootAfter = b;
		}
	}
	const size_t back = reply.find("back in battle after ");
	if (back != std::string::npos) {
		info.backInBattle = !(back >= 4 && reply.compare(back - 4, 4, "NOT ") == 0);
		unsigned long ms = 0;
		if (sscanf(reply.c_str() + back + 21, "%lu ms", &ms) == 1) info.ms = static_cast<long>(ms);
	}
	return info;
}

#ifdef _WIN32

namespace {

std::wstring Widen(const std::string& s)
{
	if (s.empty()) return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

std::string LastError(const char* what)
{
	return std::string(what) + " failed (error " + std::to_string(GetLastError()) + ")";
}

// Overlapped read/write with a deadline, so a hung server never wedges the worker forever.
bool IoWait(HANDLE h, OVERLAPPED& ov, BOOL started, DWORD timeoutMs, DWORD* got)
{
	if (!started && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_MORE_DATA) return false;
	if (WaitForSingleObject(ov.hEvent, timeoutMs) != WAIT_OBJECT_0) {
		CancelIo(h);
		GetOverlappedResult(h, &ov, got, TRUE);
		SetLastError(WAIT_TIMEOUT);
		return false;
	}
	if (GetOverlappedResult(h, &ov, got, FALSE)) return true;
	return GetLastError() == ERROR_MORE_DATA; // message longer than the buffer: keep what fits
}

} // namespace

static FpanimReply RequestExact(const std::string& pipeName, const std::string& request, unsigned timeoutMs)
{
	FpanimReply r;
	const std::string full = pipeName.rfind("\\\\.\\pipe\\", 0) == 0 ? pipeName : "\\\\.\\pipe\\" + pipeName;
	const std::wstring wname = Widen(full);
	HANDLE h = INVALID_HANDLE_VALUE;
	for (int attempt = 0; attempt < 2; attempt++) {
		h = CreateFileW(wname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (h != INVALID_HANDLE_VALUE) break;
		const DWORD e = GetLastError();
		if (e != ERROR_PIPE_BUSY || !WaitNamedPipeW(wname.c_str(), 2000)) {
			if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) { r.text = "no pipe " + full; return r; }
			r.text = e == ERROR_PIPE_BUSY ? "pipe busy: " + full : "cannot open " + full + " (error " + std::to_string(e) + ")";
			r.connected = e == ERROR_PIPE_BUSY;
			return r;
		}
	}
	if (h == INVALID_HANDLE_VALUE) { r.text = "pipe busy: " + full; r.connected = true; return r; }
	r.connected = true;
	DWORD mode = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState(h, &mode, nullptr, nullptr);

	OVERLAPPED ov{};
	ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	DWORD n = 0;
	if (!IoWait(h, ov, WriteFile(h, request.data(), (DWORD)request.size(), nullptr, &ov), 2000, &n)) {
		r.text = LastError("pipe write");
	} else {
		char buf[4097];
		ResetEvent(ov.hEvent);
		n = 0;
		if (!IoWait(h, ov, ReadFile(h, buf, 4096, nullptr, &ov), timeoutMs, &n))
			r.text = GetLastError() == WAIT_TIMEOUT ? "no reply from the game within " + std::to_string(timeoutMs / 1000) + " s" : LastError("pipe read");
		else {
			r.text.assign(buf, n);
			while (!r.text.empty() && (r.text.back() == '\n' || r.text.back() == '\r' || r.text.back() == '\0')) r.text.pop_back();
			r.ok = r.text.rfind("OK", 0) == 0;
		}
	}
	CloseHandle(ov.hEvent);
	CloseHandle(h);
	HC_LOG_INFO("[fpan-im] {} '{}' -> {}", full, request, r.text);
	return r;
}

FpanimReply FpanimRequest(const std::string& pipeName, const std::string& request, unsigned timeoutMs)
{
	FpanimReply r = RequestExact(pipeName, request, timeoutMs);
	if (r.connected) return r;
	// An fbmod-era build (the project was fbmod until 2026-10-16) only serves the old pipe names: retry once there.
	const std::string legacy = FpanimLegacyPipeName(pipeName);
	return legacy.empty() ? r : RequestExact(legacy, request, timeoutMs);
}

#else

FpanimReply FpanimRequest(const std::string&, const std::string&, unsigned)
{
	FpanimReply r;
	r.text = "fpan-im pipe: not supported on this platform";
	return r;
}

#endif

} // namespace sqedit
