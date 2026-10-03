#include "mbr_formats.h"
#include <cstring>

namespace han2 { namespace mbr {

static_assert(sizeof(MbrCommandMove) == 44 && sizeof(MbrCtParams) == 92 && sizeof(MbrWmtRecord) == 262 && sizeof(MbrCpfFile) == 193388 && sizeof(MbrCharEntry) == 196, "layout");

bool ParseCt(const uint8_t *p, size_t n, CtFile &o, std::string *err)
{
	if (n != kCtSize) { if (err) *err = "command table must be " + std::to_string(kCtSize) + " bytes"; return false; }
	memcpy(&o.count, p, 4); memcpy(o.commands, p + 4, sizeof(o.commands)); memcpy(&o.params, p + 4 + sizeof(o.commands), sizeof(o.params));
	return true;
}
void SerializeCt(const CtFile &c, std::vector<uint8_t> &o)
{
	o.resize(kCtSize); memcpy(o.data(), &c.count, 4); memcpy(o.data() + 4, c.commands, sizeof(c.commands)); memcpy(o.data() + 4 + sizeof(c.commands), &c.params, sizeof(c.params));
}

bool ParseWmt(const uint8_t *p, size_t n, WmtFile &o, std::string *err)
{
	o = WmtFile();
	if (n < 4) { if (err) *err = "too short"; return false; }
	memcpy(&o.count, p, 4);
	if ((uint64_t)n != 4 + (uint64_t)o.count * sizeof(MbrWmtRecord)) { if (err) *err = "size is not 4 + 262 * count"; return false; }
	o.records.resize(o.count);
	if (o.count) memcpy(o.records.data(), p + 4, (size_t)o.count * sizeof(MbrWmtRecord));
	return true;
}
void SerializeWmt(const WmtFile &w, std::vector<uint8_t> &o)
{
	o.resize(4 + w.records.size() * sizeof(MbrWmtRecord)); memcpy(o.data(), &w.count, 4);
	if (!w.records.empty()) memcpy(o.data() + 4, w.records.data(), w.records.size() * sizeof(MbrWmtRecord));
}

bool ParseCpf(const uint8_t *p, size_t n, MbrCpfFile &o, std::string *err)
{
	if (n != sizeof(MbrCpfFile)) { if (err) *err = "CPU script must be 193388 bytes"; return false; }
	memcpy(&o, p, n); return true;
}
void SerializeCpf(const MbrCpfFile &f, std::vector<uint8_t> &o) { o.resize(sizeof(f)); memcpy(o.data(), &f, sizeof(f)); }

// Crypto_XorWithKeyString: byte[k] ^= (k + key[k % 24] + 3), key = Shift-JIS "ファイルが見つかりません"
static void XorKey(uint8_t *b, size_t n)
{
	static const uint8_t key[24] = { 0x83,0x74,0x83,0x40,0x83,0x43,0x83,0x8B,0x82,0xAA,0x8C,0xA9,0x82,0xC2,0x82,0xA9,0x82,0xE8,0x82,0xDC,0x82,0xB9,0x82,0xF1 };
	for (size_t k = 0; k < n; k++) b[k] ^= (uint8_t)(k + key[k % 24] + 3);
}
bool ParseCharaSelect(const uint8_t *p, size_t n, CharaSelectFile &o, std::string *err)
{
	o = CharaSelectFile();
	if (n < 40) { if (err) *err = "too short"; return false; }
	memcpy(&o.count, p, 4);
	if ((uint64_t)n != 40 + (uint64_t)o.count * sizeof(MbrCharEntry)) { if (err) *err = "size is not 40 + 196 * count"; return false; }
	memcpy(o.header, p + 4, 36);
	std::vector<uint8_t> blk(p + 40, p + n); XorKey(blk.data(), blk.size());
	o.entries.resize(o.count); if (o.count) memcpy(o.entries.data(), blk.data(), blk.size());
	return true;
}
void SerializeCharaSelect(const CharaSelectFile &c, std::vector<uint8_t> &o)
{
	o.resize(40 + c.entries.size() * sizeof(MbrCharEntry)); memcpy(o.data(), &c.count, 4); memcpy(o.data() + 4, c.header, 36);
	if (!c.entries.empty()) { memcpy(o.data() + 40, c.entries.data(), c.entries.size() * sizeof(MbrCharEntry)); XorKey(o.data() + 40, c.entries.size() * sizeof(MbrCharEntry)); }
}

}} // namespace han2::mbr
