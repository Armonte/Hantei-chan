#ifndef TAG_HISTORY_H_GUARD
#define TAG_HISTORY_H_GUARD
// [authoring] One undo history for the whole sidecar workspace (docs/HANTEI_AUTHORING_MODE.md §5.5).
// An entry is {seq, label, per document: before bytes, after bytes}. A slider drag is one entry (the UI opens it on
// activation and closes it on deactivation); a style pick that also drops [tuning] keys, a checkpoint revert, an A/B
// swap and a Promote (local + shipped) are one entry each. Undo / redo put the bytes back into the documents; the
// caller then saves (the files stay the truth) and live-applies, so the game follows the undo.
#include "tag_sidecar.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tagtune {

struct HistoryEntry {
	uint32_t seq = 0;
	std::string label;          // "cancelWindowTicks (shiki, full)"
	struct Change { DocId doc; std::string before, after; };
	std::vector<Change> changes;
};

class TagHistory {
public:
	// Record the difference between `before` (a Take() from before the edit) and the workspace now. Nothing is
	// recorded when no document changed. Clears the redo list. Returns false for a no-op.
	bool Commit(const std::string& label, const SidecarWorkspace::Snapshot& before, const SidecarWorkspace& now);
	bool CanUndo() const { return m_pos > 0; }
	bool CanRedo() const { return m_pos < m_entries.size(); }
	std::string UndoLabel() const { return CanUndo() ? m_entries[m_pos - 1].label : std::string(); }
	std::string RedoLabel() const { return CanRedo() ? m_entries[m_pos].label : std::string(); }
	// Put the entry's bytes back into the workspace documents (as edits: they become dirty). Returns the documents
	// touched (empty = nothing to undo / redo).
	std::vector<DocId> Undo(SidecarWorkspace& ws);
	std::vector<DocId> Redo(SidecarWorkspace& ws);
	void Clear() { m_entries.clear(); m_pos = 0; }
	size_t Size() const { return m_entries.size(); }
	size_t Position() const { return m_pos; }
	const std::vector<HistoryEntry>& Entries() const { return m_entries; }
	size_t cap = 500;

private:
	std::vector<HistoryEntry> m_entries;
	size_t m_pos = 0;
	uint32_t m_seq = 0;
};

} // namespace tagtune

#endif
