#ifndef CMDFILE_CMD_VALIDATE_H_GUARD
#define CMDFILE_CMD_VALIDATE_H_GUARD

#include "cmd_document.h"
#include "../extension_profile.h"

#include <string>
#include <vector>

namespace cmdfile {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
	Severity severity = Severity::Error;
	int line = 0;           // 1-based physical line, 0 = whole document
	std::uint64_t uid = 0;  // line uid when the diagnostic belongs to a line
	std::string message;
};

// Errors block saving. Everything the shipped MBAACC and MBAC files contain passes the
// Vanilla profile without errors (checked by cmdfile_roundtrip over every file).
std::vector<Diagnostic> Validate(const Document& doc, ExtensionProfile profile);
bool HasErrors(const std::vector<Diagnostic>& diagnostics);
std::size_t CountSeverity(const std::vector<Diagnostic>& diagnostics, Severity severity);

// ---- ExComCheck type registry ----
struct ExComTypeInfo {
	int type;
	const char* name;
	bool extendedOnly;
	const char* summary;
	const char* paramNames[5];
	const char* paramHelp[5];
};

// Types offered for the profile (vanilla: 0 and 1; extended adds 2 and 3).
std::vector<const ExComTypeInfo*> ExComTypes(ExtensionProfile profile);
const ExComTypeInfo* FindExComType(int type); // any profile
std::string SummarizeCheck(const ExComRecord& check);

} // namespace cmdfile

#endif
