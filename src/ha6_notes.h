#ifndef HA6_NOTES_H_GUARD
#define HA6_NOTES_H_GUARD

// Editor annotations for a character (issue #58): notes on patterns and on
// individual effects/conditions, like code comments. They are never written
// into the HA6 (the game would not read them); they live beside the save
// target as "<file>.notes.json" (UTF-8):
//
//   { "version": 1, "notes": { "p12": "...", "p12.f3.ef1.t1": "...", "p12.f3.if0.t25": "..." } }
//
// A record note's key carries the record type, so a note is only shown on a
// record of the same type at the same slot; if the record is deleted or
// changes type the note is kept in the file and listed as unmatched in the
// Notes window instead of landing on an unrelated record.
#include <map>
#include <string>

struct Ha6Notes {
	std::map<std::string, std::string> notes;
	bool dirty = false;

	static std::string PatternKey(int pattern);
	static std::string RecordKey(int pattern, int frame, bool isEffect, int index, int type);
	// Parses a key; returns false for keys this build does not understand.
	static bool ParseKey(const std::string& key, int* pattern, int* frame, bool* isEffect, int* index, int* type);

	const std::string* get(const std::string& key) const;
	void set(const std::string& key, const std::string& text); // empty text removes

	static std::string PathFor(const std::string& ha6Path) { return ha6Path + ".notes.json"; }
	// A missing file is not an error (no notes). A malformed file sets *error.
	bool load(const std::string& path, std::string* error);
	bool save(const std::string& path) const;
};

#endif /* HA6_NOTES_H_GUARD */
