#ifndef HAN2_PAC_ARCHIVE_H_GUARD
#define HAN2_PAC_ARCHIVE_H_GUARD

// French-Bread PAC archive (RBO *.PAC, GOF2 data0x.dat). Format spec and the IDA
// function for each field: docs/formats/frenchbread_rbo_gof.md section 1.
//
//   u32 magic = 1
//   u32 count ^ 0xE3DF59AC
//   count * 68-byte entries: name[60] (byte j of entry i ^= (i*j*3+61)&0xFF),
//                            u32 offset, u32 size ^ 0xE3DF59AC
//   file data stored raw at the offsets
//
// The reader never loads whole archives (they reach 400+ MB): it keeps the entry
// table and reads one entry on demand.

#include <cstdint>
#include <string>
#include <vector>

namespace pac {

constexpr uint32_t kKey = 0xE3DF59ACu;
constexpr size_t   kNameLen = 60;     // bytes per name slot (last byte is always a NUL terminator slot)
constexpr size_t   kEntrySize = 68;

struct Entry {
	std::string name;                 // raw CP932 bytes up to the first NUL
	uint8_t     rawName[kNameLen]{};  // decoded name slot including bytes after the NUL (kept for exact re-encode)
	uint32_t    offset = 0;
	uint32_t    size = 0;
};

struct Archive {
	std::string        path;          // UTF-8
	uint64_t           fileSize = 0;
	std::vector<Entry> entries;
};

// True when the first 8 bytes look like a PAC header (used so GOF2 data0x.dat opens as an archive).
bool LooksLikePac(const uint8_t *first8, size_t n);
// Reads the entry table. On failure returns false and fills *err.
bool Open(const std::string &utf8Path, Archive &out, std::string *err);
// Reads entry i's bytes from disk.
bool ReadEntry(const Archive &a, size_t i, std::vector<uint8_t> &out, std::string *err = nullptr);
// Case-insensitive (ASCII) lookup; -1 when absent. When the same name occurs twice the LAST wins (what the game's loader sees is unverified; see spec).
int  Find(const Archive &a, const std::string &name);

// Writer. Entries are packed back to back after the header, in the given order.
// rawName: optional original 60-byte decoded name slot. The shipped archives keep leftover bytes after the NUL;
// passing it back makes an unchanged archive rebuild byte for byte. Empty = zero padding.
struct NewEntry { std::string name; std::vector<uint8_t> data; std::vector<uint8_t> rawName; };
// Encodes the header and data into one buffer. Fails when a name is longer than 59 bytes.
bool Build(const std::vector<NewEntry> &entries, std::vector<uint8_t> &out, std::string *err);

// Streaming writer: entries may come from loose files, from entries of an open archive, or from memory. Never overwrites a file in
// `refuseIfSame`; writes <out>.tmp and renames. Names are checked (max 59 bytes), an empty list is refused.
struct WriteSource {
	std::string name;
	std::vector<uint8_t> rawName;      // original decoded 60-byte name slot (keeps leftover bytes); empty = zero padding
	enum Kind { Memory, LooseFile, ArchiveEntry } kind = Memory;
	std::vector<uint8_t> memory;
	std::string path;                  // LooseFile (UTF-8)
	const Archive *archive = nullptr;  // ArchiveEntry
	size_t index = 0;
	uint32_t size = 0;                 // filled by WriteArchive for Loose / Archive sources
};
bool WriteArchive(const std::string &outPath, std::vector<WriteSource> &entries, const std::vector<std::string> &refuseIfSame,
                  std::string *err, void (*progress)(int done, int total, void *user) = nullptr, void *user = nullptr);

} // namespace pac

#endif
