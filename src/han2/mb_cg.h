#ifndef HAN2_MB_CG_H_GUARD
#define HAN2_MB_CG_H_GUARD

// Melty Blood (2002) and Party Breakers (2001) embedded sprite banks share one image model ("strip banks"): a table of image (group) offsets, 8 palette sets, a table of
// 14-byte strips, and per image a 52-byte header (name, inclusive bounds, first strip, strip count) followed by the raw pixels of its strips. Only the table positions differ.
// Melty Blood (2002) embedded sprite bank (docs/formats/mb.md section 4): 3000 image offsets, 8 palette sets, a table of 14-byte strips and, per image, a
// 52-byte header followed by the raw pixels of its strips (8-bit palette indices, or 3-byte BGR in the one 24-bit bank, EFFECT.DAT). The stored bytes stay
// authoritative (strip pixels are edited in place, nothing moves), so an unedited bank saves byte-identical.
#include "../cg.h"
#include <memory>

namespace han2 {

struct MbCgStrip { int16_t x, y, width, height, u, v, textureSlot; };

struct StripBankLayout {
	unsigned maxImages;          // entries in the image offset table (at 0x14)
	bool offsetsSigned;          // MB: s32 with -1 = absent; PB: u32 with 0xFFFFFFFF = absent (same bit pattern)
	size_t palOff, pieceCountOff, pieceOff;
	bool trueColorByte;          // MB: byte 0x10 == 0xFF; PB: dword 0x10 == 255
	bool exactEnd;               // images must end exactly at the end of the blob (MB); PB banks may carry slack
	const char *what;
};
extern const StripBankLayout kMbCgLayout, kPb2CgLayout;

class MbCgBank : public CgForeignBank {
public:
	// Validates the whole structure (offsets tile the blob exactly); false with *err otherwise.
	static std::shared_ptr<MbCgBank> Parse(const uint8_t *p, size_t n, std::string *err, const StripBankLayout &layout = kMbCgLayout);
	bool trueColor() const { return m_trueColor; }
	size_t size() const { return m_blob.size(); }
	// CgForeignBank
	unsigned imageCount() const override { return m_count; }
	const char *imageName(unsigned n) const override;
	bool imageInfo(unsigned n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) const override;
	bool imageCells(unsigned n, std::vector<CgCellRect> &out) const override;
	ImageData *draw(unsigned n, bool toPow2, bool indexed, const unsigned *palette) const override;
	int paletteCount() const override { return 8; }
	const unsigned *palette(int i) const override { return m_pal[i & 7]; }
	bool replaceImage(unsigned n, const unsigned char *rgba, int w, int h, int paletteIndex, std::string *err) override;
	bool dirty() const override { return m_dirty; }
	void clearDirty() override { m_dirty = false; }
	void serialize(std::vector<uint8_t> &out) const override { out = m_blob; }
private:
	struct Img { uint32_t off = 0; bool present = false; uint32_t firstPiece = 0, pieceCount = 0; size_t dataOff = 0; };
	std::vector<uint8_t> m_blob;
	std::vector<Img> m_img; unsigned m_count = 0; bool m_trueColor = false; bool m_dirty = false;
	unsigned m_pal[8][256];
	size_t m_pieceOff = 0; const StripBankLayout *m_layout = &kMbCgLayout;
	MbCgStrip strip(uint32_t i) const;
};

} // namespace han2
#endif
