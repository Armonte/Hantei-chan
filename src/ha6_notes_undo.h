#ifndef HA6_NOTES_UNDO_H
#define HA6_NOTES_UNDO_H
#include "undo_manager.h"

// Notes join the document's existing transaction/channel mechanism. Migration
// provenance and diagnostics are session state, not undoable annotation text.
class Ha6NotesUndoChannel : public UndoChannel {
	Ha6Notes* notes;
public:
	explicit Ha6NotesUndoChannel(Ha6Notes* n) : notes(n) {}
	const char* name() const override { return "HA6 notes"; }
	Snap capture(const Snap&) override { return std::make_shared<const std::map<std::string, std::string>>(notes->notes); }
	bool equalsLive(const Snap& s) const override { return notes->notes == *std::static_pointer_cast<const std::map<std::string, std::string>>(s); }
	void restore(const Snap& s) override {
		notes->notes = *std::static_pointer_cast<const std::map<std::string, std::string>>(s);
		notes->dirty = notes->notes != notes->savedNotes;
		notes->event("undo channel restore: " + std::to_string(notes->notes.size()) + " notes");
	}
};
#endif
