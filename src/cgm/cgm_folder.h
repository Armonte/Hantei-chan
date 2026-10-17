#ifndef CGM_FOLDER_H_GUARD
#define CGM_FOLDER_H_GUARD
// cgtool workflows on a folder of PNGs (docs/cg/cgtool_reference.md): build a bank from scratch, re-import a plain cgtool export
// (no manifest), and drive the bank's structure from the folder (add / remove / reorder images by adding, deleting and renaming files).
//
// File names. Identity: "<name>_ID_<n>.png" (what `cgmtool export --cgtool` and cgtool write) is old image n; without that tag a
// leading number "NNNN_<name>.png" (our rgba/ and indexed/ export) is image NNNN; anything else is a NEW image. Order: an
// order.txt (one file name per line) wins; otherwise an order prefix "<k>#" ("12.5#x_ID_40.png" moves image 40 between 12 and
// 13, "3.5#new.png" adds a new image there), else the identity; new files without a prefix go last, by name. So an untouched
// cgtool export maps to itself.
// PNGs are canvas-sized (cgtool) or bounds-sized (our export); a canvas-sized PNG's content box gives the sprite's position.
#include "cgm_bank.h"
#include <string>
#include <vector>

namespace cgm {

struct FolderEntry {
	std::string path, file;      // UTF-8 full path, file name
	int id = -1;                 // identity (old image id), -1 = new image
	double order = 1e18;         // sort key
	bool explicitOrder = false;  // came from order.txt or a leading number
};
bool ScanImageFolder(const std::string &dir, std::vector<FolderEntry> &out, std::string *err);

// Reads a PNG: RGBA always; `idx`/`pal` filled when it is an 8-bit indexed PNG.
bool ReadFolderPng(const std::string &path, std::vector<uint8_t> &rgba, int &w, int &h, std::vector<uint8_t> *idx, uint32_t pal[256], std::string *err);

// Adds a canvas-sized picture as image `at` (-1 = append): the content box becomes the bounds, the PNG size the canvas.
// `type` -99 = automatic (8-bit on the bank palette when an indexed PNG uses that palette, else 2 / 4 / 1).
bool AddCanvasImage(Bank &bank, int at, const std::string &name, int type, const uint8_t *rgba, int cw, int ch, const std::vector<uint8_t> *idx, const uint32_t *pngPal,
                    std::vector<int> &remap, int *newId, std::string *err);

struct BuildOptions {
	std::string palettePath;     // palette for slot 0..7 (.pal / .act / .gpl / indexed .png); "" = the first indexed PNG's palette
	int palIndex = 0;            // which palette of a multi-palette .pal
	bool cutter2 = false;        // write a "BMP Cutter2" header (no: always Cutter3 for new banks; reserved)
};
struct BuildReport { int images = 0, placeholders = 0, indexed = 0; std::vector<std::string> warnings; std::string error; };
// New "BMP Cutter3" bank from a folder (or a list of files: ids follow the list order).
bool BuildBankFromFolder(const std::string &dir, const BuildOptions &opt, Bank &out, BuildReport &rep);
bool BuildBankFromFiles(const std::vector<std::string> &files, const BuildOptions &opt, Bank &out, BuildReport &rep);
Bank EmptyBank(const uint32_t pal[256]);

// Re-import a cgtool export without a manifest: every file whose identity is an existing image is compared with it; changed
// pictures are written in place (same box) or re-cut (other size / position, ReplaceImageResized). Ids never change.
struct FolderImportReport { std::vector<int> replaced, resized; int unchanged = 0; std::vector<std::string> warnings; std::string error; };
bool ImportPlainFolder(Bank &bank, const std::string &dir, FolderImportReport &rep);

// The folder becomes the bank: images whose identity has no file are deleted, new files are added, the order follows the
// folder, and changed pictures are re-imported. remap = old id -> new id (-1 = deleted) for the frame references.
struct FolderSyncReport { int kept = 0, added = 0, removed = 0, moved = 0; FolderImportReport pixels; std::string error; };
bool SyncBankToFolder(Bank &bank, const std::string &dir, std::vector<int> &remap, FolderSyncReport &rep);

// Image n as a canvas-sized RGBA picture (cgtool layout) and the decoded content.
bool CanvasPicture(const Bank &bank, int n, std::vector<uint8_t> &rgba, int &cw, int &ch, const uint32_t *pal = nullptr);

} // namespace cgm
#endif
