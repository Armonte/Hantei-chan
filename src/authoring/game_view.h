#ifndef AUTHORING_GAME_VIEW_H_GUARD
#define AUTHORING_GAME_VIEW_H_GUARD
// [game-view] The "Game" panel: the running game rendered inside Hantei-chan (docs/HANTEI_AUTHORING_MODE.md §12, §12.1,
// §12.2). A dockable window of the Authoring workspace that
//   * opens the frame ring the game announces (QueryFrameShare) and uploads the newest frame to GL textures once per UI
//     frame, drawn aspect-correct (sidebars included), with an FPS / latency / dropped-frames readout;
//   * FULL mode: the game's finished frame. LAYERED mode: Hantei-chan's own stage renderer at the frame's camera (back
//     pass) -> the game's CHARS layer -> the stage's front pass -> the game's HUD layer, premultiplied alpha, so stage
//     edits show at once with no game reload;
//   * forwards keyboard / pad input while the panel is focused (InputInject), releasing on focus loss;
//   * draws an overlay from the frame's actors + the link state: hitboxes of the open characters (the selected box
//     highlighted) and pattern / frame labels;
//   * can move to its own OS window, or hand the picture back to the real game window ("Undock to real window").
#include "authoring_window.h"

namespace authoring {

extern bool showGameView;
void OpenGameView();
// Called every UI frame by MainFrame (the panel may be open while the Authoring window is closed).
void DrawGameView(HostContext& host);
// True while the panel is focused and forwarding input (MainFrame keeps editor shortcuts away from those keys).
bool GameViewCapturesInput();

} // namespace authoring

#endif
