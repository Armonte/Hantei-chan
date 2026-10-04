#ifndef FRAMEDATA_PB2K1_H_GUARD
#define FRAMEDATA_PB2K1_H_GUARD

// Party Breakers (2001, pb2k1.exe) character .DAT <-> Hantei-chan model. Spec: docs/formats/pb2k1.md, IDA types docs/formats/ida/pb2k1_types.h.
// The file on disk (after the archive layer) is enciphered in three sections (Decrypt/Encrypt); the model is fed the DECRYPTED file.
// Records are kept raw (Han2FrameRaw / Han2SeqRaw) like the GOF1 path: an unedited character saves byte-identical.
#include "framedata_han2.h"

namespace pb2k1 {

// True when `stored` is a Party Breakers character (header decrypts to the check value 15 and the section sizes add up to the file size).
bool LooksLikeCharacter(const uint8_t *stored, size_t size);
bool Decrypt(std::vector<uint8_t> &file);   // stored -> plain; false when the header does not check out
void Encrypt(std::vector<uint8_t> &file);   // plain -> stored (exact inverse; the header must be decrypted and consistent)

void RedecodeFrame(Frame &F);
bool Load(FrameData &fd, const uint8_t *decrypted, size_t size, std::string *err = nullptr);
bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err = nullptr, std::vector<std::string> *warnings = nullptr);

// Loose enciphered .DAT, or (target `.p`, or an archive-looking name such as 01.dat / 01p.dat) a NEW archive with this entry replaced.
bool SaveFile(const FrameData &fd, const char *filename, std::string *err = nullptr);
const std::string &LastSaveError();

} // namespace pb2k1
#endif
