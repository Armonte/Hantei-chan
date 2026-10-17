// [game-view] frame ring mapping — see game_frame_ring.h.
#include "game_frame_ring.h"

#include <windows.h>
#include <sddl.h>

#include <cstdio>

namespace framering {

std::string MappingName(uint32_t pid)
{
	char b[64];
	std::snprintf(b, sizeof b, "Local\\povertycaster-frames-%u", (unsigned)pid);
	return b;
}

namespace {
std::wstring W(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// A security descriptor granting only the current user (the §12 "current-user DACL"). Caller LocalFree()s it.
PSECURITY_DESCRIPTOR CurrentUserOnly()
{
	HANDLE tok = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return nullptr;
	DWORD n = 0;
	GetTokenInformation(tok, TokenUser, nullptr, 0, &n);
	std::vector<uint8_t> buf(n);
	PSECURITY_DESCRIPTOR sd = nullptr;
	if (GetTokenInformation(tok, TokenUser, buf.data(), n, &n)) {
		LPWSTR sid = nullptr;
		if (ConvertSidToStringSidW(((TOKEN_USER*)buf.data())->User.Sid, &sid)) {
			const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
			ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr);
			LocalFree(sid);
		}
	}
	CloseHandle(tok);
	return sd;
}
} // namespace

Producer::~Producer() { Close(); }

bool Producer::Create(const std::string& name, uint32_t maxW, uint32_t maxH, uint32_t layers, uint32_t slots, uint32_t pid,
                      const char* producer, std::string* why)
{
	Close();
	const size_t bytes = RingBytes(maxW, maxH, layers, slots);
	PSECURITY_DESCRIPTOR sd = CurrentUserOnly();
	SECURITY_ATTRIBUTES sa{ sizeof sa, sd, FALSE };
	HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, sd ? &sa : nullptr, PAGE_READWRITE, 0, (DWORD)bytes, W(name).c_str());
	if (sd) LocalFree(sd);
	if (!m) { if (why) *why = "CreateFileMapping failed (" + std::to_string(GetLastError()) + ")"; return false; }
	void* base = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
	if (!base) { CloseHandle(m); if (why) *why = "MapViewOfFile failed"; return false; }
	if (!InitRing(base, bytes, maxW, maxH, layers, slots, pid, producer)) {
		UnmapViewOfFile(base); CloseHandle(m);
		if (why) *why = "bad ring geometry";
		return false;
	}
	m_map = m; m_base = base; m_bytes = bytes; m_name = name;
	return true;
}

void Producer::Close()
{
	if (m_base) { Header(m_base)->flags &= ~kFlagProducerAlive; UnmapViewOfFile(m_base); }
	if (m_map) CloseHandle((HANDLE)m_map);
	m_map = m_base = nullptr;
	m_bytes = 0;
}

Reader::~Reader() { Close(); }

bool Reader::Open(const std::string& name, std::string* why)
{
	Close();
	HANDLE m = OpenFileMappingW(FILE_MAP_READ, FALSE, W(name).c_str());
	if (!m) { if (why) *why = "no mapping " + name + " (" + std::to_string(GetLastError()) + ")"; return false; }
	const void* base = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
	if (!base) { CloseHandle(m); if (why) *why = "MapViewOfFile failed"; return false; }
	MEMORY_BASIC_INFORMATION mi{};
	VirtualQuery(base, &mi, sizeof mi);
	if (!HeaderValid(base, mi.RegionSize)) {
		UnmapViewOfFile(base); CloseHandle(m);
		if (why) *why = "the mapping is not a frame ring this editor understands (magic / version / geometry)";
		return false;
	}
	m_map = m; m_base = base; m_bytes = mi.RegionSize; m_name = name;
	m_frame.assign(Header(base)->slotStride, 0);
	m_lastSeq = m_frames = m_skipped = m_torn = 0;
	m_fps = 0; m_fpsStartMs = GetTickCount64(); m_fpsCount = 0;
	return true;
}

void Reader::Close()
{
	if (m_base) UnmapViewOfFile(m_base);
	if (m_map) CloseHandle((HANDLE)m_map);
	m_map = nullptr; m_base = nullptr; m_bytes = 0;
	m_frame.clear();
}

ReadResult Reader::Poll()
{
	if (!m_base) return ReadResult::NoRing;
	const ReadResult r = ReadLatest(m_base, m_bytes, m_lastSeq, m_frame.data(), m_frame.size());
	const uint64_t now = GetTickCount64();
	if (r == ReadResult::Torn) ++m_torn;
	if (r == ReadResult::Ok) {
		const FrameSlotHeader& f = Frame();
		if (m_lastSeq && f.frameSeq > m_lastSeq + 1) m_skipped += f.frameSeq - m_lastSeq - 1;
		m_lastSeq = f.frameSeq;
		++m_frames;
		++m_fpsCount;
		m_latency = (uint32_t)now - f.presentMs;
	}
	if (now - m_fpsStartMs >= 500) {
		m_fps = m_fpsCount * 1000.0f / (float)(now - m_fpsStartMs);
		m_fpsStartMs = now;
		m_fpsCount = 0;
	}
	return r;
}

} // namespace framering
