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

// Rewrites EVERY entry from its plain bytes (cipher undone by ReadEntry, then re-enciphered) with the archive's own index; for an unmodified archive the
// result must equal the original file byte for byte (han2tool gof1rt proves it). Refuses to write over `a.path`.
bool RewriteAllFromPlain(const Archive &a, const std::string &outPath, std::string *err);

// Character .DAT section cipher (symmetric XOR, header / pattern area / parts / CG / names tail use three fixed keys).
void DecryptDat(std::vector<uint8_t> &d);
void EncryptDat(std::vector<uint8_t> &d);   // exact inverse; offsets taken from the header as it is when called (decrypted)

} // namespace gof1
#endif
