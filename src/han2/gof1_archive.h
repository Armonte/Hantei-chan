#ifndef HAN2_GOF1_ARCHIVE_H_GUARD
#define HAN2_GOF1_ARCHIVE_H_GUARD

// Glove on Fight 1 archives (gof_00.p .. gof_03.p) and character .DAT ciphers. Spec: docs/formats/gof1.md (IDA: LoadArchiveAndDecryptIndex 0x423B50,
// Archive_XOR_Decrypt_With_Filename 0x423D60, SpriteDataSlot_LoadCharacterDat 0x42B330, Crypto_XorWithKeyString 0x4238C0).
//   u32 plainFlag | u32 count ^ KEY | count x 64-byte entries {name[56] (byte j ^= (3*(j*i-28))&0xFF), u32 size ^ KEY, u32 offset} | data
//   data cipher (plainFlag == 0): first min(size, 9696) bytes ^= (i + CharUpperA(name)[i % len]) & 0xFF
#include <cstdint>
#include <string>
#include <vector>

namespace gof1 {

constexpr uint32_t kKey = 0xFA261EFBu;

struct Entry { std::string name; uint8_t rawName[56]{}; uint32_t size = 0, offset = 0; };   // rawName: the decoded 56-byte name slot, leftover bytes after the NUL included (shipped archives keep uninitialised tool memory there; kept so an unchanged archive rebuilds byte for byte)
struct Archive { std::string path; uint32_t plainFlag = 0; uint64_t fileSize = 0; std::vector<Entry> entries; };

bool LooksLikeArchive(const uint8_t *first8, size_t n);
bool Open(const std::string &utf8Path, Archive &out, std::string *err);
// Entry bytes with the archive cipher undone (a character .DAT still has its own section cipher: see DecryptDat).
bool ReadEntry(const Archive &a, size_t i, std::vector<uint8_t> &out, std::string *err = nullptr);
int  Find(const Archive &a, const std::string &name);   // case-insensitive, first match (what the game does)

// Writes a new archive: every entry copied as stored, except `replaceIndex` (>= 0) whose plain data (cipher undone) is given in `replacement`
// and re-enciphered. Refuses to write over `a.path`.
bool WriteArchiveReplacing(const Archive &a, int replaceIndex, const std::vector<uint8_t> &replacement, const std::string &outPath, std::string *err);

// In-memory archives (nested archives inside a .p entry: gof_00 PAC.PAC, gof_02 0083 / 933). Lenient: the index is parsed and every entry whose range
// fits is readable; an entry whose range does not fit (the shipped PAC.PAC has one entry of size 0xFFFFFFFF = -1, an overlap decoy) is flagged in
// `bad` and has no data. `tiles` is true when the sizes add up to the buffer modulo 2^32 (as for every shipped archive).
// The decoy has size 0xFFFFFFFF (-1): the writer that produced the index subtracted one for it, so every LATER entry's stored offset is one byte too small
// (entries after it are found at offset + 1; proven by their stage-one cipher decoding to the character magic). `dataOffset` holds the corrected offsets.
struct MemArchive { Archive a; std::vector<bool> bad; std::vector<uint64_t> dataOffset; bool tiles = false; };
void CipherEntry(std::vector<uint8_t> &d, const std::string &name);   // the per-entry stage-one cipher (symmetric XOR of the first 9696 bytes); archives with plainFlag 0
bool OpenMem(const uint8_t *b, size_t n, MemArchive &out, std::string *err);
bool ReadEntryMem(const MemArchive &m, const uint8_t *b, size_t i, std::vector<uint8_t> &out);   // false for a bad entry
// Re-encodes the index (names with rawName, sizes, offsets) exactly as stored; must equal the first 8 + 64 * count bytes of the buffer.
void EncodeIndex(const Archive &a, std::vector<uint8_t> &out);

// Rewrites EVERY entry from its plain bytes (cipher undone by ReadEntry, then re-enciphered) with the archive's own index; for an unmodified archive the
// result must equal the original file byte for byte (han2tool gof1rt proves it). Refuses to write over `a.path`.
bool RewriteAllFromPlain(const Archive &a, const std::string &outPath, std::string *err);

// Character .DAT section cipher (symmetric XOR, header / pattern area / parts / CG / names tail use three fixed keys).
void DecryptDat(std::vector<uint8_t> &d);
void EncryptDat(std::vector<uint8_t> &d);   // exact inverse; offsets taken from the header as it is when called (decrypted)

} // namespace gof1
#endif
