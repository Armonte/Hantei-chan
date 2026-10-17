#ifndef CGM_IO_H_GUARD
#define CGM_IO_H_GUARD
// BankIO: what batch export / import and the browser need from a sprite bank, whatever its format. Implemented for the editable BMP Cutter model
// (cgm::Bank) and, for every other bank Hantei-chan reads (MB / PB2K1 strips, QoH, ...), over the CG object and its CgForeignBank.
#include "cgm_bank.h"
#include "cgm_ops.h"
#include <string>
#include <vector>

class CG;

namespace cgm {

struct ImgInfo {
	bool present = false, drawable = false;
	int type = 0, bpp = 0, w = 0, h = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, blocks = 0;
	char name[32] = {};
	std::vector<int> owners, dependants;   // images whose cells this image draws / that draw its cells (BMP Cutter only)
	int boundsW() const { return x2 - x1 + 1; }
	int boundsH() const { return y2 - y1 + 1; }
};

class BankIO {
public:
	virtual ~BankIO() {}
	virtual int slots() const = 0;                                   // image slots (ids 0..slots-1)
	virtual bool info(int n, ImgInfo &out) const = 0;
	virtual bool decode(int n, Rgba &out, const uint32_t *pal) const = 0;
	virtual bool indexable(int n) const = 0;                          // can the image be exported as an 8-bit indexed PNG
	virtual bool decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], const uint32_t *bankPal) const = 0;
	virtual void defaultPalette(uint32_t pal[256]) const = 0;         // palette type 0 images use (normalised, entry 0 transparent)
	virtual int paletteSlots() const = 0;
	virtual void slotPalette(int s, uint32_t pal[256]) const = 0;
	virtual bool replace(int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep, const std::vector<uint8_t> *idx) = 0;
};

class BmpCutterIO : public BankIO {
public:
	explicit BmpCutterIO(Bank &b) : m_b(b) {}
	int slots() const override { return (int)m_b.images.size(); }
	bool info(int n, ImgInfo &o) const override;
	bool decode(int n, Rgba &o, const uint32_t *pal) const override { return m_b.decode(n, o, pal); }
	bool indexable(int n) const override { const Image &im = m_b.images[n]; return im.drawable() && (im.type == 0 || im.type == 2); }
	bool decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], const uint32_t *bp) const override { return m_b.decodeIndexed(n, idx, pal, nullptr, bp); }
	void defaultPalette(uint32_t pal[256]) const override { m_b.palette(0, pal); }
	int paletteSlots() const override { return 8; }
	void slotPalette(int s, uint32_t pal[256]) const override { memcpy(pal, m_b.palettes.data() + 0x400 * s, 1024); }
	bool replace(int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep, const std::vector<uint8_t> *idx) override { return ReplaceImage(m_b, n, rgba, w, h, err, rep, idx); }
private:
	Bank &m_b; mutable std::unordered_map<uint32_t, int> m_atlas; mutable bool m_haveAtlas = false;
};

// Any bank the CG object holds (foreign strip / tile banks included). Pixels come from CG::draw_texture with the CG's current palette.
class CgIO : public BankIO {
public:
	explicit CgIO(CG &cg) : m_cg(cg) {}
	int slots() const override;
	bool info(int n, ImgInfo &o) const override;
	bool decode(int n, Rgba &o, const uint32_t *pal) const override;
	bool indexable(int n) const override;
	bool decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], const uint32_t *bankPal) const override;
	void defaultPalette(uint32_t pal[256]) const override;
	int paletteSlots() const override;
	void slotPalette(int s, uint32_t pal[256]) const override;
	bool replace(int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep, const std::vector<uint8_t> *idx) override;
private:
	CG &m_cg;
};

} // namespace cgm
#endif
