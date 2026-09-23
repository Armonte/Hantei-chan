// Standalone checks for the bg::File stage loader/runtime (no GL).
//
//   bg_test <stage.dat>                       dump objects/frames
//   bg_test <stage.dat> <out.dat>             save, byte-compare with input, reload
//   bg_test <stage.dat> --sim N [--seed S] [--game mbaacc|mbac]
//                                             run N ticks, report live instances,
//                                             weather particles, RNG draws and a
//                                             state hash (same seed -> same hash)
//   bg_test <stage.dat> --info                side files (BgList / Info / light)
//   bg_test <stage.dat> --edit-test <out.dat> edit fields + records, save, reload, verify,
//                                             then undo (must match the input) / redo
//   bg_test --rng <seed> [n]                  first n values of the MBAACC stage RNG
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>
#include "background/bg_file.h"
#include "background/bg_types.h"
#include "background/bg_rng.h"

static bool ReadAll(const char* p, std::vector<char>& out) {
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static uint64_t StateHash(const bg::File& file) {
	uint64_t h = 1469598103934665603ull;
	auto mix = [&](int64_t v) { h ^= (uint64_t)v; h *= 1099511628211ull; };
	for (const auto& in : file.GetInstances()) {
		mix(in.state); mix(in.objIndex); mix(in.curFrame); mix(in.posX); mix(in.posY);
		mix(in.velX); mix(in.velY); mix(in.timer);
	}
	for (const auto& p : file.GetDrops().Particles()) {
		int32_t x, y; std::memcpy(&x, &p.x, 4); std::memcpy(&y, &p.y, 4);
		mix(x); mix(y); mix(p.pat); mix(p.alpha);
	}
	return h;
}

int main(int argc, char** argv)
{
	if (argc >= 3 && strcmp(argv[1], "--rng") == 0) {
		bg::Rng r;
		r.Seed(atoi(argv[2]), bg::Game::MBAACC);
		int n = argc >= 4 ? atoi(argv[3]) : 5;
		for (int i = 0; i < n; ++i) printf("%d\n", r.Next());
		return 0;
	}
	if (argc < 2) {
		fprintf(stderr, "usage: bg_test <stage.dat> [<out.dat> | --sim N [--seed S] [--game g] | --info | --edit-test <out.dat>]\n");
		return 1;
	}

	bg::File file;
	if (!file.Load(argv[1])) {
		fprintf(stderr, "load failed: %s\n", argv[1]);
		_exit(2);
	}
	const auto& objects = file.GetObjects();
	size_t totalFrames = 0;
	for (const auto& obj : objects) totalFrames += obj.frames.size();
	printf("%s: objects=%zu frames=%zu game=%s\n", argv[1], objects.size(), totalFrames,
	       file.GetGame() == bg::Game::MBAC ? "MBAC" : "MBAACC");

	if (argc >= 3 && strcmp(argv[2], "--info") == 0) {
		const auto* e = file.GetStageListEntry();
		if (e) printf("BgList [Bg_%03d] DataFile=%s sel=%d giant=%d info=%d color=%.2f\n", e->index,
		              e->dataFile.c_str(), e->isSelectable, e->isGiantStage, e->infoFile, e->stageColorVal);
		else printf("BgList: %s\n", file.GetStageList().path.empty() ? "(none)" : "(no entry)");
		const auto& si = file.GetStageInfo();
		if (si.loaded) {
			printf("Info %s: lights=%zu dropObj=%d type=%d file=%s pat=%d frames=%d wait=%d w=%d h=%d count=%d alpha=%d bmp=%s\n",
			       si.path.c_str(), si.lights.size(), si.dropObj, si.dropType, si.dropFile.c_str(),
			       si.patNum, si.frameNum, si.wait, si.w, si.h, si.count, si.alpha,
			       file.DropBitmapPath().empty() ? "-" : "found");
			for (auto& l : si.lights) printf("  light pos=%d power=%d\n", l.pos, l.power);
		}
		const auto& lf = file.GetLightFile();
		if (lf.loaded) {
			printf("light.txt %s: %zu\n", lf.path.c_str(), lf.lights.size());
			for (auto& l : lf.lights) printf("  light pos=%d power=%d\n", l.pos, l.power);
		}
		std::string sib = file.SiblingVariantPath();
		printf("variant: %s  sibling: %s\n", file.IsShortVariant() ? "_s" : "full", sib.empty() ? "-" : sib.c_str());
		_exit(0);
	}

	if (argc >= 3 && strcmp(argv[2], "--sim") == 0) {
		int n = 3600;
		int argi = 3;
		if (argi < argc && argv[argi][0] != '-') n = atoi(argv[argi++]);
		for (; argi + 1 < argc; argi += 2) {
			if (strcmp(argv[argi], "--seed") == 0) file.SetSeed(atoi(argv[argi + 1]));
			else if (strcmp(argv[argi], "--game") == 0)
				file.SetGame(strcmp(argv[argi + 1], "mbac") == 0 ? bg::Game::MBAC : bg::Game::MBAACC);
		}
		auto live = [&]() { int c = 0; for (auto& in : file.GetInstances()) if (in.state >= 1) ++c; return c; };
		int maxLive = live(), minLive = maxLive;
		for (int t = 1; t <= n; ++t) {
			file.TickRuntime();
			int l = live();
			maxLive = std::max(maxLive, l);
			if (t > 60) minLive = std::min(minLive, l);
			if (t % 600 == 0)
				printf("  tick %d live=%d pool=%zu drops=%zu rng=%llu\n", t, l, file.GetInstances().size(),
				       file.GetDrops().Particles().size(), (unsigned long long)file.GetRng().Calls());
		}
		printf("SIM seed=%d ticks=%d live[min=%d max=%d] hash=%016llx\n", file.GetSeed(), n, minLive,
		       maxLive, (unsigned long long)StateHash(file));
		// Bounded: the game's pool holds 2000; anything near it means a runaway spawner.
		_exit(maxLive >= 1900 ? 7 : 0);
	}

	if (argc >= 4 && strcmp(argv[2], "--edit-test") == 0) {
		auto& objs = file.GetObjects();
		// Edit: first object with commands gets its first command's d[1]
		// changed in place; the first object gets a new trigger record and
		// an extra frame; object flags / frame fields change.
		int withCmd = -1;
		for (size_t i = 0; i < objs.size(); ++i) if (!objs[i].commands.empty()) { withCmd = (int)i; break; }
		if (withCmd >= 0) {
			objs[withCmd].commands[0].d[1] += 7;
			objs[withCmd].commands[0].SyncRaw();
			objs[withCmd].recordsEdited = true;
		}
		file.InsertFrame(0, 0, true);
		int r = file.AddRecord(0, true);
		objs[0].frames[0].triggerRef[0] = (int16_t)r;
		objs[0].triggers[r].d[2] = 12345;
		objs[0].triggers[r].SyncRaw();
		objs[0].frames[0].velX = 99;
		objs[0].linearFilter = 1;
		int32_t cmdD1 = withCmd >= 0 ? objs[withCmd].commands[0].d[1] : 0;
		if (!file.Save(argv[3])) { fprintf(stderr, "save failed\n"); _exit(3); }
		bg::File re;
		if (!re.Load(argv[3])) { fprintf(stderr, "reload failed\n"); _exit(4); }
		auto& ro = re.GetObjects();
		int bad = 0;
		if (ro.size() != objs.size()) ++bad;
		if (ro[0].frames.size() != objs[0].frames.size()) ++bad;
		if (ro[0].frames[0].velX != 99 || ro[0].linearFilter != 1) ++bad;
		if ((int)ro[0].triggers.size() <= r || ro[0].triggers[r].d[2] != 12345) ++bad;
		if (withCmd >= 0 && (ro[withCmd].commands.empty() || ro[withCmd].commands[0].d[1] != cmdD1)) ++bad;
		for (size_t i = 1; i < ro.size() && i < objs.size(); ++i)
			if (ro[i].frames.size() != objs[i].frames.size() || ro[i].commands.size() != objs[i].commands.size()) ++bad;
		// The edited stage still runs.
		for (int t = 0; t < 600; ++t) re.TickRuntime();
		// History: the edits above are one step; undoing it must give back a
		// file byte-identical to the input, redo the edited one.
		file.CommitEdit();
		std::string undoPath = std::string(argv[3]) + ".undo.dat";
		std::vector<char> in, un, ed, rd;
		if (!file.Undo() || !file.Save(undoPath.c_str()) || !ReadAll(argv[1], in) || !ReadAll(undoPath.c_str(), un) || in != un) {
			fprintf(stderr, "undo did not restore the original bytes\n"); ++bad;
		}
		if (!file.Redo() || !file.Save(undoPath.c_str()) || !ReadAll(argv[3], ed) || !ReadAll(undoPath.c_str(), rd) || ed != rd) {
			fprintf(stderr, "redo did not restore the edited bytes\n"); ++bad;
		}
		std::remove(undoPath.c_str());
		printf("EDIT-TEST %s (%d problems)\n", bad ? "FAIL" : "OK", bad);
		_exit(bad ? 8 : 0);
	}

	if (argc >= 3) {
		const char* out = argv[2];
		if (!file.Save(out)) { fprintf(stderr, "save failed\n"); _exit(3); }
		std::vector<char> a, b;
		if (!ReadAll(argv[1], a) || !ReadAll(out, b)) { fprintf(stderr, "read back failed\n"); _exit(4); }
		if (a != b) {
			size_t i = 0;
			while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
			printf("ROUNDTRIP DIFF sizes %zu/%zu first diff at 0x%zx\n", a.size(), b.size(), i);
			_exit(5);
		}
		bg::File reload;
		if (!reload.Load(out)) { fprintf(stderr, "re-load failed\n"); _exit(4); }
		printf("ROUNDTRIP OK (%zu bytes identical)\n", a.size());
		_exit(0);
	}

	for (size_t i = 0; i < objects.size(); ++i) {
		const auto& obj = objects[i];
		printf("  obj[%zu] slot=%d layer=%d parallax=%d frames=%zu fg=%d noauto=%d lin=%d trg=%zu cmd=%zu\n",
		       i, obj.originalIndex, obj.layer, obj.parallax, obj.frames.size(), obj.foreground,
		       obj.noAutoSpawn, obj.linearFilter, obj.triggers.size(), obj.commands.size());
		for (size_t fi = 0; fi < obj.frames.size(); ++fi) {
			const auto& f = obj.frames[fi];
			printf("    frame[%zu]: spr=%d off=(%d,%d) dur=%u blend=%d a=%d ani=%d jump=%d lerp=%d vel=(%d,%d) acc=(%d,%d)\n",
			       fi, f.spriteId, f.offsetX, f.offsetY, (unsigned)(uint16_t)f.duration,
			       f.blendMode, f.opacity, f.aniType, f.jumpFrame, f.interpolate,
			       f.velX, f.velY, f.accX, f.accY);
		}
	}
	_exit(0);
}
