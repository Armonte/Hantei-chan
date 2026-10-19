#ifndef PORTABLE_PIPE_H_GUARD
#define PORTABLE_PIPE_H_GUARD

// Portable byte-stream IPC for the fpan-im / MBAA link.
//
// Lane LINUX deliverable 2. On Windows the body is CreateFileW/CreateNamedPipeA
// + PeekNamedPipe + WaitNamedPipeW copied from squirrel/fpanim_link.cpp,
// game_link.cpp and game_link_mock.cpp. On POSIX it is an AF_UNIX stream socket
// in XDG_RUNTIME_DIR (falling back to /tmp), which gives the same
// connect/server behaviour without a Windows kernel object.
//
// The name mapping is the one thing the two sides must agree on: a Windows
// endpoint "fpan-im-mbtl" (the game_link call sites pass the bare name and the
// code adds the pipe prefix) becomes "$XDG_RUNTIME_DIR/hantei/fpan-im-mbtl.sock".

#include <cstdint>
#include <string>

namespace portable {
namespace pipe {

// Sentinel for "no client connected". Callers compare through valid().
constexpr int64_t kNoClient = -1;
struct Client {
	int64_t fd = kNoClient;
	bool valid() const { return fd != kNoClient; }
};

// Server: the listening end. 'name' is the bare endpoint name ("fpan-im-uni2",
// "povertycaster-link-<pid>"); a Windows pipe prefix is accepted and stripped so
// the existing call sites keep their strings.
class Server {
public:
	Server();
	~Server();
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	// Create the endpoint. On POSIX a stale socket file from a crashed server is
	// replaced; an endpoint another live server owns fails with 'error' set.
	bool Create(const std::string& name, std::string* error = nullptr);
	// Block until a client connects, with an optional deadline. timeoutMs == 0
	// waits forever. Returns an invalid Client on timeout or when Stop() was
	// called; *timedOut is set on a real timeout so the serve loop can tell the
	// two apart (the mock's loop does).
	Client Accept(unsigned timeoutMs = 0, bool* timedOut = nullptr);
	// Close the current client (DisconnectNamedPipe).
	void Disconnect(const Client& c);
	// Wake a blocked Accept() and refuse further connects (Stop() in game_link_mock).
	void Stop();
	void Close();
	bool listening() const { return m_fd != kNoClient; }

private:
	int64_t m_fd = kNoClient;
	std::string m_path;      // POSIX socket path (empty on Windows)
	std::string m_name;
	bool m_stopped = false;  // set by Stop(); Accept() stops trying
};

// Client: the connecting end.

// Connect to a named endpoint. timeoutMs == 0 -> one attempt (the caller sees a
// busy endpoint and can report it); a longer timeout retries while the server is
// starting, like WaitNamedPipeW did. 'busy' distinguishes "the server exists but
// has no free instance" from "no server", which the link reports to the user.
Client Connect(const std::string& name, unsigned timeoutMs = 0, bool* busy = nullptr, std::string* error = nullptr);

// How many bytes are ready to read. This is the PeekNamedPipe the link's worker
// relies on so it never blocks inside a read; returns -1 when the peer closed
// the pipe (the error the callers already handle as a game exit).
int64_t BytesAvailable(const Client& c);

// Blocking read/write of exactly n bytes (the link frames its own messages and
// writes one buffer at a time). Return false on a short write, EOF or error.
bool ReadExact(const Client& c, void* out, size_t n);
bool WriteAll(const Client& c, const void* data, size_t n);

// One round trip: write 'request', then read a reply with a deadline, the shape
// squirrel/fpanim_link.cpp's RequestExact has. The reply is capped at 64 KiB.
bool Request(const std::string& name, const std::string& request, unsigned timeoutMs, std::string& reply,
             std::string* error = nullptr);

void Close(Client& c);

// The socket path a name maps to on POSIX, and the identity on Windows. Exposed
// so the inventory and the tests can print what a name becomes.
std::string EndpointPath(const std::string& name);

}  // namespace pipe
}  // namespace portable

#endif /* PORTABLE_PIPE_H_GUARD */
