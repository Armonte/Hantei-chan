// Store-only ZIP writer/reader; see pcmod_zip.h.
#include "pcmod_zip.h"

#include <cstring>

namespace pcmod {

uint32_t Crc32(const void* data, size_t size, uint32_t crc)
{
	static uint32_t table[256];
	static bool init = false;
	if (!init) {
		for (uint32_t i = 0; i < 256; ++i) {
			uint32_t c = i;
			for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
		init = true;
	}
	const uint8_t* p = (const uint8_t*)data;
	crc = ~crc;
	for (size_t i = 0; i < size; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

namespace {
void Put16(std::vector<uint8_t>& o, uint32_t v) { o.push_back(uint8_t(v)); o.push_back(uint8_t(v >> 8)); }
void Put32(std::vector<uint8_t>& o, uint32_t v) { Put16(o, v & 0xFFFF); Put16(o, v >> 16); }
uint32_t Get16(const uint8_t* p) { return p[0] | (uint32_t)p[1] << 8; }
uint32_t Get32(const uint8_t* p) { return Get16(p) | Get16(p + 2) << 16; }
constexpr uint32_t kLocalSig = 0x04034b50, kCentralSig = 0x02014b50, kEndSig = 0x06054b50;
constexpr uint16_t kVersion = 20, kFlagUtf8 = 1u << 11, kDosTime = 0, kDosDate = (0 << 9) | (1 << 5) | 1;
}

bool BuildZip(const std::vector<ZipEntry>& entries, std::vector<uint8_t>& out, std::string& why)
{
	out.clear();
	if (entries.size() > 0xFFFF) { why = "more than 65535 entries"; return false; }
	std::vector<uint8_t> central;
	for (const ZipEntry& e : entries) {
		if (e.name.size() > 0xFFFF) { why = "entry name too long"; return false; }
		const uint64_t offset = out.size();
		if (offset + 30 + e.name.size() + e.data.size() > 0xFFFFFFFFull) { why = "archive larger than 4 GiB (ZIP64 is not written)"; return false; }
		const uint32_t crc = Crc32(e.data.data(), e.data.size());
		const uint32_t size = (uint32_t)e.data.size();
		Put32(out, kLocalSig); Put16(out, kVersion); Put16(out, kFlagUtf8); Put16(out, 0 /*stored*/);
		Put16(out, kDosTime); Put16(out, kDosDate); Put32(out, crc); Put32(out, size); Put32(out, size);
		Put16(out, (uint32_t)e.name.size()); Put16(out, 0);
		out.insert(out.end(), e.name.begin(), e.name.end());
		out.insert(out.end(), e.data.begin(), e.data.end());

		const bool dir = !e.name.empty() && e.name.back() == '/';
		Put32(central, kCentralSig); Put16(central, kVersion); Put16(central, kVersion); Put16(central, kFlagUtf8); Put16(central, 0);
		Put16(central, kDosTime); Put16(central, kDosDate); Put32(central, crc); Put32(central, size); Put32(central, size);
		Put16(central, (uint32_t)e.name.size()); Put16(central, 0); Put16(central, 0); Put16(central, 0); Put16(central, 0);
		Put32(central, dir ? 0x10 : 0x20 /*MS-DOS directory / archive attribute*/); Put32(central, (uint32_t)offset);
		central.insert(central.end(), e.name.begin(), e.name.end());
	}
	const uint64_t cdOffset = out.size();
	if (cdOffset + central.size() + 22 > 0xFFFFFFFFull) { why = "archive larger than 4 GiB (ZIP64 is not written)"; return false; }
	out.insert(out.end(), central.begin(), central.end());
	Put32(out, kEndSig); Put16(out, 0); Put16(out, 0); Put16(out, (uint32_t)entries.size()); Put16(out, (uint32_t)entries.size());
	Put32(out, (uint32_t)central.size()); Put32(out, (uint32_t)cdOffset); Put16(out, 0);
	return true;
}

bool ReadZip(const std::vector<uint8_t>& b, std::vector<ZipEntry>& out, std::string& why)
{
	out.clear();
	if (b.size() < 22) { why = "not a zip (too short)"; return false; }
	size_t eocd = std::string::npos;
	const size_t lowest = b.size() > 22 + 0xFFFF ? b.size() - 22 - 0xFFFF : 0;
	for (size_t p = b.size() - 22 + 1; p-- > lowest;) if (Get32(&b[p]) == kEndSig) { eocd = p; break; }
	if (eocd == std::string::npos) { why = "not a zip (no end-of-central-directory record)"; return false; }
	const uint32_t count = Get16(&b[eocd + 10]), cdSize = Get32(&b[eocd + 12]), cdOff = Get32(&b[eocd + 16]);
	if (Get16(&b[eocd + 4]) != 0 || Get16(&b[eocd + 6]) != 0 || Get16(&b[eocd + 8]) != count) { why = "multi-disk zip"; return false; }
	if (count == 0xFFFF || cdOff == 0xFFFFFFFFu) { why = "ZIP64 archives are not supported"; return false; }
	if ((uint64_t)cdOff + cdSize > eocd) { why = "central directory out of range"; return false; }
	size_t p = cdOff;
	for (uint32_t i = 0; i < count; ++i) {
		if (p + 46 > eocd || Get32(&b[p]) != kCentralSig) { why = "bad central directory entry"; return false; }
		const uint32_t flags = Get16(&b[p + 8]), method = Get16(&b[p + 10]), crc = Get32(&b[p + 16]);
		const uint32_t csize = Get32(&b[p + 20]), usize = Get32(&b[p + 24]);
		const uint32_t nlen = Get16(&b[p + 28]), xlen = Get16(&b[p + 30]), clen = Get16(&b[p + 32]);
		const uint32_t lho = Get32(&b[p + 42]);
		if (p + 46 + nlen > eocd) { why = "central directory name out of range"; return false; }
		ZipEntry e;
		e.name.assign((const char*)&b[p + 46], nlen);
		p += 46 + nlen + xlen + clen;
		if (flags & 1) { why = "'" + e.name + "': encrypted entries are refused"; return false; }
		if (method != 0) { why = "'" + e.name + "': compressed entry (method " + std::to_string(method) + "); pcmod archives are store-only"; return false; }
		if (csize != usize) { why = "'" + e.name + "': stored entry with differing sizes"; return false; }
		if ((uint64_t)lho + 30 > b.size() || Get32(&b[lho]) != kLocalSig) { why = "'" + e.name + "': bad local header"; return false; }
		const size_t dataAt = (size_t)lho + 30 + Get16(&b[lho + 26]) + Get16(&b[lho + 28]);
		if ((uint64_t)dataAt + csize > cdOff) { why = "'" + e.name + "': data out of range"; return false; }
		e.data.assign(b.begin() + dataAt, b.begin() + dataAt + csize);
		if (Crc32(e.data.data(), e.data.size()) != crc) { why = "'" + e.name + "': CRC mismatch"; return false; }
		out.push_back(std::move(e));
	}
	return true;
}

} // namespace pcmod
