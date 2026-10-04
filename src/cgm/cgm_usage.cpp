#include "cgm_usage.h"
#include "../framedata.h"
#include <algorithm>

namespace cgm {

bool UsageIndex::stale(const FrameData &fd) const { return version != fd.dataVersion; }

void UsageIndex::build(FrameData &fd, int imageCount) {
	byImage.assign((size_t)std::max(imageCount, 0), {});
	const int n = fd.get_sequence_count();
	for (int p = 0; p < n; p++) {
		Sequence *seq = fd.get_sequence(p);
		if (!seq) continue;
		for (int f = 0; f < (int)seq->frames.size(); f++) {
			const auto &layers = seq->frames[f].AF.layers;
			for (int l = 0; l < (int)layers.size(); l++) {
				const auto &L = layers[l];
				if (L.usePat || L.spriteId < 0) continue;
				if (L.spriteId >= (int)byImage.size()) byImage.resize((size_t)L.spriteId + 1);
				byImage[L.spriteId].push_back({p, f, l});
			}
		}
	}
	version = fd.dataVersion;
}

int UsageIndex::patternsUsing(int image) const {
	if (image < 0 || image >= (int)byImage.size()) return 0;
	int c = 0, last = -1;
	for (const UsageRef &r : byImage[image]) if (r.pattern != last) { c++; last = r.pattern; }   // refs are pattern-ordered
	return c;
}

int RemapSprites(FrameData &fd, const std::vector<int> &remap, std::vector<ClearedRef> *cleared) {
	int changed = 0; const int n = fd.get_sequence_count();
	for (int p = 0; p < n; p++) {
		Sequence *seq = fd.get_sequence(p); if (!seq) continue;
		bool touched = false;
		for (int f = 0; f < (int)seq->frames.size(); f++) {
			auto &layers = seq->frames[f].AF.layers;
			for (int l = 0; l < (int)layers.size(); l++) {
				auto &L = layers[l];
				if (L.usePat || L.spriteId < 0) continue;
				const int old = L.spriteId;
				const int now = old < (int)remap.size() ? remap[old] : old;
				if (now == old) continue;
				if (now < 0 && cleared) cleared->push_back({p, f, l, old});
				L.spriteId = now < 0 ? -1 : now; changed++; touched = true;
			}
		}
		if (touched) fd.mark_modified(p);
	}
	return changed;
}

int RestoreCleared(FrameData &fd, const std::vector<ClearedRef> &cleared) {
	int n = 0;
	for (const ClearedRef &c : cleared) {
		Sequence *seq = fd.get_sequence(c.pattern);
		if (!seq || c.frame >= (int)seq->frames.size() || c.layer >= (int)seq->frames[c.frame].AF.layers.size()) continue;
		seq->frames[c.frame].AF.layers[c.layer].spriteId = c.oldId; fd.mark_modified(c.pattern); n++;
	}
	return n;
}

} // namespace cgm
