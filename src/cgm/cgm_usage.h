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

struct ClearedRef { int pattern, frame, layer, oldId; };

// Rewrites every CG image reference (layers without usePat) through `remap` (old id -> new id, -1 = the image is gone: the reference is
// cleared to -1 and recorded in `cleared`). Marks the touched patterns modified. Returns the number of references changed.
int RemapSprites(FrameData &fd, const std::vector<int> &remap, std::vector<ClearedRef> *cleared = nullptr);
// Puts cleared references back (undo of a delete).
int RestoreCleared(FrameData &fd, const std::vector<ClearedRef> &cleared);

} // namespace cgm
#endif
