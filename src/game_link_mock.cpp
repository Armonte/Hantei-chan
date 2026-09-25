// Mock dev-link server — see game_link_mock.h.
#include "game_link_mock.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace gamelink {

namespace {

void CopyName(char* dst, size_t cap, const char* src) { std::snprintf(dst, cap, "%s", src); }

bool WriteAll(HANDLE h, const void* p, DWORD n)
{
	DWORD w = 0;
	return WriteFile(h, p, n, &w, nullptr) && w == n;
}

bool ReadAll(HANDLE h, void* p, DWORD n)
{
	DWORD got = 0;
	while (got < n) {
		DWORD r = 0;
		if (!ReadFile(h, (uint8_t*)p + got, n - got, &r, nullptr) || !r) return false;
		got += r;
	}
	return true;
}

bool Send(HANDLE h, wire::Kind k, const void* body, uint32_t size)
{
	wire::Header hd{ (uint16_t)k, wire::kLinkVersion, size };
	uint8_t buf[1024];
	std::memcpy(buf, &hd, sizeof hd);
	std::memcpy(buf + sizeof hd, body, size);
	return WriteAll(h, buf, (DWORD)(sizeof hd + size));
}

bool Reply(HANDLE h, const wire::Command& c, wire::Status st, const char* msg)
{
	wire::Reply r{};
	r.op = c.op; r.seq = c.seq; r.status = (int16_t)st; r.reloadCount = 3;
	CopyName(r.message, sizeof r.message, msg);
	return Send(h, wire::Kind::LinkReply, &r, sizeof r);
}

} // namespace

MockDll::MockDll(Options o) : m_o(o) {}
MockDll::~MockDll() { Stop(); }
uint32_t MockDll::Pid() const { return (uint32_t)GetCurrentProcessId(); }

wire::State MockDll::FakeState(const Options& o)
{
	wire::State s{};
	s.worldTimer = 5400;
	s.gameModeKind = 0x100;
	s.reloadCount = 3;
	s.scene = o.inBattle ? 1 : 20;
	s.reloadAllowed = (o.inBattle && !o.session) ? 1 : 0;
	s.tagLive = 1;
	s.teamActive[0] = 0; s.teamActive[1] = 3;
	s.teamTagRequest[0] = 1; s.teamTagRequest[1] = 1;
	if (!o.inBattle) return s;
	struct A { const char* f; int team, tagFlag, partner, chara, pattern, frame, x, y; };
	const A a[4] = { { "shiki", 0, 0, 2, 7, 105, 3, -20000, 0 },  { "v_sion", 1, 1, 3, 11, 0, 0, -102400, 0 },
	                 { "sion", 0, 0, 0, 0, 455, 6, -56000, 0 },    { "miyako", 1, 0, 1, 8, 241, 9, 30000, 0 } };
	for (int i = 0; i < 4; ++i) {
		wire::Actor& x = s.actors[i];
		x.exists = 1; x.team = (uint8_t)a[i].team; x.tagFlag = (uint8_t)a[i].tagFlag; x.partnerSlot = (uint8_t)a[i].partner;
		x.chara = (int16_t)a[i].chara; x.pattern = a[i].pattern; x.frame = a[i].frame; x.frameTicks = 2; x.patternTicks = 20;
		x.x = a[i].x; x.y = a[i].y;
		CopyName(x.file, sizeof x.file, a[i].f);
	}
	return s;
}

wire::Tag MockDll::FakeTag(const Options& o)
{
	wire::Tag t{};
	t.sessionFlags = o.session ? wire::kTagSessNetplay | wire::kTagSessRollback : 0;
	t.frozen = o.session;
	t.iniPresent = 1;
	t.koRule = 0;
	t.tuningLoads = 4;
	t.warnings = 1;
	t.assistEnabled = 1;
	CopyName(t.activeStyle, sizeof t.activeStyle, "Classic");
	CopyName(t.sha, sizeof t.sha, "1a2b3c4d");
	// team 1: point Shiki (slot 0), Sion (slot 2) is acting as a 6+FN1 assist (pattern mode, drop entry), pattern 455
	t.team[0] = { 0, (uint8_t)(2 | (1 << 4)), 2, 0, 301, 14, -1, 0, 14, 0, 455, 2, 0 };
	// team 2: Miyako (slot 3) just tagged in: cooldown after the exit
	t.team[1] = { 3, 0, 0, 0, 101, 40, 0x7FFF, 80, 0, 60, 0, 0, 0 };
	const int16_t in[4] = { 241, 241, 241, 241 }, out[4] = { 242, 242, 242, 242 };
	const int32_t hp[4] = { 9200, 11400, 7300, 10050 }, red[4] = { 9800, 11400, 8100, 10050 };
	for (int i = 0; i < 4; ++i) t.slot[i] = { in[i], out[i], hp[i], red[i] };
	return t;
}

bool MockDll::Start()
{
	char name[96];
	std::snprintf(name, sizeof name, "\\\\.\\pipe\\povertycaster-link-%u", (unsigned)Pid());
	HANDLE h = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536,
	                            0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	m_pipe = h;
	m_thread = std::thread([this] { run(); });
	return true;
}

void MockDll::Stop()
{
	if (!m_pipe) return;
	m_quit = true;
	CancelIoEx((HANDLE)m_pipe, nullptr);
	DisconnectNamedPipe((HANDLE)m_pipe);
	CloseHandle((HANDLE)m_pipe);
	if (m_thread.joinable()) m_thread.join();
	m_pipe = nullptr;
}

void MockDll::run()
{
	HANDLE h = (HANDLE)m_pipe;
	while (!m_quit) {
		if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
			if (m_quit) return;
			Sleep(20);
			continue;
		}
		for (;;) {
			wire::Header hd;
			if (!ReadAll(h, &hd, sizeof hd)) break;
			uint8_t body[1024];
			if (hd.size > sizeof body || !ReadAll(h, body, hd.size)) break;
			if (hd.kind != (uint16_t)wire::Kind::LinkCommand || hd.size != sizeof(wire::Command)) continue;
			wire::Command c;
			std::memcpy(&c, body, sizeof c);
			++m_commands;
			bool ok = true;
			switch ((wire::Op)c.op) {
			case wire::Op::Ping: ok = Reply(h, c, wire::Status::Ok, "pong (mock)"); break;
			case wire::Op::QueryState: { const wire::State s = FakeState(m_o); ok = Send(h, wire::Kind::LinkState, &s, sizeof s); break; }
			case wire::Op::QueryTag:
				if (m_o.tagOps) { const wire::Tag t = FakeTag(m_o); ok = Send(h, wire::Kind::LinkTag, &t, sizeof t); }
				else ok = Reply(h, c, wire::Status::Unknown, "unknown op");
				break;
			case wire::Op::SetChar:
				if (c.chara == -1 && c.moon == -1 && c.palette == -1 && !(c.flags & wire::kFlagReload)) ++m_probes;
				if (m_o.session) ok = Reply(h, c, wire::Status::RefusedSession, "a netplay/rollback session is live (mock)");
				else if (!m_o.inBattle) ok = Reply(h, c, wire::Status::RefusedScene, "not in a battle (mock)");
				else ok = Reply(h, c, wire::Status::Ok, "held until the next reload");
				break;
			case wire::Op::Reload:
				ok = Reply(h, c, m_o.session ? wire::Status::RefusedSession : wire::Status::Ok, m_o.session ? "session (mock)" : "reload done (mock)");
				break;
			default: ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break;
			}
			if (!ok) break;
		}
		DisconnectNamedPipe(h);
	}
}

} // namespace gamelink
