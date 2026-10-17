// Debug tool: dump a pattern's frame data (AF layers, EF, IF, durations)
// using the editor's real loader. Built as ha6_dump.exe.
//   usage: ha6_dump <input.ha6> <pattern> [pattern2 ...]
#include <iostream>
#include <string>
#include <cstdlib>
#include "framedata.h"

static void DumpPattern(FrameData& fd, int p)
{
	Sequence* seq = fd.get_sequence(p);
	if (!seq) { std::cout << "pattern " << p << ": null\n"; return; }
	std::cout << "=== pattern " << p << " '" << seq->name << "' code '" << seq->codeName
	          << "' frames=" << seq->frames.size()
	          << " psts=" << seq->psts << " level=" << seq->level << " flag=" << seq->flag
	          << " pups=" << seq->pups << " ATV2=" << seq->usedATV2 << " AFGX=" << seq->usedAFGX << "\n";
	int tick = 0;
	for (size_t f = 0; f < seq->frames.size(); ++f) {
		const Frame& fr = seq->frames[f];
		const auto& af = fr.AF;
		std::cout << "frame " << f << " [tick " << tick << ".." << (tick + af.duration - 1) << "] dur=" << af.duration
		          << " aniType=" << af.aniType << " aniFlag=" << af.aniFlag << " jump=" << af.jump
		          << " landJump=" << af.landJump << " interp=" << af.interpolationType
		          << " prio=" << af.priority << " loop=" << af.loopCount << "/" << af.loopEnd << "\n";
		tick += af.duration;
		for (size_t li = 0; li < af.layers.size(); ++li) {
			const auto& l = af.layers[li];
			std::cout << "   layer " << li << ": sprite=" << l.spriteId << (l.usePat ? " (PAT)" : "")
			          << " ofs=(" << l.offset_x << "," << l.offset_y << ")"
			          << " blend=" << l.blend_mode
			          << " rgba=(" << l.rgba[0] << "," << l.rgba[1] << "," << l.rgba[2] << "," << l.rgba[3] << ")"
			          << " rot=(" << l.rotation[0] << "," << l.rotation[1] << "," << l.rotation[2] << ")"
			          << " scale=(" << l.scale[0] << "," << l.scale[1] << ")"
			          << " prio=" << l.priority << "\n";
		}
		for (const auto& ef : fr.EF) {
			std::cout << "   EF type=" << ef.type << " no=" << ef.number << " params=[";
			for (int i = 0; i < 12; ++i) std::cout << ef.parameters[i] << (i < 11 ? "," : "");
			std::cout << "]\n";
		}
		for (const auto& iff : fr.IF) {
			std::cout << "   IF type=" << iff.type << " params=[";
			for (int i = 0; i < 9; ++i) std::cout << iff.parameters[i] << (i < 8 ? "," : "");
			std::cout << "]\n";
		}
		if (!fr.hitboxes.empty()) {
			std::cout << "   boxes:";
			for (const auto& [idx, b] : fr.hitboxes)
				std::cout << " " << idx << ":(" << b.xy[0] << "," << b.xy[1] << "," << b.xy[2] << "," << b.xy[3] << ")";
			std::cout << "\n";
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 3) { std::cerr << "usage: ha6_dump <input.ha6> <pattern> [pattern2 ...]\n"; return 1; }
	FrameData fd;
	if (!fd.load(argv[1])) { std::cerr << "load failed\n"; return 2; }
	for (int i = 2; i < argc; ++i)
		DumpPattern(fd, atoi(argv[i]));
	return 0;
}
