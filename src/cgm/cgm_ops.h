#ifndef CGM_OPS_H_GUARD
#define CGM_OPS_H_GUARD
// Edits of a cgm::Bank. Every operation touches only the bytes it must: replacing an image rewrites that image's blob and nothing else.
#include "cgm_bank.h"
#include <string>
#include <vector>

namespace cgm {

struct ReplaceReport {
	int changedPixels = 0;               // pixels of the image whose stored value changed
	int lostPixels = 0;                  // changed pixels that sit in blocks borrowed from another image (cannot be written here)
	std::vector<int> borrowedFrom;       // owner images of the lost pixels
	std::vector<int> alsoChanges;        // other images that draw cells of this image and therefore change too
	bool paletteChanged = false;
};

// New pixels (straight RGBA, memory R,G,B,A) for image n; must have the image's bounds size. Types 0 (colours snap to the bank palette
// unless `indices` is given), 1, 2, 3 (alpha only), 4. Pixels that already render identically keep their stored index, so an image edited
// in a few places changes in few bytes. `indices`: optional exact 8-bit indices for type 0.
bool ReplaceImage(Bank &bank, int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep = nullptr, const std::vector<uint8_t> *indices = nullptr);

// Do the stored bytes of image n already render as `rgba` (type 2: alpha < 128 counts as transparent)?
bool SameRendering(const Bank &bank, int n, const uint8_t *rgba, int w, int h);

} // namespace cgm
#endif
