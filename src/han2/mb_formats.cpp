#include "mb_formats.h"
#include <cstring>

namespace han2 { namespace mb {

static_assert(sizeof(MbCtFile) == 4496 && sizeof(MbWmtRecord) == 156 && sizeof(MbCpfFile) == 193388 && sizeof(MbCharSelEntry) == 136, "layout");

bool ParseCt(const uint8_t *p, size_t n, CtFile &o, std::string *err)
{
	if (n != sizeof(MbCtFile)) { if (err) *err = "command table must be 4496 bytes"; return false; }
	memcpy(&o.f, p, n); return true;
}
void SerializeCt(const CtFile &c, std::vector<uint8_t> &o) { o.resize(sizeof(c.f)); memcpy(o.data(), &c.f, sizeof(c.f)); }

bool ParseOldCt(const uint8_t *p, size_t n, OldCtFile &o, std::string *err)
{
	o = OldCtFile();
	const size_t cmd = n == 4232 ? 42 : n == 4432 ? 44 : 0;
	if (!cmd) { if (err) *err = "old command table must be 4232 or 4432 bytes"; return false; }
	o.padded44 = cmd == 44; memcpy(&o.count, p, 4);
	o.commands.resize(100);
	for (int i = 0; i < 100; i++) { memcpy(&o.commands[i], p + 4 + cmd * i, 42); if (cmd == 44) o.pad.insert(o.pad.end(), p + 4 + cmd * i + 42, p + 4 + cmd * i + 44); }
	memcpy(&o.header, p + 4 + cmd * 100, 28);
	return true;
}
void SerializeOldCt(const OldCtFile &c, std::vector<uint8_t> &o)
{
	const size_t cmd = c.padded44 ? 44 : 42; o.assign(4 + cmd * 100 + 28, 0); memcpy(o.data(), &c.count, 4);
	for (int i = 0; i < 100; i++) { memcpy(o.data() + 4 + cmd * i, &c.commands[i], 42); if (c.padded44) memcpy(o.data() + 4 + cmd * i + 42, &c.pad[2 * i], 2); }
	memcpy(o.data() + 4 + cmd * 100, &c.header, 28);
}

bool ParseWmt(const uint8_t *p, size_t n, WmtFile &o, std::string *err)
{
	o = WmtFile();
	if (n < 4) { if (err) *err = "too short"; return false; }
	memcpy(&o.count, p, 4);
	if ((uint64_t)n != 4 + (uint64_t)o.count * sizeof(MbWmtRecord)) { if (err) *err = "size is not 4 + 156 * count"; return false; }
	o.records.resize(o.count); if (o.count) memcpy(o.records.data(), p + 4, (size_t)o.count * sizeof(MbWmtRecord));
	return true;
}
void SerializeWmt(const WmtFile &w, std::vector<uint8_t> &o)
{
	o.resize(4 + w.records.size() * sizeof(MbWmtRecord)); memcpy(o.data(), &w.count, 4);
	if (!w.records.empty()) memcpy(o.data() + 4, w.records.data(), w.records.size() * sizeof(MbWmtRecord));
}

bool ParseCpf(const uint8_t *p, size_t n, MbCpfFile &o, std::string *err)
{
	if (n != sizeof(MbCpfFile)) { if (err) *err = "CPU script must be 193388 bytes"; return false; }
	memcpy(&o, p, n); return true;
}
void SerializeCpf(const MbCpfFile &f, std::vector<uint8_t> &o) { o.resize(sizeof(f)); memcpy(o.data(), &f, sizeof(f)); }

// Crypto_XorWithKeyString (mb.exe 0x46C6D4 key "ファイルが見つかりません"): block[p] ^= (p + key[p % 24]) & 0xFF, p relative to the block
static void XorKey(uint8_t *b, size_t n)
{
	static const uint8_t key[24] = { 0x83,0x74,0x83,0x40,0x83,0x43,0x83,0x8B,0x82,0xAA,0x8C,0xA9,0x82,0xC2,0x82,0xA9,0x82,0xE8,0x82,0xDC,0x82,0xB9,0x82,0xF1 };
	for (size_t k = 0; k < n; k++) b[k] ^= (uint8_t)(k + key[k % 24]);
}
bool ParseCharSel(const uint8_t *p, size_t n, CharSelFile &o, std::string *err)
{
	o = CharSelFile();
	if (n < 4) { if (err) *err = "too short"; return false; }
	memcpy(&o.count, p, 4);
	if ((uint64_t)n != 4 + (uint64_t)o.count * sizeof(MbCharSelEntry)) { if (err) *err = "size is not 4 + 136 * count"; return false; }
	std::vector<uint8_t> blk(p + 4, p + n); XorKey(blk.data(), blk.size());
	o.entries.resize(o.count); if (o.count) memcpy(o.entries.data(), blk.data(), blk.size());
	return true;
}
void SerializeCharSel(const CharSelFile &c, std::vector<uint8_t> &o)
{
	o.resize(4 + c.entries.size() * sizeof(MbCharSelEntry)); memcpy(o.data(), &c.count, 4);
	if (!c.entries.empty()) { memcpy(o.data() + 4, c.entries.data(), c.entries.size() * sizeof(MbCharSelEntry)); XorKey(o.data() + 4, c.entries.size() * sizeof(MbCharSelEntry)); }
}

}} // namespace han2::mb
