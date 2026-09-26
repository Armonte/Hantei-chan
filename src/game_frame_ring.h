#ifndef GAME_FRAME_RING_H_GUARD
#define GAME_FRAME_RING_H_GUARD
// [game-view] Win32 side of the frame ring (game_frame_share.h): the named file mapping for a producer (the mock DLL;
// pchost.dll has its own copy) and for the reader (Hantei-chan's Game panel, game_link_cli frame-check).
#include "game_frame_share.h"

#include <cstdint>
#include <string>
#include <vector>

namespace framering {

// "Local\povertycaster-frames-<pid>"
std::string MappingName(uint32_t producerPid);

class Producer {
public:
	~Producer();
	// Create the mapping (current-user DACL) and initialise the ring. False (+ why) on failure.
	bool Create(const std::string& name, uint32_t maxW, uint32_t maxH, uint32_t layers, uint32_t slots, uint32_t pid,
	            const char* producer, std::string* why);
	void Close();
	void* Base() const { return m_base; }
	size_t Bytes() const { return m_bytes; }
	const std::string& Name() const { return m_name; }

private:
	void* m_map = nullptr;
	void* m_base = nullptr;
	size_t m_bytes = 0;
	std::string m_name;
};

// A reader: open by name, then Poll() once per UI frame for the newest frame.
class Reader {
public:
	~Reader();
	bool Open(const std::string& name, std::string* why);
	void Close();
	bool IsOpen() const { return m_base != nullptr; }
	const std::string& Name() const { return m_name; }
	// The newest frame if it is newer than the last one returned. Ok -> Frame() / LayerPixels() hold it.
	ReadResult Poll();
	const FrameSlotHeader& Frame() const { return *(const FrameSlotHeader*)m_frame.data(); }
	const uint8_t* LayerPixels(const FrameLayer& l) const { return m_frame.data() + l.offset; }
	const FrameRingHeader* Ring() const { return m_base ? Header(m_base) : nullptr; }
	// Stats over the frames Poll() returned.
	uint32_t Frames() const { return m_frames; }
	uint32_t Skipped() const { return m_skipped; }     // frameSeq gaps: frames published but never seen
	uint32_t Torn() const { return m_torn; }
	float Fps() const { return m_fps; }
	uint32_t LatencyMs() const { return m_latency; }   // now - presentMs of the last frame

private:
	void* m_map = nullptr;
	const void* m_base = nullptr;
	size_t m_bytes = 0;
	std::string m_name;
	std::vector<uint8_t> m_frame;
	uint32_t m_lastSeq = 0, m_frames = 0, m_skipped = 0, m_torn = 0, m_latency = 0;
	float m_fps = 0;
	uint64_t m_fpsStartMs = 0;
	uint32_t m_fpsCount = 0;
};

} // namespace framering

#endif
