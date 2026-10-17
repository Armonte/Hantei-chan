#ifndef LIVE_RELOAD_H_GUARD
#define LIVE_RELOAD_H_GUARD

// Editor side of the asset hot-reload seam (docs/formats/dmp_live_reload.md section 3): write the edited asset where the running game looks first (loose file under the
// game directory), append `reload <path>` to <gameDir>\pchost_reload.req (temp file + rename), and wait for the answer line in <gameDir>\pchost_reload.ack.
#include <cstdint>
#include <string>
#include <vector>

namespace livereload {

struct Result { bool ok = false; std::string status; };

// `relPath`: forward slashes, relative to the game directory, no "..": it is validated here as well as by pchost.
bool ValidRelPath(const std::string &relPath, std::string *why = nullptr);
// Pure helpers (unit tested): the request line and the parse of an ack line.
std::string RequestLine(const std::string &relPath);
bool ParseAck(const std::string &line, const std::string &relPath, Result &out);

// Writes the asset (new file, never in place: the destination may be a hard link of the install), sends the request, waits up to timeoutMs for the ack.
Result Reload(const std::string &gameDir, const std::string &relPath, const std::vector<uint8_t> &bytes, int timeoutMs = 3000);

// Where the game asks for a PAC entry (loose path): scans the script strings of the given archive's .FOB entries for ".\\Data\\...\\<basename>" and returns the
// relative path (forward slashes) or an empty string. `fobs` are the plain bytes of every FOB of the archive.
std::string GuessLoosePath(const std::string &basename, const std::vector<std::vector<uint8_t>> &fobs);

} // namespace livereload
#endif
