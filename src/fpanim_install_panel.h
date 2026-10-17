#ifndef FPANIM_INSTALL_PANEL_H_GUARD
#define FPANIM_INSTALL_PANEL_H_GUARD

// "fpan-im (UNI2 / MBTL)" window (Game menu): per configured game folder, whether fpan-im is installed (core + shim
// versions from their version resources), whether the running game answers on the fpan-im pipe, and Install / Update /
// Uninstall from the copy bundled in res/fpan-im (or a chosen folder). Logic: fpanim_install.h.

namespace fpanimpanel {

extern bool showPanel;
void DrawPanel();   // every frame; does nothing while hidden (a ping already running finishes in the background)

} // namespace fpanimpanel

#endif
