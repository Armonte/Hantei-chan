#include "fb_archive.h"
#include "../han2/gof1_archive.h"
#include "fb_filepac.h"
#include "fb_internal.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fbarc {

namespace {
using namespace detail;
// ---------------------------------------------------------------------------------------------------------------------
// PKFileInfo  (MBAC / Act Cadenza .p)   IDA: mbacPC.exe PKArchive_Open 0x431670, PKArchive_ReadEntry 0x431A30
//   +0x00 "PKFileInfo\0" + 5 bytes never read by the game (kept verbatim)     +0x10 u32 type   +0x14 u32 count ^ 0xE3DF59AC
//   count x 72-byte entries at 0x18: name[63] (byte j ^= 3*(j*idx+26)), byte 63 stored plain, u32 offset @64 (plain), u32 size @68 ^ key
//   payload (first 0x7B9 bytes only): type 0: ^= i + upper(name)[i % len] + 3;  type 1: stored;  type 2: ^= 0xCA
// ---------------------------------------------------------------------------------------------------------------------
class PkArchive : public Archive {
public:
	uint8_t hdr[16]{};
	uint32_t type = 0;
	std::vector<std::vector<uint8_t>> slots;   // 64 bytes: decoded name slot incl. byte 63
	Kind kind() const override { return Kind::PkFileInfo; }

	static void Cipher(uint32_t type, const std::string& name, std::vector<uint8_t>& d, size_t from = 0)
	{
		if (type == 1) return;
		size_t n = std::min<size_t>(d.size() + from, 0x7B9);
		if (type == 2) { for (size_t i = from; i < n; i++) d[i - from] ^= 0xCA; return; }
		const std::string key = SjisUpper(name);
		if (key.empty()) return;
		for (size_t i = from; i < n; i++) d[i - from] ^= (uint8_t)(i + (uint8_t)key[i % key.size()] + 3);
	}
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		const Entry& e = m_entries[i];
		if (!ReadStored(m_path, e.offset, e.size, maxBytes, plain, e.name, err)) return false;
		Cipher(type, e.name, plain);
		return true;
	}
	bool rebuild(const std::string& outPath, const Edit& edit, std::string* err) const override
	{
		std::vector<Item> items = MakePlan(*this, edit, slots);
		for (auto& it : items) if (it.name.size() > 62 || it.name.empty()) { if (err) *err = "name empty or longer than 62 bytes: " + it.name; return false; }
		uint64_t off = 24 + 72ull * items.size();
		std::vector<uint8_t> head(24 + 72 * items.size(), 0);
		memcpy(head.data(), hdr, 16);
		wr32(head.data() + 16, type);
		wr32(head.data() + 20, (uint32_t)items.size() ^ kKeyE3);
		for (size_t i = 0; i < items.size(); i++) {
			uint8_t* e = head.data() + 24 + 72 * i;
			if (items[i].slot.size() == 64) memcpy(e, items[i].slot.data(), 64);
			else memcpy(e, items[i].name.data(), items[i].name.size());
			// the slot keeps its leftover bytes, but the name itself always wins
			memcpy(e, items[i].name.data(), items[i].name.size());
			if (items[i].name.size() < 63) e[items[i].name.size()] = 0;
			for (int j = 0; j < 63; j++) e[j] ^= (uint8_t)(3 * (j * (int)i + 26));
			if (off > 0xFFFFFFFFull) { if (err) *err = "archive would exceed 4 GB"; return false; }
			wr32(e + 64, (uint32_t)off);
			wr32(e + 68, (uint32_t)items[i].size ^ kKeyE3);
			off += items[i].size;
		}
		if (off > 0xFFFFFFFFull) { if (err) *err = "archive would exceed 4 GB"; return false; }
		return StreamOut(*this, outPath, head, items, [&](const Item& it, std::vector<uint8_t>& d) { Cipher(type, it.name, d); }, err);
	}
	std::string describe() const override
	{
		const char* t = type == 0 ? "type 0 (name-keyed cipher over the first 0x7B9 bytes)" : type == 1 ? "type 1 (stored plain)" : "type 2 (XOR 0xCA over the first 0x7B9 bytes)";
		return std::string("PKFileInfo, ") + t + ", " + std::to_string(m_entries.size()) + " entries";
	}
	static std::unique_ptr<Archive> Load(const std::string& path, std::string* err)
	{
		auto fail = [&](const std::string& m) { if (err) *err = m; return std::unique_ptr<Archive>(); };
		std::ifstream f(P(path), std::ios::binary);
		if (!f) return fail("cannot open " + path);
		f.seekg(0, std::ios::end); const uint64_t fs = (uint64_t)f.tellg(); f.seekg(0);
		uint8_t h[24];
		f.read((char*)h, 24);
		if (!f || memcmp(h, "PKFileInfo", 11) != 0) return fail("not a PKFileInfo archive");
		auto a = std::make_unique<PkArchive>();
		a->m_path = path; memcpy(a->hdr, h, 16);
		a->type = rd32(h + 16);
		const uint32_t n = rd32(h + 20) ^ kKeyE3;
		if (a->type > 2 || n == 0 || n > 1000000 || 24 + 72ull * n > fs) return fail("bad PKFileInfo header (type " + std::to_string(a->type) + ", count " + std::to_string(n) + ")");
		std::vector<uint8_t> raw((size_t)n * 72);
		f.read((char*)raw.data(), (std::streamsize)raw.size());
		if (!f) return fail("short read of the index");
		for (uint32_t i = 0; i < n; i++) {
			uint8_t e[72]; memcpy(e, raw.data() + (size_t)i * 72, 72);
			for (int j = 0; j < 63; j++) e[j] ^= (uint8_t)(3 * (j * (int)i + 26));
			Entry en; size_t len = 0; while (len < 63 && e[len]) len++;
			en.name.assign((const char*)e, len);
			en.offset = rd32(e + 64); en.size = rd32(e + 68) ^ kKeyE3;
			if (en.offset + en.size > fs) return fail("entry " + std::to_string(i) + " (" + en.name + ") runs past the end of the file");
			a->slots.emplace_back(e, e + 64);
			a->m_entries.push_back(std::move(en));
		}
		return a;
	}
};

// ---------------------------------------------------------------------------------------------------------------------
// PAC v1 / Melty Blood PC .p     header: u32 version, u32 count ^ 0xE3DF59AC;  count x 68-byte entries:
//   name[60] (byte j of entry i ^= (i*j*3+61)&0xFF, byte 59 is the NUL slot), u32 offset, u32 size ^ key;  data follows, contiguous.
//   version 1 (RBO / GOF2 *.PAC, data0x.dat, MB data00.p): payload stored plain.
//   version 0 (Melty Blood / ReAct / Act Cadenza PC .p): the first 0x2173 bytes of every file ^= (name[i % len] + i + 3) (montopak).
// ---------------------------------------------------------------------------------------------------------------------
class PacArchive : public Archive {
public:
	uint32_t version = 1;
	std::vector<std::vector<uint8_t>> slots;
	Kind kind() const override { return version == 0 ? Kind::MbOldP : Kind::RboPac; }
	static void Cipher(const std::string& name, std::vector<uint8_t>& d)
	{
		if (name.empty()) return;
		const size_t n = std::min<size_t>(d.size(), 0x2173);
		for (size_t i = 0; i < n; i++) d[i] ^= (uint8_t)((uint8_t)name[i % name.size()] + i + 3);
	}
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		if (!ReadStored(m_path, m_entries[i].offset, m_entries[i].size, maxBytes, plain, m_entries[i].name, err)) return false;
		if (version == 0) Cipher(m_entries[i].name, plain);
		return true;
	}
	bool rebuild(const std::string& outPath, const Edit& edit, std::string* err) const override
	{
		std::vector<Item> items = MakePlan(*this, edit, slots);
		for (auto& it : items) if (it.name.empty() || it.name.size() >= 60) { if (err) *err = "name empty or longer than 59 bytes: " + it.name; return false; }
		uint64_t off = 8 + 68ull * items.size();
		std::vector<uint8_t> head(8 + 68 * items.size(), 0);
		wr32(head.data(), version); wr32(head.data() + 4, (uint32_t)items.size() ^ kKeyE3);
		for (size_t i = 0; i < items.size(); i++) {
			uint8_t* e = head.data() + 8 + 68 * i;
			for (size_t j = 0; j < 60; j++) {
				uint8_t c = j < items[i].name.size() ? (uint8_t)items[i].name[j] : (items[i].slot.size() == 60 ? items[i].slot[j] : 0);
				e[j] = c ^ (uint8_t)((i * j * 3 + 61) & 0xFF);
			}
			wr32(e + 60, (uint32_t)off); wr32(e + 64, (uint32_t)items[i].size ^ kKeyE3);
			off += items[i].size;
		}
		if (off > 0xFFFFFFFFull) { if (err) *err = "archive would exceed 4 GB"; return false; }
		const uint32_t v = version;
		return StreamOut(*this, outPath, head, items, [v](const Item& it, std::vector<uint8_t>& d) { if (v == 0) Cipher(it.name, d); }, err);
	}
	std::string describe() const override
	{
		return std::string("PAC v") + std::to_string(version) + ", key 0xE3DF59AC, " + std::to_string(m_entries.size()) +
		       (version ? " entries, payload stored plain" : " entries, name-keyed cipher over the first 0x2173 bytes");
	}
	static std::unique_ptr<Archive> Load(const std::string& path, std::string* err)
	{
		auto fail = [&](const std::string& m) { if (err) *err = m; return std::unique_ptr<Archive>(); };
		std::ifstream f(P(path), std::ios::binary);
		if (!f) return fail("cannot open " + path);
		f.seekg(0, std::ios::end); const uint64_t fs = (uint64_t)f.tellg(); f.seekg(0);
		uint8_t h[8]; f.read((char*)h, 8);
		if (!f) return fail("not a PAC archive");
		auto a = std::make_unique<PacArchive>();
		a->m_path = path; a->version = rd32(h);
		const uint32_t n = rd32(h + 4) ^ kKeyE3;
		if (a->version > 1 || n == 0 || n > 100000 || 8 + 68ull * n > fs) return fail("not a PAC archive");
		std::vector<uint8_t> raw((size_t)n * 68);
		f.read((char*)raw.data(), (std::streamsize)raw.size());
		if (!f) return fail("short read of the entry table");
		for (uint32_t i = 0; i < n; i++) {
			const uint8_t* e = raw.data() + (size_t)i * 68;
			std::vector<uint8_t> slot(60);
			for (size_t j = 0; j < 60; j++) slot[j] = e[j] ^ (uint8_t)((i * j * 3 + 61) & 0xFF);
			Entry en; size_t len = 0; while (len < 60 && slot[len]) len++;
			en.name.assign((const char*)slot.data(), len);
			en.offset = rd32(e + 60); en.size = rd32(e + 64) ^ kKeyE3;
			if (en.offset + en.size > fs) return fail("entry " + std::to_string(i) + " (" + en.name + ") runs past the end of the file");
			a->slots.push_back(std::move(slot));
			a->m_entries.push_back(std::move(en));
		}
		return a;
	}
};

// ---------------------------------------------------------------------------------------------------------------------
// Gof1Pb (PB2K1 .dat, GOF1 .p, Rosa / dMp): see han2/gof1_archive.h
// ---------------------------------------------------------------------------------------------------------------------
class PbArchive : public Archive {
public:
	gof1::Archive ga;
	std::vector<std::vector<uint8_t>> slots;
	Kind kind() const override { return Kind::Gof1Pb; }
	static void Cipher(const std::string& name, std::vector<uint8_t>& d)
	{
		const std::string key = SjisUpper(name);
		if (key.empty()) return;
		size_t n = std::min<size_t>(d.size(), 9696);
		for (size_t i = 0; i < n; i++) d[i] ^= (uint8_t)((i + (uint8_t)key[i % key.size()]) & 0xFF);
	}
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		if (!ReadStored(m_path, m_entries[i].offset, m_entries[i].size, maxBytes, plain, m_entries[i].name, err)) return false;
		if (ga.plainFlag == 0) Cipher(m_entries[i].name, plain);
		return true;
	}
	bool rebuild(const std::string& outPath, const Edit& edit, std::string* err) const override
	{
		std::vector<Item> items = MakePlan(*this, edit, slots);
		for (auto& it : items) if (it.name.empty() || it.name.size() >= 56) { if (err) *err = "name empty or longer than 55 bytes: " + it.name; return false; }
		uint64_t off = 8 + 64ull * items.size();
		std::vector<uint8_t> head(8 + 64 * items.size(), 0);
		wr32(head.data(), ga.plainFlag); wr32(head.data() + 4, (uint32_t)items.size() ^ kKeyPb);
		for (size_t i = 0; i < items.size(); i++) {
			uint8_t* e = head.data() + 8 + 64 * i;
			for (size_t j = 0; j < 56; j++) {
				uint8_t c = j < items[i].name.size() ? (uint8_t)items[i].name[j] : (items[i].slot.size() == 56 ? items[i].slot[j] : 0);
				e[j] = c ^ (uint8_t)((3 * ((int)j * (int)i - 28)) & 0xFF);
			}
			wr32(e + 56, (uint32_t)items[i].size ^ kKeyPb); wr32(e + 60, (uint32_t)off);
			off += items[i].size;
		}
		if (off > 0xFFFFFFFFull) { if (err) *err = "archive would exceed 4 GB"; return false; }
		const bool plain = ga.plainFlag != 0;
		return StreamOut(*this, outPath, head, items, [&](const Item& it, std::vector<uint8_t>& d) { if (!plain) Cipher(it.name, d); }, err);
	}
	std::string describe() const override
	{
		return std::string("PB/GOF1 archive v") + std::to_string(ga.plainFlag) + ", key 0xFA261EFB, " + std::to_string(m_entries.size()) +
		       (ga.plainFlag ? " entries, payload stored plain" : " entries, name-keyed cipher over the first 9696 bytes");
	}
	static std::unique_ptr<Archive> Load(const std::string& path, std::string* err)
	{
		auto a = std::make_unique<PbArchive>();
		if (!gof1::Open(path, a->ga, err)) return nullptr;
		a->m_path = path;
		for (auto& e : a->ga.entries) {
			Entry en; en.name = e.name; en.offset = e.offset; en.size = e.size;
			a->m_entries.push_back(en);
			a->slots.emplace_back(e.rawName, e.rawName + 56);
		}
		return a;
	}
};

} // namespace

#ifdef _WIN32
#include <windows.h>
static std::string Conv(const std::string& in, UINT from, UINT to)
{
	if (in.empty()) return in;
	const int wn = MultiByteToWideChar(from, 0, in.data(), (int)in.size(), nullptr, 0);
	std::wstring w(wn > 0 ? wn : 0, L'\0');
	if (wn > 0) MultiByteToWideChar(from, 0, in.data(), (int)in.size(), w.data(), wn);
	const int n = WideCharToMultiByte(to, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string o(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(to, 0, w.data(), (int)w.size(), o.data(), n, nullptr, nullptr);
	return o;
}
std::string NameToUtf8(const std::string& s) { return Conv(s, 932, CP_UTF8); }
std::string NameFromUtf8(const std::string& s) { return Conv(s, CP_UTF8, 932); }
#else
std::string NameToUtf8(const std::string& s) { return s; }
std::string NameFromUtf8(const std::string& s) { return s; }
#endif

const char* KindName(Kind k)
{
	switch (k) {
	case Kind::PkFileInfo: return "PKFileInfo (MBAC .p)";
	case Kind::MbFilePacA: return "FilePacHeaderA (Melty Blood AA .p)";
	case Kind::RboPac: return "PAC v1 (RBO / GOF2, key E3DF59AC)";
	case Kind::MbOldP: return "PAC v0 (Melty Blood PC .p, key E3DF59AC)";
	case Kind::Gof1Pb: return "PB / GOF1 archive (key FA261EFB)";
	default: return "unknown";
	}
}

std::string Archive::relativePath(size_t i) const
{
	const Entry& e = m_entries[i];
	std::string d = e.dir;
	if (d.rfind(".\\", 0) == 0) d = d.substr(2);
	else if (d == ".") d.clear();
	for (auto& c : d) if (c == '\\') c = '/';
	return d.empty() ? e.name : d + "/" + e.name;
}

int Archive::find(const std::string& q) const
{
	std::string qq = q; for (auto& c : qq) if (c == '\\') c = '/';
	for (size_t i = 0; i < m_entries.size(); i++) if (ieq(relativePath(i), qq)) return (int)i;
	for (size_t i = 0; i < m_entries.size(); i++) if (ieq(m_entries[i].name, qq)) return (int)i;
	return -1;
}

Kind Detect(const uint8_t* b, size_t n, const std::string& ext)
{
	if (n >= 11 && memcmp(b, "PKFileInfo", 11) == 0) return Kind::PkFileInfo;
	if (n >= 14 && memcmp(b, "FilePacHeaderA", 14) == 0) return Kind::MbFilePacA;
	if (n < 8) return Kind::Unknown;
	const uint32_t a = rd32(b), c = rd32(b + 4);
	if (a == 1 && (c ^ kKeyE3) > 0 && (c ^ kKeyE3) < 100000) return Kind::RboPac;
	if (a == 0 && (c ^ kKeyE3) > 0 && (c ^ kKeyE3) < 100000) return Kind::MbOldP;
	if (a <= 1 && (c ^ kKeyPb) > 0 && (c ^ kKeyPb) < 100000) return Kind::Gof1Pb;
	(void)ext;
	return Kind::Unknown;
}

std::unique_ptr<Archive> OpenAs(Kind k, const std::string& path, std::string* err)
{
	switch (k) {
	case Kind::PkFileInfo: return PkArchive::Load(path, err);
	case Kind::RboPac: case Kind::MbOldP: return PacArchive::Load(path, err);
	case Kind::Gof1Pb: return PbArchive::Load(path, err);
	case Kind::MbFilePacA: return LoadFilePacA(path, err);
	default: if (err) *err = "unknown archive format"; return nullptr;
	}
}

std::unique_ptr<Archive> Open(const std::string& path, std::string* err)
{
	uint8_t head[64]{};
	size_t n = 0;
	{
		std::ifstream f(P(path), std::ios::binary);
		if (!f) { if (err) *err = "cannot open " + path; return nullptr; }
		f.read((char*)head, sizeof head); n = (size_t)f.gcount();
	}
	std::string ext = P(path).extension().string();
	for (auto& c : ext) c = (char)tolower((unsigned char)c);
	const Kind k = Detect(head, n, ext);
	if (k == Kind::Unknown) { if (err) *err = "not a recognised French-Bread archive"; return nullptr; }
	return OpenAs(k, path, err);
}

static std::map<std::string, Origin>& Origins() { static std::map<std::string, Origin> m; return m; }
static std::string OriginKey(std::string p) { for (auto& c : p) { if (c == '\\') c = '/'; else if (c >= 'A' && c <= 'Z') c += 32; } return p; }
void SetOrigin(const std::string& loosePath, const Origin& o) { Origins()[OriginKey(loosePath)] = o; }
bool GetOrigin(const std::string& loosePath, Origin* out)
{
	auto it = Origins().find(OriginKey(loosePath));
	if (it == Origins().end()) return false;
	if (out) *out = it->second;
	return true;
}
bool SaveEntryReplacing(const Origin& o, const std::vector<uint8_t>& plain, const std::string& outPath, std::string* err)
{
	auto a = Open(o.archive, err);
	if (!a) return false;
	const int idx = a->find(NameFromUtf8(o.entry));
	if (idx < 0) { if (err) *err = "entry " + o.entry + " not found in " + o.archive; return false; }
	Edit e; e.replace[(size_t)idx] = plain;
	return a->rebuild(outPath, e, err);
}

bool VerifyRebuild(const Archive& a, std::string* detail)
{
	std::error_code ec;
	const std::string tmp = (std::filesystem::temp_directory_path(ec) / ("fbarc_rt_" + std::to_string(std::hash<std::string>()(a.path())) + ".bin")).u8string();
	std::string err;
	if (!a.rebuild(tmp, Edit(), &err)) { if (detail) *detail = "rebuild: " + err; return false; }
	std::ifstream x(P(a.path()), std::ios::binary), y(P(tmp), std::ios::binary);
	bool same = std::filesystem::file_size(P(a.path()), ec) == std::filesystem::file_size(P(tmp), ec);
	if (!same && detail) *detail = "size differs: " + std::to_string(std::filesystem::file_size(P(a.path()), ec)) + " vs " + std::to_string(std::filesystem::file_size(P(tmp), ec));
	uint64_t pos = 0;
	std::vector<char> bx(1 << 20), by(1 << 20);
	while (same) {
		x.read(bx.data(), (std::streamsize)bx.size()); y.read(by.data(), (std::streamsize)by.size());
		if (x.gcount() != y.gcount()) { same = false; break; }
		if (memcmp(bx.data(), by.data(), (size_t)x.gcount()) != 0) {
			size_t d = 0; while (bx[d] == by[d]) d++;
			if (detail) *detail = "first difference at 0x" + std::to_string(pos + d);
			same = false; break;
		}
		pos += (uint64_t)x.gcount();
		if (x.gcount() == 0) break;
	}
	x.close(); y.close();
	std::filesystem::remove(P(tmp), ec);
	return same;
}

} // namespace fbarc
