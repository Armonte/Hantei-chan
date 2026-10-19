#ifndef PCMOD_PACKAGE_H_GUARD
#define PCMOD_PACKAGE_H_GUARD
// pcmod package files on disk (docs/modmanager/pcmod-manifest_schema1.md §1, §8): pack a set of game-relative files into
// <out>\<id>\ + <out>\<id>-<version>.zip, verify a package folder or archive, read the local mod.ini. All paths are UTF-8.
// No GL / ImGui: pcmodtool, the GUI exporter and pcmod_test share it.
#include "pcmod_manifest.h"
#include "pcmod_schema2.h"

#include <string>
#include <vector>

namespace pcmod {

struct SourceFile {
	std::string rel;       // game-relative path ("data\SION_3.txt"), either separator
	std::string absPath;   // where the bytes are now (UTF-8)
};

// Every regular file under `dir`, relative to it, except a root-level pcmod.json / mod.ini. Sorted by normalised path.
bool ScanSourceDir(const std::string& dir, std::vector<SourceFile>& out, std::string& why);
// Reads, hashes and sizes one file into `e` (path left as is).
bool HashFile(const std::string& absPath, FileEntry& e, std::string& why);
bool ReadAll(const std::string& path, std::vector<uint8_t>& out);
std::string Sha256OfFile(const std::string& path);   // "" when unreadable

struct PackResult {
	bool ok = false;
	std::string error;
	Manifest manifest;            // as written
	std::string packageDir;       // <out>\<id>
	std::string zipPath;          // <out>\<id>-<version>.zip ("" when not requested)
	std::string digestHex;        // §5 package digest
	std::string simDigestHex;     // §5 package sim digest (sim-class files only; what the session digest folds)
	std::vector<std::string> warnings;
};
// `meta` supplies every field except `files`, which is built from `sources` (hashing each). A previous export of the same
// id in `outDir` (a folder holding a pcmod.json) is replaced; any other non-empty folder there is refused.
// Schema 2 (PC S2.1): when meta.HasAux() (pcmod_schema2.h AttachToManifest set schema 2 + the refs), `aux` holds the TOML
// texts; they are written next to pcmod.json (and into the zip under <id>/), never into files[].
PackResult Pack(const std::vector<SourceFile>& sources, const Manifest& meta, const std::string& outDir, bool writeZip,
                const s2::Files* aux = nullptr);

struct VerifyResult {
	bool ok = false;
	std::vector<std::string> errors;     // any error = BROKEN package (§4)
	std::vector<std::string> warnings;   // e.g. unlisted files (never served)
	Manifest manifest;
	std::string digestHex;      // package digest
	std::string simDigestHex;   // package sim digest
	s2::Data s2;                // schema 2: the parsed patches/features/rules (MbaaccProfile)
};
// A package folder: pcmod.json parses, the folder name equals the id (unless !requireFolderName), every listed file exists
// with the listed size and hash, and unlisted files are reported.
VerifyResult VerifyDirectory(const std::string& dir, bool requireFolderName = true);
// A distribution archive: a single top-level <id>/ directory, <id>/pcmod.json, every listed file present and matching, and
// no entry outside the listed paths (§3: such entries are refused, not extracted). <id>/mod.ini is allowed.
VerifyResult VerifyZip(const std::string& zipPath);
VerifyResult VerifyZipBytes(const std::vector<uint8_t>& bytes);
// Directory or .zip, by what is on disk.
VerifyResult VerifyPath(const std::string& path);

// Local state from <pkg>\mod.ini [Config] enabled= priority= (absent keys keep the given defaults). False when no file.
bool ReadModIni(const std::string& packageDir, bool& enabled, int& priority);

// The session sim digest (§5) over installed package folders, with the §4 checks that make a package BROKEN: verify
// failures, an exe not in its list, unmet `requires` (exact version), enabled `conflicts`, and a path shipped by two
// packages where a sim package is involved and the winner does not list the loser in `overrides`. mod.ini enabled=0
// leaves a package out; mod.ini priority overrides the manifest's. Cosmetic packages never enter the digest.
// `mode` (§6 per-mode set): only live sim packages whose `modes` is empty or lists it enter the digest; the others are
// still checked (a broken one still blocks) and reported in `notes`. "" = no mode filter (the plain session sim digest).
struct SessionResult {
	bool ok = false;                      // false = the digest cannot be formed (an enabled sim package is broken)
	std::string digestHex;                // only when ok
	std::vector<std::string> errors, notes;
	std::vector<SessionEntry> simOrder;   // resolved order (priority desc, then id), only those in the mode's set
	std::string mode;                     // the mode the set was built for ("" = every package)
};
SessionResult ResolveSession(const std::string& exeSha256Hex, const std::vector<std::string>& packageDirs,
                             const std::string& mode = std::string());

} // namespace pcmod
#endif
