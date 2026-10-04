#ifndef CGM_EXPORT_H_GUARD
#define CGM_EXPORT_H_GUARD
// Batch export / import of a whole bank as a folder: manifest.json + rgba/*.png + indexed/*.png + palettes/*.pal.
// Import re-encodes only images whose pixels changed (FNV-1a hash of the decoded RGBA, recorded in the manifest at export time).
#include "cgm_bank.h"
#include <string>
#include <vector>

namespace cgm {
class BankIO;

struct ExportOptions {
	bool rgba = true;            // rgba/NNNN_name.png (straight alpha, bounds sized)
	bool indexed = true;         // indexed/NNNN_name.png for types 0, 2 and 4 (palette preserved, 8-bit PNG)
	bool palettes = true;        // palettes/bank_slotN.pal (+ the external palette file when given)
	std::vector<int> onlyIds;    // export just these image ids (empty = all); the manifest still lists every image
	bool cgtoolLayout = false;   // <name>_ID_<n>.png, canvas sized RGBA/indexed, no manifest (compatible with the community cgtool)
};
struct ExportResult { int images = 0, files = 0; std::string error; };

// extPal: optional 256-entry palette (0xAABBGGRR) used for type 0 images instead of bank slot 0 (e.g. the character's .pal number).
bool ExportBank(const Bank &bank, const std::string &dir, const std::string &bankName, const ExportOptions &opt, ExportResult &res, const uint32_t *extPal = nullptr);

struct ImportResult {
	std::vector<int> changed;               // image ids re-encoded
	std::vector<std::string> warnings;      // size mismatch, lost pixels in borrowed blocks, dependants that also change ...
	int skipped = 0;                        // images whose pixels are unchanged
	std::string error;
};
bool ImportBank(Bank &bank, const std::string &dir, ImportResult &res);
// the same over any bank format (BankIO, cgm_io.h)
bool ExportBank(const BankIO &bank, const std::string &dir, const std::string &bankName, const ExportOptions &opt, ExportResult &res, const uint32_t *extPal = nullptr);
bool ImportBank(BankIO &bank, const std::string &dir, ImportResult &res);

std::string HashHex(uint64_t h);
std::string FileSafeName(const char *name32);

} // namespace cgm
#endif
