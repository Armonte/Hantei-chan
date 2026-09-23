#ifndef CG_H_GUARD
#define CG_H_GUARD

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
	unsigned long long m_generation = 0;
	void			touch();
public:
	bool m_loaded;
	bool load(const char *name);
	// Load a CG image bank from memory (copied), e.g. the CG blob embedded in an MBAC .DAT.
	bool loadFromMemory(const void *data, unsigned int size);
	bool loadPalette(const char *name);
	bool changePaletteNumber(int number);
	int getPalNumber();
	unsigned int getColorFromPal(int palIndex);

	void free();

	const char *get_filename(unsigned int n);

	ImageData* draw_texture(unsigned int n, bool to_pow2, bool draw_8bpp = 0);
	//True if image n is palette-indexed (8bpp) and can use the shader palette path.
	bool image_is_8bpp(unsigned int n);
	const unsigned int* getPalettePtr() const { return palette; }
	// Process-unique stamp, renewed by every load / palette change / free.
	// Render's sprite texture cache keys on (this, generation, image).
	unsigned long long generation() const { return m_generation; }

	int	get_image_count();

	CG();
	~CG();
};

#endif /* CG_H_GUARD */
