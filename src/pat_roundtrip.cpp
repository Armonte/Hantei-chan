// Standalone PAT round-trip validator. Mirrors roundtrip.cpp for .ha6.
// Loads a .pat, saves it, reloads, compares structural counts. Diffs file
// size against original. Skips OpenGL operations (no GL context).
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include "parts/parts.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cerr << "usage: pat_roundtrip <input.pat> [output.pat]\n";
		return 1;
	}
	std::string in = argv[1];
	std::string out = (argc >= 3) ? argv[2] : (in + ".rt");

	Parts p1(nullptr);
	std::cout << "[1/3] Loading " << in << "\n" << std::flush;
	if (!p1.Load(in.c_str())) { std::cerr << "load failed\n" << std::flush; _exit(2); }
	std::cout << "      partSets=" << p1.partSets.size()
	          << " cutOuts=" << p1.cutOuts.size()
	          << " shapes=" << p1.shapes.size()
	          << " gfxMeta=" << p1.gfxMeta.size() << "\n" << std::flush;

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

	std::cout << "\nResult: " << diffs << " top-level count diffs\n";
	std::cout.flush();
	std::cerr.flush();
	// Skip ~Parts() — it calls Free() which deletes Texture* which calls
	// glDeleteTextures with no GL context (segfaults).
	_exit(diffs > 0 ? 5 : 0);
}
