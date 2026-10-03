#include "pac_archive.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace pac {

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static std::filesystem::path P(const std::string &u8) { return std::filesystem::u8path(u8); }

bool LooksLikePac(const uint8_t *b, size_t n)
{
	if (n < 8) return false;
	if (rd32(b) != 1) return false;
	uint32_t count = rd32(b + 4) ^ kKey;
	return count > 0 && count < 100000;
}

bool Open(const std::string &path, Archive &out, std::string *err)
{
	out = Archive();
	out.path = path;
	std::ifstream f(P(path), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + path; return false; }
	f.seekg(0, std::ios::end);
	out.fileSize = (uint64_t)f.tellg();
	f.seekg(0);
	uint8_t head[8];
	f.read((char *)head, 8);
	if (!f || !LooksLikePac(head, 8)) { if (err) *err = "not a PAC archive"; return false; }
	uint32_t n = rd32(head + 4) ^ kKey;
	if (8 + (uint64_t)n * kEntrySize > out.fileSize) { if (err) *err = "entry table past end of file"; return false; }
	std::vector<uint8_t> raw((size_t)n * kEntrySize);
	f.read((char *)raw.data(), (std::streamsize)raw.size());
	if (!f) { if (err) *err = "short read of entry table"; return false; }
	out.entries.resize(n);
	for (uint32_t i = 0; i < n; i++) {
		const uint8_t *e = raw.data() + (size_t)i * kEntrySize;
		Entry &en = out.entries[i];
		for (size_t j = 0; j < kNameLen; j++) en.rawName[j] = e[j] ^ (uint8_t)((i * j * 3 + 61) & 0xFF);
		size_t len = 0;
		while (len < kNameLen && en.rawName[len]) len++;
		en.name.assign((const char *)en.rawName, len);
		en.offset = rd32(e + 60);
		en.size = rd32(e + 64) ^ kKey;
		if ((uint64_t)en.offset + en.size > out.fileSize) {
			if (err) *err = "entry " + std::to_string(i) + " (" + en.name + ") runs past end of file";
			return false;
		}
	}
	return true;
}

bool ReadEntry(const Archive &a, size_t i, std::vector<uint8_t> &out, std::string *err)
{
	if (i >= a.entries.size()) { if (err) *err = "entry index out of range"; return false; }
	const Entry &e = a.entries[i];
	std::ifstream f(P(a.path), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + a.path; return false; }
	f.seekg((std::streamoff)e.offset);
	out.resize(e.size);
	if (e.size) f.read((char *)out.data(), (std::streamsize)e.size);
	if (!f) { if (err) *err = "short read of " + e.name; return false; }
	return true;
}

static bool ieq(const std::string &a, const std::string &b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		unsigned char x = a[i], y = b[i];
		if (x >= 'a' && x <= 'z') x -= 32;
		if (y >= 'a' && y <= 'z') y -= 32;
		if (x != y) return false;
	}
	return true;
}

int Find(const Archive &a, const std::string &name)
{
	int r = -1;
	for (size_t i = 0; i < a.entries.size(); i++) if (ieq(a.entries[i].name, name)) r = (int)i;
	return r;
}

bool Build(const std::vector<NewEntry> &entries, std::vector<uint8_t> &out, std::string *err)
{
	size_t n = entries.size();
	uint64_t total = 8 + (uint64_t)n * kEntrySize;
	for (auto &e : entries) {
		if (e.name.size() >= kNameLen) { if (err) *err = "name too long (max 59 bytes): " + e.name; return false; }
		total += e.data.size();
	}
	if (total > 0xFFFFFFFFull) { if (err) *err = "archive would exceed 4 GB"; return false; }
	out.assign((size_t)total, 0);
	wr32(out.data(), 1);
	wr32(out.data() + 4, (uint32_t)n ^ kKey);
	uint32_t off = (uint32_t)(8 + n * kEntrySize);
	for (size_t i = 0; i < n; i++) {
		uint8_t *e = out.data() + 8 + i * kEntrySize;
		for (size_t j = 0; j < kNameLen; j++) {
			uint8_t c = j < entries[i].name.size() ? (uint8_t)entries[i].name[j] : 0;
			if (j >= entries[i].name.size() && entries[i].rawName.size() == kNameLen) c = entries[i].rawName[j];
			e[j] = c ^ (uint8_t)((i * j * 3 + 61) & 0xFF);
		}
		wr32(e + 60, off);
		wr32(e + 64, (uint32_t)entries[i].data.size() ^ kKey);
		if (!entries[i].data.empty()) memcpy(out.data() + off, entries[i].data.data(), entries[i].data.size());
		off += (uint32_t)entries[i].data.size();
	}
	return true;
}

} // namespace pac
