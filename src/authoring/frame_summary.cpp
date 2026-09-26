// [authoring] inline frame data — see frame_summary.h.
//
// The walk mirrors the game's own frame advance (Character_AdvanceFrameByAniFlag 0x461150, the same rules
// preview_sim.cpp AdvanceFrame implements): a frame lasts max(1, AF duration) ticks, then
//   aniType 0 (end)   -> the pattern ends; the actor starts pattern AF.jump (+ own pattern with aniFlag 4)
//   aniType 1 (next)  -> frame + 1
//   aniType 2 (jump)  -> loop counter (set from AF loopCount on frame entry) decremented; while it is non-zero or
//                        aniFlag 2 is clear: AF.jump (relative with aniFlag 4); else AF.loopEnd (relative with 8)
//   other             -> the frame is held
// Boxes (box_pane.cpp's classification): 0 collision, 1..8 hurt, 9..24 clash / projectile / special, 25+ attack.
// Invulnerable = no hurt box on the frame, or AS invincibility 3 ("strike invincible", hantei4 labels).
#include "frame_summary.h"
#include "../framedata.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <utility>

namespace authoring {

std::string MoveSummary::Text() const
{
	if (!valid) return "no such pattern";
	char b[200];
	if (startup < 0)
		std::snprintf(b, sizeof b, "no attack box, total %d%s", totalTicks, loops ? " (loops)" : "");
	else
		std::snprintf(b, sizeof b, "startup %d, active %s (%d), recovery %d, total %d%s", startup, activeText.c_str(), active,
		              recovery, totalTicks, loops ? " (loops)" : "");
	return b;
}

MoveSummary SummarizePattern(const FrameData& fd, int pattern)
{
	MoveSummary m;
	m.pattern = pattern;
	if (pattern < 0 || pattern >= (int)fd.m_sequences.size()) { m.note = "no such pattern"; return m; }
	const Sequence& seq = fd.m_sequences[pattern];
	if (seq.frames.empty()) { m.note = "pattern has no frames"; return m; }
	m.valid = true;
	m.name = std::string(seq.name.data(), seq.name.size());
	m.frames = (int)seq.frames.size();

	const int nFrames = (int)seq.frames.size();
	std::set<std::pair<int, int>> seen;   // (frame, loop counter) at entry
	int frame = 0, loopCounter = 0, tick = 0;
	bool first = true;
	constexpr int kMaxTicks = 20000, kMaxVisits = 4000;
	for (int visits = 0; visits < kMaxVisits && tick < kMaxTicks; ++visits) {
		if (frame < 0 || frame >= nFrames) {
			char b[80];
			std::snprintf(b, sizeof b, "runs off the end (frame %d)", frame);
			m.note = b;
			break;
		}
		const Frame& fr = seq.frames[frame];
		const Frame_AF& af = fr.AF;
		if (af.loopCount) loopCounter = af.loopCount;
		else if (first) loopCounter = 0;
		first = false;
		if (!seen.insert({ frame, loopCounter }).second) {
			m.loops = true;
			char b[80];
			std::snprintf(b, sizeof b, "loops at frame %d", frame);
			m.note = b;
			break;
		}
		FrameStrip s;
		s.frame = frame;
		s.startTick = tick;
		s.duration = std::max(1, af.duration);
		for (const auto& kv : fr.hitboxes) {
			++s.boxes;
			if (kv.first >= 25) s.attack = true;
			else if (kv.first >= 1 && kv.first <= 8) s.hurt = true;
		}
		s.invuln = !s.hurt || fr.AS.invincibility == 3;
		m.strip.push_back(s);
		tick += s.duration;

		bool stop = false;
		switch (af.aniType) {
		case 0: {
			const int target = af.jump + ((af.aniFlag & 4) ? pattern : 0);
			char b[80];
			if (target == pattern) { m.loops = true; std::snprintf(b, sizeof b, "restarts itself at the end"); }
			else std::snprintf(b, sizeof b, "ends into pattern %d", target);
			m.note = b;
			stop = true;
			break;
		}
		case 1:
			if (frame + 1 >= nFrames) { m.note = "ends after the last frame"; stop = true; }
			else frame = frame + 1;
			break;
		case 2: {
			if (loopCounter) --loopCounter;
			int next;
			if (!(af.aniFlag & 2) || loopCounter) next = (af.aniFlag & 4) ? frame + af.jump : af.jump;
			else next = (af.aniFlag & 8) ? frame + af.loopEnd : af.loopEnd;
			if (next < 0 || next >= nFrames) {
				char b[80];
				std::snprintf(b, sizeof b, "jumps out of the pattern (frame %d)", next);
				m.note = b;
				stop = true;
			} else {
				// entering a frame resets the counter from ITS loopCount only; keep ours otherwise
				frame = next;
			}
			break;
		}
		default: {
			char b[80];
			std::snprintf(b, sizeof b, "holds frame %d (aniType %d)", frame, af.aniType);
			m.note = b;
			stop = true;
			break;
		}
		}
		if (stop) break;
	}
	m.totalTicks = tick;

	// active ranges (1-based ticks), merged when contiguous
	int firstA = -1, lastA = -1;
	std::vector<std::pair<int, int>> ranges;
	for (const FrameStrip& s : m.strip) {
		if (!s.attack) continue;
		const int a = s.startTick + 1, b = s.startTick + s.duration;
		if (firstA < 0) firstA = s.startTick;
		lastA = s.startTick + s.duration;   // exclusive end, 0-based
		if (!ranges.empty() && ranges.back().second + 1 >= a) ranges.back().second = std::max(ranges.back().second, b);
		else ranges.push_back({ a, b });
	}
	if (firstA >= 0) {
		m.startup = firstA;
		m.active = lastA - firstA;
		m.recovery = m.totalTicks - lastA;
		for (size_t i = 0; i < ranges.size(); ++i) {
			char b[32];
			if (ranges[i].first == ranges[i].second) std::snprintf(b, sizeof b, "%d", ranges[i].first);
			else std::snprintf(b, sizeof b, "%d-%d", ranges[i].first, ranges[i].second);
			if (i) m.activeText += ", ";
			m.activeText += b;
		}
	}
	return m;
}

} // namespace authoring
