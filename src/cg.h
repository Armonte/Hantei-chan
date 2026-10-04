#ifndef CG_H_GUARD
#define CG_H_GUARD
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>

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

struct CgCellRect { int x, y, w, h; };

// A sprite bank that is not a "BMP Cutter" bank (Melty Blood 2002 strips, Party Breakers groups, QoH sprite tiles ...): the format module decodes its own
// images and CG forwards every query to it, so the whole editor (renderer, sprite list, import/export) works on it unchanged. The module keeps the
// stored bytes authoritative: an unedited bank serializes back byte-identical.
struct CgForeignBank {
	virtual ~CgForeignBank() {}
	virtual unsigned imageCount() const = 0;
	virtual const char *imageName(unsigned n) const = 0;
	virtual bool imageInfo(unsigned n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2) const = 0;   // false = absent image
	virtual bool imageCells(unsigned n, std::vector<CgCellRect> &out) const = 0;
	// RGBA (indexed == false) or 1 byte per pixel palette indices (indexed == true, only for 8-bit images) of the image's bounds rectangle.
	virtual ImageData *draw(unsigned n, bool toPow2, bool indexed, const unsigned *palette) const = 0;
	virtual int paletteCount() const = 0;
	virtual const unsigned *palette(int i) const = 0;               // 256 entries 0xAABBGGRR, entry 0 transparent
	virtual bool replaceImage(unsigned n, const unsigned char *rgba, int w, int h, int paletteIndex, std::string *err) = 0;
	virtual bool dirty() const = 0;
	virtual void clearDirty() = 0;
	virtual void serialize(std::vector<uint8_t> &out) const = 0;    // the stored bank bytes
};

class CG {
protected:
	std::shared_ptr<CgForeignBank> m_foreign;
	unsigned int	m_basePalette[256] = {};   // normalised copy of the bank palette (the bank bytes are never modified)
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
	int				appliedBank = 0;   // bank whose data `palette` points into
	std::string		palPaths[kPupsBanks];
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
		ImageCell	cell[1024];   // (256 / cu)^2 cells used
	};
	int cu = 16;    // cell unit in px
	int cpr = 16;   // cells per page row

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
	// Replace the bank bytes (any size) but keep the loaded .pal / PUPS palettes and the selected palette number: the CG manager's structural edits.
	bool replaceBank(const void *data, unsigned int size);
	// Foreign (non Cutter) bank: see CgForeignBank. Replaces whatever is loaded.
	bool loadForeign(std::shared_ptr<CgForeignBank> bank);
	CgForeignBank *foreign() const { return m_foreign.get(); }
	bool loadPalette(const char *name);
	// Re-parse palette bytes (a .pal file image) as bank `bank` (0 = <cg>.pal, 1..7 = PUPS): live palette editing in the CG manager. Keeps the selection.
	bool setPaletteBytes(int bank, const void *bytes, unsigned size);
	// Source file of palette bank n as loaded (empty = none).
	const std::string &palettePath(int bank) const { static const std::string none; return (bank >= 0 && bank < kPupsBanks) ? palPaths[bank] : none; }
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
	// 256 entries (0xAABBGGRR, entry 0 transparent) of palette `number` in PUPS bank `pups` (0 = the bank / .pal), or null when it does not exist.
	// Does not change the current palette (the CG manager previews with it).
	const unsigned int *paletteAt(int number, int pups = 0) const;
	unsigned int getColorFromPal(int palIndex);

	void free();

	const char *get_filename(unsigned int n);

	ImageData* draw_texture(unsigned int n, bool to_pow2, bool draw_8bpp = 0);
	//True if image n is palette-indexed (8bpp) and can use the shader palette path.
	bool image_is_8bpp(unsigned int n);
	const unsigned int* getPalettePtr() const { return palette; }
	// Process-unique stamp, renewed by every load / palette change / free.
	// Render's sprite texture cache keys on (this, generation, image).
	// The PUPS bank in effect is folded into the low bits, so switching
	// banks per layer changes the key (no stale baked texture) while
	// switching back hits the entries made earlier (no cache thrash).
	unsigned long long generation() const { return (m_generation << 3) | (unsigned)(appliedBank & 7); }

	int	get_image_count();
	// Replace the pixels of image n with RGBA (straight alpha) of exactly the image's bounds size. Supports storage types 1 (32-bit),
	// 2 (256-colour palette, binary alpha) and 4 (palette + alpha plane); palettes are quantized to 255 colours when needed.
	// Only blocks the image owns are written (blocks that copy another image's cells are left alone). Returns false with *err.
	// force=false: pixels identical to what the bank already renders for image n leave the bank untouched (no palette re-ordering, byte-identical);
	// force=true always re-encodes (used by han2tool cgrt to prove the encoder itself).
	bool replace_image_rgba(unsigned int n, const unsigned char *rgba, int w, int h, std::string *err = nullptr, bool force = false);
	// The whole bank as stored (for saving it back into a .DAT).
	const char *bank_data() const { return m_data; }
	unsigned int bank_size() const { return m_data_size; }
	// Restore a previous copy of the bank bytes (same size): used by the editor's sprite-import undo.
	bool restore_bank(const char *bytes, unsigned int size) { if (!m_data || size != m_data_size) return false; memcpy(m_data, bytes, size); touch(); return true; }
	// Raw header fields of image n (false if absent / unused). bpp is the
	// stored depth (8 = palette-indexed); bounds are canvas coordinates.
	bool image_info(unsigned int n, int &bpp, int &typeId, int &x1, int &y1, int &x2, int &y2);
	// The image's alignment cells (canvas rects), in table order.
	using CellRect = CgCellRect;
	bool image_cells(unsigned int n, std::vector<CellRect> &out);

	CG();
	~CG();
};

#endif /* CG_H_GUARD */
