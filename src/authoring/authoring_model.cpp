// [authoring] the pure model — see authoring_model.h.
#include "authoring_model.h"
#include "roster_mirror.h"
#include "../tag_tuning/tag_levers.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace authoring {

namespace {
std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }
std::string Str(const char* p, size_t cap) { return std::string(p, strnlen(p, cap)); }
const char* kMoonLetter = "CFH";
} // namespace

// ================== roster ==================

std::vector<RosterChar> MirrorRoster(bool modData)
{
	std::vector<RosterChar> out;
	for (const RosterMirrorRow& r : kRosterMirror) {
		RosterChar c;
		c.chara = r.chara;
		c.selector = r.selector;
		c.name = r.name;
		c.file1 = r.file1;
		c.file2 = r.file2;
		c.fullName = r.fullName;
		const bool duo = r.file2[0] != 0 || r.tagType != 0;
		const tagtune::TeamChangeRow* row = tagtune::FindTeamChangeRow(r.file1);
		const bool inNeedsMod = row && row->inMod && row->inFallback <= 0;
		const bool outNeedsMod = row && row->outMod;
		if (duo) c.flags |= wire::kRosterDuo;
		if (row) c.flags |= wire::kRosterHasTcRow;
		if (row && (inNeedsMod || outNeedsMod)) c.flags |= wire::kRosterNeedsMod;
		if (!duo && row && (modData || !(inNeedsMod || outNeedsMod))) c.flags |= wire::kRosterTagOk;
		if (!duo) c.flags |= wire::kRosterTeamOk;
		out.push_back(std::move(c));
	}
	return out;
}

std::vector<RosterChar> RosterFromLink(const std::vector<wire::Roster>& pages)
{
	std::vector<RosterChar> out;
	for (const wire::Roster& p : pages)
		for (int i = 0; i < p.count && i < 16; ++i) {
			const wire::RosterEntry& e = p.e[i];
			RosterChar c;
			c.chara = e.chara;
			c.selector = e.selector;
			c.flags = e.flags;
			c.file1 = Lower(Str(e.file1, sizeof e.file1));
			c.file2 = Lower(Str(e.file2, sizeof e.file2));
			c.name = Str(e.name, sizeof e.name);
			for (const RosterMirrorRow& r : kRosterMirror) if (r.chara == c.chara) c.fullName = r.fullName;
			if (c.fullName.empty()) c.fullName = c.name;
			out.push_back(std::move(c));
		}
	return out;
}

const RosterChar* FindChara(const std::vector<RosterChar>& r, int chara)
{
	for (const RosterChar& c : r) if (c.chara == chara) return &c;
	return nullptr;
}

const RosterChar* FindFile(const std::vector<RosterChar>& r, const std::string& file1)
{
	for (const RosterChar& c : r) if (c.file1 == Lower(file1) && !c.Duo()) return &c;
	for (const RosterChar& c : r) if (c.file1 == Lower(file1)) return &c;
	return nullptr;
}

const char* ModeName(Mode m) { return m == Mode::Versus ? "1v1" : m == Mode::Tag ? "TAG" : "TEAM"; }

std::string BanReason(const RosterChar& c, Mode m)
{
	if (m == Mode::Versus) return {};
	if (c.flags & wire::kRosterDuo) return "a duo (two fighters in one slot) cannot play TAG / TEAM";
	if (m == Mode::Team) return (c.flags & wire::kRosterTeamOk) ? std::string() : "not allowed in TEAM";
	if (c.flags & wire::kRosterTagOk) return {};
	if (!(c.flags & wire::kRosterHasTcRow)) return "no tag-in / tag-out data (no TeamChangeData row)";
	return "its tag patterns exist only in tag_mod data, which this game does not load";
}

const char* SlotRoleName(int slot)
{
	static const char* n[] = { "P1 point", "P2 point", "P1 partner", "P2 partner" };
	return slot >= 0 && slot < 4 ? n[slot] : "?";
}

// ================== setup <-> wire ==================

bool Setup::operator==(const Setup& o) const
{
	const wire::MatchSetup a = ToWire(*this), b = ToWire(o);
	return std::memcmp(&a, &b, sizeof a) == 0 && style == o.style;
}

wire::MatchSetup ToWire(const Setup& s)
{
	wire::MatchSetup w{};
	w.version = wire::kMatchSetupVersion;
	w.mode = (uint8_t)s.mode;
	w.scene = s.scene;
	w.flags = (s.assists ? wire::kSetupAssists : 0) | (s.keepBgm ? wire::kSetupKeepBgm : 0) | (s.force ? wire::kSetupForce : 0) |
	          (s.resetPos ? wire::kSetupResetPos : 0) | (s.hotOnly ? wire::kSetupHotOnly : 0) |
	          (s.tuningFirst ? wire::kSetupTuningFirst : 0);
	w.stage = (int16_t)s.stage;
	w.koRule = (uint8_t)s.koRule;
	w.timer = (uint8_t)s.timer;
	for (int i = 0; i < 4; ++i) {
		const SlotPick& p = s.slot[i];
		w.slot[i].chara = p.Empty() ? (int16_t)-1 : (int16_t)p.chara;
		w.slot[i].moon = p.Empty() ? 0 : (uint8_t)p.moon;
		w.slot[i].palette = p.Empty() ? 0 : (uint8_t)p.palette;
	}
	for (int side = 0; side < 2; ++side)
		for (int d = 0; d < 5; ++d) w.assist[side][d] = s.mode == Mode::Tag ? s.assist[side][d] : 0;
	return w;
}

Setup FromWire(const wire::MatchSetup& w)
{
	Setup s;
	s.mode = w.mode <= 2 ? (Mode)w.mode : Mode::Versus;
	s.scene = w.scene;
	s.assists = (w.flags & wire::kSetupAssists) != 0;
	s.keepBgm = (w.flags & wire::kSetupKeepBgm) != 0;
	s.force = (w.flags & wire::kSetupForce) != 0;
	s.resetPos = (w.flags & wire::kSetupResetPos) != 0;
	s.hotOnly = (w.flags & wire::kSetupHotOnly) != 0;
	s.tuningFirst = (w.flags & wire::kSetupTuningFirst) != 0;
	s.stage = w.stage;
	s.koRule = w.koRule;
	s.timer = w.timer;
	for (int i = 0; i < 4; ++i) {
		s.slot[i].chara = w.slot[i].chara;
		s.slot[i].moon = s.slot[i].chara < 0 ? 0 : w.slot[i].moon;
		s.slot[i].palette = s.slot[i].chara < 0 ? 0 : w.slot[i].palette;
	}
	std::memcpy(s.assist, w.assist, sizeof s.assist);
	return s;
}

std::string ToHex(const wire::MatchSetup& w)
{
	static const char* d = "0123456789abcdef";
	const uint8_t* p = (const uint8_t*)&w;
	std::string h;
	for (size_t i = 0; i < sizeof w; ++i) { h += d[p[i] >> 4]; h += d[p[i] & 15]; }
	return h;
}

bool FromHex(const std::string& hex, wire::MatchSetup& out)
{
	if (hex.size() != 2 * sizeof out) return false;
	uint8_t* p = (uint8_t*)&out;
	auto nib = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return 10 + c - 'a';
		if (c >= 'A' && c <= 'F') return 10 + c - 'A';
		return -1;
	};
	for (size_t i = 0; i < sizeof out; ++i) {
		const int a = nib(hex[2 * i]), b = nib(hex[2 * i + 1]);
		if (a < 0 || b < 0) return false;
		p[i] = (uint8_t)(a << 4 | b);
	}
	return true;
}

std::string Describe(const Setup& s, const std::vector<RosterChar>& roster)
{
	auto pick = [&](int slot) -> std::string {
		const SlotPick& p = s.slot[slot];
		if (p.Empty()) return "-";
		const RosterChar* c = FindChara(roster, p.chara);
		char b[64];
		std::snprintf(b, sizeof b, "%s %c%d", c ? c->name.c_str() : ("#" + std::to_string(p.chara)).c_str(),
		              p.moon >= 0 && p.moon < 3 ? kMoonLetter[p.moon] : '?', p.palette);
		return b;
	};
	std::string t = ModeName(s.mode);
	t += " " + pick(0);
	if (s.mode != Mode::Versus && !s.slot[2].Empty()) t += " + " + pick(2);
	t += " vs " + pick(1);
	if (s.mode != Mode::Versus && !s.slot[3].Empty()) t += " + " + pick(3);
	t += s.stage > 0 ? ", stage " + std::to_string(s.stage) : s.stage < 0 ? ", random stage" : ", same stage";
	if (s.mode == Mode::Tag) t += s.assists ? ", assists" : ", no assists";
	return t;
}

// ================== validation ==================

std::vector<Problem> Validate(const Setup& s, const std::vector<RosterChar>& roster, const ValidateContext& ctx)
{
	std::vector<Problem> out;
	auto add = [&out](int slot, wire::Status st, const std::string& m) { out.push_back({ slot, (int16_t)st, m }); };
	const bool need[4] = { true, true, s.mode == Mode::Team, s.mode == Mode::Team };
	const bool allow[4] = { true, true, s.mode != Mode::Versus, s.mode != Mode::Versus };
	for (int i = 0; i < 4; ++i) {
		const SlotPick& p = s.slot[i];
		if (p.Empty()) {
			if (need[i]) add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": pick a character");
			continue;
		}
		if (!allow[i]) { add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": 1v1 has no partners"); continue; }
		const RosterChar* c = FindChara(roster, p.chara);
		if (!c) { add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": character #" + std::to_string(p.chara) + " is not in the roster"); continue; }
		if (p.moon < 0 || p.moon > 2) add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": moon must be crescent / full / half");
		if (p.palette < 0 || p.palette > 255) add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": palette out of range");
		else if (ctx.paletteCount) {
			const int n = ctx.paletteCount(c->file1);
			if (n >= 0 && p.palette >= n)
				add(i, wire::Status::BadArgs, std::string(SlotRoleName(i)) + ": palette " + std::to_string(p.palette) + " >= " +
				                                  c->file1 + ".pal's " + std::to_string(n));
		}
		const std::string ban = BanReason(*c, s.mode);
		if (!ban.empty()) add(i, wire::Status::Ineligible, std::string(SlotRoleName(i)) + ": " + c->name + " - " + ban);
		if (ctx.txtExists && !s.force && p.moon >= 0 && p.moon <= 2) {
			if (!ctx.txtExists(c->file1, p.moon))
				add(i, wire::Status::MissingFile, "data\\" + c->file1 + "_" + std::to_string(p.moon) + ".txt missing (packed data?)");
			if (!c->file2.empty() && !ctx.txtExists(c->file2, p.moon))
				add(i, wire::Status::MissingFile, "data\\" + c->file2 + "_" + std::to_string(p.moon) + ".txt missing (packed data?)");
		}
	}
	if (s.mode == Mode::Team && !ctx.team4p)
		add(-1, wire::Status::NeedsRestart, "TEAM needs 4 input slots from boot: relaunch with 4 players (PCHOST_MBAACC_2V2=1)");
	if (s.scene == (uint8_t)wire::Scene::Training && s.mode != Mode::Versus)
		add(-1, wire::Status::Unsupported, "Training is 1v1 only for now (TAG / TEAM use Authoring VS; §7 Q1)");
	if (s.scene == (uint8_t)wire::Scene::Training && !ctx.trainingScene)
		add(-1, wire::Status::Unsupported, "this pchost.dll cannot reach the Training scene");
	if (s.scene > 2) add(-1, wire::Status::BadArgs, "unknown scene");
	if (s.stage < -1 || s.stage > 99) add(-1, wire::Status::BadArgs, "stage must be 1..99, 0 (keep) or -1 (random)");
	else if (s.stage > 0 && ctx.stageExists && !ctx.stageExists(s.stage))
		add(-1, wire::Status::BadArgs, "stage " + std::to_string(s.stage) + " has no BgList.ini entry");
	if (s.koRule != 0 && s.koRule != 1 && s.koRule != 0xFF) add(-1, wire::Status::BadArgs, "KO rule must be oneDown, allDown or the tuning's");
	if (s.timer != 0 && s.timer != 1 && s.timer != 2 && s.timer != 4 && s.timer != 0xFF) add(-1, wire::Status::BadArgs, "timer must be 0, 1, 2, 4 or default");
	for (int side = 0; side < 2; ++side)
		for (int d = 0; d < 5; ++d)
			if (s.assist[side][d] > kAssistMotionCount) add(-1, wire::Status::BadArgs, "assist choice out of range");
	return out;
}

// ================== files ==================

std::vector<OpenTarget> FilesToOpen(const Setup& s, const std::vector<RosterChar>& roster, const std::string& gameDir)
{
	std::vector<OpenTarget> out;
	auto add = [&](int slot, const std::string& file, int moon, bool second) {
		if (file.empty()) return;
		for (const OpenTarget& t : out) if (t.file == file && t.moon == moon) return;
		OpenTarget t;
		t.slot = slot;
		t.file = file;
		t.moon = moon;
		t.point = slot < 2;
		t.second = second;
		t.txtPath = gameDir + "\\data\\" + file + "_" + std::to_string(moon) + ".txt";
		out.push_back(t);
	};
	for (int slot : { 0, 2, 1, 3 }) {   // P1 point first (it gets the focus), then its partner, then P2
		const SlotPick& p = s.slot[slot];
		if (p.Empty() || (s.mode == Mode::Versus && slot >= 2)) continue;
		const RosterChar* c = FindChara(roster, p.chara);
		if (!c) continue;
		add(slot, c->file1, p.moon, false);
		if (s.mode == Mode::Versus && !c->file2.empty()) add(slot, c->file2, p.moon, true);
	}
	return out;
}

int PaletteCountOf(const std::string& palPath)
{
	std::ifstream f(palPath, std::ios::binary);
	if (!f) return -1;
	uint32_t n = 0;
	if (!f.read((char*)&n, 4)) return -1;
	if (n == 0xFFFFu) {   // UNI2 / MBTL layout (never MBAACC, but do not misread it)
		uint32_t h[3] = {};
		if (!f.read((char*)h, sizeof h)) return -1;
		n = h[2];
	}
	return (int)std::min<uint32_t>(n, 256);
}

// ================== launch ==================

std::vector<std::pair<std::string, std::string>> LaunchEnv(const Setup& s)
{
	std::vector<std::pair<std::string, std::string>> v = {
		{ "PCHOST_GAME", "mbaacc" },
		{ "PCHOST_MBAACC_LINK", "1" },
		{ "PCHOST_MBAACC_AUTHORING", "1" },
		{ "PCHOST_MBAACC_AUTHORING_SETUP", ToHex(ToWire(s)) },
		{ "PCHOST_LOG_TAG", "authoring" },
	};
	if (s.mode == Mode::Team) v.push_back({ "PCHOST_MBAACC_2V2", "1" });
	return v;
}

std::wstring BuildEnvBlock(const std::vector<std::pair<std::string, std::string>>& vars, const wchar_t* parent)
{
	auto upper = [](std::wstring s) { for (wchar_t& c : s) if (c >= L'a' && c <= L'z') c = (wchar_t)(c - L'a' + L'A'); return s; };
	std::map<std::wstring, std::wstring> byName;   // upper-case name -> "Name=value"
	std::vector<std::wstring> drive;               // "=C:=C:\x" entries stay first
	if (parent)
		for (const wchar_t* p = parent; *p; p += wcslen(p) + 1) {
			std::wstring e = p;
			if (e[0] == L'=') { drive.push_back(e); continue; }
			const size_t eq = e.find(L'=');
			byName[upper(e.substr(0, eq))] = e;
		}
	for (const auto& kv : vars) {
		std::wstring n(kv.first.begin(), kv.first.end()), v(kv.second.begin(), kv.second.end());
		byName[upper(n)] = n + L"=" + v;
	}
	std::wstring block;
	for (const std::wstring& d : drive) { block += d; block += L'\0'; }
	for (const auto& kv : byName) { block += kv.second; block += L'\0'; }
	block += L'\0';
	return block;
}

// ================== saved setups ==================

void SetupLibrary::Save(const Setup& s, const std::string& name)
{
	Setup c = s;
	c.name = name;
	for (Setup& x : named) if (x.name == name) { x = c; return; }
	named.push_back(c);
	std::sort(named.begin(), named.end(), [](const Setup& a, const Setup& b) { return a.name < b.name; });
}

bool SetupLibrary::Remove(const std::string& name)
{
	auto it = std::find_if(named.begin(), named.end(), [&](const Setup& x) { return x.name == name; });
	if (it == named.end()) return false;
	named.erase(it);
	return true;
}

void SetupLibrary::PushRecent(const Setup& s)
{
	Setup c = s;
	c.name.clear();
	recent.erase(std::remove_if(recent.begin(), recent.end(), [&](const Setup& x) { return x == c; }), recent.end());
	recent.insert(recent.begin(), c);
	if ((int)recent.size() > recentCap) recent.resize(recentCap);
}

const Setup* SetupLibrary::Find(const std::string& name) const
{
	for (const Setup& x : named) if (x.name == name) return &x;
	return nullptr;
}

std::string SetupLibrary::Serialize() const
{
	std::string t = "; Hantei-chan Authoring: saved setups (hex = the 64-byte LinkMatchSetup, docs/HANTEI_AUTHORING_MODE.md §3.3)\n";
	for (const Setup& s : named) t += "[setup]\nname=" + s.name + "\nhex=" + ToHex(ToWire(s)) + "\nstyle=" + s.style + "\n";
	for (const Setup& s : recent) t += "[recent]\nhex=" + ToHex(ToWire(s)) + "\nstyle=" + s.style + "\n";
	return t;
}

bool SetupLibrary::Parse(const std::string& text)
{
	named.clear();
	recent.clear();
	std::istringstream in(text);
	std::string line, section;
	Setup cur;
	bool have = false, ok = true;
	auto flush = [&]() {
		if (!have) return;
		if (section == "setup" && !cur.name.empty()) named.push_back(cur);
		else if (section == "recent") recent.push_back(cur);
		have = false;
	};
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty() || line[0] == ';') continue;
		if (line[0] == '[') { flush(); section = line.substr(1, line.find(']') - 1); cur = Setup{}; continue; }
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
		if (k == "name") cur.name = v;
		else if (k == "style") cur.style = v;
		else if (k == "hex") {
			wire::MatchSetup w{};
			if (FromHex(v, w)) { const std::string n = cur.name, st = cur.style; cur = FromWire(w); cur.name = n; cur.style = st; have = true; }
			else ok = false;
		}
	}
	flush();
	if ((int)recent.size() > recentCap) recent.resize(recentCap);
	return ok;
}

// ================== per-game tables ==================

const GameTable* FindGameTable(const std::string& gameId)
{
	static const GameTable kMbaacc = { "mbaacc", tagtune::LeverTableHash(), (int)tagtune::kLeverCount, tagtune::PerCharLeverCount() };
	if (gameId.empty() || gameId == "mbaacc") return &kMbaacc;
	return nullptr;
}

} // namespace authoring
