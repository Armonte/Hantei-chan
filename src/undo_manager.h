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
	std::unique_ptr<SequenceSnapshot> lastRestored;  // Temporarily holds undo/redo target
	size_t cleanStateDepth = 0;  // Undo stack depth at last save (0 = initial clean state)

public:
	UndoManager() = default;

	// Temporarily disable undo recording (for undo/redo operations)
	void setEnabled(bool enable) { enabled = enable; }
	bool isEnabled() const { return enabled; }

	// Called at beginning of frame - saves current state before any modifications
	void beginFrame(int patternIndex, const Sequence& sequence) {
		if (!enabled) return;

		// If pattern changed or no pending snapshot, create new one
		// Don't update existing snapshot - it should preserve the pre-modification state
		// After endFrame() commits, pendingSnapshot becomes null (via std::move)
		if (!pendingSnapshot || pendingSnapshot->patternIndex != patternIndex) {
			pendingSnapshot = std::make_unique<SequenceSnapshot>(patternIndex, sequence);
			hasUnsavedChanges = false;
		}
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

	// Clear pending snapshot (called after undo/redo to discard stale state)
	void clearPending() {
		pendingSnapshot.reset();
		hasUnsavedChanges = false;
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

		// Move target snapshot to temporary storage (keeps it alive for caller)
		lastRestored = std::move(undoStack.back());
		undoStack.pop_back();

		// Return pointer to the temporarily stored snapshot
		return lastRestored.get();
	}

	// Redo last undone action
	// Caller should save current state to undo before calling
	SequenceSnapshot* redo(int currentPatternIndex, const Sequence& currentSequence) {
		if (redoStack.empty()) {
			return nullptr;
		}

		// Save current state to undo stack
		undoStack.push_back(std::make_unique<SequenceSnapshot>(currentPatternIndex, currentSequence));

		// Move target snapshot to temporary storage (keeps it alive for caller)
		lastRestored = std::move(redoStack.back());
		redoStack.pop_back();

		// Return pointer to the temporarily stored snapshot
		return lastRestored.get();
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
		cleanStateDepth = 0;
	}

	// Mark current state as clean (saved)
	void markCleanState() {
		cleanStateDepth = undoStack.size();
	}

	// Check if we're at the clean state (saved state)
	bool isAtCleanState() const {
		return undoStack.size() == cleanStateDepth;
	}
};

#endif /* UNDO_MANAGER_H_GUARD */
