#ifndef CGM_PROJECT_H_GUARD
#define CGM_PROJECT_H_GUARD
// Who uses a sprite bank on disk: every project .txt of the bank's folder whose [BmpcutFile] File00 names the bank, and the
// distinct HA6 files of their [DataFile] lists. A structural edit (insert / delete / move) renumbers images, so the references
// of EVERY one of those files must follow, not only the stack that is loaded (MBAACC: akiha.txt, akiha_0/1/2.txt with their _r
// files and b_akiha_8.txt all draw akiha.cg; MBTL: chrNNN.txt and the variants chrNNN_6..9.txt share chrNNN.cg).
//
// A file is "shared" when it also belongs to a project of ANOTHER bank, or lives outside the bank's folder (../BaseData.HA6 of
// UNI2 / MBTL, used by every character with its own bank). Its sprite ids mean different pictures per bank, so it is never
// rewritten on disk: the loaded character bakes the remapped patterns into its own file instead (the template overrides).
// No GL / ImGui (cgmtool and the tests link it).
#include <cstdint>
#include <string>
#include <vector>

class FrameData;

namespace cgm {

struct ProjectTxt {
	std::string path;                 // UTF-8
	std::vector<std::string> dataFiles;   // [DataFile] entries as written (CP932 decoded to UTF-8)
	std::string bank;                 // [BmpcutFile] File00 ("" = none)
	std::vector<std::string> dataPaths;   // resolved UTF-8 paths of dataFiles
	std::string bankPath;             // resolved UTF-8 path of bank
};
// Parses a project .txt ([DataFile] FileNum N + File%02d / File%03d, or a single File=; [BmpcutFile] File00). False when the
// file is not a project (no [DataFile] list), e.g. MBTL command scripts or _c.txt command tables.
bool ParseProjectTxt(const std::string &utf8Path, ProjectTxt &out);

struct BankUserFile {
	std::string path;                 // UTF-8, as resolved from the first project naming it
	bool shared = false;              // also used with another bank / outside the bank folder: never rewritten on disk
	bool unlisted = false;            // in no project .txt, but named after the bank (<bank>_*.ha6, e.g. UNIB Hyd_s.HA6 loaded by game code)
	std::vector<int> projects;        // indices into BankUsers::projects
};
struct BankUsers {
	std::string bankPath;
	std::vector<ProjectTxt> projects;     // projects drawing this bank
	std::vector<BankUserFile> files;      // distinct HA6 files of those projects
	int fileIndex(const std::string &utf8Path) const;   // -1 = not a user
	int ownFileCount() const { int n = 0; for (auto &f : files) n += !f.shared; return n; }
};
// Scans the .txt files of the bank's folder. Path comparison is case-insensitive and lexically normalised (Windows semantics).
BankUsers FindBankUsers(const std::string &utf8BankPath);
std::string NormPath(const std::string &utf8Path);   // lower case, '/' separators, "." and ".." resolved

// Usage of each image across every user file (each HA6 loaded on its own; a pattern a later moon file overrides still counts,
// since another project loads it). Layers drawing PAT parts (usePat) are not CG references.
struct ProjectUsage {
	std::vector<int> total;                          // per image id
	std::vector<std::vector<int>> perFile;           // [file][image] layer count
	std::vector<std::string> errors;                 // files that could not be read
	int count(int image) const { return image >= 0 && image < (int)total.size() ? total[image] : 0; }
};
ProjectUsage CountProjectUsage(const BankUsers &users, int imageCount);

// Rewrites the CG references of every non-shared user file through `remap` (old id -> new id, -1 = image gone: the reference
// becomes -1). Files whose references do not change are not touched. With `backup` the first write of a file keeps the original
// as <file>.bak. `skip`: normalised paths not to write (the loaded stack's files when the caller writes them itself).
struct DiskRemapResult {
	std::vector<std::string> written, unchanged, sharedSkipped, errors;
	int refsChanged = 0, refsCleared = 0;
};
bool RemapUserFiles(const BankUsers &users, const std::vector<int> &remap, bool backup, DiskRemapResult &res, const std::vector<std::string> &skip = {});

// One file: load, remap, save if changed (atomic). Returns false on a read/write error.
bool RemapHa6File(const std::string &utf8Path, const std::vector<int> &remap, bool backup, int *changed, int *cleared, std::string *err);

bool IsIdentityRemap(const std::vector<int> &remap);

} // namespace cgm
#endif
