// Standalone PAT round-trip validator. Mirrors roundtrip.cpp for .ha6.
// Loads a .pat, saves it, reloads, compares structural counts. Diffs file
// size against original. Skips OpenGL operations (no GL context).
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include "parts/parts.h"

int main(int argc, char** argv)
{
	// --bytes: also require a byte-identical file (exit 7)
	// --fresh: drop the loaded raw records so every record is re-encoded
	bool bytes = false, fresh = false;
	int ai = 1;
	for (; ai < argc && !strncmp(argv[ai], "--", 2); ++ai) {
		if (!strcmp(argv[ai], "--bytes")) bytes = true;
		else if (!strcmp(argv[ai], "--fresh")) fresh = true;
	}
	if (argc - ai < 1) {
		std::cerr << "usage: pat_roundtrip [--bytes] [--fresh] <input.pat> [output.pat]\n";
		return 1;
	}
	std::string in = argv[ai];
	std::string out = (argc - ai >= 2) ? argv[ai + 1] : (in + ".rt");

	Parts p1(nullptr);
	std::cout << "[1/3] Loading " << in << "\n" << std::flush;
	if (!p1.Load(in.c_str())) { std::cerr << "load failed\n" << std::flush; _exit(2); }
	std::cout << "      partSets=" << p1.partSets.size()
	          << " cutOuts=" << p1.cutOuts.size()
	          << " shapes=" << p1.shapes.size()
	          << " gfxMeta=" << p1.gfxMeta.size() << "\n" << std::flush;

	if (fresh) { p1.rawRecords.clear(); p1.rawHeader.clear(); }
	std::cout << "[2/3] Saving " << out << "\n" << std::flush;
	if (!p1.Save(out.c_str())) { std::cerr << "save failed\n" << std::flush; _exit(3); }

	Parts p2(nullptr);
	std::cout << "[3/3] Re-loading " << out << "\n" << std::flush;
	if (!p2.Load(out.c_str())) { std::cerr << "re-load failed\n" << std::flush; _exit(4); }

	int diffs = 0;
	if (p1.partSets.size() != p2.partSets.size()) {
		std::cerr << "partSets count: " << p1.partSets.size() << " vs " << p2.partSets.size() << "\n" << std::flush;
		++diffs;
	}
	if (p1.cutOuts.size() != p2.cutOuts.size()) {
		std::cerr << "cutOuts count: " << p1.cutOuts.size() << " vs " << p2.cutOuts.size() << "\n" << std::flush;
		++diffs;
	}
	if (p1.shapes.size() != p2.shapes.size()) {
		std::cerr << "shapes count: " << p1.shapes.size() << " vs " << p2.shapes.size() << "\n" << std::flush;
		++diffs;
	}
	if (p1.gfxMeta.size() != p2.gfxMeta.size()) {
		std::cerr << "gfxMeta count: " << p1.gfxMeta.size() << " vs " << p2.gfxMeta.size() << "\n" << std::flush;
		++diffs;
	}

	// Field-level: every record's model fingerprint must survive.
	auto cmpKind = [&](char kind, size_t n, const char* label) {
		for (size_t i = 0; i < n; ++i)
			if (p1.Fingerprint(kind, (uint32_t)i) != p2.Fingerprint(kind, (uint32_t)i)) {
				std::cerr << label << " " << i << " differs after round trip\n" << std::flush;
				++diffs;
			}
	};
	cmpKind('P', std::min(p1.partSets.size(), p2.partSets.size()), "partSet");
	cmpKind('C', std::min(p1.cutOuts.size(), p2.cutOuts.size()), "cutOut");
	cmpKind('G', std::min(p1.gfxMeta.size(), p2.gfxMeta.size()), "texture");
	if (p1.Fingerprint('V', 0) != p2.Fingerprint('V', 0)) { std::cerr << "shapes differ\n"; ++diffs; }

	std::cout << "\nResult: " << diffs << " field-level diffs\n";
	if (!diffs && bytes) {
		FILE* a = fopen(in.c_str(), "rb"); FILE* b = fopen(out.c_str(), "rb");
		bool same = a && b;
		while (same) {
			int x = fgetc(a), y = fgetc(b);
			if (x != y) same = false;
			if (x == EOF || y == EOF) break;
		}
		if (a) fclose(a);
		if (b) fclose(b);
		std::cout << (same ? "Bytes identical\n" : "Bytes differ\n");
		std::cout.flush();
		if (!same) _exit(7);
	}
	std::cout.flush();
	std::cerr.flush();
	// Skip ~Parts() — it calls Free() which deletes Texture* which calls
	// glDeleteTextures with no GL context (segfaults).
	_exit(diffs > 0 ? 5 : 0);
}
