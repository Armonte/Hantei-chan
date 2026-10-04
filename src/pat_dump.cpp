// Debug tool: dump PAT part-set properties (groups, cutouts, shapes) via the
// real loader, headless. Built as pat_dump.exe.
//   usage: pat_dump <input.pat> <partset> [partset2 ...]
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include "parts/parts.h"

int main(int argc, char** argv)
{
	if (argc < 3) { std::cerr << "usage: pat_dump <input.pat> <partset> [partset2 ...]\n"; return 1; }
	std::cout << std::unitbuf; // auto-flush: keep output on crash

	Parts p(nullptr);
	if (!p.Load(argv[1])) { std::cerr << "load failed\n"; _exit(2); }
	std::cout << "partSets=" << p.partSets.size() << " cutOuts=" << p.cutOuts.size()
	          << " shapes=" << p.shapes.size() << " gfxMeta=" << p.gfxMeta.size() << "\n";

	for (int a = 2; a < argc; ++a) {
		int id = atoi(argv[a]);
		if (id < 0 || id >= (int)p.partSets.size()) { std::cout << "partset " << id << ": out of range\n"; continue; }
		const auto& ps = p.partSets[id];
		std::cout << "=== partset " << id << " '" << ps.name << "' groups=" << ps.groups.size() << "\n";
		for (const auto& g : ps.groups) {
			std::cout << "  prop " << g.propId << ": ppId=" << g.ppId
			          << " xy=(" << g.x << "," << g.y << ")"
			          << " scale=(" << g.scaleX << "," << g.scaleY << ")"
			          << " rot=(" << g.rotation[0] << "," << g.rotation[1] << "," << g.rotation[2] << "," << g.rotation[3] << ")"
			          << " flip=" << g.flip
			          << " prio=" << g.priority
			          << " additive=" << g.additive
			          << " bgra=(" << (int)g.bgra[0] << "," << (int)g.bgra[1] << "," << (int)g.bgra[2] << "," << (int)g.bgra[3] << ")\n";
			if (g.ppId >= 0 && g.ppId < (int)p.cutOuts.size()) {
				const auto& c = p.cutOuts[g.ppId];
				std::cout << "     cutout " << g.ppId << ": tex=" << c.texture
				          << " shapeIdx=" << c.shapeIndex
				          << " xy=(" << c.xy[0] << "," << c.xy[1] << ")"
				          << " wh=(" << c.wh[0] << "," << c.wh[1] << ")"
				          << " uv=(" << c.uv[0] << "," << c.uv[1] << "," << c.uv[2] << "," << c.uv[3] << ")"
				          << " colorSlot=" << c.colorSlot << "\n";
				if (c.shapeIndex >= 0 && c.shapeIndex < (int)p.shapes.size()) {
					const auto& s = p.shapes[c.shapeIndex];
					std::cout << "     shape " << c.shapeIndex << ": type=" << (int)s.type
					          << " vtx=" << s.vertexCount << "/" << s.vertexCount2
					          << " len=" << s.length << "/" << s.length2
					          << " radius=" << s.radius << " dRadius=" << s.dRadius
					          << " width=" << s.width << " dz=" << s.dz << "\n";
				}
			}
		}
	}
	return 0;
}
