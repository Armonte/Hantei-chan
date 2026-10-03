#ifndef HAN2_PAT_H_GUARD
#define HAN2_PAT_H_GUARD

// PAT v3 (RBO, embedded in .DAT) / v4 (GOF2 .PAT) -> Parts model. Format: docs/formats/frenchbread_rbo_gof.md section 4.
#include <cstddef>
#include <cstdint>
#include <string>

class Parts;

namespace han2 {

bool IsPat(const uint8_t *blob, size_t size);
// Fill `parts` (Free()d first). No GL work; call UploadPartsTextures to render.
bool PatToParts(const uint8_t *blob, size_t size, Parts &parts, std::string *err = nullptr);
void UploadPartsTextures(Parts &parts);   // han2_pat_gl.cpp (GUI build only)

} // namespace han2

#endif
