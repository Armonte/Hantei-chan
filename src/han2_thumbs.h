#ifndef HAN2_THUMBS_H_GUARD
#define HAN2_THUMBS_H_GUARD

// Frame thumbnails for the animation list and the frame strip: composited on the CPU (han2::RenderFrameThumb), cached as GL textures,
// generated lazily a few per frame so a 100-frame pattern never stalls the editor. The cache is keyed by character and frame and drops an entry
// when the character's data changed (FrameData::dataVersion).
#include <string>

class CharacterInstance;
struct FrameState;

namespace han2ui {

void BeginThumbFrame();                      // once per UI frame: resets the per-frame generation budget
// True and fills tex / w / h when a thumbnail exists; otherwise queues it (returns false). tex stays valid until the next call that evicts it.
bool FrameThumb(CharacterInstance &ch, int pattern, int frame, unsigned &tex, int &w, int &h);
void ForgetCharacterThumbs(const CharacterInstance *ch);

extern bool showAnimList;                    // "Animations" list: every pattern with a thumbnail, search, click to select
extern unsigned dockAnimListId;              // Left Pane node
void DrawAnimListWindow(CharacterInstance *ch, FrameState &state);

} // namespace han2ui

#endif
