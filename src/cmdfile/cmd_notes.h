#ifndef CMDFILE_CMD_NOTES_H_GUARD
#define CMDFILE_CMD_NOTES_H_GUARD

// Editor notes for a command file, stored beside it as "<file>.notes.json" (UTF-8).
//
// In memory, a note belongs to a record's line uid, so it follows the record through
// reorders, inserts and deletes of other rows. On disk it is keyed by content, never by
// row position:
//   command:<ID>#<n>   n-th active command with that numeric ID (0 for unique IDs)
//   excom:<CheckNum>#<n>  n-th ExComCheck row linked to that command ID
// Notes whose key no longer matches anything are kept as "orphaned" and written back, so a
// note is never dropped silently. A notes file that fails to parse is reported and is never
// overwritten.

#include "cmd_document.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cmdfile {

struct OrphanNote {
	std::string key;
	std::string hint; // input / type recorded when the note was saved
	std::string text;
};

struct Notes {
	std::map<std::uint64_t, std::string> byUid; // UTF-8
	std::vector<OrphanNote> orphans;
	bool operator==(const Notes& o) const;
	bool operator!=(const Notes& o) const { return !(*this == o); }
};

std::string NotesPathFor(const std::string& commandFilePath);
std::string CommandNoteKey(const Document& doc, std::size_t commandRow);
std::string CheckNoteKey(const Document& doc, std::size_t checkRow);

// Returns false and sets *error when the JSON is malformed or has the wrong shape.
bool ParseNotes(const std::string& json, const Document& doc, Notes& out, std::string* error);
std::string SerializeNotes(const Document& doc, const Notes& notes);
// Drop notes whose uid no longer names a record (the record was deleted). Returns the
// removed notes as orphans-to-be so callers can decide; the editor keeps them in undo.
void PruneNotes(const Document& doc, Notes& notes);

} // namespace cmdfile

#endif
