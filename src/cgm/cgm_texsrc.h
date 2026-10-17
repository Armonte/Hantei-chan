#ifndef CGM_TEXSRC_H_GUARD
#define CGM_TEXSRC_H_GUARD
// One interface over the two kinds of character images Hantei-chan edits: CG bank images (cgm::BankIO, any bank format) and
// .pat part textures (Parts::gfxMeta, pattex codecs). The CG manager's unified browser, cgmtex and the tests run the same
// operations on both: browse, view (decode), replace, PNG export, PNG import, references, save into the game's container /
// the UNI2-IM / MBTL-IM mod folder. No GL / ImGui in here.
//
// Texture formats written per game (.pat, pat_texconv.h):
//   MBAACC                    PGTX 32-bit BGRA (the only record MBAA.exe reads)
//   UNIB / UNI2 / MBTL        PGT2 DXT1 (1), DXT3 (3, UNIB), DXT5 (5), A8R8G8B8 (21), X8R8G8B8 (22); a replace keeps the texture's own kind
//   ActCadenza                no .pat (HA4 parts live in the DAT)
//   RBO / GOF2                part sheets are CG-style banks (han2), browsed through the CG side
#include "cgm_bank.h"
#include "cgm_io.h"
#include <string>
#include <vector>

class Parts;
class FrameData;

namespace cgm {

enum class TexKind { CgImage = 0, PatTexture = 1 };

struct TexEntry {
	bool present = false;   // false: empty slot (nothing to view / export)
	int w = 0, h = 0;
	std::string name, format;
};

class TexSource {
public:
	virtual ~TexSource() {}
	virtual TexKind kind() const = 0;
	virtual int count() const = 0;
	virtual bool entry(int i, TexEntry &out) const = 0;
	virtual bool decode(int i, Rgba &out) const = 0;   // straight RGBA, memory R,G,B,A
	// Replace item i's pixels. Pixels that already render identically are not re-encoded: `changed` = false and no stored byte moves
	// (that is what keeps a PNG export -> import round trip byte-exact in every container).
	virtual bool replace(int i, const Rgba &px, std::string *err, bool *changed) = 0;
};

// Same operation for both kinds.
bool ExportPng(const TexSource &s, int i, const std::string &utf8Path, std::string *err);
bool ImportPng(TexSource &s, int i, const std::string &utf8Path, std::string *err, bool *changed = nullptr);

class CgTexSource : public TexSource {
public:
	// pal: 256 normalised entries used to view type 0 images (null = the bank's default palette)
	explicit CgTexSource(BankIO &io, const uint32_t *pal = nullptr) : m_io(io), m_pal(pal) {}
	TexKind kind() const override { return TexKind::CgImage; }
	int count() const override { return m_io.slots(); }
	bool entry(int i, TexEntry &out) const override;
	bool decode(int i, Rgba &out) const override;
	bool replace(int i, const Rgba &px, std::string *err, bool *changed) override;
	ReplaceReport lastReport;
private:
	BankIO &m_io; const uint32_t *m_pal;
};

class PatTexSource : public TexSource {
public:
	explicit PatTexSource(Parts &p) : m_p(p) {}
	TexKind kind() const override { return TexKind::PatTexture; }
	int count() const override;
	bool entry(int i, TexEntry &out) const override;
	bool decode(int i, Rgba &out) const override;
	// Keeps the texture's own kind (PGTX stays PGTX, DXT stays DXT, ...); updates every part (cut-out) that samples the texture
	// (its PPTE size, as the PAT editor's DDS import does).
	bool replace(int i, const Rgba &px, std::string *err, bool *changed) override;
	int encodeType = -1;   // -1 = the texture's own kind, else 1 / 5 / 21 / 22 (PGT2 games only)
	std::vector<int> touchedParts;   // parts (cut-outs) updated by the last replace
private:
	Parts &m_p;
};
const char *PatTextureFormat(const Parts &p, int texture);

// ---- references
struct TexUse {
	int pattern = -1, frame = -1, layer = -1;   // a frame layer that draws it (CG id, or the part set built from the texture)
	int part = -1, partSet = -1;                // .pat only: the cut-out sampling the texture / the part set placing that cut-out
};
// .pat texture -> parts (cut-outs) -> part sets -> pattern layers (usePat). fd may be null (parts / part sets only).
std::vector<TexUse> PatTextureUsers(const Parts &p, int texture, FrameData *fd);
// CG image -> pattern layers (layers without usePat), the reverse index the CG manager keeps (cgm_usage.h).
std::vector<TexUse> CgImageUsers(FrameData &fd, int image);

// ---- saving into the game
// Game ids as hmod uses them: "mbaacc", "mbac", "unib", "uni2", "mbtl", "rbo", "gof2".
// True for the games whose shipped archives are read-only (UNI2 d\*.pak-style archive, MBTL data0NN.bin): edits go to the
// improvement mod's always-on Mods folder, which UNI2-IM / MBTL-IM serve and fpan-im's VFS mounts (core imCompatLayers):
//   <gameDir>\UNI2-IM\Mods\<game path>     <gameDir>\MBTL-IM\Mods\<game path>
bool UsesModFolder(const std::string &game);
// <gameDir>\<IM>\Mods\<key>, key = the game path of the file ("data/chr003/chr003.cg"); "" for a game without a mod folder.
std::string ModFolderPath(const std::string &game, const std::string &gameDir, const std::string &key);
// "data/chr003/chr003.cg" from a path to an extracted / loaded character file (the part from the "data" folder on); "" when none.
std::string GameKeyOf(const std::string &path);
// Atomic write (temp + rename); creates folders. A file already there is moved to <file>.hcbak once (restore puts it back).
bool WriteModFile(const std::string &dst, const std::vector<uint8_t> &bytes, std::string *err);
// Undo WriteModFile: puts <file>.hcbak back, or removes the file when there was none.
bool RestoreModFile(const std::string &dst, std::string *err);

} // namespace cgm
#endif
