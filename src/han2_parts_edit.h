#ifndef HAN2_PARTS_EDIT_H_GUARD
#define HAN2_PARTS_EDIT_H_GUARD

// PACNyx-style edits of a French-Bread (RBO / GOF2 / GOF1) parts model: sprite sheets, poses, parts, layers.
// Pure model code (no GL, no ImGui): the PAT editor panes, han2tool and tests call it. The writer (han2_pat_write.cpp)
// turns the model back into the PAT block; the limits here are the ones it enforces at save time, checked while editing.
// Behaviour reference: PACNyx+ DATView (2010) and icaroffa's 2026 build, reimplemented (docs/formats/pacnyx_parity.md).
#include <cstdint>
#include <string>
#include <vector>

#include "parts/parts.h"

namespace han2 {
namespace edit {

constexpr int kPartsPerPose = 40;    // the engine reads 40 part records per pose
constexpr int kMaxSheets = 50;       // texture slots of the PAT image head
constexpr int kMaxSheetSize = 4096;  // the largest texture the reader accepts

int MaxPoses(const Parts &parts);                     // 1000 (RBO / GOF1) or 2000 (GOF2 PAT v4)
int UsedPartSlots(const PartSet<> &ps); // highest used slot + 1
int FreePartSlot(const PartSet<> &ps);  // first slot without a part, -1 when all 40 are used
bool CanAddPart(const Parts &parts, int pose, std::string *why);
bool CanAddPose(const Parts &parts, std::string *why);

// Sprite sheets. Pixels are straight RGBA (memory R,G,B,A); the model stores them as the PAT does (B,G,R,A).
// slot == gfxMeta.size() appends a sheet. The sheet must be square (PACNyx: "width must equal height").
bool ImportSheetRgba(Parts &parts, int slot, const uint8_t *rgba, int w, int h, std::string *err);
bool SheetRgba(const Parts &parts, int slot, std::vector<uint8_t> &rgba, int &w, int &h);
// Number of parts (in every pose) whose cut-out draws from `slot`; `poses` gets the pose indices (deduplicated).
int SheetUsers(const Parts &parts, int slot, std::vector<int> *poses = nullptr);
// Remove a sheet (the slot becomes empty so the other sheet indices stay). Refused while parts use it unless force.
bool RemoveSheet(Parts &parts, int slot, bool force, std::string *err);

// Poses.
bool AddPose(Parts &parts, const std::string &name, int *newIndex, std::string *err);   // name required, first empty slot
// Swap the contents of poses a and b (pose indices are what frames reference: callers remap frame references).
bool SwapPoses(Parts &parts, int a, int b, std::string *err);
void ShiftPose(PartSet<> &ps, int dx, int dy);                           // "set pose base point": every part moves

// Parts.
// Source rects are stored in 1/256 of the sheet (a 512 sheet has 2 px per unit); the UI works in pixels.
int PxToUnits(int px, int sheetSize);
int UnitsToPx(int units, int sheetSize);
// New part from a sheet rectangle (pixels): a cut-out with that source rect (quad = rect size, origin = rect centre) and a part in
// the first free slot, layered just below the selected part (PACNyx NewBodyPartForm). Returns the slot or -1.
int AddPartFromSheet(Parts &parts, int pose, int sheet, int sx, int sy, int sw, int sh, int belowSlot, std::string *err);

// Cut-outs (source rect, quad size, origin, sheet) are shared between parts that were identical on load. Before a
// per-part edit of those values the part gets its own copy; returns the cut-out index the part now uses (-1: no part).
int UniqueCutOut(Parts &parts, int pose, int slot);

// Layer order (draw order is ascending (layer << 8) + slot: a higher layer is drawn on top; layer is one byte).
enum class LayerCmd { ToBack, Backward, Forward, ToFront, Set };
bool LayerCommand(PartSet<> &ps, int slot, LayerCmd cmd, int value = 0);
int LayerOf(const PartProperty &p);

} // namespace edit
} // namespace han2

#endif
