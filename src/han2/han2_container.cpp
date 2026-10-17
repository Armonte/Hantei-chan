#include "han2_container.h"

#include <cstring>

namespace han2 {

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

bool IsHan2(const uint8_t *p, size_t n)
{
	if (n < kFileHeader || memcmp(p, "HAN2RBO ", 8) != 0) return false;
	uint32_t sub = rd32(p + 0x18);
	return rd32(p + 0x10) == 2 && (sub == 1 || sub == 2);
}

bool Parse(const uint8_t *p, size_t n, Han2File &o, std::string *err)
{
	o = Han2File();
	if (!IsHan2(p, n)) { if (err) *err = "not a HAN2RBO file (signature, version 2, sub 1 or 2)"; return false; }
	memcpy(o.header, p, kFileHeader);
	o.kind = rd32(p + 8);
	o.sub = rd32(p + 0x18);
	o.xorFlag = rd32(p + 0x1C);
	if (o.xorFlag != 0) { if (err) *err = "XOR-obfuscated pattern area (flag +0x1C != 0) is not supported"; return false; }
	uint64_t end = kFileHeader;
	for (int i = 0; i < 4; i++) {
		uint32_t off = rd32(p + 0x20 + 8 * i), sz = rd32(p + 0x24 + 8 * i);
		o.areaOff[i] = off;
		if (sz == 0) continue;
		if ((uint64_t)off + sz > n) { if (err) *err = "area " + std::to_string(i) + " runs past end of file"; return false; }
		o.area[i].assign(p + off, p + off + sz);
		if ((uint64_t)off + sz > end) end = (uint64_t)off + sz;
	}
	if (end != n) { if (err) *err = "file has " + std::to_string(n - end) + " bytes outside the four areas"; return false; }
	// the areas must follow each other with no gap or overlap, or the re-layout would not reproduce the file
	uint64_t pos = kFileHeader;
	for (int i = 0; i < 4; i++) {
		if (o.area[i].empty()) continue;
		if (o.areaOff[i] != pos) { if (err) *err = "area " + std::to_string(i) + " is not contiguous"; return false; }
		pos += o.area[i].size();
	}
	// split the pattern area
	const std::vector<uint8_t> &pa = o.area[kAreaPattern];
	size_t nl = o.nLead(), ns = o.nSec(), hlen = (nl + ns + 3) * 4;
	if (pa.size() < hlen) { if (err) *err = "pattern area shorter than its header"; return false; }
	o.lead.resize(nl); o.tail.resize(3);
	for (size_t i = 0; i < nl; i++) o.lead[i] = rd32(pa.data() + i * 4);
	for (size_t i = 0; i < 3; i++) o.tail[i] = rd32(pa.data() + (nl + ns + i) * 4);
	size_t pos2 = hlen;
	o.sec.resize(ns);
	for (size_t i = 0; i < ns; i++) {
		uint32_t s = rd32(pa.data() + (nl + i) * 4);
		if (pos2 + s > pa.size()) { if (err) *err = "section " + std::to_string(i) + " runs past the pattern area"; return false; }
		o.sec[i].assign(pa.begin() + pos2, pa.begin() + pos2 + s);
		pos2 += s;
	}
	if (pos2 != pa.size()) { if (err) *err = "pattern area has " + std::to_string(pa.size() - pos2) + " unaccounted bytes"; return false; }
	return true;
}

bool Serialize(const Han2File &f, std::vector<uint8_t> &out, std::string *err)
{
	size_t nl = f.nLead(), ns = f.nSec();
	if (f.lead.size() != nl || f.tail.size() != 3 || f.sec.size() != ns) { if (err) *err = "container not in a consistent state"; return false; }
	std::vector<uint8_t> pa((nl + ns + 3) * 4);
	for (size_t i = 0; i < nl; i++) wr32(pa.data() + i * 4, f.lead[i]);
	for (size_t i = 0; i < ns; i++) wr32(pa.data() + (nl + i) * 4, (uint32_t)f.sec[i].size());
	for (size_t i = 0; i < 3; i++) wr32(pa.data() + (nl + ns + i) * 4, f.tail[i]);
	for (size_t i = 0; i < ns; i++) pa.insert(pa.end(), f.sec[i].begin(), f.sec[i].end());

	uint8_t hdr[kFileHeader];
	memcpy(hdr, f.header, kFileHeader);
	out.assign(hdr, hdr + kFileHeader);
	uint64_t pos = kFileHeader;
	for (int i = 0; i < 4; i++) {
		const std::vector<uint8_t> &a = i == kAreaPattern ? pa : f.area[i];
		uint32_t off = f.areaOff[i];
		if (!a.empty() || (i == kAreaPattern)) {
			// full files lay the areas out back to back; a .DT2 keeps the .DAT's offsets for the areas it does not carry
			if (!a.empty()) off = (uint32_t)pos;
			if (!a.empty()) { out.insert(out.end(), a.begin(), a.end()); pos += a.size(); }
		}
		wr32(out.data() + 0x20 + 8 * i, off);
		wr32(out.data() + 0x24 + 8 * i, (uint32_t)a.size());
	}
	return true;
}

} // namespace han2
