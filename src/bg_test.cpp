// Standalone smoke test for bg::File loader. Just verifies the parser handles
// real MBAACC stage data without crashing and reports object/frame counts.
#include <cstdio>
#include <cstring>
#include "background/bg_file.h"
#include "background/bg_types.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: bg_test <stage.dat>\n");
		return 1;
	}

	bg::File file;
	printf("Loading %s\n", argv[1]);
	if (!file.Load(argv[1])) {
		fprintf(stderr, "load failed\n");
		_exit(2);
	}

	const auto& objects = file.GetObjects();
	printf("  objects: %zu\n", objects.size());
	size_t totalFrames = 0;
	for (const auto& obj : objects) totalFrames += obj.frames.size();
	printf("  total frames across all objects: %zu\n", totalFrames);

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

	// _exit skips destructors (some may touch GL).
	_exit(0);
}
