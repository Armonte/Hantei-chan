#ifndef TAG_MIGRATE_H_GUARD
#define TAG_MIGRATE_H_GUARD
// [authoring] tag_tuning.ini -> sidecar tree, one-shot (docs/HANTEI_AUTHORING_MODE.md §2.7 as placed by §9.1):
//   * refuse when <gamedir>\povertycaster\tag\local\ already holds global.ini or chars\*.ini (no merging);
//   * local\global.ini = every line of the legacy file NOT inside a [char.*] section (comments, [tuning], [style.*],
//     unknown sections, commented-out "; [char.x]" examples), with one "; migrated ..." line on top;
//   * local\chars\<name>.ini = "[char]" + every line of every [char.<name>] instance (case-insensitive merge), in order;
//     a <name> that breaks the §2.1 rule stays in global.ini as a legacy [char.*] section;
//   * VERIFY before writing: the legacy text resolved with the legacy rules and the new local tree resolved with the
//     sidecar rules give identical global values, style and per-character values (every name x every moon), and the
//     same warning messages (line numbers and section labels ignored); anything else aborts with the first difference;
//   * write the character files, then global.ini; rename tag_tuning.ini -> tag_tuning.ini.migrated.
#include <map>
#include <string>
#include <vector>

namespace tagtune {

struct MigrationPlan {
	bool ok = false;                              // false: refused / failed verification (error says why)
	std::string error;
	std::string globalText;                       // local\global.ini
	std::map<std::string, std::string> chars;     // file -> local\chars\<file>.ini
	std::vector<std::string> keptLegacy;          // [char.<name>] kept in global.ini (bad name)
	bool verified = false;
	std::string firstDifference;
	int comparedSlots = 0;                        // names x moons compared
	std::vector<std::string> shippedChanges;      // levers the shipped defaults change on top (information only)
};

// Pure: split + verify one legacy text. date = the stamp in the header line.
MigrationPlan PlanMigration(const std::string& legacyText, const std::string& date);

struct MigrationResult {
	MigrationPlan plan;
	bool wrote = false;
	std::vector<std::string> files;               // absolute paths written
	std::string log;                              // human-readable report (the CLI prints it)
};
// The whole §2.7 procedure on a game folder. dryRun: plan + verify + report, write nothing.
MigrationResult MigrateGameDir(const std::string& gameDir, bool dryRun, const std::string& date);

// Warning text without "<layer> <file>: " / "[section]: " prefixes (the §2.7 comparison).
std::string NormalizeWarning(const std::string& msg);

} // namespace tagtune

#endif
