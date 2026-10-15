#ifndef SQUIRREL_SCRIPT_DOCUMENT_H_GUARD
#define SQUIRREL_SCRIPT_DOCUMENT_H_GUARD

// CP932 document model for French-Bread Squirrel scripts (.txt), docs/squirrel/SCOPE.md §1.8 / §2.5.
//
// The file is kept as raw lines (bytes + their own terminator). The editor sees UTF-8 lines decoded
// with the cmdfile codec (cmdfile::Cp932ToUtf8). Saving re-encodes ONLY the lines whose UTF-8 text
// changed; every untouched line is written back from its original bytes, with its original terminator.
// So an unmodified document, or one whose edits were undone, is byte-identical to the file on disk,
// including CP932 sequences that do not survive a CP932->Unicode->CP932 trip (NEC/IBM duplicates,
// invalid bytes shown as U+FFFD), mixed CRLF/LF, and a missing final newline.

#include <cstddef>
#include <string>
#include <vector>

namespace sqedit {

struct RawLine {
	std::string bytes; // CP932, without terminator
	std::string eol;   // "\r\n", "\n", "\r" or "" (last line without terminator)
	std::string utf8;  // display text
};

struct EncodeError {
	int line = 0; // 0-based line in the new text
	std::string message;
};

class ScriptDocument {
public:
	// Parses raw file bytes.
	void loadBytes(const std::string& bytes);
	const std::string& originalBytes() const { return m_original; }

	// UTF-8 text for the editor; lines joined with '\n'.
	std::string utf8Text() const;
	std::vector<std::string> utf8Lines() const;

	// Builds the CP932 file bytes for the edited UTF-8 lines. Lines equal to an original line are
	// written from the original bytes; changed lines are strictly re-encoded (no '?' substitution).
	// Returns false and fills *errors when some line has a character CP932 cannot represent.
	bool encode(const std::vector<std::string>& utf8Lines, std::string& out, std::vector<EncodeError>* errors) const;

	// After a successful save: the encoded bytes become the new original.
	void commit(const std::string& bytes) { loadBytes(bytes); }

	std::size_t lineCount() const { return m_lines.size(); }
	const std::vector<RawLine>& lines() const { return m_lines; }
	// Terminator used for newly inserted lines (the file's most common one).
	const std::string& defaultEol() const { return m_defaultEol; }

private:
	std::string m_original;
	std::vector<RawLine> m_lines;
	std::string m_defaultEol = "\r\n";
};

// Splits text on '\n' (the editor's line model). "a\nb" -> {"a","b"}; "" -> {""}.
std::vector<std::string> SplitLines(const std::string& text);

} // namespace sqedit

#endif
