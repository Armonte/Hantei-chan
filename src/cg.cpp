#include <filesystem>
// .CG loader
//
// .CG contains information about sprite mappings from the ENC and PVR tiles.

#include "cg.h"
#include <algorithm>
#include "misc.h"

#include <cstdlib>
#include <cstring>

#include <iostream>

const CG_Image *CG::get_image(unsigned int n) {
	if (n >= m_nimages) {
		return 0;
	}
	
	unsigned int index = m_indices[n];
	if (index < 0) {
		return 0;
	}
	
	if (index + sizeof(CG_Image) > m_data_size) {
		return 0;
	}
	
	return (const CG_Image *)(m_data + index);
}

const char *CG::get_filename(unsigned int n) {
	if (!m_loaded) {
		return 0;
	}
	if (m_foreign) return n < m_foreign->imageCount() ? m_foreign->imageName(n) : nullptr;
	
	const CG_Image *image = get_image(n);
	if (!image) {
		return 0;
	}
	
	return image->filename;
}

bool CG::image_info(unsigned int n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) {
	if (m_foreign) return n < m_foreign->imageCount() && m_foreign->imageInfo(n, bpp, typeId, x1, y1, x2, y2);
	const CG_Image *image = get_image(n);
	if (!image || image->type_id == -1) return false;
	bpp = (int)image->bpp; typeId = image->type_id;
	x1 = image->bounds_x1; y1 = image->bounds_y1; x2 = image->bounds_x2; y2 = image->bounds_y2;
	return true;
}

bool CG::image_cells(unsigned int n, std::vector<CellRect> &out) {
	out.clear();
	if (m_foreign) return n < m_foreign->imageCount() && m_foreign->imageCells(n, out);
	const CG_Image *image = get_image(n);
	if (!image || image->type_id == -1) return false;
	if ((image->align_start + image->align_len) > m_nalign) return false;
	const CG_Alignment *a = &m_align[image->align_start];
	for (unsigned int i = 0; i < image->align_len; ++i, ++a)
		out.push_back({a->x, a->y, a->width, a->height});
	return true;
}

int CG::get_image_count() {
	if (m_foreign) return (int)m_foreign->imageCount();
	return m_nimages;
}

static unsigned long long g_cgGeneration = 0;

void CG::touch() {
	m_generation = ++g_cgGeneration;
}

void CG::copy_cells(const CG_Image *image,
			const CG_Alignment *align,
			unsigned char *pixels,
			unsigned int x1,
			unsigned int y1,
			unsigned int width,
			unsigned int height,
			unsigned int *palette,
			bool is_8bpp) {
	int w = align->width / cu;
	int h = align->height / cu;
	int x = align->source_x / cu;
	int y = align->source_y / cu;
	int cell_n = (y * cpr) + x;
	Page *im = &pages[align->source_image];
	
	for (int a = 0; a < h; ++a) {
		for (int b = 0 ; b < w; ++b) {
			ImageCell *cell = &im->cell[cell_n + b];
			
			if (cell->start == 0) {
				continue;
			}
			
			unsigned char *dest = pixels;
			unsigned int offset;
			
			offset = (align->y + (a * cu) - y1) * width;
			offset += align->x + (b * cu) - x1;
			
			if (is_8bpp) {
				// 8bpp -> 8bpp
				unsigned char *src = ((unsigned char *)m_data) + cell->start + cell->offset;
				int cellw = cell->width;
				
				dest += offset;
				
				for (int c = 0; c < cu; ++c) {
					for (int d = 0; d < cu; ++d) {
						dest[d] = src[d];
					}
					
					src += cellw;
					dest += width;
				}
			} else if (image->type_id == 4 || image->type_id == 5) {
				// two pass: first 8bit palettized (type 4: the image's own palette, type 5: the bank / character palette), second 8bit alpha
				unsigned int *ldest = (unsigned int *)dest;
				unsigned char *src = ((unsigned char *)m_data) + cell->start + cell->offset;
				int cellw = cell->width;
				
				ldest += offset;
				
				for (int c = 0; c < cu; ++c) {
					for (int d = 0; d < cu; ++d) {
						ldest[d] = palette[src[d]] & 0xffffff;
					}
					
					src += cellw;
					ldest += width;
				}
				
				ldest = (unsigned int *)dest;
				ldest += offset;
				
				src = ((unsigned char *)m_data) + cell->start + cell->offset;
				src += align->width * align->height;

				for (int c = 0; c < cu; ++c) {
					for (int d = 0; d < cu; ++d) {
						ldest[d] |= src[d] << 24;
					}
					
					src += cellw;
					ldest += width;
				}
			} else if (image->type_id == 1) {
				// 32bpp bgr -> rgb
				unsigned int *ldest = (unsigned int *)dest;
				unsigned int *src = (unsigned int *)(m_data + cell->start + cell->offset);
				int cellw = cell->width;
				
				ldest += offset;
				
				for (int c = 0; c < cu; ++c) {
					for (int d = 0; d < cu; ++d) {
						unsigned int v = src[d];
						v = (v & 0xff00ff00) | ((v&0xff) << 16) | ((v&0xff0000) >> 16);
						ldest[d] = v;
					}
					
					src += cellw;
					ldest += width;
				}
			} else {
				// palettized 8bpp -> 32bpp
				unsigned int *ldest = (unsigned int *)dest;
				unsigned char *src = ((unsigned char *)m_data) + cell->start + cell->offset;
				int cellw = cell->width;
				
				ldest += offset;
				
				
				for (int c = 0; c < cu; ++c) {
					for (int d = 0; d < cu; ++d) {
						ldest[d] = palette[src[d]];
					}
					
					src += cellw;
					ldest += width;
				}
			}
		}
		
		cell_n += cpr;
	}
}
			

// ---- palette quantizer (median cut) for replace_image_rgba ----
namespace {
struct QBox { std::vector<unsigned int> px; };   // packed 0xRRGGBB values (with repeats)
void MedianCut(std::vector<unsigned int> colors, int maxColors, std::vector<unsigned int> &outPal)
{
	std::vector<QBox> boxes(1); boxes[0].px = std::move(colors);
	while ((int)boxes.size() < maxColors) {
		int pick = -1; size_t bestN = 1;
		for (size_t i = 0; i < boxes.size(); i++) if (boxes[i].px.size() > bestN) { bestN = boxes[i].px.size(); pick = (int)i; }
		if (pick < 0) break;
		QBox &b = boxes[pick];
		int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
		for (unsigned int c : b.px) for (int k = 0; k < 3; k++) { int v = (c >> (16 - 8 * k)) & 255; lo[k] = std::min(lo[k], v); hi[k] = std::max(hi[k], v); }
		int axis = 0; for (int k = 1; k < 3; k++) if (hi[k] - lo[k] > hi[axis] - lo[axis]) axis = k;
		if (hi[axis] == lo[axis]) { bestN = 0; b.px.resize(1); continue; }
		const int sh = 16 - 8 * axis;
		std::sort(b.px.begin(), b.px.end(), [&](unsigned int x, unsigned int y) { return ((x >> sh) & 255) < ((y >> sh) & 255); });
		QBox nb; size_t half = b.px.size() / 2;
		nb.px.assign(b.px.begin() + half, b.px.end()); b.px.resize(half);
		boxes.push_back(std::move(nb));
	}
	for (auto &b : boxes) {
		unsigned long long r = 0, g = 0, bl = 0; for (unsigned int c : b.px) { r += (c >> 16) & 255; g += (c >> 8) & 255; bl += c & 255; }
		size_t n = std::max<size_t>(1, b.px.size());
		outPal.push_back(((unsigned)(r / n) << 16) | ((unsigned)(g / n) << 8) | (unsigned)(bl / n));
	}
}
}

bool CG::replace_image_rgba(unsigned int n, const unsigned char *rgba, int w, int h, std::string *err, bool force) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (m_foreign) { (void)force; bool ok = m_foreign->replaceImage(n, rgba, w, h, curPalIndex, err); if (ok) touch(); return ok; }
	const CG_Image *image = get_image(n);
	if (!image || image->type_id == -1) return fail("no such image");
	if ((image->align_start + image->align_len) > m_nalign) return fail("broken alignment table");
	const int x1 = image->bounds_x1, y1 = image->bounds_y1, bw = image->bounds_x2 - x1 + 1, bh = image->bounds_y2 - y1 + 1;
	if (w != bw || h != bh) return fail("size mismatch: image " + std::to_string(n) + " is " + std::to_string(bw) + " x " + std::to_string(bh) + ", the PNG is " + std::to_string(w) + " x " + std::to_string(h));
	const int ty = image->type_id;
	if (ty != 1 && ty != 2 && ty != 3 && ty != 4) return fail("storage type " + std::to_string(ty) + " cannot be imported (only 1, 2, 3, 4)");
	if (image->bpp != 32) return fail("only 32-bit banks are supported");
	if (!force) {   // unchanged pixels: keep the stored bytes (a re-quantised palette would reorder them)
		if (ImageData *cur = draw_texture(n, false, false)) {
			bool same = cur->width == w && cur->height == h;
			for (int i = 0; same && i < w * h; i++) {
				const unsigned char *a = &cur->pixels[i * 4], *b = &rgba[i * 4];
				if (a[3] != b[3] || (a[3] && memcmp(a, b, 3) != 0)) same = false;
			}
			delete cur;
			if (same) return true;
		}
	}
	char *base = (char *)image->data;
	std::vector<unsigned char> idx;            // palette index per pixel (types 2, 4)
	if (ty == 2 || ty == 4) {
		std::vector<unsigned int> opaque;
		for (int i = 0; i < w * h; i++) if (rgba[i * 4 + 3] != 0 && (ty == 4 || rgba[i * 4 + 3] >= 128)) opaque.push_back(((unsigned)rgba[i*4] << 16) | ((unsigned)rgba[i*4+1] << 8) | rgba[i*4+2]);
		std::vector<unsigned int> uniq = opaque; std::sort(uniq.begin(), uniq.end()); uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
		std::vector<unsigned int> pal;
		if ((int)uniq.size() <= 255) pal = uniq; else MedianCut(opaque, 255, pal);
		unsigned int *pw = (unsigned int *)base;
		for (int i = 0; i < 256; i++) pw[i] = 0;
		for (size_t i = 0; i < pal.size(); i++) { unsigned int c = pal[i]; pw[i + 1] = ((c >> 16) & 255) | (c & 0xFF00) | ((c & 255) << 16); }   // memory order R,G,B,x
		idx.assign((size_t)w * h, 0);
		for (int i = 0; i < w * h; i++) {
			if (rgba[i*4+3] == 0 || (ty == 2 && rgba[i*4+3] < 128)) continue;
			int r = rgba[i*4], g = rgba[i*4+1], b = rgba[i*4+2], best = 0, bd = 1 << 30;
			for (size_t k = 0; k < pal.size(); k++) {
				int dr = (int)((pal[k] >> 16) & 255) - r, dg = (int)((pal[k] >> 8) & 255) - g, db = (int)(pal[k] & 255) - b, d = dr*dr + dg*dg + db*db;
				if (d < bd) { bd = d; best = (int)k; if (d == 0) break; }
			}
			idx[i] = (unsigned char)(best + 1);
		}
	}
	// type 3 = one colour (the first dword of the image) with an 8-bit alpha plane: index i has alpha i, index 0 is transparent
	unsigned int address = (unsigned int)(base - m_data) + ((ty == 2 || ty == 4) ? 1024 : ty == 3 ? 4 : 0);
	const CG_Alignment *align = &m_align[image->align_start];
	for (unsigned int j = 0; j < image->align_len; ++j, ++align) {
		if (align->copy_flag != 0) continue;
		const int aw = align->width, ah = align->height;
		for (int ly = 0; ly < ah; ly++)
			for (int lx = 0; lx < aw; lx++) {
				const int cx = align->x + lx - x1, cy = align->y + ly - y1;
				const bool inside = cx >= 0 && cy >= 0 && cx < w && cy < h;
				const size_t o = (size_t)ly * aw + lx;
				if (ty == 1) {
					unsigned int *dst = (unsigned int *)(m_data + address) + o;
					if (inside) { const unsigned char *s = rgba + ((size_t)cy * w + cx) * 4; *dst = ((unsigned)s[3] << 24) | ((unsigned)s[0] << 16) | ((unsigned)s[1] << 8) | s[2]; }   // memory B,G,R,A
				} else if (ty == 3) {
					unsigned char *dst = (unsigned char *)(m_data + address) + o;
					*dst = inside ? rgba[((size_t)cy * w + cx) * 4 + 3] : 0;
				} else {
					unsigned char *dst = (unsigned char *)(m_data + address) + o;
					*dst = inside ? idx[(size_t)cy * w + cx] : 0;
					if (ty == 4) dst[(size_t)aw * ah] = inside ? rgba[((size_t)cy * w + cx) * 4 + 3] : 0;
				}
			}
		address += (unsigned)(aw * ah * (ty == 1 ? 4 : (ty == 4 ? 2 : 1)));   // types 2 and 3: one byte per pixel
	}
	touch();
	return true;
}

bool CG::image_is_8bpp(unsigned int n) {
	if (m_foreign) { int b, t, x1, y1, x2, y2; return n < m_foreign->imageCount() && m_foreign->imageInfo(n, b, t, x1, y1, x2, y2) && b <= 8; }
	const CG_Image *image = get_image(n);
	return image && image->type_id != -1 && image->bpp <= 8;
}

ImageData *CG::draw_texture(unsigned int n, bool to_pow2_flg, bool draw_8bpp) {
	if (m_foreign) return n < m_foreign->imageCount() ? m_foreign->draw(n, to_pow2_flg, draw_8bpp, palette) : nullptr;
	const CG_Image *image = get_image(n);
	if (!image) {
		return 0;
	}

	if (image->type_id == -1) {
		return 0; //The game doesn't draw them either.
	}

	if ((image->align_start + image->align_len) > m_nalign) {
		return 0;
	}
	
	// initialize texture and boundaries.
	// Indexed (draw_8bpp) output crops to the same bounds as the RGBA path so
	// the renderer can substitute it 1:1 (palette resolved in the shader).
	int x1 = image->bounds_x1;
	int y1 = image->bounds_y1;

	int width = image->bounds_x2 - x1+1;
	int height = image->bounds_y2 - y1+1;
	
	if (width == 0 || height == 0) {
		return 0;
	}
	
	if (to_pow2_flg) {
		width = to_pow2(width);
		height = to_pow2(height);
	}
	
	// check to see if we need a custom palette
	static unsigned int custom_palette[256];
	bool needsCustom = false;
	if (image->bpp == 32) {
		if (image->type_id == 3) {
			unsigned int color = *(unsigned int *)image->data;
			
			color &= 0xffffff;
			
			custom_palette[0] = 0;
			for (int i = 1; i < 256; ++i) {
				custom_palette[i] = (i << 24) | color;
			}
			
			needsCustom = true;
		} else if (image->type_id == 2 || image->type_id == 4) {
			memcpy(custom_palette, image->data, 1024);
			
			for (int i = 0; i < 256; ++i) {
				custom_palette[i] = (0xff << 24) | custom_palette[i];
			}
			// Index 0 is transparent: MBAA uploads type 2 images as 8-bit
			// indices with alpha = (index != 0) (Texture_ConvertFormat 0x402eb0,
			// P8 source 41). Type 4 takes alpha from its own plane instead.
			if (image->type_id == 2) custom_palette[0] = 0;
			needsCustom = true;
		}
	}
	
	unsigned char *pixels = new unsigned char[width*height*4];
	memset(pixels, 0, width*height*4);
	
	// run through all tile region data
	const CG_Alignment *align;

	bool is_8bpp;
	
	if (draw_8bpp && image->bpp <= 8) {
		is_8bpp = 1;
	} else {
		is_8bpp = 0;
	}
	
	align = &m_align[image->align_start];
	for (unsigned int i = 0; i < image->align_len; ++i, ++align) {
		copy_cells(image, align, pixels, x1, y1, width, height, needsCustom ? custom_palette : palette, is_8bpp);
	}
	
	// finalize in texture
	ImageData *texture;
	
	// NOTE: CG images use RGBA format (bgr=false), unlike PAT textures which use BGRA (bgr=true)
	if (!(texture = new ImageData{pixels, width, height, is_8bpp, false, image->bounds_x1, image->bounds_y1}))
	{
		delete texture;
		delete[] pixels;
		texture = nullptr;
	}
		
	return texture;
} 

void CG::build_image_table() {

	// Create new image table and initialize it.
	pages = new Page[page_count];
	memset(pages, 0, sizeof(Page) * page_count);

	// Go through and initialize all the cells.
	int maxCelln = 0;	
	for (unsigned int i = 0; i < 0x3000; ++i) {
		const CG_Image *image = get_image(i);
		
		if (!image) {
			continue;
		}

		if(image->type_id == -1)
			continue;
		
		if ((image->align_start + image->align_len) > m_nalign) {
			continue;
		}

		const CG_Alignment *align = &m_align[image->align_start];
		unsigned int address = ((char *)image->data) - m_data;
		
		if (image->bpp == 32) {
			if (image->type_id == 3) {
				address += 4; //Color key I think?
			} else if (image->type_id == 2 || image->type_id == 4) {
				address += 1024; //Indexed and alpha indexed?
			}
		}

		for (unsigned int j = 0; j < image->align_len; ++j, ++align) {
			if (align->copy_flag != 0) {
				continue;
			}

			
			int w = align->width / cu;
			int h = align->height / cu;
			int x = align->source_x / cu;
			int y = align->source_y / cu;
			int cell_n = (y * cpr) + x;
			Page *im = &pages[align->source_image];

			if(cell_n > maxCelln)
				maxCelln = cell_n;

			if (x + w >= cpr) {
				w = cpr - x;
			}
			if (y + h >= cpr) {
				h = cpr - y;
			}
			
			int mult = 1;
			if (image->type_id == 1) {
				mult = 4;
			}
			
			for (int a = 0; a < h; ++a) {
				ImageCell *cell = &im->cell[cell_n];
				for (int b = 0; b < w; ++b, ++cell) {
					cell->start = address;
					cell->width = align->width;
					cell->height = align->height;
					cell->offset = ( (b * cu) + (a * align->width * cu) ) * mult; //thxxx u4ick <3 
					cell->type_id = image->type_id;
					cell->bpp = image->bpp;
				}
				cell_n += cpr;
			}
			
			if (image->type_id == 4 || image->type_id == 5) {
				mult = 2;
			}
			
			address += align->width * align->height * mult;
		}
	}

}

int CG::getPalNumber()
{
	return palMax;
}

// Parses a .pal file in place: MBAACC "count + count*256 BGRA" or the
// UNI/MBTL "FFFF, split, 0, count" header + count*256 BGRA (130 palettes =
// 65 colours x 2 sets; see docs/HANTEI_UNI_MBTL.md). Alpha is made binary and
// index 0 transparent for display.
static bool ParsePaletteBuffer(char *data, unsigned int size, char *&out, int &count, int &offset);
static bool ParsePalette(const char *name, char *&out, int &count, int &offset)
{
	unsigned int size;
	char *data = nullptr;
	if (!ReadInMem(name, data, size)) { delete[] data; return false; }
	return ParsePaletteBuffer(data, size, out, count, offset);
}
// Takes ownership of `data` (new[]).
static bool ParsePaletteBuffer(char *data, unsigned int size, char *&out, int &count, int &offset)
{
	if (size < 4) {
		delete[] data;
		return false;
	}
	unsigned int *d = (unsigned int *)data;
	count = d[0];
	if((unsigned long long)count*0x400+4 > size)
	{
		if (size < 16) { delete[] data; return false; }
		count = d[3];
		if((unsigned long long)count*0x400+4*4 > size)
		{
			delete[] data;
			return false;
		}
		offset = 4;
	}
	else
		offset = 1;

	unsigned int *paletteIterator = d + offset;
	for(int i = 0; i < count; i++)
	{
		unsigned int *p = paletteIterator;
		for (int j = 0; j < 256; ++j) {
			unsigned int v = *p;
			unsigned int alpha = v>>24;
			alpha = (alpha != 0) ? 255 : 0;
			*p = (v&0xffffff) | (alpha<<24);
			++p;
		}
		paletteIterator[0] = 0;
		paletteIterator += 0x100;
	}
	out = data;
	return true;
}

const unsigned int *CG::paletteAt(int number, int pups) const
{
	if (number < 0) return nullptr;
	if (m_foreign) return number < m_foreign->paletteCount() ? m_foreign->palette(number) : nullptr;
	if (pups > 0 && pups < kPupsBanks && pupsData[pups] && number < pupsMax[pups]) return (unsigned int *)pupsData[pups] + pupsOffset[pups] + number * 0x100;
	if (paletteData && number < palMax) return (unsigned int *)paletteData + paletteOffset + number * 0x100;
	return number == 0 && m_loaded ? m_basePalette : nullptr;
}

bool CG::loadPalette(const char *name) {
	touch();
	if (paletteData) {
		palette = origPalette;
		delete[] paletteData;
		paletteData = nullptr;
		palMax = 0;
	}

	if (!ParsePalette(name, paletteData, palMax, paletteOffset)) {
		paletteData = nullptr;
		palMax = 0;
		return false;
	}
	curPalIndex = 0;
	curPups = 0;
	appliedBank = 0;
	palette = (unsigned int *)paletteData + paletteOffset;
	palPaths[0] = name;
	return true;
}

bool CG::setPaletteBytes(int bank, const void *bytes, unsigned size)
{
	if (bank < 0 || bank >= kPupsBanks) return false;
	char *copy = new char[size ? size : 1]; memcpy(copy, bytes, size);
	char *data = nullptr; int count = 0, offset = 0;
	if (!ParsePaletteBuffer(copy, size, data, count, offset)) return false;
	if (bank == 0) {
		delete[] paletteData; paletteData = data; palMax = count; paletteOffset = offset;
	} else {
		delete[] pupsData[bank]; pupsData[bank] = data; pupsMax[bank] = count; pupsOffset[bank] = offset;
	}
	if (curPalIndex >= std::max(1, palMax)) curPalIndex = 0;
	appliedBank = -1;       // force applyPalette to re-point
	applyPalette();
	if (appliedBank < 0) appliedBank = 0;
	touch();
	return true;
}

void CG::freePupsBanks()
{
	touch();
	for (int i = 1; i < kPupsBanks; ++i) {
		delete[] pupsData[i];
		pupsData[i] = nullptr;
		pupsMax[i] = 0;
		palPaths[i].clear();
	}
}

bool CG::loadPupsPalettes(const std::string &stem)
{
	freePupsBanks();
	bool ok = std::filesystem::exists(stem + ".pal") && loadPalette((stem + ".pal").c_str());
	for (int i = 1; i < kPupsBanks; ++i) {
		const std::string p = stem + "_p" + std::to_string(i) + ".pal";
		if (!std::filesystem::exists(p)) continue;
		char *data = nullptr; int count = 0, offset = 0;
		if (ParsePalette(p.c_str(), data, count, offset)) {
			pupsData[i] = data; pupsMax[i] = count; pupsOffset[i] = offset; palPaths[i] = p;
		}
	}
	touch();   // bank contents changed
	return ok;
}

int CG::pupsBankCount() const
{
	int n = paletteData ? 1 : 0;
	for (int i = 1; i < kPupsBanks; ++i) if (pupsData[i]) n = i + 1;
	return n;
}

void CG::applyPalette()
{
	if (curPups > 0 && curPups < kPupsBanks && pupsData[curPups] && curPalIndex < pupsMax[curPups]) {
		palette = (unsigned int *)pupsData[curPups] + pupsOffset[curPups] + curPalIndex * 0x100;
		appliedBank = curPups;
	} else if (paletteData && curPalIndex < palMax) {
		palette = (unsigned int *)paletteData + paletteOffset + curPalIndex * 0x100;
		appliedBank = 0;
	}
}

bool CG::setPupsBank(int bank)
{
	if (bank < 0 || bank >= kPupsBanks) bank = 0;
	if (bank == curPups) return false;
	const unsigned int *before = palette;
	curPups = bank;
	applyPalette();
	return palette != before;
}

bool CG::changePaletteNumber(int number)
{
	touch();
	if (m_foreign) {
		if (number < 0 || number >= m_foreign->paletteCount()) return false;
		curPalIndex = number; palette = (unsigned int *)m_foreign->palette(number); origPalette = palette; return true;
	}
	if(paletteData && number < palMax && number >= 0)
	{
		curPalIndex = number;
		applyPalette();
		return true;
	}
	return false;
}

bool CG::load(const char *name) {
	if (m_loaded) {
		free();
	}

	if (paletteData) {
		delete[] paletteData;
		paletteData = nullptr;
		palMax = 0;
	}
	
	char *data;
	unsigned int size;
	
	if (!ReadInMem(name, data, size)) {
		return 0;
	}
	return loadOwned(data, size);
}

bool CG::loadFromMemory(const void *src, unsigned int size) {
	if (m_loaded) {
		free();
	}
	if (paletteData) {
		delete[] paletteData;
		paletteData = nullptr;
		palMax = 0;
	}
	char *data = new char[size ? size : 1];
	memcpy(data, src, size);
	return loadOwned(data, size);
}

bool CG::replaceBank(const void *src, unsigned size) {
	if (m_foreign || !m_loaded) return false;
	char *data = new char[size ? size : 1];
	memcpy(data, src, size);
	const int keepMax = palMax, keepIdx = curPalIndex, keepPups = curPups;
	char *keepPal = paletteData; const int keepOff = paletteOffset;
	if (m_data) delete[] m_data;
	if (pages) delete[] pages;
	pages = nullptr; m_data = nullptr; m_loaded = 0;
	if (!loadOwned(data, size)) { return false; }   // frees data on failure; the caller keeps its model
	if (keepPal) { paletteData = keepPal; paletteOffset = keepOff; palMax = keepMax; curPalIndex = keepIdx; curPups = keepPups; origPalette = m_basePalette; applyPalette(); }
	touch();
	return true;
}

// Takes ownership of `data` (new[]). Shared by load() and loadFromMemory().
// "BMP Cutter2" (MBAC GAKIHA.DAT) has the same table layout.
bool CG::loadOwned(char *data, unsigned int size) {
	touch();
	// verify size and header
	if (size < 0x4f30 || (memcmp(data, "BMP Cutter3", 11) && memcmp(data, "BMP Cutter2", 11))) {
		delete[] data;
		
		return 0;
	}
	
	// palette data.
	unsigned int *d = (unsigned int *)(data + 0x10);
	d += 1; // has palette data?
	// The bank palette is normalised (binary alpha, entry 0 transparent) in a COPY: the bank bytes stay exactly as shipped, so a bank that is
	// written back (SyncPartsToContainer) is byte-identical to the one that was loaded.
	memcpy(m_basePalette, d, sizeof(m_basePalette));
	palette = m_basePalette;
	origPalette = m_basePalette;
	palMax = 1;
	d += 0x800;	// There are 8 dupe palettes. The game doesn't use them. - always included.

	unsigned int *p = palette;
	for (int j = 0; j < 256; ++j) {
		unsigned int v = *p;
		unsigned int alpha = v>>24;
		
		alpha = (alpha != 0) ? 255 : 0;
		
		*p = (v&0xffffff) | (alpha<<24);
		++p;
	}
	palette[0] = 0;
	
	// parse header
	page_count = (*d) + 1;
	// Cell size of the page grid (header +16). 16 in most banks, 32 in many
	// stages (both handled on a 16-px grid), 8 in MBAACC bg52 (car/airport):
	// the game indexes cells with it (CG_BmpCutter_ParseSpriteData 0x402970).
	{
		unsigned int cs = d[4];
		cu = (cs >= 1 && cs < 16) ? (int)cs : 16;
		cpr = 256 / cu;
	}
	m_nalign = *(d+2);

	unsigned int *indices = d + 12;
	unsigned int image_count = d[3];
	
	//was 2999
	if (image_count >= 3000) {
		delete[] data;
		
		return 0;
	}
	
	// alignment data
	// store everything for lookup later
	m_align = (CG_Alignment *)(data + indices[3000]);
	
	
	m_data = data;
	m_data_size = size;
	
	m_indices = indices;
	
	m_nimages = image_count;
	
	// but wait, there's more!
	// because of the compression added to AACC, we need to go create
	// an image table for this crap.
	build_image_table();
	
	// we're done, so finish up
	
	m_loaded = 1;
	
	return 1;
}

bool CG::loadForeign(std::shared_ptr<CgForeignBank> bank) {
	if (m_loaded) free();
	if (!bank) return false;
	touch();
	m_foreign = std::move(bank);
	palMax = m_foreign->paletteCount(); curPalIndex = 0;
	palette = (unsigned int *)m_foreign->palette(0); origPalette = palette;
	m_nimages = m_foreign->imageCount();
	m_loaded = true;
	return true;
}

void CG::free() {
	touch();
	m_foreign.reset();
	freePupsBanks();
	curPups = 0;
	curPalIndex = 0;
	appliedBank = 0;
	if (paletteData) {
		delete[] paletteData;
	}
	if (m_data) {
		delete[] m_data;
	}
	palMax = 0;
	paletteData = nullptr;
	m_data = nullptr;
	m_data_size = 0;
	
	if (pages) {
		delete[] pages;
	}
	pages = nullptr;
	page_count = 0;
	
	m_indices = nullptr;
	
	m_nimages = 0;
	
	m_align = nullptr;
	m_nalign = 0;
	
	m_loaded = 0;
}

unsigned int CG::getColorFromPal(int palIndex)
{
	// Return color from current palette at given index
	if (!palette || palIndex < 0 || palIndex >= 256) {
		return 0xFF000000; // Return opaque black if invalid
	}
	return palette[palIndex];
}

CG::CG() {
	touch();
	m_data = 0;
	m_data_size = 0;
	
	m_indices = 0;
	
	m_nimages = 0;
	
	pages = 0;
	page_count = 0;
	
	m_align = 0;
	m_nalign = 0;
	
	m_loaded = 0;
}

CG::~CG() {
	free();
}

