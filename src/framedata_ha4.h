#ifndef FRAMEDATA_HA4_H_GUARD
#define FRAMEDATA_HA4_H_GUARD

// MBAC (Melty Blood Act Cadenza PC) Hantei4 ".DAT" character container.
//
// Layout (hantei4.exe WriteHantei4DatFile 0x40CF60 / UnpackHantei4DatToPatterns
// 0x40EC60, mbacPC.exe SpriteDataSlot_Load 0x43DB20), verified on all 50 MBAC
// .DAT files; full table in docs/bg_research/HA4_SECTION.md:
//   0x00 "Hantei4\0", 0x10 version=1, 0x14 partsOff, 0x18 partsSize,
//   0x1C cgOff, 0x20 cgSize, 0x44 int32[256] absolute pattern offsets (-1 empty),
//   0x444.. patterns packed in slot order,
//   partsOff: old-format PAT blob (magic 2, 0x01234567), cgOff: "BMP Cutter3",
//   then 256 x 64-byte CP932 pattern names.
// Pattern: 0x44 header {frames, moveInfo, level, boxOff, atOff, ifOff, efOff, 40 B garbage},
//   frames (216 B: AF 44 | AS 56 | int16 idx[58]), then box(8)/AT(88)/IF(52)/EF(52)
//   tables, records appended in frame/slot order (no dedup).

#include <cstdint>
#include <string>
#include <vector>

class FrameData;

struct Ha4Container
{
	uint8_t header[0x44]{};          // file header as loaded (offsets are recomputed on save)
	std::vector<uint8_t> parts;      // embedded parts blob (old PAT), may be empty
	std::vector<uint8_t> cg;         // embedded CG blob ("BMP Cutter3"/"BMP Cutter2")
	std::string sourcePath;          // file it was loaded from
};

namespace ha4 {

constexpr int kPatterns = 256;
constexpr int kMaxFrames = 100;
constexpr int kFrameSize = 216;
constexpr int kNamesSize = 0x4000;

// True if the buffer starts with the "Hantei4\0" signature.
bool IsHA4(const void *data, size_t size);
bool IsHA4File(const char *filename);

// Parse an HA4 image into fd (replaces its contents). Returns false on a
// malformed container; `err` gets a human-readable reason.
bool Load(FrameData &fd, const uint8_t *data, size_t size, std::string *err = nullptr);
bool LoadFile(FrameData &fd, const char *filename, std::string *err = nullptr);

// Serialize fd to an HA4 image. Requires fd.m_ha4 (for the parts/CG blobs;
// without it the blobs are empty). Fails (with err) if the data cannot be
// represented: pattern index >= 256, > 100 frames, > 8 IF/EF per frame,
// > 32767 table records. `warnings` receives lossy-field notes (e.g. HA6-only
// fields that HA4 cannot store).
bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err = nullptr,
               std::vector<std::string> *warnings = nullptr);
// Atomic write (temp file + rename).
bool SaveFile(const FrameData &fd, const char *filename, std::string *err = nullptr,
              std::vector<std::string> *warnings = nullptr);

// Result of the last SaveFile (error text, lossy-field warnings); shown by the UI.
const std::string &LastSaveError();
const std::vector<std::string> &LastSaveWarnings();

// AF flip/rotate mode (+8) + arbitrary angle (+0x24) <-> HA6 layer rotation (AFAX/AFAY/AFAZ).
void DecodeFlip(int mode, int rot, float r[3]);
void EncodeFlip(const float r[3], int &mode, int &rot);

// Box coordinate conversion (HA6 = model space). Parts frames (sprite < 10000)
// use double-resolution coordinates: mbacPC Actor_BoxToWorldRect 0x440750.
void BoxToModel(const int16_t in[4], bool partsFrame, int out[4]);
void BoxFromModel(const int in[4], bool partsFrame, int16_t out[4]);

// EF types whose p0/p1 are frame-space positions stored with a (+128,+224) bias.
bool EfHasPositionBias(int type);

} // namespace ha4

#endif /* FRAMEDATA_HA4_H_GUARD */
