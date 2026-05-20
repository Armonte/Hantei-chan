// Standalone smoke test for bg::File loader. Just verifies the parser handles
// real MBAACC stage data without crashing and reports object/frame counts.
#include <cstdio>
#include <cstring>
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
				    a.enableXVec != b.enableXVec || a.enableYVec != b.enableYVec ||
				    a.xVec != b.xVec || a.yVec != b.yVec) {
					fprintf(stderr, "obj %zu frame %zu differs\n", i, k);
					++diffs;
				}
			}
		}

		printf("\nResult: %d field-level diffs\n", diffs);
		_exit(diffs > 0 ? 5 : 0);
	}

	if (!objects.empty()) {
		const auto& obj0 = objects[0];
		printf("  obj[0]: layer=%d parallax=%d frames=%zu\n",
		       obj0.layer, obj0.parallax, obj0.frames.size());
		if (!obj0.frames.empty()) {
			const auto& f0 = obj0.frames[0];
			printf("  obj[0].frame[0]: spriteId=%d offset=(%d,%d) duration=%d blend=%d alpha=%d\n",
			       f0.spriteId, f0.offsetX, f0.offsetY, f0.duration, f0.blendMode, f0.opacity);
		}
	}

	_exit(0);
}
