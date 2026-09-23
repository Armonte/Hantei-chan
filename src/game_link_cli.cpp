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
#include "game_link.h"
#include "framedata.h"

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

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2) { std::fprintf(stderr, "usage: see the banner of src/game_link_cli.cpp\n"); return 1; }
	const std::string cmd = argv[1];

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
	if (!Connect(c)) return 2;

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
