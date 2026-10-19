#ifndef HA6_NOTES_H_GUARD
#define HA6_NOTES_H_GUARD

// Editor annotations for a character (issue #58): notes on patterns, states and
// individual effects/conditions, like code comments. They are never written
// into the HA6 (the game would not read them); they live beside the save
// target as "<file>.notes.json" (UTF-8):
//
//   { "version": 1, "notes": { "p12": "...", "p12.f3": "...", "p12.f3.ef1.t1": "...", "p12.f3.if0.t25": "..." } }
//
// A record note's key carries the record type, so a note is only shown on a
// record of the same type at the same slot; if the record is deleted or
// changes type the note is kept in the file and listed as unmatched in the
// Notes window instead of landing on an unrelated record.
#include <map>
#include <string>
#include <vector>
template<template<typename> class Allocator> struct Sequence_T;

struct Ha6Notes {
	std::map<std::string, std::string> notes;
	bool dirty = false;
	std::string legacyPayload; // Original bytes retained until explicit save/backup.
	std::map<std::string, std::string> savedNotes;
	void markSaved() { savedNotes = notes; dirty = false; }
	std::vector<std::string> trace; // Bounded diagnostics; never annotation text.

	static std::string PatternKey(int pattern);
	static std::string StateKey(int pattern, int frame);
	static std::string RecordKey(int pattern, int frame, bool isEffect, int index, int type);
	// Parses a key; returns false for keys this build does not understand.
	static bool ParseKey(const std::string& key, int* pattern, int* frame, bool* isEffect, int* index, int* type);

	const std::string* get(const std::string& key) const;
	void set(const std::string& key, const std::string& text); // empty text removes

	static std::string PathFor(const std::string& ha6Path) { return ha6Path + ".notes.json"; }
	// A missing file is not an error (no notes). A malformed file sets *error.
	bool load(const std::string& path, std::string* error);
	bool loadForHa6(const std::string& path, const std::vector<Sequence_T<std::allocator>>& sequences, std::string* error);
	bool save(const std::string& path) const;
	// Old index -> new index; -1 archives the note as unmatched. Apply before
	// mutating the corresponding vector, in the same document undo transaction.
	void remap(int pattern, int frame, bool isEffect, const std::vector<int>& mapping);
	void insert(int pattern, int frame, bool isEffect, int at, int count, int oldSize);
	void erase(int pattern, int frame, bool isEffect, int at, int oldSize);
	void move(int pattern, int frame, bool isEffect, int from, int to, int size);
	void cloneState(int pattern, int source, int destination);
	void archivePattern(int pattern);
	void archiveState(int pattern, int frame);
	void movePatterns(const std::vector<int>& from, const std::vector<int>& to);
	void event(const std::string& message);
};

#endif /* HA6_NOTES_H_GUARD */
