#include "gof1_archive.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace gof1 {

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static std::filesystem::path P(const std::string &u) { return std::filesystem::u8path(u); }

static std::string SjisUpper(const std::string &s)
{
	std::string o = s;
	for (size_t i = 0; i < o.size();) {
		unsigned char c = (unsigned char)o[i];
		if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) { i += 2; continue; }
		if (c >= 'a' && c <= 'z') o[i] = (char)(c - 32);
		i++;
	}
	return o;
}

static void StageOne(std::vector<uint8_t> &d, const std::string &name)
{
	std::string key = SjisUpper(name);
	if (key.empty()) return;
	size_t n = std::min<size_t>(d.size(), 9696);
	for (size_t i = 0; i < n; i++) d[i] ^= (uint8_t)((i + (uint8_t)key[i % key.size()]) & 0xFF);
}

bool LooksLikeArchive(const uint8_t *b, size_t n)
{
	if (n < 8) return false;
	uint32_t flag = rd32(b), count = rd32(b + 4) ^ kKey;
	return flag <= 1 && count > 0 && count < 100000;
}

bool Open(const std::string &path, Archive &out, std::string *err)
{
	out = Archive(); out.path = path;
	std::ifstream f(P(path), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + path; return false; }
	f.seekg(0, std::ios::end); out.fileSize = (uint64_t)f.tellg(); f.seekg(0);
	uint8_t head[8]; f.read((char *)head, 8);
	if (!f || !LooksLikeArchive(head, 8)) { if (err) *err = "not a GOF1 .p archive"; return false; }
	out.plainFlag = rd32(head);
	uint32_t n = rd32(head + 4) ^ kKey;
	if (8 + (uint64_t)n * 64 > out.fileSize) { if (err) *err = "index past end of file"; return false; }
	std::vector<uint8_t> raw((size_t)n * 64);
	f.read((char *)raw.data(), (std::streamsize)raw.size());
	uint64_t sum = 8 + (uint64_t)n * 64;
	for (uint32_t i = 0; i < n; i++) {
		uint8_t e[64]; memcpy(e, raw.data() + (size_t)i * 64, 64);
		for (int j = 0; j < 56; j++) e[j] ^= (uint8_t)((3 * ((int)j * (int)i - 28)) & 0xFF);
		Entry en; size_t len = 0; while (len < 56 && e[len]) len++;
		en.name.assign((const char *)e, len);
		en.size = rd32(e + 56) ^ kKey; en.offset = rd32(e + 60);
		if ((uint64_t)en.offset + en.size > out.fileSize) { if (err) *err = "entry " + std::to_string(i) + " runs past the file"; return false; }
		sum += en.size;
		out.entries.push_back(std::move(en));
	}
	if (sum != out.fileSize) { if (err) *err = "entries do not tile the archive"; return false; }
	return true;
}

bool ReadEntry(const Archive &a, size_t i, std::vector<uint8_t> &out, std::string *err)
{
	if (i >= a.entries.size()) { if (err) *err = "bad entry"; return false; }
	const Entry &e = a.entries[i];
	std::ifstream f(P(a.path), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + a.path; return false; }
	f.seekg(e.offset); out.resize(e.size);
	if (e.size) f.read((char *)out.data(), e.size);
	if (!f) { if (err) *err = "short read of " + e.name; return false; }
	if (a.plainFlag == 0) StageOne(out, e.name);
	return true;
}

int Find(const Archive &a, const std::string &name)
{
	std::string u = SjisUpper(name);
	for (size_t i = 0; i < a.entries.size(); i++) if (SjisUpper(a.entries[i].name) == u) return (int)i;
	return -1;
}

bool WriteArchiveReplacing(const Archive &a, int ri, const std::vector<uint8_t> &repl, const std::string &outPath, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::error_code ec;
	if (std::filesystem::weakly_canonical(P(outPath), ec) == std::filesystem::weakly_canonical(P(a.path), ec)) return fail("refusing to overwrite " + a.path);
	const size_t n = a.entries.size();
	std::vector<uint32_t> sizes(n);
	uint64_t total = 8 + (uint64_t)n * 64;
	for (size_t i = 0; i < n; i++) { sizes[i] = (int)i == ri ? (uint32_t)repl.size() : a.entries[i].size; total += sizes[i]; }
	if (total > 0xFFFFFFFFull) return fail("archive would exceed 4 GB");
	std::string tmp = outPath + ".tmp";
	{
		std::ofstream f(P(tmp), std::ios::binary);
		if (!f) return fail("cannot create " + tmp);
		std::vector<uint8_t> head(8 + n * 64, 0);
		wr32(head.data(), a.plainFlag); wr32(head.data() + 4, (uint32_t)n ^ kKey);
		uint32_t off = (uint32_t)head.size();
		for (size_t i = 0; i < n; i++) {
			uint8_t *e = head.data() + 8 + i * 64;
			memcpy(e, a.entries[i].name.data(), a.entries[i].name.size());
			for (int j = 0; j < 56; j++) e[j] ^= (uint8_t)((3 * ((int)j * (int)i - 28)) & 0xFF);
			wr32(e + 56, sizes[i] ^ kKey); wr32(e + 60, off); off += sizes[i];
		}
		f.write((const char *)head.data(), (std::streamsize)head.size());
		std::ifstream in(P(a.path), std::ios::binary);
		std::vector<uint8_t> buf(1 << 20);
		for (size_t i = 0; i < n; i++) {
			if ((int)i == ri) {
				std::vector<uint8_t> d = repl;
				if (a.plainFlag == 0) StageOne(d, a.entries[i].name);
				f.write((const char *)d.data(), (std::streamsize)d.size());
				continue;
			}
			in.clear(); in.seekg(a.entries[i].offset);
			uint32_t left = a.entries[i].size;
			while (left) {
				uint32_t k = std::min<uint32_t>(left, (uint32_t)buf.size());
				in.read((char *)buf.data(), k);
				if ((uint32_t)in.gcount() != k) { f.close(); std::filesystem::remove(P(tmp), ec); return fail("short read of " + a.entries[i].name); }
				f.write((const char *)buf.data(), k); left -= k;
			}
		}
		if (!f) { f.close(); std::filesystem::remove(P(tmp), ec); return fail("write failed"); }
	}
	std::filesystem::remove(P(outPath), ec);
	std::filesystem::rename(P(tmp), P(outPath), ec);
	if (ec) return fail("could not move the archive into place: " + ec.message());
	return true;
}

// ---- character .DAT section cipher ----
static const uint8_t kHeaderKey[] = {0x4d,0x65,0x6d,0x6f,0x72,0x79,0x82,0xa6,0x82,0xe7,0x81,0x5b,0x82,0xc1,0x82,0xc4,0x82,0xb1,0x82,0xc6,0x82,0xc9,0x83,0x56,0x83,0x65,0x83,0x49,0x83,0x4e};
static const uint8_t kPatternKey[] = {0x4d,0x65,0x83,0x93,0x82,0xc7,0x82,0xa4,0x2d,0x2d,0x82,0xc8,0x8e,0x96,0x82,0xcd,0x59,0x41,0x82,0xe8,0x82,0xbd,0x82,0xad,0x4e,0x61,0x82,0xa2,0x82,0xf1,0x82,0xbe,0x82,0xaf,0x82,0xc7,0x82,0xcb};
static const uint8_t kBlobKey[] = {0x68,0x69,0x82,0xdc,0xc5,0x6e,0x6f,0x83,0x4a,0x82,0xc9,0x82,0xe5,0x81,0x48,0x20,0x82,0xb2,0x8b,0xea,0x98,0x4a,0x83,0x69,0x82,0xb1,0x54,0x6f,0x82,0xbe,0x82,0xc9,0x82,0xe5};

static void Xor(uint8_t *p, size_t n, const uint8_t *key, size_t kl) { for (size_t i = 0; i < n; i++) p[i] ^= (uint8_t)((i + key[i % kl]) & 0xFF); }

static void Cipher(std::vector<uint8_t> &d, bool decrypt)
{
	if (d.size() < 0x444) return;
	auto offsets = [&](uint32_t &patEnd, uint32_t &partsSize, uint32_t &cgOff, uint32_t &cgSize) { patEnd = rd32(&d[0x14]); partsSize = rd32(&d[0x18]); cgOff = rd32(&d[0x1C]); cgSize = rd32(&d[0x20]); };
	uint32_t pe, ps, co, cs;
	if (decrypt) Xor(d.data(), 0x444, kHeaderKey, sizeof(kHeaderKey));   // header first: it holds the section offsets
	offsets(pe, ps, co, cs);
	if (pe >= 0x444 && (uint64_t)pe + ps <= d.size()) {
		Xor(d.data() + 0x444, pe - 0x444, kPatternKey, sizeof(kPatternKey));
		Xor(d.data() + pe, ps, kBlobKey, sizeof(kBlobKey));
	}
	if ((uint64_t)co + cs <= d.size() && cs) Xor(d.data() + co, cs, kBlobKey, sizeof(kBlobKey));
	size_t tail = (size_t)co + cs;
	if (d.size() - tail == 0x4000) Xor(d.data() + tail, 0x4000, kBlobKey, sizeof(kBlobKey));
	if (!decrypt) Xor(d.data(), 0x444, kHeaderKey, sizeof(kHeaderKey));
}

void DecryptDat(std::vector<uint8_t> &d) { Cipher(d, true); }
void EncryptDat(std::vector<uint8_t> &d) { Cipher(d, false); }

} // namespace gof1
