#ifndef AUTHORING_SYSCONST_MODEL_H_GUARD
#define AUTHORING_SYSCONST_MODEL_H_GUARD

// MBAACC system constants: data\_type.txt (per-moon [Type_N] tuning, [System]) and data\vector.txt
// (hit vectors). Both are edited in place on the raw CP932 bytes: a value edit replaces only that
// value's bytes, so comments, spacing and every untouched line stay byte-identical (the mod project
// diff and the pcmod digest see exactly the change).

#include <cstddef>
#include <string>
#include <vector>

namespace sysconst {

// One "Key = value // comment" line of _type.txt.
struct TypeKey {
	std::string section; // "" before the first [Section]
	std::string key, value;
	std::string comment; // CP932, without the leading //
	size_t line = 0;     // 0-based
};

class TypeFile {
public:
	bool load(const std::string& path);
	bool loadText(const std::string& raw);
	bool save(const std::string& path); // atomic
	const std::string& text() const { return m_text; }
	bool dirty() const { return m_text != m_saved; }

	const std::vector<std::string>& sections() const { return m_sections; } // file order
	const std::vector<TypeKey>& keys() const { return m_keys; }               // file order
	// Replaces the value bytes of section/key (first match, case-insensitive key); false if absent.
	bool setValue(const std::string& section, const std::string& key, const std::string& value);
	const TypeKey* find(const std::string& section, const std::string& key) const;

private:
	void parse();
	std::string m_text, m_saved;
	std::vector<std::string> m_sections;
	std::vector<TypeKey> m_keys;
	std::vector<std::pair<size_t, size_t>> m_valueSpan; // byte span of each key's value in m_text
};

// vector.txt: part 1 (until END) is the vector list "id x y addX addY"; part 2 is the hit-vector table,
// a header "No name VecCnt UkemiTime Prio PrioAni KoCheck" followed by its step rows
// "VecNum HitAni GuardAni Time VecTime flag". HA6 ATVV/ATHV ids refer to part 2's No.
class VectorFile {
public:
	enum Kind { List, Header, Step };
	struct Row {
		Kind kind = List;
		size_t line = 0;
		int owner = -1;                             // Step: index of its Header row
		std::vector<std::pair<size_t, size_t>> tok; // byte spans of the fields (before any //)
		std::string comment;                        // CP932
	};

	bool load(const std::string& path);
	bool loadText(const std::string& raw);
	bool save(const std::string& path); // atomic
	const std::string& text() const { return m_text; }
	bool dirty() const { return m_text != m_saved; }

	const std::vector<Row>& rows() const { return m_rows; }
	std::string field(size_t row, size_t i) const;
	// Replaces field i of a row. Numeric fields must stay a single token (no spaces).
	bool setField(size_t row, size_t i, const std::string& value);
	int idOf(size_t row) const; // the row's first field as a number, or -1

	static const char* const kListFields[5];
	static const char* const kHeaderFields[7];
	static const char* const kStepFields[6];

private:
	void parse();
	std::string m_text, m_saved;
	std::vector<Row> m_rows;
};

} // namespace sysconst

#endif
