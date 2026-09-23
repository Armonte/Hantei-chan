// Standalone smoke test for bg::File loader. Just verifies the parser handles
// real MBAACC stage data without crashing and reports object/frame counts.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "background/bg_file.h"
#include "background/bg_types.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: bg_test <stage.dat> [<output.dat>]\n");
		return 1;
	}

	bg::File file;
	printf("[1/3] Loading %s\n", argv[1]);
	if (!file.Load(argv[1])) {
		fprintf(stderr, "load failed\n");
		_exit(2);
	}

	const auto& objects = file.GetObjects();
	size_t totalFrames = 0;
	for (const auto& obj : objects) totalFrames += obj.frames.size();
	printf("       objects=%zu total_frames=%zu\n", objects.size(), totalFrames);

	// --sim N: run N game ticks of the instance-pool runtime and report the
	// live-instance count every 600 ticks (spawners/despawn sanity check).
	if (argc >= 3 && strcmp(argv[2], "--sim") == 0) {
		int n = argc >= 4 ? atoi(argv[3]) : 3600;
		auto live = [&]() { int c = 0; for (auto& in : file.GetInstances()) if (in.state >= 1) ++c; return c; };
		printf("       tick 0 live=%d\n", live());
		for (int t = 1; t <= n; ++t) {
			file.TickRuntime();
			if (t % 600 == 0) printf("       tick %d live=%d pool=%zu\n", t, live(), file.GetInstances().size());
		}
		_exit(0);
	}

	if (argc >= 3) {
		const char* out = argv[2];
		printf("[2/3] Saving %s\n", out);
		if (!file.Save(out)) {
			fprintf(stderr, "save failed\n");
			_exit(3);
		}

		bg::File reload;
		printf("[3/3] Re-loading %s\n", out);
		if (!reload.Load(out)) {
			fprintf(stderr, "re-load failed\n");
			_exit(4);
		}

		const auto& r = reload.GetObjects();
		int diffs = 0;
		if (r.size() != objects.size()) {
			fprintf(stderr, "obj count: %zu vs %zu\n", objects.size(), r.size());
			++diffs;
		}
		size_t n = std::min(r.size(), objects.size());
		for (size_t i = 0; i < n; ++i) {
			if (r[i].frames.size() != objects[i].frames.size()) {
				fprintf(stderr, "obj %zu frame count differs\n", i);
				++diffs;
			}
			if (r[i].parallax != objects[i].parallax) {
				fprintf(stderr, "obj %zu parallax %d vs %d\n", i, objects[i].parallax, r[i].parallax);
				++diffs;
			}
			if (r[i].layer != objects[i].layer) {
				fprintf(stderr, "obj %zu layer %d vs %d\n", i, objects[i].layer, r[i].layer);
				++diffs;
			}
			size_t fn = std::min(r[i].frames.size(), objects[i].frames.size());
			for (size_t k = 0; k < fn; ++k) {
				const auto& a = objects[i].frames[k];
				const auto& b = r[i].frames[k];
				if (a.spriteId != b.spriteId || a.offsetX != b.offsetX || a.offsetY != b.offsetY ||
				    a.duration != b.duration || a.blendMode != b.blendMode || a.opacity != b.opacity ||
				    a.aniType != b.aniType || a.jumpFrame != b.jumpFrame ||
				    a.flagClearX != b.flagClearX || a.flagClearY != b.flagClearY ||
				    a.flagSetX != b.flagSetX || a.flagSetY != b.flagSetY ||
				    a.velX != b.velX || a.velY != b.velY ||
				    a.accX != b.accX || a.accY != b.accY) {
					fprintf(stderr, "obj %zu frame %zu differs\n", i, k);
					++diffs;
				}
			}
		}

		printf("\nResult: %d field-level diffs\n", diffs);
		_exit(diffs > 0 ? 5 : 0);
	}

	for (size_t i = 0; i < objects.size(); ++i) {
		const auto& obj = objects[i];
		printf("  obj[%zu]: layer=%d parallax=%d frames=%zu\n",
		       i, obj.layer, obj.parallax, obj.frames.size());
		for (size_t fi = 0; fi < obj.frames.size(); ++fi) {
			const auto& f = obj.frames[fi];
			printf("    frame[%zu]: spriteId=%d offset=(%d,%d) dur=%d blend=%d alpha=%d aniType=%d jump=%d vel=(%d,%d) acc=(%d,%d)\n",
			       fi, f.spriteId, f.offsetX, f.offsetY, f.duration,
			       f.blendMode, f.opacity, f.aniType, f.jumpFrame,
			       f.velX, f.velY, f.accX, f.accY);
		}
	}

	_exit(0);
}
