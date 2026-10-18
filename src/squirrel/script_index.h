#ifndef SQUIRREL_SCRIPT_INDEX_H_GUARD
#define SQUIRREL_SCRIPT_INDEX_H_GUARD

// Project-wide index of a French-Bread game's Squirrel scripts (every script/ and data/ .txt the VM loads): definitions,
// move tables, command entries and Mv_ references, for search, go-to-definition, find-usages, completion and the move
// checks. No UI and no VM: plain text parsing over the UTF-8 view of the CP932 files.
//
// What the move checks catch (BootySama, 2026-10-15): a cmd entry or script naming an Mv_ table that does not exist makes
// the game spawn copies of the character on button presses; a broken cmd table stops the character moving. The fix the
// modders asked for: create the missing Mv_ state, name for name, with its brackets, so the character cannot get stuck.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace sqedit {

struct ScriptDef {
	enum Kind { Function, Slot, MoveTable, Const, CmdEntry };
	Kind kind = Function;
	std::string name;    // "Init_After", "Mv_Atk_StdA", "def_PAT_Neutral", "Skill_236A"
	std::string owner;   // enclosing table / slot owner ("Battle_Std", "t", "Mv_Atk_StdA"), "" for globals
	std::string params;  // functions: "a, b" as written
	std::string value;   // consts: the value text; cmd entries: the Mv_ name their UpdateTable runs ("" if none)
	std::string doc;     // the // comment lines just above the definition and its trailing comment (UTF-8)
	std::string file;    // game-relative, '/' separators
	int line = 0;        // 0-based
	int col = 0;
};

struct ScriptHit {
	std::string file;
	int line = 0, col = 0, len = 0;
	std::string text;   // the whole line (UTF-8), for the result list
};

struct ScriptProblem {
	enum Severity { Warning, Error };
	enum Fix { NoFix, CreateMoveTable };
	Severity severity = Warning;
	std::string file;
	int line = 0, col = 0;
	std::string message;
	Fix fix = NoFix;
	std::string fixArg;   // CreateMoveTable: the Mv_ name
	std::string fixFile;  // where the fix writes (the character's _mv file)
	std::string message_suffix_hint;   // nearest existing name ("did you mean"), when known
};

struct SearchOptions {
	bool regex = false, wholeWord = false, matchCase = false;
	bool codeOnly = false;   // skip comments (strings still count: Mv_ names live in strings)
	size_t maxHits = 5000;
};

class ScriptIndex {
public:
	// rootUtf8: the folder holding script/ and data/; files: game-relative paths of the Squirrel scripts.
	// pathOf (optional): the file to read for a rel (layered mod folders, script_layers.h); "" or unset = rootUtf8/rel.
	void build(const std::string& rootUtf8, const std::vector<std::string>& files,
		const std::function<std::string(const std::string&)>& pathOf = {});
	// Re-parse one file from editor text (live editing); adds it if new.
	void updateFile(const std::string& rel, const std::string& utf8Text);

	const std::vector<ScriptDef>& defs() const { return m_defs; }
	std::vector<const ScriptDef*> find(const std::string& name) const;
	// Definitions owned by a table or slot ("BMvTbl", "Battle_Std", ...), for member completion.
	std::vector<const ScriptDef*> members(const std::string& owner) const;
	std::vector<ScriptHit> search(const std::string& query, const SearchOptions& opt) const;
	// Whole-word code occurrences of `name`, plus its string occurrences when it is an Mv_ name.
	std::vector<ScriptHit> usages(const std::string& name) const;
	const std::vector<std::string>* lines(const std::string& rel) const;
	const std::vector<std::string>& files() const { return m_files; }

	// Move checks for one character folder ("data/chr001"): every Mv_ name its cmd / mv / sc scripts use must be a
	// move table defined in that character's _mv files or the shared library (script/, e.g. Std_MoveTable).
	std::vector<ScriptProblem> checkMoves(const std::string& charDir) const;

	// The text of `utf8Text` (a character's _mv file) with an empty move table `name` inserted just before the line
	// that builds the move table (<CHR>_MoveTable <- Battle_Std.MakeMoveTable( t, ... )), using that call's table
	// variable; appended at the end when there is no such line. `lineOut` = the inserted block's first line.
	static std::string InsertMoveTableStub(const std::string& utf8Text, const std::string& name, int* lineOut = nullptr);
	// Nearest defined Mv_ name (edit distance), for "did you mean".
	std::string nearestMove(const std::string& name, const std::string& charDir) const;

private:
	struct FileData {
		std::vector<std::string> lines;   // UTF-8
		std::vector<std::string> code;    // same lines, comments blanked, string CONTENTS blanked (quotes kept)
		struct Str { int line, col; std::string text; };
		std::vector<Str> strings;         // string literals (content)
	};
	void parse(const std::string& rel, FileData& fd, std::vector<ScriptDef>& out) const;
	void rebuildDefs();

	std::string m_root;
	std::vector<std::string> m_files;
	std::map<std::string, FileData> m_data;
	std::map<std::string, std::vector<ScriptDef>> m_fileDefs;
	std::vector<ScriptDef> m_defs;
};

// Splits UTF-8 text into lines (\r\n or \n).
std::vector<std::string> SplitScriptLines(const std::string& text);

}  // namespace sqedit

#endif
