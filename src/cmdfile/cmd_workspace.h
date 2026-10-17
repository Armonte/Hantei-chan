#ifndef CMDFILE_CMD_WORKSPACE_H_GUARD
#define CMDFILE_CMD_WORKSPACE_H_GUARD

// One open command file: the staged document, its notes, its own undo history, and the
// save pipeline (validate -> detect external change -> unique backup -> atomic replace).
// Nothing touches the disk until save(). The HA6 undo stack is never involved.

#include "cmd_document.h"
#include "cmd_notes.h"
#include "cmd_validate.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace cmdfile {

struct WorkspaceSnapshot {
	Document document;
	Notes notes;
};

// Undo/redo of whole-workspace snapshots (document and notes change together).
class WorkspaceHistory {
public:
	explicit WorkspaceHistory(std::size_t capacity = 200) : m_capacity(capacity) {}
	void clear() { m_undo.clear(); m_redo.clear(); }
	void record(WorkspaceSnapshot before);
	bool undo(WorkspaceSnapshot& current);
	bool redo(WorkspaceSnapshot& current);
	// Drop the last recorded step without touching redo (used when an edit fails).
	bool rollback(WorkspaceSnapshot& current);
	bool canUndo() const { return !m_undo.empty(); }
	bool canRedo() const { return !m_redo.empty(); }
	std::size_t undoDepth() const { return m_undo.size(); }

private:
	std::size_t m_capacity;
	std::vector<WorkspaceSnapshot> m_undo, m_redo;
};

enum class SaveStatus { Saved, ValidationFailed, ExternalChange, BackupFailed, WriteFailed, NotesWriteFailed };

struct SaveResult {
	SaveStatus status = SaveStatus::Saved;
	std::string message;
	std::string backupPath;
};

class Workspace {
public:
	bool open(const std::string& path, std::string* error);
	// For tests and in-memory use: no file behind it.
	void openFromBytes(const std::string& path, const std::string& bytes);

	const std::string& path() const { return m_path; }
	const std::string& savedBytes() const { return m_savedBytes; }
	WorkspaceSnapshot state; // document + notes being edited

	// Transaction helper: records the current state, runs `edit`, and rolls the
	// snapshot back if it returns false.
	bool apply(const std::function<bool(WorkspaceSnapshot&)>& edit);
	bool undo();
	bool redo();
	bool canUndo() const { return m_history.canUndo(); }
	bool canRedo() const { return m_history.canRedo(); }
	void revert(); // back to the saved bytes and saved notes (undoable)

	bool dirty() const;
	bool documentDirty() const;
	bool notesDirty() const { return state.notes != m_savedNotes; }
	std::size_t changedLineCount() const;
	std::uint64_t revision() const { return m_revision; } // bumps on every change

	// Notes file problems. When notesReadOnly() is true the notes file is never written.
	const std::string& notesError() const { return m_notesError; }
	bool notesReadOnly() const { return !m_notesError.empty(); }

	SaveResult save(ExtensionProfile profile, bool overwriteExternalChanges);

private:
	std::string m_path;
	std::string m_savedBytes;
	Document m_savedDocument; // same uids as `state` at load/save time
	Notes m_savedNotes;
	std::string m_notesError;
	WorkspaceHistory m_history;
	std::uint64_t m_revision = 1;
	void loadNotes();
};

} // namespace cmdfile

#endif
