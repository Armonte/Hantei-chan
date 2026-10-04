#include "cgm_io.h"
#include "../cg.h"
#include <algorithm>
#include <cstring>

namespace cgm {

bool BmpCutterIO::info(int n, ImgInfo &o) const {
	o = ImgInfo(); if (n < 0 || n >= (int)m_b.images.size()) return false;
	const Image &im = m_b.images[n]; o.present = im.present; if (!im.present) return true;
	o.drawable = im.drawable(); o.type = im.type; o.bpp = im.bpp; o.w = im.w; o.h = im.h; o.x1 = im.x1; o.y1 = im.y1; o.x2 = im.x2; o.y2 = im.y2; o.blocks = (int)im.blocks.size();
	memcpy(o.name, im.name, 32);
	if (!m_haveAtlas) { m_atlas = m_b.buildAtlas(); m_haveAtlas = true; }
	o.owners = m_b.owners(n, m_atlas); o.dependants = m_b.dependants(n, m_atlas);
	return true;
}

int CgIO::slots() const { return const_cast<CG &>(m_cg).get_image_count(); }
bool CgIO::info(int n, ImgInfo &o) const {
	o = ImgInfo(); int bpp, ty, x1, y1, x2, y2;
	if (n < 0 || n >= slots()) return false;
	if (!m_cg.image_info((unsigned)n, bpp, ty, x1, y1, x2, y2)) { o.present = false; return true; }
	o.present = true; o.drawable = x2 >= x1 && y2 >= y1; o.type = ty; o.bpp = bpp; o.x1 = x1; o.y1 = y1; o.x2 = x2; o.y2 = y2; o.w = x2 + 1; o.h = y2 + 1;
	if (const char *nm = m_cg.get_filename((unsigned)n)) strncpy(o.name, nm, 31);
	std::vector<CgCellRect> cells; if (m_cg.image_cells((unsigned)n, cells)) o.blocks = (int)cells.size();
	return true;
}
bool CgIO::decode(int n, Rgba &o, const uint32_t *) const {
	ImageData *im = m_cg.draw_texture((unsigned)n, false, false); if (!im) return false;
	o.w = im->width; o.h = im->height; o.px.assign(im->pixels, im->pixels + (size_t)o.w * o.h * 4); delete im; return true;
}
bool CgIO::indexable(int n) const { return m_cg.image_is_8bpp((unsigned)n); }
bool CgIO::decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], const uint32_t *) const {
	if (!m_cg.image_is_8bpp((unsigned)n)) return false;
	ImageData *im = m_cg.draw_texture((unsigned)n, false, true); if (!im || !im->is8bpp) { delete im; return false; }
	idx.assign(im->pixels, im->pixels + (size_t)im->width * im->height);
	if (const unsigned *p = m_cg.getPalettePtr()) memcpy(pal, p, 1024); else memset(pal, 0, 1024);
	delete im; return true;
}
void CgIO::defaultPalette(uint32_t pal[256]) const { const unsigned *p = m_cg.paletteAt(0); if (p) memcpy(pal, p, 1024); else memset(pal, 0, 1024); }
int CgIO::paletteSlots() const { return std::max(1, m_cg.getPalNumber()); }
void CgIO::slotPalette(int s, uint32_t pal[256]) const { const unsigned *p = m_cg.paletteAt(s); if (p) memcpy(pal, p, 1024); else memset(pal, 0, 1024); }
bool CgIO::replace(int n, const uint8_t *rgba, int w, int h, std::string *err, ReplaceReport *rep, const std::vector<uint8_t> *) {
	const unsigned long long g0 = m_cg.generation();
	if (!m_cg.replace_image_rgba((unsigned)n, rgba, w, h, err)) return false;
	if (rep) { *rep = ReplaceReport(); rep->changedPixels = m_cg.generation() != g0 ? 1 : 0; }
	return true;
}

} // namespace cgm
