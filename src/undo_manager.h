#ifndef UNDO_MANAGER_H_GUARD
#define UNDO_MANAGER_H_GUARD

#include "framedata.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Document-level undo/redo for one character's pattern list.
//
// Model
// -----
// The manager keeps a *shadow* of the document: one immutable, shared copy of
// every pattern (std::shared_ptr<const Sequence>) as of the last committed
// undo step. When a step is committed, the live pattern list is compared with
// the shadow; each pattern that differs becomes a PatternDelta holding the
// shared "before" and "after" copies, and the shadow slot is replaced by the
// new "after" copy. Unchanged patterns are never copied again, so memory is one
// document copy plus the patterns that actually changed in the retained
// history (copies are shared between the shadow, undo and redo entries).
//
// Because the commit diffs the whole document, every kind of edit is covered
// regardless of which pattern the view is showing: widget edits, frame
// insert/delete, box drawing, effects/conditions, pattern metadata, pattern
// paste, multi-pattern "Pop all and paste", and anything that edits a pattern
// other than the visible one. Callers only have to say *when* a step ends.
//
// Grouping
// --------
//  * markModified()      - something was edited; commit at the next idle point.
//  * endFrame(active)    - called once per UI frame. While `active` (an ImGui
//                          widget is active or a mouse button is held) or while
//                          an explicit transaction is open, edits keep
//                          accumulating into one pending step. When the gesture
//                          ends, the step is committed. The falling edge of a
//                          gesture also triggers a diff even without
//                          markModified(), so a widget that forgot to notify is
//                          still recorded.
//  * begin/commitTransaction() - explicit grouping for viewport gestures (box
//                          drawing, position tool) and multi-step operations.
//                          Nestable. cancelTransaction() rolls the live
//                          document back to the pre-transaction state.
//  * flush()             - commit now (undo/redo/save call this first).
//
// Cost
// ----
// A commit used to compare every pattern with its shadow (12-19 ms per step on
// a 700-pattern character under load). Now only a markModified() without an
// index, flush() (save), markClean(), undo/redo and cancelTransaction() compare
// the whole document. Every other commit compares the patterns marked with
// markModified(index) or reported through the dirty feed
// (FrameData::mark_modified), the focus pattern, any pattern-list size change
// and a rotating slice of kDriftSlice other patterns. The slice catches edits
// that notified nobody within a few steps, without a per-frame full scan.
//
// Dirty state
// -----------
// Each committed entry has a unique revision id. markClean() records the
// current revision; isClean() is true exactly when undo/redo has brought the
// document back to that revision. Sequence::modified ("changed since load",
// used by Save-as-MOD) is recomputed for restored patterns from the shadow.
// Extra state of a character that is not part of the pattern list (the parts model of the PAT editor, ...) can join the
// same history through a channel. A channel snapshots its state as an immutable object; the manager keeps the committed
// snapshot as the channel's shadow, asks the channel whether the live state still equals it at every commit, and stores
// (before, after) snapshot pairs in the entry. Undo / redo restore a snapshot through the channel.
// Snapshots are opaque to the manager; a channel may share unchanged pieces between consecutive snapshots (capture()
// gets the previous one) so memory grows with what changed, as for patterns.
// Used by: src/parts/parts_undo.h (PAT editor parts model; French-Bread characters).
class UndoChannel {
public:
	using Snap = std::shared_ptr<const void>;
	virtual ~UndoChannel() = default;
	virtual const char* name() const = 0;
	// Is the channel tracking anything right now? An inactive channel is skipped (no snapshot, no compare).
	virtual bool active() const { return true; }
	virtual Snap capture(const Snap& previous) = 0;
	virtual bool equalsLive(const Snap& s) const = 0;
	virtual void restore(const Snap& s) = 0;
	virtual size_t approxBytes(const Snap&) const { return 0; }
};

class UndoManager {
public:
	using SeqPtr = std::shared_ptr<const Sequence>;

	struct ChannelDelta {
		int channel = -1;
		UndoChannel::Snap before, after;
	};

	struct PatternDelta {
		int index = -1;
		SeqPtr before;   // null: pattern slot did not exist before
		SeqPtr after;    // null: pattern slot does not exist after
	};

	struct Entry {
		uint64_t id = 0;
		std::string label;
		std::vector<PatternDelta> changes;
		size_t countBefore = 0;   // pattern-list size before the step
		size_t countAfter = 0;    // pattern-list size after the step
		int focusPattern = -1;    // where the edit was made (for navigation)
		int focusFrame = -1;
		std::vector<ChannelDelta> channels;   // channel state changed by this step
	};

	UndoManager() = default;
	UndoManager(const UndoManager&) = delete;
	UndoManager& operator=(const UndoManager&) = delete;

	// Bind the live document. The baseline is captured lazily by
	// ensureBaseline() (or the first endFrame), so binding is free.
	void attach(std::vector<Sequence>* document);
	std::vector<Sequence>* document() const { return m_doc; }

	// Forget history and baseline. Call after the document was replaced
	// (load / patch load / new). The next ensureBaseline() re-snapshots.
	void reset();

	// Add a channel (kept across reset(); its shadow is re-captured with the baseline). Returns its index.
	// The channel must outlive the manager or be removed first.
	int addChannel(UndoChannel* channel);
	void removeChannel(UndoChannel* channel);
	size_t channelCount() const { return m_channels.size(); }

	// Capture the baseline if it does not exist yet. Returns false if no
	// document is attached.
	bool ensureBaseline();
	bool hasBaseline() const { return m_hasBaseline; }

	// Record which pattern/frame the user is looking at. The value current
	// when a step starts is stored in the entry and used to navigate on undo.
	void noteFocus(int pattern, int frame);

	// ---- Grouping ---------------------------------------------------------
	// Something was edited somewhere: the next commit compares the WHOLE document (cross-pattern pastes, tools).
	void markModified();
	// Pattern `index` was edited: the next commit compares only the marked patterns, the focus pattern and a small
	// rotating slice of the rest (see "Cost" above), so a step costs what it changed, not the document size.
	void markModified(int index);
	// Edit sink: FrameData::mark_modified() appends pattern indices here; every commit consumes them like
	// markModified(index). Null = none.
	void setDirtyFeed(std::vector<int>* feed) { m_dirtyFeed = feed; }
	void beginTransaction(const char* label = nullptr);
	// Returns true if the outermost commit produced an undo entry.
	bool commitTransaction();
	// Close every open transaction level and restore the live document to the
	// last committed state (discarding the pending step). No entry is made.
	void cancelTransaction();
	bool inTransaction() const { return m_txDepth > 0; }
	int transactionDepth() const { return m_txDepth; }

	// Per-UI-frame hook. `gestureActive`: an ImGui item is active or a mouse
	// button is held. Returns true if an entry was committed.
	bool endFrame(bool gestureActive);

	// Commit any pending change immediately (ignores gesture state). Returns
	// true if an entry was created. `label` names the entry if one is made.
	bool flush(const char* label = nullptr);

	bool hasPendingChanges() const { return m_dirty; }

	// ---- Undo / redo ------------------------------------------------------
	// Both flush pending edits first, then apply the step to the live
	// document. Return the applied entry (valid until the next call that
	// modifies history) or nullptr when there is nothing to do.
	const Entry* undo();
	const Entry* redo();

	bool canUndo() const { return !m_undo.empty() || m_dirty; }
	bool canRedo() const { return !m_redo.empty(); }
	size_t undoCount() const { return m_undo.size(); }
	size_t redoCount() const { return m_redo.size(); }
	const Entry* peekUndo() const { return m_undo.empty() ? nullptr : &m_undo.back(); }
	const Entry* peekRedo() const { return m_redo.empty() ? nullptr : &m_redo.back(); }

	void clear();                 // drop history, keep baseline
	void setMaxLevels(size_t n) { m_maxLevels = n ? n : 1; trim(); }
	size_t maxLevels() const { return m_maxLevels; }

	// ---- Dirty state ------------------------------------------------------
	void markClean();             // flushes, then records current revision
	bool isClean() const { return currentRevision() == m_savedRevision && !m_dirty; }
	uint64_t currentRevision() const;

	// Approximate bytes held by history (patterns not shared with the shadow).
	size_t historyBytes() const;

	// Content equality used for change detection (ignores Sequence::modified).
	static bool SequenceContentEquals(const Sequence& a, const Sequence& b);
	static size_t ApproxSequenceBytes(const Sequence& s);

private:
	bool commitInternal(const char* label, bool fullScan);
	bool commitPending(const char* label) { return commitInternal(label, wantsFullScan()); }
	bool wantsFullScan() const;
	void settlePending();
	std::vector<PatternDelta> diff() const;
	// Only `indices` (any order, duplicates allowed) plus the list-size change.
	std::vector<PatternDelta> diffIndices(std::vector<int> indices) const;
	std::vector<int> incrementalScanSet();
	std::vector<ChannelDelta> diffChannels();
	void captureChannelBaseline();
	void applySide(const Entry& e, bool toAfter);
	void pushEntry(Entry&& e);
	void trim();
	void updateModifiedFlag(size_t index);

	std::vector<Sequence>* m_doc = nullptr;
	bool m_hasBaseline = false;
	std::vector<SeqPtr> m_shadow;   // committed state
	std::vector<SeqPtr> m_loaded;   // state at baseline capture (for Sequence::modified)

	struct ChannelSlot { UndoChannel* ch = nullptr; UndoChannel::Snap shadow; bool wasActive = false; };
	std::vector<ChannelSlot> m_channels;

	std::vector<Entry> m_undo;
	std::vector<Entry> m_redo;
	size_t m_maxLevels = 200;

	uint64_t m_nextId = 1;
	uint64_t m_baseRevision = 0;    // revision of the oldest reachable state
	uint64_t m_savedRevision = 0;

	bool m_dirty = false;
	bool m_prevGesture = false;
	int m_txDepth = 0;
	std::string m_txLabel;

	// Incremental commits (markModified(index) / the dirty feed)
	std::vector<int> m_marked;          // patterns marked since the last commit
	bool m_fullScan = false;            // markModified() without an index since the last commit
	std::vector<int>* m_dirtyFeed = nullptr;
	size_t m_driftCursor = 0;           // round-robin position of the unmarked-drift slice
	int m_focusPattern = -1, m_focusFrame = -1;
	int m_stepFocusPattern = -1, m_stepFocusFrame = -1;
	bool m_stepFocusLatched = false;
};

#endif /* UNDO_MANAGER_H_GUARD */
