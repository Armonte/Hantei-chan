// Game Link client — see game_link.h and docs/HANTEI_GAME_LINK.md.
#include "game_link.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
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

Client::Client() { m_thread = std::thread([this] { run(); }); }

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

void Client::SetPollHz(int hz) { std::lock_guard<std::mutex> lk(m_mx); m_pollHz = hz; }
void Client::SetAutoReload(bool on) { std::lock_guard<std::mutex> lk(m_mx); m_snap.autoReload = on; }

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

// ---- worker ----------------------------------------------------------------------------------------------------

bool Client::tryOpen()
{
	const std::vector<uint32_t> pids = FindGamePids();
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
		note(std::string("connected to ") + name);
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
	{
		std::lock_guard<std::mutex> lk(m_mx);
		out.swap(m_out);
		pollHz = m_pollHz;
	}
	const uint64_t now = NowMs();
	if (pollHz > 0 && now - m_lastPollMs >= (uint64_t)(1000 / pollHz)) {
		m_lastPollMs = now;
		wire::Command q{}; q.op = (uint16_t)wire::Op::QueryState;
		out.push_back(q);
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
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
}

} // namespace gamelink
