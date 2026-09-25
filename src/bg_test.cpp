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
#include "background/bg_project.h"

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
	// --camera-test: the stage view's game camera must follow pans but never
	// zooms (zoom-to-cursor rewrites the pan every frame of the animation).
	if (argc >= 2 && strcmp(argv[1], "--camera-test") == 0) {
		bg::Camera c;
		c.clampToGame = false;
		int bad = 0;
		c.SetPan(400, 540); c.zoom = 1.0f; c.SetGameCamFromView(1280, 720);
		c.SetPan(380, 520); c.SetGameCamFromView(1280, 720);            // pan by (-20,-20)
		if (c.camX != 20 || c.camY != 20) { printf("pan did not move the camera (%g,%g)\n", c.camX, c.camY); ++bad; }
		const float cx = c.camX, cy = c.camY;
		// zoom about a cursor at (900, 200) from 1.0 to 2.0 over 12 frames
		for (int f = 1; f <= 12; ++f) {
			float z = 1.0f + f / 12.0f, wx = 900.0f / 1.0f - 380.0f, wy = 200.0f - 520.0f;
			c.zoom = z;
			c.SetPan(900.0f / z - wx, 200.0f / z - wy);
			c.SetGameCamFromView(1280, 720);
		}
		if (c.camX != cx || c.camY != cy) { printf("zoom moved the camera (%g,%g)\n", c.camX, c.camY); ++bad; }
		// parallax shift at a fixed camera does not depend on zoom
		if (c.ParallaxX(128) != (1.0f - 0.5f) * (cx - 1.0f)) ++bad;
		// clamp: panning past the game's limits stops the camera at them
		c.clampToGame = true;
		c.SetPan(-2000, 3000); c.SetGameCamFromView(1280, 720);
		if (c.camX != 208.0f || c.camY != -340.0f) { printf("clamp failed (%g,%g)\n", c.camX, c.camY); ++bad; }
		printf("CAMERA-TEST %s (%d problems)\n", bad ? "FAIL" : "OK", bad);
		_exit(bad ? 8 : 0);
	}

	// --project-test <stage.dat|game dir> <scratch dir>: BgList.ini / bgm.txt
	// open, unedited save byte-identical, edits touch only their value, undo
	// restores the original bytes, add/move/remove round trip.
	if (argc >= 4 && strcmp(argv[1], "--project-test") == 0) {
		bg::StageProject pr;
		if (!pr.Open(argv[2], bg::Game::MBAACC)) { fprintf(stderr, "open failed\n"); _exit(2); }
		printf("project %s: %zu entries, bgm %s\n", pr.BgDir().c_str(), pr.Entries().size(),
		       pr.Bgm().IsLoaded() ? "loaded" : "missing");
		int bad = 0;
		const std::string origList = pr.BgList().Text(), origBgm = pr.Bgm().Text();
		std::string sd = argv[3];
		auto same = [&](const std::string& path, const std::string& want) {
			std::vector<char> d; if (!ReadAll(path.c_str(), d)) return false;
			return std::string(d.begin(), d.end()) == want;
		};
		// unedited save
		if (!pr.BgList().Save(sd + "/BgList.ini") || !same(sd + "/BgList.ini", origList)) { printf("unedited BgList save differs\n"); ++bad; }
		if (pr.Bgm().IsLoaded() && (!pr.Bgm().Save(sd + "/bgm.txt") || !same(sd + "/bgm.txt", origBgm))) { printf("unedited bgm save differs\n"); ++bad; }
		// edits
		pr.SetListValue(28, "StageColorVal", "0.75");
		pr.SetListValue(28, "IsGiantStage", "1");
		pr.SetBgmValue(28, "LoopPos", "12.500");
		const bg::StageEntry* e = pr.Find(28);
		if (!e || e->colorVal != 0.75f || e->giant != 1 || e->bgmLoopPos != "12.500") { printf("edit not visible\n"); ++bad; }
		size_t delta = pr.BgList().Text().size() > origList.size() ? pr.BgList().Text().size() - origList.size() : origList.size() - pr.BgList().Text().size();
		printf("BgList grew by %zu bytes for 2 edits\n", delta);
		pr.AddStage(97, "bg28");
		pr.MoveStage(97, 98);
		if (!pr.Find(98) || !pr.Find(98)->listed || (pr.Find(97) && pr.Find(97)->listed)) { printf("add/move failed\n"); ++bad; }
		pr.RemoveStage(98);
		for (int i = 0; i < 7; ++i) pr.Undo();
		if (pr.BgList().Text() != origList || pr.Bgm().Text() != origBgm) { printf("undo did not restore the original bytes\n"); ++bad; }
		for (int i = 0; i < 2; ++i) pr.Redo();
		if (!pr.BgList().Save(sd + "/BgList.ini")) ++bad;
		printf("PROJECT-TEST %s (%d problems)\n", bad ? "FAIL" : "OK", bad);
		_exit(bad ? 8 : 0);
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

	// Game-state comparison (tools/stage_capture dumps):
	//   --find-tick <prefix> [max]   sim from reset; report ticks whose live pool
	//                                matches the game's <prefix>.pool exactly
	//   --state <prefix> --step N --compare <prefix2>
	//                                import pool + drops + RNG, tick N, diff against prefix2
	if (argc >= 4 && (strcmp(argv[2], "--find-tick") == 0 || strcmp(argv[2], "--state") == 0)) {
		struct G { int st, obj, cur, loop; int32_t px, py, vx, vy, t; };
		auto loadPool = [&](const std::string& pre, std::vector<G>& out) -> bool {
			std::vector<char> b;
			if (!ReadAll((pre + ".pool").c_str(), b) || b.size() < 44) return false;
			out.clear();
			for (size_t i = 0; i + 44 <= b.size(); i += 44) {
				const uint8_t* r = (const uint8_t*)b.data() + i;
				G g{}; g.st = r[0]; g.obj = r[1]; g.cur = r[2]; g.loop = r[4];
				std::memcpy(&g.px, r + 16, 4); std::memcpy(&g.py, r + 20, 4);
				std::memcpy(&g.vx, r + 24, 4); std::memcpy(&g.vy, r + 28, 4);
				std::memcpy(&g.t, r + 40, 4);
				out.push_back(g);
			}
			return true;
		};
		auto diff = [&](const std::vector<G>& gp, bool verbose) -> int {
			const auto& ins = file.GetInstances();
			const auto& objs = file.GetObjects();
			int bad = 0;
			size_t n = std::max(gp.size(), ins.size());
			for (size_t i = 0; i < n; ++i) {
				G g = i < gp.size() ? gp[i] : G{};
				bool gl = g.st != 0;
				bool hl = i < ins.size() && ins[i].state != 0;
				if (!gl && !hl) continue;
				bool same = gl && hl;
				if (same) {
					const bg::Instance& h = ins[i];
					int hobj = objs[h.objIndex].originalIndex;
					same = (g.st == 2) == (h.state == 2) && g.obj == hobj && g.cur == h.curFrame && g.px == h.posX &&
					       g.py == h.posY && g.vx == h.velX && g.vy == h.velY && g.t == h.timer && g.loop == h.loopCounter;
				}
				if (!same) {
					++bad;
					if (verbose && bad <= 12) {
						if (gl) printf("  slot %zu game: st=%d obj=%d cur=%d pos=(%d,%d) vel=(%d,%d) t=%d loop=%d\n", i, g.st, g.obj, g.cur, g.px, g.py, g.vx, g.vy, g.t, g.loop);
						else printf("  slot %zu game: free\n", i);
						if (hl) { const bg::Instance& h = ins[i];
							printf("  slot %zu hant: st=%d obj=%d cur=%d pos=(%d,%d) vel=(%d,%d) t=%d loop=%d\n", i, h.state, objs[h.objIndex].originalIndex, h.curFrame, h.posX, h.posY, h.velX, h.velY, h.timer, h.loopCounter); }
						else printf("  slot %zu hant: free\n", i);
					}
				}
			}
			return bad;
		};
		std::vector<G> gp;
		if (strcmp(argv[2], "--find-tick") == 0) {
			if (!loadPool(argv[3], gp)) { fprintf(stderr, "no pool\n"); _exit(2); }
			int maxT = argc >= 5 ? atoi(argv[4]) : 20000;
			int best = 1 << 30, bestT = -1, found = 0;
			for (int t = 0; t <= maxT; ++t) {
				int b = diff(gp, false);
				if (b == 0) { printf("MATCH tick=%d\n", t); ++found; if (found >= 3) break; }
				if (b < best) { best = b; bestT = t; }
				file.TickRuntime();
			}
			if (!found) {
				printf("NO MATCH; best tick=%d with %d differing slots\n", bestT, best);
				file.ResetRuntime();
				for (int t = 0; t < bestT; ++t) file.TickRuntime();
				diff(gp, true);
			}
			_exit(found ? 0 : 9);
		}
		// --state <prefix> [--step N] [--compare <prefix2>]
		std::string pre = argv[3];
		std::vector<char> pool, drop, rng;
		if (!ReadAll((pre + ".pool").c_str(), pool)) { fprintf(stderr, "no pool\n"); _exit(2); }
		std::vector<uint8_t> up(pool.begin(), pool.end()), ud;
		if (ReadAll((pre + ".drop").c_str(), drop) && drop.size() > 8) ud.assign(drop.begin() + 8, drop.end());
		file.ImportGameState(up, ud.empty() ? nullptr : &ud);
		if (ReadAll((pre + ".rng").c_str(), rng)) file.ImportGameRng(std::vector<uint8_t>(rng.begin(), rng.end()));
		int step = 0; std::string cmp;
		for (int i = 4; i + 1 < argc; i += 2) {
			if (strcmp(argv[i], "--step") == 0) step = atoi(argv[i + 1]);
			else if (strcmp(argv[i], "--compare") == 0) cmp = argv[i + 1];
		}
		for (int t = 0; t < step; ++t) file.TickRuntime();
		if (cmp.empty()) _exit(0);
		if (!loadPool(cmp, gp)) { fprintf(stderr, "no pool %s\n", cmp.c_str()); _exit(2); }
		int b = diff(gp, true);
		printf("%s after %d ticks: %d differing slots\n", b ? "POOL DIFF" : "POOL MATCH", step, b);
		_exit(b ? 9 : 0);
	}

	if (argc >= 3 && strcmp(argv[2], "--census") == 0) {
		// One JSON line of stage features (docs/bg_research/STAGE_AUDIT.md).
		CG* cg = file.GetCG();
		int nImg = 0, n8 = 0, n32 = 0; long budget = 0;
		if (cg) {
			for (int i = 0; i < cg->get_image_count(); ++i) {
				int bpp, tid, x1, y1, x2, y2;
				if (!cg->image_info(i, bpp, tid, x1, y1, x2, y2)) continue;
				++nImg;
				if (bpp == 8) { ++n8; continue; }
				++n32;
				int w = 64, h = 64;
				while (x2 - x1 + 1 > w) w *= 2;
				while (y2 - y1 + 1 > h) h *= 2;
				budget += 4L * w * h / 1024;
			}
		}
		int cgFrames = 0, patFrames = 0, blend[4] = {0, 0, 0, 0}, interp = 0, fg = 0, noauto = 0, lin = 0;
		int cmd1 = 0, cmd2 = 0, cmd100 = 0, trig = 0, ani[6] = {0}, vel = 0, pmin = 9999, pmax = -9999;
		std::vector<int> usedCg;
		for (const auto& o : objects) {
			fg += o.foreground ? 1 : 0; noauto += o.noAutoSpawn ? 1 : 0; lin += o.linearFilter ? 1 : 0;
			pmin = std::min(pmin, (int)(int16_t)o.parallax); pmax = std::max(pmax, (int)(int16_t)o.parallax);
			for (const auto& c : o.commands) { if (c.type == 1) ++cmd1; else if (c.type == 2) ++cmd2; else if (c.type == 100) ++cmd100; }
			trig += (int)o.triggers.size();
			for (const auto& f : o.frames) {
				if (f.spriteId >= 10000) { ++cgFrames; if (f.blendMode < 4) ++blend[f.blendMode]; usedCg.push_back(f.spriteId - 10000); }
				else if (f.spriteId >= 0) ++patFrames;
				if (f.interpolate) ++interp;
				if (f.aniType < 6) ++ani[f.aniType];
				if (f.flagSetX || f.flagSetY) ++vel;
			}
		}
		std::sort(usedCg.begin(), usedCg.end());
		usedCg.erase(std::unique(usedCg.begin(), usedCg.end()), usedCg.end());
		int used8 = 0;
		if (cg) for (int id : usedCg) { int bpp, tid, a, b, c, d; if (cg->image_info(id, bpp, tid, a, b, c, d) && bpp == 8) ++used8; }
		const auto* e = file.GetStageListEntry();
		const auto& si = file.GetStageInfo();
		printf("{\"file\":\"%s\",\"objects\":%zu,\"frames\":%zu,\"cgImages\":%d,\"cg8bpp\":%d,\"cg32\":%d,"
		       "\"usedCg\":%zu,\"used8bpp\":%d,\"texBudgetKB\":%ld,\"dxt5\":%d,\"pat\":%d,\"cgFrames\":%d,\"patFrames\":%d,"
		       "\"blend\":[%d,%d,%d,%d],\"interp\":%d,\"fgObjects\":%d,\"noAutoSpawn\":%d,\"linearObj\":%d,"
		       "\"cmdSpawn\":%d,\"cmdRandSpawn\":%d,\"cmdRandVel\":%d,\"triggers\":%d,\"ani\":[%d,%d,%d,%d,%d,%d],\"moving\":%d,"
		       "\"parallax\":[%d,%d],\"bglist\":%d,\"colorVal\":%.3f,\"giant\":%d,\"lights\":%zu,\"dropObj\":%d,\"dropType\":%d}\n",
		       argv[1], objects.size(), totalFrames, nImg, n8, n32, usedCg.size(), used8, budget, budget > 30000 ? 1 : 0,
		       file.GetOldPat() ? 1 : 0, cgFrames, patFrames, blend[0], blend[1], blend[2], blend[3], interp, fg, noauto, lin,
		       cmd1, cmd2, cmd100, trig, ani[0], ani[1], ani[2], ani[3], ani[4], ani[5], vel, pmin, pmax,
		       e ? e->index : -1, e ? e->stageColorVal : 0.0f, e ? e->isGiantStage : 0,
		       si.loaded ? si.lights.size() : (size_t)0, si.loaded ? si.dropObj : 0, si.loaded ? si.dropType : -2);
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
