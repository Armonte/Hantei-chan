#include "fbmod_link.h"

#include "../app_log.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sqedit {

std::string FbmodModsDir(const std::string& pingReply)
{
	const size_t at = pingReply.find("mods=");
	if (at == std::string::npos) return "";
	std::string s = pingReply.substr(at + 5);
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\0')) s.pop_back();
	return s;
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

FbmodReply FbmodRequest(const std::string& pipeName, const std::string& request, unsigned timeoutMs)
{
	FbmodReply r;
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
	HC_LOG_INFO("[fbmod] {} '{}' -> {}", full, request, r.text);
	return r;
}

#else

FbmodReply FbmodRequest(const std::string&, const std::string&, unsigned)
{
	FbmodReply r;
	r.text = "fbmod pipe: not supported on this platform";
	return r;
}

#endif

} // namespace sqedit
