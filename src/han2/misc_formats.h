#ifndef HAN2_MISC_FORMATS_H_GUARD
#define HAN2_MISC_FORMATS_H_GUARD

// Smaller structured models used by han2tool's round-trip sections: RIFF/WAVE, MPEG audio (validated), GOF1 .EX3 (LLIF byte-pair compressed image),
// Windows .BMP, GOF1 .CT command tables, GOF1 .WMT tables, Shift-JIS text. Each Parse is exact and each Serialize rebuilds every byte from the fields.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

// ---- RIFF (.WAV) -----------------------------------------------------------------------------------------------------------
struct RiffChunk { char id[4]; std::vector<uint8_t> data; bool padded = false; uint8_t padValue = 0; uint32_t declaredSize = 0; };
struct Riff { char form[4]{}; uint32_t riffSize = 0; std::vector<RiffChunk> chunks; std::vector<uint8_t> tail; uint16_t fmtTag = 0, channels = 0; uint32_t sampleRate = 0; uint16_t bits = 0; };
bool ParseRiff(const uint8_t *p, size_t n, Riff &out, std::string *err);
void SerializeRiff(const Riff &r, std::vector<uint8_t> &out);

// ---- MPEG audio (.MP3): frames validated end to end ------------------------------------------------------------------------------
struct MpegInfo { size_t frames = 0, id3v2 = 0, trailer = 0; int sampleRate = 0; };
bool ValidateMpeg(const uint8_t *p, size_t n, MpegInfo &out, std::string *err);

// ---- GOF1 .EX3 -------------------------------------------------------------------------------------------------------------
// Decompress_EX3_File (gof.exe 0x423C10): 64-byte header "LLIF" + file name + two u16; then blocks of byte-pair-compressed data. A block is a pair table
// written as groups [c][entries]: c > 127 skips c-127 identity entries then defines one, else defines c+1; an entry is `p1` (identity when p1 == its index) or
// `p1 p2` (a pair); the table ends at index 256; then u16 BE symbol count and that many symbol bytes. Blocks repeat to the end of the file.
struct Ex3Group { uint8_t c = 0; std::vector<std::pair<uint8_t, int>> entries; };   // second == -1 for an identity entry
struct Ex3Block { std::vector<Ex3Group> groups; uint16_t count = 0; std::vector<uint8_t> symbols; };
// The Melty line (PB2K1, Melty Blood, ReAct, Act Cadenza / MBAC) uses the same block coder under three header sizes: the decoded byte count sits in the
// last u32 of the header (PB / GOF1: size @0x3C data @0x40; MB / ReAct: @0x40 / 0x44; AC: @0x44 / 0x48). Decoded payload is a BMP.
struct Ex3 { uint8_t header[80]{}; size_t headerSize = 64; std::vector<Ex3Block> blocks; std::vector<uint8_t> tail; size_t decodedBytes = 0; };
bool ParseEx3(const uint8_t *p, size_t n, Ex3 &out, std::string *err, size_t headerSize = 64);
// Tries header sizes 64, 68 and 72 and keeps the one whose blocks tile the file and decode to exactly the size word in the header.
// The encoder: French Bread's EX3 tool is Philip Gage's Byte Pair Encoding (1994) with block size 2000, hash size 4096, no distinct-character limit (256)
// and pair threshold 7. Re-encoding the decoded BMP reproduces the shipped file bit for bit (docs/formats/fb_ex3_encoder.md; proof: fbarctool ex3enc).
// `header` is the shipped header (headerSize bytes, including the decoded-size word); the caller updates the size word when the pixels changed.
struct Ex3EncodeParams { int blockSize = 2000, hashSize = 4096, maxChars = 1 << 30, threshold = 7; };
void EncodeEx3(const uint8_t *header, size_t headerSize, const uint8_t *decoded, size_t n, std::vector<uint8_t> &out, const Ex3EncodeParams &prm = Ex3EncodeParams());
// Expands every block's symbols through its pair table into the decoded bytes (a Windows BMP file).
bool DecodeEx3(const Ex3 &e, std::vector<uint8_t> &out, std::string *err);
bool ParseEx3Auto(const uint8_t *p, size_t n, Ex3 &out, std::string *err);
void SerializeEx3(const Ex3 &e, std::vector<uint8_t> &out);

// ---- Windows BMP -----------------------------------------------------------------------------------------------------------
struct Bmp {
	uint32_t fileSize = 0, reserved = 0, dataOffset = 0;
	uint32_t headerSize = 0; int32_t width = 0, height = 0; uint16_t planes = 0, bpp = 0;
	uint32_t compression = 0, imageSize = 0; int32_t xppm = 0, yppm = 0; uint32_t colorsUsed = 0, colorsImportant = 0;
	std::vector<uint8_t> headerExtra;   // info header bytes after the first 40 (V4/V5 headers)
	std::vector<uint8_t> palette;       // between the headers and dataOffset
	std::vector<uint8_t> pixels;        // dataOffset .. end (rows are padded to 4 bytes as stored)
};
bool ParseBmp(const uint8_t *p, size_t n, Bmp &out, std::string *err);
void SerializeBmp(const Bmp &b, std::vector<uint8_t> &out);

// ---- GOF1 .CT command table (docs/formats/gof1.md 12.1): u32 count | Gof1CommandMove[100] (46 B) | Gof1CtHeader (28 B) = 4632 bytes -------------------
struct CtCommand {
	uint8_t commandId = 0, unref01 = 0, sequence[32]{}, targetPattern = 0, moveClass = 0; uint16_t meterCostLevels = 0;
	uint8_t requestParam = 0, unref27 = 0; uint16_t counterRequirement = 0; uint8_t unref2A[2]{}, flags = 0, flags2 = 0;
};
struct CtHeader { uint8_t unref00 = 0, unref01 = 0, recoveryStyle = 0, unref03 = 0, unref04 = 0, flags = 0, unref06[2]{}; float knockbackScale = 0, damageScale[4]{}; };
struct Ct { uint32_t count = 0; CtCommand cmd[100]; CtHeader hdr; };
bool ParseCt(const uint8_t *p, size_t n, Ct &out, std::string *err);
void SerializeCt(const Ct &c, std::vector<uint8_t> &out);

// ---- GOF1 .WMT (CharFile_LoadWmt, gof.exe 0x40D930): u32 count | count x 154-byte records ---------------------------------------------------------
struct Wmt { uint32_t count = 0; std::vector<std::vector<uint8_t>> records; };
bool ParseWmt(const uint8_t *p, size_t n, Wmt &out, std::string *err);
void SerializeWmt(const Wmt &w, std::vector<uint8_t> &out);

// ---- Shift-JIS text (.TXT / .H): CP932 -> UTF-16 -> CP932 and the CRLF line structure ------------------------------------------------------------
struct Text { std::vector<std::string> lines; std::vector<uint8_t> eol; /* per line: 0 none, 1 "\n", 2 "\r\n" */ };
bool LooksLikeText(const uint8_t *p, size_t n);
bool ParseText(const uint8_t *p, size_t n, Text &out, std::string *err);
void SerializeText(const Text &t, std::vector<uint8_t> &out);


// ---- GOF1 .B polygon object (gof.exe SysEffects_UpdateAndDrawAll -> Poly_DrawObject 0x423170, loader SysGraphic_LoadFileIntoSlot 0x42B170) --------------
// header[16] "Object" + garbage | u16 nTextures | u16 flag (non-zero = has geometry) | nTextures x slot[56] | u32 geomOffset | gap | geometry at geomOffset:
//   u16 nVerts | u32 nFaces (unaligned, bytes 2..5) | tail[20] (unread) | Vertex[nVerts] (3 floats) | Face[nFaces] (56 bytes)
// Face: u16 textureSlot | u16 nIndices (3 = triangle, 4 = quad, others are not drawn) | u16 index[4] | float u[4] | float v[4] | 12 unread bytes.
// Texture slot: char name[32] (empty = untextured; the loader prefixes ".\\grp\\tex\\") | u32 argb | float f[5] (unread). `gap` (396 bytes in every file) and the
// geometry tail are never read by the engine; both are kept verbatim.
struct PolyVertex { float x = 0, y = 0, z = 0; };
struct PolyFace { uint16_t texture = 0, nIndices = 0, index[4]{}; float u[4]{}, v[4]{}; uint8_t unread[12]{}; };
struct PolyTexSlot { char name[32]{}; uint32_t argb = 0; float f[5]{}; };
struct PolyObject {
	uint8_t header[16]{}; uint16_t nTextures = 0, flag = 0;
	std::vector<PolyTexSlot> slots; uint32_t geomOffset = 0; std::vector<uint8_t> gap;
	uint16_t nVerts = 0; uint32_t nFaces = 0; uint8_t geomTail[20]{};
	std::vector<PolyVertex> verts; std::vector<PolyFace> faces;
};
bool ParsePoly(const uint8_t *p, size_t n, PolyObject &out, std::string *err);
void SerializePoly(const PolyObject &o, std::vector<uint8_t> &out);

// ---- GOF1 CHARSEL.CT (CSS_LoadCharselTxtGrid 0x42BCA0): u32 count | count x 168-byte entry, the entry block enciphered with the string key "ファイルが見つかりません" ------
// (buf[i] ^= i + key[i % klen], Crypto_XorWithKeyString 0x4238C0). Entry: name[32] | datFile[32] | ctFile[32] | aiFile[32] | u32 runtimeGridIndex (written by the loader) |
// u32 charFileId | u32 ordinal | u32 id2 | u32 unlockMask | u32 page (1 = grid rows 10+) | u32 gridPos (row*10+col) | u32 a | u32 b | u32 c.
struct CharSelEntry { char name[32]{}, datFile[32]{}, ctFile[32]{}, aiFile[32]{}; uint32_t runtimeGridIndex = 0, charFileId = 0, ordinal = 0, id2 = 0, unlockMask = 0, page = 0, gridPos = 0, a = 0, b = 0, c = 0; };
struct CharSel { uint32_t count = 0; std::vector<CharSelEntry> entries; std::vector<uint8_t> tail; };
bool ParseCharSel(const uint8_t *p, size_t n, CharSel &out, std::string *err);
void SerializeCharSel(const CharSel &c, std::vector<uint8_t> &out);

// ---- GOF1 CPU AI script file (<CHAR>_COM.TXT, training dummies *.CPF): FighterCpuAi_LoadScriptFile 0x403BB0 reads the whole file into a 56,048-byte per-player buffer
// (consumers FighterCpuAiStep 0x403E70, FighterCpuAi_ApplyScriptCommand, FighterCpuAi_PickWeightedScript) -------------------------------------------------------
//   u8 guardBase | u8 reactChance | u8 reactCmd[3] | 203 unread bytes | WeightTable[24] (160 B: s32 script[20], s32 weight[20]; index = oppStance + 3 * (distBucket + 4 * ownAir))
//   | Script[50] x Step[20] (52 B). Files from the archive are 59,048 bytes: 3,000 bytes after the buffer (scripts 50, 51 and 920 more bytes) are never read.
struct AiStep { uint8_t action = 0, unref01 = 0; int16_t commandId = 0, durationBase = 0, durationRandom = 0; uint8_t endFlag = 0, unref09[33]{}, flags = 0, inputDir = 0, unref44[8]{}; };
struct AiTable { int32_t script[20]{}, weight[20]{}; };
struct AiFile {
	uint8_t guardBase = 0, reactChance = 0, reactCmd[3]{}, unref05[203]{};
	AiTable tables[24]; AiStep steps[50][20]; std::vector<uint8_t> tail;
};
bool ParseAi(const uint8_t *p, size_t n, AiFile &out, std::string *err);
void SerializeAi(const AiFile &a, std::vector<uint8_t> &out);

// ---- legacy AI table list (MULTICOM.TXT, not referenced by any gof.exe name table): i32 a | i32 b | 160-byte weight tables to the end -------------------------------
struct AiLegacy { int32_t a = 0, b = 0; std::vector<AiTable> tables; };
bool ParseAiLegacy(const uint8_t *p, size_t n, AiLegacy &out, std::string *err);
void SerializeAiLegacy(const AiLegacy &a, std::vector<uint8_t> &out);

} // namespace han2
#endif
