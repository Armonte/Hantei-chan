#ifndef HAN2_QOH_DAT_H_GUARD
#define HAN2_QOH_DAT_H_GUARD

// Queen of Heart character files (docs/formats/qoh98.md, docs/formats/qoh99.md): '98 .dat (plain) and '99 .chr (enciphered with the file-name stem). The two
// share every record (tiles, actions, frames, boxes, attacks); only the container differs, so one layout descriptor serves both.
//   '98:  u32 version(5) | u32 c[6] = image, tile, anim, sprite, body, hit | TimPath[image] 260 | ImageRange[image] 8 | Tile[tile] 260 | Anim[anim] 24 | Sprite[sprite] 52 | palette 1024 | BodyRect[body] 8 | HitBox[hit] 24
//   '99:  name[16] | u32 tag | u32 c[6] = image, tile, action, frame, box, attack | Box[box] 8 | Action[action] 24 | ImageRec[image] 12 | palette 1024 | Tile[tile] 260 | Frame[frame] 64 | Attack[attack] 24
#include <cstdint>
#include <string>
#include <vector>

namespace han2 { namespace qoh {

enum Version : int { V98 = 98, V99 = 99 };

struct Layout {
	int version = 0;
	uint32_t images = 0, tiles = 0, actions = 0, frames = 0, boxes = 0, attacks = 0;
	size_t imageRecSize = 8, frameSize = 52;
	size_t timPaths = 0, imageRecs = 0, tileOff = 0, actionOff = 0, frameOff = 0, paletteOff = 0, boxOff = 0, attackOff = 0, end = 0;
};

// Plaintext layouts. false (with err) when the sizes do not add up to exactly `size` or the header is implausible.
bool ComputeLayout98(const uint8_t *p, size_t size, Layout &out, std::string *err = nullptr);
bool ComputeLayout99(const uint8_t *p, size_t size, Layout &out, std::string *err = nullptr);
bool LooksLike98(const uint8_t *p, size_t size);

// '99 cipher (docs/formats/qoh99.md section 2). `stem` = upper-case file name without directory and extension ("CORIN").
// Decrypt returns false when the name block does not decode to the stem or the sizes do not add up; Encrypt is its exact inverse.
std::string StemOfPath(const std::string &path);
bool Decrypt99(std::vector<uint8_t> &file, const std::string &stem, std::string *err = nullptr);
void Encrypt99(std::vector<uint8_t> &file, const std::string &stem);   // `file` must be plaintext with consistent counts

}} // namespace han2::qoh
#endif
