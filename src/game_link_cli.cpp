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
// --pid <n> (before the command) or HANTEI_GAME_LINK_PID=<n>: talk ONLY to that MBAA.exe; no discovery by name.
#include "game_link.h"
#include "game_link_mock.h"
#include "framedata.h"
#include "background/bg_file.h"

#include <windows.h>

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

	if (cmd == "mock-dll") {
		gamelink::MockDll::Options o;
		for (int i = 3; i < argc; ++i) {
			if (!std::strcmp(argv[i], "notag")) o.tagOps = false;
			if (!std::strcmp(argv[i], "session")) o.session = true;
			if (!std::strcmp(argv[i], "menu")) o.inBattle = false;
		}
		gamelink::MockDll m(o);
		if (!m.Start()) { std::fprintf(stderr, "cannot create the mock pipe\n"); return 2; }
		std::printf("mock dev-link serving as pid %u for %s s\n", m.Pid(), argc > 2 ? argv[2] : "60");
		std::fflush(stdout);
		Sleep((DWORD)(1000 * (argc > 2 ? std::atoi(argv[2]) : 60)));
		std::printf("mock: %u commands, %u gate probes\n", m.Commands(), m.Probes());
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
		std::printf("session 0x%02X frozen %u ini %u koRule %u loads %u warnings %u assist %u style '%s' sha %s\n",
		            t.sessionFlags, t.frozen, t.iniPresent, t.koRule, t.tuningLoads, t.warnings, t.assistEnabled,
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
