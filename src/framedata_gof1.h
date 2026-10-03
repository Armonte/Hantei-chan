#ifndef FRAMEDATA_GOF1_H_GUARD
#define FRAMEDATA_GOF1_H_GUARD

// Glove on Fight 1 character .DAT <-> Hantei-chan model. Spec: docs/formats/gof1.md. The model is fed with the section-DECRYPTED file
// (gof1::DecryptDat); Serialize returns the decrypted file, gof1::EncryptDat makes it the stored form.
// Records are kept raw (Han2FrameRaw / Han2SeqRaw) exactly like the RBO/GOF2 path: unedited -> byte-identical.
#include "framedata_han2.h"

namespace gof1 {

void RedecodeFrame(Frame &F);
bool Load(FrameData &fd, const uint8_t *decrypted, size_t size, std::string *err = nullptr);
bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err = nullptr, std::vector<std::string> *warnings = nullptr);

// .DAT target: the stored (enciphered) character file; .p target: a NEW archive equal to the source archive with this entry replaced.
bool SaveFile(const FrameData &fd, const char *filename, std::string *err = nullptr);
const std::string &LastSaveError();

} // namespace gof1
#endif
