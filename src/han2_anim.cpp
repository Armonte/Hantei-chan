#include "han2_anim.h"
#include "framedata.h"

namespace han2 {

static const Sequence *Seq(const FrameData &fd, int p) { return p >= 0 && p < (int)fd.m_sequences.size() && !fd.m_sequences[p].frames.empty() ? &fd.m_sequences[p] : nullptr; }

static void EnterFrame(const Frame &f, AnimState &s)
{
	s.ticksInFrame = 0;
	if (f.han2.valid && f.han2.rec[0x12]) s.loopCounter = f.han2.rec[0x12];   // Actor_EnterFrame: loop counter loaded from the frame
}

void AnimStart(const FrameData &fd, AnimState &s, int pattern)
{
	s = AnimState();
	s.pattern = pattern;
	if (const Sequence *q = Seq(fd, pattern)) EnterFrame(q->frames[0], s);
}

bool AnimTick(const FrameData &fd, AnimState &s, bool follow, bool loop)
{
	const Sequence *seq = Seq(fd, s.pattern);
	if (!seq || s.ended) return false;
	if (s.frame < 0 || s.frame >= (int)seq->frames.size()) { s.frame = 0; }
	const Frame &f = seq->frames[s.frame];
	s.ticksInFrame++; s.totalTicks++;
	if (s.ticksInFrame < f.AF.duration) return false;
	const int n = (int)seq->frames.size();
	const bool raw = f.han2.valid;
	const int ani = raw ? f.han2.rec[0x0B] : 1, jump = raw ? f.han2.rec[0x0C] : 0, loopEnd = raw ? f.han2.rec[0x13] : 0;
	int next = s.frame;
	switch (ani) {
	case 0:   // end: go to pattern `jump`
		s.jumpedToPattern = jump;
		if (follow && Seq(fd, jump)) { s.pattern = jump; s.frame = 0; s.totalTicks = 0; EnterFrame(fd.m_sequences[jump].frames[0], s); return true; }
		if (loop) { s.frame = 0; EnterFrame(seq->frames[0], s); s.totalTicks = 0; return true; }
		s.ended = true; return false;
	case 1: case 3: case 7: case 9: next = s.frame + 1; break;
	case 2: case 4: next = jump; break;
	case 5: if (s.loopCounter) { s.loopCounter--; next = jump; } else next = loopEnd; break;
	case 6: next = s.frame - 1; break;
	case 8: if (s.loopCounter) { s.loopCounter--; next = jump; } else next = loopEnd; break;
	default: next = s.frame + 1; break;
	}
	if (next < 0 || next >= n) {
		if (loop) { next = 0; s.totalTicks = 0; } else { s.ended = true; return false; }
	}
	s.frame = next;
	EnterFrame(seq->frames[next], s);
	return true;
}

void AnimStepFrame(const FrameData &fd, AnimState &s, int dir)
{
	const Sequence *seq = Seq(fd, s.pattern);
	if (!seq) return;
	const int n = (int)seq->frames.size();
	s.frame = ((s.frame + dir) % n + n) % n;
	s.ended = false;
	EnterFrame(seq->frames[s.frame], s);
}

} // namespace han2
