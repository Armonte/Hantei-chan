#ifndef CGM_UNDO_H_GUARD
#define CGM_UNDO_H_GUARD
// Undo / redo of CG manager operations. A step is a pair of whole-bank snapshots; Bank copies are cheap because image blobs are shared
// (std::shared_ptr<const vector>), so only the blobs an operation actually replaced cost memory.
#include "cgm_bank.h"
#include <string>
#include <vector>

namespace cgm {

struct HistoryEntry {
	std::string label;
	Bank before, after;
	std::vector<int> remap;   // optional: old image id -> new id (-1 = removed), for reference fix-up on undo/redo (M5)
};

class History {
public:
	size_t maxEntries = 64;
	void push(std::string label, Bank before, Bank after, std::vector<int> remap = {}) {
		redo_.clear();
		done_.push_back({std::move(label), std::move(before), std::move(after), std::move(remap)});
		if (done_.size() > maxEntries) done_.erase(done_.begin());
	}
	bool canUndo() const { return !done_.empty(); }
	bool canRedo() const { return !redo_.empty(); }
	const HistoryEntry &top() const { return done_.back(); }
	const HistoryEntry &topRedo() const { return redo_.back(); }
	HistoryEntry undo() { HistoryEntry e = std::move(done_.back()); done_.pop_back(); redo_.push_back(e); return e; }
	HistoryEntry redo() { HistoryEntry e = std::move(redo_.back()); redo_.pop_back(); done_.push_back(e); return e; }
	void clear() { done_.clear(); redo_.clear(); }
private:
	std::vector<HistoryEntry> done_, redo_;
};

} // namespace cgm
#endif
