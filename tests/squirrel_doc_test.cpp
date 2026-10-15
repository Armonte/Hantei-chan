// squirrel_doc_test: CP932 script document round trip (SCOPE.md §2.5, phase P2) and the sandboxed
// in-editor VM (phase P3).
//
//   squirrel_doc_test                       unit tests (synthetic bytes, fixtures)
//   squirrel_doc_test --games <dir>...      every .txt under <dir>/script and <dir>/data must
//                                           round-trip byte-identical (load -> editor lines -> save),
//                                           also after a typed-and-deleted edit. Exit 77 when no
//                                           <dir> exists.
// Output goes through quill.

#include "squirrel/script_document.h"
#include "squirrel/script_vm.h"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using sqedit::ScriptDocument;

static quill::Logger* g_log = nullptr;
static int g_fail = 0;

#define CHECK(cond)                                                                \
	do {                                                                           \
		if (!(cond)) {                                                             \
			QUILL_LOG_ERROR(g_log, "CHECK failed line {}: {}", __LINE__, #cond);   \
			++g_fail;                                                              \
		}                                                                          \
	} while (0)

static bool ReadFile(const fs::path& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static std::string Save(const ScriptDocument& d, const std::vector<std::string>& lines, bool* ok = nullptr)
{
	std::string out;
	std::vector<sqedit::EncodeError> errs;
	const bool r = d.encode(lines, out, &errs);
	if (ok) *ok = r;
	return out;
}

static void UnitTests()
{
	// Mixed terminators, CP932 with a 0x5C trail byte (表 = 95 5C), an NEC-selected IBM extension
	// (0xED40, which Windows maps back to 0xFA5C), an invalid lead byte (0x80), no final newline.
	const std::string raw = std::string("// \x95\x5C\r\n") + "local a = 1;\n" + "x <- \"\xED\x40\";\r\n" + "\x80\x81\r" + "last";
	ScriptDocument d;
	d.loadBytes(raw);
	CHECK(d.lineCount() == 5);
	CHECK(Save(d, d.utf8Lines()) == raw);
	CHECK(Save(d, sqedit::SplitLines(d.utf8Text())) == raw);

	// Edit one line: the others keep their bytes and terminators.
	auto lines = d.utf8Lines();
	lines[1] = "local a = 2;";
	std::string edited = Save(d, lines);
	CHECK(edited == std::string("// \x95\x5C\r\n") + "local a = 2;\n" + "x <- \"\xED\x40\";\r\n" + "\x80\x81\r" + "last");
	// Type then delete -> byte-identical.
	lines[1] = "local a = 1;";
	CHECK(Save(d, lines) == raw);

	// Insert a line (new lines take the dominant terminator, CRLF here) and append after the
	// unterminated last line.
	lines = d.utf8Lines();
	lines.insert(lines.begin() + 1, "// \xE8\xA1\xA8"); // 表 in UTF-8
	lines.push_back("tail");
	CHECK(Save(d, lines) == std::string("// \x95\x5C\r\n// \x95\x5C\r\n") + "local a = 1;\n" + "x <- \"\xED\x40\";\r\n" + "\x80\x81\r" + "last\r\n" + "tail");

	// Moved line keeps its original bytes (the lossy 0xED40 line).
	lines = d.utf8Lines();
	std::swap(lines[1], lines[2]);
	const std::string moved = Save(d, lines);
	CHECK(moved.find("\xED\x40") != std::string::npos);

	// A character CP932 cannot represent is refused, with its line.
	lines = d.utf8Lines();
	lines[3] = "\xF0\x9F\x98\x80"; // U+1F600
	std::string out;
	std::vector<sqedit::EncodeError> errs;
	CHECK(!d.encode(lines, out, &errs));
	CHECK(errs.size() == 1 && errs[0].line == 3);

	// Empty file and a file that is only a newline.
	d.loadBytes("");
	CHECK(Save(d, d.utf8Lines()).empty());
	d.loadBytes("\r\n");
	CHECK(d.lineCount() == 2 && Save(d, d.utf8Lines()) == "\r\n");

	// VM: fixtures compile; syntax error reports a line; sandbox blocks io/system.
	sqedit::ScriptVM vm;
	std::vector<std::string> logs;
	std::string err;
	CHECK(vm.open("tests/fixtures/squirrel", "", [&](const std::string& s) { logs.push_back(s); }, &err));
	std::string src;
	CHECK(ReadFile("tests/fixtures/squirrel/fb_style_move.txt", src));
	CHECK(vm.compile(src, "fb_style_move.txt").ok);
	CHECK(ReadFile("tests/fixtures/squirrel/syntax_error.txt", src));
	auto diag = vm.compile(src, "syntax_error.txt");
	CHECK(!diag.ok && !diag.runtime && diag.line > 0);
	diag = vm.compileAndRun("system(\"calc.exe\");\nlocal f = file(\"x.txt\", \"wb\");\nremove(\"y\");\nlocal t = clock();\n", "sandbox.nut");
	CHECK(diag.ok);
	CHECK(vm.sandboxBlockedCount() == 3);
	bool sawSystem = false;
	for (auto& l : logs) sawSystem |= l.find("blocked system(\"calc.exe\")") != std::string::npos;
	CHECK(sawSystem);
	diag = vm.compileAndRun("local a = 1;\nlocal b = NoSuchTable.Foo();\n", "rt.nut");
	CHECK(!diag.ok && diag.runtime && diag.line == 2);
	// __dofile__ outside the game root is refused.
	diag = vm.compileAndRun("__dofile__(\"../../CMakeLists.txt\");\n", "escape.nut");
	CHECK(!diag.ok);
}

static int GameTests(const std::vector<std::string>& dirs)
{
	int files = 0, present = 0;
	std::size_t bytes = 0;
	for (const auto& dir : dirs) {
		const fs::path root(dir);
		if (!fs::exists(root)) {
			QUILL_LOG_WARNING(g_log, "skip {} (not found)", dir);
			continue;
		}
		++present;
		int here = 0;
		for (const char* sub : {"script", "data"}) {
			if (!fs::exists(root / sub)) continue;
			for (auto& it : fs::recursive_directory_iterator(root / sub)) {
				if (!it.is_regular_file() || it.path().extension() != ".txt") continue;
				std::string raw;
				if (!ReadFile(it.path(), raw)) { ++g_fail; continue; }
				ScriptDocument d;
				d.loadBytes(raw);
				bool ok = false;
				if (Save(d, sqedit::SplitLines(d.utf8Text()), &ok) != raw || !ok) {
					QUILL_LOG_ERROR(g_log, "round trip differs: {}", it.path().generic_string());
					++g_fail;
				}
				auto lines = d.utf8Lines();
				const std::size_t mid = lines.size() / 2;
				lines[mid] += "x";
				Save(d, lines);
				lines[mid].pop_back();
				if (Save(d, lines) != raw) {
					QUILL_LOG_ERROR(g_log, "edit+undo differs: {}", it.path().generic_string());
					++g_fail;
				}
				++here;
				bytes += raw.size();
			}
		}
		QUILL_LOG_INFO(g_log, "{}: {} .txt files round-tripped", dir, here);
		files += here;
	}
	if (!present) return 77;
	QUILL_LOG_INFO(g_log, "round trip: {} files, {} bytes", files, bytes);
	return files > 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
	quill::Backend::start();
	auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sqdoc_console");
	g_log = quill::Frontend::create_or_get_logger("sqdoc", std::move(sink), quill::PatternFormatterOptions{"%(message)"});

	int rc = 0;
	std::vector<std::string> args(argv + 1, argv + argc);
	if (!args.empty() && args[0] == "--games") {
		rc = GameTests(std::vector<std::string>(args.begin() + 1, args.end()));
	} else {
		UnitTests();
		QUILL_LOG_INFO(g_log, "unit tests: {} failures", g_fail);
	}
	if (g_fail) rc = 1;
	g_log->flush_log();
	quill::Backend::stop();
	return rc;
}
