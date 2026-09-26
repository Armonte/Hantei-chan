// Command-file editor tests (ported and extended from Gonptechan EX 1abc27f9:
// mbaacc_command_workspace_test.cpp and the --structural / --mutation / --metadata
// self-tests of mbaacc_command_check_cli.cpp).
//
//   cmdfile_test [FIXTURE_DIR] [EXTRA_COMMAND_FILES...]
//
// FIXTURE_DIR defaults to tests/fixtures/cmdfile next to the executable's source tree.
// Every extra file gets the per-file structural suite (use it on the game's data folder).
// Exit code is the number of failed checks (0 = pass).

#include "cmdfile/cmd_document.h"
#include "cmdfile/cmd_io.h"
#include "cmdfile/cmd_notes.h"
#include "cmdfile/cmd_validate.h"
#include "cmdfile/cmd_workspace.h"

#include <windows.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace cmdfile;

namespace {

int g_failures = 0;
int g_checks = 0;
std::string g_context;

void Check(bool ok, const std::string& what)
{
	++g_checks;
	if (ok) return;
	++g_failures;
	std::cout << "FAIL [" << g_context << "] " << what << "\n";
}

std::vector<std::string> ListFiles(const std::string& dir)
{
	std::vector<std::string> out;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return out;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(dir + "\\" + fd.cFileName);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	std::sort(out.begin(), out.end());
	return out;
}

std::string TempDir()
{
	char base[MAX_PATH];
	GetTempPathA(MAX_PATH, base);
	std::string dir = std::string(base) + "hantei_cmdfile_test_" + std::to_string(GetCurrentProcessId());
	CreateDirectoryA(dir.c_str(), nullptr);
	return dir;
}

void RemoveTree(const std::string& dir)
{
	for (const auto& f : ListFiles(dir)) DeleteFileA(f.c_str());
	for (const auto& f : ListFiles(dir + "\\.hantei-backups")) DeleteFileA(f.c_str());
	RemoveDirectoryA((dir + "\\.hantei-backups").c_str());
	RemoveDirectoryA(dir.c_str());
}

bool WriteBytes(const std::string& path, const std::string& bytes)
{
	HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD w = 0;
	const bool ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &w, nullptr) && w == bytes.size();
	CloseHandle(h);
	return ok;
}

// ---------------------------------------------------------------------------------------
// WorkspaceHistory (EX mbaacc_command_workspace_test, adapted)

WorkspaceSnapshot Marker(const char* text)
{
	WorkspaceSnapshot s;
	s.document = Document::parse(std::string(text) + "\r\nEND\r\n");
	return s;
}

std::string FirstLine(const WorkspaceSnapshot& s) { return s.document.lines().empty() ? std::string() : s.document.lines()[0].text; }

void TestHistory()
{
	g_context = "history";
	WorkspaceHistory history(2);
	history.record(Marker("A"));
	history.record(Marker("B"));
	WorkspaceSnapshot current = Marker("C");
	Check(history.undo(current) && FirstLine(current) == "B" && history.canRedo(), "undo restores the last transaction");
	Check(history.redo(current) && FirstLine(current) == "C", "redo restores the current state");
	history.record(Marker("C"));
	Check(!history.canRedo(), "recording a new step clears redo");
	Check(history.undoDepth() == 2, "capacity limit drops the oldest step");
	WorkspaceSnapshot rolled = Marker("X");
	Check(history.rollback(rolled) && FirstLine(rolled) == "C" && !history.canRedo(), "rollback leaves redo untouched");
	history.clear();
	Check(!history.canUndo() && !history.canRedo(), "clear");

	Workspace ws;
	ws.openFromBytes("memory_c.txt", "1  5A  00000101 00000000  1  0  0 0 0  // x\r\nEND\r\n");
	const std::string before = ws.state.document.serialize();
	Check(ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "77"); }), "apply edit");
	ws.state.notes.byUid[ws.state.document.commands[0].uid] = "note";
	Check(ws.dirty() && ws.undo() && ws.state.document.serialize() == before, "document-level undo restores the whole workspace");
	Check(ws.redo() && ws.state.document.commands[0].fields[CF_Pattern] == "77", "document-level redo");
	Check(!ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "bad value"); }) &&
		ws.state.document.commands[0].fields[CF_Pattern] == "77", "failed edit rolls back");
	ws.revert();
	Check(ws.state.document.serialize() == before && !ws.documentDirty(), "revert returns to the saved bytes");
	Check(ws.undo() && ws.state.document.commands[0].fields[CF_Pattern] == "77", "revert is undoable");
}

std::size_t ErrorCount(const Document& d)
{
	return CountSeverity(Validate(d, ExtensionProfile::Vanilla), Severity::Error) +
		CountSeverity(Validate(d, ExtensionProfile::Extended), Severity::Error);
}

// ---------------------------------------------------------------------------------------
// Per-file structural suite (EX --structural / --mutation, plus byte-exact inverses)

void TestFile(const std::string& path)
{
	g_context = path.substr(path.find_last_of("\\/") + 1);
	std::string bytes;
	if (!ReadFileBytes(path, bytes)) { Check(false, "read"); return; }
	const Document original = Document::parse(bytes);
	Check(original.serialize() == bytes, "unmodified save is byte-identical");
	if (!original.endLine) return; // fixtures without END only test the round trip

	// Surgical mutation: only the edited lines change, values reparse.
	if (!original.commands.empty()) {
		Document d = original;
		const std::string newPattern = d.commands[0].fields[CF_Pattern] == "9876" ? "9875" : "9876";
		Check(d.setCommandField(0, CF_Pattern, newPattern), "set pattern");
		Check(d.setCommandComment(0, "round trip comment"), "set comment");
		const Document re = Document::parse(d.serialize());
		Check(re.commands.size() == original.commands.size() && re.commands[0].fields[CF_Pattern] == newPattern &&
			re.commands[0].comment == "round trip comment", "mutation reparses");
		std::size_t changed = 0;
		for (std::size_t i = 0; i < std::min(re.lines().size(), original.lines().size()); ++i)
			if (re.lines()[i].text != original.lines()[i].text) ++changed;
		Check(changed == 1 && re.lines().size() == original.lines().size(), "mutation touches exactly one line");
		for (int f = 0; f < CF_Count; ++f)
			if (f != CF_Pattern) Check(re.commands[0].fields[f] == original.commands[0].fields[f], std::string("column untouched: ") + CommandFieldName(f));
	}
	if (!original.checks.empty() && original.checks[0].values[XF_P0]) {
		Document d = original;
		Check(d.setCheckValue(0, XF_P0, std::string("777")), "set ExComCheck P0");
		Check(Document::parse(d.serialize()).checks[0].values[XF_P0] == std::string("777"), "P0 reparses");
		Check(d.setCheckValue(0, XF_P0, original.checks[0].values[XF_P0]) && d.serialize() == bytes, "P0 edit reverts byte-exactly");
	}

	// Comment-out then restore gives the original bytes back.
	if (original.commands.size() > 3) {
		Document d = original;
		const auto a = d.commands[1].uid, b = d.commands[3].uid;
		Check(d.commentOut({ a, b }) && d.commands.size() + 2 == original.commands.size(), "comment out two commands");
		Check(d.kind(*d.lineIndexOf(a)) == LineKind::CommentedCommand, "commented line is recognised as a command");
		Check(d.uncomment(a) && d.uncomment(b) && d.serialize() == bytes, "uncomment restores exact bytes");
	}

	// Insert then delete a command: byte-identical.
	if (!original.commands.empty()) {
		Document d = original;
		CommandFields f;
		f.fields = { "999", "6+C", "00000101", "00000000", "999", "0", "0", "0", "0" };
		f.comment = "STRUCTURAL COMMAND TEST";
		const auto uid = d.insertCommand(d.commands[0].uid, true, f);
		Check(uid != 0 && d.commands.size() == original.commands.size() + 1 && d.commands[1].fields[CF_Id] == "999", "insert command after the first");
		Check(ErrorCount(d) <= ErrorCount(original), "inserted command adds no errors");
		Check(d.deleteLines({ uid }) && d.serialize() == bytes, "delete inserted command restores bytes");
		const auto end = d.insertCommand(0, false, f);
		Check(end != 0 && *d.lineIndexOf(end) + 1 == *d.endLine, "append lands right before END");
		const auto comment = d.insertCommentLines(d.commands[0].uid, false, { "COMMENT NODE TEST" });
		Check(comment != 0 && d.replaceCommentLine(comment, "EDITED COMMENT") && d.serialize().find("// EDITED COMMENT") != std::string::npos, "comment line insert and edit");
		Check(d.deleteLines({ comment, end }) && d.serialize() == bytes, "comment/command deletion restores bytes");
	}

	// Reorder commands: move the last before the first and back.
	if (original.commands.size() > 2) {
		Document d = original;
		const auto first = d.commands.front().uid, last = d.commands.back().uid;
		const auto lastLine = *d.lineIndexOf(last);
		const auto after = d.lines()[lastLine - 1].uid;
		Check(d.moveLines({ last }, first, false) && d.commands.front().uid == last && d.commands.size() == original.commands.size(), "move last command to the top");
		Check(d.moveLines({ last }, after, true) && d.serialize() == bytes, "moving it back restores bytes");
		// Batch move of two commands below the last one keeps the count.
		Document e = original;
		Check(e.moveLines({ e.commands[0].uid, e.commands[1].uid }, e.commands.back().uid, true) &&
			e.commands.size() == original.commands.size() && e.commands.back().uid == original.commands[1].uid, "batch move keeps order");
	}

	// ExComCheck structural edits.
	if (original.checks.size() > 2) {
		Document d = original;
		const auto count = d.checks.size();
		const auto moved = d.checks.back().values[XF_CheckNum];
		Check(d.moveCheck(count - 1, 0, false) && d.checks.front().values[XF_CheckNum] == moved && d.checks.size() == count &&
			d.declaredExComCount == static_cast<long long>(count), "move last ExComCheck to the top");
		Check(ErrorCount(d) <= ErrorCount(original), "reordered checks add no errors");
		Check(d.moveCheck(0, count - 1, true) && d.serialize() == bytes, "moving it back restores bytes");
		ExComFields added;
		added.values = { d.commands.front().fields[CF_Id], std::string("1"), std::string("1400"), std::string("5"), std::nullopt, std::nullopt, std::nullopt };
		added.comment = "STRUCTURAL TEST";
		const auto uid = d.insertCheck(0, true, added);
		Check(uid != 0 && d.checks.size() == count + 1 && d.checks[1].uid == uid && d.declaredExComCount == static_cast<long long>(count + 1), "insert ExComCheck after row 0");
		Check(ErrorCount(d) <= ErrorCount(original), "inserted check adds no errors");
		Check(d.deleteChecks({ 1 }) && d.serialize() == bytes, "deleting it restores bytes");
	}
}

// ---------------------------------------------------------------------------------------
// Targeted regressions for the EX bugs fixed in this port.

const char* kSmall =
	"// header\r\n"
	"10  5A   00000101 00000000  1  0  0 0 0  // one\r\n"
	"11  5B   00000101 00000000  2  0  1 0 0  // two\r\n"
	"12  5C   20000101 00000001  3  0  2 61 0  // three\r\n"
	"END\r\n"
	"\r\n"
	"[TeamChangeData]\r\n"
	"test = 235, 236\r\n"
	"\r\n"
	"[ExComCheck]\r\n"
	"Num = 2\r\n"
	"000_CheckNum = 10 // first\r\n"
	"000_Type = 1\r\n"
	"000_P0 = 1400\r\n"
	"000_P1 = 5\r\n"
	"001_CheckNum = 12\r\n"
	"001_Type = 0\r\n"
	"001_P0 = 0\r\n"
	"001_P1 = 3\r\n"
	"\r\n"
	"[Other]\r\n"
	"000_Note = keep me\r\n"
	"005_Label = keep me too\r\n";

void TestRegressions()
{
	g_context = "regressions";
	const Document base = Document::parse(kSmall);
	Check(base.serialize() == kSmall, "small fixture round trip");
	Check(base.commands.size() == 3 && base.checks.size() == 2, "records parsed (ExComCheck rows are not commands)");
	Check(base.teamChange.present && base.teamChange.tagIn == 235 && base.teamChange.tagOut == 236 && base.teamChange.assignmentForm, "MBAACC TeamChangeData");
	Check(base.commands[2].fields[CF_TeamSolo] == "2" && base.commands[2].fields[CF_Projectile] == "61", "team/solo is the first trailing column");

	// Renumbering stays inside [ExComCheck].
	{
		Document d = base;
		ExComFields f;
		f.values = { std::string("11"), std::string("1"), std::string("100"), std::string("5"), std::nullopt, std::nullopt, std::nullopt };
		Check(d.insertCheck(-1, true, f) != 0 && d.checks.size() == 3, "append check");
		Check(d.deleteChecks({ 0 }) && d.checks.size() == 2 && d.checks[0].values[XF_CheckNum] == std::string("12"), "delete first check renumbers");
		const std::string out = d.serialize();
		Check(out.find("000_Note = keep me\r\n") != std::string::npos && out.find("005_Label = keep me too") != std::string::npos,
			"NNN_ keys in other sections are not renumbered");
		Check(out.find("Num = 2\r\n") != std::string::npos, "Num updated");
	}

	// Duplicate IDs: warning in Vanilla, error in Extended.
	{
		Document d = base;
		Check(d.setCommandField(1, CF_Id, "010"), "make a numeric duplicate (010 == 10)");
		const auto v = Validate(d, ExtensionProfile::Vanilla);
		const auto e = Validate(d, ExtensionProfile::Extended);
		Check(!HasErrors(v), "duplicate ID does not block a vanilla save");
		Check(HasErrors(e), "duplicate ID blocks an Extended save");
	}

	// BOF Types are gated.
	{
		Document d = base;
		Check(d.setCheckValue(1, XF_Type, std::string("2")) && d.setCheckValue(1, XF_P0, std::string("30")) && d.setCheckValue(1, XF_P1, std::string("0")) &&
			d.setCheckValue(1, XF_P2, std::string("1")) && d.setCheckValue(1, XF_P3, std::string("0")) && d.setCheckValue(1, XF_P4, std::string("0")), "author a Type 2 row");
		Check(HasErrors(Validate(d, ExtensionProfile::Vanilla)), "Type 2 is an error for vanilla MBAACC");
		Check(!HasErrors(Validate(d, ExtensionProfile::Extended)), "valid Type 2 passes the Extended profile");
		Check(d.setCheckValue(1, XF_P4, std::string("1")) && HasErrors(Validate(d, ExtensionProfile::Extended)), "BOF reserved P4 rule enforced");
		Check(ExComTypes(ExtensionProfile::Vanilla).size() == 2 && ExComTypes(ExtensionProfile::Extended).size() == 4, "type registry is profile-gated");
		Check(d.setCheckValue(1, XF_P4, std::nullopt) && !Document::parse(d.serialize()).checks[1].values[XF_P4], "optional field removal");
	}

	// Engine-shaped problems.
	{
		std::string lf = kSmall;
		std::string::size_type p;
		while ((p = lf.find("\r\n")) != std::string::npos) lf.replace(p, 2, "\n");
		const Document d = Document::parse(lf);
		Check(d.serialize() == lf, "LF file round trip");
		Check(HasErrors(Validate(d, ExtensionProfile::Vanilla)), "LF-only command lines are reported (engine splits at CR)");
		Check(d.eolStyle == "\n", "new lines follow the file's LF style");
	}
	{
		const std::string s = "10 5A 00000101 00000000 1 0 0 0 0\r\n[Oops]\r\nEND\r\n";
		const Document d = Document::parse(s);
		Check(d.kind(1) == LineKind::MalformedCommand, "non-comment line before END is a (malformed) command");
		Check(!HasErrors(Validate(d, ExtensionProfile::Vanilla)), "malformed rows warn only");
	}

	// CP932 trail bytes: ']' (0x5D) and '/' neighbours inside Shift-JIS characters.
	{
		const std::string s = std::string("1 5A 00000101 00000000 1 0 0 0 0 // \x83\x5D\x83\x5D\r\nEND\r\n[Te\x83\x5Dst]\r\nKey = 1 // \x81\x5E\r\n");
		const Document d = Document::parse(s);
		Check(d.serialize() == s, "CP932 round trip");
		Check(d.commands.size() == 1 && d.commands[0].comment == "\x83\x5D\x83\x5D", "CP932 comment bytes kept raw");
		Check(d.assignments.size() == 1 && d.assignments[0].section == std::string("Te\x83\x5Dst"), "trail byte ']' does not end the header");
	}

	// No final newline: an insert still produces well-formed lines.
	{
		const std::string s = "10 5A 00000101 00000000 1 0 0 0 0\r\nEND";
		Document d = Document::parse(s);
		Check(d.serialize() == s, "no-final-EOL round trip");
		ExComFields f;
		f.values = { std::string("10"), std::string("1"), std::string("1400"), std::string("5"), std::nullopt, std::nullopt, std::nullopt };
		Check(d.insertCheck(-1, true, f) != 0, "create [ExComCheck] section");
		const Document re = Document::parse(d.serialize());
		Check(re.checks.size() == 1 && re.declaredExComCount == 1 && re.exComHeaderLine && !HasErrors(Validate(re, ExtensionProfile::Vanilla)), "created section parses");
	}

	// TeamChangeData edit (MBAC form).
	{
		Document d = Document::parse("1 5A 00000101 00000000 1 0 0 0 0\r\nEND\r\n\r\n[TeamChangeData]\r\n241 242\r\n\r\n0 0 0\r\nEND\r\n");
		Check(d.dialect == Dialect::MBAC && d.teamChange.tagIn == 241 && !d.teamChange.assignmentForm, "MBAC TeamChangeData");
		Check(d.setTeamChange(188, 1890) && d.teamChange.tagIn == 188 && d.teamChange.tagOut == 1890, "TeamChangeData edit");
		Check(d.serialize().find("[TeamChangeData]\r\n188 1890\r\n") != std::string::npos, "TeamChangeData written in place");
	}

	// Column alignment on edits.
	{
		Document d = Document::parse("10     623A 10000101 00000001  160   0      0 0 0  // x\r\nEND\r\n");
		Check(d.setCommandField(0, CF_Pattern, "1600") && d.lines()[0].text == "10     623A 10000101 00000001  1600  0      0 0 0  // x", "wider value absorbs padding");
		Check(d.setCommandField(0, CF_Pattern, "7") && d.lines()[0].text == "10     623A 10000101 00000001  7     0      0 0 0  // x", "narrower value pads");
	}
}

void TestNotes()
{
	g_context = "notes";
	Document d = Document::parse(kSmall);
	Notes notes;
	const auto target = d.commands[2].uid; // ID 12
	notes.byUid[target] = "EX-style positional keys would lose this";
	notes.byUid[d.checks[1].uid] = "check note";
	// Reorder: move command 12 to the top and insert another command before it.
	Check(d.moveLines({ target }, d.commands[0].uid, false), "reorder");
	CommandFields f;
	f.fields = { "20", "5D", "00000101", "00000000", "9", "0", "0", "0", "0" };
	Check(d.insertCommand(target, false, f) != 0, "insert before");
	const std::string json = SerializeNotes(d, notes);
	const Document reloaded = Document::parse(d.serialize());
	Notes back;
	std::string error;
	Check(ParseNotes(json, reloaded, back, &error), "notes parse: " + error);
	const int row = reloaded.findCommandRow(reloaded.commandsWithId("12").empty() ? 0 : reloaded.commands[reloaded.commandsWithId("12")[0]].uid);
	Check(row >= 0 && back.byUid.count(reloaded.commands[row].uid) && back.byUid.at(reloaded.commands[row].uid).find("positional") != std::string::npos,
		"note follows command 12 after reorder + insert");
	Check(back.byUid.count(reloaded.checks[1].uid) == 1, "ExComCheck note keyed by CheckNum");

	// Unknown keys are kept as orphans, not dropped.
	Notes orphan;
	Check(ParseNotes("{\"format\":\"hantei-cmdfile-notes\",\"version\":1,\"notes\":[{\"key\":\"command:555#0\",\"text\":\"gone\"}]}", reloaded, orphan, &error) &&
		orphan.orphans.size() == 1 && SerializeNotes(reloaded, orphan).find("gone") != std::string::npos, "orphaned notes survive a save");

	// Malformed notes are an error, never silently empty.
	Notes bad;
	Check(!ParseNotes("{ not json", reloaded, bad, &error) && !error.empty(), "malformed JSON reported");
	Check(!ParseNotes("{\"format\":\"something-else\",\"version\":1}", reloaded, bad, &error), "foreign format reported");
}

void TestSavePipeline()
{
	g_context = "save";
	const std::string dir = TempDir();
	const std::string path = dir + "\\test_0_c.txt";
	Check(WriteBytes(path, kSmall), "write fixture");

	// A malformed notes file blocks note writes but not the command-file save.
	Check(WriteBytes(path + ".notes.json", "{ broken"), "write broken notes");
	{
		Workspace ws;
		std::string error;
		Check(ws.open(path, &error), "open");
		Check(ws.notesReadOnly(), "broken notes reported");
		ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "5"); });
		const auto r = ws.save(ExtensionProfile::Vanilla, false);
		Check(r.status == SaveStatus::Saved, "save with broken notes: " + r.message);
		std::string notesNow;
		ReadFileBytes(path + ".notes.json", notesNow);
		Check(notesNow == "{ broken", "broken notes file left untouched");
	}
	DeleteFileA((path + ".notes.json").c_str());

	Workspace ws;
	std::string error;
	Check(ws.open(path, &error), "reopen");
	Check(!ws.dirty(), "clean after open");
	Check(ws.save(ExtensionProfile::Vanilla, false).status == SaveStatus::Saved, "save unmodified");
	std::string onDisk;
	ReadFileBytes(path, onDisk);
	const std::string saved = onDisk;

	// Two saves within the same second produce two distinct backups.
	ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "100"); });
	const auto r1 = ws.save(ExtensionProfile::Vanilla, false);
	ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "101"); });
	const auto r2 = ws.save(ExtensionProfile::Vanilla, false);
	Check(r1.status == SaveStatus::Saved && r2.status == SaveStatus::Saved, "two quick saves");
	Check(!r1.backupPath.empty() && !r2.backupPath.empty() && r1.backupPath != r2.backupPath, "unique backup names");
	std::string b1, b2;
	ReadFileBytes(r1.backupPath, b1);
	ReadFileBytes(r2.backupPath, b2);
	Check(b1 == saved && Document::parse(b2).commands[0].fields[CF_Pattern] == "100", "each backup holds the previous file");
	ReadFileBytes(path, onDisk);
	Check(Document::parse(onDisk).commands[0].fields[CF_Pattern] == "101", "file holds the latest save");

	// Notes are written with the file.
	ws.apply([](WorkspaceSnapshot& s) { s.notes.byUid[s.document.commands[1].uid] = "note"; return true; });
	Check(ws.dirty() && ws.save(ExtensionProfile::Vanilla, false).status == SaveStatus::Saved, "save notes");
	std::string notesText;
	Check(ReadFileBytes(path + ".notes.json", notesText) && notesText.find("command:11#0") != std::string::npos, "notes keyed by command ID");

	// External modification is detected.
	Check(WriteBytes(path, onDisk + "// external\r\n"), "external edit");
	ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Pattern, "102"); });
	Check(ws.save(ExtensionProfile::Vanilla, false).status == SaveStatus::ExternalChange, "external change blocks save");
	Check(ws.save(ExtensionProfile::Vanilla, true).status == SaveStatus::Saved, "explicit overwrite");

	// Validation errors block the write.
	ws.apply([](WorkspaceSnapshot& s) { return s.document.setCommandField(0, CF_Meter, "abc"); });
	std::string beforeBad;
	ReadFileBytes(path, beforeBad);
	Check(ws.save(ExtensionProfile::Vanilla, false).status == SaveStatus::ValidationFailed, "invalid document not saved");
	ReadFileBytes(path, onDisk);
	Check(onDisk == beforeBad, "file untouched after a refused save");

	RemoveTree(dir);
}

} // namespace

int main(int argc, char** argv)
{
	std::string fixtures = argc > 1 ? argv[1] : "tests\\fixtures\\cmdfile";
	TestHistory();
	TestRegressions();
	TestNotes();
	TestSavePipeline();
	auto files = ListFiles(fixtures);
	for (int i = 2; i < argc; ++i) files.push_back(argv[i]);
	if (files.empty()) { g_context = "fixtures"; Check(false, "no fixture files found in " + fixtures); }
	for (const auto& f : files) TestFile(f);
	std::cout << g_checks << " checks, " << g_failures << " failed, " << files.size() << " command files\n";
	return g_failures == 0 ? 0 : 1;
}
