// See hmod_ha6.h.
#include "hmod_ha6.h"
#include "../pcmod/pcmod_sha256.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace hmod {
namespace {

struct Ev { char tag[5]; std::vector<int32_t> w; std::string raw; };

const std::map<std::string, int>& FixedWords()
{
	static const std::map<std::string, int> t = {
		{"PSTS",1},{"PLVL",1},{"PFLG",1},{"PUPS",1},{"PDST",1},
		{"FSNH",1},{"FSNA",1},{"FSNE",1},{"FSNI",1},{"HRNM",5},{"HRAT",5},{"HRNS",2},{"HRAS",2},{"HRFF",2},{"ASSM",1},{"EFST",1},{"IFST",1},
		{"AFGP",2},{"AFGX",3},{"AFOF",2},{"AFDL",1},{"AFFL",1},{"AFFE",1},{"AFAL",2},{"AFRG",3},{"AFAX",1},{"AFAY",1},{"AFAZ",1},
		{"AFAN",1},{"AFZM",2},{"AFJP",1},{"AFHK",1},{"AFPR",1},{"AFCT",1},{"AFLP",1},{"AFJC",1},{"AFTN",2},{"AFPL",1},{"AFRT",1},
		{"AFID",1},{"AFPA",1},{"AFJH",1},
		{"ASV0",5},{"ASV1",5},{"ASVA",2},{"ASVC",2},{"ASMV",1},{"ASMX",1},{"ASCN",1},{"ASCS",1},{"ASCT",1},{"ASCF",1},{"ASAA",1},
		{"ASYS",1},{"ASF0",1},{"ASF1",1},{"ASF2",1},{"ASF3",1},{"AST0",7},{"ASAT",1},{"ASKV",1},{"ASSS",1},{"ASDF",1},{"ASCL",2},
		{"ASSE",2},{"ASDE",1},
		{"ATGD",1},{"ATHS",1},{"ATVV",2},{"ATHT",1},{"ATF1",1},{"ATF2",1},{"ATHE",2},{"ATKK",1},{"ATNG",1},{"ATUH",1},{"ATBT",1},
		{"ATSN",1},{"ATSU",1},{"ATSP",1},{"ATGN",1},{"ATAT",1},{"ATHH",1},{"ATAM",1},{"ATCA",1},{"ATC0",3},{"ATSA",1},{"ATSH",1},
		{"ATRF",1},{"ATBC",1},{"ATVD",1},{"ATAB",1},{"ATBG",1},{"ATGE",2},{"ATKZ",1},{"ATGS",1},
		{"EFTP",1},{"EFNO",1},{"IFTP",1},
	};
	return t;
}
const std::set<std::string>& ZeroWord()
{
	static const std::set<std::string> t = {"PEND","FSTR","FEND","AFST","AFED","ASST","ASED","ATST","ATED","EFED","IFED","ASVX","ASS1","ASS2",
		"AFYX","AFF1","AFF2","ATS1","ATS2","ATS3","ATS4","ATS5","ATS6","ASMV0","AFD0","AFD1","AFD2","AFD3","AFD4","AFD5","AFD6","AFD7","AFD8","AFD9",
		"AFY1","AFY2","AFY3","AFY7","AFY8","AFY9"};
	return t;
}

uint32_t U32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }

struct Pattern { uint32_t id; size_t startOff; std::vector<Ev> ev; };

bool Walk(const std::vector<uint8_t>& b, std::vector<Pattern>& pats, std::string& err)
{
	if (b.size() < 40 || std::memcmp(b.data(), "Hantei6DataFile", 15) != 0) { err = "not an HA6 file"; return false; }
	size_t p = 32;
	if (std::memcmp(b.data() + p, "_STR", 4) != 0) { err = "missing _STR"; return false; }
	p += 8;
	auto need = [&](size_t n) { return p + n <= b.size(); };
	while (p + 4 <= b.size()) {
		Ev e{};
		std::memcpy(e.tag, b.data() + p, 4); e.tag[4] = 0;
		const size_t off = p;
		const std::string tag = e.tag;
		p += 4;
		if (tag == "_END") return true;
		if (tag == "PSTR") {
			if (!need(4)) break;
			pats.push_back({U32(b.data() + p), off, {}});
			p += 4;
			continue;
		}
		if (pats.empty()) { err = "tag before the first PSTR"; return false; }
		auto words = [&](size_t n) -> bool {
			if (!need(4 * n)) return false;
			e.w.resize(n);
			for (size_t i = 0; i < n; ++i) e.w[i] = (int32_t)U32(b.data() + p + 4 * i);
			p += 4 * n;
			return true;
		};
		bool ok = true;
		if (tag == "PTT2" || tag == "PTCN" || tag == "PDS2") {
			if (!need(4)) { ok = false; }
			else {
				const uint32_t n = U32(b.data() + p);
				if (!need(4 + (size_t)n)) ok = false;
				else { e.raw.assign((const char*)b.data() + p + 4, n); p += 4 + n; }
			}
		} else if (tag == "PTIT") {
			if (!need(32)) ok = false; else { e.raw.assign((const char*)b.data() + p, 32); p += 32; }
		} else if (tag == "ATV2") {
			if (!need(8)) ok = false;
			else {
				const uint32_t a = U32(b.data() + p), c = U32(b.data() + p + 4);
				const size_t n = 2 + (size_t)a * c * 2;
				if (a > 4096 || c > 4096) ok = false; else ok = words(n);
			}
		} else if (tag == "ATHV" || tag == "ATGV" || tag == "EFPR" || tag == "IFPR") {
			if (!need(4)) ok = false;
			else { const uint32_t n = U32(b.data() + p); ok = n < (1u << 20) && words((size_t)n + 1); }
		} else if (auto it = FixedWords().find(tag); it != FixedWords().end()) {
			ok = words((size_t)it->second);
		} else if (ZeroWord().count(tag)) {
		} else { err = "unknown tag '" + tag + "'"; return false; }
		if (!ok) { err = "truncated at tag '" + tag + "'"; return false; }
		pats.back().ev.push_back(std::move(e));
	}
	err = "no _END";
	return false;
}

using Tag = std::pair<std::string, std::vector<int32_t>>;

Tag Norm(const std::string& t, const std::vector<int32_t>& w)
{
	if (t.size() == 4 && t.compare(0, 3, "AFY") == 0 && t != "AFYX" && t[3] >= '0' && t[3] <= '9') {
		const int v = t[3] - '0';
		return {"AFOF", {0, v >= 4 ? v : v + 10}};
	}
	if (t == "AFYX") return {"AFOF", {0, 10}};
	if (t.size() == 4 && t.compare(0, 3, "AFD") == 0 && t[3] >= '0' && t[3] <= '9') return {"AFDL", {t[3] - '0'}};
	if (t == "ASVX") return {"ASV0", {0x11, 0, 0, 0, 0}};
	return {t, w};
}

std::string CanonHash(const std::vector<Ev>& evs)
{
	std::vector<std::vector<Tag>> frames;
	std::vector<Tag> cur, asblk;
	std::vector<std::vector<Tag>> pool;
	bool inAs = false;
	for (const Ev& e : evs) {
		const std::string t = e.tag;
		if (t == "PTT2" || t == "PTIT" || t == "PTCN" || t == "PDS2") continue;
		if (t == "FSTR") { std::sort(cur.begin(), cur.end()); frames.push_back(cur); cur.clear(); continue; }
		if (t == "ASST") { inAs = true; asblk.clear(); continue; }
		if (t == "ASED") { inAs = false; pool.push_back(asblk); cur.insert(cur.end(), asblk.begin(), asblk.end()); continue; }
		if (t == "ASSM") {
			const size_t k = e.w.empty() ? 0 : (size_t)e.w[0];
			if (k < pool.size()) cur.insert(cur.end(), pool[k].begin(), pool[k].end());
			else cur.push_back({"ASSM?", {(int32_t)k}});
			continue;
		}
		(inAs ? asblk : cur).push_back(Norm(t, e.w));
	}
	std::sort(cur.begin(), cur.end());
	frames.push_back(cur);
	pcmod::Sha256 h;
	auto put = [&](const void* p, size_t n) { h.update(p, n); };
	for (const auto& f : frames) {
		const uint32_t n = (uint32_t)f.size();
		put("F", 1); put(&n, 4);
		for (const Tag& t : f) {
			put(t.first.data(), t.first.size());
			const uint32_t nw = (uint32_t)t.second.size();
			put(&nw, 4);
			if (nw) put(t.second.data(), 4 * nw);
		}
	}
	return pcmod::ToHex(h.finish());
}

} // namespace

bool IsHa6(const std::vector<uint8_t>& b) { return b.size() >= 15 && std::memcmp(b.data(), "Hantei6DataFile", 15) == 0; }

bool SplitHa6(const std::vector<uint8_t>& bytes, Ha6Slots& out, std::string& err)
{
	out.clear();
	std::vector<Pattern> pats;
	if (!Walk(bytes, pats, err)) return false;
	for (const Pattern& p : pats) {
		Ha6Slot s;
		s.id = p.id;
		for (const Ev& e : p.ev) {
			const std::string t = e.tag;
			if (t == "FSTR") s.real = true;
			if ((t == "PTT2" || t == "PTIT") && s.name.empty()) s.name = e.raw.substr(0, e.raw.find('\0'));
		}
		if (s.real) s.canon = CanonHash(p.ev);
		out[p.id] = std::move(s);   // a later block for the same id wins, as in the loader
	}
	return true;
}

const char* SlotDeltaName(SlotDelta d)
{
	switch (d) {
	case SlotDelta::Same: return "same";
	case SlotDelta::Filled: return "fill";
	case SlotDelta::Cleared: return "clear";
	case SlotDelta::Modified: return "modify";
	case SlotDelta::NameOnly: return "name-only";
	}
	return "?";
}

SlotDelta CompareSlot(const Ha6Slot* base, const Ha6Slot* mod)
{
	const bool br = base && base->real, mr = mod && mod->real;
	if (!br && !mr) return SlotDelta::Same;
	if (!br && mr) return SlotDelta::Filled;
	if (br && !mr) return SlotDelta::Cleared;
	if (base->canon != mod->canon) return SlotDelta::Modified;
	return base->name != mod->name ? SlotDelta::NameOnly : SlotDelta::Same;
}

SlotDiffSummary DiffSlots(const Ha6Slots& base, const Ha6Slots& mod)
{
	SlotDiffSummary r;
	std::set<uint32_t> ids;
	for (const auto& kv : base) ids.insert(kv.first);
	for (const auto& kv : mod) ids.insert(kv.first);
	for (uint32_t id : ids) {
		const auto bi = base.find(id), mi = mod.find(id);
		const SlotDelta d = CompareSlot(bi == base.end() ? nullptr : &bi->second, mi == mod.end() ? nullptr : &mi->second);
		switch (d) {
		case SlotDelta::Filled: ++r.filled; r.claimed.push_back(id); break;
		case SlotDelta::Cleared: ++r.cleared; r.claimed.push_back(id); break;
		case SlotDelta::Modified: ++r.modified; r.claimed.push_back(id); break;
		case SlotDelta::NameOnly: ++r.nameOnly; break;
		default: break;
		}
	}
	return r;
}

} // namespace hmod
