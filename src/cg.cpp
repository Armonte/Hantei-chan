// .CG loader
//
// .CG contains information about sprite mappings from the ENC and PVR tiles.

#include "cg.h"
#include "misc.h"

#include <cstdlib>
#include <cstring>

#include <iostream>

const CG_Image *CG::get_image(unsigned int n) {
	if (n >= m_nimages) {
		return 0;
	}


	unsigned int index = m_indices[n];
	// Check for invalid index (-1 stored as 0xFFFFFFFF in unsigned)
	if (index == 0xFFFFFFFF || index == 0) {
		return 0;
	}

	// IMPORTANT: Indices are ALWAYS relative to BMP Cutter3!
	// Even in HA4-extracted files with BMP at non-zero offset.
	// Proof: Python analysis shows indices[0]=0x4F30 points to valid CG_Image at BMP+0x4F30
	unsigned int absolute_index = index + m_bmp_offset;

	// Debug: check first few images to see if indices look reasonable
	static int debug_count = 0;
	if (debug_count < 3) {
		std::cout << "DEBUG get_image(" << n << "): index=" << index
		          << ", bmp_offset=" << m_bmp_offset << ", using absolute=" << absolute_index << "\n";
		debug_count++;
	}

	if (absolute_index + sizeof(CG_Image) > m_data_size) {
		// Silent fail - CG files often have invalid indices for unused slots
		// Only log first few errors to avoid spam
		static int error_count = 0;
		if (error_count < 5) {
			std::cerr << "Warning: Image " << n << " has invalid index " << absolute_index 
			          << " (exceeds file size " << m_data_size << ")\n";
			if (error_count == 4) {
				std::cerr << "  (Suppressing further index warnings...)\n";
			}
			error_count++;
		}
		return 0;
	}

	return (const CG_Image *)(m_data + absolute_index);
}

const char *CG::get_filename(unsigned int n) {
	if (!m_loaded) {
		return 0;
	}
	
	const CG_Image *image = get_image(n);
	if (!image) {
		return 0;
	}
	
	return image->filename;
}

int CG::get_image_count() {
	return m_nimages;
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
	int w = align->width / 0x10;
	int h = align->height / 0x10;
	int x = align->source_x / 0x10;
	int y = align->source_y / 0x10;
	int cell_n = (y * 0x10) + x;
	Page *im = &pages[align->source_image];
	
	for (int a = 0; a < h; ++a) {
		for (int b = 0 ; b < w; ++b) {
			ImageCell *cell = &im->cell[cell_n + b];
			
			if (cell->start == 0) {
				continue;
			}
			
			unsigned char *dest = pixels;
			unsigned int offset;
			
			offset = (align->y + (a * 0x10) - y1) * width;
			offset += align->x + (b * 0x10) - x1;
			
			if (is_8bpp) {
				// 8bpp -> 8bpp
				unsigned char *src = ((unsigned char *)m_data) + cell->start + cell->offset;
				int cellw = cell->width;

				// Debug first few pixel reads
				static int copy_debug = 0;
				if (copy_debug < 2 && m_bmp_offset > 0) {
					size_t src_offset = src - ((unsigned char*)m_data);
					std::cout << "DEBUG copy_cells 8bpp: cell->start=" << cell->start
					          << ", cell->offset=" << cell->offset
					          << ", src offset=" << src_offset
					          << ", cellw=" << cellw
					          << ", first 16 bytes: ";
					for (int i = 0; i < 16; i++) {
						printf("%02X ", src[i]);
					}
					std::cout << "\n";
					copy_debug++;
				}

				dest += offset;

				for (int c = 0; c < 0x10; ++c) {
					for (int d = 0; d < 0x10; ++d) {
						dest[d] = src[d];
					}

					src += cellw;
					dest += width;
				}
			} else if (image->type_id == 4) {
				// two pass: first 8bit palettized, second 8bit alpha
				unsigned int *ldest = (unsigned int *)dest;
				unsigned char *src = ((unsigned char *)m_data) + cell->start + cell->offset;
				int cellw = cell->width;
				
				ldest += offset;
				
				for (int c = 0; c < 0x10; ++c) {
					for (int d = 0; d < 0x10; ++d) {
						ldest[d] = palette[src[d]] & 0xffffff;
					}
					
					src += cellw;
					ldest += width;
				}
				
				ldest = (unsigned int *)dest;
				ldest += offset;
				
				src = ((unsigned char *)m_data) + cell->start + cell->offset;
				src += align->width * align->height;

				for (int c = 0; c < 0x10; ++c) {
					for (int d = 0; d < 0x10; ++d) {
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
				
				for (int c = 0; c < 0x10; ++c) {
					for (int d = 0; d < 0x10; ++d) {
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

				// Debug first few pixel reads in 8bpp->32bpp path
				static int copy_debug_32 = 0;
				if (copy_debug_32 < 2 && m_bmp_offset > 0) {
					size_t src_offset = src - ((unsigned char*)m_data);
					std::cout << "DEBUG copy_cells 8bpp->32bpp: cell->start=" << cell->start
					          << ", cell->offset=" << cell->offset
					          << ", src offset=" << src_offset
					          << ", cellw=" << cellw
					          << ", first 16 palette indices: ";
					for (int i = 0; i < 16; i++) {
						printf("%02X ", src[i]);
					}
					std::cout << "\n  Palette colors for first 4 indices: ";
					for (int i = 0; i < 4; i++) {
						printf("pal[%02X]=0x%08X ", src[i], palette[src[i]]);
					}
					std::cout << "\n";
					copy_debug_32++;
				}

				ldest += offset;


				for (int c = 0; c < 0x10; ++c) {
					for (int d = 0; d < 0x10; ++d) {
						ldest[d] = palette[src[d]];
					}

					src += cellw;
					ldest += width;
				}
			}
		}
		
		cell_n += 0x10;
	}
}
			

ImageData *CG::draw_texture(unsigned int n, bool to_pow2_flg, bool draw_8bpp) {
	const CG_Image *image = get_image(n);
	if (!image) {
		std::cerr << "draw_texture(" << n << "): get_image returned NULL\n";
		return 0;
	}

	if (image->type_id == -1) {
		return 0; //The game doesn't draw them either.
	}

	// Debug: log first few texture draws to help diagnose issues
	static int draw_count = 0;
	if (draw_count < 3 && m_bmp_offset > 0) {
		std::cout << "DEBUG: draw_texture(" << n << ") with m_bmp_offset=" << m_bmp_offset
		          << ", type=" << image->type_id << ", bpp=" << image->bpp
		          << ", align_start=" << image->align_start << ", align_len=" << image->align_len
		          << ", m_nalign=" << m_nalign
		          << ", bounds=(" << image->bounds_x1 << "," << image->bounds_y1 << ","
		          << image->bounds_x2 << "," << image->bounds_y2 << ")\n";
		draw_count++;
	}

	if ((image->align_start + image->align_len) > m_nalign) {
		if (draw_count < 3) {
			std::cerr << "draw_texture(" << n << "): alignment out of range ("
			          << image->align_start << " + " << image->align_len << " > " << m_nalign << ")\n";
		}
		return 0;
	}
	
	// initialize texture and boundaries
	int x1 = 0;
	int y1 = 0;
	
	if (!draw_8bpp) {
		x1 = image->bounds_x1;
		y1 = image->bounds_y1;
	}
	
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
	
	if (!(texture = new ImageData{pixels, width, height, is_8bpp, image->bounds_x1, image->bounds_y1}))
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
	// IMPORTANT: Loop only through actual images, not the full 0x3000 index table!
	// The index table has 3000 entries for images + 1 for alignment offset,
	// but m_nimages tells us how many are actually valid.
	for (unsigned int i = 0; i < m_nimages; ++i) {
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
		// IMPORTANT: image->data points to pixel data which is at ABSOLUTE offset in blob
		// (pixel data is stored BEFORE BMP Cutter3 in HA4 extracted files)
		unsigned int address = ((char *)image->data) - m_data;

		// Validate that image->data points to valid memory within the blob
		if (address >= m_data_size) {
			// Silently skip images with invalid pixel data pointers
			static int invalid_data_count = 0;
			if (invalid_data_count < 3) {
				std::cerr << "Warning: Image " << i << " has invalid data pointer (offset " 
				          << address << " >= file size " << m_data_size << "), skipping\n";
				invalid_data_count++;
			}
			continue;
		}

		// Debug first few images to see pixel data addresses
		static int addr_debug = 0;
		if (addr_debug < 3 && m_bmp_offset > 0) {
			std::cout << "DEBUG build_image_table image " << i << ": image->data offset = "
			          << (void*)((char*)image->data - m_data) << " (address=" << address << ")\n";
			addr_debug++;
		}

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

			// Bounds check for source_image to prevent crashes from invalid alignment data
			if (align->source_image >= page_count) {
				std::cerr << "WARNING: Invalid source_image " << align->source_image
				          << " >= page_count " << page_count << " at image " << i << "\n";
				continue;
			}

			int w = align->width / 0x10;
			int h = align->height / 0x10;
			int x = align->source_x / 0x10;
			int y = align->source_y / 0x10;
			int cell_n = (y * 0x10) + x;
			Page *im = &pages[align->source_image];

			if(cell_n > maxCelln)
				maxCelln = cell_n;

			if (x + w >= 0x10) {
				w = 0x10 - x;
			}
			if (y + h >= 0x10) {
				h = 0x10 - y;
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
					cell->offset = ( (b * 0x10) + (a * align->width * 0x10) ) * mult; //thxxx u4ick <3 
					cell->type_id = image->type_id;
					cell->bpp = image->bpp;
				}
				cell_n += 0x10;
			}
			
			if (image->type_id == 4) {
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

bool CG::loadPalette(const char *name) {
	if (paletteData) {
		palette = origPalette;
		delete[] paletteData;
		palMax = 0;
	}

	unsigned int size;
	char *data;
	if (!ReadInMem(name, paletteData, size)) {
		return false;
	}

	unsigned int *d = (unsigned int *)paletteData;
	palMax = d[0];

	//Quick filesize check to make sure it's valid.
	if(palMax*0x400+4 > size)
	{
		palMax = d[3];
		if(palMax*0x400+4*4 > size)
		{
			delete[] paletteData;
			paletteData = nullptr;
			palMax = 0;
			return false;
		}
		paletteOffset = 4;
	}
	else
		paletteOffset = 1;

	palette = d+paletteOffset;

	unsigned int *paletteIterator = palette;
	for(int i = 0; i < palMax; i++)
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
	return true;
}

bool CG::changePaletteNumber(int number)
{
	if(paletteData && number < palMax && number >= 0)
	{
		unsigned int *d = (unsigned int *)paletteData;
		palette = d + paletteOffset + number * 0x100;
		return true;
	}
	return false;
}

bool CG::load(const char *name) {
	std::cout << "CG::load() started for: " << name << "\n";

	if (m_loaded) {
		std::cout << "  Freeing existing CG data\n";
		free();
	}

	if (paletteData) {
		delete[] paletteData;
		paletteData = nullptr;
		palMax = 0;
	}

	char *data;
	unsigned int size;

	std::cout << "  Reading file into memory...\n";
	if (!ReadInMem(name, data, size)) {
		std::cerr << "  ERROR: ReadInMem failed\n";
		return 0;
	}
	std::cout << "  File size: " << size << " bytes\n";

	// verify size and header
	// NOTE: For HA4 extracted CG, "BMP Cutter3" might not be at offset 0!
	// Search for it within the first 2MB
	const char *bmp_cutter_offset = nullptr;
	size_t search_size = std::min((size_t)size, (size_t)2000000);
	for (size_t i = 0; i < search_size - 11; i++) {
		if (memcmp(data + i, "BMP Cutter3", 11) == 0) {
			bmp_cutter_offset = data + i;
			if (i > 0) {
				std::cout << "  Found BMP Cutter3 at offset +" << i << " (not at file start)\n";
			}
			break;
		}
	}

	if (!bmp_cutter_offset || (bmp_cutter_offset - data) + 0x4f30 > (long)size) {
		std::cerr << "  ERROR: Invalid CG header or size too small\n";
		delete[] data;
		return 0;
	}
	std::cout << "  Header validation passed\n";

	// Store BMP offset for later use in get_image() and build_image_table()
	size_t bmp_offset = bmp_cutter_offset - data;
	m_bmp_offset = bmp_offset;
	if (bmp_offset > 0) {
		std::cout << "  BMP Cutter3 is at offset +" << bmp_offset << " (indices will be adjusted)\n";
		// For HA4 extracted CG: the indices table has offsets relative to BMP Cutter3,
		// but m_data points to blob start. We need to add m_bmp_offset when using indices.
	}

	// palette data - relative to BMP Cutter3, not file start
	std::cout << "  Processing palette data...\n";
	unsigned int *d = (unsigned int *)(bmp_cutter_offset + 0x10);
	d += 1; // has palette data?
	palette = d;
	origPalette = d;
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
	std::cout << "  Parsing CG header...\n";
	page_count = (*d) + 1;
	m_nalign = *(d+2);

	unsigned int *indices = d + 12;
	unsigned int image_count = d[3];

	std::cout << "    page_count: " << page_count << "\n";
	std::cout << "    m_nalign: " << m_nalign << "\n";
	std::cout << "    image_count: " << image_count << "\n";

	//was 2999
	if (image_count >= 3000) {
		std::cerr << "  ERROR: image_count >= 3000\n";
		delete[] data;

		return 0;
	}

	// alignment data
	// store everything for lookup later
	std::cout << "  Setting up alignment data (offset indices[3000] = " << indices[3000] << ")...\n";

	// IMPORTANT: indices[3000] is DIFFERENT from image indices!
	// - Image indices [0-2999] are relative to BMP Cutter3
	// - Alignment offset [3000] is ABSOLUTE (relative to blob start)
	// This is because alignment data is stored at the END of the file
	size_t alignment_offset = indices[3000];  // Use as-is, it's already absolute!
	std::cout << "  Alignment offset (absolute): " << alignment_offset << "\n";

	// Bounds check for alignment data offset
	if (alignment_offset >= size) {
		std::cerr << "  ERROR: Alignment data offset " << alignment_offset << " exceeds file size " << size << "\n";
		std::cerr << "  This CG file appears to be corrupted or truncated\n";
		delete[] data;
		return 0;
	}

	// Also check that there's enough space for the alignment array
	size_t alignment_array_size = m_nalign * sizeof(CG_Alignment);
	if (alignment_offset + alignment_array_size > size) {
		std::cerr << "  ERROR: Alignment array (offset " << alignment_offset << " + size " << alignment_array_size
		          << ") exceeds file size " << size << "\n";
		std::cerr << "  m_nalign=" << m_nalign << ", sizeof(CG_Alignment)=" << sizeof(CG_Alignment) << "\n";
		delete[] data;
		return 0;
	}

	m_align = (CG_Alignment *)(data + alignment_offset);


	m_data = data;
	m_data_size = size;

	m_indices = indices;

	m_nimages = image_count;

	// but wait, there's more!
	// because of the compression added to AACC, we need to go create
	// an image table for this crap.
	std::cout << "  Building image table...\n";
	build_image_table();
	std::cout << "  Image table built successfully\n";

	// we're done, so finish up

	m_loaded = 1;
	std::cout << "CG::load() completed successfully\n";

	return 1;
}

void CG::free() {
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
	m_bmp_offset = 0;  // IMPORTANT: Reset BMP offset when freeing!

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

CG::CG() {
	m_data = 0;
	m_data_size = 0;
	m_bmp_offset = 0;

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

