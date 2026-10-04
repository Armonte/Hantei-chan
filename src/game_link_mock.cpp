// Mock dev-link server — see game_link_mock.h.
#include "game_link_mock.h"
#include "authoring/authoring_model.h"
#include "authoring/roster_mirror.h"
#include "tag_tuning/tag_sidecar.h"
#include "game_frame_ring.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace gamelink {

namespace {

uint64_t NowMs() { return GetTickCount64(); }

void CopyName(char* dst, size_t cap, const char* src) { std::snprintf(dst, cap, "%s", src); }

bool WriteAll(HANDLE h, const void* p, DWORD n)
{
	DWORD w = 0;
	return WriteFile(h, p, n, &w, nullptr) && w == n;
}

bool Send(HANDLE h, wire::Kind k, const void* body, uint32_t size)
{
	wire::Header hd{ (uint16_t)k, wire::kLinkVersion, size };
	uint8_t buf[sizeof(wire::Header) + 1024];
	std::memcpy(buf, &hd, sizeof hd);
	std::memcpy(buf + sizeof hd, body, size);
	return WriteAll(h, buf, (DWORD)(sizeof hd + size));
}

bool ReplyTo(HANDLE h, uint16_t op, uint16_t seq, wire::Status st, const std::string& msg)
{
	wire::Reply r{};
	r.op = op; r.seq = seq; r.status = (int16_t)st; r.reloadCount = 3;
	CopyName(r.message, sizeof r.message, msg.c_str());
	return Send(h, wire::Kind::LinkReply, &r, sizeof r);
}
bool Reply(HANDLE h, const wire::Command& c, wire::Status st, const char* msg) { return ReplyTo(h, c.op, c.seq, st, msg); }

std::string FileOfChara(int chara)
{
	for (const authoring::RosterMirrorRow& r : authoring::kRosterMirror) if (r.chara == chara) return r.file1;
	return {};
}

uint8_t SessionFlags(const MockDll::Options& o) { return o.session ? wire::kTagSessNetplay | wire::kTagSessRollback : 0; }

} // namespace

MockDll::MockDll(Options o) : m_o(o)
{
	m_ss.requested = DefaultSetup();
	if (!m_o.setupHex.empty()) authoring::FromHex(m_o.setupHex, m_ss.requested);
	m_ss.inForce = m_ss.requested;
	m_ss.phase = (uint8_t)(m_o.session && m_o.betweenRounds ? wire::Phase::RoundEnd : m_o.inBattle ? wire::Phase::Battle : wire::Phase::CharaSelect);
	m_ss.authState = (uint8_t)(m_o.inBattle ? wire::AuthState::Ready : wire::AuthState::Idle);
	if (!m_o.inBattle) std::memset(&m_ss.inForce, 0, sizeof m_ss.inForce);
	m_ss.gameModeKind = 0x100;
	m_ss.sessionFlags = SessionFlags(m_o);
	m_ss.sessionRole = m_o.session ? m_o.role : 0;
	m_ss.betweenRounds = m_o.session && m_o.betweenRounds;
	m_ss.editsAllowed = !m_o.session || (m_o.role == 1 && m_o.betweenRounds);
	CopyName(m_ss.message, sizeof m_ss.message, m_o.inBattle ? "authoring VS (mock)" : "character select (mock)");
}
MockDll::~MockDll() { Stop(); }
uint32_t MockDll::Pid() const { return (uint32_t)GetCurrentProcessId(); }

wire::MatchSetup MockDll::DefaultSetup()
{
	authoring::Setup s;
	s.mode = authoring::Mode::Tag;
	s.assists = true;
	s.stage = 16;
	s.slot[0] = { 7, 0, 3 };
	s.slot[1] = { 11, 0, 0 };
	s.slot[2] = { 0, 0, 0 };
	s.slot[3] = { 8, 1, 0 };
	s.assist[0][1] = 1;
	s.assist[1][0] = 13;
	return authoring::ToWire(s);
}

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

// The live state follows the setup in force: the actors carry its picks (so Follow point / SlotMaskForFile work
// against whatever the Setup tab loaded).
wire::State MockDll::state() const
{
	Options o = m_o;
	o.inBattle = m_ss.phase == (uint8_t)wire::Phase::Battle;
	wire::State s = FakeState(o);
	if (!o.inBattle) return s;
	const wire::MatchSetup& f = m_ss.inForce;
	for (int i = 0; i < 4; ++i) {
		wire::Actor& x = s.actors[i];
		const int chara = f.slot[i].chara;
		if (chara < 0) { x = wire::Actor{}; continue; }
		const std::string file = FileOfChara(chara);
		x.exists = 1;
		x.chara = (int16_t)chara;
		x.moon = f.slot[i].moon;
		x.palette = f.slot[i].palette;
		CopyName(x.file, sizeof x.file, file.c_str());
	}
	if (f.mode == (uint8_t)wire::Mode::Versus) { s.tagLive = 0; s.teamActive[0] = 0; s.teamActive[1] = 1; }
	return s;
}

wire::Tag MockDll::FakeTag(const Options& o)
{
	wire::Tag t{};
	t.sessionFlags = SessionFlags(o);
	t.frozen = o.session;
	t.iniPresent = 1;
	t.koRule = 0;
	t.tuningLoads = 4;
	t.warnings = 1;
	t.assistEnabled = 1;
	t.tagConfig = wire::kTagCfgTag | wire::kTagCfgPartner0 | wire::kTagCfgPartner1 | (o.session ? wire::kTagCfgFromHost : 0);
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

std::string MockDll::FrameName() const { return framering::MappingName(Pid()); }

// [game-view] the fake producer: one frame per 1/fps s into the ring (seqlock writer, like pchost's Present hook).
void MockDll::produce()
{
	using namespace framering;
	const uint32_t w = (uint32_t)m_o.frameW, h = (uint32_t)m_o.frameH, pitch = PitchFor(w);
	std::vector<uint8_t> scratch((size_t)pitch * h);
	uint32_t n = 0;
	const uint64_t start = NowMs();
	while (!m_quit) {
		++n;
		int mode;
		wire::MatchSetup inForce;
		{
			std::lock_guard<std::mutex> lk(m_fmx);
			mode = m_embedMode;
			inForce = m_ss.inForce;
			for (Held& hd : m_held) {
				if (hd.frames <= 0) continue;
				if (&hd == &m_held[0]) m_p1Offset += (hd.dir == 6 || hd.dir == 3 || hd.dir == 9) ? 4 * 128 : (hd.dir == 4 || hd.dir == 1 || hd.dir == 7) ? -4 * 128 : 0;
				if (--hd.frames == 0) { hd.dir = 5; hd.buttons = 0; }
			}
		}
		// §12.3 regrow: a new ring <name>-g<n>, the old one's kFlagProducerAlive cleared (Close clears it)
		if (m_o.regrowAfter > 0 && n == (uint32_t)m_o.regrowAfter + 1 && m_generation == 0) {
			auto* next = new Producer();
			std::string why;
			if (next->Create(FrameName() + "-g1", w, h, 3, 3, Pid(), "mock g1", &why)) {
				std::lock_guard<std::mutex> lk(m_fmx);
				Producer* old = (Producer*)m_ring;
				m_ring = next;
				m_generation = 1;
				old->Close();
				delete old;
			} else {
				delete next;
			}
		}
		Producer& ring = *(Producer*)m_ring;
		void* base = ring.Base();
		FrameRingHeader* rh = Header(base);
		rh->flags = kFlagProducerAlive | kFlagChecksums | (m_o.session ? 0 : kFlagInputInject) | (mode ? kFlagEmbedded : 0) |
		            (mode == 2 ? kFlagLayered : 0);
		uint32_t slot = 0;
		FrameSlotHeader* s = BeginWrite(base, slot);
		const uint64_t t0 = NowMs();
		s->frameSeq = n;
		s->gameFrame = 5400 + n;
		s->width = (uint16_t)w;
		s->height = (uint16_t)h;
		s->flags = kSlotHasFull | (mode == 2 ? kSlotLayered : 0);
		TestCamera(n, s->camera);
		s->camera.stageLightArgb = m_lightArgb;
		if (m_colorOverride) s->camera.stageColorValX1000 = m_colorValX1000;
		// the fighters as drawn: the setup's picks, walking, slot 0 moved by injected input
		for (int i = 0; i < 4; ++i) {
			FrameActor& a = s->actors[i];
			a = FrameActor{};
			a.exists = inForce.slot[i].chara >= 0 && (i < 2);   // partners are parked (not drawn) in this mock
			a.team = (uint8_t)(i & 1);
			a.facing = (uint8_t)(i & 1);
			a.x = (i == 0 ? -(80 + (int32_t)(n % 60u)) : (80 + (int32_t)(n % 60u))) * 128 + (i == 0 ? m_p1Offset : 0);
			a.y = 0;
			a.pattern = 0;
			a.frame = (int16_t)((n / 6u) % 8u);
		}
		uint8_t* full = AddLayer(base, s, kLayerFull, kFormatBGRX8, 0, (uint16_t)w, (uint16_t)h);
		DrawTestFull(full, w, h, pitch, n, scratch.data());
		s->layers[0].checksum = FrameChecksum(full, w, h, pitch);
		if (mode == 2) {
			if (uint8_t* ch = AddLayer(base, s, kLayerChars, kFormatBGRA8, kLayerPremultiplied, (uint16_t)w, (uint16_t)h)) {
				DrawTestChars(ch, w, h, pitch, s->camera, n);
				s->layers[1].checksum = FrameChecksum(ch, w, h, pitch);
			}
			if (uint8_t* hud = AddLayer(base, s, kLayerHud, kFormatBGRA8, kLayerPremultiplied, (uint16_t)w, (uint16_t)h)) {
				DrawTestHud(hud, w, h, pitch, n);
				s->layers[2].checksum = FrameChecksum(hud, w, h, pitch);
			}
		}
		s->presentMs = (uint32_t)NowMs();
		rh->lastCopyUs = (uint32_t)((NowMs() - t0) * 1000);
		Publish(base, slot, (uint32_t)NowMs());
		++m_framesProduced;
		// pace to fps against the start time (no drift)
		const uint64_t due = start + (uint64_t)n * 1000u / (uint64_t)(m_o.fps > 0 ? m_o.fps : 60);
		const uint64_t now = NowMs();
		if (due > now) Sleep((DWORD)(due - now));
	}
}

bool MockDll::Start()
{
	char name[96];
	std::snprintf(name, sizeof name, "\\\\.\\pipe\\povertycaster-link-%u", (unsigned)Pid());
	HANDLE h = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536,
	                            0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	m_pipe = h;
	if (m_o.frames) {
		auto* ring = new framering::Producer();
		std::string why;
		if (!ring->Create(FrameName(), (uint32_t)m_o.frameW, (uint32_t)m_o.frameH, 3, 3, Pid(), "mock", &why)) {
			delete ring;
			CloseHandle(h);
			m_pipe = nullptr;
			return false;
		}
		m_ring = ring;
		m_embedMode = m_o.layeredAtStart ? 2 : 0;
		m_producer = std::thread([this] { produce(); });
	}
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
	if (m_producer.joinable()) m_producer.join();
	delete (framering::Producer*)m_ring;
	m_ring = nullptr;
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
		serve(h);
		DisconnectNamedPipe(h);
	}
}

namespace {

struct TuningAnswer {
	wire::TuningGlobal g{};
	wire::TuningSlot s[4]{};
	std::string summary;
	uint16_t warnings = 0;
};

// The mock's "DLL side": re-read the fixture sidecars with Hantei-chan's own resolver (docs §2.4 / §9.1).
TuningAnswer ResolveForGame(const MockDll::Options& o, const wire::MatchSetup& inForce, bool battle, uint32_t loads)
{
	using namespace tagtune;
	TuningAnswer a;
	SidecarWorkspace ws;
	const bool open = !o.tagRoot.empty() && ws.Open(o.tagRoot);
	const bool sidecars = open && ws.Exists();
	const SidecarSet set = open ? ws.Set() : SidecarSet{};
	const GlobalResolution g = ResolveGlobal(set);
	const std::vector<Warning> w = AllSidecarWarnings(set);
	a.warnings = (uint16_t)w.size();
	wire::TuningGlobal& tg = a.g;
	tg.revision = 1;
	tg.source = (uint8_t)(o.session ? wire::TuningSource::Adopted : sidecars ? wire::TuningSource::Sidecars : wire::TuningSource::Defaults);
	tg.frozen = o.session;
	tg.leverCount = (uint8_t)kLeverCount;
	tg.leverTableHash = o.leverHash ? o.leverHash : LeverTableHash();
	tg.tuningLoads = loads;
	tg.warnings = a.warnings;
	tg.flags = (o.hotReloadPaused ? wire::kTunFlagHotReloadPaused : 0);
	tg.sessionFlags = SessionFlags(o);
	tg.charFiles = (uint16_t)set.CharFiles().size();
	CopyName(tg.activeStyle, sizeof tg.activeStyle, g.style.c_str());
	// a stable 15-hex-digit id of the resolved set (the real DLL prints the sha256 of the set / the adopted payload)
	uint32_t h1 = 2166136261u, h2 = 0x9E3779B9u;
	for (size_t i = 0; i < kLeverCount; ++i) { h1 = (h1 ^ (uint32_t)g.values[i]) * 16777619u; h2 = (h2 ^ (uint32_t)g.values[i]) * 2654435761u; }
	std::snprintf(tg.sha, sizeof tg.sha, "%08x%07x", (unsigned)h1, (unsigned)(h2 & 0xFFFFFFF));
	std::memcpy(tg.envMask, o.envMask, sizeof tg.envMask);
	for (size_t i = 0; i < kLeverCount; ++i) {
		tg.values[i] = kLevers[i].scope == LeverScope::CharOnly ? kLevers[i].def : g.values[i];
		if (g.from[i].src == Src::Tuning) wire::SetMaskBit(tg.tuningMask, (int)i);
		if (g.from[i].src == Src::Style) wire::SetMaskBit(tg.styleMask, (int)i);
	}
	int chars = 0;
	for (int slot = 0; slot < 4; ++slot) {
		wire::TuningSlot& ts = a.s[slot];
		ts.slot = (uint8_t)slot;
		const int chara = inForce.slot[slot].chara;
		ts.exists = battle && chara >= 0;
		ts.moon = ts.exists ? inForce.slot[slot].moon : 0xFF;
		if (!ts.exists) { for (size_t i = 0; i < kLeverCount; ++i) ts.values[i] = g.values[i]; continue; }
		const std::string file = FileOfChara(chara);
		CopyName(ts.file, sizeof ts.file, file.c_str());
		SlotResolution r = ResolveSlot(set, g, file, ts.moon);
		chars += r.hasCharFile || r.hasMoonFile;
		if (r.hasCharFile) ts.flags |= wire::kTunSlotHasCharFile;
		if (r.hasMoonFile) ts.flags |= wire::kTunSlotHasMoonSec;
		if (o.session) ts.flags |= wire::kTunSlotFromHost;
		for (size_t i = 0; i < kLeverCount; ++i) {
			if (r.from[i].src == Src::Char) wire::SetMaskBit(ts.charMask, (int)i);
			if (r.from[i].src == Src::Moon) wire::SetMaskBit(ts.moonMask, (int)i);
		}
		// the TAG CSS assist choices on the partner slots (TagCss.hpp applyAssistChoices)
		if (slot >= 2 && inForce.mode == (uint8_t)wire::Mode::Tag) {
			for (int d = 0; d < 5; ++d) {
				const uint8_t c = inForce.assist[slot - 2][d];
				if (!c || c > authoring::kAssistMotionCount) continue;
				const std::string dir = std::to_string(kAssistDirs[d]);
				int32_t packed = 0;
				PackMotion(authoring::kAssistMotionChoices[c - 1], packed);
				const int lm = FindLever("assist." + dir + ".motion"), lp = FindLever("assist." + dir + ".pattern"),
				          lc = FindLever("assist." + dir + ".command");
				r.values[lm] = packed; r.values[lp] = 0; r.values[lc] = -1;
				wire::SetMaskBit(ts.cssMask, lm); wire::SetMaskBit(ts.cssMask, lp); wire::SetMaskBit(ts.cssMask, lc);
				ts.flags |= wire::kTunSlotCssAssists;
			}
		}
		for (size_t i = 0; i < kLeverCount; ++i) ts.values[i] = r.values[i];
		if (slot == 0 && o.skewLever >= 0 && o.skewLever < (int)kLeverCount) ts.values[o.skewLever] += 1;
	}
	char b[116];
	std::snprintf(b, sizeof b, "%s: global + %d chars, style '%s', %u warnings, sha %s (mock)",
	              sidecars ? "sidecars" : "defaults", chars, g.style.c_str(), (unsigned)a.warnings, tg.sha);
	a.summary = b;
	return a;
}

bool SendTuning(HANDLE h, const TuningAnswer& a, uint8_t slotMask)
{
	if (!Send(h, wire::Kind::LinkTuningGlobal, &a.g, sizeof a.g)) return false;
	for (int s = 0; s < 4; ++s)
		if (!slotMask || (slotMask & (1u << s)))
			if (!Send(h, wire::Kind::LinkTuningSlot, &a.s[s], sizeof a.s[s])) return false;
	return true;
}

} // namespace

void MockDll::serve(void* pipe)
{
	HANDLE h = (HANDLE)pipe;
	std::vector<uint8_t> rx;
	for (;;) {
		if (m_quit) return;
		// 1. finish a running setup
		if (m_busySeq || m_busyPath) {
			if (NowMs() >= m_busyUntil) {
				wire::MatchSetup done = m_pending;
				if (done.stage == 0) done.stage = m_ss.inForce.stage ? m_ss.inForce.stage : 16;
				else if (done.stage < 0) done.stage = 7;   // the "random" roll
				{ std::lock_guard<std::mutex> lk(m_fmx); m_ss.inForce = done; }
				m_ss.phase = (uint8_t)wire::Phase::Battle;
				m_ss.authState = (uint8_t)wire::AuthState::Ready;
				m_ss.lastStatus = 0;
				m_ss.lastSeq = m_busySeq;
				m_ss.lastPath = m_busyPath;
				m_ss.lastDurationMs = (uint32_t)(m_busyPath == 1 ? m_o.hotMs : m_o.coldMs);
				m_ss.gameModeKind = done.mode == 1 ? 0x100 : done.mode == 2 ? 0x200 : (done.scene == 1 ? 0x1010 : 0x1);
				++m_ss.setupCount;
				CopyName(m_ss.message, sizeof m_ss.message, m_busyPath == 1 ? "ready (hot)" : "ready (cold)");
				++m_setups;
				if (m_busySeq && !ReplyTo(h, (uint16_t)wire::Op::SetMatchSetup, m_busySeq, wire::Status::Ok, m_busyMsg)) return;
				m_busySeq = 0;
				m_busyPath = 0;
			} else if (m_busyPath == 2) {
				const uint64_t left = m_busyUntil - NowMs();
				m_ss.phase = (uint8_t)(left > (uint64_t)m_o.coldMs / 2 ? wire::Phase::CharaSelect : wire::Phase::Loading);
				CopyName(m_ss.message, sizeof m_ss.message, left > (uint64_t)m_o.coldMs / 2 ? "committing the picks (mock)" : "loading (mock)");
			}
		}
		// 2. read what is there
		DWORD avail = 0;
		if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr)) return;
		if (avail) {
			uint8_t tmp[4096];
			DWORD rd = 0;
			if (!ReadFile(h, tmp, avail < sizeof tmp ? avail : (DWORD)sizeof tmp, &rd, nullptr) || !rd) return;
			rx.insert(rx.end(), tmp, tmp + rd);
		} else {
			Sleep(5);
			continue;
		}
		while (rx.size() >= sizeof(wire::Header)) {
			wire::Header hd;
			std::memcpy(&hd, rx.data(), sizeof hd);
			if (hd.size > wire::kLinkMaxPayload) return;
			if (rx.size() < sizeof hd + hd.size) break;
			std::vector<uint8_t> body(rx.begin() + sizeof hd, rx.begin() + sizeof hd + hd.size);
			rx.erase(rx.begin(), rx.begin() + sizeof hd + hd.size);
			bool ok = true;
			if (hd.kind == (uint16_t)wire::Kind::LinkCommandEx) {
				if (!m_o.authoring) { ++m_exDropped; continue; }   // an old DLL drops unknown kinds without a reply
				wire::CommandEx x{};
				if (body.size() < sizeof x) continue;
				std::memcpy(&x, body.data(), sizeof x);
				++m_commands;
				if (x.op == (uint16_t)wire::Op::InputInject || x.op == (uint16_t)wire::Op::SetStageLighting) {
					const bool inject = x.op == (uint16_t)wire::Op::InputInject;
					const size_t want = inject ? sizeof(wire::InputInject) : sizeof(wire::StageLighting);
					if (!m_o.frames) ok = ReplyTo(h, x.op, x.seq, wire::Status::Unknown, "unknown op");
					else if (x.size != want || body.size() != sizeof x + want) ok = ReplyTo(h, x.op, x.seq, wire::Status::BadArgs, "bad size (mock)");
					else if (m_o.session) ok = ReplyTo(h, x.op, x.seq, wire::Status::RefusedSession, "a netplay session is live (mock)");
					else if (inject) {
						wire::InputInject in;
						std::memcpy(&in, body.data() + sizeof x, sizeof in);
						if (in.version != wire::kInjectVersion || in.player > 3 || in.direction < 1 || in.direction > 9 || in.holdFrames > 600)
							ok = ReplyTo(h, x.op, x.seq, wire::Status::BadArgs, "bad inject (mock)");
						else if (in.player >= 2)
							ok = ReplyTo(h, x.op, x.seq, wire::Status::Unsupported, "P3/P4: only the P1/P2 pad words are owned (mock)");
						else if (in.player == 1 && m_ss.inForce.scene == (uint8_t)wire::Scene::Training)
							ok = ReplyTo(h, x.op, x.seq, wire::Status::Unsupported, "P2 is the Training dummy's (mock)");
						else {
							std::lock_guard<std::mutex> lk(m_fmx);
							Held& hd = m_held[in.player];
							if (in.flags & wire::kInjFlagRelease) hd = Held{};
							else { hd.dir = in.direction; hd.buttons = in.buttons; hd.frames = in.holdFrames ? in.holdFrames : 1; }
							++m_injects;
							char b[96];
							std::snprintf(b, sizeof b, "input #%u p%u dir %u btn 0x%02X hold %u (mock)", (unsigned)in.serial, in.player,
							              in.direction, in.buttons, in.holdFrames);
							ok = ReplyTo(h, x.op, x.seq, wire::Status::Ok, b);
						}
					} else {
						wire::StageLighting l;
						std::memcpy(&l, body.data() + sizeof x, sizeof l);
						{
							std::lock_guard<std::mutex> lk(m_fmx);   // persists until reset (§12.3)
							m_lightArgb = (l.flags & 1) ? 0 : l.lightArgb;
							m_colorOverride = !(l.flags & 1);
							m_colorValX1000 = l.stageColorValX1000;
						}
						ok = ReplyTo(h, x.op, x.seq, wire::Status::Ok, "stage lighting applied (mock)");
					}
					if (!ok) return;
					continue;
				}
				if (x.op != (uint16_t)wire::Op::SetMatchSetup || x.size != sizeof(wire::MatchSetup) || body.size() != sizeof x + x.size) {
					ok = ReplyTo(h, x.op, x.seq, x.op == (uint16_t)wire::Op::SetMatchSetup ? wire::Status::BadArgs : wire::Status::Unknown,
					             "bad LinkCommandEx (mock)");
					if (!ok) return;
					continue;
				}
				wire::MatchSetup ms;
				std::memcpy(&ms, body.data() + sizeof x, sizeof ms);
				bool reservedClean = ms.dummy[0] == 0 && ms.dummy[1] == 0;
				for (uint8_t b : ms._reserved) reservedClean = reservedClean && b == 0;
				auto refuse = [&](wire::Status st, const std::string& m) {
					m_ss.lastStatus = (int16_t)st;
					m_ss.lastSeq = x.seq;
					return ReplyTo(h, x.op, x.seq, st, m);
				};
				if (ms.version != wire::kMatchSetupVersion || !reservedClean) { ok = refuse(wire::Status::BadArgs, "version / reserved bytes (mock)"); }
				else if (m_o.session) { ok = refuse(wire::Status::RefusedSession, "a netplay session is live (mock)"); }
				else if (m_busySeq || m_busyPath) { ok = refuse(wire::Status::Busy, "another SetMatchSetup is running (mock)"); }
				else {
					const authoring::Setup s = authoring::FromWire(ms);
					authoring::ValidateContext vc;
					vc.team4p = m_o.team4p;
					const std::vector<authoring::Problem> probs = authoring::Validate(s, authoring::MirrorRoster(true), vc);
					if (!probs.empty()) { ok = refuse((wire::Status)probs.front().status, probs.front().msg + " (mock)"); }
					else {
						const bool hot = m_ss.phase == (uint8_t)wire::Phase::Battle && m_ss.authState == (uint8_t)wire::AuthState::Ready &&
						                 ms.mode == m_ss.inForce.mode && ms.scene == m_ss.inForce.scene;
						if (!hot && (ms.flags & wire::kSetupHotOnly)) { ok = refuse(wire::Status::Unsupported, "hot only, but this needs the cold path (mock)"); }
						else {
							if (ms.flags & wire::kSetupTuningFirst) ++m_tuningLoads;
							m_ss.requested = ms;
							m_pending = ms;
							m_busySeq = x.seq;
							m_busyPath = hot ? 1 : 2;
							m_busyUntil = NowMs() + (uint64_t)(hot ? m_o.hotMs : m_o.coldMs);
							m_ss.authState = (uint8_t)(hot ? wire::AuthState::ApplyingHot : wire::AuthState::Rebuilding);
							m_ss.lastStatus = 0;
							int picks = 0;
							for (int i = 0; i < 4; ++i)
								picks += std::memcmp(&ms.slot[i], &m_ss.inForce.slot[i], sizeof ms.slot[i]) != 0;
							char b[116];
							if (hot) std::snprintf(b, sizeof b, "hot: %d picks, stage %d -> %d, reload %d ms (mock)", picks, m_ss.inForce.stage, ms.stage, m_o.hotMs);
							else std::snprintf(b, sizeof b, "cold: %s, rebuilt through the CSS in %d ms (mock)", ms.mode == 0 ? "1v1" : ms.mode == 1 ? "TAG" : "TEAM", m_o.coldMs);
							m_busyMsg = b;
							CopyName(m_ss.message, sizeof m_ss.message, hot ? "applying (hot)" : "leaving to the character select");
						}
					}
				}
				if (!ok) return;
				continue;
			}
			if (hd.kind != (uint16_t)wire::Kind::LinkCommand || hd.size != sizeof(wire::Command)) continue;
			wire::Command c;
			std::memcpy(&c, body.data(), sizeof c);
			++m_commands;
			const bool battle = m_ss.phase == (uint8_t)wire::Phase::Battle;
			switch ((wire::Op)c.op) {
			case wire::Op::Ping: ok = Reply(h, c, wire::Status::Ok, "pong (mock)"); break;
			case wire::Op::QueryState: { const wire::State s = state(); ok = Send(h, wire::Kind::LinkState, &s, sizeof s); break; }
			case wire::Op::QueryTag:
				if (m_o.tagOps) { const wire::Tag t = FakeTag(m_o); ok = Send(h, wire::Kind::LinkTag, &t, sizeof t); }
				else ok = Reply(h, c, wire::Status::Unknown, "unknown op");
				break;
			case wire::Op::SetChar:
				if (c.chara == -1 && c.moon == -1 && c.palette == -1 && !(c.flags & wire::kFlagReload)) ++m_probes;
				if (m_o.session) ok = Reply(h, c, wire::Status::RefusedSession, "a netplay/rollback session is live (mock)");
				else if (!battle) ok = Reply(h, c, wire::Status::RefusedScene, "not in a battle (mock)");
				else ok = Reply(h, c, wire::Status::Ok, "held until the next reload");
				break;
			case wire::Op::Reload:
				ok = Reply(h, c, m_o.session ? wire::Status::RefusedSession : wire::Status::Ok, m_o.session ? "session (mock)" : "reload done (mock)");
				break;
			case wire::Op::QueryCaps: {
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				wire::Caps k{};
				k.revision = 1;
				k.caps = wire::kCapTag | wire::kCapRoster | wire::kCapSetup | wire::kCapTuning | wire::kCapSidecars |
				         wire::kCapTrainingScene | (m_o.team4p ? wire::kCapTeam4P : 0) |
				         (m_o.frames ? wire::kCapFrameShare | wire::kCapInputInject : 0);
				k.leverTableHash = m_o.leverHash ? m_o.leverHash : tagtune::LeverTableHash();
				k.leverCount = (uint8_t)tagtune::kLeverCount;
				k.perCharLeverCount = (uint8_t)tagtune::PerCharLeverCount();
				k.matchSetupVersion = wire::kMatchSetupVersion;
				k.tuningWireVersion = 3;
				CopyName(k.build, sizeof k.build, "mock");
				k.tuningSource = (uint8_t)(m_o.session ? wire::TuningSource::Adopted : wire::TuningSource::Sidecars);
				CopyName(k.gameId, sizeof k.gameId, "mbaacc");
				ok = Send(h, wire::Kind::LinkCaps, &k, sizeof k);
				break;
			}
			case wire::Op::QueryRoster: {
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				const std::vector<authoring::RosterChar> r = authoring::MirrorRoster(true);
				const int pages = ((int)r.size() + 15) / 16;
				if (c.slot < 0 || c.slot >= pages) { ok = Reply(h, c, wire::Status::BadArgs, "no such roster page (mock)"); break; }
				wire::Roster p{};
				p.page = (uint8_t)c.slot; p.pageCount = (uint8_t)pages; p.total = (uint8_t)r.size(); p.modDataLoaded = 1;
				for (int i = 0; i < 16 && c.slot * 16 + i < (int)r.size(); ++i) {
					const authoring::RosterChar& rc = r[c.slot * 16 + i];
					wire::RosterEntry& e = p.e[i];
					e.chara = (int16_t)rc.chara; e.selector = (uint8_t)rc.selector; e.flags = rc.flags;
					CopyName(e.file1, sizeof e.file1, rc.file1.c_str());
					CopyName(e.file2, sizeof e.file2, rc.file2.c_str());
					CopyName(e.name, sizeof e.name, rc.name.c_str());
					++p.count;
				}
				ok = Send(h, wire::Kind::LinkRoster, &p, sizeof p);
				break;
			}
			case wire::Op::QueryMatchSetup: {
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				wire::SetupState s = m_ss;
				if (s.phase != (uint8_t)wire::Phase::Battle) std::memset(&s.inForce, 0, sizeof s.inForce);
				ok = Send(h, wire::Kind::LinkSetupState, &s, sizeof s);
				break;
			}
			case wire::Op::EndAuthoring:
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				if (m_o.session) { ok = Reply(h, c, wire::Status::RefusedSession, "session (mock)"); break; }
				m_ss.authState = (uint8_t)wire::AuthState::Idle;
				CopyName(m_ss.message, sizeof m_ss.message, "authoring ended: a normal offline match");
				ok = Reply(h, c, wire::Status::Ok, "authoring pins cleared (mock)");
				break;
			case wire::Op::ApplyTuning: {
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				if (m_o.session && !(m_o.role == 1 && m_o.betweenRounds)) {
					ok = Reply(h, c, wire::Status::RefusedSession, "session: the game plays the host's tuning (mock)");
					break;
				}
				++m_applies;
				++m_tuningLoads;
				const TuningAnswer a = ResolveForGame(m_o, m_ss.inForce, battle, m_tuningLoads);
				std::string msg = m_o.session ? "queued for the next round (mock host)" : a.summary;
				if ((c.flags & wire::kFlagReload) && !battle) msg += "; reload skipped (not in battle)";
				ok = ReplyTo(h, c.op, c.seq, wire::Status::Ok, msg);
				if (ok && (c.flags & wire::kFlagQueryAfter)) ok = SendTuning(h, a, 0);
				break;
			}
			case wire::Op::QueryTuning: {
				if (!m_o.authoring) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				if (!m_tuningLoads) ++m_tuningLoads;
				ok = SendTuning(h, ResolveForGame(m_o, m_ss.inForce, battle, m_tuningLoads), c.slotMask);
				break;
			}
			case wire::Op::QueryFrameShare: {
				if (!m_o.frames) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				std::lock_guard<std::mutex> lk(m_fmx);
				const framering::Producer& ring = *(framering::Producer*)m_ring;
				const framering::FrameRingHeader* rh = framering::Header(ring.Base());
				wire::FrameShare f{};
				f.version = framering::kVersion;
				f.slotCount = (uint8_t)rh->slotCount;
				f.layerCapacity = (uint8_t)rh->layerCapacity;
				f.maxWidth = rh->maxWidth; f.maxHeight = rh->maxHeight;
				f.ringBytes = (uint32_t)ring.Bytes();
				f.flags = rh->flags;
				f.width = (uint32_t)m_o.frameW; f.height = (uint32_t)m_o.frameH;
				f.framesPublished = rh->framesPublished;
				CopyName(f.name, sizeof f.name, ring.Name().c_str());
				ok = Send(h, wire::Kind::LinkFrameShare, &f, sizeof f);
				break;
			}
			case wire::Op::SetEmbedded:
				if (!m_o.frames) { ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break; }
				if (c.slot < 0 || c.slot > 2) { ok = Reply(h, c, wire::Status::BadArgs, "embedded mode 0..2 (mock)"); break; }
				if (c.slot == 2 && m_o.layeredUnsupported) { ok = Reply(h, c, wire::Status::Unsupported, "layered capture: use mode 1 (mock)"); break; }
				{ std::lock_guard<std::mutex> lk(m_fmx); m_embedMode = c.slot; }
				ok = Reply(h, c, wire::Status::Ok, c.slot == 0 ? "real window shown (mock)" : c.slot == 1 ? "embedded: full frame (mock)"
				                                                                                   : "embedded: layered (mock)");
				break;
			default: ok = Reply(h, c, wire::Status::Unknown, "unknown op"); break;
			}
			if (!ok) return;
		}
	}
}

} // namespace gamelink
