// Read-only readers for the two modern French-Bread archive layouts that have no PAC-style header:
//   * UNI2 (UNDER NIGHT IN-BIRTH II Sys:Celes) <install>\d\ : small obfuscated-name INDEX files, each naming a data file in the same folder; entries are stored raw.
//   * MBTL (MELTY BLOOD: TYPE LUMINA) data000..019.bin : no table inside the archives, the table is compiled into MBTL.exe; every entry is XOR-enciphered.
// Layouts and the cipher: Hantei_Docs/update_2026_09/UNI2_MBTL_DATA.md (verified there against the games' own code). Neither archive is ever written.
#include "fb_internal.h"
#include <map>
#include <mutex>

namespace fbarc {
namespace {
using namespace detail;

// ---- UNI2 -------------------------------------------------------------------------------------------------------------------------------------------------
class Uni2Archive : public Archive {
public:
	std::vector<std::string> files;     // data files (full paths)
	std::vector<uint16_t> fileOf;       // per entry
	std::string desc;
	std::vector<Entry>& ent() { return m_entries; }
	void setPath(const std::string& p) { m_path = p; }
	Kind kind() const override { return Kind::Uni2D; }
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		const Entry& e = m_entries[i];
		std::ifstream f(P(files[fileOf[i]]), std::ios::binary);
		if (!f) { if (err) *err = "cannot open " + files[fileOf[i]]; return false; }
		const size_t n = (size_t)std::min<uint64_t>(e.size, maxBytes);
		plain.resize(n);
		f.seekg((std::streamoff)e.offset);
		if (n) f.read((char*)plain.data(), (std::streamsize)n);
		if (!f) { if (err) *err = "short read of " + e.name; return false; }
		return true;
	}
	bool rebuild(const std::string&, const Edit&, std::string* err) const override { if (err) *err = "UNI2 archives are read-only here (edit the working copy, or use the game's mod folder)"; return false; }
	std::string describe() const override { return desc; }
};

static std::string CStr(const uint8_t* p, size_t n) { size_t k = 0; while (k < n && p[k]) k++; return std::string((const char*)p, k); }

static bool ReadUni2Index(const std::filesystem::path& ip, const std::filesystem::path& dir, Uni2Archive& a)
{
	std::ifstream f(ip, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (d.size() < 68) return false;
	int32_t nFold, nFile; uint32_t asz;
	memcpy(&nFold, &d[0], 4); memcpy(&nFile, &d[4], 4); memcpy(&asz, &d[8], 4);
	if (nFold < 1 || nFold > 20000 || nFile < 1 || nFile > 200000) return false;
	const std::string arch = CStr(&d[12], 52);
	if (arch.empty()) return false;
	for (unsigned char c : arch) if (c < 0x21 || c > 0x7E) return false;
	std::error_code ec;
	const std::filesystem::path dp = dir / arch;
	if (!std::filesystem::is_regular_file(dp, ec)) return false;
	if (64 + 128ull * nFold + 4 + 80ull * nFile > d.size() + 80) return false;
	struct F { int32_t cnt; std::string name; };
	std::vector<F> folders; size_t p = 64;
	for (int i = 0; i < nFold; i++) { if (p + 128 > d.size()) return false; int32_t c; memcpy(&c, &d[p], 4); folders.push_back({ c, CStr(&d[p + 12], 116) }); p += 128; }
	p += 4;
	struct E { int32_t dsz, csz; uint32_t off; std::string name; };
	std::vector<E> ents;
	for (int i = 0; i < nFile; i++) { if (p + 76 > d.size()) break; E e; memcpy(&e.dsz, &d[p], 4); memcpy(&e.csz, &d[p + 4], 4); memcpy(&e.off, &d[p + 8], 4); e.name = CStr(&d[p + 12], 64); ents.push_back(e); p += 80; }
	const uint16_t fi = (uint16_t)a.files.size();
	a.files.push_back(dp.u8string());
	size_t j = 0;
	for (auto& fo : folders) for (int k = 0; k < fo.cnt && j < ents.size(); k++, j++) {
		if (ents[j].csz < 0) continue;
		Entry e; e.dir = fo.name; for (auto& c : e.dir) if (c == '\\') c = '/';
		while (!e.dir.empty() && e.dir.front() == '/') e.dir.erase(0, 1);
		while (!e.dir.empty() && e.dir.back() == '/') e.dir.pop_back();
		e.name = ents[j].name; e.offset = ents[j].off; e.size = (uint64_t)ents[j].csz;
		a.ent().push_back(std::move(e)); a.fileOf.push_back(fi);
	}
	return true;
}

// ---- MBTL -------------------------------------------------------------------------------------------------------------------------------------------------
static const char kKeyHex[] =
	"d0502599940b206154ca25cce6ded24ef5aab448e2c209b4a1bcb61295f9eede150b495d2769afe98b677888fc65d1c4b24662b72b242e662b41ba5002417146"
	"0d97477c7f3e9de161a964acd8fc8637a77c8111f444554f4f3ba122f04209af40b2da1a9c05f1e455d3d978fb013300db355b7dc68c48171490b4b1e9647dc1"
	"b7424c5f0428749c6ecd1d14ecdaa1c7565736242863ce6783293b25740f97257930e372410deeaeb47f78a933f1983321cb5b2ea231aee7a2ee7ea61aab1d83"
	"8e65e87cdb1e27c52fd2345f59ace0ec437a12c2bc5fb13e7bc7c45a61a0d882ffc8a2a458c1e687e8ac985de575409bc44ba308ff539f14169d576cd45790cb"
	"d3405b134260f59ce6f7ebcc5fb381e8ad255529f276401379567d02f8360d0612b759f020621aad319976d34fcd6b7a05f2714d1e305be1aedc7f4557a616da"
	"c514e6647b2f1e62d27c819b3e2dc6fad4993f9b09eaba26bd15a55c781075eff33f4897da2fc862d086534cb43a5a0f2202073c3d0a238baceb366fe3daf0ee"
	"a41fc7b0c5cae25633a1360d375cef62f815115842fa5fb785457ca8216e507fe09eadd8c46832e504b4700651fb4c9a5dbaa4169e203654500abd2db9325c49"
	"b0a34036607182b84aa64a60897f3a605ad90a9edbe66f07132442263390def51b17caf3204d98760d610c436751815cf65a891980b3d47ad87a53bc18ef9c2a"
	"2ae091368d643cc855ccfed674d7e93539256aaf16ee2b55a6f43716eeb66091e4e7de282d1e38562bcf6842377a3a942b22f58724013d3f867a385e404ff0d2"
	"5114f8f08be453c7965291ae39a23b20d53a71ca3253d2e17ef49cb993211494795029b82c1c54c49f3dc24300b8b5823e53289fc94cb2e3994aac517194967f"
	"6581b7a59a3004f44d784329162270616e56602f7054a4ecdc65b04e61103d3c1c90ebe15c862b00a8ec5c87034a34666e2c62a1afd371a4512bf0d8ecfdd073"
	"a6660d94fa88918fb87f55854d96c93945bb761e0e31e1b5ff85b31598c318cb0ce965e5fc9cfd4a861a754d7e70f681fbede3ce16d6bcc4ee5b4330f0cadded"
	"55033afdeb2c38d819a605041d3f84e699a9f3d84e2bca7c2796e64f797be681899bd6044f9f0be379094dd5b36b3c1226d6ec653e96d382b11ce59bbd3cfd2e"
	"b1997f21ae5d3b10af2d95e6c75ce3aaaa5e179c6f809e8194d7883b4376e89cd3e47e7c92ce9309c2f8255fe17944592e28bca66852f41fd9ae165793927074"
	"fa661b3e825ada76ba5444698a2c26c5b81c23aab1729d048788d91936f65d5e2b069d8f0769d7fea0283c2c48dd5096532293d1d34a61d9a64f17a7b30c7701"
	"70ac4e96a863534a7b5c53cfa6f28b7504225643554008463fea1929923b8606d040747cedb0160253d7d27a29d49f0ad504b227bfbe59f2584127c85cea5114";

static const std::vector<uint8_t>& Key()
{
	static std::vector<uint8_t> k = [] {
		std::vector<uint8_t> v; v.reserve(1024);
		auto hv = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
		for (size_t i = 0; i + 1 < sizeof kKeyHex - 1; i += 2) v.push_back((uint8_t)(hv(kKeyHex[i]) * 16 + hv(kKeyHex[i + 1])));
		return v;
	}();
	return k;
}

// BinCipher: keystream byte i (i >= 2) = KEY[A ^ ((B0 + (n-1-i)) & 0x3FF)]. `p` holds the first `take` bytes of an entry of `n` bytes.
static void MbtlDecrypt(uint8_t* p, size_t take, size_t n)
{
	if (n < 2 || take < 2) return;
	const auto& K = Key();
	const uint8_t b0 = p[0] ^ 0xA5, b1 = p[1] ^ 0x18;
	p[0] = b0; p[1] = b1;
	const uint32_t A = b0 ^ 0xAC, B0 = (uint32_t)(b0 ^ 0xAC ^ b1) ^ 0x76381u;
	for (size_t i = 2; i < take; i++) p[i] ^= K[A ^ ((B0 + (uint32_t)(n - 1 - i)) & 0x3FF)];
}

class MbtlArchive : public Archive {
public:
	std::string root;
	std::vector<std::string> archOf;    // per entry: data file name
	std::string stage;                  // HC_MBTL_ARCHIVE_DIR override (empty = none)
	// file of an archive: the override folder's copy when present, else the install's
	std::string DataFile(const std::string& name) const
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		if (!stage.empty() && fs::exists(P(stage) / name, ec)) return (P(stage) / name).u8string();
		return root + "/" + name;
	}
	std::vector<Entry>& ent() { return m_entries; }
	void setPath(const std::string& p) { m_path = p; }
	Kind kind() const override { return Kind::MbtlBin; }
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		const Entry& e = m_entries[i];
		const std::string fp = DataFile(archOf[i]);
		std::ifstream f(P(fp), std::ios::binary);
		if (!f) { if (err) *err = "cannot open " + fp; return false; }
		const size_t take = (size_t)std::min<uint64_t>(e.size, maxBytes);
		plain.resize(take);
		f.seekg((std::streamoff)e.offset);
		if (take) f.read((char*)plain.data(), (std::streamsize)take);
		if (!f) { if (err) *err = "short read of " + e.name + " from " + archOf[i]; return false; }
		MbtlDecrypt(plain.data(), take, (size_t)e.size);
		return true;
	}
	bool rebuild(const std::string&, const Edit&, std::string* err) const override { if (err) *err = "MBTL archives are read-only here (edit the working copy, or use the game's __Mods / fu folder)"; return false; }
	std::string describe() const override { return "table read from MBTL.exe" + std::string(stage.empty() ? "" : " (archives also read from HC_MBTL_ARCHIVE_DIR)") + ", per-entry XOR cipher, " + std::to_string(m_entries.size()) + " entries"; }
};

struct Pe {
	struct S { uint32_t va, vs, ra; };
	std::vector<S> s; uint32_t ib = 0;
	bool init(const std::vector<uint8_t>& x)
	{
		if (x.size() < 0x200) return false;
		uint32_t pe; memcpy(&pe, &x[0x3C], 4);
		if (pe + 0x100 > x.size()) return false;
		uint16_t nsec, opt; memcpy(&nsec, &x[pe + 6], 2); memcpy(&opt, &x[pe + 20], 2);
		memcpy(&ib, &x[pe + 24 + 28], 4);
		for (int i = 0; i < nsec; i++) {
			const size_t o = pe + 24 + opt + 40 * (size_t)i;
			if (o + 40 > x.size()) return false;
			uint32_t vs, va, rs, ra; memcpy(&vs, &x[o + 8], 4); memcpy(&va, &x[o + 12], 4); memcpy(&rs, &x[o + 16], 4); memcpy(&ra, &x[o + 20], 4);
			s.push_back({ va, std::max(vs, rs), ra });
		}
		return true;
	}
	int64_t v2f(uint32_t v) const { const int64_t r = (int64_t)v - ib; for (auto& e : s) if (r >= e.va && r < (int64_t)e.va + e.vs) return r - e.va + e.ra; return -1; }
	int64_t f2v(uint64_t f) const { for (auto& e : s) if (f >= e.ra && f < (uint64_t)e.ra + e.vs) return (int64_t)(f - e.ra + e.va + ib); return -1; }
};

static size_t Find(const std::vector<uint8_t>& h, const void* n, size_t nl, size_t from = 0)
{
	if (h.size() < nl) return (size_t)-1;
	const uint8_t c0 = *(const uint8_t*)n;
	for (size_t i = from; i + nl <= h.size(); i++) {
		const void* q = memchr(&h[i], c0, h.size() - nl - i + 1);
		if (!q) return (size_t)-1;
		i = (size_t)((const uint8_t*)q - h.data());
		if (memcmp(&h[i], n, nl) == 0) return i;
	}
	return (size_t)-1;
}

static const char* ArchOfTop(const std::string& top)
{
	static const struct { const char* t; const char* a; } m[] = {
		{ "___English", "data000.bin" }, { "___Korean", "data001.bin" }, { "___Region", "data002.bin" }, { "___S_Chinese", "data003.bin" }, { "___T_Chinese", "data004.bin" },
		{ "BattleRes", "data005.bin" }, { "bg", "data006.bin" }, { "Bgm", "data007.bin" }, { "data", "data008.bin" }, { "DLC", "data009.bin" }, { "grpdat", "data010.bin" },
		{ "script", "data011.bin" }, { "se", "data012.bin" }, { "Shader", "data013.bin" }, { "System", "data014.bin" }, { "___French", "data015.bin" },
		{ "___Portuguese", "data016.bin" }, { "___Spanish", "data017.bin" } };
	for (auto& e : m) if (top == e.t) return e.a;
	return nullptr;
}
}  // namespace

std::unique_ptr<Archive> OpenUni2Data(const std::string& dPath, std::string* err)
{
	namespace fs = std::filesystem;
	auto a = std::make_unique<Uni2Archive>();
	a->setPath(dPath);
	std::error_code ec;
	const fs::path dir = P(dPath);
	if (!fs::is_directory(dir, ec)) { if (err) *err = dPath + ": not a folder"; return nullptr; }
	std::vector<fs::path> idx;
	for (auto& e : fs::directory_iterator(dir, ec)) { std::error_code e2; if (e.is_regular_file(e2) && e.file_size(e2) < (4u << 20)) idx.push_back(e.path()); }
	std::sort(idx.begin(), idx.end());
	int n = 0;
	for (auto& p : idx) if (ReadUni2Index(p, dir, *a)) n++;
	if (!n) { if (err) *err = dPath + ": no UNI2 index files"; return nullptr; }
	a->desc = std::to_string(n) + " index files, " + std::to_string(a->ent().size()) + " entries, stored raw";
	return a;
}

std::unique_ptr<Archive> OpenMbtlExe(const std::string& exePath, std::string* err)
{
	namespace fs = std::filesystem;
	// HC_MBTL_ARCHIVE_DIR: read the data*.bin files from this folder first (a relocated copy, or the archives of an install that Steam is replacing:
	// mid-update the install lacks data008.bin and the staged one in steamapps\downloading\<appid>\ may not match this exe's table: caller's risk).
	std::string stageDir, useExe = exePath;
	if (const char* ov = getenv("HC_MBTL_ARCHIVE_DIR")) stageDir = ov;
	std::ifstream f(P(useExe), std::ios::binary);
	if (!f) { if (err) *err = "cannot open " + useExe; return nullptr; }
	std::vector<uint8_t> exe((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	Pe pe;
	if (!pe.init(exe)) { if (err) *err = exePath + ": not a PE file"; return nullptr; }
	const auto& K = Key();
	size_t kpos = Find(exe, K.data(), 32);
	if (kpos == (size_t)-1) { if (err) *err = "ENTRY_KEY not found in " + exePath + " (a different MBTL build?)"; return nullptr; }
	auto cstrAt = [&](size_t o) { size_t k = o; while (k < exe.size() && exe[k]) k++; return std::string((const char*)&exe[o], k - o); };
	std::vector<std::string> names;
	for (size_t p = kpos + 1024; p + 4 <= exe.size(); p += 4) {
		uint32_t v; memcpy(&v, &exe[p], 4);
		const int64_t fo = pe.v2f(v);
		if (fo < 0) break;
		const std::string nm = cstrAt((size_t)fo);
		if (nm.find('/') == std::string::npos || nm.size() > 200) break;
		names.push_back(nm);
	}
	if (names.empty()) { if (err) *err = "file name table not found in " + exePath; return nullptr; }
	const size_t d0 = Find(exe, "data000", 8);
	int64_t szPos = -1;
	if (d0 != (size_t)-1) {
		const int64_t dv = pe.f2v(d0);
		if (dv > 0) {
			const uint32_t ptr = (uint32_t)dv;
			for (size_t q = Find(exe, &ptr, 4); q != (size_t)-1; q = Find(exe, &ptr, 4, q + 4)) {
				// the array of archive names: as many consecutive pointers to "dataNNN" as there are (20 in the 2025 build, 21 once data020.bin exists); the size table follows it
				int n = 0;
				for (; q + 4 * (size_t)(n + 1) <= exe.size() && n < 64; n++) { uint32_t v; memcpy(&v, &exe[q + 4 * n], 4); const int64_t fo = pe.v2f(v); if (fo < 0 || (size_t)fo + 4 > exe.size() || memcmp(&exe[(size_t)fo], "data", 4) != 0) break; }
				if (n >= 20) { szPos = (int64_t)q + 4 * n; break; }
			}
		}
	}
	if (szPos < 0 || (size_t)szPos + 4 * names.size() > exe.size()) { if (err) *err = "size table not found in " + exePath; return nullptr; }
	auto a = std::make_unique<MbtlArchive>();
	a->setPath(exePath);
	a->root = P(exePath).parent_path().u8string();
	a->stage = stageDir;
	std::map<std::string, uint64_t> run;
	std::error_code ec;
	for (size_t i = 0; i < names.size(); i++) {
		int32_t sz; memcpy(&sz, &exe[(size_t)szPos + 4 * i], 4);
		const std::string& nm = names[i];
		const std::string top = nm.substr(0, nm.find('/'));
		const char* arch = ArchOfTop(top);
		uint64_t off = run[top];
		std::string an = arch ? arch : "";
		if (arch && (top == "BattleRes" || top == "grpdat")) {
			const fs::path bp = !stageDir.empty() && fs::exists(P(stageDir) / arch, ec) ? P(stageDir) / arch : P(a->root) / arch;
			const uint64_t base = (uint64_t)fs::file_size(bp, ec);
			if (!ec && off >= base) { an = top == "BattleRes" ? "data018.bin" : "data019.bin"; off -= base; }
		}
		run[top] += sz > 0 ? (uint64_t)sz : 0;
		if (sz <= 0 || an.empty()) continue;
		Entry e;
		const size_t sl = nm.find_last_of('/');
		e.dir = nm.substr(0, sl); e.name = nm.substr(sl + 1); e.offset = off; e.size = (uint64_t)sz;
		a->ent().push_back(std::move(e)); a->archOf.push_back(an);
	}
	return a;
}

}  // namespace fbarc
