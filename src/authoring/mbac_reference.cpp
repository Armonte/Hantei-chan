// [authoring] MBAC reference values — see mbac_reference.h.
#include "mbac_reference.h"
#include "roster_mirror.h"
#include "../framedata.h"
#include "../framedata_ha4.h"
#include "../tag_tuning/tag_levers.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace authoring {

namespace {

bool ReadAll(const std::string& path, std::string& out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream s;
	s << f.rdbuf();
	out = s.str();
	return true;
}

// "[TeamChangeData]" then the first line holding two integers: "<tagIn> <tagOut>".
bool ParseTeamChange(const std::string& text, int& in, int& out)
{
	std::string low = text;
	for (char& c : low) c = (char)std::tolower((unsigned char)c);
	const size_t at = low.find("[teamchangedata]");
	if (at == std::string::npos) return false;
	std::istringstream s(text.substr(at + 16));
	std::string line;
	while (std::getline(s, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t b = 0;
		while (b < line.size() && (line[b] == ' ' || line[b] == '\t')) ++b;
		if (b >= line.size()) continue;
		if (line[b] == '[') return false;   // next section: no row
		if (line.compare(b, 2, "//") == 0 || line[b] == ';') continue;
		char* e1 = nullptr;
		const long a = std::strtol(line.c_str() + b, &e1, 10);
		if (e1 == line.c_str() + b) return false;
		char* e2 = nullptr;
		const long c = std::strtol(e1, &e2, 10);
		if (e2 == e1) return false;
		in = (int)a;
		out = (int)c;
		return true;
	}
	return false;
}

MbacCharRef Build(const std::string& dir, int chara)
{
	MbacCharRef r;
	r.stem = MbacStemForChara(chara);
	if (r.stem.empty()) { r.error = "no MBAC counterpart"; return r; }
	std::string text;
	const std::string ctxt = dir + "\\" + r.stem + "_C.TXT";
	if (!ReadAll(ctxt, text)) { r.error = "cannot read " + ctxt; return r; }
	if (!ParseTeamChange(text, r.tagIn, r.tagOut)) { r.error = "no [TeamChangeData] row in " + ctxt; return r; }
	r.ok = true;
	FrameData fd;
	std::string err;
	const std::string dat = dir + "\\" + r.stem + ".DAT";
	if (ha4::LoadFile(fd, dat.c_str(), &err)) {
		r.in = SummarizePattern(fd, r.tagIn);
		r.out = SummarizePattern(fd, r.tagOut);
	} else {
		r.error = "tag patterns unavailable: cannot load " + dat + (err.empty() ? std::string() : ": " + err);
	}
	return r;
}

} // namespace

std::string DefaultMbacDir() { return "C:\\games\\MB\\AC\\install\\MBACPC\\02_extracted"; }

const MbacCharRef& MbacReferenceFor(const std::string& dir, int chara)
{
	static std::mutex mx;
	static std::map<std::pair<std::string, int>, MbacCharRef> cache;
	std::lock_guard<std::mutex> lk(mx);
	const auto key = std::make_pair(dir, chara);
	auto it = cache.find(key);
	if (it == cache.end()) it = cache.emplace(key, Build(dir, chara)).first;
	return it->second;
}

const std::vector<MbacGlobalRef>& MbacGlobalReference()
{
	// Every value is cited from docs/tag_research/MBACPC_TAG_MECHANICS.md (mbacPC.exe RE) unless noted.
	static const std::vector<MbacGlobalRef> v = {
		{ "cooldownTicks", 120, "mbacPC TagTeam_UpdateReserveCharacter 0x437b70: state 101 until team timer > 120" },
		{ "regen", 1, "mbacPC Character_UpdateHealthRegenAndMeterMode 0x436af0: resting partner health = min(health+1, red)" },
		{ "regenPerTick", 1, "mbacPC 0x436af0: +1 health per frame" },
		{ "parkX", -65536, "mbacPC TagTeam_UpdateReserveCharacter 0x437b70: reserve parked at x = -65536 (states 0 / 101)" },
		{ "koRule", 0, "mbacPC Round_CheckPointCharacterKOs 0x450290: the point's KO ends the round (oneDown)" },
		{ "airTagOut", 0, "mbacPC TagTeam_CheckTagCommand22D 0x4385a0: 22D on a cancelable GROUNDED frame only" },
		{ "entryInvulnTicks", 0, "TAG_TUNING_GUIDE.md entryInvulnTicks: MBAC has no entry invulnerability" },
		{ "swapRedKeepPct", 50, "mbacPC TagTeam_ProcessSwap 0x435830: the incoming's red-health gap is halved" },
	};
	return v;
}

const MbacGlobalRef* FindMbacGlobal(const std::string& key)
{
	for (const MbacGlobalRef& r : MbacGlobalReference())
		if (tagtune::ieq(r.key, key)) return &r;
	return nullptr;
}

} // namespace authoring
