#ifndef HAN2_CONTAINER_H_GUARD
#define HAN2_CONTAINER_H_GUARD

// HAN2RBO container: RBO .DAT / .DT2 and GOF2 .DT2. Spec: docs/formats/frenchbread_rbo_gof.md section 2.
//
//   0x00 "HAN2RBO "   0x08 kind (0 full .DAT, 3 pattern-area-only .DT2)   0x10 version=2
//   0x18 sub (1 RBO, 2 GOF2)   0x1C xor flag   0x20 4 x {off,size}: pattern area, parts (PAT), CG, names
//   pattern area = header (11+8+3 dwords RBO, 12+9+3 GOF2) followed by the section blobs
//
// The container keeps every byte it does not understand. A load -> Serialize with no edits is byte-identical.

#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

constexpr size_t kFileHeader = 0x40;

enum AreaIdx { kAreaPattern = 0, kAreaParts = 1, kAreaCg = 2, kAreaNames = 3 };

struct Han2File {
	uint8_t  header[kFileHeader]{};   // verbatim file header; offsets/sizes of areas are refreshed by Serialize
	uint32_t kind = 0;                // header +0x08
	uint32_t sub = 0;                 // header +0x18 (1 = RBO, 2 = GOF2)
	uint32_t xorFlag = 0;             // header +0x1C
	// the four areas as stored on disk (any may be empty; a .DT2 holds the pattern area only)
	std::vector<uint8_t> area[4];
	uint32_t areaOff[4]{};            // header offsets as found (for .DT2 they still point into the .DAT)

	// pattern area, split. lead/tail dwords are kept verbatim.
	std::vector<uint32_t> lead;       // 11 (RBO) or 12 (GOF2)
	std::vector<uint32_t> tail;       // 3
	std::vector<std::vector<uint8_t>> sec;   // 8 (RBO) or 9 (GOF2) section blobs

	size_t   nLead() const { return sub == 2 ? 12 : 11; }
	size_t   nSec()  const { return sub == 2 ? 9 : 8; }
	size_t   frameSize() const { return sub == 2 ? 404 : 300; }
	bool     patternAreaOnly() const { return kind == 3; }
};

bool IsHan2(const uint8_t *p, size_t n);
bool Parse(const uint8_t *p, size_t n, Han2File &out, std::string *err);
// Re-lays the file out. Sizes of the pattern area and sections are recomputed from the blobs;
// for kind 0 the offsets of the following areas are recomputed too.
bool Serialize(const Han2File &f, std::vector<uint8_t> &out, std::string *err);

} // namespace han2

#endif
