#ifndef HAN2_PAC_WINDOW_H_GUARD
#define HAN2_PAC_WINDOW_H_GUARD

// PAC creation window (icaroffa's CreatePACForm behaviour, reimplemented: base PAC is copied to a temp file and read from the copy,
// a same-named file replaces the entry instead of being ignored, picking a folder is a batch override, the root import takes every
// base entry, the base PAC is never overwritten, an empty list and over-long names are refused) and the file viewer windows for
// .IMG / .FOB / other archive entries.
#include <cstdint>
#include <string>
#include <vector>

namespace han2ui {

extern bool showPacCreate;
void DrawPacCreate();
// Queue a replacement in the creator (used by the viewers' "put into PAC list").
void PacCreateAddMemory(const std::string &cp932Name, std::vector<uint8_t> data);

// `archivePath` (optional): the archive the entry came from, used to guess the loose path the game opens it from (live reload).
void OpenFileViewer(const std::string &cp932Name, std::vector<uint8_t> bytes, const std::string &origin, const std::string &archivePath = std::string());
void DrawFileViewers();

} // namespace han2ui

#endif
