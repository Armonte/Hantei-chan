#ifndef FBARC_FB_INTERNAL_H_GUARD
#define FBARC_FB_INTERNAL_H_GUARD
// Shared helpers of the fbarc readers / writers (not part of the public API).
#include "fb_archive.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fbarc { namespace detail {
constexpr uint32_t kKeyE3 = 0xE3DF59ACu;
constexpr uint32_t kKeyPb = 0xFA261EFBu;

inline uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
inline void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
inline std::filesystem::path P(const std::string& u) { return std::filesystem::u8path(u); }

// ASCII upper-case that leaves CP932 lead/trail bytes alone (the game's name key is the upper-cased name).
inline std::string SjisUpper(const std::string& s)
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

inline bool ieq(const std::string& a, const std::string& b) { return SjisUpper(a) == SjisUpper(b); }

inline bool SamePath(const std::string& a, const std::string& b)
{
	std::error_code ec;
	std::string x = std::filesystem::weakly_canonical(P(a), ec).string(), y = std::filesystem::weakly_canonical(P(b), ec).string();
	for (auto& c : x) c = (char)tolower((unsigned char)c);
	for (auto& c : y) c = (char)tolower((unsigned char)c);
	return x == y;
}

// One output entry of a rebuild: either an untouched source entry (stored bytes are copied verbatim) or in-memory plain bytes.
struct Item {
	int src = -1;                       // index into the source archive, -1 = new data
	std::string dir, name;
	std::vector<uint8_t> slot;          // decoded name slot (leftover bytes after the NUL included); empty = zero padding
	std::vector<uint8_t> data;          // plain bytes when src < 0 or replaced
	bool inMemory = false;
	uint64_t size = 0;
};

inline std::vector<Item> MakePlan(const Archive& a, const Edit& e, const std::vector<std::vector<uint8_t>>& slots)
{
	std::vector<Item> items;
	for (size_t i = 0; i < a.entries().size(); i++) {
		if (e.remove.count(i)) continue;
		Item it; it.src = (int)i; it.dir = a.entries()[i].dir; it.name = a.entries()[i].name;
		if (i < slots.size()) it.slot = slots[i];
		auto r = e.replace.find(i);
		if (r != e.replace.end()) { it.data = r->second; it.inMemory = true; it.size = it.data.size(); }
		else it.size = a.entries()[i].size;
		items.push_back(std::move(it));
	}
	for (auto& ad : e.add) {
		Item it; it.name = ad.first; it.data = ad.second; it.inMemory = true; it.size = it.data.size();
		items.push_back(std::move(it));
	}
	return items;
}

// Streams header + payloads. `encrypt(item, bytes)` enciphers in-memory plain bytes; untouched entries are copied as stored.
template <class Enc>
inline bool StreamOut(const Archive& a, const std::string& outPath, const std::vector<uint8_t>& header, const std::vector<Item>& items, Enc encrypt, std::string* err)
{
	auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
	if (SamePath(a.path(), outPath)) return fail("refusing to overwrite " + a.path());
	const std::string tmp = outPath + ".tmp";
	std::error_code ec;
	{
		std::ofstream f(P(tmp), std::ios::binary);
		if (!f) return fail("cannot create " + tmp);
		f.write((const char*)header.data(), (std::streamsize)header.size());
		std::ifstream in(P(a.path()), std::ios::binary);
		std::vector<uint8_t> buf(1 << 20);
		for (const Item& it : items) {
			if (it.inMemory) {
				std::vector<uint8_t> d = it.data;
				encrypt(it, d);
				if (!d.empty()) f.write((const char*)d.data(), (std::streamsize)d.size());
				continue;
			}
			in.clear(); in.seekg((std::streamoff)a.entries()[(size_t)it.src].offset);
			uint64_t left = it.size;
			while (left) {
				size_t k = (size_t)std::min<uint64_t>(left, buf.size());
				in.read((char*)buf.data(), (std::streamsize)k);
				if ((size_t)in.gcount() != k) { f.close(); std::filesystem::remove(P(tmp), ec); return fail("short read of " + it.name); }
				f.write((const char*)buf.data(), (std::streamsize)k); left -= k;
			}
		}
		if (!f) { f.close(); std::filesystem::remove(P(tmp), ec); return fail("write failed"); }
	}
	std::filesystem::remove(P(outPath), ec);
	std::filesystem::rename(P(tmp), P(outPath), ec);
	if (ec) return fail("could not move the archive into place: " + ec.message());
	return true;
}

inline bool ReadStored(const std::string& path, uint64_t off, uint64_t size, size_t maxBytes, std::vector<uint8_t>& out, const std::string& name, std::string* err)
{
	std::ifstream f(P(path), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + path; return false; }
	size_t n = (size_t)std::min<uint64_t>(size, maxBytes);
	f.seekg((std::streamoff)off); out.resize(n);
	if (n) f.read((char*)out.data(), (std::streamsize)n);
	if (!f) { if (err) *err = "short read of " + name; return false; }
	return true;
}

} } // namespace fbarc::detail
#endif
