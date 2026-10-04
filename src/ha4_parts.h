#ifndef HA4_PARTS_H_GUARD
#define HA4_PARTS_H_GUARD

// Old-format PAT (magic 2, 0x01234567) -> the editor's PAniDataFile Parts model.
//
// MBAC stores its parts ("PAT") inside the character .DAT in the pre-PAniDataFile
// format that MBAACC stages still use (docs/bg_research/BG_HA4_RE.md A.5). MBAACC
// ships the same parts converted to PAniDataFile; comparing MBAC ARC.DAT with
// MBAACC arc.pat gives the mapping used here (580/580 parts equal):
//   PRXY = (x - 320, y - 448)     PRZM = scale / 1000    PRAN = rot / 10000
//   PRPR = +71 priority           PRID = cutout          PRRV = +48 flip
//   PRAL = +49 additive           PRFL = +50 filter
//   PRCL = (B,G,R,A) from +67,+66,+65,+64   PRSP = add colour +68..70 (R,G,B)
//   cutout: PPUV = src rect (256-unit space), PPSS = quad W/H, PPCC = origin,
//           PPTP = texture, PPTX = cutout dword 1
//   texture: PGTX 32 bpp BGRA, square size from the texture region.

#include <cstdint>
#include <cstddef>
#include <string>

class Parts;

namespace ha4 {

// Fill `parts` (Free()d first) from an old-PAT blob. Texture pixels are copied
// into a buffer owned by parts.data. No GL work is done here; the caller
// uploads textures (UploadPartsTextures) when it wants to render.
bool OldPatToParts(const uint8_t *blob, size_t size, Parts &parts, std::string *err = nullptr);

// Create GL textures for parts.gfxMeta (same as Parts::Load does for PGTX).
// Defined in ha4_parts_gl.cpp (GUI build only).
void UploadPartsTextures(Parts &parts);

} // namespace ha4

#endif
