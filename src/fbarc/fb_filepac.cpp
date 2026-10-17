#include "fb_filepac.h"
#include "fb_internal.h"

#include <cmath>

namespace fbarc {
using namespace detail;

namespace {
constexpr size_t kChunk = 0x1000;

void XorStream(uint8_t* data, size_t size, uint32_t key, uint32_t increment)
{
	if (increment == 0) increment = 1;
	uint8_t k[4] = {(uint8_t)key, (uint8_t)(key >> 8), (uint8_t)(key >> 16), (uint8_t)(key >> 24)};
	for (size_t i = 0; i < size; ++i) { data[i] ^= k[i & 3]; k[i & 3] = (uint8_t)(k[i & 3] + increment); }
}

double Entropy(const uint8_t* d, size_t n)
{
	if (!n) return 0.0;
	size_t c[256] = {};
	for (size_t i = 0; i < n; ++i) ++c[d[i]];
	double e = 0.0;
	for (size_t v : c) if (v) { const double p = (double)v / (double)n; e -= p * std::log2(p); }
	return e;
}

std::string NormDir(std::string s)
{
	for (char& c : s) { if (c == '/') c = '\\'; }
	if (s.rfind(".\\", 0) != 0 && s != ".") s = ".\\" + s;
	while (!s.empty() && s.back() == '\\') s.pop_back();
	return s;
}

class FilePacArchive : public Archive {
public:
	uint8_t hdr[52]{};
	uint32_t key = 0, doff = 0, flag = 1, increment = 3, chunk = 0x1000, version = 1;
	// slot: 256 decoded bytes (leftovers kept). The three record words: retail archives store (first file position, first file index,
	// total size of the folder's files); the game's own version pack (0008.p) stores (0, first index, 1). `style` remembers which.
	struct Folder { std::string name; std::vector<uint8_t> slot; uint32_t pos = 0, fi = 0, sz = 0; int style = 0; };   // style 0 canonical, 1 (0, fi, 1), 2 verbatim
	std::vector<Folder> folders;
	std::vector<uint32_t> owners;                  // per entry
	std::vector<std::vector<uint8_t>> slots;       // per entry: 32 decoded name bytes (leftovers kept)
	mutable int steamMode = -1;                    // lazily detected: 0 = PC (first 4 KiB only), 1 = Steam (+ last 4 KiB)
	Kind kind() const override { return Kind::MbFilePacA; }

	// Payload cipher: the first 4 KiB, and in the Steam build also the last 4 KiB of files larger than 8 KiB. Symmetric.
	void payloadXor(uint8_t* d, size_t n, bool steam) const
	{
		XorStream(d, std::min(n, kChunk), key, increment);
		if (steam && n > 2 * kChunk) XorStream(d + n - kChunk, kChunk, key, increment);
	}
	// PC and Steam differ only in the last 4 KiB of files > 8 KiB. Vote over the larger files: the tail of a Steam archive is
	// ciphered (high entropy raw, low entropy once undone); a PC archive's tail is plain.
	bool steam() const
	{
		if (steamMode >= 0) return steamMode == 1;
		std::vector<size_t> big;
		for (size_t i = 0; i < m_entries.size(); i++) if (m_entries[i].size > 2 * kChunk) big.push_back(i);
		std::sort(big.begin(), big.end(), [&](size_t a, size_t b) { return m_entries[a].size > m_entries[b].size; });
		if (big.size() > 24) big.resize(24);
		int votesSteam = 0, votes = 0;
		std::ifstream f(P(m_path), std::ios::binary);
		for (size_t i : big) {
			std::vector<uint8_t> raw(kChunk);
			f.clear(); f.seekg((std::streamoff)(m_entries[i].offset + m_entries[i].size - kChunk));
			f.read((char*)raw.data(), kChunk);
			if ((size_t)f.gcount() != kChunk) continue;
			std::vector<uint8_t> un = raw; XorStream(un.data(), kChunk, key, increment);
			const double er = Entropy(raw.data(), kChunk), eu = Entropy(un.data(), kChunk);
			if (std::fabs(er - eu) < 0.5) continue;   // uninformative (e.g. already low entropy both ways)
			votes++; if (eu < er) votesSteam++;
		}
		steamMode = (votes > 0 && votesSteam * 2 > votes) ? 1 : 0;
		return steamMode == 1;
	}
	bool read(size_t i, std::vector<uint8_t>& plain, std::string* err, size_t maxBytes) const override
	{
		if (i >= m_entries.size()) { if (err) *err = "bad entry"; return false; }
		const Entry& e = m_entries[i];
		// the cipher spans the first/last 4 KiB, so a truncated probe still needs the whole entry when it reaches the tail
		const bool needAll = maxBytes < e.size && e.size > 2 * kChunk && steam();
		std::vector<uint8_t> all;
		if (!ReadStored(m_path, e.offset, e.size, needAll ? SIZE_MAX : maxBytes, all, e.name, err)) return false;
		if (needAll || maxBytes >= e.size) payloadXor(all.data(), all.size(), steam());
		else XorStream(all.data(), std::min(all.size(), kChunk), key, increment);
		if (all.size() > maxBytes) all.resize(maxBytes);
		plain = std::move(all);
		return true;
	}
	bool rebuild(const std::string& outPath, const Edit& edit, std::string* err) const override
	{
		auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
		struct Out { Item it; uint32_t owner; };
		std::vector<Folder> fold = folders;
		std::vector<Out> outs;
		for (size_t i = 0; i < m_entries.size(); i++) {
			if (edit.remove.count(i)) continue;
			Out o; o.it.src = (int)i; o.it.dir = m_entries[i].dir; o.it.name = m_entries[i].name; o.it.slot = slots[i]; o.owner = owners[i];
			auto r = edit.replace.find(i);
			if (r != edit.replace.end()) { o.it.data = r->second; o.it.inMemory = true; o.it.size = o.it.data.size(); } else o.it.size = m_entries[i].size;
			outs.push_back(std::move(o));
		}
		for (auto& ad : edit.add) {
			std::string path = ad.first; for (char& c : path) if (c == '\\') c = '/';
			size_t sl = path.rfind('/');
			std::string dir = sl == std::string::npos ? "." : NormDir(path.substr(0, sl)), nm = sl == std::string::npos ? path : path.substr(sl + 1);
			if (nm.empty() || nm.size() > 32) return fail("file name empty or longer than 32 bytes: " + nm);
			int fi = -1;
			for (size_t k = 0; k < fold.size(); k++) if (ieq(fold[k].name, dir)) { fi = (int)k; break; }
			if (fi < 0) {
				if (dir.size() > 255) return fail("folder name longer than 255 bytes: " + dir);
				Folder f; f.name = dir; f.slot.assign(256, 0); memcpy(f.slot.data(), dir.data(), dir.size()); fold.push_back(f); fi = (int)fold.size() - 1;
			}
			Out o; o.it.name = nm; o.it.data = ad.second; o.it.inMemory = true; o.it.size = o.it.data.size(); o.owner = (uint32_t)fi;
			outs.push_back(std::move(o));
		}
		std::stable_sort(outs.begin(), outs.end(), [](const Out& a, const Out& b) { return a.owner < b.owner; });
		const size_t nd = fold.size(), nf = outs.size();
		const uint64_t newDoff = 52 + 268ull * nd + 44ull * nf;
		std::vector<uint8_t> head(newDoff, 0);
		uint64_t total = 0;
		std::vector<uint64_t> rel(nf);
		for (size_t i = 0; i < nf; i++) { rel[i] = total; total += outs[i].it.size; }
		if (newDoff + total > 0xFFFFFFFFull) return fail("archive would exceed 4 GB");
		memcpy(head.data(), hdr, 52);
		wr32(head.data() + 24, (uint32_t)newDoff); wr32(head.data() + 28, (uint32_t)total);
		wr32(head.data() + 32, (uint32_t)nd); wr32(head.data() + 36, (uint32_t)nf);
		// folder records: first file position / index and the total size (whose low byte is the name increment)
		std::vector<int64_t> first(nd, -1); std::vector<uint64_t> sum(nd, 0);
		for (size_t i = 0; i < nf; i++) { if (first[outs[i].owner] < 0) first[outs[i].owner] = (int64_t)i; sum[outs[i].owner] += outs[i].it.size; }
		for (size_t d = 0; d < nd; d++) {
			uint8_t* r = head.data() + 52 + 268 * d;
			const bool empty = first[d] < 0;
			uint32_t w0 = empty ? 0 : (uint32_t)rel[(size_t)first[d]], w1 = empty ? 0 : (uint32_t)first[d], w2 = empty ? 0 : (uint32_t)sum[d];
			if (fold[d].style == 1) { w0 = 0; w2 = empty ? 0 : 1; }
			else if (fold[d].style == 2) { w0 = fold[d].pos; w1 = fold[d].fi; w2 = fold[d].sz; }
			wr32(r, w0); wr32(r + 4, w1); wr32(r + 8, w2);
			std::vector<uint8_t> slot = fold[d].slot; slot.resize(256, 0);
			memcpy(r + 12, slot.data(), 256);
			XorStream(r + 12, 256, key, w2 & 0xFF);
		}
		for (size_t i = 0; i < nf; i++) {
			uint8_t* r = head.data() + 52 + 268 * nd + 44 * i;
			wr32(r, (uint32_t)rel[i]); wr32(r + 4, outs[i].owner); wr32(r + 8, (uint32_t)outs[i].it.size);
			std::vector<uint8_t> slot = outs[i].it.slot; slot.resize(32, 0);
			const std::string& nm = outs[i].it.name;
			if (outs[i].it.src < 0 || slot.empty()) { std::fill(slot.begin(), slot.end(), 0); memcpy(slot.data(), nm.data(), std::min<size_t>(nm.size(), 32)); }
			memcpy(r + 12, slot.data(), 32);
			XorStream(r + 12, 32, key, (uint32_t)outs[i].it.size & 0xFF);
		}
		std::vector<Item> items; for (auto& o : outs) items.push_back(o.it);
		const bool st = steam();
		return StreamOut(*this, outPath, head, items, [&](const Item&, std::vector<uint8_t>& d) { payloadXor(d.data(), d.size(), st); }, err);
	}
	std::string describe() const override
	{
		return "FilePacHeaderA v" + std::to_string(version) + ", key 0x" + [&] { char b[16]; snprintf(b, sizeof b, "%08X", key); return std::string(b); }() +
		       ", " + std::to_string(folders.size()) + " folders, " + std::to_string(m_entries.size()) + " files, cipher " + (steam() ? "Steam (first + last 4 KiB)" : "PC (first 4 KiB)");
	}
	static std::unique_ptr<Archive> Load(const std::string& path, std::string* err)
	{
		auto fail = [&](const std::string& m) { if (err) *err = m; return std::unique_ptr<Archive>(); };
		std::ifstream f(P(path), std::ios::binary);
		if (!f) return fail("cannot open " + path);
		f.seekg(0, std::ios::end); const uint64_t fs = (uint64_t)f.tellg(); f.seekg(0);
		auto a = std::make_unique<FilePacArchive>();
		f.read((char*)a->hdr, 52);
		if (!f || memcmp(a->hdr, "FilePacHeaderA", 14) != 0) return fail("not a FilePacHeaderA archive");
		a->m_path = path;
		a->version = rd32(a->hdr + 16); a->key = rd32(a->hdr + 20); a->doff = rd32(a->hdr + 24);
		const uint32_t nd = rd32(a->hdr + 32), nf = rd32(a->hdr + 36);
		a->flag = rd32(a->hdr + 40); a->increment = rd32(a->hdr + 44); a->chunk = rd32(a->hdr + 48);
		if (nd > 100000 || nf > 1000000 || a->doff != 52 + 268ull * nd + 44ull * nf || a->doff > fs) return fail("implausible FilePacHeaderA header");
		if (a->increment != 3 || a->chunk != kChunk) return fail("unsupported cipher parameters (increment " + std::to_string(a->increment) + ", chunk " + std::to_string(a->chunk) + ")");
		std::vector<uint8_t> rec(268);
		for (uint32_t i = 0; i < nd; i++) {
			f.read((char*)rec.data(), 268);
			if (!f) return fail("truncated folder table");
			const uint32_t sz = rd32(rec.data() + 8);
			Folder fo; fo.slot.assign(rec.begin() + 12, rec.end());
			fo.pos = rd32(rec.data()); fo.fi = rd32(rec.data() + 4); fo.sz = sz;
			XorStream(fo.slot.data(), 256, a->key, sz & 0xFF);
			size_t len = 0; while (len < 256 && fo.slot[len]) len++;
			fo.name.assign((const char*)fo.slot.data(), len);
			a->folders.push_back(std::move(fo));
		}
		for (uint32_t i = 0; i < nf; i++) {
			f.read((char*)rec.data(), 44);
			if (!f) return fail("truncated file table");
			Entry e; const uint32_t owner = rd32(rec.data() + 4);
			e.offset = (uint64_t)a->doff + rd32(rec.data()); e.size = rd32(rec.data() + 8);
			std::vector<uint8_t> nm(rec.begin() + 12, rec.begin() + 44);
			XorStream(nm.data(), 32, a->key, (uint32_t)e.size & 0xFF);
			size_t len = 0; while (len < 32 && nm[len]) len++;
			e.name.assign((const char*)nm.data(), len);
			if (owner >= nd) return fail("file " + std::to_string(i) + " has an owner folder out of range");
			e.dir = a->folders[owner].name;
			if (e.offset + e.size > fs) return fail("entry " + std::to_string(i) + " (" + e.name + ") runs past the end of the file");
			a->owners.push_back(owner); a->slots.push_back(std::move(nm));
			a->m_entries.push_back(std::move(e));
		}
		// classify each folder's record words
		std::vector<int64_t> first(nd, -1); std::vector<uint64_t> sum(nd, 0);
		for (uint32_t i = 0; i < nf; i++) { const uint32_t o = a->owners[i]; if (first[o] < 0) first[o] = (int64_t)i; sum[o] += a->m_entries[i].size; }
		for (uint32_t d = 0; d < nd; d++) {
			Folder& fo = a->folders[d];
			const bool empty = first[d] < 0;
			const uint32_t cp = empty ? 0 : (uint32_t)(a->m_entries[(size_t)first[d]].offset - a->doff), cf = empty ? 0 : (uint32_t)first[d], cs = empty ? 0 : (uint32_t)sum[d];
			if (fo.pos == cp && fo.fi == cf && fo.sz == cs) fo.style = 0;
			else if (fo.pos == 0 && fo.fi == cf && fo.sz == (empty ? 0u : 1u)) fo.style = 1;
			else fo.style = 2;
		}
		return a;
	}
};
} // namespace

std::unique_ptr<Archive> LoadFilePacA(const std::string& path, std::string* err) { return FilePacArchive::Load(path, err); }

} // namespace fbarc
