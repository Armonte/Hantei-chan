#ifndef CGM_UNDO_H_GUARD
#define CGM_UNDO_H_GUARD
// Undo / redo of CG manager operations. A step is a pair of whole-bank snapshots; Bank copies are cheap because image blobs are shared
// (std::shared_ptr<const vector>), so only the blobs an operation actually replaced cost memory.
#include "cgm_bank.h"
#include "cgm_palette.h"
#include "cgm_usage.h"
#include "cgm_texsrc.h"
#include <string>
#include <vector>

namespace cgm {

struct HistoryEntry {
	std::string label;
	bool hasBank = false;
	Bank before, after;
	int palBank = -1;          // >= 0: a palette file (0 = <cg>.pal, 1..7 = PUPS) changed in this step
	PalSet palBefore, palAfter;
	std::vector<int> remap;   // optional: old image id -> new id (-1 = removed): the frame references were rewritten through it
	std::vector<ClearedRef> cleared;   // references cleared because their image was deleted (restored on undo)
	// "CG + .pat textures" tab: a .pat texture replace (texture records + the parts that follow it), and a CG image replace in a bank
	// format the Bank model does not hold (CgIO: the CG's own stored bytes)
	PatTexSnap patBefore, patAfter;
	std::shared_ptr<const std::vector<uint8_t>> rawBefore, rawAfter;
	// a write into the UNI2-IM / MBTL-IM mod folder ("Save .pal to mod folder", "Save to mod folder"): the file before (null = none) / after
	std::string modDst;
	std::shared_ptr<const std::vector<uint8_t>> modBefore, modAfter;
};

class History {
public:
	size_t maxEntries = 64;
	void push(HistoryEntry e) {
		redo_.clear();
		done_.push_back(std::move(e));
		if (done_.size() > maxEntries) done_.erase(done_.begin());
	}
	bool canUndo() const { return !done_.empty(); }
	bool canRedo() const { return !redo_.empty(); }
	const HistoryEntry &top() const { return done_.back(); }   // only while canUndo()
	// Label of the step Undo would revert, "" when there is none. UI code reads this after a button may have undone the last step in
	// the same frame (reading top() then was the "CG + .pat textures" tab's undo crash: back() of an empty history).
	const char *topLabel() const { return done_.empty() ? "" : done_.back().label.c_str(); }
	const HistoryEntry &topRedo() const { return redo_.back(); }
	HistoryEntry undo() { HistoryEntry e = std::move(done_.back()); done_.pop_back(); redo_.push_back(e); return e; }
	HistoryEntry redo() { HistoryEntry e = std::move(redo_.back()); redo_.pop_back(); done_.push_back(e); return e; }
	void clear() { done_.clear(); redo_.clear(); }
private:
	std::vector<HistoryEntry> done_, redo_;
};

} // namespace cgm
#endif
