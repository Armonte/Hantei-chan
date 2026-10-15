#ifndef PCMOD_ZIP_H_GUARD
#define PCMOD_ZIP_H_GUARD
// Minimal store-only ZIP (PKWARE APPNOTE 6.3, no compression, no ZIP64) for pcmod distribution archives. Names are written
// with '/' and the UTF-8 flag (bit 11); timestamps are fixed (1980-01-01) so the same package always zips to the same bytes.
// The reader accepts stored entries only and refuses anything else by name (encrypted, deflated, ZIP64, data descriptors).
#include <cstdint>
#include <string>
#include <vector>

namespace pcmod {

struct ZipEntry {
	std::string name;            // as stored ('/' separators); a trailing '/' marks a directory entry
	std::vector<uint8_t> data;
};

uint32_t Crc32(const void* data, size_t size, uint32_t crc = 0);

// Builds the archive in memory. Entries are written in the order given. Fails (false + why) past the 4 GiB ZIP32 limit.
bool BuildZip(const std::vector<ZipEntry>& entries, std::vector<uint8_t>& out, std::string& why);
// Reads every entry; CRCs are checked.
bool ReadZip(const std::vector<uint8_t>& bytes, std::vector<ZipEntry>& out, std::string& why);

} // namespace pcmod
#endif
