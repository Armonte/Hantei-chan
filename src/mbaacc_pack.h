#ifndef MBAACC_PACK_H_GUARD
#define MBAACC_PACK_H_GUARD

// Read-only access to MBAACC "FilePacHeaderA" .p archives (0000.p ... 0008.p
// next to MBAA.exe). Format and cipher from castergroup's montopak.py, which
// was checked against the PC 1.07 and Steam executables:
//
//   header (52 bytes): "FilePacHeaderA" magic in [0,20), xor key @20,
//     data offset @24, folder count @32, file count @36
//   folder record (268): first file @4, name-increment @8 (0 = empty
//     sentinel), 256-byte encrypted name @12 (".\data", ".\grp\...")
//   file record (44): offset @0 (from the data offset), owner folder @4,
//     size @8, 32-byte encrypted name @12 (increment = size & 0xFF)
//   names: XOR with the 4 key bytes, each key byte += increment after use
//   payload: the first 4 KiB is XOR'd (increment 3, fresh key). The Steam
//     build also XORs the LAST 4 KiB of files larger than 8 KiB; the PC build
//     does not. Headers (first bytes) therefore read the same in both modes,
//     but a full extraction must use the right mode. detectMode() uses
//     montopak's heuristic: decrypt the largest file's tail both ways and keep
//     the lower-entropy result.
//
// Paths are UTF-16 on the Win32 side; names inside the archive are
// Shift-JIS and are returned as UTF-8.

#include <cstdint>
#include <string>
#include <vector>

namespace mbpack {

enum class CryptMode { pc, steam };
const char* CryptModeName(CryptMode mode);

struct Entry {
	std::string folder;   // UTF-8, as stored (".\\data")
	std::string name;     // UTF-8 file name
	uint32_t offset = 0;  // relative to the data offset
	uint32_t size = 0;
	// "data/akiha.HA6" style path (folder without the leading ".\", '/').
	std::string relativePath() const;
};

class Archive {
public:
	bool open(const std::wstring& path, std::string& error);
	const std::wstring& path() const { return m_path; }
	const std::vector<Entry>& entries() const { return m_entries; }
	// Case-insensitive lookup of "data/akiha.HA6" (either slash).
	const Entry* find(const std::string& relativePath) const;
	// Decrypted bytes; maxBytes limits the read (a header probe).
	bool read(const Entry& entry, std::vector<uint8_t>& out, CryptMode mode,
		size_t maxBytes = SIZE_MAX) const;
	CryptMode detectMode() const;

private:
	std::wstring m_path;
	uint32_t m_key = 0;
	uint32_t m_dataOffset = 0;
	std::vector<Entry> m_entries;
};

// *.p files directly in `gameDir`, sorted by name.
std::vector<std::wstring> FindPacks(const std::wstring& gameDir);

// Symmetric payload cipher (exposed for tests).
void XorPayload(uint8_t* data, size_t size, uint32_t key, CryptMode mode);

} // namespace mbpack

#endif /* MBAACC_PACK_H_GUARD */
