// Game Link client — see game_link.h and docs/HANTEI_GAME_LINK.md.
#include "game_link.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gamelink {
namespace {

uint64_t NowMs() { return GetTickCount64(); }

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

// "C:\x\data\akiha_0_c.txt" -> "akiha_0_c"
std::string Stem(const std::string& path)
{
	size_t b = path.find_last_of("\\/");
	std::string f = b == std::string::npos ? path : path.substr(b + 1);
	size_t d = f.find_last_of('.');
	if (d != std::string::npos) f = f.substr(0, d);
	return Lower(f);
}

const char* OpName(uint16_t op)
{
	switch ((wire::Op)op) {
	case wire::Op::Ping: return "ping";
	case wire::Op::Reload: return "reload";
	case wire::Op::SetChar: return "setchar";
	case wire::Op::QueryState: return "state";
	case wire::Op::SetStage: return "setstage";
	case wire::Op::ReloadStage: return "reloadstage";
	case wire::Op::QueryStage: return "stage";
	case wire::Op::QueryTag: return "tag";
	}
	return "?";
}

} // namespace

uint8_t SlotMaskForFile(const std::string& key, const wire::State& st)
{
	const std::string stem = Stem(key);
	size_t best = 0;
	uint8_t mask = 0;
	for (int s = 0; s < 4; ++s) {
		const wire::Actor& a = st.actors[s];
		if (!a.exists || !a.file[0]) continue;
		std::string name = Lower(std::string(a.file, strnlen(a.file, sizeof a.file)));
		const bool hit = stem == name || (stem.size() > name.size() && stem.compare(0, name.size(), name) == 0 &&
		                                  stem[name.size()] == '_');
		if (!hit) continue;
		if (name.size() > best) { best = name.size(); mask = 0; }
		if (name.size() == best) mask |= (uint8_t)(1u << s);
	}
	return mask;
}

std::string StageStemOf(const std::string& path)
{
	std::string stem = Stem(path);
	if (stem == "bglist") return {};
	for (const char* suf : { "info", "light", "_s" }) {
		const size_t n = std::strlen(suf);
		if (stem.size() > n && stem.compare(stem.size() - n, n, suf) == 0) return stem.substr(0, stem.size() - n);
	}
	return stem;
}

bool StageFileMatches(const std::string& path, const wire::Stage& st, bool* needsList)
{
	if (needsList) *needsList = false;
	if (st.loaded < 1 || !st.dataFile[0]) return false;
	const std::string stem = Stem(path);
	if (stem == "bglist") { if (needsList) *needsList = true; return true; }
	const std::string data = Lower(std::string(st.dataFile, strnlen(st.dataFile, sizeof st.dataFile)));
	return stem == data || stem == data + "info" || stem == data + "light" || stem == data + "_s";
}

std::vector<uint32_t> FindGamePids()
{
	std::vector<uint32_t> out;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return out;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof pe;
	for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
		if (_wcsicmp(pe.szExeFile, L"MBAA.exe") == 0) out.push_back(pe.th32ProcessID);
	CloseHandle(snap);
	return out;
}

Client::Client()
{
	if (const char* e = std::getenv("HANTEI_GAME_LINK_PID")) m_snap.targetPid = (uint32_t)std::strtoul(e, nullptr, 0);
	m_thread = std::thread([this] { run(); });
}

Client::~Client()
{
	m_quit = true;
	if (m_thread.joinable()) m_thread.join();
	closePipe(nullptr);
}

void Client::note(const std::string& line)
{
	// caller holds m_mx
	m_log.push_back(line);
	if (m_log.size() > 200) m_log.erase(m_log.begin(), m_log.begin() + 50);
}

void Client::Connect()
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_snap.wantConnected = true;
	m_nextOpenMs = 0;
}

void Client::Disconnect()
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_snap.wantConnected = false;
	m_onConnect.clear();
}

void Client::SetTargetPid(uint32_t pid)
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_snap.targetPid = pid;
}

uint16_t Client::queueCommand(wire::Command c)
{
	std::lock_guard<std::mutex> lk(m_mx);
	c.seq = ++m_seq;
	m_out.push_back(c);
	return c.seq;
}

uint16_t Client::Ping()
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::Ping;
	return queueCommand(c);
}

uint16_t Client::Reload(uint8_t slotMask, uint8_t flags)
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::Reload; c.slotMask = slotMask; c.flags = flags;
	return queueCommand(c);
}

uint16_t Client::SetChar(int slot, int chara, int moon, int palette, uint8_t flags)
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::SetChar; c.slot = slot; c.chara = chara; c.moon = moon;
	c.palette = palette; c.flags = flags;
	return queueCommand(c);
}

uint16_t Client::SetStage(int stageId, uint8_t flags)
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::SetStage; c.slot = stageId; c.flags = flags;
	return queueCommand(c);
}

uint16_t Client::ReloadStage(uint8_t flags)
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::ReloadStage; c.flags = flags;
	return queueCommand(c);
}

uint16_t Client::SetStageWhenConnected(int stageId, uint8_t flags)
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::SetStage; c.slot = stageId; c.flags = flags;
	std::lock_guard<std::mutex> lk(m_mx);
	c.seq = ++m_seq;
	if (m_snap.connected) m_out.push_back(c);
	else {
		m_onConnect.clear();          // only the latest "show this stage" matters
		m_onConnect.push_back(c);
		m_snap.wantConnected = true;
		m_nextOpenMs = 0;
		note("stage " + std::to_string(stageId) + ": connecting first, will switch once the link is up");
	}
	return c.seq;
}

void Client::SetPollHz(int hz) { std::lock_guard<std::mutex> lk(m_mx); m_pollHz = hz; }
void Client::SetAutoReload(bool on) { std::lock_guard<std::mutex> lk(m_mx); m_snap.autoReload = on; }

void Client::SetAutoReloadStage(bool on) { std::lock_guard<std::mutex> lk(m_mx); m_snap.autoReloadStage = on; }

void Client::SetWatchedStageFiles(std::vector<WatchedStageFile> files)
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_stageWatch = std::move(files);
	m_snap.watchedStage = m_stageWatch.size();
}

void Client::SetWatchedFiles(std::vector<WatchedFile> files)
{
	std::lock_guard<std::mutex> lk(m_mx);
	m_watch = std::move(files);
	m_snap.watched = m_watch.size();
}

Snapshot Client::Get() const
{
	std::lock_guard<std::mutex> lk(m_mx);
	Snapshot s = m_snap;
	s.stateAgeMs = m_lastStateMs ? (uint32_t)(NowMs() - m_lastStateMs) : 0;
	return s;
}

std::vector<std::string> Client::RecentLog() const
{
	std::lock_guard<std::mutex> lk(m_mx);
	return m_log;
}

void Client::SetTagQuery(bool on) { std::lock_guard<std::mutex> lk(m_mx); m_tagQuery = on; }

uint16_t Client::ProbeGate()
{
	wire::Command c{}; c.op = (uint16_t)wire::Op::SetChar; c.slot = 0; c.chara = -1; c.moon = -1; c.palette = -1;
	return queueCommand(c);
}

bool Client::PeekReply(uint16_t seq, wire::Reply& out) const
{
	std::lock_guard<std::mutex> lk(m_mx);
	for (const auto& r : m_replies)
		if (r.seq == seq) { out = r; return true; }
	return false;
}

bool Client::WaitReply(uint16_t seq, int timeoutMs, wire::Reply& out)
{
	const uint64_t end = NowMs() + (uint64_t)timeoutMs;
	while (NowMs() < end) {
		{
			std::lock_guard<std::mutex> lk(m_mx);
			for (const auto& r : m_replies)
				if (r.seq == seq) { out = r; return true; }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

bool Client::WaitState(int timeoutMs, wire::State& out)
{
	uint32_t start;
	{ std::lock_guard<std::mutex> lk(m_mx); start = m_stateSerial; }
	const uint64_t end = NowMs() + (uint64_t)timeoutMs;
	while (NowMs() < end) {
		{
			std::lock_guard<std::mutex> lk(m_mx);
			if (m_stateSerial != start) { out = m_snap.state; return true; }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

bool Client::WaitStage(int timeoutMs, wire::Stage& out)
{
	uint32_t start;
	{ std::lock_guard<std::mutex> lk(m_mx); start = m_stageSerial; }
	const uint64_t end = NowMs() + (uint64_t)timeoutMs;
	while (NowMs() < end) {
		{
			std::lock_guard<std::mutex> lk(m_mx);
			if (m_stageSerial != start) { out = m_snap.stage; return true; }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

// ---- worker ----------------------------------------------------------------------------------------------------

bool Client::tryOpen()
{
	uint32_t target;
	{ std::lock_guard<std::mutex> lk(m_mx); target = m_snap.targetPid; }
	// A pinned pid is the ONLY process tried: no discovery (another game may be running next to it).
	const std::vector<uint32_t> pids = target ? std::vector<uint32_t>{ target } : FindGamePids();
	std::string why = pids.empty() ? "MBAA.exe is not running" : "";
	for (uint32_t pid : pids) {
		char name[96];
		std::snprintf(name, sizeof name, "\\\\.\\pipe\\povertycaster-link-%u", (unsigned)pid);
		HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (h == INVALID_HANDLE_VALUE) {
			const DWORD e = GetLastError();
			if (e == ERROR_PIPE_BUSY) why = "MBAA.exe pid " + std::to_string(pid) + ": another editor holds the link";
			else why = "MBAA.exe pid " + std::to_string(pid) + " serves no link pipe (launch it with PCHOST_MBAACC_LINK=1)";
			continue;
		}
		DWORD mode = PIPE_READMODE_BYTE;
		SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
		m_pipe = h;
		m_rx.clear();
		std::lock_guard<std::mutex> lk(m_mx);
		m_snap.connected = true;
		m_snap.pid = pid;
		m_snap.pipe = name;
		m_snap.status = "connected";
		m_snap.stageUnsupported = false;
		m_snap.tagUnsupported = false;
		note(std::string("connected to ") + name);
		for (const wire::Command& c : m_onConnect) m_out.push_back(c);   // SetStageWhenConnected
		m_onConnect.clear();
		return true;
	}
	std::lock_guard<std::mutex> lk(m_mx);
	if (m_snap.status != why) { m_snap.status = why; note(why); }
	return false;
}

void Client::closePipe(const char* why)
{
	if (m_pipe) { CloseHandle((HANDLE)m_pipe); m_pipe = nullptr; }
	if (!why) return;
	std::lock_guard<std::mutex> lk(m_mx);
	if (m_snap.connected) note(std::string("disconnected: ") + why);
	m_snap.connected = false;
	m_snap.status = why;
	m_snap.haveState = false;
	m_snap.haveStage = false;
	m_snap.haveTag = false;
}

void Client::handleMessage(const wire::Header& h, const uint8_t* body)
{
	std::lock_guard<std::mutex> lk(m_mx);
	if (h.kind == (uint16_t)wire::Kind::LinkState && h.size == sizeof(wire::State)) {
		std::memcpy(&m_snap.state, body, sizeof(wire::State));
		m_snap.haveState = true;
		m_snap.gameReloads = m_snap.state.reloadCount;
		m_lastStateMs = NowMs();
		++m_stateSerial;
	} else if (h.kind == (uint16_t)wire::Kind::LinkStage && h.size == sizeof(wire::Stage)) {
		std::memcpy(&m_snap.stage, body, sizeof(wire::Stage));
		m_snap.stage.dataFile[sizeof m_snap.stage.dataFile - 1] = 0;
		m_snap.haveStage = true;
		++m_stageSerial;
	} else if (h.kind == (uint16_t)wire::Kind::LinkTag && h.size == sizeof(wire::Tag)) {
		std::memcpy(&m_snap.tag, body, sizeof(wire::Tag));
		m_snap.tag.activeStyle[sizeof m_snap.tag.activeStyle - 1] = 0;
		m_snap.tag.sha[sizeof m_snap.tag.sha - 1] = 0;
		m_snap.haveTag = true;
	} else if (h.kind == (uint16_t)wire::Kind::LinkTag) {
		note("dropped a LinkTag of " + std::to_string(h.size) + " bytes (this editor expects " +
		     std::to_string(sizeof(wire::Tag)) + ": protocol skew)");
	} else if (h.kind == (uint16_t)wire::Kind::LinkReply && h.size == sizeof(wire::Reply)) {
		wire::Reply r;
		std::memcpy(&r, body, sizeof r);
		r.message[sizeof r.message - 1] = 0;
		m_replies.push_back(r);
		if (m_replies.size() > 32) m_replies.erase(m_replies.begin());
		m_snap.gameReloads = r.reloadCount;
		char line[200];
		std::snprintf(line, sizeof line, "%s #%u: %s - %s", OpName(r.op), (unsigned)r.seq, wire::StatusName(r.status),
		              r.message);
		m_snap.lastReply = line;
		if (r.op == (uint16_t)wire::Op::QueryStage && r.status == (int16_t)wire::Status::Unknown) {
			if (!m_snap.stageUnsupported) note("the game's pchost.dll predates the stage ops (QueryStage unknown)");
			m_snap.stageUnsupported = true;
			return;
		}
		if (r.op == (uint16_t)wire::Op::QueryTag && r.status == (int16_t)wire::Status::Unknown) {
			if (!m_snap.tagUnsupported) note("the game's pchost.dll has no QueryTag (older than PovertyCaster mbaacc/link-tag) - using LinkState");
			m_snap.tagUnsupported = true;
			return;
		}
		if (r.op != (uint16_t)wire::Op::Ping) note(line);
	} else {
		note("dropped an unknown message kind " + std::to_string(h.kind));
	}
}

void Client::pump()
{
	// 1. writes
	std::deque<wire::Command> out;
	int pollHz;
	bool stageOps, tagOps;
	{
		std::lock_guard<std::mutex> lk(m_mx);
		out.swap(m_out);
		pollHz = m_pollHz;
		stageOps = !m_snap.stageUnsupported;
		tagOps = m_tagQuery && !m_snap.tagUnsupported;
	}
	const uint64_t now = NowMs();
	if (pollHz > 0 && now - m_lastPollMs >= (uint64_t)(1000 / pollHz)) {
		m_lastPollMs = now;
		wire::Command q{}; q.op = (uint16_t)wire::Op::QueryState;
		out.push_back(q);
		if (stageOps) { wire::Command qs{}; qs.op = (uint16_t)wire::Op::QueryStage; out.push_back(qs); }
		if (tagOps) { wire::Command qt{}; qt.op = (uint16_t)wire::Op::QueryTag; out.push_back(qt); }
	}
	for (const wire::Command& c : out) {
		const wire::Header h{ (uint16_t)wire::Kind::LinkCommand, wire::kLinkVersion, sizeof c };
		uint8_t buf[sizeof h + sizeof c];
		std::memcpy(buf, &h, sizeof h);
		std::memcpy(buf + sizeof h, &c, sizeof c);
		DWORD wr = 0;
		if (!WriteFile((HANDLE)m_pipe, buf, sizeof buf, &wr, nullptr) || wr != sizeof buf) {
			closePipe("write failed (game closed?)");
			return;
		}
	}
	// 2. reads — PeekNamedPipe first, so the worker never blocks in ReadFile
	for (;;) {
		DWORD avail = 0;
		if (!PeekNamedPipe((HANDLE)m_pipe, nullptr, 0, nullptr, &avail, nullptr)) { closePipe("pipe closed (game exited?)"); return; }
		if (!avail) break;
		uint8_t tmp[4096];
		DWORD rd = 0;
		if (!ReadFile((HANDLE)m_pipe, tmp, avail < sizeof tmp ? avail : (DWORD)sizeof tmp, &rd, nullptr) || !rd) {
			closePipe("read failed"); return;
		}
		m_rx.insert(m_rx.end(), tmp, tmp + rd);
	}
	while (m_rx.size() >= sizeof(wire::Header)) {
		wire::Header h;
		std::memcpy(&h, m_rx.data(), sizeof h);
		if (h.size > 1024) { closePipe("framing error"); return; }
		if (m_rx.size() < sizeof h + h.size) break;
		handleMessage(h, m_rx.data() + sizeof h);
		m_rx.erase(m_rx.begin(), m_rx.begin() + sizeof h + h.size);
	}
}

void Client::watchTick()
{
	const uint64_t now = NowMs();
	if (now - m_lastWatchMs < 250) return;
	m_lastWatchMs = now;
	std::vector<WatchedFile> watch;
	bool autoReload, connected;
	wire::State st{};
	bool haveState;
	{
		std::lock_guard<std::mutex> lk(m_mx);
		watch = m_watch;
		autoReload = m_snap.autoReload;
		connected = m_snap.connected;
		st = m_snap.state;
		haveState = m_snap.haveState;
	}
	// keep stamps in step with the watch list (a newly opened character starts from its current stamp)
	std::vector<FileStamp> next;
	for (const WatchedFile& w : watch) {
		auto it = std::find_if(m_stamps.begin(), m_stamps.end(), [&](const FileStamp& f) { return f.path == w.path; });
		FileStamp f = it != m_stamps.end() ? *it : FileStamp{ w.path, w.key };
		f.key = w.key;
		WIN32_FILE_ATTRIBUTE_DATA fa{};
		if (GetFileAttributesExA(w.path.c_str(), GetFileExInfoStandard, &fa)) {
			const uint64_t t = ((uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
			const uint64_t sz = ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
			if (it == m_stamps.end()) { f.time = t; f.size = sz; }
			else if (t != f.time || sz != f.size) { f.time = t; f.size = sz; f.changedMs = now; f.pending = true; }
		}
		next.push_back(f);
	}
	m_stamps.swap(next);
	// settle: 400 ms with no further change, then one reload for everything that settled together
	uint8_t mask = 0;
	std::string names;
	bool any = false;
	for (FileStamp& f : m_stamps) {
		if (!f.pending || now - f.changedMs < 400) continue;
		f.pending = false;
		any = true;
		const uint8_t m = haveState ? SlotMaskForFile(f.key, st) : 0;
		mask |= m;
		if (!names.empty()) names += ", ";
		names += Stem(f.path);
	}
	if (!any) return;
	std::string what;
	if (!autoReload) what = "auto-reload is off";
	else if (!connected) what = "not connected";
	else if (!mask) what = "no game slot uses it - not reloading";
	else {
		wire::Command c{}; c.op = (uint16_t)wire::Op::Reload; c.slotMask = mask;
		const uint16_t seq = queueCommand(c);
		char b[64];
		std::snprintf(b, sizeof b, "sent reload #%u (slot mask 0x%X)", (unsigned)seq, (unsigned)mask);
		what = b;
	}
	std::lock_guard<std::mutex> lk(m_mx);
	m_snap.lastChange = "saved " + names + ": " + what;
	note(m_snap.lastChange);
}

// The stage twin of watchTick: same stamp/settle rule, one ReloadStage for everything that settled together, and
// only when a saved file belongs to the stage the game is showing (StageFileMatches).
void Client::stageWatchTick(uint64_t now)
{
	std::vector<WatchedStageFile> watch;
	bool autoReload, connected, haveStage;
	wire::Stage st{};
	{
		std::lock_guard<std::mutex> lk(m_mx);
		watch = m_stageWatch;
		autoReload = m_snap.autoReloadStage;
		connected = m_snap.connected;
		haveStage = m_snap.haveStage;
		st = m_snap.stage;
	}
	std::vector<StageStamp> next;
	for (const WatchedStageFile& w : watch) {
		auto it = std::find_if(m_stageStamps.begin(), m_stageStamps.end(), [&](const StageStamp& f) { return f.path == w.path; });
		StageStamp f = it != m_stageStamps.end() ? *it : StageStamp{ w.path, w.kind };
		WIN32_FILE_ATTRIBUTE_DATA fa{};
		if (GetFileAttributesExA(w.path.c_str(), GetFileExInfoStandard, &fa)) {
			const uint64_t t = ((uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
			const uint64_t sz = ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
			if (it == m_stageStamps.end()) { f.time = t; f.size = sz; }
			else if (t != f.time || sz != f.size) { f.time = t; f.size = sz; f.changedMs = now; f.pending = true; }
		}
		next.push_back(f);
	}
	m_stageStamps.swap(next);
	bool any = false, match = false, list = false;
	std::string names;
	for (StageStamp& f : m_stageStamps) {
		if (!f.pending || now - f.changedMs < 400) continue;
		f.pending = false;
		any = true;
		bool needsList = false;
		if (haveStage && StageFileMatches(f.path, st, &needsList)) { match = true; list |= needsList; }
		if (!names.empty()) names += ", ";
		names += Stem(f.path);
	}
	if (!any) return;
	std::string what;
	if (!autoReload) what = "stage auto-reload is off";
	else if (!connected) what = "not connected";
	else if (!haveStage) what = "the game reports no stage (old pchost.dll?)";
	else if (!match) what = std::string("the game shows ") + (st.dataFile[0] ? st.dataFile : "no stage") + " - not reloading";
	else {
		const uint16_t seq = ReloadStage(list ? wire::kFlagStageList : 0);
		char b[80];
		std::snprintf(b, sizeof b, "sent reloadstage #%u%s", (unsigned)seq, list ? " (+BgList.ini)" : "");
		what = b;
	}
	std::lock_guard<std::mutex> lk(m_mx);
	m_snap.lastChange = "saved " + names + ": " + what;
	note(m_snap.lastChange);
}

void Client::run()
{
	while (!m_quit) {
		bool want;
		{ std::lock_guard<std::mutex> lk(m_mx); want = m_snap.wantConnected; }
		if (!want && m_pipe) closePipe("disconnected by user");
		if (want && !m_pipe && NowMs() >= m_nextOpenMs) {
			if (!tryOpen()) m_nextOpenMs = NowMs() + 1000;
		}
		if (m_pipe) pump();
		else { std::lock_guard<std::mutex> lk(m_mx); m_out.clear(); }
		watchTick();
		{
			const uint64_t now = NowMs();
			if (now - m_lastStageWatchMs >= 250) { m_lastStageWatchMs = now; stageWatchTick(now); }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
}

} // namespace gamelink
