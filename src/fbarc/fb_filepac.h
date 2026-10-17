#ifndef FBARC_FB_FILEPAC_H_GUARD
#define FBARC_FB_FILEPAC_H_GUARD
#include "fb_archive.h"
namespace fbarc {
// "FilePacHeaderA" (Melty Blood AA PC / Steam .p) reader + writer. Spec: docs/formats/fb_archives.md.
std::unique_ptr<Archive> LoadFilePacA(const std::string& utf8Path, std::string* err);
}
#endif
