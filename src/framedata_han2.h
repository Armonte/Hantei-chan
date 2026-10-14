#ifndef FRAMEDATA_HAN2_H_GUARD
#define FRAMEDATA_HAN2_H_GUARD

// French-Bread HAN2RBO characters (RBO .DAT/.DT2, GOF2 .DT2) <-> Hantei-chan in-memory model.
// Format spec and evidence: docs/formats/frenchbread_rbo_gof.md. Mapping rules and what is modelled: docs/formats/han2_model.md.
//
// Save strategy is the HA4 one (see framedata_ha4.cpp): original bytes are kept (Han2FrameRaw / Han2SeqRaw) and a field is
// re-encoded only when the model value differs from what the original bytes decode to.

#include <cstdint>
#include <string>
#include <vector>

#include "framedata.h"

struct Han2Container
{
	uint8_t  header[0x40]{};          // file header as loaded (area offsets/sizes are recomputed on save)
	uint32_t sub = 1;                 // 1 = RBO, 2 = GOF2, 3 = GOF1 (framedata_gof1.cpp; own container, same Han2Raw records)
	uint32_t kind = 0;                // 0 = full file, 3 = pattern area only (.DT2)
	std::vector<uint32_t> lead, tail; // pattern-area header dwords kept verbatim
	std::vector<uint8_t> sec[9];      // sections the model does not rebuild (4, 5, GOF2 8) + record 0 of the script lists (6, 7)
	std::vector<uint8_t> boxTail;     // rectangles after the last one any frame references (kept verbatim)
	std::vector<uint8_t> parts;       // PAT block (areas 1..3 only present in full files)
	std::vector<uint8_t> cg;          // CG bank
	std::vector<uint8_t> names;       // 256 x 64 name area (raw)
	uint32_t areaOff[4]{};            // header area offsets as loaded (a .DT2 keeps the .DAT's)
	std::string sourcePath;           // file the frame data came from
	// GOF2: parts and sprites live in <stem>NN.PAT / <stem>NN.CHP (NN = costume variant 00 or 01) next to the .DT2
	std::string gofVariant = "00";
	std::string gofDir;               // folder the companions were read from / are written to (empty for archive sources)
	bool partsDirty = false, cgDirty = false;
	std::vector<uint8_t> originalPatternFile;   // the file's pattern area as loaded (a .DT2-shaped copy), for 'diff against original'
	std::vector<uint8_t> extra;       // GOF1 (sub 3): the 0x44-byte file header (decrypted)
	std::string gof1Name;             // GOF1: entry name inside the .p archive (e.g. AKIKO.DAT)
	std::string datPath;              // .DAT carrying parts / CG / names when sourcePath is a .DT2 (may be empty)
	// .DT2 loaded together with its .DAT: header / areaOff above are the .DAT's (for a full save); the .DT2's own ones are kept
	// here so a .DT2 save of an unedited character is byte-identical (Update01 .DT2s point at other .DAT offsets than the shipped .DAT)
	bool hasDt2Header = false;
	uint8_t dt2Header[0x40]{};
	uint32_t dt2AreaOff[4]{};
};

namespace han2 {

constexpr int kPatterns = 256;
constexpr int kNamesSize = 0x4000;

bool IsHan2(const void *data, size_t size);

// `names`: 256 x 64 name bytes to use when `data` is a .DT2 without a name area (may be null).
bool Load(FrameData &fd, const uint8_t *data, size_t size, std::string *err = nullptr,
          const uint8_t *names = nullptr, size_t namesSize = 0);
bool LoadFile(FrameData &fd, const char *filename, std::string *err = nullptr);

// as_dt2: write only the pattern area (kind 3), the way the game prefers it over the .DAT's pattern area.
bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err = nullptr,
               std::vector<std::string> *warnings = nullptr, bool asDt2 = false);
bool SaveFile(const FrameData &fd, const char *filename, std::string *err = nullptr,
              std::vector<std::string> *warnings = nullptr, bool asDt2 = false);

// A .DT2 holds only the pattern area. When an RBO character's parts, CG or pattern names (all in the .DAT) were edited,
// a .DT2 save must also write the .DAT: `needed` says so and `out` gets the full .DAT (frames + parts + CG + names).
// Fails when those edits exist but the .DAT was never loaded (nothing to rebuild it from).
bool BuildDatCompanion(const FrameData &fd, std::vector<uint8_t> &out, bool &needed, std::string *err = nullptr);
bool NamesChanged(const FrameData &fd);   // a pattern name differs from the .DAT's name area
void RefreshNamesArea(const FrameData &fd, std::vector<uint8_t> &area);   // write the model's pattern names into a 256 x 64 name area


const std::string &LastSaveError();
const std::vector<std::string> &LastSaveWarnings();

// Follows one pattern's frame flow with the game's rules (Actor_AdvanceByAniFlag): visits = (frame, ticks); endNote says why it stopped.
void SimulateFlow(const Sequence &seq, std::vector<std::pair<int, int>> &visits, std::string &endNote);

// Re-derive the modelled fields of a frame from its raw bytes (call after editing Han2FrameRaw directly).
void RedecodeFrame(Frame &F);

// Box slot table of the RBO layout (model hitbox key per frame slot).
struct BoxSlotInfo { int key; int idxOffset; int groupOffset; const char *group; int slotInGroup; };
const BoxSlotInfo *RboBoxSlots(int &count);
const char *BoxLabel(int modelKey);

} // namespace han2

#endif /* FRAMEDATA_HAN2_H_GUARD */
