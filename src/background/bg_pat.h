#ifndef BG_PAT_H_GUARD
#define BG_PAT_H_GUARD

#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>

namespace bg {

// Parser for the OLDER PAT format embedded in every MBAACC bgmake stage
// (magic dword[0]==2, dword[1]==0x01234567). This is NOT the editor's
// PAniDataFile PAT — it is the mbac-era stage format with sprites baked in.
//
// Decoded from MBAA.exe Background_RenderLayerWithPalette @0x4b7060 (the
// render path reads the raw file directly, so it is authoritative) and
// verified byte-for-byte against bg07.dat.
//
// On-disk layout (offsets relative to the PAT block start):
//   +0     dword = 2
//   +4     dword = 0x01234567
//   +40    pattern offset table — 1000 dwords (0 = pattern absent)
//   +4040  cutout  offset table — 1000 dwords (0 = cutout absent)
//   +40040 dword   = texture-region offset
//
// A pattern is 40 parts * 104 bytes. Part fields (part-relative):
//   +32  int32  cutout-ref index           (-1 = empty part, skipped)
//   +36  int32  position X   (INT, loaded with fild — MBAA.exe 0x4b75d0;
//   +40  int32  position Y    was misread as float, which turned the common
//                             value 320 into ~4.5e-43 = 0)
//   +48  uint8  flip       (0=none 1=H 2=V 3=both)
//   +49  uint8  additive blend (MBAA sets blend mode 2)
//   +50  uint8  LINEAR texture filter (sampler mode 2) — NOT a blend flag
//   +52  int32  scale X    (* 0.001)
//   +56  int32  scale Y    (* 0.001)
//   +64..67     diffuse color (A,R,G,B bytes) — modulates the texel
//   +68..70     specular/add colour (R,G,B) — D3DRS_SPECULARENABLE is on
//   +71  uint8  draw priority (higher = drawn first / further back)
//   +60  int32  rotation (1/10000 turn) — used by MBAC only, ignored by MBAACC
//
// Cutout source rects are stored in 256-UNIT space: the loader
// (MBAA.exe BgPat_UploadTexturesAndScaleCutouts @0x418420) multiplies them
// by texSize/256 before use, so u = src/256 regardless of texture size.
//
// A cutout is 84 bytes, dword fields:
//   [0] texture index   [2/3] src X/Y   [4/5] src W/H
//   [6/7] quad W/H      [8/9] origin X/Y    +52 name string
//
// The texture region holds, at texReg+28, a 50-slot array of texture data
// offsets (relative to texReg) and, at texReg+3428, a 50-slot array of
// square texture sizes. Texture pixels are raw BGRA8888 (D3DFMT_A8R8G8B8).

// One drawable part within a pattern.
struct PatPart {
	int     partIndex = 0;     // original 0..39 slot (stable draw-order tiebreak)
	int     cutoutRef = -1;    // index into OldPat cutouts
	float   posX = 0.0f, posY = 0.0f;
	float   scaleX = 1.0f, scaleY = 1.0f;
	int     flip = 0;          // 0=none 1=H 2=V 3=both
	bool    additive = false;  // +49 — additive blend
	bool    linearFilter = false; // +50 — bilinear sampling (not a blend)
	int     rotation = 0;      // +60 — MBAC only (1/10000 turn)
	uint8_t addR = 0, addG = 0, addB = 0; // +68..70 specular add colour
	uint8_t priority = 0;      // +71 — draw-order key
	uint8_t colA = 255, colR = 255, colG = 255, colB = 255;
};

// A pattern = a list of present parts, pre-sorted back-to-front.
struct PatPattern {
	std::vector<PatPart> parts;
};

// A sub-rectangle of a texture plus its display quad and pivot.
struct PatCutout {
	int texture = 0;
	int srcX = 0, srcY = 0, srcW = 0, srcH = 0;   // texels in the texture
	int quadW = 0, quadH = 0;                     // on-screen quad size
	int originX = 0, originY = 0;                 // pivot offset
};

// One embedded texture: square, raw D3DFMT_A8R8G8B8 (BGRA byte order) —
// exactly what Texture::LoadDirect(..., bgr=true) consumes.
struct PatTexture {
	int size = 0;                 // width == height
	std::vector<uint8_t> bgra;    // size*size*4 bytes, BGRA8888
};

class OldPat {
public:
	// True if `data` carries the older-PAT magic.
	static bool IsOldPat(const uint8_t* data, size_t size);

	// Parse the raw PAT block. Returns false (and leaves IsValid() false) if
	// the magic is wrong or the file is structurally broken.
	bool Parse(const uint8_t* data, size_t size);
	bool IsValid() const { return valid; }

	// Null if the id is absent.
	const PatPattern* GetPattern(int id) const;
	const PatCutout*  GetCutout(int id) const;
	const PatTexture* GetTexture(int id) const;

	// Whole-container access (for converting into the editor's Parts model).
	const std::map<int, PatPattern>& Patterns() const { return patterns; }
	const std::map<int, PatCutout>&  Cutouts()  const { return cutouts; }
	const std::vector<PatTexture>&   Textures() const { return textures; }

private:
	bool valid = false;
	std::map<int, PatPattern> patterns;
	std::map<int, PatCutout>  cutouts;
	std::vector<PatTexture>   textures;   // dense, indexed by texture id
};

} // namespace bg

#endif // BG_PAT_H_GUARD
