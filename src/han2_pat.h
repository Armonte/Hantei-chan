#ifndef HAN2_PAT_H_GUARD
#define HAN2_PAT_H_GUARD

// PAT v3 (RBO, embedded in .DAT) / v4 (GOF2 .PAT) -> Parts model. Format: docs/formats/frenchbread_rbo_gof.md section 4.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Parts;

namespace han2 {

bool IsPat(const uint8_t *blob, size_t size);
// Fill `parts` (Free()d first). No GL work; call UploadPartsTextures to render.
bool PatToParts(const uint8_t *blob, size_t size, Parts &parts, std::string *err = nullptr);
// Rebuild a PAT block from the (possibly edited) Parts model and the original block; byte-identical when nothing changed.
bool BuildPat(const Parts &parts, const std::vector<uint8_t> &original, std::vector<uint8_t> &out, std::string *err = nullptr);
void UploadPartsTextures(Parts &parts);   // han2_pat_gl.cpp (GUI build only)

} // namespace han2

#endif
