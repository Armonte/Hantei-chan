#ifndef PARTS_UNDO_H_GUARD
#define PARTS_UNDO_H_GUARD

// Undo channel for a Parts model (PAT editor): joins the character's UndoManager history, so PAT-editor edits are undoable
// with Ctrl+Z, set the tab '*' and are committed with the same gesture rules as frame edits (undo_manager.h).
//
// Snapshot = poses (one shared copy per pose), cut-outs, shapes and sprite-sheet records. Consecutive snapshots share
// every piece that did not change, so a step that moves one part stores one pose. Sprite-sheet pixels are not copied:
// PartGfx::data points into buffers the model never frees before the character is reloaded (Parts::AdoptPixels), so a
// snapshot keeps the pointer. A restore that changes sheet records calls onSheetsRestored (GL re-upload).
//
// Enabled by `enabled` (CharacterInstance turns it on for French-Bread characters: their parts are saved with the
// character). UNI / MBAACC .pat files are saved separately (savePAT); enabling it for them only needs their save path to
// clear the dirty state too (see docs/formats/pacnyx_parity.md, "Parts undo").
#include "../undo_manager.h"
#include <functional>

class Parts;

class PartsUndoChannel : public UndoChannel {
public:
	explicit PartsUndoChannel(Parts* parts) : m_parts(parts) {}
	const char* name() const override { return "parts"; }
	bool active() const override;
	Snap capture(const Snap& previous) override;
	bool equalsLive(const Snap& s) const override;
	void restore(const Snap& s) override;
	size_t approxBytes(const Snap& s) const override;

	std::function<bool()> enabled;              // null = always
	std::function<void()> onSheetsRestored;     // after a restore that changed sprite-sheet records
	std::function<void()> onRestored;           // after every restore (pane name caches)

private:
	Parts* m_parts;
};

#endif
