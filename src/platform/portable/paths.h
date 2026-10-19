#ifndef PORTABLE_PATHS_H_GUARD
#define PORTABLE_PATHS_H_GUARD

// Portable paths, encodings, file replacement and the ini the app keeps next to
// itself.
//
// Lane LINUX deliverable 2: the same signatures on Windows and POSIX, with the
// Windows body copied from the code it replaces, so P5 switches the callers
// over without a behaviour change (PLAN.md P5 step 4, docs/render/linux.md).
//
// Nothing in src/ calls these yet. The callers are still on the Win32 calls and
// docs/render/linux.md section 7 lists each switch-over as a P5 TODO.

#include <cstdint>
#include <string>
#include <vector>

namespace portable {

// ---- encoding ---------------------------------------------------------------------------------------------------
// Replaces AnsiToUtf8 / Utf8ToWide / WideToUtf8 and the raw
// MultiByteToWideChar / WideCharToMultiByte pairs.

// Bytes in code page `cp` -> UTF-8. cp == 0 means "the ANSI code page" on
// Windows (CP_ACP, 932 on a Japanese machine) and UTF-8 on POSIX, which is what
// the editor's ANSI-path helpers mean on each platform.
std::string CpToUtf8(const std::string& bytes, unsigned cp = 0);
std::string Utf8ToCp(const std::string& utf8, unsigned cp = 0);

// The two the editor uses most, under the names png_writer.h already exports.
std::string AnsiToUtf8(const std::string& bytes);
std::string Utf8ToAnsi(const std::string& utf8);

// UTF-8 <-> wchar_t. On Windows this is the UTF-16 MultiByteToWideChar pair; on
// POSIX wchar_t is UTF-32 and the conversion is done here, so the signature is
// the same and the call sites do not care.
std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

// ---- paths ------------------------------------------------------------------------------------------------------

// Absolute path of the running executable, UTF-8: "C:\...\gonptechan.exe" or
// "/home/.../gonptechan".
std::string ExecutablePath();
// ExecutablePath() without the last component and without a trailing separator.
std::string ExecutableDir();

// Per-user writable folder for settings, backups and caches:
// %LOCALAPPDATA%\hantei-chan on Windows, $XDG_DATA_HOME/hantei-chan
// (~/.local/share/hantei-chan) on POSIX. Created on demand; "" when neither
// variable is set.
std::string UserDataDir();

// The process working directory in "wide" form, UTF-8.
std::string CurrentDir();

// Separators to '/', drive letters lower-cased, no trailing separator. This is
// misc.cpp's normalizePath() lifted here so the ini, the Steam scan and the
// game key share one implementation.
std::string NormalizeSeparators(std::string path);

// ---- files ------------------------------------------------------------------------------------------------------

bool FileExists(const std::string& utf8Path);
// 0 when the file does not exist or is a directory.
uint64_t FileSize(const std::string& utf8Path);
// create_directories() with the same error string the editor showed.
bool CreateDirectories(const std::string& utf8Dir, std::string* error = nullptr);
// Atomically replace `utf8Path` with `size` bytes from `data`: a uniquely named
// temp file in the same folder, flushed to disk, then renamed over the target
// with the same antivirus retry misc.cpp's WriteFileAtomic used. The original
// is untouched on failure and the temp file is removed.
bool ReplaceFileAtomic(const std::string& utf8Path, const void* data, size_t size);
bool ReadWholeFile(const std::string& utf8Path, std::vector<uint8_t>& out);

// ---- ini --------------------------------------------------------------------------------------------------------
// GetPrivateProfile* / WritePrivateProfile* on Windows; on POSIX the same text
// format ("[section]", "key=value", ';' and '#' comments, the value trimmed of
// trailing space) is parsed here, so an ini written on one platform reads on
// the other. Writes keep the surrounding file intact and add a missing key.
std::string IniGetString(const std::string& iniPath, const std::string& section, const std::string& key,
                         const std::string& def = std::string());
int IniGetInt(const std::string& iniPath, const std::string& section, const std::string& key, int def = 0);
bool IniSetString(const std::string& iniPath, const std::string& section, const std::string& key, const std::string& value);

}  // namespace portable

#endif /* PORTABLE_PATHS_H_GUARD */
