#ifndef HAN2_DIFF_H_GUARD
#define HAN2_DIFF_H_GUARD
// Diff of an edited RBO / GOF2 character against the pattern area it was loaded with (kept in Han2Container::originalPatternFile).
#include <string>
#include <vector>

class FrameData;

namespace han2 {

struct DiffEntry {
	int pattern = -1, frame = -1;      // frame = -1: pattern level
	std::string what;                  // e.g. "duration 3 -> 10", "hurt box 1 (-50,-89,58,7) -> (-50,-89,60,7)"
};

// Compares every pattern/frame/box/AT/script-list byte of the model's current raw records with the original. Field names come from the
// generated IDA reflection tables. Returns false with *err when the original cannot be re-parsed.
bool DiffAgainstOriginal(const FrameData &fd, std::vector<DiffEntry> &out, std::string *err);

} // namespace han2
#endif
