// cmdfile_roundtrip: parse -> serialize -> byte compare for command files, plus a
// validation summary per extension profile. Used to fill the round-trip table in
// docs/HANTEI_CMDFILE_EDITOR.md.
//
//   cmdfile_roundtrip [--markdown] [--diagnostics] FILE...
//
// Exit code 0 when every file round-trips byte-identically and has no Vanilla errors.

#include "cmd_document.h"
#include "cmd_validate.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool ReadAll(const std::string& path, std::string& out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

std::string BaseName(const std::string& path)
{
	const auto pos = path.find_last_of("\\/");
	return pos == std::string::npos ? path : path.substr(pos + 1);
}

} // namespace

int main(int argc, char** argv)
{
	bool markdown = false, diagnostics = false;
	std::vector<std::string> files;
	for (int i = 1; i < argc; ++i) {
		const std::string a = argv[i];
		if (a == "--markdown") markdown = true;
		else if (a == "--diagnostics") diagnostics = true;
		else files.push_back(a);
	}
	if (files.empty()) {
		std::cerr << "usage: cmdfile_roundtrip [--markdown] [--diagnostics] FILE...\n";
		return 2;
	}
	int failures = 0;
	if (markdown) {
		std::cout << "| File | Dialect | Bytes | Lines | Commands | Commented | ExComChecks | TeamChange | Round trip | Vanilla E/W | Extended E/W |\n";
		std::cout << "|---|---|---:|---:|---:|---:|---:|---|---|---|---|\n";
	}
	for (const auto& path : files) {
		std::string bytes;
		if (!ReadAll(path, bytes)) { std::cerr << path << ": cannot read\n"; ++failures; continue; }
		const auto doc = cmdfile::Document::parse(bytes);
		const bool identical = doc.serialize() == bytes;
		// A second parse of the output must see the same structure.
		const auto again = cmdfile::Document::parse(doc.serialize());
		const bool stable = again.commands.size() == doc.commands.size() && again.checks.size() == doc.checks.size();
		const auto vanilla = cmdfile::Validate(doc, ExtensionProfile::Vanilla);
		const auto extended = cmdfile::Validate(doc, ExtensionProfile::Extended);
		std::size_t commented = 0;
		for (std::size_t i = 0; i < doc.lines().size(); ++i)
			if (doc.kind(i) == cmdfile::LineKind::CommentedCommand) ++commented;
		std::string team = "-";
		if (doc.teamChange.present) {
			team = (doc.teamChange.tagIn ? std::to_string(*doc.teamChange.tagIn) : "?") + "/" +
				(doc.teamChange.tagOut ? std::to_string(*doc.teamChange.tagOut) : "?");
		}
		const bool ok = identical && stable && !cmdfile::HasErrors(vanilla);
		if (!ok) ++failures;
		const char* dialect = doc.dialect == cmdfile::Dialect::MBAC ? "MBAC" : "MBAACC";
		if (markdown) {
			std::cout << "| " << BaseName(path) << " | " << dialect << " | " << bytes.size() << " | " << doc.lines().size()
				<< " | " << doc.commands.size() << " | " << commented << " | " << doc.checks.size() << " | " << team
				<< " | " << (identical && stable ? "identical" : "**MISMATCH**")
				<< " | " << cmdfile::CountSeverity(vanilla, cmdfile::Severity::Error) << "/" << cmdfile::CountSeverity(vanilla, cmdfile::Severity::Warning)
				<< " | " << cmdfile::CountSeverity(extended, cmdfile::Severity::Error) << "/" << cmdfile::CountSeverity(extended, cmdfile::Severity::Warning)
				<< " |\n";
		} else {
			std::cout << BaseName(path) << ": " << dialect << ", " << doc.commands.size() << " commands, "
				<< doc.checks.size() << " ExComChecks, team " << team << ", "
				<< (identical ? "byte-identical" : "MISMATCH") << (stable ? "" : ", UNSTABLE")
				<< ", vanilla " << cmdfile::CountSeverity(vanilla, cmdfile::Severity::Error) << "E/"
				<< cmdfile::CountSeverity(vanilla, cmdfile::Severity::Warning) << "W\n";
		}
		if (diagnostics) {
			for (const auto* set : { &vanilla, &extended }) {
				const char* name = set == &vanilla ? "vanilla" : "extended";
				for (const auto& d : *set) {
					if (d.severity == cmdfile::Severity::Info) continue;
					std::cout << "    [" << name << "] " << (d.severity == cmdfile::Severity::Error ? "error" : "warning")
						<< " line " << d.line << ": " << d.message << "\n";
				}
			}
		}
	}
	if (!markdown) std::cout << files.size() << " file(s), " << failures << " failure(s)\n";
	return failures ? 1 : 0;
}
