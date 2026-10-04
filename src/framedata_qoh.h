#ifndef FRAMEDATA_QOH_H_GUARD
#define FRAMEDATA_QOH_H_GUARD

// Queen of Heart '98 (.dat) and '99 (.chr) characters <-> Hantei-chan model. Specs: docs/formats/qoh98.md, docs/formats/qoh99.md.
// The model is fed the PLAINTEXT file ('99: after qoh::Decrypt99). Records are kept raw (Han2FrameRaw): an unedited character saves byte-identical, an edit patches
// only the fields it changed, in place. The tables are allotted per action (counts and bases live in the action records), so adding or removing frames or boxes is
// refused with an explanation instead of silently corrupting a neighbour.
#include "framedata_han2.h"

namespace qoh {

void RedecodeFrame(Frame &F, int version);
bool Load(FrameData &fd, const uint8_t *plain, size_t size, int version, std::string *err = nullptr);
bool Serialize(const FrameData &fd, std::vector<uint8_t> &plain, std::string *err = nullptr);   // plaintext file
// '98: writes the loose .dat; '99: enciphers with the stem of `filename` and writes it (the game requires the stem to match the path).
bool SaveFile(const FrameData &fd, const char *filename, std::string *err = nullptr);
const std::string &LastSaveError();

} // namespace qoh
#endif
