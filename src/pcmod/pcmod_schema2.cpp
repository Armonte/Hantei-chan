// pcmod schema 2 data files: see pcmod_schema2.h. Parser/canonical mirror PovertyCaster pc/mods/Schema2.hpp.
#include "pcmod_schema2.h"
#include "pcmod_manifest.h"
#include "pcmod_sha256.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace pcmod::s2 {

// ---------------------------------------------------------------------------------------------------------------------
// Strict TOML subset (S2.2). Behaviour identical to PC parseToml.
// ---------------------------------------------------------------------------------------------------------------------
static bool IsKeyChar(char c, bool first)
{
	const bool al = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	return first ? ((c >= 'a' && c <= 'z') || c == '_') : al;
}

std::string ParseToml(const std::string& text, TDoc& out)
{
	out = TDoc{};
	out.tables.push_back(TTable{});
	if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) return "line 1: BOM";
	size_t pos = 0;
	int ln = 0;
	auto fail = [&](const std::string& w) { return "line " + std::to_string(ln) + ": " + w; };
	while (pos <= text.size()) {
		size_t e = text.find('\n', pos);
		const bool last = e == std::string::npos;
		if (last) e = text.size();
		std::string l = text.substr(pos, e - pos);
		pos = e + 1;
		++ln;
		if (ln > 65536) return fail("more than 65536 lines");
		if (!l.empty() && l.back() == '\r') l.pop_back();
		if (l.find('\r') != std::string::npos) return fail("stray CR");
		for (char c : l) if (((unsigned char)c < 0x20 && c != '\t') || c == 0x7F) return fail("control character");
		size_t i = 0;
		auto sp = [&] { while (i < l.size() && (l[i] == ' ' || l[i] == '\t')) ++i; };
		sp();
		if (i >= l.size() || l[i] == '#') { if (last) break; continue; }
		if (l[i] == '[') {
			const bool arr = i + 1 < l.size() && l[i + 1] == '[';
			i += arr ? 2 : 1;
			sp();
			std::string name;
			while (i < l.size() && (IsKeyChar(l[i], name.empty()) || (l[i] == '.' && !name.empty() && name.back() != '.'))) name += l[i++];
			if (name.empty() || name.back() == '.') return fail("bad table name");
			sp();
			if (arr) { if (l.compare(i, 2, "]]") != 0) return fail("expected ]]"); i += 2; }
			else { if (i >= l.size() || l[i] != ']') return fail("expected ]"); ++i; }
			sp();
			if (i < l.size() && l[i] != '#') return fail("trailing text after table header");
			if (!arr) for (const auto& t : out.tables) if (t.name == name) return fail("duplicate table [" + name + "]");
			TTable t;
			t.name = name; t.array = arr; t.line = ln;
			out.tables.push_back(t);
		} else {
			std::string key;
			while (i < l.size() && IsKeyChar(l[i], key.empty())) key += l[i++];
			if (key.empty()) return fail("expected a key");
			sp();
			if (i >= l.size() || l[i] != '=') return fail(i < l.size() && l[i] == '.' ? "dotted keys are not supported" : "expected '='");
			++i;
			sp();
			TKv kv;
			kv.key = key; kv.line = ln;
			if (i >= l.size()) return fail("missing value");
			if (l[i] == '"') {
				++i;
				kv.v.k = TVal::K::Str;
				bool closed = false;
				while (i < l.size()) {
					const char c = l[i++];
					if (c == '"') { closed = true; break; }
					if (c == '\\') {
						if (i >= l.size() || (l[i] != '\\' && l[i] != '"')) return fail("only \\\\ and \\\" escapes are supported");
						kv.v.s += l[i++];
					} else kv.v.s += c;
					if (kv.v.s.size() > 256) return fail("string longer than 256 bytes");
				}
				if (!closed) return fail("unterminated string");
			} else if (l.compare(i, 4, "true") == 0 && (i + 4 >= l.size() || !IsKeyChar(l[i + 4], false))) { kv.v.k = TVal::K::Bool; kv.v.b = true; i += 4; }
			else if (l.compare(i, 5, "false") == 0 && (i + 5 >= l.size() || !IsKeyChar(l[i + 5], false))) { kv.v.k = TVal::K::Bool; i += 5; }
			else if (l[i] == '0' && i + 1 < l.size() && l[i + 1] == 'x') {
				i += 2;
				size_t n = 0;
				unsigned long long v = 0;
				while (i < l.size() && std::isxdigit((unsigned char)l[i])) {
					const char c = l[i++];
					v = v * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
					if (++n > 8) return fail("hex integer over 0xFFFFFFFF");
				}
				if (!n) return fail("empty hex integer");
				kv.v.i = (long long)v;
			} else if (l[i] == '-' || (l[i] >= '0' && l[i] <= '9')) {
				const bool neg = l[i] == '-';
				if (neg) ++i;
				size_t n = 0;
				long long v = 0;
				while (i < l.size() && l[i] >= '0' && l[i] <= '9') { v = v * 10 + (l[i++] - '0'); if (++n > 15) return fail("integer too long"); }
				if (!n) return fail("bad integer");
				if (i < l.size() && (l[i] == '.' || l[i] == 'e' || l[i] == 'E' || l[i] == '_' || l[i] == ':' || l[i] == '-'))
					return fail("floats, dates and digit separators are not supported");
				kv.v.i = neg ? -v : v;
			} else return fail("unsupported value (inline tables, arrays, literals and multi-line strings are refused)");
			sp();
			if (i < l.size() && l[i] != '#') return fail("trailing text after value");
			TTable& t = out.tables.back();
			for (const auto& o : t.kv) if (o.key == key) return fail("duplicate key '" + key + "'");
			t.kv.push_back(kv);
		}
		if (last) break;
	}
	return {};
}

// ---------------------------------------------------------------------------------------------------------------------
// Profile
// ---------------------------------------------------------------------------------------------------------------------
const Profile& MbaaccProfile()
{
	static const Profile pf = [] {
		Profile p;
		p.abiId = "mbaacc-prim-1";
		p.imageBegin = 0x401000; p.imageEnd = 0x54B000;   // .text + .idata + .rdata (PC MbaaccSim_PcMods.cpp profile())
		p.textBegin = 0x401000;  p.textEnd = 0x51B000;    // vanilla code (.text)
		// S2.8 allow-listed .data table. Extent = the BOF roster edits in docs/extended_melty_port/RANGE_CATALOG.json
		// (0x551D30..0x551DF0); PC has not pinned the table id/extent yet (see the cg2 questions in the export doc).
		p.dataTables.push_back({ "css_roster", 0x551D30, 0x551DF0 });
		// Advisory copy of PC's reserved list (the parts visible in its profile source). PC rebuilds it from its real tables
		// at boot and is authoritative; the boot-patch table (appendBootReserved) and NearestEnemy/MoonAnimWiden sites are
		// not mirrored here.
		p.reserved.push_back({ 0x51B000, 0x410, "import table (.idata)" });
		static const std::pair<uint32_t, const char*> kHooks[] = {
			{ 0x463448, "Tag hook H1 (22D gate)" }, { 0x448D4F, "Tag hook H2 (TagIn/TagOut fill)" }, { 0x449063, "Tag hook H3 (partner)" },
			{ 0x449044, "Tag hook H4 (mode)" }, { 0x465F97, "Tag hook H5 (reserve guards)" }, { 0x466095, "Tag hook H6 (cooldown)" },
			{ 0x448670, "Tag hook H7 (win screen)" }, { 0x46624F, "Tag hit-swap hook" }, { 0x4746ED, "Tag time-over hook" },
			{ 0x439AD6, "Tag result-draw hook" }, { 0x439E64, "Tag summary hook" }, { 0x46BE42, "Tag command-text hook" },
			{ 0x465FC4, "TagTuningNet cooldown imm" }, { 0x466059, "TagTuningNet cooldown imm" }, { 0x415580, "CssNative blit interposer" },
			{ 0x48AAD6, "CssNative char preview call" }, { 0x48AAE3, "CssNative char preview bound" }, { 0x48A720, "CssNative char preview fn" },
		};
		p.reserved.push_back({ 0x463500, 0x260, "Character_StartTechRecovery (TagAssist caller, typed-hitgrab hook)" });
		p.reserved.push_back({ 0x448FB0, 0x075, "EtmReload: Scene_LoadInitialAssets" });
		p.reserved.push_back({ 0x45F650, 0x0CD, "EtmReload: Actor_LoadPatternAndFramePointers" });
		p.reserved.push_back({ 0x423380, 0x167, "EtmReload: ResetBattleMode" });
		for (const auto& h : kHooks) p.reserved.push_back({ h.first, 16, h.second });
		// P1: no primitives, features or rules are registered yet (PC refuses any pc:/feature/rule until they land).
		return p;
	}();
	return pf;
}

// ---------------------------------------------------------------------------------------------------------------------
// Byte strings
// ---------------------------------------------------------------------------------------------------------------------
static int HexNib(char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }

bool ParseHexBytes(const std::string& s, std::vector<uint8_t>& out)
{
	out.clear();
	size_t i = 0;
	while (i < s.size()) {
		if (!out.empty() && i < s.size() && s[i] == ' ') ++i;   // one optional space between bytes
		if (i + 2 > s.size() || !std::isxdigit((unsigned char)s[i]) || !std::isxdigit((unsigned char)s[i + 1])) return false;
		out.push_back((uint8_t)(HexNib(s[i]) * 16 + HexNib(s[i + 1])));
		i += 2;
		if (i < s.size() && s[i] == ' ' && (i + 1 >= s.size() || s[i + 1] == ' ')) return false;
	}
	return !out.empty();
}

std::string HexBytes(const std::vector<uint8_t>& b)
{
	static const char* k = "0123456789abcdef";
	std::string s;
	for (uint8_t x : b) { s += k[x >> 4]; s += k[x & 15]; }
	return s;
}

std::string HexBytesSpaced(const std::vector<uint8_t>& b)
{
	static const char* k = "0123456789ABCDEF";
	std::string s;
	for (size_t i = 0; i < b.size(); ++i) { if (i) s += ' '; s += k[b[i] >> 4]; s += k[b[i] & 15]; }
	return s;
}

std::string H8(uint32_t v) { char b[16]; std::snprintf(b, sizeof b, "%08x", v); return b; }

// ---------------------------------------------------------------------------------------------------------------------
// Typed parse (mirrors PC parsePatches/parseFeatures/parseRules)
// ---------------------------------------------------------------------------------------------------------------------
namespace {
std::string At(const char* file, int line, const std::string& w) { return std::string(file) + " line " + std::to_string(line) + ": " + w; }
const TKv* Find(const TTable& t, const char* k) { for (const auto& e : t.kv) if (e.key == k) return &e; return nullptr; }
std::string CheckFormat(const TDoc& d, const char* file)
{
	const TTable& root = d.tables[0];
	for (const auto& e : root.kv) if (e.key != "format") return At(file, e.line, "unknown key '" + e.key + "'");
	const TKv* f = Find(root, "format");
	if (!f || f->v.k != TVal::K::Int || f->v.i != 1) return std::string(file) + ": 'format = 1' is required";
	return {};
}
bool InDataTable(const Profile& pf, uint32_t addr, size_t len)
{
	for (const DataTable& t : pf.dataTables)
		if (addr >= t.begin && (uint64_t)addr + len <= t.end) return true;
	return false;
}
}

std::string ParsePatches(const std::string& text, const Profile& pf, Data& out)
{
	static const char* F = "patches.toml";
	TDoc d;
	std::string e = ParseToml(text, d);
	if (!e.empty()) return std::string(F) + " " + e;
	if (!(e = CheckFormat(d, F)).empty()) return e;
	for (size_t ti = 1; ti < d.tables.size(); ++ti) {
		const TTable& t = d.tables[ti];
		if (t.name != "patch" || !t.array) return At(F, t.line, "unknown table '" + t.name + "'");
		Patch p;
		p.line = t.line;
		const TKv *a = nullptr, *o = nullptr, *n = nullptr, *c = nullptr, *j = nullptr;
		for (const auto& kv : t.kv) {
			if (kv.key == "addr") a = &kv; else if (kv.key == "orig") o = &kv; else if (kv.key == "new") n = &kv;
			else if (kv.key == "call") c = &kv; else if (kv.key == "jmp") j = &kv;
			else return At(F, kv.line, "unknown key '" + kv.key + "'");
		}
		if (!a || a->v.k != TVal::K::Int || a->v.i < 0) return At(F, t.line, "addr: integer required");
		if (!o || o->v.k != TVal::K::Str) return At(F, t.line, "orig: hex string required");
		p.addr = (uint32_t)a->v.i;
		if (!ParseHexBytes(o->v.s, p.orig) || p.orig.size() > 32) return At(F, o->line, "orig: 1..32 hex bytes");
		const int forms = (n ? 1 : 0) + (c ? 1 : 0) + (j ? 1 : 0);
		if (forms != 1) return At(F, t.line, "exactly one of new / call / jmp is required");
		if (n) {
			if (n->v.k != TVal::K::Str || !ParseHexBytes(n->v.s, p.raw)) return At(F, n->line, "new: hex string required");
			if (p.raw.size() != p.orig.size()) return At(F, n->line, "orig and new differ in length");
		} else {
			const TKv* x = c ? c : j;
			p.kind = c ? Kind::Call : Kind::Jmp;
			if (x->v.k != TVal::K::Str) return At(F, x->line, "call/jmp: string required");
			if (p.orig.size() < 5) return At(F, x->line, "call/jmp needs orig of at least 5 bytes");
			const std::string& s = x->v.s;
			if (s.rfind("vanilla:0x", 0) == 0) {
				p.toVanilla = true;
				unsigned long long v = 0;
				size_t k = 10;
				if (s.size() == k || s.size() - k > 8) return At(F, x->line, "bad vanilla target");
				for (; k < s.size(); ++k) {
					if (!std::isxdigit((unsigned char)s[k])) return At(F, x->line, "bad vanilla target");
					v = v * 16 + (unsigned)HexNib(s[k]);
				}
				p.target = (uint32_t)v;
				if (p.target < pf.textBegin || p.target >= pf.textEnd) return At(F, x->line, "call target 0x" + H8(p.target) + " is not vanilla code");
			} else if (s.rfind("pc:", 0) == 0) {
				p.primitive = s.substr(3);
				if (std::find(pf.callPrimitives.begin(), pf.callPrimitives.end(), p.primitive) == pf.callPrimitives.end())
					return At(F, x->line, "unknown primitive 'pc:" + p.primitive + "'");
			} else return At(F, x->line, "call/jmp target must be vanilla:0x... or pc:<primitive>");
		}
		const bool inImage = (uint64_t)p.addr >= pf.imageBegin && (uint64_t)p.addr + p.orig.size() <= pf.imageEnd;
		if (!inImage && !(p.kind == Kind::Raw && InDataTable(pf, p.addr, p.orig.size()))) {
			char b[80];
			std::snprintf(b, sizeof b, "patch @0x%08x: outside the patchable image range", p.addr);
			return std::string(F) + " line " + std::to_string(t.line) + ": " + b;
		}
		for (const ReservedRange& r : pf.reserved)
			if (p.addr < r.addr + r.len && r.addr < p.addr + p.orig.size()) {
				char b[160];
				std::snprintf(b, sizeof b, "patch @0x%08x overlaps PovertyCaster's ", p.addr);
				return std::string(F) + " line " + std::to_string(t.line) + ": " + b + r.name;
			}
		out.patches.push_back(std::move(p));
	}
	if (out.patches.empty() || out.patches.size() > 4096) return std::string(F) + ": 1..4096 [[patch]] entries required";
	std::sort(out.patches.begin(), out.patches.end(), [](const Patch& x, const Patch& y) { return x.addr < y.addr; });
	for (size_t i = 1; i < out.patches.size(); ++i)
		if (out.patches[i].addr < out.patches[i - 1].addr + out.patches[i - 1].orig.size()) {
			char b[96];
			std::snprintf(b, sizeof b, "patch @0x%08x overlaps another patch in the same file", out.patches[i].addr);
			return std::string(F) + ": " + b;
		}
	return {};
}

std::string ParseFeatures(const std::string& text, const Profile& pf, Data& out)
{
	static const char* F = "features.toml";
	TDoc d;
	std::string e = ParseToml(text, d);
	if (!e.empty()) return std::string(F) + " " + e;
	if (!(e = CheckFormat(d, F)).empty()) return e;
	for (size_t ti = 1; ti < d.tables.size(); ++ti) {
		const TTable& t = d.tables[ti];
		if (t.name == "feature" && t.array) {
			Feature f;
			const TKv *id = nullptr, *abi = nullptr;
			for (const auto& kv : t.kv) {
				if (kv.key == "id") id = &kv; else if (kv.key == "abi") abi = &kv;
				else return At(F, kv.line, "unknown key '" + kv.key + "'");
			}
			if (!id || id->v.k != TVal::K::Str || !abi || abi->v.k != TVal::K::Int) return At(F, t.line, "id (string) and abi (int) are required");
			f.id = id->v.s;
			f.abi = (int)abi->v.i;
			const FeatureDef* def = nullptr;
			for (const auto& fd : pf.features) if (fd.id == f.id) def = &fd;
			if (!def) return At(F, id->line, "unknown feature '" + f.id + "'");
			if (def->abi != f.abi) return At(F, abi->line, "feature '" + f.id + "' abi " + std::to_string(f.abi) + " != " + std::to_string(def->abi));
			for (const auto& o : out.features) if (o.id == f.id) return At(F, id->line, "feature '" + f.id + "' listed twice");
			out.features.push_back(std::move(f));
		} else if (t.name == "feature.params" && !t.array) {
			if (out.features.empty()) return At(F, t.line, "[feature.params] before any [[feature]]");
			if (ti < 2 || !(d.tables[ti - 1].name == "feature")) return At(F, t.line, "[feature.params] must follow its [[feature]]");
			Feature& f = out.features.back();
			const FeatureDef* def = nullptr;
			for (const auto& fd : pf.features) if (fd.id == f.id) def = &fd;
			for (const auto& kv : t.kv) {
				const ParamDef* pd = nullptr;
				for (const auto& x : def->params) if (x.name == kv.key) pd = &x;
				if (!pd) return At(F, kv.line, "unknown param '" + kv.key + "' for feature '" + f.id + "'");
				if (kv.v.k == TVal::K::Str) return At(F, kv.line, "param '" + kv.key + "': int or bool required");
				const long long v = kv.v.k == TVal::K::Bool ? (kv.v.b ? 1 : 0) : kv.v.i;
				if (v < pd->min || v > pd->max) return At(F, kv.line, "param '" + kv.key + "' outside " + std::to_string(pd->min) + ".." + std::to_string(pd->max));
				f.params.push_back({ kv.key, v, kv.v.k == TVal::K::Bool });
			}
		} else return At(F, t.line, "unknown table '" + t.name + "'");
		if (out.features.size() > 64) return std::string(F) + ": at most 64 features";
	}
	Normalise(out);
	return {};
}

std::string ParseRules(const std::string& text, const Profile& pf, Data& out)
{
	static const char* F = "rules.toml";
	TDoc d;
	std::string e = ParseToml(text, d);
	if (!e.empty()) return std::string(F) + " " + e;
	if (!(e = CheckFormat(d, F)).empty()) return e;
	if (d.tables.size() != 2 || d.tables[1].name != "rules" || d.tables[1].array) return std::string(F) + ": exactly one [rules] table is required";
	for (const auto& kv : d.tables[1].kv) {
		const RuleDef* rd = nullptr;
		for (const auto& x : pf.rules) if (x.name == kv.key) rd = &x;
		if (!rd) return At(F, kv.line, "unknown rule '" + kv.key + "'");
		if (kv.v.k == TVal::K::Str) return At(F, kv.line, "rule '" + kv.key + "': int or bool required");
		const long long v = kv.v.k == TVal::K::Bool ? (kv.v.b ? 1 : 0) : kv.v.i;
		if (v < rd->min || v > rd->max) return At(F, kv.line, "rule '" + kv.key + "' outside " + std::to_string(rd->min) + ".." + std::to_string(rd->max));
		out.rules.push_back({ kv.key, v, kv.v.k == TVal::K::Bool });
		if (out.rules.size() > 256) return std::string(F) + ": at most 256 rules";
	}
	Normalise(out);
	return {};
}

void Normalise(Data& d)
{
	std::stable_sort(d.patches.begin(), d.patches.end(), [](const Patch& x, const Patch& y) { return x.addr < y.addr; });
	std::sort(d.features.begin(), d.features.end(), [](const Feature& a, const Feature& b) { return a.id < b.id; });
	for (auto& f : d.features)
		std::sort(f.params.begin(), f.params.end(), [](const FeatureParam& a, const FeatureParam& b) { return a.key < b.key; });
	std::sort(d.rules.begin(), d.rules.end(), [](const Rule& a, const Rule& b) { return a.name < b.name; });
}

// ---------------------------------------------------------------------------------------------------------------------
// Canonical text (S2.3) -- identical to PC canonical()
// ---------------------------------------------------------------------------------------------------------------------
std::string Canonical(const Data& d)
{
	std::string t = "patches\n";
	for (const Patch& p : d.patches) {
		t += "P" + H8(p.addr) + " " + HexBytes(p.orig) + " ";
		if (p.kind == Kind::Raw) t += "raw " + HexBytes(p.raw);
		else t += std::string(p.kind == Kind::Call ? "call " : "jmp ") + (p.toVanilla ? "vanilla:" + H8(p.target) : "pc:" + p.primitive);
		t += "\n";
	}
	t += "features\n";
	for (const Feature& f : d.features) {
		t += "F" + f.id + " abi=" + std::to_string(f.abi);
		for (const auto& p : f.params) t += " " + p.key + "=" + (p.isBool ? (p.v ? "true" : "false") : std::to_string(p.v));
		t += "\n";
	}
	t += "rules\n";
	for (const Rule& r : d.rules) t += "R" + r.name + "=" + std::to_string(r.v) + "\n";
	return t;
}

// ---------------------------------------------------------------------------------------------------------------------
// Writers
// ---------------------------------------------------------------------------------------------------------------------
static std::string Hex32(uint32_t v) { char b[16]; std::snprintf(b, sizeof b, "0x%08X", v); return b; }
static std::string QuoteToml(const std::string& s)
{
	std::string o = "\"";
	for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
	return o + "\"";
}

std::string WritePatchesToml(const Data& in)
{
	Data d = in;
	Normalise(d);
	std::string o = "# patches.toml -- pcmod schema 2 (written by Hantei-chan)\nformat = 1\n";
	for (const Patch& p : d.patches) {
		o += "\n[[patch]]\naddr = " + Hex32(p.addr) + "\norig = \"" + HexBytesSpaced(p.orig) + "\"\n";
		if (p.kind == Kind::Raw) o += "new = \"" + HexBytesSpaced(p.raw) + "\"\n";
		else {
			o += p.kind == Kind::Call ? "call = " : "jmp = ";
			o += p.toVanilla ? "\"vanilla:" + Hex32(p.target) + "\"\n" : QuoteToml("pc:" + p.primitive) + "\n";
		}
	}
	return o;
}

std::string WriteFeaturesToml(const Data& in)
{
	Data d = in;
	Normalise(d);
	std::string o = "# features.toml -- pcmod schema 2 (written by Hantei-chan)\nformat = 1\n";
	for (const Feature& f : d.features) {
		o += "\n[[feature]]\nid = " + QuoteToml(f.id) + "\nabi = " + std::to_string(f.abi) + "\n";
		if (!f.params.empty()) {
			o += "[feature.params]\n";
			for (const auto& p : f.params) o += p.key + " = " + (p.isBool ? (p.v ? "true" : "false") : std::to_string(p.v)) + "\n";
		}
	}
	return o;
}

std::string WriteRulesToml(const Data& in)
{
	Data d = in;
	Normalise(d);
	std::string o = "# rules.toml -- pcmod schema 2 (written by Hantei-chan)\nformat = 1\n\n[rules]\n";
	for (const Rule& r : d.rules) o += r.name + " = " + (r.isBool ? (r.v ? "true" : "false") : std::to_string(r.v)) + "\n";
	return o;
}

bool PatchBytes(const Patch& p, std::vector<uint8_t>& out)
{
	if (p.kind == Kind::Raw) { out = p.raw; return true; }
	if (!p.toVanilla || p.orig.size() < 5) return false;
	out.assign(p.orig.size(), 0x90);
	out[0] = p.kind == Kind::Call ? 0xE8 : 0xE9;
	const int32_t rel = (int32_t)(p.target - (p.addr + 5));
	std::memcpy(out.data() + 1, &rel, 4);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// PE32
// ---------------------------------------------------------------------------------------------------------------------
static uint32_t Rd32(const std::vector<uint8_t>& b, size_t o) { return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24); }
static uint16_t Rd16(const std::vector<uint8_t>& b, size_t o) { return (uint16_t)(b[o] | (b[o + 1] << 8)); }

bool ParsePe(std::vector<uint8_t> bytes, PeImage& out, std::string& error)
{
	out = PeImage{};
	if (bytes.size() < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') { error = "not an MZ executable"; return false; }
	const uint32_t pe = Rd32(bytes, 0x3C);
	if ((uint64_t)pe + 24 > bytes.size() || std::memcmp(&bytes[pe], "PE\0\0", 4) != 0) { error = "no PE header"; return false; }
	const uint16_t nsec = Rd16(bytes, pe + 6), optSize = Rd16(bytes, pe + 20);
	const size_t opt = pe + 24;
	if (opt + optSize > bytes.size() || optSize < 64 || Rd16(bytes, opt) != 0x10B) { error = "not a PE32 image"; return false; }
	out.imageBase = Rd32(bytes, opt + 28);
	out.sizeOfHeaders = Rd32(bytes, opt + 60);
	const size_t sh = opt + optSize;
	if (sh + (size_t)nsec * 40 > bytes.size()) { error = "truncated section table"; return false; }
	for (uint16_t i = 0; i < nsec; ++i) {
		const size_t s = sh + (size_t)i * 40;
		PeSection sec;
		sec.name.assign((const char*)&bytes[s], strnlen((const char*)&bytes[s], 8));
		sec.vsize = Rd32(bytes, s + 8);
		sec.va = out.imageBase + Rd32(bytes, s + 12);
		sec.rawSize = Rd32(bytes, s + 16);
		sec.rawOff = Rd32(bytes, s + 20);
		sec.chars = Rd32(bytes, s + 36);
		if (!sec.vsize) sec.vsize = sec.rawSize;
		out.sections.push_back(sec);
	}
	out.bytes = std::move(bytes);
	return true;
}

const PeSection* PeImage::SectionAt(uint32_t va) const
{
	for (const PeSection& s : sections) if (va >= s.va && (uint64_t)va < (uint64_t)s.va + s.vsize) return &s;
	return nullptr;
}

bool PeImage::Read(uint32_t va, size_t len, std::vector<uint8_t>& out) const
{
	out.assign(len, 0);
	const PeSection* s = SectionAt(va);
	if (!s || (uint64_t)va + len > (uint64_t)s->va + s->vsize) return false;
	for (size_t i = 0; i < len; ++i) {
		const uint32_t off = va - s->va + (uint32_t)i;
		if (off < s->rawSize && (size_t)s->rawOff + off < bytes.size()) out[i] = bytes[s->rawOff + off];
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Verification
// ---------------------------------------------------------------------------------------------------------------------
const char* IssueName(IssueCode c)
{
	switch (c) {
	case IssueCode::OrigMismatch: return "orig-mismatch";
	case IssueCode::OutOfRange: return "out-of-range";
	case IssueCode::PeHeader: return "pe-header";
	case IssueCode::Resources: return "resources";
	case IssueCode::CallTooShort: return "call-too-short";
	case IssueCode::CallTargetNotCode: return "call-target-not-code";
	case IssueCode::Overlap: return "overlap";
	case IssueCode::Reserved: return "reserved";
	case IssueCode::LengthMismatch: return "length-mismatch";
	case IssueCode::BadLength: return "bad-length";
	}
	return "?";
}

std::vector<Issue> VerifyPatches(const Data& in, const PeImage& exe, const Profile& pf)
{
	std::vector<Issue> out;
	Data d = in;
	Normalise(d);
	auto add = [&](IssueCode c, uint32_t a, const std::string& m) { out.push_back({ c, a, "patch @0x" + H8(a) + ": " + m }); };
	for (size_t i = 0; i < d.patches.size(); ++i) {
		const Patch& p = d.patches[i];
		const size_t len = p.orig.size();
		if (len == 0 || len > 32) { add(IssueCode::BadLength, p.addr, "length 0 or > 32"); continue; }
		if (p.kind == Kind::Raw && p.raw.size() != len) add(IssueCode::LengthMismatch, p.addr, "orig and new differ in length");
		if (p.kind != Kind::Raw && len < 5) add(IssueCode::CallTooShort, p.addr, "call/jmp needs orig of at least 5 bytes (found " + std::to_string(len) + ")");
		// Range: the section by name, then the profile's patchable range / .data allow-list.
		const PeSection* s = exe.SectionAt(p.addr);
		const PeSection* e = len ? exe.SectionAt(p.addr + (uint32_t)len - 1) : s;
		if (p.addr < exe.imageBase + exe.sizeOfHeaders && p.addr >= exe.imageBase) add(IssueCode::PeHeader, p.addr, "inside the PE header (file-on-disk only; refused)");
		else if (s && s->name == ".rsrc") add(IssueCode::Resources, p.addr, "inside .rsrc (file-on-disk only; refused)");
		else if (!s || s != e) add(IssueCode::OutOfRange, p.addr, "outside the patchable image range (not inside one section)");
		else {
			const bool codeOrRo = s->name == ".text" || s->name == ".rdata";
			const bool inImage = p.addr >= pf.imageBegin && (uint64_t)p.addr + len <= pf.imageEnd;
			bool table = false;
			for (const DataTable& t : pf.dataTables) if (p.addr >= t.begin && (uint64_t)p.addr + len <= t.end && p.kind == Kind::Raw) table = true;
			if (!((codeOrRo && inImage) || table))
				add(IssueCode::OutOfRange, p.addr, "outside the patchable image range (section " + s->name + (s->name == ".data" ? ", not an allow-listed table" : "") + ")");
		}
		for (const ReservedRange& r : pf.reserved)
			if (p.addr < r.addr + r.len && r.addr < p.addr + len) add(IssueCode::Reserved, p.addr, "overlaps PovertyCaster's " + r.name);
		// orig vs the vanilla exe
		std::vector<uint8_t> have;
		if (!exe.Read(p.addr, len, have)) add(IssueCode::OutOfRange, p.addr, "not mapped in the executable");
		else if (have != p.orig) add(IssueCode::OrigMismatch, p.addr, "orig does not match the vanilla executable (found " + HexBytesSpaced(have) + ")");
		// call/jmp target
		if (p.kind != Kind::Raw && p.toVanilla) {
			const PeSection* ts = exe.SectionAt(p.target);
			if (!ts || ts->name != ".text" || p.target < pf.textBegin || p.target >= pf.textEnd)
				add(IssueCode::CallTargetNotCode, p.addr, "call target 0x" + H8(p.target) + " is not vanilla code");
		}
		// overlaps inside the package (sorted: compare with every later patch that starts before our end)
		for (size_t j = i + 1; j < d.patches.size() && d.patches[j].addr < p.addr + len; ++j)
			add(IssueCode::Overlap, d.patches[j].addr, "overlaps the patch @0x" + H8(p.addr));
	}
	return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Package level
// ---------------------------------------------------------------------------------------------------------------------
std::string AttachToManifest(Manifest& m, const Data& in, const Profile& pf, Files& out)
{
	out = Files{};
	m.schema = 2;
	m.exePatches = m.features = m.rules = AuxFile{};
	Data d = in;
	Normalise(d);
	if (!d.patches.empty()) out.patches = WritePatchesToml(d);
	if (!d.features.empty()) out.features = WriteFeaturesToml(d);
	if (!d.rules.empty()) out.rules = WriteRulesToml(d);
	auto ref = [](AuxFile& a, const char* path, const std::string& text) {
		if (text.empty()) return;
		a.present = true; a.path = path; a.sha256 = Sha256Hex(text); a.size = text.size();
	};
	ref(m.exePatches, "patches.toml", out.patches);
	ref(m.features, "features.toml", out.features);
	ref(m.rules, "rules.toml", out.rules);
	if (m.HasAux()) m.cls = PackageClass::Sim;   // S2.1: any of the three makes the package sim
	Data back;
	return LoadFromTexts(m, out, pf, back);
}

std::string LoadFromTexts(Manifest& m, const Files& t, const Profile& pf, Data& out)
{
	out = Data{};
	m.s2Canon.clear();
	const AuxFile* a[3] = { &m.exePatches, &m.features, &m.rules };
	const std::string* x[3] = { &t.patches, &t.features, &t.rules };
	for (int i = 0; i < 3; ++i) {
		if (!a[i]->present) continue;
		if (x[i]->size() != a[i]->size || Sha256Hex(*x[i]) != a[i]->sha256) return m.id + ": " + a[i]->path + " differs from its manifest";
	}
	std::string e;
	if (m.exePatches.present && !(e = ParsePatches(t.patches, pf, out)).empty()) return e;
	if (m.features.present && !(e = ParseFeatures(t.features, pf, out)).empty()) return e;
	if (m.rules.present && !(e = ParseRules(t.rules, pf, out)).empty()) return e;
	Normalise(out);
	m.s2Canon = Canonical(out);
	return {};
}

} // namespace pcmod::s2
