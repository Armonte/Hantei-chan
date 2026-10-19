// Unit tests for src/platform/portable/{paths,process,pipe,steam} (lane LINUX
// deliverable 2). Runs on the Windows mingw build and natively on Linux, so the
// same assertions cover both bodies. Exit code 0 = pass.
#include "platform/portable/paths.h"
#include "platform/portable/pipe.h"
#include "platform/portable/process.h"
#include "platform/portable/steam.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

// A throwaway folder next to the executable that is removed on scope exit.
struct TempDir {
	fs::path path;
	TempDir()
	{
		const fs::path base = fs::u8path(portable::ExecutableDir()) / "portable_test_tmp";
		std::error_code ec;
		fs::remove_all(base, ec);
		fs::create_directories(base, ec);
		char tag[32];
		std::snprintf(tag, sizeof tag, "%u", (unsigned)portable::NowUs());
		path = base / tag;
		fs::create_directories(path, ec);
	}
	~TempDir() { std::error_code ec; fs::remove_all(fs::u8path(portable::ExecutableDir()) / "portable_test_tmp", ec); }
};

std::string ReadText(const fs::path& p)
{
	std::vector<uint8_t> b;
	portable::ReadWholeFile(p.u8string(), b);
	return std::string(b.begin(), b.end());
}

}  // namespace

static void TestPaths(const TempDir& tmp)
{
	// Encoding: UTF-8 round trips through wchar_t on both platforms, and the
	// non-ASCII bytes survive (Japanese, an emoji outside the BMP).
	for (const char* s : { "", "plain", u8"\u30C6\u30B9\u30C8", u8"a\U0001F600b" }) {
		const std::string in = s;
		CHECK(portable::WideToUtf8(portable::Utf8ToWide(in)) == in);
	}
	// The ANSI helpers are the code page 0 pair; on Linux they are UTF-8.
	CHECK(portable::AnsiToUtf8("abc") == "abc");
	CHECK(portable::Utf8ToAnsi("abc") == "abc");
	CHECK(portable::CpToUtf8("x") == "x");

	// Separator and drive-letter normalisation, the misc.cpp behaviour.
	// The misc.cpp normalizePath behaviour: separators forward, drive letter up.
	CHECK(portable::NormalizeSeparators("C:\\Games\\MBAA\\") == "C:/Games/MBAA");
	CHECK(portable::NormalizeSeparators("/a/b/") == "/a/b");
	CHECK(portable::NormalizeSeparators("a") == "a");

	// ExecutableDir is the parent of ExecutablePath and both are non-empty on a
	// normal run.
	CHECK(!portable::ExecutablePath().empty());
	CHECK(fs::u8path(portable::ExecutablePath()).parent_path().u8string() == portable::ExecutableDir());

	// CurrentDir agrees with std::filesystem.
	CHECK(portable::CurrentDir() == fs::current_path().u8string());

	// ReplaceFileAtomic writes, replaces and leaves no temp file behind.
	const std::string target = (tmp.path / "atomic.bin").u8string();
	CHECK(portable::ReplaceFileAtomic(target, "one", 3));
	CHECK(ReadText(fs::u8path(target)) == "one");
	CHECK(portable::ReplaceFileAtomic(target, "two-two", 7));
	CHECK(ReadText(fs::u8path(target)) == "two-two");
	CHECK(portable::FileSize(target) == 7);
	int siblings = 0;
	for (const auto& e : fs::directory_iterator(tmp.path)) if (e.path().filename().string().find(".tmp") != std::string::npos) ++siblings;
	CHECK(siblings == 0);
	CHECK(!portable::FileExists((tmp.path / "missing.bin").u8string()));

	CHECK(portable::CreateDirectories((tmp.path / "a" / "b").u8string()));
	CHECK(fs::is_directory(tmp.path / "a" / "b"));

	// ini: round trip across sections and the parsing rules the Win32 calls had.
	const std::string ini = (tmp.path / "settings.ini").u8string();
	CHECK(portable::IniSetString(ini, "han2", "Language", "2"));
	CHECK(portable::IniSetString(ini, "browser", "Dir_3", "c:/games/mbaa"));
	CHECK(portable::IniGetInt(ini, "han2", "Language", 0) == 2);
	CHECK(portable::IniGetString(ini, "browser", "Dir_3") == "c:/games/mbaa");
	CHECK(portable::IniGetString(ini, "browser", "Absent", "fallback") == "fallback");
	CHECK(portable::IniGetInt(ini, "browser", "Absent", 7) == 7);
	// Overwrite keeps one key and the other section intact.
	CHECK(portable::IniSetString(ini, "han2", "Language", "1"));
	CHECK(portable::IniGetInt(ini, "han2", "Language", 0) == 1);
	CHECK(portable::IniGetString(ini, "browser", "Dir_3") == "c:/games/mbaa");
	// A hand-written ini with CRLF, a comment and spaces reads the way
	// GetPrivateProfileString did (value trimmed, ';' starts a comment).
	const std::string hand = (tmp.path / "hand.ini").u8string();
	const std::string body = "; comment\r\n[s]\r\nkey =  spaced  ; trailing\r\nother=1\r\n";
	CHECK(portable::ReplaceFileAtomic(hand, body.data(), body.size()));
	CHECK(portable::IniGetString(hand, "s", "key") == "spaced");
	CHECK(portable::IniGetInt(hand, "s", "other", 0) == 1);

	// ReadFileAlloc is misc.cpp's ReadInMem: a caller-owned buffer, a one-byte
	// allocation for an empty file, and false (no allocation) on a missing one.
	const std::string blob = (tmp.path / "blob.bin").u8string();
	CHECK(portable::ReplaceFileAtomic(blob, "0123456789", 10));
	char* data = nullptr;
	unsigned size = 0;
	CHECK(portable::ReadFileAlloc(blob, data, size));
	CHECK(size == 10 && data && std::memcmp(data, "0123456789", 10) == 0);
	delete[] data;
	data = nullptr; size = 0;
	CHECK(!portable::ReadFileAlloc((tmp.path / "nope.bin").u8string(), data, size));
	CHECK(data == nullptr && size == 0);

	// The CP932 pair: the games' Japanese names. The bytes are the CP932 form of
	// "テスト", the same string the encoding checks above round-trip through
	// wchar_t. A byte sequence CP932 cannot decode gives "" (strict) or a
	// replacement character (lenient), never a crash.
	const std::string sjis("\x83\x65\x83\x58\x83\x67");
	const std::string utf8("\u30c6\u30b9\u30c8");
	CHECK(portable::SjisToUtf8(sjis) == utf8);
	CHECK(portable::Utf8ToSjis(utf8) == sjis);
	CHECK(portable::SjisToUtf8("") == "");
	CHECK(portable::Utf8ToSjis("") == "");
	// A lone lead byte that cannot start a CP932 pair is not valid input.
	CHECK(portable::SjisToUtf8("\x83").size() <= 3);
}

static void TestSteam()
{
	// Nothing to assert about the machine's Steam install; the point is that the
	// call is safe when there is none and the helpers stay consistent.
	const std::vector<std::string> libs = portable::SteamLibraries();
	for (const std::string& l : libs) CHECK(l.find('\\') == std::string::npos);
	const std::vector<portable::SteamApp> apps = portable::SteamApps();
	for (const portable::SteamApp& a : apps) CHECK(!a.dir.empty());
	CHECK(portable::SteamAppDir("this-app-does-not-exist-9f3a").empty());
}

static void TestProcess()
{
	CHECK(portable::CurrentPid() != 0);
	CHECK(portable::ProcessAlive(portable::CurrentPid()));
	// A pid that will not be live: 0 is never a real process on either platform.
	CHECK(!portable::ProcessAlive(0));
	CHECK(portable::NowMs() > 0);
	CHECK(portable::NowUs() > portable::NowMs());   // microseconds are the finer unit
	CHECK(portable::LocalTimeString().size() == 19);
	const uint64_t t0 = portable::NowMs();
	portable::SleepMs(20);
	CHECK(portable::NowMs() >= t0 + 15);
	// FindProcessesByName matches case-insensitively and finds nothing for a name
	// no process can have.
	CHECK(portable::FindProcessesByName("__no_such_process__").empty());
}

static void TestPipe()
{
	using namespace portable::pipe;
	const std::string name = "portable-test-" + std::to_string(portable::CurrentPid());
	// EndpointPath is the one string both sides must agree on.
#ifdef _WIN32
	CHECK(EndpointPath(name) == "\\\\.\\pipe\\" + name);
#else
	CHECK(EndpointPath(name).find(name + ".sock") != std::string::npos);
#endif
	CHECK(EndpointPath("\\\\.\\pipe\\" + name) == EndpointPath(name));

	Server server;
	std::string error;
	CHECK(server.Create(name, &error));
	if (!server.listening()) { std::fprintf(stderr, "server create failed: %s\n", error.c_str()); return; }

	std::atomic<bool> serverOk{false};
	std::thread worker([&] {
		bool timedOut = false;
		Client c = server.Accept(2000, &timedOut);
		if (!c.valid()) return;
		char buf[64] = {0};
		if (!ReadExact(c, buf, 5)) return;
		const bool wrote = WriteAll(c, "PONG!", 5);
		(void)wrote;
		serverOk = std::memcmp(buf, "PING!", 5) == 0;
		// No Disconnect here: DisconnectNamedPipe discards data the client has
		// not read yet, so the server side of the test only tears the endpoint
		// down after the client is finished (server.Close() below).
	});

	bool busy = false;
	Client client = Connect(name, 2000, &busy, &error);
	CHECK(client.valid());
	if (client.valid()) {
		CHECK(WriteAll(client, "PING!", 5));
		char back[5] = {0};
		CHECK(ReadExact(client, back, 5));
		CHECK(std::memcmp(back, "PONG!", 5) == 0);
		Close(client);
	}
	worker.join();
	CHECK(serverOk.load());
	server.Close();

	// Connecting to a closed endpoint reports no server, not a hang.
	bool busy2 = false;
	Client gone = Connect(name, 0, &busy2, &error);
	CHECK(!gone.valid());

	// One round trip through the helper the fpan-im client uses.
	Server echo;
	CHECK(echo.Create(name + "-echo", &error));
	std::thread echoThread([&] {
		Client c = echo.Accept(2000);
		if (!c.valid()) return;
		char req[32] = {0};
		if (ReadExact(c, req, 4)) WriteAll(c, "OK 1", 4);
	});
	std::string reply;
	const bool ok = Request(name + "-echo", "ping", 2000, reply, &error);
	if (!ok) std::fprintf(stderr, "Request failed: %s (reply '%s')\n", error.c_str(), reply.c_str());
	CHECK(ok);
	CHECK(reply == "OK 1");
	echoThread.join();
	echo.Close();
}

int main()
{
	TempDir tmp;
	if (tmp.path.empty()) { std::fprintf(stderr, "could not create a temp folder\n"); return 1; }
	TestPaths(tmp);
	TestSteam();
	TestProcess();
	TestPipe();
	if (g_fail) { std::fprintf(stderr, "%d check(s) failed\n", g_fail); return 1; }
	std::printf("portable_test: ok\n");
	return 0;
}
