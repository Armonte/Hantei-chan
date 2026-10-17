#ifndef CMDFILE_CMD_IO_H_GUARD
#define CMDFILE_CMD_IO_H_GUARD

// Windows-side helpers for the command-file editor: CP932 conversion, file reads,
// collision-free backups. Atomic replacement reuses WriteFileAtomic from misc.cpp.

#include <optional>
#include <string>
#include <vector>

namespace cmdfile {

// Display conversion. Bytes that are not valid CP932 become U+FFFD instead of vanishing.
std::string Cp932ToUtf8(const std::string& cp932);
// Strict: nullopt when any character has no CP932 form (no silent '?' substitution).
std::optional<std::string> Utf8ToCp932(const std::string& utf8);

bool ReadFileBytes(const std::string& path, std::string& out, std::string* error = nullptr);
bool FileExists(const std::string& path);

// Copies `bytes` into <dir>/.hantei-backups/<name>.<YYYYmmdd-HHMMSS>.bak. The file is
// created with CREATE_NEW, and a numeric suffix is added until the name is free, so two
// saves in the same second never collide or overwrite an older backup.
bool WriteUniqueBackup(const std::string& targetPath, const std::string& bytes, std::string* backupPath, std::string* error);

// Candidate command files for a character load list (hisui_0.txt -> hisui_0_c.txt,
// hisui.txt -> hisui_0_c.txt ..., ARC.TXT -> ARC_C.TXT). Only existing files are returned.
std::vector<std::string> CommandFileCandidates(const std::string& characterTxtPath);

} // namespace cmdfile

#endif
