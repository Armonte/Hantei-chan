// game_link_cli — the Game Link client without the GUI: the SAME gamelink::Client the editor's panel runs, plus
// an edit command that goes through the editor's own HA6 loader/saver (FrameData, the .txt stack exactly as
// LoadFromIni loads it, saved to the same top HA6 Hantei-chan's "Save Character" writes). Used to verify the
// link end to end from a script (docs/HANTEI_GAME_LINK.md §Verification) and handy for build scripts.
//
//   game_link_cli state [count] [intervalMs]         print the per-slot state
//   game_link_cli ping
//   game_link_cli reload [slotMask] [force]          reload and wait for the verdict
//   game_link_cli setchar <slot> <chara> <moon> <palette>   pick + reload
//   game_link_cli sample <slot> <pattern> <frame> <ms>      max ticks the slot spent in pattern/frame while sampling
//   game_link_cli watch <file> <key> <timeoutMs>     auto-reload-on-save path: wait for <file> to change, then the
//                                                    reload the watcher sends, and print its verdict
//   game_link_cli ha6-get <chara.txt> <pattern> <frame>
//   game_link_cli ha6-set-duration <chara.txt> <pattern> <frame> <duration>   edit + save through FrameData
//
// Stage ops (docs/HANTEI_STAGE_LINK.md):
//   game_link_cli stage                              print the stage the game shows
//   game_link_cli setstage <id> [keepbgm] [list]     switch the live stage and wait for the verdict
//   game_link_cli reloadstage [list]                 re-read the current stage from disk
//   game_link_cli stage-watch <file> <timeoutMs>     stage auto-reload-on-save path: wait for <file> to change, then
//                                                    the ReloadStage the watcher sends, and print its verdict
//   game_link_cli bg-get <bgNN.dat> <object> <frame>
//   game_link_cli bg-set <bgNN.dat> <object> <frame> <field> <value>   edit + save through bg::File (the stage
//                                                    editor's own loader/saver); field = duration | offsetX |
//                                                    offsetY | opacity  (bg-set-duration <..> <value> = duration)
//
// Tag / Team panel (docs/HANTEI_TAG_PANEL.md):
//   game_link_cli tag                                print the QueryTag readout (or "unsupported": pchost.dll older than mbaacc/link-tag)
//   game_link_cli mock-dll <seconds> [notag] [session] [menu]   serve a MOCK dev-link pipe as this process (a fixed
//                                                    TAG state + LinkTag) for headless UI captures;
//                                                    point the editor at it with HANTEI_GAME_LINK_PID=<printed pid>
//
// Authoring Mode (docs/HANTEI_AUTHORING_MODE.md §6.3 H10):
//   game_link_cli caps                               QueryCaps (or "rev 1": pchost.dll predates Authoring Mode)
//   game_link_cli roster                             the game's roster (QueryRoster pages)
//   game_link_cli setup-get                          QueryMatchSetup: phase, auth state, requested / in force
//   game_link_cli setup-set <mode> <p1> <p2> [p3 p4] [stage N] [scene auto|training|vs] [assists on|off] [hot]
//                           [ko one|all] [timer N] [force] [keepbgm] [resetpos] [notuning] [assist <side> <dir> <choice>]
//                                                    mode = 1v1|tag|team; a pick = <name|id>/<c|f|h|0-2>/<palette>,
//                                                    e.g. tohno/c/3; p3 / p4 = the P1 / P2 partners (engine slots 2/3);
//                                                    waits for the one reply (up to 25 s) and prints the new state
//   game_link_cli setup-hex <same args as setup-set> print the 128-digit PCHOST_MBAACC_AUTHORING_SETUP value
//   game_link_cli tuning-apply [reload]              ApplyTuning (+ QueryAfter) and print the global + slot values
//   game_link_cli tuning-get [slot]                  QueryTuning (all slots, or one)
//   game_link_cli end-authoring
//   game_link_cli tag-migrate <gamedir> [--dry-run]  tag_tuning.ini -> povertycaster\tag\local\ (§2.7 / §9.1)
//   game_link_cli launch <gamedir> <setup args>      pc_inject + pchost with the §3.7 environment, then wait for the
//                                                    link and the setup to reach Ready (prints the game pid)
//   game_link_cli mock-dll <s> [notag] [session] [menu] [rev1] [host] [between] [team4p] [paused]
//                           [root <povertycaster\tag dir>] [skew <lever>] [hash <hex>] [env <lever>] [hot <ms>] [cold <ms>]
//
// --pid <n> (before the command) or HANTEI_GAME_LINK_PID=<n>: talk ONLY to that MBAA.exe; no discovery by name.
#include "game_link.h"
#include "game_link_mock.h"
#include "framedata.h"
#include "background/bg_file.h"
#include "authoring/authoring_model.h"
#include "authoring/game_launcher.h"
#include "authoring/roster_mirror.h"
#include "tag_tuning/tag_migrate.h"
#include "tag_tuning/tag_levers.h"
#include "game_frame_ring.h"

#include <windows.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace gamelink;

namespace {

void PrintState(const wire::State& s)
{
	std::printf("scene=%u timer=%u gameModeKind=0x%X reloads=%u allowed=%u tag=%u teams[act %d/%d req %d/%d]\n",
	            s.scene, s.worldTimer, s.gameModeKind, s.reloadCount, s.reloadAllowed, s.tagLive, s.teamActive[0],
	            s.teamActive[1], s.teamTagRequest[0], s.teamTagRequest[1]);
	for (int i = 0; i < 4; ++i) {
		const wire::Actor& a = s.actors[i];
		if (!a.exists) { std::printf("  P%d (empty)\n", i + 1); continue; }
		std::printf("  P%d %-10s chara=%d moon=%d pal=%d pat=%d frame=%d ticks=%d/%d x=%d y=%d team=%u tag=%u partner=%d\n",
		            i + 1, a.file, a.chara, a.moon, a.palette, a.pattern, a.frame, a.frameTicks, a.patternTicks, a.x,
		            a.y, a.team, a.tagFlag, a.partnerSlot == 0xFF ? -1 : a.partnerSlot);
	}
}

bool Connect(Client& c, int timeoutMs = 5000)
{
	c.Connect();
	for (int t = 0; t < timeoutMs; t += 20) {
		if (c.Get().connected) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	std::fprintf(stderr, "not connected: %s\n", c.Get().status.c_str());
	return false;
}

int WaitAndPrintReply(Client& c, uint16_t seq, int timeoutMs)
{
	wire::Reply r;
	if (!c.WaitReply(seq, timeoutMs, r)) { std::fprintf(stderr, "no reply to #%u within %d ms\n", seq, timeoutMs); return 3; }
	std::printf("reply #%u: %s (%d) - %s [game reloads %u]\n", r.seq, wire::StatusName(r.status), r.status, r.message,
	            r.reloadCount);
	return r.status == (int16_t)wire::Status::Ok ? 0 : 4;
}

// The .txt stack exactly as ini.cpp LoadFromIni reads it: [DataFile] File00..; File00 loaded plain, the rest as
// patches; the save target is the highest-indexed file (MBAACC layout).
bool LoadStack(const std::string& txt, FrameData& fd, std::string& top)
{
	const int n = GetPrivateProfileIntA("DataFile", "FileNum", 0, txt.c_str());
	if (n <= 0) { std::fprintf(stderr, "%s: no [DataFile]\n", txt.c_str()); return false; }
	const std::string folder = txt.substr(0, txt.find_last_of("\\/"));
	for (int i = 0; i < n; ++i) {
		char key[16], name[256]{};
		std::snprintf(key, sizeof key, "File%02d", i);
		GetPrivateProfileStringA("DataFile", key, nullptr, name, sizeof name, txt.c_str());
		const std::string p = folder + "\\" + name;
		if (!fd.load(p.c_str(), i != 0)) { std::fprintf(stderr, "load failed: %s\n", p.c_str()); return false; }
		top = p;
	}
	return true;
}

void PrintStage(const wire::Stage& s)
{
	int n = 0;
	for (int i = 1; i < 100; ++i) n += s.IsValid(i);
	std::printf("stage loaded=%d (%s) selected=%d bgm=%d stageLoads=%u allowed=%u listEntries=%d\n", s.loaded,
	            s.dataFile, s.selected, s.bgmId, s.stageLoads, s.allowed, n);
}

// ---- [authoring] ----
const char* MoonLetter(int m) { return m == 0 ? "C" : m == 1 ? "F" : m == 2 ? "H" : "?"; }

bool ParsePick(const std::string& spec, const std::vector<authoring::RosterChar>& roster, authoring::SlotPick& out, std::string& err)
{
	if (spec == "-" || spec == "none") { out = {}; return true; }
	const size_t a = spec.find('/'), b = a == std::string::npos ? a : spec.find('/', a + 1);
	const std::string name = spec.substr(0, a);
	std::string moon = a == std::string::npos ? "c" : spec.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
	const std::string pal = b == std::string::npos ? "0" : spec.substr(b + 1);
	int chara = -1;
	if (!name.empty() && (std::isdigit((unsigned char)name[0]) || name[0] == '-')) chara = std::atoi(name.c_str());
	else
		for (const authoring::RosterChar& c : roster) {
			std::string n = c.name, x = name;
			for (char& ch : n) ch = (char)std::tolower((unsigned char)ch);
			for (char& ch : x) ch = (char)std::tolower((unsigned char)ch);
			if (n == x || c.file1 == x) { chara = c.chara; break; }
		}
	if (chara < 0) { err = "unknown character '" + name + "'"; return false; }
	for (char& ch : moon) ch = (char)std::tolower((unsigned char)ch);
	const int m = moon == "c" || moon == "0" ? 0 : moon == "f" || moon == "1" ? 1 : moon == "h" || moon == "2" ? 2 : -1;
	if (m < 0) { err = "bad moon '" + moon + "' (c/f/h)"; return false; }
	out = { chara, m, std::atoi(pal.c_str()) };
	return true;
}

// argv[first..] = <mode> <p1> <p2> [p3 p4] [options]
bool ParseSetupArgs(int argc, char** argv, int first, const std::vector<authoring::RosterChar>& roster, authoring::Setup& s,
                    std::string& err)
{
	if (argc <= first + 2) { err = "setup-set <mode> <p1> <p2> [p3 p4] [options]"; return false; }
	const std::string mode = argv[first];
	s = authoring::Setup{};
	s.mode = mode == "1v1" || mode == "vs" || mode == "versus" ? authoring::Mode::Versus : mode == "team" ? authoring::Mode::Team : authoring::Mode::Tag;
	if (mode != "1v1" && mode != "vs" && mode != "versus" && mode != "tag" && mode != "team") { err = "mode must be 1v1, tag or team"; return false; }
	s.assists = s.mode == authoring::Mode::Tag;
	int i = first + 1, slot = 0;
	while (i < argc && slot < 4 && (std::strchr(argv[i], '/') || !std::strcmp(argv[i], "-"))) {
		if (!ParsePick(argv[i], roster, s.slot[slot], err)) return false;
		++slot; ++i;
	}
	if (slot < 2) { err = "two points are required"; return false; }
	for (; i < argc; ++i) {
		const std::string o = argv[i];
		auto next = [&](const char* what) -> const char* {
			if (i + 1 >= argc) { err = o + " needs " + what; return nullptr; }
			return argv[++i];
		};
		if (o == "stage") { const char* v = next("a number"); if (!v) return false; s.stage = std::atoi(v); }
		else if (o == "scene") {
			const char* v = next("auto|training|vs"); if (!v) return false;
			s.scene = !std::strcmp(v, "training") ? 1 : !std::strcmp(v, "vs") ? 2 : 0;
		}
		else if (o == "assists") { const char* v = next("on|off"); if (!v) return false; s.assists = !std::strcmp(v, "on"); }
		else if (o == "hot") s.hotOnly = true;
		else if (o == "force") s.force = true;
		else if (o == "keepbgm") s.keepBgm = true;
		else if (o == "resetpos") s.resetPos = true;
		else if (o == "notuning") s.tuningFirst = false;
		else if (o == "ko") { const char* v = next("one|all"); if (!v) return false; s.koRule = !std::strcmp(v, "all") ? 1 : 0; }
		else if (o == "timer") { const char* v = next("a number"); if (!v) return false; s.timer = std::atoi(v); }
		else if (o == "assist") {
			if (i + 3 >= argc) { err = "assist <side 1|2> <dir 5|2|6|4|8> <choice 0..16 or motion>"; return false; }
			const int side = std::atoi(argv[++i]) - 1, dir = std::atoi(argv[++i]);
			const std::string ch = argv[++i];
			int d = -1;
			for (int k = 0; k < 5; ++k) if (tagtune::kAssistDirs[k] == dir) d = k;
			bool num = !ch.empty();
			for (char x : ch) num = num && std::isdigit((unsigned char)x);
			int c = num ? std::atoi(ch.c_str()) : -1;
			for (int k = 0; c < 0 && k < authoring::kAssistMotionCount; ++k) if (ch == authoring::kAssistMotionChoices[k]) c = k + 1;
			if (side < 0 || side > 1 || d < 0 || c < 0) { err = "bad assist choice"; return false; }
			s.assist[side][d] = (uint8_t)c;
		}
		else { err = "unknown option '" + o + "'"; return false; }
	}
	return true;
}

void PrintMatchSetup(const char* label, const wire::MatchSetup& m, const std::vector<authoring::RosterChar>& roster)
{
	std::printf("%s: %s (scene %u flags 0x%02X ko %u timer %u)\n", label, authoring::Describe(authoring::FromWire(m), roster).c_str(),
	            m.scene, m.flags, m.koRule, m.timer);
}

void PrintSetupState(const wire::SetupState& s, const std::vector<authoring::RosterChar>& roster)
{
	std::printf("phase=%s auth=%s session=0x%02X role=%u betweenRounds=%u editsAllowed=%u lastSeq=%u lastStatus=%s path=%s "
	            "duration=%ums setups=%u gameModeKind=0x%X message='%s'\n",
	            wire::PhaseName(s.phase), wire::AuthStateName(s.authState), s.sessionFlags, s.sessionRole, s.betweenRounds,
	            s.editsAllowed, s.lastSeq, wire::StatusName(s.lastStatus), s.lastPath == 1 ? "hot" : s.lastPath == 2 ? "cold" : "-",
	            (unsigned)s.lastDurationMs, (unsigned)s.setupCount, (unsigned)s.gameModeKind, s.message);
	PrintMatchSetup("  requested", s.requested, roster);
	if (s.phase == (uint8_t)wire::Phase::Battle) PrintMatchSetup("  in force ", s.inForce, roster);
}

void PrintTuning(const Snapshot& s, int onlySlot)
{
	const wire::TuningGlobal& g = s.tuning;
	std::printf("tuning: source=%u frozen=%u style='%s' sha=%s loads=%u warnings=%u flags=0x%02X charFiles=%u leverTable=0x%08X%s\n",
	            g.source, g.frozen, g.activeStyle, g.sha, (unsigned)g.tuningLoads, g.warnings, g.flags, g.charFiles,
	            (unsigned)g.leverTableHash, g.leverTableHash == tagtune::LeverTableHash() ? "" : " (DIFFERS from this editor's)");
	std::printf("%-22s %10s", "lever", "global");
	for (int k = 0; k < 4; ++k)
		if ((onlySlot < 0 || onlySlot == k) && s.haveTuningSlot[k]) {
			const wire::TuningSlot& t = s.tuningSlot[k];
			char h[40];
			std::snprintf(h, sizeof h, "%s %s", t.exists ? t.file : "-", t.exists && t.moon < 3 ? MoonLetter(t.moon) : "");
			std::printf(" %14s", h);
		}
	std::printf("\n");
	for (size_t i = 0; i < tagtune::kLeverCount && i < 64; ++i) {
		const tagtune::Lever& l = tagtune::kLevers[i];
		auto src = [&](const uint32_t* m, char c) { return wire::MaskBit(m, (int)i) ? c : ' '; };
		std::printf("%-22s %8s %c%c", l.key, tagtune::FormatLeverValue(l, g.values[i]).c_str(),
		            wire::MaskBit(g.envMask, (int)i) ? 'e' : wire::MaskBit(g.tuningMask, (int)i) ? 'g' : ' ', src(g.styleMask, 's'));
		for (int k = 0; k < 4; ++k)
			if ((onlySlot < 0 || onlySlot == k) && s.haveTuningSlot[k]) {
				const wire::TuningSlot& t = s.tuningSlot[k];
				const char c = wire::MaskBit(t.cssMask, (int)i) ? 'x' : wire::MaskBit(t.moonMask, (int)i) ? 'm' : wire::MaskBit(t.charMask, (int)i) ? 'c' : ' ';
				std::printf(" %13s%c", tagtune::FormatLeverValue(l, t.values[i]).c_str(), c);
			}
		std::printf("\n");
	}
}

std::vector<authoring::RosterChar> RosterOf(Client& c)
{
	c.WaitCaps(3000);
	for (int k = 0; k < 100 && c.Get().haveCaps && (c.Get().caps.caps & wire::kCapRoster) && !c.Get().rosterComplete; ++k) Sleep(20);
	const Snapshot s = c.Get();
	return s.rosterComplete ? authoring::RosterFromLink(s.roster) : authoring::MirrorRoster(true);
}

} // namespace

int main(int argc, char** argv)
{
	uint32_t pid = 0;
	if (argc > 2 && std::strcmp(argv[1], "--pid") == 0) {
		pid = (uint32_t)std::strtoul(argv[2], nullptr, 0);
		argv += 2; argc -= 2;
	}
	if (argc < 2) { std::fprintf(stderr, "usage: see the banner of src/game_link_cli.cpp\n"); return 1; }
	const std::string cmd = argv[1];


	// ---- [authoring] offline commands ----
	if (cmd == "tag-migrate") {
		if (argc < 3) { std::fprintf(stderr, "tag-migrate <gamedir> [--dry-run]\n"); return 2; }
		const bool dry = argc > 3 && !std::strcmp(argv[3], "--dry-run");
		SYSTEMTIME st; GetLocalTime(&st);
		char date[32];
		std::snprintf(date, sizeof date, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
		const tagtune::MigrationResult r = tagtune::MigrateGameDir(argv[2], dry, date);
		std::printf("%s", r.log.c_str());
		return r.plan.ok && (dry || r.wrote) ? 0 : 1;
	}
	if (cmd == "setup-hex") {
		authoring::Setup s;
		std::string err;
		const std::vector<authoring::RosterChar> roster = authoring::MirrorRoster(true);
		if (!ParseSetupArgs(argc, argv, 2, roster, s, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
		std::printf("%s\n", authoring::ToHex(authoring::ToWire(s)).c_str());
		return 0;
	}
	if (cmd == "launch") {
		if (argc < 5) { std::fprintf(stderr, "launch <gamedir> <mode> <p1> <p2> [p3 p4] [options]\n"); return 2; }
		const std::string dir = argv[2];
		authoring::Setup s;
		std::string err;
		const std::vector<authoring::RosterChar> roster = authoring::MirrorRoster(true);
		if (!ParseSetupArgs(argc, argv, 3, roster, s, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
		authoring::ValidateContext vc;
		vc.team4p = s.mode == authoring::Mode::Team;   // the launch itself adds PCHOST_MBAACC_2V2 for TEAM
		vc.paletteCount = [&](const std::string& f) { return authoring::PaletteCountOf(dir + "\\data\\" + f + ".pal"); };
		for (const authoring::Problem& p : authoring::Validate(s, roster, vc)) {
			std::fprintf(stderr, "refused before launch: %s (%s)\n", p.msg.c_str(), wire::StatusName(p.status));
			return 1;
		}
		authoring::GameLauncher l;
		if (!l.Launch(dir, authoring::LaunchEnv(s), &err)) { std::fprintf(stderr, "launch: %s\n", err.c_str()); return 1; }
		const DWORD t0 = GetTickCount();
		size_t printed = 0;
		for (;;) {
			const authoring::LaunchState st = l.Get();
			const std::vector<std::string> log = l.Log();
			for (; printed < log.size(); ++printed) std::printf("%s\n", log[printed].c_str());
			if (st.phase == authoring::LaunchPhase::Running) break;
			if (st.phase == authoring::LaunchPhase::Failed || st.phase == authoring::LaunchPhase::Exited) { std::fprintf(stderr, "%s\n", st.message.c_str()); return 1; }
			Sleep(50);
		}
		const uint32_t gamePid = l.Get().gamePid;
		std::printf("game pid %u (%u ms after launch)\n", (unsigned)gamePid, (unsigned)(GetTickCount() - t0));
		std::fflush(stdout);
		Client c;
		c.SetTargetPid(gamePid);
		c.SetAuthoringPoll(true, false);
		if (!Connect(c, 20000)) return 1;
		if (!c.WaitCaps(5000) || c.Get().capsUnknown) { std::fprintf(stderr, "pchost.dll has no Authoring Mode (QueryCaps)\n"); return 1; }
		std::printf("linked %u ms after launch; waiting for the launch setup (seq 0) to reach Ready\n", (unsigned)(GetTickCount() - t0));
		for (;;) {
			wire::SetupState st{};
			if (c.WaitSetup(1000, st)) {
				if (st.setupCount > 0 && (st.authState == (uint8_t)wire::AuthState::Ready || st.authState == (uint8_t)wire::AuthState::Failed)) {
					PrintSetupState(st, roster);
					std::printf("%s %u ms after launch\n", wire::AuthStateName(st.authState), (unsigned)(GetTickCount() - t0));
					std::printf("GAME_PID=%u\n", (unsigned)gamePid);
					return st.authState == (uint8_t)wire::AuthState::Ready ? 0 : 1;
				}
			}
			if (GetTickCount() - t0 > 60000) { std::fprintf(stderr, "no Ready within 60 s\n"); std::printf("GAME_PID=%u\n", (unsigned)gamePid); return 1; }
			if (l.Get().phase == authoring::LaunchPhase::Exited) { std::fprintf(stderr, "%s\n", l.Get().message.c_str()); return 1; }
		}
	}
	if (cmd == "mock-dll") {
		gamelink::MockDll::Options o;
		for (int i = 3; i < argc; ++i) {
			if (!std::strcmp(argv[i], "notag")) o.tagOps = false;
			if (!std::strcmp(argv[i], "session")) o.session = true;
			if (!std::strcmp(argv[i], "menu")) o.inBattle = false;
			if (!std::strcmp(argv[i], "rev1")) o.authoring = false;
			if (!std::strcmp(argv[i], "host")) o.role = 1;
			if (!std::strcmp(argv[i], "between")) o.betweenRounds = true;
			if (!std::strcmp(argv[i], "team4p")) o.team4p = true;
			if (!std::strcmp(argv[i], "paused")) o.hotReloadPaused = true;
			if (i + 1 < argc && !std::strcmp(argv[i], "root")) o.tagRoot = argv[++i];
			else if (i + 1 < argc && !std::strcmp(argv[i], "skew")) o.skewLever = tagtune::FindLever(argv[++i]);
			else if (i + 1 < argc && !std::strcmp(argv[i], "hash")) o.leverHash = (uint32_t)std::strtoul(argv[++i], nullptr, 16);
			else if (i + 1 < argc && !std::strcmp(argv[i], "env")) wire::SetMaskBit(o.envMask, tagtune::FindLever(argv[++i]));
			else if (i + 1 < argc && !std::strcmp(argv[i], "hot")) o.hotMs = std::atoi(argv[++i]);
			else if (i + 1 < argc && !std::strcmp(argv[i], "cold")) o.coldMs = std::atoi(argv[++i]);
			else if (i + 1 < argc && !std::strcmp(argv[i], "setup")) o.setupHex = argv[++i];
			else if (!std::strcmp(argv[i], "frames")) o.frames = true;
			else if (!std::strcmp(argv[i], "nolayered")) { o.frames = true; o.layeredUnsupported = true; }
			else if (i + 1 < argc && !std::strcmp(argv[i], "regrow")) { o.frames = true; o.regrowAfter = std::atoi(argv[++i]); }
			else if (!std::strcmp(argv[i], "layered")) { o.frames = true; o.layeredAtStart = true; }
			else if (i + 1 < argc && !std::strcmp(argv[i], "fps")) o.fps = std::atoi(argv[++i]);
			else if (i + 2 < argc && !std::strcmp(argv[i], "size")) { o.frameW = std::atoi(argv[++i]); o.frameH = std::atoi(argv[++i]); }
		}
		gamelink::MockDll m(o);
		if (!m.Start()) { std::fprintf(stderr, "cannot create the mock pipe\n"); return 2; }
		std::printf("mock dev-link serving as pid %u for %s s\n", m.Pid(), argc > 2 ? argv[2] : "60");
		std::fflush(stdout);
		Sleep((DWORD)(1000 * (argc > 2 ? std::atoi(argv[2]) : 60)));
		std::printf("mock: %u commands, %u gate probes, %u setups, %u tuning applies, %u Ex dropped, %u frames produced, %u injects\n",
		            m.Commands(), m.Probes(), m.SetupsFinished(), m.Applies(), m.ExDropped(), m.FramesProduced(), m.Injects());
		m.Stop();
		return 0;
	}
	if (cmd == "tag") {
		gamelink::Client c;
		if (pid) c.SetTargetPid(pid);
		c.SetTagQuery(true);
		c.SetPollHz(20);
		c.Connect();
		gamelink::Snapshot s;
		for (int k = 0; k < 150; ++k) { s = c.Get(); if (s.haveTag || s.tagUnsupported) break; Sleep(20); }
		if (!s.connected) { std::printf("not connected: %s\n", s.status.c_str()); return 2; }
		if (!s.haveTag) { std::printf("QueryTag unsupported by this pchost.dll (needs PovertyCaster mbaacc/link-tag)\n"); return 3; }
		const auto& t = s.tag;
		std::printf("session 0x%02X config 0x%02X frozen %u ini %u koRule %u loads %u warnings %u assist %u style '%s' sha %s\n",
		            t.sessionFlags, t.tagConfig, t.frozen, t.iniPresent, t.koRule, t.tuningLoads, t.warnings, t.assistEnabled,
		            t.activeStyle, t.sha);
		for (int i = 0; i < 2; ++i) {
			const auto& m = t.team[i];
			std::printf("team %d: point %d req %d counter %d tagInTick %d cooldown %d | assist slot 0x%02X place %u flags %u "
			            "tick %d cd %d pattern %d calls %d meter %d\n", i + 1, m.activeSlot, m.tagRequest, m.counter,
			            m.tagInTick, m.cooldownLeft, m.assistSlot, m.assistPlacement, m.assistFlags, m.assistTick,
			            m.assistCooldown, m.assistPattern, m.assistCalls, m.meterPaid);
		}
		for (int i = 0; i < 4; ++i)
			std::printf("P%d tagIn %d tagOut %d hp %d red %d\n", i + 1, t.slot[i].tagIn, t.slot[i].tagOut, t.slot[i].health,
			            t.slot[i].red);
		return 0;
	}

	if (cmd == "bg-get" || cmd == "bg-set-duration" || cmd == "bg-set") {
		if (argc < 5) return 1;
		bg::File f;
		if (!f.Load(argv[2])) { std::fprintf(stderr, "load failed: %s\n", argv[2]); return 2; }
		const int o = std::atoi(argv[3]), fr = std::atoi(argv[4]);
		auto& objs = f.GetObjects();
		if (o < 0 || o >= (int)objs.size() || fr < 0 || fr >= (int)objs[o].frames.size()) {
			std::fprintf(stderr, "no object %d frame %d (%zu objects)\n", o, fr, objs.size()); return 2;
		}
		bg::Frame& frame = objs[o].frames[fr];
		std::printf("object %d frame %d/%zu sprite %d duration %d offset %d,%d opacity %d (%s)\n", o, fr,
		            objs[o].frames.size(), frame.spriteId, frame.duration, frame.offsetX, frame.offsetY, frame.opacity,
		            argv[2]);
		if (cmd == "bg-get") return 0;
		const std::string field = cmd == "bg-set" ? (argc > 5 ? argv[5] : "") : "duration";
		const char* val = cmd == "bg-set" ? (argc > 6 ? argv[6] : nullptr) : (argc > 5 ? argv[5] : nullptr);
		if (!val) return 1;
		const int v = std::atoi(val);
		if (field == "duration") frame.duration = (int16_t)v;
		else if (field == "offsetX") frame.offsetX = (int16_t)v;
		else if (field == "offsetY") frame.offsetY = (int16_t)v;
		else if (field == "opacity") frame.opacity = (uint8_t)v;
		else { std::fprintf(stderr, "unknown field %s\n", field.c_str()); return 1; }
		f.MarkDirty();
		if (!f.Save(argv[2])) { std::fprintf(stderr, "save failed: %s\n", argv[2]); return 2; }
		std::printf("saved %s %d -> %s\n", field.c_str(), v, argv[2]);
		return 0;
	}

	if (cmd == "ha6-get" || cmd == "ha6-set-duration") {
		if (argc < 5) return 1;
		FrameData fd; std::string top;
		if (!LoadStack(argv[2], fd, top)) return 2;
		const int p = std::atoi(argv[3]), f = std::atoi(argv[4]);
		Sequence* seq = fd.get_sequence(p);
		if (!seq || f < 0 || f >= (int)seq->frames.size()) { std::fprintf(stderr, "no pattern %d frame %d\n", p, f); return 2; }
		auto& af = seq->frames[f].AF;
		std::printf("pattern %d '%s' frame %d/%zu duration %d (save target %s)\n", p, seq->name.c_str(), f,
		            seq->frames.size(), af.duration, top.c_str());
		if (cmd == "ha6-get") return 0;
		if (argc < 6) return 1;
		af.duration = std::atoi(argv[5]);
		seq->modified = true;
		if (!fd.save(top.c_str())) { std::fprintf(stderr, "save failed: %s\n", top.c_str()); return 2; }
		std::printf("saved duration %d -> %s\n", af.duration, top.c_str());
		return 0;
	}

	Client c;
	c.SetPollHz(0);
	if (pid) c.SetTargetPid(pid);
	if (!Connect(c)) return 2;

	if (cmd == "stage") {
		c.SetPollHz(10);
		wire::Stage s;
		if (!c.WaitStage(3000, s)) { std::fprintf(stderr, "no stage state (%s)\n", c.Get().stageUnsupported ? "old pchost.dll" : "timeout"); return 3; }
		PrintStage(s);
		return 0;
	}
	if (cmd == "setstage") {
		if (argc < 3) return 1;
		uint8_t fl = 0;
		for (int i = 3; i < argc; ++i) {
			if (!std::strcmp(argv[i], "keepbgm")) fl |= wire::kFlagKeepBgm;
			if (!std::strcmp(argv[i], "list")) fl |= wire::kFlagStageList;
		}
		return WaitAndPrintReply(c, c.SetStage(std::atoi(argv[2]), fl), 10000);
	}
	if (cmd == "reloadstage") {
		const uint8_t fl = argc > 2 && !std::strcmp(argv[2], "list") ? wire::kFlagStageList : 0;
		return WaitAndPrintReply(c, c.ReloadStage(fl), 10000);
	}
	if (cmd == "stage-watch") {
		if (argc < 4) return 1;
		c.SetPollHz(5);
		c.SetAutoReloadStage(true);
		const std::string file = argv[2];
		const std::string stem = StageStemOf(file);
		StageFileKind kind = StageFileKind::Dat;
		if (stem.empty()) kind = StageFileKind::List;
		else if (file.size() > 8 && (file.find("nfo") != std::string::npos || file.find("NFO") != std::string::npos)) kind = StageFileKind::Info;
		else if (file.find("ight") != std::string::npos) kind = StageFileKind::Light;
		c.SetWatchedStageFiles({ { file, kind } });
		const int timeout = std::atoi(argv[3]);
		std::printf("watching %s for %d ms\n", file.c_str(), timeout);
		const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
		std::string seen;
		while (std::chrono::steady_clock::now() < end) {
			const Snapshot s = c.Get();
			if (s.lastChange != seen && !s.lastChange.empty()) { seen = s.lastChange; std::printf("%s\n", seen.c_str()); }
			for (const auto& l : c.RecentLog())
				if (l.rfind("reloadstage #", 0) == 0) { std::printf("%s\n", l.c_str()); return l.find(": ok") != std::string::npos ? 0 : 4; }
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		std::fprintf(stderr, "no reloadstage verdict within %d ms\n", timeout);
		return 3;
	}

	if (cmd == "state") {
		const int count = argc > 2 ? std::atoi(argv[2]) : 1;
		const int interval = argc > 3 ? std::atoi(argv[3]) : 200;
		c.SetPollHz(20);
		for (int i = 0; i < count; ++i) {
			wire::State s;
			if (!c.WaitState(2000, s)) { std::fprintf(stderr, "no state\n"); return 3; }
			PrintState(s);
			if (i + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		}
		return 0;
	}

	// ---- [authoring] linked commands ----
	if (cmd == "caps") {
		if (!c.WaitCaps(3000)) { std::fprintf(stderr, "no caps answer\n"); return 1; }
		const Snapshot s = c.Get();
		if (s.capsUnknown) { std::printf("rev 1: pchost.dll predates Authoring Mode (QueryCaps unknown)\n"); return 0; }
		std::printf("authoring rev %u caps=0x%08X build=%s game=%s leverTable=0x%08X (%s) levers=%u perChar=%u setupVersion=%u "
		            "tuningWire=%u tuningSource=%u tuningFlags=0x%02X charFiles=%u\n",
		            s.caps.revision, (unsigned)s.caps.caps, s.caps.build, s.gameId.empty() ? "(mbaacc)" : s.gameId.c_str(),
		            (unsigned)s.caps.leverTableHash, s.caps.leverTableHash == tagtune::LeverTableHash() ? "matches" : "DIFFERS",
		            s.caps.leverCount, s.caps.perCharLeverCount, s.caps.matchSetupVersion, s.caps.tuningWireVersion,
		            s.caps.tuningSource, s.caps.tuningFlags, s.caps.charFiles);
		return 0;
	}
	if (cmd == "roster") {
		const std::vector<authoring::RosterChar> r = RosterOf(c);
		for (const authoring::RosterChar& x : r)
			std::printf("%3d sel %2d %-9s %-10s %-10s %s%s%s%s\n", x.chara, x.selector, x.name.c_str(), x.file1.c_str(), x.file2.c_str(),
			            x.Duo() ? "duo " : "", (x.flags & wire::kRosterTagOk) ? "tag " : "", (x.flags & wire::kRosterTeamOk) ? "team " : "",
			            (x.flags & wire::kRosterNeedsMod) ? "needs-mod" : "");
		std::printf("%zu entries (%s)\n", r.size(), c.Get().rosterComplete ? "from the game" : "mirror: the game sent none");
		return 0;
	}
	if (cmd == "setup-get") {
		c.SetAuthoringPoll(true, false);
		const std::vector<authoring::RosterChar> roster = RosterOf(c);
		wire::SetupState st{};
		if (!c.WaitSetup(3000, st)) { std::fprintf(stderr, "no LinkSetupState (rev 1 DLL?)\n"); return 1; }
		PrintSetupState(st, roster);
		return 0;
	}
	if (cmd == "setup-set") {
		const std::vector<authoring::RosterChar> roster = RosterOf(c);
		authoring::Setup s;
		std::string err;
		if (!ParseSetupArgs(argc, argv, 2, roster, s, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
		const DWORD t0 = GetTickCount();
		const uint16_t seq = c.SetMatchSetup(authoring::ToWire(s));
		if (!seq) { std::fprintf(stderr, "not sent: %s\n", c.Get().capsUnknown ? "pchost.dll predates Authoring Mode" : "no link"); return 1; }
		c.SetAuthoringPoll(true, false);
		wire::Reply r{};
		if (!c.WaitReply(seq, 25000, r)) { std::fprintf(stderr, "no reply within 25 s\n"); return 1; }
		std::printf("setup-set #%u: %s - %s (%u ms)\n", seq, wire::StatusName(r.status), r.message, (unsigned)(GetTickCount() - t0));
		wire::SetupState st{};
		if (c.WaitSetup(2000, st)) PrintSetupState(st, roster);
		return r.status == 0 ? 0 : 1;
	}
	if (cmd == "tuning-apply") {
		uint8_t fl = wire::kFlagQueryAfter;
		if (argc > 2 && !std::strcmp(argv[2], "reload")) fl |= wire::kFlagReload;
		c.WaitCaps(3000);
		const uint32_t since = c.Get().tuningSerial;
		const DWORD t0 = GetTickCount();
		const uint16_t seq = c.ApplyTuning(fl);
		wire::Reply r{};
		if (!c.WaitReply(seq, 5000, r)) { std::fprintf(stderr, "no reply\n"); return 1; }
		std::printf("tuning-apply #%u: %s - %s (%u ms)\n", seq, wire::StatusName(r.status), r.message, (unsigned)(GetTickCount() - t0));
		Snapshot s;
		if (r.status == 0 && c.WaitTuning(3000, s, since)) PrintTuning(s, -1);
		return r.status == 0 ? 0 : 1;
	}
	if (cmd == "tuning-get") {
		c.WaitCaps(3000);
		const int slot = argc > 2 ? std::atoi(argv[2]) : -1;
		const uint32_t since = c.Get().tuningSerial;
		c.QueryTuning(slot >= 0 && slot < 4 ? (uint8_t)(1u << slot) : 0);
		Snapshot s;
		if (!c.WaitTuning(3000, s, since)) { std::fprintf(stderr, "no LinkTuningGlobal (rev 1 DLL?)\n"); return 1; }
		PrintTuning(s, slot);
		return 0;
	}
	if (cmd == "end-authoring") { c.WaitCaps(3000); return WaitAndPrintReply(c, c.EndAuthoring(), 3000); }

	// ---- [game-view] ----
	if (cmd == "frame-check") {
		const int want = argc > 2 ? std::atoi(argv[2]) : 60;
		const bool layered = argc > 3 && !std::strcmp(argv[3], "layered");
		if (!c.WaitCaps(3000) || !(c.Get().caps.caps & wire::kCapFrameShare)) { std::fprintf(stderr, "no frame export (kCapFrameShare)\n"); return 1; }
		if (layered) {
			wire::Reply r{};
			if (c.WaitReply(c.SetEmbedded(2), 3000, r) && r.status != 0) {
				std::printf("layered capture refused (%s: %s): checking full frames only (§12.3)\n", wire::StatusName(r.status), r.message);
				c.WaitReply(c.SetEmbedded(1), 3000, r);
			}
		}
		c.QueryFrameShare();
		for (int k = 0; k < 150 && !c.Get().haveFrameShare; ++k) Sleep(20);
		const wire::FrameShare fs = c.Get().frameShare;
		if (!fs.version) { std::fprintf(stderr, "the game announces no ring\n"); return 1; }
		std::printf("ring %s: %ux%u max, %u slots, %u layers, %u bytes\n", fs.name, fs.maxWidth, fs.maxHeight, fs.slotCount, fs.layerCapacity, fs.ringBytes);
		framering::Reader rd;
		std::string why;
		if (!rd.Open(fs.name, &why)) { std::fprintf(stderr, "%s\n", why.c_str()); return 1; }
		int got = 0, bad = 0, rebuilt = 0, layeredFrames = 0;
		const DWORD t0 = GetTickCount();
		std::vector<uint8_t> px;
		while (got < want && GetTickCount() - t0 < 20000) {
			if (rd.Poll() != framering::ReadResult::Ok) { Sleep(2); continue; }
			const framering::FrameSlotHeader& f = rd.Frame();
			bool ok = true;
			for (uint32_t k = 0; k < f.layerCount; ++k) {
				const framering::FrameLayer& l = f.layers[k];
				if (l.checksum && framering::FrameChecksum(rd.LayerPixels(l), l.width, l.height, l.pitch) != l.checksum) ok = false;
			}
			bad += !ok;
			const framering::FrameLayer* full = framering::FindLayer(f, framering::kLayerFull);
			const framering::FrameLayer* ch = framering::FindLayer(f, framering::kLayerChars);
			const framering::FrameLayer* hud = framering::FindLayer(f, framering::kLayerHud);
			// the FULL rebuild uses the synthetic DrawTestStage: it is only meaningful against the mock producer (§12.3)
			const bool mockRing = rd.Ring() && std::strncmp(rd.Ring()->producer, "mock", 4) == 0;
			if (full && ch && hud && mockRing) {
				++layeredFrames;
				px.assign((size_t)full->pitch * full->height, 0);
				framering::DrawTestStage(px.data(), full->width, full->height, full->pitch, f.camera);
				framering::CompositePremulOver(px.data(), full->width, full->height, full->pitch, rd.LayerPixels(*ch), ch->width, ch->height, ch->pitch, ch->x, ch->y);
				framering::CompositePremulOver(px.data(), full->width, full->height, full->pitch, rd.LayerPixels(*hud), hud->width, hud->height, hud->pitch, hud->x, hud->y);
				rebuilt += framering::FrameChecksum(px.data(), full->width, full->height, full->pitch) == full->checksum;
			}
			if (got % 30 == 0)
				std::printf("frame %u (game %u) %ux%u layers %u checksum %s cam %d,%d\n", f.frameSeq, f.gameFrame, f.width, f.height, f.layerCount,
				            ok ? "ok" : "BAD", f.camera.cameraX, f.camera.cameraY);
			++got;
		}
		const DWORD ms = GetTickCount() - t0;
		std::printf("%d frames in %u ms (%.1f fps), %d bad checksums, %u skipped, %u torn retries, latency %u ms; layered %d, rebuilt FULL %d\n",
		            got, (unsigned)ms, got * 1000.0 / (ms ? ms : 1), bad, rd.Skipped(), rd.Torn(), rd.LatencyMs(), layeredFrames, rebuilt);
		return got == want && bad == 0 && rebuilt == layeredFrames && (!layered || layeredFrames > 0) ? 0 : 1;
	}
	if (cmd == "embed") {
		c.WaitCaps(3000);
		return WaitAndPrintReply(c, c.SetEmbedded(argc > 2 ? std::atoi(argv[2]) : 1), 3000);
	}
	if (cmd == "input") {
		if (argc < 6) { std::fprintf(stderr, "input <player> <dir 1-9> <buttons hex> <frames>\n"); return 2; }
		c.WaitCaps(3000);
		wire::InputInject in{};
		in.version = wire::kInjectVersion;
		in.player = (uint8_t)std::atoi(argv[2]);
		in.direction = (uint8_t)std::atoi(argv[3]);
		in.buttons = (uint8_t)std::strtoul(argv[4], nullptr, 16);
		in.holdFrames = (uint16_t)std::atoi(argv[5]);
		in.serial = 1;
		const uint16_t seq = c.InputInject(in);
		if (!seq) { std::fprintf(stderr, "not sent: this pchost.dll has no input injection (kCapInputInject)\n"); return 1; }
		return WaitAndPrintReply(c, seq, 3000);
	}
	if (cmd == "ping") return WaitAndPrintReply(c, c.Ping(), 3000);
	if (cmd == "reload") {
		const uint8_t mask = argc > 2 ? (uint8_t)std::strtoul(argv[2], nullptr, 0) : 0;
		const uint8_t flags = argc > 3 && std::strcmp(argv[3], "force") == 0 ? wire::kFlagForce : 0;
		return WaitAndPrintReply(c, c.Reload(mask, flags), 20000);
	}
	if (cmd == "setchar") {
		if (argc < 6) return 1;
		return WaitAndPrintReply(c, c.SetChar(std::atoi(argv[2]), std::atoi(argv[3]), std::atoi(argv[4]),
		                                      std::atoi(argv[5]), wire::kFlagReload), 20000);
	}
	if (cmd == "sample") {
		if (argc < 6) return 1;
		const int slot = std::atoi(argv[2]), pat = std::atoi(argv[3]), fr = std::atoi(argv[4]), ms = std::atoi(argv[5]);
		c.SetPollHz(60);
		int maxTicks = -1, hits = 0, samples = 0;
		const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
		while (std::chrono::steady_clock::now() < end) {
			wire::State s;
			if (!c.WaitState(1000, s)) continue;
			++samples;
			const wire::Actor& a = s.actors[slot];
			if (a.exists && a.pattern == pat && a.frame == fr) { ++hits; if (a.frameTicks > maxTicks) maxTicks = a.frameTicks; }
		}
		std::printf("sample P%d pattern %d frame %d: %d/%d samples in it, max ticks in frame %d\n", slot + 1, pat, fr,
		            hits, samples, maxTicks);
		return hits ? 0 : 5;
	}
	if (cmd == "watch") {
		if (argc < 5) return 1;
		c.SetPollHz(5);
		c.SetAutoReload(true);
		c.SetWatchedFiles({ { argv[2], argv[3] } });
		const int timeout = std::atoi(argv[4]);
		std::printf("watching %s (key %s) for %d ms\n", argv[2], argv[3], timeout);
		const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
		std::string seen;
		while (std::chrono::steady_clock::now() < end) {
			const Snapshot s = c.Get();
			if (s.lastChange != seen && !s.lastChange.empty()) { seen = s.lastChange; std::printf("%s\n", seen.c_str()); }
			const auto log = c.RecentLog();
			for (const auto& l : log)
				if (l.rfind("reload #", 0) == 0) { std::printf("%s\n", l.c_str()); return l.find(": ok") != std::string::npos ? 0 : 4; }
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		std::fprintf(stderr, "no reload verdict within %d ms\n", timeout);
		return 3;
	}
	std::fprintf(stderr, "unknown command %s\n", cmd.c_str());
	return 1;
}
