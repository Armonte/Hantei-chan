#ifndef HAN2_MISC_FORMATS_H_GUARD
#define HAN2_MISC_FORMATS_H_GUARD

// Smaller structured models used by han2tool's round-trip sections: RIFF/WAVE, MPEG audio (validated), GOF1 .EX3 (LLIF byte-pair compressed image),
// Windows .BMP, GOF1 .CT command tables, GOF1 .WMT tables, Shift-JIS text. Each Parse is exact and each Serialize rebuilds every byte from the fields.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 {

// ---- RIFF (.WAV) -----------------------------------------------------------------------------------------------------------
struct RiffChunk { char id[4]; std::vector<uint8_t> data; bool padded = false; uint32_t declaredSize = 0; };
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

} // namespace han2
#endif
