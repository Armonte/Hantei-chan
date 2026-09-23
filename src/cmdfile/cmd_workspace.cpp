#include "cmd_workspace.h"

#include "cmd_io.h"
#include "../misc.h"

#include <algorithm>

namespace cmdfile {

void WorkspaceHistory::record(WorkspaceSnapshot before)
{
	m_undo.push_back(std::move(before));
	if (m_undo.size() > m_capacity) m_undo.erase(m_undo.begin());
	m_redo.clear();
}

bool WorkspaceHistory::undo(WorkspaceSnapshot& current)
{
	if (m_undo.empty()) return false;
	m_redo.push_back(std::move(current));
	current = std::move(m_undo.back());
	m_undo.pop_back();
	return true;
}

bool WorkspaceHistory::redo(WorkspaceSnapshot& current)
{
	if (m_redo.empty()) return false;
	m_undo.push_back(std::move(current));
	current = std::move(m_redo.back());
	m_redo.pop_back();
	return true;
}

bool WorkspaceHistory::rollback(WorkspaceSnapshot& current)
{
	if (m_undo.empty()) return false;
	current = std::move(m_undo.back());
	m_undo.pop_back();
	return true;
}

// ---------------------------------------------------------------------------------------

bool Workspace::open(const std::string& path, std::string* error)
{
	std::string bytes;
	if (!ReadFileBytes(path, bytes, error)) return false;
	openFromBytes(path, bytes);
	loadNotes();
	return true;
}

void Workspace::openFromBytes(const std::string& path, const std::string& bytes)
{
	m_path = path;
	m_savedBytes = bytes;
	state.document = Document::parse(bytes);
	m_savedDocument = state.document;
	state.notes = Notes{};
	m_savedNotes = Notes{};
	m_notesError.clear();
	m_history.clear();
	++m_revision;
}

void Workspace::loadNotes()
{
	const std::string notesPath = NotesPathFor(m_path);
	if (!FileExists(notesPath)) return;
	std::string text, error;
	if (!ReadFileBytes(notesPath, text, &error)) { m_notesError = error; return; }
	Notes notes;
	if (!ParseNotes(text, state.document, notes, &error)) {
		m_notesError = error + " The notes file is left untouched; fix or move it, then reopen.";
		return;
	}
	state.notes = notes;
	m_savedNotes = notes;
}

bool Workspace::apply(const std::function<bool(WorkspaceSnapshot&)>& edit)
{
	const std::string before = state.document.serialize();
	const Notes notesBefore = state.notes;
	m_history.record(state);
	if (!edit(state)) {
		m_history.rollback(state);
		return false;
	}
	if (state.document.serialize() == before && state.notes == notesBefore) {
		// No-op edit (same value typed again): keep the history clean.
		m_history.rollback(state);
		return true;
	}
	++m_revision;
	return true;
}

bool Workspace::undo()
{
	if (!m_history.undo(state)) return false;
	++m_revision;
	return true;
}

bool Workspace::redo()
{
	if (!m_history.redo(state)) return false;
	++m_revision;
	return true;
}

void Workspace::revert()
{
	apply([&](WorkspaceSnapshot& s) {
		s.document = m_savedDocument;
		s.notes = m_savedNotes;
		return true;
	});
}

bool Workspace::documentDirty() const { return state.document.serialize() != m_savedBytes; }

bool Workspace::dirty() const { return documentDirty() || (!notesReadOnly() && notesDirty()); }

std::size_t Workspace::changedLineCount() const
{
	const auto& a = m_savedDocument.lines();
	const auto& b = state.document.lines();
	// Common prefix/suffix, then count the differing middle.
	std::size_t prefix = 0;
	while (prefix < a.size() && prefix < b.size() && a[prefix].text == b[prefix].text && a[prefix].eol == b[prefix].eol) ++prefix;
	std::size_t suffix = 0;
	while (suffix < a.size() - prefix && suffix < b.size() - prefix &&
		a[a.size() - 1 - suffix].text == b[b.size() - 1 - suffix].text && a[a.size() - 1 - suffix].eol == b[b.size() - 1 - suffix].eol) ++suffix;
	return std::max(a.size(), b.size()) - prefix - suffix;
}

SaveResult Workspace::save(ExtensionProfile profile, bool overwriteExternalChanges)
{
	SaveResult result;
	const auto diagnostics = Validate(state.document, profile);
	if (HasErrors(diagnostics)) {
		result.status = SaveStatus::ValidationFailed;
		for (const auto& d : diagnostics)
			if (d.severity == Severity::Error) { result.message = (d.line ? "Line " + std::to_string(d.line) + ": " : std::string()) + d.message; break; }
		return result;
	}
	const std::string bytes = state.document.serialize();
	std::string onDisk;
	const bool exists = FileExists(m_path);
	if (exists && !ReadFileBytes(m_path, onDisk, &result.message)) { result.status = SaveStatus::WriteFailed; return result; }
	if (exists && onDisk != m_savedBytes && !overwriteExternalChanges) {
		result.status = SaveStatus::ExternalChange;
		result.message = "The file changed on disk since it was opened. Saving would overwrite those changes.";
		return result;
	}
	if (documentDirty() || (exists && onDisk != bytes)) {
		if (exists && !WriteUniqueBackup(m_path, onDisk, &result.backupPath, &result.message)) {
			result.status = SaveStatus::BackupFailed;
			return result;
		}
		if (!WriteFileAtomic(m_path.c_str(), bytes.data(), bytes.size())) {
			result.status = SaveStatus::WriteFailed;
			result.message = "Could not replace " + m_path + ". The original is unchanged" +
				(result.backupPath.empty() ? std::string(".") : " and backed up to " + result.backupPath + ".");
			return result;
		}
		m_savedBytes = bytes;
	}
	m_savedDocument = state.document;
	if (!notesReadOnly() && notesDirty()) {
		const std::string notesPath = NotesPathFor(m_path);
		const bool anyNotes = !state.notes.orphans.empty() ||
			std::any_of(state.notes.byUid.begin(), state.notes.byUid.end(), [](const auto& n) { return !n.second.empty(); });
		if (anyNotes || FileExists(notesPath)) {
			const std::string json = SerializeNotes(state.document, state.notes);
			if (!WriteFileAtomic(notesPath.c_str(), json.data(), json.size())) {
				result.status = SaveStatus::NotesWriteFailed;
				result.message = "The command file was saved, but " + notesPath + " could not be written.";
				return result;
			}
		}
		m_savedNotes = state.notes;
	}
	++m_revision;
	return result;
}

} // namespace cmdfile
