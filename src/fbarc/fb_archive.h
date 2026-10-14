#ifndef FBARC_FB_ARCHIVE_H_GUARD
#define FBARC_FB_ARCHIVE_H_GUARD

// Generalized French-Bread archive layer: ONE interface over every container family the studio shipped between 2001 and 2016.
// Format specs: docs/formats/fb_archives.md. Each reader is validated on the shipped files by `han2tool fbrt` (byte-exact rebuild).
//
//   Kind          container                                 key         games
//   PkFileInfo    "PKFileInfo" 72-byte entries, flat        0xE3DF59AC  Act Cadenza, MBAC (C:\games\MB\AC\*.p)
//   MbFilePacA    "FilePacHeaderA" folders + 44-byte files  per-archive Melty Blood AA PC / Steam .p
//   RboPac        u32 1, 68-byte entries, plain payload     0xE3DF59AC  RBO *.PAC, GOF2 data0x.dat, MB data00.p
//   MbOldP        u32 0, 68-byte entries, name-keyed cipher 0xE3DF59AC  Melty Blood / ReAct PC .p
//   Gof1Pb        u32 flag, 64-byte entries                 0xFA261EFB  PB2K1 .dat, GOF1 .p, Rosa PAC.PAC, dMp
//
// Archives are never loaded whole (they reach 460 MB): the entry table is read, one entry on demand, rebuilds stream.
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fbarc {

enum class Kind { Unknown, PkFileInfo, MbFilePacA, RboPac, MbOldP, Gof1Pb, Uni2D, MbtlBin };
const char* KindName(Kind k);

struct Entry {
	std::string dir;        // folder inside the archive ("" for flat containers), CP932 bytes as stored
	std::string name;       // file name, CP932 bytes up to the first NUL
	uint64_t offset = 0;    // stored offset in the archive file
	uint64_t size = 0;      // plain (decrypted) size
};

// Edits applied by Archive::rebuild. Untouched entries are copied as they are, in their original order; layout, header words and
// the leftover bytes after each name's NUL are preserved so that an empty Edit rebuilds the archive byte for byte.
struct Edit {
	std::map<size_t, std::vector<uint8_t>> replace;                          // entry index -> new plain bytes
	std::set<size_t> remove;                                                 // entry indices to drop
	std::vector<std::pair<std::string, std::vector<uint8_t>>> add;           // appended (name in CP932, plain bytes)
};

class Archive {
public:
	virtual ~Archive() {}
	virtual Kind kind() const = 0;
	const std::string& path() const { return m_path; }
	const std::vector<Entry>& entries() const { return m_entries; }
	// "dir/name" with '/' separators (the form montopak and the tools print).
	std::string relativePath(size_t i) const;
	// Case-insensitive (ASCII) lookup of a relative path or bare name; -1 when absent.
	int find(const std::string& nameOrPath) const;
	// Plain bytes of entry i (cipher undone). maxBytes limits a probe.
	virtual bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes = SIZE_MAX) const = 0;
	// Streams a new archive to `outPath` (via <out>.tmp + rename). Refuses to overwrite this archive's own file.
	virtual bool rebuild(const std::string& outPath, const Edit& edit, std::string* err) const = 0;
	// One-line description of the header facts that an editor shows (version / cipher mode / key / counts).
	virtual std::string describe() const = 0;
protected:
	std::string m_path;
	std::vector<Entry> m_entries;
};

// Entry names are CP932 bytes; these convert to / from UTF-8 for file systems and UI (identity for ASCII, and off Windows).
std::string NameToUtf8(const std::string& cp932);
std::string NameFromUtf8(const std::string& utf8);

// Header sniffing (first bytes of the file; `ext` is the lower-case extension with the dot, a tiebreaker only).
Kind Detect(const uint8_t* head, size_t n, const std::string& ext = std::string());
// Modern titles without a PAC-style header (read-only, fb_modern.cpp): UNI2's <install>\d folder of obfuscated index + data files, and MBTL's data000..019.bin
// (the table is compiled into MBTL.exe: pass the exe). Open() routes a folder to the first and a file named MBTL.exe to the second.
std::unique_ptr<Archive> OpenUni2Data(const std::string& dFolderUtf8, std::string* err);
std::unique_ptr<Archive> OpenMbtlExe(const std::string& exeUtf8, std::string* err);
// Detects from the file on disk, then opens the entry table.
std::unique_ptr<Archive> Open(const std::string& utf8Path, std::string* err);
// Opens as a specific kind.
std::unique_ptr<Archive> OpenAs(Kind kind, const std::string& utf8Path, std::string* err);

// Byte-exact proof used by the suite: rebuild with an empty Edit to a temp file and compare with the original.
// Returns true when identical; `detail` receives the first difference otherwise.
bool VerifyRebuild(const Archive& a, std::string* detail);

// ---- editing one entry of an archive from a loose file ---------------------------------------------------------------------------------------------
// The browser extracts an entry to a temp file and opens it like any loose file; the origin registry remembers where that file came from so that
// Save As ... .p can write a NEW archive equal to the source with that entry replaced (the source is never touched).
struct Origin { std::string archive, entry; };   // archive path (UTF-8) and the entry's relative path as relativePath() prints it
void SetOrigin(const std::string& loosePath, const Origin& o);
bool GetOrigin(const std::string& loosePath, Origin* out);
// Writes outPath = the origin archive with `plain` (the entry's STORED bytes) replacing the origin entry.
bool SaveEntryReplacing(const Origin& o, const std::vector<uint8_t>& plain, const std::string& outPath, std::string* err);

} // namespace fbarc

#endif
