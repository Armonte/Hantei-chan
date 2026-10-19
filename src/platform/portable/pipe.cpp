#include "pipe.h"
#include "process.h"   // NowMs / SleepMs, shared by both bodies

#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace portable {
namespace pipe {

namespace {

constexpr const char* kWindowsPrefix = "\\\\.\\pipe\\";

// Strips a leading Windows pipe prefix, so a caller that passes the full name
// ("\\\\.\\pipe\\fpan-im-uni2") and one that passes the bare name ("fpan-im-uni2")
// reach the same endpoint.
std::string BareName(const std::string& name)
{
	const size_t n = std::strlen(kWindowsPrefix);
	if (name.compare(0, n, kWindowsPrefix) == 0) return name.substr(n);
	return name;
}

}  // namespace

#ifdef _WIN32

namespace {

std::wstring Widen(const std::string& s)
{
	if (s.empty()) return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

std::string FullPipePath(const std::string& name) { return std::string(kWindowsPrefix) + BareName(name); }

}  // namespace

std::string EndpointPath(const std::string& name) { return FullPipePath(name); }

Server::Server() = default;
Server::~Server() { Close(); }

bool Server::Create(const std::string& name, std::string* error)
{
	Close();
	m_stopped = false;
	m_name = BareName(name);
	const std::string full = FullPipePath(name);
	// PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, one instance: the same flags the
	// editor's client and the mock's server already used.
	HANDLE h = CreateNamedPipeA(full.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
	                            1, 65536, 65536, 0, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		if (error) *error = "CreateNamedPipe failed (" + std::to_string(GetLastError()) + ")";
		return false;
	}
	m_fd = (int64_t)(intptr_t)h;
	return true;
}

Client Server::Accept(unsigned timeoutMs, bool* timedOut)
{
	if (timedOut) *timedOut = false;
	if (!listening()) return Client();
	HANDLE h = (HANDLE)(intptr_t)m_fd;
	if (timeoutMs == 0) {
		if (ConnectNamedPipe(h, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
			Client c;
			c.fd = m_fd;
			return c;
		}
		return Client();
	}
	// A deadline needs overlapped connect; keep it simple and let the caller
	// poll listening()/Stop() the way game_link_mock's loop already does. A
	// stopped server has closed its handle, which unblocks ConnectNamedPipe.
	if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
		if (timedOut) *timedOut = true;
		return Client();
	}
	Client c;
	c.fd = m_fd;
	return c;
}

void Server::Disconnect(const Client&) { if (listening()) DisconnectNamedPipe((HANDLE)(intptr_t)m_fd); }

void Server::Stop()
{
	if (!listening()) return;
	HANDLE h = (HANDLE)(intptr_t)m_fd;
	// CancelIoEx wakes a blocked ConnectNamedPipe/ReadFile, DisconnectNamedPipe
	// refuses new clients: exactly what MockDll::Stop does before joining.
	CancelIoEx(h, nullptr);
	DisconnectNamedPipe(h);
	CloseHandle(h);
	m_fd = kNoClient;
}

void Server::Close() { Stop(); }

Client Connect(const std::string& name, unsigned timeoutMs, bool* busy, std::string* error)
{
	if (busy) *busy = false;
	const std::string full = FullPipePath(name);
	const std::wstring wname = Widen(full);
	const uint64_t start = GetTickCount64();
	for (;;) {
		HANDLE h = CreateFileA(full.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			DWORD mode = PIPE_READMODE_BYTE;
			SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
			Client c;
			c.fd = (int64_t)(intptr_t)h;
			return c;
		}
		const DWORD e = GetLastError();
		if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
			if (error) *error = "no pipe " + full;
			return Client();
		}
		if (e != ERROR_PIPE_BUSY) {
			if (error) *error = "cannot open " + full + " (error " + std::to_string(e) + ")";
			return Client();
		}
		if (busy) *busy = true;
		if (timeoutMs == 0) {
			if (error) *error = "pipe busy: " + full;
			return Client();
		}
		// Busy: the server exists but has no free instance. WaitNamedPipe is the
		// Win32 wait; fall back to a short sleep when it times out so the total
		// deadline is still honoured.
		DWORD slice = timeoutMs;
		const uint64_t elapsed = GetTickCount64() - start;
		if (elapsed >= timeoutMs) {
			if (error) *error = "pipe busy: " + full;
			return Client();
		}
		if (slice > timeoutMs - elapsed) slice = (DWORD)(timeoutMs - elapsed);
		if (!WaitNamedPipeW(wname.c_str(), slice)) Sleep(10);
	}
}

int64_t BytesAvailable(const Client& c)
{
	if (!c.valid()) return -1;
	DWORD avail = 0;
	if (!PeekNamedPipe((HANDLE)(intptr_t)c.fd, nullptr, 0, nullptr, &avail, nullptr)) return -1;
	return (int64_t)avail;
}

bool ReadExact(const Client& c, void* out, size_t n)
{
	if (!c.valid()) return false;
	char* p = (char*)out;
	size_t done = 0;
	while (done < n) {
		DWORD rd = 0;
		if (!ReadFile((HANDLE)(intptr_t)c.fd, p + done, (DWORD)(n - done), &rd, nullptr) || rd == 0) return false;
		done += rd;
	}
	return true;
}

bool WriteAll(const Client& c, const void* data, size_t n)
{
	if (!c.valid()) return false;
	const char* p = (const char*)data;
	size_t done = 0;
	while (done < n) {
		DWORD wr = 0;
		if (!WriteFile((HANDLE)(intptr_t)c.fd, p + done, (DWORD)(n - done), &wr, nullptr) || wr == 0) return false;
		done += wr;
	}
	return true;
}

void Close(Client& c) { if (c.valid()) CloseHandle((HANDLE)(intptr_t)c.fd); c.fd = kNoClient; }

#else  /* POSIX */

namespace {

// XDG_RUNTIME_DIR is the per-user, 0700 directory the socket belongs in; /tmp
// is the fallback for a session without one (the name still carries the
// endpoint, and the socket is created 0600).
std::string RuntimeDir()
{
	const char* xdg = getenv("XDG_RUNTIME_DIR");
	if (xdg && *xdg) return std::string(xdg) + "/hantei";
	return "/tmp/hantei-" + std::to_string((unsigned)getuid());
}

bool MakeSockaddr(const std::string& path, sockaddr_un& addr, socklen_t& len)
{
	if (path.size() >= sizeof addr.sun_path) return false;
	std::memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
	len = (socklen_t)(offsetof(sockaddr_un, sun_path) + path.size() + 1);
	return true;
}

}  // namespace

std::string EndpointPath(const std::string& name) { return RuntimeDir() + "/" + BareName(name) + ".sock"; }

Server::Server() = default;
Server::~Server() { Close(); }

bool Server::Create(const std::string& name, std::string* error)
{
	Close();
	m_stopped = false;   // Close() set it; this is a fresh endpoint
	m_name = BareName(name);
	m_path = EndpointPath(name);
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(m_path).parent_path(), ec);
	const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		if (error) *error = std::string("socket failed: ") + std::strerror(errno);
		return false;
	}
	sockaddr_un addr{};
	socklen_t len = 0;
	if (!MakeSockaddr(m_path, addr, len)) {
		close(fd);
		if (error) *error = "endpoint path too long: " + m_path;
		return false;
	}
	// A crashed server leaves the file behind. Connect once to find out whether
	// it is live; only a dead one is unlinked and re-bound.
	if (std::filesystem::exists(std::filesystem::path(m_path), ec)) {
		const int probe = socket(AF_UNIX, SOCK_STREAM, 0);
		const bool live = probe >= 0 && connect(probe, (sockaddr*)&addr, len) == 0;
		if (probe >= 0) close(probe);
		if (live) {
			close(fd);
			if (error) *error = "endpoint already served: " + m_path;
			return false;
		}
		std::filesystem::remove(std::filesystem::path(m_path), ec);
	}
	if (bind(fd, (sockaddr*)&addr, len) != 0) {
		close(fd);
		if (error) *error = std::string("bind failed: ") + std::strerror(errno);
		return false;
	}
	chmod(m_path.c_str(), 0600);
	if (listen(fd, 1) != 0) {
		close(fd);
		if (error) *error = std::string("listen failed: ") + std::strerror(errno);
		return false;
	}
	// A non-blocking accept lets the caller honour a timeout and notice Stop().
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	m_fd = fd;
	return true;
}

Client Server::Accept(unsigned timeoutMs, bool* timedOut)
{
	if (timedOut) *timedOut = false;
	if (!listening()) return Client();
	const int fd = (int)m_fd;
	const uint64_t deadline = timeoutMs ? NowMs() + timeoutMs : 0;
	for (;;) {
		const int c = accept(fd, nullptr, nullptr);
		if (c >= 0) {
			Client out;
			out.fd = c;
			return out;
		}
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return Client();
		if (m_stopped) return Client();
		if (timeoutMs && NowMs() >= deadline) {
			if (timedOut) *timedOut = true;
			return Client();
		}
		// 5 ms poll interval: the mock's serve loop already slept 20 ms between
		// connects, and game_link_mock's Stop() must not wait a full deadline.
		SleepMs(5);
	}
}

void Server::Disconnect(const Client& c) { if (c.valid()) close((int)c.fd); }

void Server::Stop()
{
	m_stopped = true;
	if (m_fd != kNoClient) {
		close((int)m_fd);
		m_fd = kNoClient;
	}
	std::error_code ec;
	if (!m_path.empty()) std::filesystem::remove(std::filesystem::path(m_path), ec);
}

void Server::Close() { Stop(); }

Client Connect(const std::string& name, unsigned timeoutMs, bool* busy, std::string* error)
{
	if (busy) *busy = false;
	const std::string path = EndpointPath(name);
	sockaddr_un addr{};
	socklen_t len = 0;
	if (!MakeSockaddr(path, addr, len)) {
		if (error) *error = "endpoint path too long: " + path;
		return Client();
	}
	const uint64_t deadline = timeoutMs ? NowMs() + timeoutMs : 0;
	for (;;) {
		const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) {
			if (error) *error = std::string("socket failed: ") + std::strerror(errno);
			return Client();
		}
		if (connect(fd, (sockaddr*)&addr, len) == 0) {
			Client c;
			c.fd = fd;
			return c;
		}
		const int e = errno;
		close(fd);
		if (e == ENOENT || e == ECONNREFUSED) {
			// No live server (a leftover socket file gives ECONNREFUSED).
			if (timeoutMs == 0 || NowMs() >= deadline) {
				if (error) *error = "no pipe " + path;
				return Client();
			}
			SleepMs(10);
			continue;
		}
		if (error) *error = "cannot open " + path + ": " + std::strerror(e);
		return Client();
	}
}

int64_t BytesAvailable(const Client& c)
{
	if (!c.valid()) return -1;
	int avail = 0;
	if (ioctl((int)c.fd, FIONREAD, &avail) != 0) return -1;
	return (int64_t)avail;
}

bool ReadExact(const Client& c, void* out, size_t n)
{
	if (!c.valid()) return false;
	char* p = (char*)out;
	size_t done = 0;
	while (done < n) {
		const ssize_t r = read((int)c.fd, p + done, n - done);
		if (r <= 0) return false;
		done += (size_t)r;
	}
	return true;
}

bool WriteAll(const Client& c, const void* data, size_t n)
{
	if (!c.valid()) return false;
	const char* p = (const char*)data;
	size_t done = 0;
	while (done < n) {
		const ssize_t w = write((int)c.fd, p + done, n - done);
		if (w <= 0) return false;
		done += (size_t)w;
	}
	return true;
}

void Close(Client& c) { if (c.valid()) close((int)c.fd); c.fd = kNoClient; }

#endif  /* _WIN32 */

bool Request(const std::string& name, const std::string& request, unsigned timeoutMs, std::string& reply,
             std::string* error)
{
	reply.clear();
	if (error) error->clear();
	bool busy = false;
	Client c = Connect(name, 2000, &busy, error);
	if (!c.valid()) return false;
	bool ok = WriteAll(c, request.data(), request.size());
	if (!ok && error) *error = "pipe write failed";
	if (ok) {
		const uint64_t deadline = NowMs() + (timeoutMs ? timeoutMs : 2000);
		char buf[4097];
		bool closed = false;
		uint64_t idleSince = 0;   // when the pipe last had nothing to read
		for (;;) {
			const int64_t avail = BytesAvailable(c);
			if (avail < 0) { closed = true; break; }   // peer closed
			if (avail > 0) {
				const size_t want = avail < (int64_t)sizeof buf ? (size_t)avail : sizeof buf;
				if (!ReadExact(c, buf, want)) { closed = true; break; }
				reply.append(buf, want);
				if (reply.size() >= 4096) break;
				idleSince = 0;
			} else if (!reply.empty()) {
				// The link's replies arrive as one short line and the server keeps
				// the pipe open, so stop once nothing more has come for a moment
				// instead of waiting out the whole timeout.
				const uint64_t now = NowMs();
				if (!idleSince) idleSince = now;
				if (now - idleSince >= 20) break;
			}
			if (NowMs() >= deadline) break;
			SleepMs(2);
		}
		if (reply.empty() && error) *error = closed ? "the pipe closed with no reply" : "no reply within the timeout";
	}
	Close(c);
	return ok && !reply.empty();
}

}  // namespace pipe
}  // namespace portable
