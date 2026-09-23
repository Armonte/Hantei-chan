#ifndef CG_H_GUARD
#define CG_H_GUARD
#include <string>

struct ImageData
{
	unsigned char *pixels = nullptr;
	int		width;
	int		height;
	bool	is8bpp;
	bool	bgr = false;  // True if pixel data is in BGR/BGRA format
	int		offsetX;
	int		offsetY;

	~ImageData()
	{
		delete[] pixels;
	}
};

struct CG_Alignment {
	int		x;
	int		y;
	int		width;
	int		height;
	short	source_x;
	short	source_y;
	short	source_image;
	short	copy_flag;
};

struct CG_Image {
	char			filename[32];
	int				type_id;	// i think this is render mode
	unsigned int	width;
	unsigned int	height;
	unsigned int	bpp;
	int				bounds_x1;
	int				bounds_y1;
	int				bounds_x2;
	int				bounds_y2;
	unsigned int	align_start;
	unsigned int	align_len;
	unsigned char	data[1]; 	// for indexing.
};

class CG {
protected:
	unsigned int	*origPalette;
	unsigned int	*palette;
	char			*paletteData = nullptr;
	int				palMax = 0;
	int				paletteOffset = 0;

	// PUPS palette files (issue #76): bank 0 is <cg>.pal (paletteData above),
	// bank n is <cg>_pn.pal, n = 1..7 (CharaPalette_LoadPalAndPupsVariants,
	// MBTL.exe 0x5934B0). A pattern's PUPS value selects the bank; the
	// palette number (colour) stays the same.
	static constexpr int kPupsBanks = 8;
	char			*pupsData[kPupsBanks] = {};
	int				pupsMax[kPupsBanks] = {};
	int				pupsOffset[kPupsBanks] = {};
	int				curPalIndex = 0;
	int				curPups = 0;
	void			freePupsBanks();
	void			applyPalette();

	char					*m_data;
	unsigned int			m_data_size;

	const unsigned int		*m_indices;

	unsigned int			m_nimages;

	const CG_Alignment		*m_align;
	unsigned int			m_nalign;

	struct ImageCell {
		unsigned int		start;
		unsigned int		width;
		unsigned int		height;
		unsigned int		offset;
		unsigned short		type_id;
		unsigned short		bpp;
	};

	//Hantei4 calls these "pages".
	struct Page {
		ImageCell	cell[256];
	};

	Page			*pages;
	unsigned int	page_count;

	void			copy_cells(
					const CG_Image *image,
					const CG_Alignment *align,
					unsigned char *pixels,
					unsigned int x1,
					unsigned int y1,
					unsigned int width,
					unsigned int height,
					unsigned int *palette,
					bool is_8bpp);

	void			build_image_table();

	const CG_Image	*get_image(unsigned int n);
	bool			loadOwned(char *data, unsigned int size);
public:
	bool m_loaded;
	bool load(const char *name);
	// Load a CG image bank from memory (copied), e.g. the CG blob embedded in an MBAC .DAT.
	bool loadFromMemory(const void *data, unsigned int size);
	bool loadPalette(const char *name);
	// Loads <stem>.pal as bank 0 and <stem>_p1.pal .. _p7.pal as banks 1..7.
	bool loadPupsPalettes(const std::string &stem);
	// Selects the PUPS bank. A missing bank falls back to bank 0 (the game
	// would show its default grey ramp). Returns true if the palette changed.
	bool setPupsBank(int bank);
	int pupsBank() const { return curPups; }
	bool hasPupsBank(int bank) const { return bank == 0 ? paletteData != nullptr : (bank > 0 && bank < kPupsBanks && pupsData[bank]); }
	int pupsBankCount() const;
	bool changePaletteNumber(int number);
	int getPalNumber();
	unsigned int getColorFromPal(int palIndex);

	void free();

	const char *get_filename(unsigned int n);

	ImageData* draw_texture(unsigned int n, bool to_pow2, bool draw_8bpp = 0);
	//True if image n is palette-indexed (8bpp) and can use the shader palette path.
	bool image_is_8bpp(unsigned int n);
	const unsigned int* getPalettePtr() const { return palette; }

	int	get_image_count();

	CG();
	~CG();
};

#endif /* CG_H_GUARD */
