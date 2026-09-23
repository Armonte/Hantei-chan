#ifndef HA4_CHARACTER_H_GUARD
#define HA4_CHARACTER_H_GUARD

// Editor-side glue for MBAC Hantei4 (.DAT) characters: sprite/palette/parts
// resources, the "MBAC (HA4)" inspector window and HA6 export.

#include <string>

class CharacterInstance;
struct FrameState;

namespace ha4 {

// After frameData loaded an HA4 file: load the embedded CG, the sibling .PAL,
// the embedded parts (converted to the Parts model) and, for normal
// characters, the sibling EFFECT.DAT as the effect character.
// Returns a one-line summary (for logging / tooltips).
std::string AttachCharacterResources(CharacterInstance &ch, const std::string &datPath);

} // namespace ha4

namespace ha4ui {

extern bool showInspector;

// "MBAC (HA4)" window: HA4-only fields for the current pattern/frame.
// No-op unless the character's data came from an HA4 file.
void DrawInspector(CharacterInstance *ch, FrameState &state);

// File -> Export MBAC as HA6: writes <ha6Path> plus .cg/.pat/.pal/.txt next to it.
// Returns false on error; `report` gets a human-readable summary.
bool ExportAsHA6(CharacterInstance *ch, const std::string &ha6Path, std::string &report);

} // namespace ha4ui

#endif
