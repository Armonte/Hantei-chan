#ifndef UNDO_MANAGER_H_GUARD
#define UNDO_MANAGER_H_GUARD

#include "framedata.h"
#include <vector>
#include <memory>

// Snapshot of a single sequence for undo/redo
struct SequenceSnapshot {
	int patternIndex;
	Sequence sequence;

	SequenceSnapshot(int index, const Sequence& seq)
		: patternIndex(index) {
		// Use assignment operator for proper deep copy
		sequence = seq;
	}
};

class UndoManager {
private:
	std::vector<std::unique_ptr<SequenceSnapshot>> undoStack;
	std::vector<std::unique_ptr<SequenceSnapshot>> redoStack;
	static constexpr size_t MAX_UNDO_LEVELS = 50;
	bool enabled = true;
	bool hasUnsavedChanges = false;
	std::unique_ptr<SequenceSnapshot> pendingSnapshot;

public:
	UndoManager() = default;

	// Temporarily disable undo recording (for undo/redo operations)
	void setEnabled(bool enable) { enabled = enable; }
	bool isEnabled() const { return enabled; }

	// Called at beginning of frame - saves current state before any modifications
	void beginFrame(int patternIndex, const Sequence& sequence) {
		if (!enabled) return;

		// If pattern changed or no pending snapshot, create new one
		if (!pendingSnapshot || pendingSnapshot->patternIndex != patternIndex) {
			pendingSnapshot = std::make_unique<SequenceSnapshot>(patternIndex, sequence);
			hasUnsavedChanges = false;
		}
		// If same pattern and we haven't saved yet this frame, update with current state
		// This captures the pre-modification state
		else if (!hasUnsavedChanges) {
			pendingSnapshot->sequence = sequence;
		}
		// else: already marked as modified, don't update (preserve pre-modification state)
	}

	// Called when data is modified - marks that we should commit the pending snapshot
	void markModified() {
		if (!enabled) return;
		hasUnsavedChanges = true;
	}

	// Called at end of frame - commits pending snapshot if there were changes
	void endFrame() {
		if (!enabled) return;

		if (hasUnsavedChanges && pendingSnapshot) {
			// Clear redo stack when new action is performed
			redoStack.clear();

			// Commit the pending snapshot to undo stack
			undoStack.push_back(std::move(pendingSnapshot));
			hasUnsavedChanges = false;

			// Limit stack size
			if (undoStack.size() > MAX_UNDO_LEVELS) {
				undoStack.erase(undoStack.begin());
			}
		}
	}

	// Legacy saveState for backward compatibility (immediately commits)
	void saveState(int patternIndex, const Sequence& sequence) {
		if (!enabled) return;

		// Clear redo stack when new action is performed
		redoStack.clear();

		// Add to undo stack
		undoStack.push_back(std::make_unique<SequenceSnapshot>(patternIndex, sequence));

		// Limit stack size
		if (undoStack.size() > MAX_UNDO_LEVELS) {
			undoStack.erase(undoStack.begin());
		}
	}

	// Undo last action - returns the previous state to restore
	// Caller should save current state to redo before calling
	SequenceSnapshot* undo(int currentPatternIndex, const Sequence& currentSequence) {
		if (undoStack.empty()) {
			return nullptr;
		}

		// Save current state to redo stack
		redoStack.push_back(std::make_unique<SequenceSnapshot>(currentPatternIndex, currentSequence));

		// Pop and return the previous state
		auto* snapshot = undoStack.back().get();
		undoStack.pop_back();

		return snapshot;
	}

	// Redo last undone action
	// Caller should save current state to undo before calling
	SequenceSnapshot* redo(int currentPatternIndex, const Sequence& currentSequence) {
		if (redoStack.empty()) {
			return nullptr;
		}

		// Save current state to undo stack
		undoStack.push_back(std::make_unique<SequenceSnapshot>(currentPatternIndex, currentSequence));

		// Pop and return the state to redo
		auto* snapshot = redoStack.back().get();
		redoStack.pop_back();

		return snapshot;
	}

	bool canUndo() const {
		return !undoStack.empty();
	}

	bool canRedo() const {
		return !redoStack.empty();
	}

	void clear() {
		undoStack.clear();
		redoStack.clear();
	}
};

#endif /* UNDO_MANAGER_H_GUARD */
