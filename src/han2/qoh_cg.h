#ifndef HAN2_QOH_CG_H_GUARD
#define HAN2_QOH_CG_H_GUARD

// QoH sprite images ('98 and '99) as a CgForeignBank: image i = the 16x16 tiles of image record i placed at their (x, y) inside the canvas, 4-bit pixels (index 0 transparent),
// 16 palette banks of 16 entries from the 1024-byte BGRX palette. The whole plaintext file stays the authoritative buffer; replacing an image rewrites tile pixels in place.
#include "../cg.h"
#include "qoh_dat.h"
#include <memory>

namespace han2 {

class QohCgBank : public CgForeignBank {
public:
	static std::shared_ptr<QohCgBank> Parse(const uint8_t *plain, size_t n, int version, std::string *err);
	const qoh::Layout &layout() const { return m_layout; }
	unsigned imageCount() const override { return m_layout.images; }
	const char *imageName(unsigned n) const override;
	bool imageInfo(unsigned n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) const override;
	bool imageCells(unsigned n, std::vector<CgCellRect> &out) const override;
	ImageData *draw(unsigned n, bool toPow2, bool indexed, const unsigned *palette) const override;
	int paletteCount() const override { return 16; }
	const unsigned *palette(int i) const override { return m_pal[i & 15]; }
	bool replaceImage(unsigned n, const unsigned char *rgba, int w, int h, int paletteIndex, std::string *err) override;
	bool dirty() const override { return m_dirty; }
	void clearDirty() override { m_dirty = false; }
	void serialize(std::vector<uint8_t> &out) const override { out = m_file; }
private:
	std::vector<uint8_t> m_file; qoh::Layout m_layout; bool m_dirty = false;
	unsigned m_pal[16][256];
	mutable char m_name[16];
	bool bounds(unsigned n, int &x1, int &y1, int &x2, int &y2) const;
};

} // namespace han2
#endif
