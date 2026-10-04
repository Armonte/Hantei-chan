#ifndef CGM_USAGE_H_GUARD
#define CGM_USAGE_H_GUARD
// Reverse index of a loaded character: which patterns / frames / layers draw each CG image.
#include <cstdint>
#include <vector>

class FrameData;

namespace cgm {

struct UsageRef { int pattern = -1, frame = -1, layer = -1; };

struct UsageIndex {
	std::vector<std::vector<UsageRef>> byImage;   // index = CG image id; layers with usePat (PAT part sets) are not CG references
	uint64_t version = ~0ull;                     // FrameData::dataVersion the index was built from
	int patternsUsing(int image) const;           // distinct patterns
	int count(int image) const { return image >= 0 && image < (int)byImage.size() ? (int)byImage[image].size() : 0; }
	void build(FrameData &fd, int imageCount);
	bool stale(const FrameData &fd) const;
};

} // namespace cgm
#endif
