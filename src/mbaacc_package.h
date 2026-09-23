#ifndef MBAACC_PACKAGE_H_GUARD
#define MBAACC_PACKAGE_H_GUARD

// MBAACC character-package tools (Gonptechan EX port plan, phase H).
//
// Ported from Gonptechan EX drop 1abc27f9 (mbaacc_package_validator.*,
// ha6_consolidator.*) with these changes:
//   * every path is UTF-8 in the API and UTF-16 at the Win32 boundary (EX used
//     narrow std::filesystem::path::string() and GetPrivateProfile*A);
//   * [PAniFile] FileNum=0 (no PAT, the vanilla case) is valid; EX required
//     1..99 for every section and so failed stock descriptors;
//   * descriptor rows that are missing as loose files are looked up in the
//     game's FilePacHeaderA archives (mbaacc_pack.h) and reported as packed;
//   * backups get unique names (a counter after the timestamp), and every
//     write is temp file + MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH);
//   * a character's files can be extracted from the archives (PC or Steam
//     cipher, detected per archive).
//
// No GL / ImGui dependency; the CLI tool mbaaccpackage and the unit test link
// this directly.

#include <string>
#include <vector>

namespace mbpackage {

enum class RowStatus {
	loose,       // file beside the descriptor, header OK
	packed,      // not loose, found in a game archive, header OK
	badHeader,   // present but the magic is wrong
	missing,     // neither loose nor in an archive
};
const char* RowStatusName(RowStatus s);

struct RowReport {
	std::string section;   // "DataFile", "BmpcutFile", "PAniFile"
	std::string key;       // "File00"
	std::string file;      // value (UTF-8)
	RowStatus status = RowStatus::missing;
	std::string where;     // loose path or "0002.p: data/akiha.HA6"
};

struct ValidationResult {
	bool valid = false;
	bool newlineRepairAvailable = false;
	size_t crlf = 0, loneLf = 0, loneCr = 0;
	std::vector<RowReport> rows;
	std::vector<std::string> errors;
	std::vector<std::string> notes;
	std::string report;    // human-readable summary (UTF-8)
};

struct ValidateOptions {
	bool searchPacks = true;   // look into <game>/*.p for rows missing on disk
	std::string gameDir;       // UTF-8; default: parent of the descriptor's folder
};

// Validates the descriptor as raw bytes (line endings, sections, contiguous
// FileNN rows) and every referenced HA6 / CG / PAT (existence + magic).
ValidationResult ValidateCharacterPackage(const std::string& descriptorUtf8,
	const ValidateOptions& options = ValidateOptions());

struct ActionResult {
	bool success = false;
	std::string message;   // UTF-8
	std::string backup;    // backup file or folder, when one was made
};

// Normalises every line ending to CRLF after a unique timestamped backup,
// atomically, then re-validates. MBAACC 1.07 crashes on a lone LF (EX RE).
ActionResult RepairDescriptorNewlines(const std::string& descriptorUtf8);

// Replaces a layered [DataFile] (FileNum >= 2) by its highest layer, kept
// byte-for-byte, when that layer already holds every frameful pattern of the
// lower layers (checked with the real HA6 loader). Refuses otherwise. Backs up
// descriptor, layers and .notes sidecars into a unique folder first; restores
// the descriptor if the result fails validation.
ActionResult ConsolidateHa6Layers(const std::string& descriptorUtf8,
	const std::string& mainFileName = std::string(), bool mergeNotes = true);

// Writes every descriptor row that is only packed as a loose file beside the
// descriptor (never overwrites an existing file). Uses each archive's
// detected cipher mode.
ActionResult ExtractPackedRows(const std::string& descriptorUtf8,
	const ValidateOptions& options = ValidateOptions());

} // namespace mbpackage

#endif /* MBAACC_PACKAGE_H_GUARD */
