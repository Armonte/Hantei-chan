// Headless driver for the preview tick simulator (src/preview_sim.*).
//
//   preview_sim_tool --data DIR --char NAME [--moon M] --pattern P
//                    [--ticks T] [--every K] [--assume-true] [--bench]
//   preview_sim_tool --data DIR --char NAME [--moon M] --scan [--bench]
//
// Loads the character's [DataFile] list (<char>_<moon>.txt) plus effect.HA6,
// simulates the pattern and prints the actor list every K ticks. --scan runs
// every pattern and verifies that checkpoint replay (random-order queries)
// reproduces the sequential frontier bit-for-bit. --bench measures scrubbing.
// CLI tool: plain stdout is intended here (the app itself logs through quill).

#include "framedata.h"
#include "preview_sim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <initializer_list>

namespace {

std::string FindCaseInsensitive(const std::string& dir, const std::string& name)
{
	DIR* d = opendir(dir.c_str());
	if (!d) return {};
	std::string found;
	while (dirent* e = readdir(d)) {
		if (strcasecmp(e->d_name, name.c_str()) == 0) { found = dir + "/" + e->d_name; break; }
	}
	closedir(d);
	return found;
}

std::vector<std::string> DataFileList(const std::string& dir, const std::string& chr, int moon)
{
	std::vector<std::string> out;
	std::string txt = FindCaseInsensitive(dir, chr + "_" + std::to_string(moon) + ".txt");
	if (txt.empty()) {
		std::string base = FindCaseInsensitive(dir, chr + ".HA6");
		if (!base.empty()) out.push_back(base);
		return out;
	}
	std::ifstream f(txt, std::ios::binary);
	std::string line;
	bool inSection = false;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!line.empty() && line[0] == '[') { inSection = line.rfind("[DataFile]", 0) == 0; continue; }
		if (!inSection || line.rfind("File", 0) != 0 || line.rfind("FileNum", 0) == 0) continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string name = line.substr(eq + 1);
		name.erase(0, name.find_first_not_of(" \t"));
		name.erase(name.find_last_not_of(" \t") + 1);
		std::string path = FindCaseInsensitive(dir, name);
		if (!path.empty()) out.push_back(path);
	}
	return out;
}

double NowMs()
{
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

bool SameState(const preview::TickState& a, const preview::TickState& b, std::string* why)
{
	if (a.actors.size() != b.actors.size()) {
		if (why) *why = "actor count " + std::to_string(a.actors.size()) + " vs " + std::to_string(b.actors.size());
		return false;
	}
	for (size_t i = 0; i < a.actors.size(); i++) {
		const auto& x = a.actors[i];
		const auto& y = b.actors[i];
		if (x.id != y.id || x.pattern != y.pattern || x.frame != y.frame || x.frameTimer != y.frameTimer ||
		    x.x != y.x || x.y != y.y || x.facingLeft != y.facingLeft || x.angle != y.angle ||
		    x.effectHa6 != y.effectHa6 || x.rootEnded != y.rootEnded) {
			if (why) *why = "actor " + std::to_string(x.id) + " differs";
			return false;
		}
	}
	return true;
}

void PrintState(const preview::TickState& st)
{
	std::printf("tick %4d  actors %zu\n", st.tick, st.actors.size());
	for (const auto& a : st.actors) {
		std::printf("   #%-4d %*s%s%s pat %4d fr %3d t%-3d  pos (%7.1f,%7.1f) %s ang %5d z %3d  ef%-4d src p%d f%d e%d%s\n",
		            a.id, a.depth * 2, "", a.isRoot ? "ROOT" : (a.isPreset ? "PRESET" : "actor"),
		            a.effectHa6 ? "[fx]" : "    ", a.pattern, a.frame, a.frameTimer, a.x, a.y,
		            a.facingLeft ? "L" : "R", a.angle, a.zPriority, a.effectType,
		            a.srcPattern, a.srcFrame, a.srcEffectIndex,
		            a.rootEnded ? "  (ended)" : "");
	}
}


// ---------------------------------------------------------------------------
// Baseline: Gonptechan EX 1abc27f9 SimulateSpawnsToTickInternal (per-draw
// re-simulation from tick 0; every spawn's frame re-derived from its spawn
// tick via SimulateAnimationFlow on every tick). Ported for cost comparison
// only; it is not used by the editor.
// ---------------------------------------------------------------------------

int ExSimulateAnimationFlow(FrameData* fd, int patternId, int targetTick)
{
	auto seq = fd ? fd->get_sequence(patternId) : nullptr;
	if (!seq || seq->frames.empty() || targetTick < 0) return 0;
	int cur = 0, tick = 0, loop = seq->frames[0].AF.loopCount > 0 ? seq->frames[0].AF.loopCount : 0, dur = 0;
	for (int it = 0; tick < targetTick && it < 100000; it++) {
		if (cur < 0 || cur >= (int)seq->frames.size()) return (int)seq->frames.size() - 1;
		auto& af = seq->frames[cur].AF;
		int fd_ = af.duration <= 0 ? 1 : af.duration;
		if (dur >= fd_) {
			dur = 0;
			int next = cur;
			if (af.aniType == 1) { if (cur + 1 >= (int)seq->frames.size()) return cur; next = cur + 1; }
			else if (af.aniType == 2) {
				if ((af.aniFlag & 2) && loop < 0) next = (af.aniFlag & 8) ? cur + af.loopEnd : af.loopEnd;
				else { if (af.aniFlag & 2) loop--; next = (af.aniFlag & 4) ? cur + af.jump : af.jump; }
			} else return cur;
			if (next >= 0 && next < (int)seq->frames.size() && seq->frames[next].AF.loopCount > 0)
				loop = seq->frames[next].AF.loopCount;
			cur = next;
		} else {
			int adv = std::min(fd_ - dur, targetTick - tick);
			dur += adv; tick += adv;
		}
	}
	return cur;
}

struct ExSpawn { int spawnTick, pattern; bool effect; int frame; };

size_t ExSimulateSpawnsToTick(FrameData* main, FrameData* effect, int patternId, int targetTick)
{
	auto seq = main->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;
	std::vector<ExSpawn> spawns;
	auto src = [&](bool e) { return e && effect ? effect : main; };
	auto parse = [&](const Frame& f, int ownerPattern, bool ownerEffect, int tick) {
		for (const auto& ef : f.EF) {
			int t = ef.type;
			if (t == 1 || t == 101 || t == 1000 || t == 11 || t == 111)
				spawns.push_back({tick, ef.number + ((t == 101 || t == 111) ? ownerPattern : 0), ownerEffect, 0});
			else if (t == 8)
				spawns.push_back({tick, ef.number, true, 0});
		}
	};
	int prevMain = -1;
	for (int tick = 0; tick <= targetTick; ++tick) {
		int mf = ExSimulateAnimationFlow(main, patternId, tick);
		if (mf != prevMain && mf >= 0 && mf < (int)seq->frames.size()) { parse(seq->frames[mf], patternId, false, tick); prevMain = mf; }
		for (size_t i = 0; i < spawns.size() && spawns.size() < 4096; ++i) {
			auto& sp = spawns[i];
			if (sp.spawnTick > tick) continue;
			FrameData* s = src(sp.effect);
			auto ss = s->get_sequence(sp.pattern);
			if (!ss || ss->frames.empty()) continue;
			int el = tick - sp.spawnTick;
			int fr = ExSimulateAnimationFlow(s, sp.pattern, el);
			int prior = el > 0 ? ExSimulateAnimationFlow(s, sp.pattern, el - 1) : -1;
			sp.frame = fr;
			if (fr >= 0 && fr < (int)ss->frames.size() && fr != prior) parse(ss->frames[fr], sp.pattern, sp.effect, tick);
		}
	}
	return spawns.size();
}

// ---------------------------------------------------------------------------
// --selftest: synthetic FrameData exercising each engine rule. No game data.
// ---------------------------------------------------------------------------

Frame& AddFrame(FrameData& d, int pat, int dur, int aniType = 1, int aniFlag = 0, int jump = 0)
{
	Sequence* s = d.get_sequence(pat);
	s->frames.emplace_back();
	Frame& f = s->frames.back();
	f.AF.layers.emplace_back();
	f.AF.duration = dur;
	f.AF.aniType = aniType;
	f.AF.aniFlag = aniFlag;
	f.AF.jump = jump;
	return f;
}

void AddEF(Frame& f, int type, int no, std::initializer_list<int> params)
{
	Frame_EF e{};
	e.type = type; e.number = no;
	int i = 0;
	for (int v : params) e.parameters[i++] = v;
	f.EF.push_back(e);
}

void AddIF(Frame& f, int type, std::initializer_list<int> params)
{
	Frame_IF c{};
	c.type = type;
	int i = 0;
	for (int v : params) c.parameters[i++] = v;
	f.IF.push_back(c);
}

int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail++; } else std::printf("ok:   %s\n", msg); } while (0)

const preview::SimActor* ByPattern(const preview::TickState& st, int pat, int nth = 0)
{
	for (const auto& a : st.actors)
		if (a.pattern == pat && !a.isRoot && nth-- == 0) return &a;
	return nullptr;
}

int SelfTest()
{
	FrameData d, fx;
	d.initEmpty();
	fx.initEmpty();
	preview::Options opt;
	preview::TickState st;

	// 1) Nested facing: root(R) --EF1 X=100 bit11--> A --EF1 X=50--> B.
	//    Engine: A facing L, x=-100; B inherits L, x = -100 - 50 = -150.
	{
		Frame& r = AddFrame(d, 10, 5, 0);
		AddEF(r, 1, 11, {100, -20, 0x800, 0});
		Frame& a = AddFrame(d, 11, 5, 0);
		AddEF(a, 1, 12, {50, 0, 0, 0});
		AddFrame(d, 12, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 10, opt);
		sim.getStateAt(0, st);
		auto* A = ByPattern(st, 11); auto* B = ByPattern(st, 12);
		CHECK(A && A->facingLeft && A->x == -100.f && A->y == -20.f, "bit11 child faces left, own X mirrored");
		CHECK(B && B->facingLeft && B->x == -150.f, "grandchild inherits facing; only own X mirrored");
		// Same tree under a left-facing root: A toggles back to R.
		preview::Options o2 = opt; o2.rootFacingLeft = true;
		sim.setInputs(&d, &fx, 10, o2);
		sim.getStateAt(0, st);
		A = ByPattern(st, 11); B = ByPattern(st, 12);
		CHECK(A && !A->facingLeft && A->x == 100.f, "left root + bit11 -> child faces right");
		CHECK(B && !B->facingLeft && B->x == 150.f, "grandchild under flipped chain");
	}

	// 2) Self-jump re-fires EFs: frame 0 (dur 3) aniType 2 -> frame 0.
	{
		Frame& r = AddFrame(d, 20, 3, 2, 0, 0);
		AddEF(r, 1, 21, {0, 0, 0, 0});
		AddFrame(d, 21, 100, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 20, opt);
		sim.getStateAt(9, st);
		int n = 0; for (auto& a : st.actors) if (a.pattern == 21) n++;
		CHECK(n == 4, "self-jump re-fires spawn EF each visit (ticks 0,3,6,9)");
	}

	// 3) Loop counter (engine semantics): f0 AFCT=3, f1 loops to itself with
	//    flag 2 -> 3 visits of f1 then loopEnd f2.
	{
		Frame& f0 = AddFrame(d, 30, 1, 1); f0.AF.loopCount = 3;
		Frame& f1 = AddFrame(d, 30, 1, 2, 2, 1); f1.AF.loopEnd = 2;
		AddEF(f1, 1, 31, {0, 0, 0, 0});
		AddFrame(d, 30, 1, 0);
		AddFrame(d, 31, 50, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 30, opt);
		sim.ensureSimulatedTo(10);
		std::string t; for (int i = 0; i < 6 && i < (int)sim.rootFrameTrack().size(); i++) t += std::to_string(sim.rootFrameTrack()[i]);
		CHECK(t == "011122", ("loop counter 3 -> f1 x3 then loopEnd, root holds (track " + t + ")").c_str());
	}

	// 4) Offset scale: a PAT spawner frame halves EF offsets.
	{
		Frame& r = AddFrame(d, 40, 5, 0);
		r.AF.layers[0].usePat = true;
		AddEF(r, 1, 41, {100, 40, 0, 0});
		AddFrame(d, 41, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 40, opt);
		sim.getStateAt(0, st);
		auto* A = ByPattern(st, 41);
		CHECK(A && A->x == 50.f && A->y == 20.f && A->offsetScale == 0.5f, "PAT spawner frame -> 0.5 offset scale");
	}

	// 5) Positioning modes.
	{
		Frame& r = AddFrame(d, 50, 5, 0);
		AddEF(r, 1, 51, {200, 10, 0, 0x200});        // fixed: X-128
		AddEF(r, 1, 52, {10, 5, 0x10, 0});           // camera-relative
		AddEF(r, 1, 53, {10, 0, 0x200, 0});          // back screen edge
		AddEF(r, 1, 54, {10, 0, 0, 0x100});          // + opponent distance
		AddEF(r, 1, 55, {10, 77, (int)0x80000000, 0}); // absolute Y
		for (int p = 51; p <= 55; p++) AddFrame(d, p, 5, 0);
		preview::Options o = opt; o.cameraX = 30.f; o.cameraY = -40.f; o.opponentX = 120.f;
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 50, o);
		sim.getStateAt(0, st);
		auto* a1 = ByPattern(st, 51); auto* a2 = ByPattern(st, 52); auto* a3 = ByPattern(st, 53);
		auto* a4 = ByPattern(st, 54); auto* a5 = ByPattern(st, 55);
		CHECK(a1 && a1->x == 72.f && a1->y == 10.f, "flagset2 0x200 fixed position (X-128, Y)");
		CHECK(a2 && a2->x == 40.f && a2->y == -35.f, "flagset1 0x10 camera-relative");
		CHECK(a3 && a3->x == 30.f - 160.f + 8.f + 10.f, "flagset1 0x200 screen edge (left for right-facing)");
		CHECK(a4 && a4->x == 130.f, "flagset2 0x100 adds signed opponent distance");
		CHECK(a5 && a5->y == 77.f, "flagset1 sign bit -> absolute Y");
	}

	// 6) IF2 p3 destroys at pattern end; without it the actor chains to AF jump.
	{
		Frame& r = AddFrame(d, 60, 20, 0);
		AddEF(r, 1, 61, {0, 0, 0, 0});
		AddEF(r, 1, 62, {0, 0, 0, 0});
		Frame& a = AddFrame(d, 61, 2, 0); AddIF(a, 2, {0, 0, 1});
		AddFrame(d, 62, 2, 0, 0, 63);
		AddFrame(d, 63, 50, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 60, opt);
		sim.getStateAt(3, st);
		CHECK(!ByPattern(st, 61), "IF2 p3 -> destroyed at pattern end");
		auto* c = ByPattern(st, 63);
		CHECK(c && !ByPattern(st, 62), "aniType 0 without IF2 p3 -> starts AF jump pattern");
	}

	// 7) Link 0x20 children die when the root changes pattern (ends).
	{
		Frame& r = AddFrame(d, 70, 4, 0);
		AddEF(r, 1, 71, {0, 0, 0x20, 0});
		AddEF(r, 1, 72, {0, 0, 0, 0});
		AddFrame(d, 71, 100, 0);
		AddFrame(d, 72, 100, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 70, opt);
		sim.getStateAt(3, st);
		CHECK(ByPattern(st, 71) && ByPattern(st, 72), "children alive before root end");
		sim.getStateAt(5, st);
		CHECK(!ByPattern(st, 71) && ByPattern(st, 72), "link 0x20 child removed on root pattern change");
		CHECK(sim.rootEndTick() == 4, "root end tick");
	}

	// 8) EF11: p6=3 random picks from range 2; p6=0 spawns each once.
	{
		Frame& r = AddFrame(d, 80, 5, 0);
		AddEF(r, 11, 81, {0, 0, 10, 10, 2, 3, 0, 0, 0, 0, 0});
		AddEF(r, 11, 81, {0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0});
		AddFrame(d, 81, 5, 0); AddFrame(d, 82, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 80, opt);
		sim.getStateAt(0, st);
		int n = 0, n81 = 0, n82 = 0;
		for (auto& a : st.actors) if (!a.isRoot) { n++; if (a.pattern == 81) n81++; if (a.pattern == 82) n82++; }
		CHECK(n == 5 && n81 >= 1 && n82 >= 1, "EF11 count p6 / sequential range when p6=0");
	}

	// 9) Runtime IF: assumed false follows AF flow; assumed true jumps.
	{
		Frame& f0 = AddFrame(d, 90, 5, 1);
		AddIF(f0, 4, {2, 0, 0});        // velocity check -> frame 2 (runtime)
		AddFrame(d, 90, 5, 1);
		AddFrame(d, 90, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 90, opt);
		sim.getStateAt(1, st);
		CHECK(st.root()->frame == 0, "runtime IF assumed false -> stays on authored flow");
		bool logged = false;
		for (auto& e : sim.events()) if (e.kind == preview::EventKind::IfUnmodeled && e.ifType == 4) logged = true;
		CHECK(logged, "unmodeled IF reported as event");
		preview::Options o = opt;
		o.ifOverrides[preview::IfKey{false, 90, 0, 0}] = preview::IfAssume::True;
		sim.setInputs(&d, &fx, 90, o);
		sim.getStateAt(1, st);
		CHECK(st.root()->frame == 2, "IF override True -> jump on first evaluation (tick 1)");
	}

	// 10) Invalidation: mark_modified bumps dataVersion; a direct edit that
	//     bypasses it is caught by the sequence fingerprint.
	{
		Frame& r = AddFrame(d, 100, 5, 0);
		AddEF(r, 1, 101, {10, 0, 0, 0});
		AddFrame(d, 101, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 100, opt);
		sim.getStateAt(0, st);
		CHECK(ByPattern(st, 101)->x == 10.f, "baseline");
		d.get_sequence(100)->frames[0].EF[0].parameters[0] = 30;
		d.mark_modified(100);
		sim.getStateAt(0, st);
		CHECK(ByPattern(st, 101)->x == 30.f, "edit + mark_modified invalidates");
		d.get_sequence(100)->frames[0].EF[0].parameters[0] = 45; // no mark_modified
		sim.getStateAt(0, st);
		CHECK(ByPattern(st, 101)->x == 45.f, "silent edit caught by fingerprint");
	}

	// 11) EF1000 spawns once per spawner var even when the frame re-enters.
	{
		Frame& r = AddFrame(d, 110, 2, 2, 0, 0);
		AddEF(r, 1000, 111, {0, 0, 0, 0, 0, 3});
		AddFrame(d, 111, 100, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 110, opt);
		sim.getStateAt(8, st);
		int n = 0; for (auto& a : st.actors) if (a.pattern == 111) n++;
		CHECK(n == 1, "EF1000 spawn-once guard");
	}

	// 12) EF8 into effect.ha6 + relative EF108, angle p8, inherit angle bit 8.
	{
		Frame& r = AddFrame(d, 120, 5, 0);
		AddEF(r, 8, 5, {0, 0, 0, 0, 0, 0, 0, 2500});
		Sequence* e5 = fx.get_sequence(5); (void)e5;
		AddFrame(fx, 5, 5, 0);
		Frame& e = fx.get_sequence(5)->frames.back();
		AddEF(e, 1, 6, {0, 0, 0x100, 0, 0, 0, 0, 100});
		AddFrame(fx, 6, 5, 0);
		preview::PreviewSim sim;
		sim.setInputs(&d, &fx, 120, opt);
		sim.getStateAt(0, st);
		auto* A = ByPattern(st, 5); auto* B = ByPattern(st, 6);
		CHECK(A && A->effectHa6 && A->angle == 2500, "EF8 -> effect.ha6, angle from p8");
		CHECK(B && B->effectHa6 && B->angle == 2600, "EF1 inside effect.ha6 inherits data + parent angle (bit 8)");
	}

	// 13) Position-tool placement (ResolveSpawnPlacement) agrees with the
	//     simulated actor for every flag combination the tool can edit.
	{
		const int combos[][2] = {{0, 0}, {0x800, 0}, {0, 0x800}, {0x800, 0x800}, {0x10, 0},
		                         {0x10, (int)0x80000000}, {0x200, 0}, {0x400, 0x800}, {0, 0x100},
		                         {0, 0x200}, {(int)0x80000000, 0}, {0x2 | 0x800, 0}};
		preview::Options o = opt; o.cameraX = 12.f; o.opponentX = 90.f;
		bool allOk = true;
		for (int facingLeft = 0; facingLeft < 2; facingLeft++) {
			for (int pat = 0; pat < 2; pat++) {
				for (const auto& cb : combos) {
					FrameData t; t.initEmpty(); FrameData tfx; tfx.initEmpty();
					Frame& r = AddFrame(t, 1, 5, 0);
					r.AF.layers[0].usePat = pat != 0;
					AddEF(r, 1, 2, {37, -21, cb[0], cb[1]});
					AddFrame(t, 2, 5, 0);
					preview::Options oo = o; oo.rootFacingLeft = facingLeft != 0;
					preview::PreviewSim sim;
					sim.setInputs(&t, &tfx, 1, oo);
					sim.getStateAt(0, st);
					auto* A = ByPattern(st, 2);
					preview::SimActor owner; owner.isRoot = true; owner.facingLeft = oo.rootFacingLeft;
					auto pl = preview::ResolveSpawnPlacement(oo, owner, pat != 0, cb[0], cb[1]);
					float hx = pl.baseX + pl.kx * 37, hy = pl.baseY + pl.ky * -21;
					if (!A || std::fabs(A->x - hx) > 1e-4f || std::fabs(A->y - hy) > 1e-4f || A->facingLeft != pl.facingLeft) {
						std::printf("  placement mismatch fs1=%x fs2=%x facing=%d pat=%d: actor (%g,%g) handle (%g,%g)\n",
						            cb[0], cb[1], facingLeft, pat, A ? A->x : -1.f, A ? A->y : -1.f, hx, hy);
						allOk = false;
					}
				}
			}
		}
		CHECK(allOk, "position-tool placement == simulated spawn position (48 combos)");
	}

	std::printf("\nselftest: %s (%d failure(s))\n", g_fail ? "FAILED" : "passed", g_fail);
	return g_fail ? 4 : 0;
}

struct Args {
	std::string data, chr;
	int moon = 0;
	int pattern = -1;
	int ticks = -1;
	int every = 1;
	bool scan = false, bench = false, assumeTrue = false, events = false, selftest = false;
};

} // namespace

int main(int argc, char** argv)
{
	Args a;
	for (int i = 1; i < argc; i++) {
		std::string s = argv[i];
		auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
		if (s == "--data") a.data = next();
		else if (s == "--char") a.chr = next();
		else if (s == "--moon") a.moon = std::atoi(next().c_str());
		else if (s == "--pattern") a.pattern = std::atoi(next().c_str());
		else if (s == "--ticks") a.ticks = std::atoi(next().c_str());
		else if (s == "--every") a.every = std::max(1, std::atoi(next().c_str()));
		else if (s == "--scan") a.scan = true;
		else if (s == "--bench") a.bench = true;
		else if (s == "--assume-true") a.assumeTrue = true;
		else if (s == "--events") a.events = true;
		else if (s == "--selftest") a.selftest = true;
		else { std::cerr << "unknown argument " << s << "\n"; return 2; }
	}
	if (a.selftest) return SelfTest();
	if (a.data.empty() || a.chr.empty() || (!a.scan && a.pattern < 0)) {
		std::cerr << "usage: preview_sim_tool --data DIR --char NAME [--moon M] (--pattern P | --scan)"
		             " [--ticks T] [--every K] [--assume-true] [--events] [--bench]\n";
		return 2;
	}

	FrameData main, effect;
	auto files = DataFileList(a.data, a.chr, a.moon);
	if (files.empty()) { std::cerr << "no HA6 files for " << a.chr << "\n"; return 1; }
	bool first = true;
	for (const auto& f : files) {
		if (!main.load(f.c_str(), !first)) { std::cerr << "failed to load " << f << "\n"; return 1; }
		first = false;
	}
	std::string eff = FindCaseInsensitive(a.data, "effect.HA6");
	bool haveEffect = !eff.empty() && effect.load(eff.c_str());
	std::printf("loaded %zu file(s) for %s_%d, effect.ha6 %s\n", files.size(), a.chr.c_str(), a.moon,
	            haveEffect ? "yes" : "no");

	preview::Options opt;
	if (a.assumeTrue) opt.defaultIfAssumption = preview::IfAssume::True;
	if (a.ticks > 0) opt.horizonTicks = a.ticks;

	if (!a.scan) {
		preview::PreviewSim sim;
		sim.setInputs(&main, haveEffect ? &effect : nullptr, a.pattern, opt);
		int settled = sim.settledTick();
		int end = a.ticks > 0 ? a.ticks : (settled >= 0 ? settled : 240);
		std::printf("pattern %d: root ends at %d, settled at %d, %zu actors ever spawned\n",
		            a.pattern, sim.rootEndTick(), settled, sim.spawnRecords().size());
		preview::TickState st;
		for (int t = 0; t <= end; t += a.every) {
			sim.getStateAt(t, st);
			PrintState(st);
		}
		if (a.events) {
			for (const auto& e : sim.events())
				std::printf("event t%-4d %-12s actor %-4d a=%d b=%d IF%d@p%d/f%d/#%d %s\n", e.tick,
				            preview::EventKindName(e.kind), e.actorId, e.a, e.b, e.ifType,
				            e.ifKey.pattern, e.ifKey.frame, e.ifKey.index, e.detail.c_str());
		}
		if (!a.bench) return 0;
	}

	// Scan / verification / bench over one or all patterns.
	std::vector<int> patterns;
	if (a.scan) {
		for (int p = 0; p < main.get_sequence_count(); p++) {
			auto* s = main.get_sequence(p);
			if (s && !s->frames.empty()) patterns.push_back(p);
		}
	} else {
		patterns.push_back(a.pattern);
	}

	std::mt19937 rng(1234);
	int mismatches = 0, withSpawns = 0, maxActorsSeen = 0, maxActorsPat = -1;
	long totalRecords = 0, totalUnmodeled = 0;
	double worstRandomMs = 0, sumRandomMs = 0; long randomQueries = 0;
	double worstFrontierMs = 0; int worstFrontierPat = -1;
	double naiveMsTotal = 0; long naiveQueries = 0;

	for (int pat : patterns) {
		preview::PreviewSim sim;
		sim.setInputs(&main, haveEffect ? &effect : nullptr, pat, opt);
		double t0 = NowMs();
		int settled = sim.settledTick();
		double frontierMs = NowMs() - t0;
		int span = settled >= 0 ? settled : std::min(opt.horizonTicks, 600);
		if (frontierMs > worstFrontierMs) { worstFrontierMs = frontierMs; worstFrontierPat = pat; }

		const auto& recs = sim.spawnRecords();
		totalRecords += (long)recs.size() - 1;
		if (recs.size() > 1) withSpawns++;
		for (const auto& e : sim.events())
			if (e.kind == preview::EventKind::IfUnmodeled) totalUnmodeled++;

		// Sequential reference (frontier replay through cursor 0).
		std::vector<preview::TickState> seq(span + 1);
		for (int t = 0; t <= span; t++) {
			sim.getStateAt(t, seq[t]);
			if ((int)seq[t].actors.size() > maxActorsSeen) { maxActorsSeen = (int)seq[t].actors.size(); maxActorsPat = pat; }
		}
		// Random-order queries must match.
		preview::TickState st;
		std::uniform_int_distribution<int> dist(0, span);
		for (int q = 0; q < 64; q++) {
			int t = dist(rng);
			double q0 = NowMs();
			sim.getStateAt(t, st);
			double qms = NowMs() - q0;
			worstRandomMs = std::max(worstRandomMs, qms);
			sumRandomMs += qms; randomQueries++;
			std::string why;
			if (!SameState(st, seq[t], &why)) {
				if (mismatches < 10)
					std::printf("MISMATCH pattern %d tick %d: %s\n", pat, t, why.c_str());
				mismatches++;
			}
		}
		// A fresh simulator stepping straight to t (no checkpoints between)
		// must agree too: this is the "re-run from tick 0" reference.
		if (a.bench || patterns.size() == 1) {
			for (int q = 0; q < 8; q++) {
				int t = dist(rng);
				preview::Options o2 = opt;
				o2.checkpointInterval = 1 << 20;
				preview::PreviewSim fresh;
				fresh.setInputs(&main, haveEffect ? &effect : nullptr, pat, o2);
				double n0 = NowMs();
				fresh.getStateAt(t, st);
				naiveMsTotal += NowMs() - n0; naiveQueries++;
				std::string why;
				if (!SameState(st, seq[t], &why)) {
					if (mismatches < 10)
						std::printf("MISMATCH(fresh) pattern %d tick %d: %s\n", pat, t, why.c_str());
					mismatches++;
				}
			}
		}
		if (patterns.size() == 1) {
			std::printf("pattern %d: settled %d, frontier build %.3f ms, %zu records\n", pat, settled,
			            frontierMs, recs.size());
		}
	}

	std::printf("\n== %s_%d: %zu pattern(s) ==\n", a.chr.c_str(), a.moon, patterns.size());
	std::printf("patterns with spawns: %d, spawned actors total: %ld, unmodeled IF sites: %ld\n",
	            withSpawns, totalRecords, totalUnmodeled);
	std::printf("max simultaneous actors: %d (pattern %d)\n", maxActorsSeen, maxActorsPat);
	std::printf("frontier build worst: %.3f ms (pattern %d)\n", worstFrontierMs, worstFrontierPat);
	std::printf("random scrub query: avg %.4f ms, worst %.4f ms over %ld queries\n",
	            randomQueries ? sumRandomMs / randomQueries : 0.0, worstRandomMs, randomQueries);
	if (naiveQueries)
		std::printf("re-run-from-0 reference query: avg %.4f ms over %ld queries\n",
		            naiveMsTotal / naiveQueries, naiveQueries);
	std::printf("determinism/checkpoint mismatches: %d\n", mismatches);

	if (a.bench && patterns.size() == 1) {
		// Scrub bench on one pattern: sequential playback, random seeks,
		// 17-pass onion-style bursts, and invalidate+rebuild.
		int pat = patterns[0];
		preview::PreviewSim sim;
		sim.setInputs(&main, haveEffect ? &effect : nullptr, pat, opt);
		int span = sim.settledTick();
		if (span < 0) span = std::min(opt.horizonTicks, 600);
		preview::TickState st;
		double t0 = NowMs();
		for (int r = 0; r < 10; r++)
			for (int t = 0; t <= span; t++) sim.getStateAt(t, st);
		double seqMs = (NowMs() - t0) / (10.0 * (span + 1));
		std::uniform_int_distribution<int> dist(0, span);
		t0 = NowMs();
		for (int q = 0; q < 2000; q++) sim.getStateAt(dist(rng), st);
		double rndMs = (NowMs() - t0) / 2000.0;
		t0 = NowMs();
		for (int f = 0; f < 200; f++) {
			int c = dist(rng);
			for (int k = -8; k <= 8; k++) sim.getStateAt(std::clamp(c + k * 2, 0, span), st);
		}
		double onionMs = (NowMs() - t0) / 200.0;
		t0 = NowMs();
		for (int r = 0; r < 20; r++) { sim.invalidate(); sim.getStateAt(span, st); }
		double rebuildMs = (NowMs() - t0) / 20.0;
		t0 = NowMs();
		for (int r = 0; r < 1000; r++) sim.validate();
		double validateMs = (NowMs() - t0) / 1000.0;
		std::printf("\nbench pattern %d (span %d ticks):\n", pat, span);
		std::printf("  sequential playback: %.4f ms/tick\n", seqMs);
		std::printf("  random seek:         %.4f ms/query\n", rndMs);
		std::printf("  onion burst (17 q):  %.4f ms/UI frame\n", onionMs);
		std::printf("  invalidate+rebuild:  %.4f ms\n", rebuildMs);
		std::printf("  validate (per query overhead): %.4f ms\n", validateMs);

		// EX baseline: one query per draw re-simulates from tick 0.
		for (int probe : {span / 4, span / 2, span}) {
			double e0 = NowMs();
			size_t n = 0;
			const int reps = 3;
			for (int r = 0; r < reps; r++) n = ExSimulateSpawnsToTick(&main, haveEffect ? &effect : nullptr, pat, probe);
			double exMs = (NowMs() - e0) / reps;
			std::printf("  EX re-run-from-0 at tick %4d: %8.3f ms/query (%zu spawns); x17 onion = %.1f ms/UI frame\n",
			            probe, exMs, n, exMs * 17);
		}
	}
	return mismatches ? 3 : 0;
}
