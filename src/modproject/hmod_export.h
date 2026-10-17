#ifndef HMOD_EXPORT_H_GUARD
#define HMOD_EXPORT_H_GUARD
// One-click export of a mod project (AUTHORING_DESIGN.md section 6):
//   MBAACC  -> pcmod packages (docs/modmanager/pcmod-manifest_schema1.md) through src/pcmod (the same Pack the GUI exporter
//              and pcmodtool use): <out>\<id>\pcmod.json + files + <out>\<id>-<version>.zip;
//   UNI2/MBTL -> the UNI2-IM / MBTL-IM mod-folder layout (tools/gamecap/re/IMPROVEMENT_MODS.md): <root>\<IM>\Packs\<pack>\<game
//              path> with a mod.ini [Mod] block (switchable in the IM overlay), or <root>\<IM>\Mods\<game path> (always on).
// Only files tracked as changed / added ship. Before anything is written the package is checked against the other installed
// / known packages (hmod_conflict.h); any unresolved conflict refuses the export and nothing is written. A file already
// at the destination that this project did not write is never replaced.
#include "hmod_conflict.h"
#include "hmod_project.h"
#include "hmod_track.h"

#include <string>
#include <vector>

namespace hmod {

struct ExportOptions {
	std::string package;               // "" = every package of the project
	std::string outDir;                // pcmod: output folder ("" = <project>\build\pkg)
	std::string gameDir;               // the game install: installed-mod scan, exe hash (mbaacc); IM root for uni2 / mbtl
	std::vector<std::string> knownDirs;   // extra package folders to check against
	bool writeZip = true;
	bool dryRun = false;               // run every check, write nothing
	bool imUsePacks = true;            // IM: Packs\<package> (switchable) instead of Mods (always on)
	bool saveProject = true;           // record the outputs in hmod.json
};

struct ExportReport {
	bool ok = false;
	std::vector<std::string> errors, warnings, notes;
	std::vector<Conflict> conflicts;
	std::vector<Output> outputs;
};

// Dispatches on project.game.
ExportReport Export(Project& p, const ExportOptions& o, const VanillaIndex* vanilla);
ExportReport ExportPcmod(Project& p, const ExportOptions& o, const VanillaIndex* vanilla);
ExportReport ExportIm(Project& p, const ExportOptions& o, const VanillaIndex* vanilla);

// IM folder names ("UNI2-IM" / "MBTL-IM"); "" for a game that has none.
std::string ImFolderName(const std::string& game);
std::string ReportText(const ExportReport& r);

} // namespace hmod
#endif
