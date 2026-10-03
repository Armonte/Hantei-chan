#ifndef HAN2_CHARACTER_H_GUARD
#define HAN2_CHARACTER_H_GUARD

// Loading a French-Bread character (RBO .DAT/.DT2, GOF2 .DT2+.PAT+.CHP) as a Hantei-chan character:
// frame data, CG bank, parts. Files come from a ReadFn so they can sit in a folder or inside PAC archives.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class CharacterInstance;
struct FrameState;
namespace pac { struct Archive; }

namespace han2 {

// Reads the file called `name` (case-insensitive, e.g. "ACOLYTE_F.DAT"); false when it does not exist.
using ReadFn = std::function<bool(const std::string &name, std::vector<uint8_t> &out)>;

ReadFn DirReader(const std::string &dir);
// Archives in priority order: the first archive that has the name wins.
ReadFn PacReader(const std::vector<std::shared_ptr<pac::Archive>> &archives);

// `stem`: character file name without extension. The pattern area comes from <stem>.DT2 when present (as the game does),
// else <stem>.DAT; parts, CG and names always come from <stem>.DAT. `origin` is shown to the user (folder or archive name).
bool LoadCharacter(CharacterInstance &ch, const std::string &stem, const ReadFn &read, const std::string &origin, std::string *summary, std::string *err);

// GOF1: load character `entryName` (e.g. AKIKO.DAT) from a gof_0N.p archive: archive cipher + section cipher undone, parts from the old PAT v2 block.
bool LoadGof1Character(CharacterInstance &ch, const std::string &archivePath, const std::string &entryName, std::string *err);

// Rebuild the container's PAT block from the character's (possibly edited) Parts model before a save.
// Returns false with *err on failure; *partsChanged tells whether the block differs from the one loaded.
bool SyncPartsToContainer(CharacterInstance &ch, bool *partsChanged, std::string *err);

// GOF2: write the edited <stem>NN.PAT / <stem>NN.CHP next to the .DT2 (only the ones that changed; .bak of the first version kept).
bool SaveGof2Companions(CharacterInstance &ch, const std::string &dt2Path, std::string *err);

} // namespace han2

namespace han2ui {

extern bool showInspector;
void DrawInspector(CharacterInstance *ch, FrameState &state);

extern bool showCgWindow;
extern bool showAnimWindow;
// Game-rate animation player; drives the view's pattern/frame so the editor draws the boxes of the playing frame.
void DrawAnimWindow(CharacterInstance *ch, FrameState &state, void *onion /* OnionSkinSettings* */);
extern bool showDiffWindow;
void DrawDiffWindow(CharacterInstance *ch);
// CG bank window: list, preview, export / import of the sprites (PNG).
void DrawCgWindow(CharacterInstance *ch);

} // namespace han2ui

#endif
