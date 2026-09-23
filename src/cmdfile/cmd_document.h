#ifndef CMDFILE_CMD_DOCUMENT_H_GUARD
#define CMDFILE_CMD_DOCUMENT_H_GUARD

// Lossless model of a Melty Blood character command file (MBAACC `<chr>_<moon>_c.txt`,
// MBAC `<CHR>_C.TXT`).
//
// The document is a list of physical lines. Each line keeps its exact bytes and its own
// terminator, so an unmodified document serializes back byte-for-byte (CP932 comments,
// tabs, trailing blanks, a missing final newline, mixed line endings). Records (commands,
// ExComChecks, TeamChangeData) are derived views over those lines and are recomputed after
// every edit. Edits are surgical: they rewrite only the tokens they change, or insert and
// remove whole lines.
//
// Engine facts this model follows (MBAA.exe, IDA session 212fe94c):
//  - LoadCharacterCommandFile 0x46ba10: every line before the first line starting with
//    "END" that is not blank and not a "//" comment is read as a command. Fields are
//    ID, input, flagset1, flagset2, pattern, meter, then three bytes: team/solo (read by
//    Command_CheckCmdVars, 1 = team only, 2 = solo only), projectile limit ("tobi") and
//    air-dash limit. Missing fields read as 0; extra tokens are ignored.
//  - Duplicate command IDs: the first definition wins; later ones are discarded.
//  - Lines end only at CR (0x0D). A file that uses bare LF breaks the command parse.
//  - Everything after END is read by global key lookup ("Num", "%03d_CheckNum", ...),
//    not per section. MBAACC has no TeamChangeData reader; MBAC reads it from the
//    compiled _C.CT (see docs/tag_research/STATE_COMPARISON_MBAC_vs_MBAACC.md).
//  - ExComCheck (Command_CheckExComConditions 0x46d3d0): every row whose CheckNum equals
//    the command ID must pass. Only Type 0 and Type 1 exist in vanilla; any other type
//    fails, so the command can never be used.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cmdfile {

enum class Dialect { MBAACC, MBAC };

enum class LineKind {
	Blank,
	Comment,          // "//" comment (command region or tail)
	CommentedCommand, // command region "//" line whose body is a full command row
	Command,
	MalformedCommand, // command region line the engine reads as a command, but with < 9 fields
	End,              // first END (end of the command region)
	SectionHeader,    // [Name]
	Assignment,       // key = value
	Other,            // anything else after END (MBAC tables, extra END markers, notes)
};

struct Line {
	std::uint64_t uid = 0;
	std::string text; // bytes without the terminator
	std::string eol;  // "\r\n", "\n" or "" (last line without a terminator)
};

enum CommandField {
	CF_Id = 0, CF_Input, CF_Flags1, CF_Flags2, CF_Pattern, CF_Meter,
	CF_TeamSolo, CF_Projectile, CF_AirDash, CF_Count
};

struct CommandRecord {
	std::uint64_t uid = 0;       // uid of its line (stable across edits in a session)
	std::size_t line = 0;        // physical line index
	std::array<std::string, CF_Count> fields;
	std::size_t fieldCount = 0;  // tokens actually present (may be < 9 for malformed rows)
	std::size_t extraTokens = 0; // tokens beyond the ninth (ignored by the engine)
	bool hasComment = false;
	std::string comment;         // raw CP932 bytes after "//", trimmed
};

enum ExComField { XF_CheckNum = 0, XF_Type, XF_P0, XF_P1, XF_P2, XF_P3, XF_P4, XF_Count };

struct ExComRecord {
	std::uint64_t uid = 0;  // uid of its first field line
	int index = 0;          // NNN key prefix
	std::size_t keyDigits = 3;
	std::array<std::optional<std::string>, XF_Count> values;
	std::array<int, XF_Count> lineOf{ -1, -1, -1, -1, -1, -1, -1 };
	std::string comment;    // comment on the CheckNum line (raw CP932)
	int firstLine() const;
	int lastLine() const;
};

struct TeamChangeData {
	bool present = false;
	std::size_t headerLine = 0;
	int valueLine = -1;
	bool assignmentForm = false; // MBAACC "test = 235, 236" vs MBAC "241 242"
	std::optional<int> tagIn, tagOut;
};

struct Assignment {
	std::size_t line = 0;
	std::string section; // without brackets, empty before the first header
	std::string key;
	std::string value;
};

struct CommandFields {
	std::array<std::string, CF_Count> fields;
	std::string comment; // raw CP932
};

struct ExComFields {
	std::array<std::optional<std::string>, XF_Count> values;
	std::string comment; // raw CP932
};

class Document {
public:
	static Document parse(const std::string& bytes);
	std::string serialize() const;

	const std::vector<Line>& lines() const { return m_lines; }
	LineKind kind(std::size_t line) const { return m_kinds[line]; }
	std::optional<std::size_t> lineIndexOf(std::uint64_t uid) const;

	// Derived views (recomputed by every edit).
	std::vector<CommandRecord> commands;
	std::vector<ExComRecord> checks;
	std::vector<Assignment> assignments;
	TeamChangeData teamChange;
	std::optional<std::size_t> endLine;
	std::optional<std::size_t> exComHeaderLine;
	std::optional<std::size_t> exComNumLine;
	std::optional<long long> declaredExComCount;
	std::vector<std::size_t> strayExComKeyLines; // NNN_ keys outside [ExComCheck]
	Dialect dialect = Dialect::MBAACC;
	std::string eolStyle = "\r\n"; // terminator used for new lines

	int findCommandRow(std::uint64_t uid) const;
	int findCheckRow(std::uint64_t uid) const;
	std::vector<std::size_t> checksForCommand(const std::string& commandId) const;
	std::vector<std::size_t> commandsWithId(const std::string& commandId) const;
	std::string commandRegionText(std::size_t line) const; // comment body for display
	// For a CommentedCommand line, the command fields it would restore.
	std::optional<CommandFields> commentedCommand(std::size_t line) const;

	// ---- command region edits (all return false and set *error on failure) ----
	bool setCommandField(std::size_t row, int field, const std::string& value, std::string* error = nullptr);
	bool setCommandComment(std::size_t row, const std::string& cp932, std::string* error = nullptr);
	// anchorUid: a command-region line (command or comment); 0 = append before END.
	std::uint64_t insertCommand(std::uint64_t anchorUid, bool after, const CommandFields& fields, std::string* error = nullptr);
	std::uint64_t insertCommentLines(std::uint64_t anchorUid, bool after, const std::vector<std::string>& cp932Lines, std::string* error = nullptr);
	bool deleteLines(const std::vector<std::uint64_t>& uids, std::string* error = nullptr); // command region only
	bool commentOut(const std::vector<std::uint64_t>& commandUids, std::string* error = nullptr);
	bool uncomment(std::uint64_t lineUid, std::string* error = nullptr);
	bool replaceCommentLine(std::uint64_t lineUid, const std::string& cp932, std::string* error = nullptr);
	// Moves command-region lines (commands or comments) before/after target, keeping their order.
	bool moveLines(const std::vector<std::uint64_t>& uids, std::uint64_t targetUid, bool after, std::string* error = nullptr);

	// ---- ExComCheck edits (renumbering is confined to the [ExComCheck] section) ----
	bool setCheckValue(std::size_t row, int field, const std::optional<std::string>& value, std::string* error = nullptr);
	bool setCheckComment(std::size_t row, const std::string& cp932, std::string* error = nullptr);
	// anchorRow < 0 appends at the end of the section (creating it when needed).
	std::uint64_t insertCheck(int anchorRow, bool after, const ExComFields& fields, std::string* error = nullptr);
	bool deleteChecks(const std::vector<std::size_t>& rows, std::string* error = nullptr);
	bool moveCheck(std::size_t sourceRow, std::size_t targetRow, bool after, std::string* error = nullptr);

	// ---- TeamChangeData ----
	bool setTeamChange(int tagIn, int tagOut, std::string* error = nullptr);

private:
	std::vector<Line> m_lines;
	std::vector<LineKind> m_kinds;
	std::uint64_t m_nextUid = 1;

	void reindex();
	std::uint64_t newUid() { return m_nextUid++; }
	Line makeLine(std::string text);
	void ensureTerminated(std::size_t line);
	bool renumberExCom(std::string* error);
	std::size_t checkBlockStart(std::size_t row) const; // one past the last line of the section
	bool inCommandRegion(std::size_t line) const { return !endLine || line < *endLine; }
};

// Helpers shared with the validator and UI.
std::vector<std::pair<std::size_t, std::size_t>> TokenSpans(const std::string& text, std::size_t limit);
std::size_t CommentMarker(const std::string& text); // npos when none
bool IsDecimal(const std::string& value, bool allowSign = true);
std::optional<long long> ParseInteger(const std::string& value);
const char* CommandFieldName(int field);
const char* ExComFieldName(int field); // "CheckNum", "Type", "P0".."P4"

} // namespace cmdfile

#endif
