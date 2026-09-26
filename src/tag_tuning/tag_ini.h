#ifndef TAG_INI_H_GUARD
#define TAG_INI_H_GUARD
// tag_tuning.ini for the Tag / Team panel: a byte-preserving editor plus the game's resolution rules.
//
// EDITING. TagIni keeps the file as raw bytes. Every line keeps its bytes and its terminator; an edit rewrites only
// the value text of one key (spacing and a trailing comment stay), or inserts / removes whole lines. So an unedited
// save is byte-identical, comments and unknown keys survive, and the in-game F3 panel's full rewrite is the only
// thing that ever drops comments. (The same idea as the stage metadata editor's TextIni on feat/stage, with this
// file's own syntax.)
//
// SYNTAX = PovertyCaster's parseTuningIni (MbaaccTagTuning.hpp): sections [tuning], [style.<name>], [char.<file>]
// (case-insensitive, other sections ignored with a warning); `key=value`; a line starting with ; or # is a comment,
// and so is anything after a ; or # that follows a space or tab. Keys are case-insensitive.
//
// WHICH OCCURRENCE COUNTS (how the game reads it, so it is the one the editor edits):
//   [tuning]        every [tuning] section, in file order, later keys win (active_style too)
//   [char.<file>]   every section of that name, merged, later keys win
//   [style.<name>]  only the FIRST section of that name (findIniStyle)
//
// RESOLUTION = resolveTuning: defaults <- active style (ini section, else built-in) <- [tuning]; then per character
// [char.<file>] on top (perChar levers only). Unknown keys / bad values / wrong-section keys are warnings and are
// skipped, never half-applied — exactly the game's behaviour, so the panel shows what the game will do.
#include "tag_levers.h"

#include <string>
#include <vector>

namespace tagtune {

enum class SecKind : uint8_t { None, Tuning, Style, Char, Other };

struct Warning {
	int line = 0;          // 1-based, 0 = not tied to a line
	std::string msg;       // without the line number (stable across edits that move lines)
	std::string Text() const { return line ? "line " + std::to_string(line) + ": " + msg : msg; }
};

struct KeyValue { std::string key, value; int line = 0; };

class TagIni {
public:
	void LoadText(const std::string& raw);          // also marks it as the saved state
	const std::string& Text() const { return m_text; }
	const std::string& SavedText() const { return m_saved; }
	void MarkSaved() { m_saved = m_text; }
	bool IsDirty() const { return m_text != m_saved; }
	void SetText(const std::string& raw);           // replace (undo), keeps the saved state
	// [authoring] A per-character sidecar (povertycaster\tag\chars\<file>[_<moon>].ini, HANTEI_AUTHORING_MODE §9.1):
	// the bare [char] header is a Char section, [char.<name>] is its synonym, and every Char instance is in scope
	// whatever its name (the FILE names the character). A new Char section is written as "[char]".
	void SetCharFileMode(bool on) { if (m_charFile != on) { m_charFile = on; reindex(); } }
	bool CharFileMode() const { return m_charFile; }
	// Every section header of a kind (instances, file order), with its line: for the per-file warnings.
	struct SectionRef { SecKind kind; std::string name; int line; };
	std::vector<SectionRef> Sections() const;

	// Section names of a kind, first appearance order, one entry per distinct (case-insensitive) name.
	std::vector<std::string> SectionNames(SecKind k) const;
	bool HasSection(SecKind k, const std::string& name) const;
	// The effective key=value list of a section (see "which occurrence counts"): every line, in file order.
	std::vector<KeyValue> Keys(SecKind k, const std::string& name) const;
	// The effective value of one key (the last occurrence in the scope). name is ignored for Tuning.
	bool Get(SecKind k, const std::string& name, const std::string& key, std::string& value) const;
	// Replace the effective occurrence's value text in place, or add "key=value" at the end of the section's key
	// lines (creating the section at the end of the file when it does not exist).
	void Set(SecKind k, const std::string& name, const std::string& key, const std::string& value);
	// Remove every occurrence of the key in the section's scope. True if anything was removed.
	bool Remove(SecKind k, const std::string& name, const std::string& key);
	// Remove the section header(s) and their key lines (comment lines stay).
	void RemoveSection(SecKind k, const std::string& name);

	// ---- the game's reading ----
	std::string ActiveStyle() const;                 // [tuning] active_style ("" = none)
	std::vector<Warning> ParseWarnings() const;      // unknown sections, non key=value lines, keys outside sections

private:
	struct Line {
		size_t begin = 0, end = 0, next = 0;         // [begin,end) = text without the terminator; next = after it
		int inst = -1;                               // section instance index (-1 = before any section)
		bool header = false, kv = false, junk = false;
		size_t keyB = 0, keyE = 0, valB = 0, valE = 0;
	};
	struct Inst { SecKind kind = SecKind::None; std::string name; size_t headerLine = 0; };
	void reindex();
	std::vector<int> scope(SecKind k, const std::string& name) const;   // instance indices in scope
	std::string eol() const;
	void insertAt(size_t pos, const std::string& bytes);
	std::string key(const Line& l) const { return m_text.substr(l.keyB, l.keyE - l.keyB); }
	std::string value(const Line& l) const { return m_text.substr(l.valB, l.valE - l.valB); }

	std::string m_text, m_saved;
	bool m_charFile = false;
	std::vector<Line> m_lines;
	std::vector<Inst> m_inst;
};

// ---- resolution (mirrors resolveTuning / tuningForChar) ----
// Apply key=value pairs as the game's applyKvs does. perChar = a [char] section.
void ApplyKvs(Values& v, const std::vector<KeyValue>& kvs, const std::string& where, std::vector<Warning>& warn,
              bool perChar);
// defaults <- the named style (ini section first, then built-in). False (+ a warning) for an unknown name.
bool StyleBase(const TagIni& ini, const std::string& name, Values& out, std::vector<Warning>& warn);
struct Resolved {
	Values global{};
	std::string style;           // the active style actually used ("" = defaults)
	bool styleFound = true;
	std::vector<Warning> warnings;
};
Resolved Resolve(const TagIni& ini);
// The values one character plays with: global + its [char.<file>] keys.
Values ForChar(const TagIni& ini, const Values& global, const std::string& file, std::vector<Warning>* warn = nullptr);
// Every style the game's picker offers: built-ins, then ini-only ones.
std::vector<std::string> StyleNames(const TagIni& ini);
// Every warning the game would log for this file (parse + resolution + every [char] section).
std::vector<Warning> AllWarnings(const TagIni& ini);
// The warnings in `now` that are not in `before` (by message; line numbers ignored).
std::vector<Warning> NewWarnings(const std::vector<Warning>& before, const std::vector<Warning>& now);

// ---- panel operations (the F3 panel's semantics, but surgical) ----
// Pick a style: active_style=name; with dropOverrides the [tuning] lever keys go (F3 iniSelectStyle does that).
void SelectStyle(TagIni& ini, const std::string& name, bool dropOverrides);
// Save as style (F3 iniSaveAsStyle): [style.<name>] = the levers where `live` differs from the defaults, the style
// becomes active, the [tuning] lever keys go. Unknown keys in either section are kept.
void SaveAsStyle(TagIni& ini, const std::string& name, const Values& live);
// A [tuning] override: set / clear.
void SetTuningLever(TagIni& ini, int lever, int32_t v);
void ClearTuningLever(TagIni& ini, int lever);
// A [char.<file>] override (file is lower-cased for a new section, as the game writes it).
void SetCharLever(TagIni& ini, const std::string& file, int lever, int32_t v);
void ClearCharLever(TagIni& ini, const std::string& file, int lever);

} // namespace tagtune

#endif
