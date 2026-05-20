// Standalone round-trip validator: load → save → reload → compare.
// Built as roundtrip.exe.
#include <iostream>
#include <cstring>
#include <string>
#include "framedata.h"

static int CompareSeq(int i, const Sequence* a, const Sequence* b)
{
	int diffs = 0;
	if (!a && !b) return 0;
	if (!a || !b) { std::cerr << "seq " << i << ": presence differs\n"; return 1; }

	if (a->frames.size() != b->frames.size()) {
		std::cerr << "seq " << i << ": frame count " << a->frames.size()
		          << " vs " << b->frames.size() << "\n";
		++diffs;
	}
	if (a->pups != b->pups) {
		std::cerr << "seq " << i << ": PUPS " << a->pups << " vs " << b->pups << "\n";
		++diffs;
	}
	if (a->usedATV2 != b->usedATV2) {
		std::cerr << "seq " << i << ": usedATV2 " << a->usedATV2 << " vs " << b->usedATV2 << "\n";
		++diffs;
	}
	if (a->usedAFGX != b->usedAFGX) {
		std::cerr << "seq " << i << ": usedAFGX differs\n";
		++diffs;
	}

	size_t n = std::min(a->frames.size(), b->frames.size());
	for (size_t fi = 0; fi < n; ++fi) {
		const Frame& f1 = a->frames[fi];
		const Frame& f2 = b->frames[fi];
		if (f1.AF.frameId != f2.AF.frameId) {
			std::cerr << "seq " << i << " frame " << fi << ": AFID " << f1.AF.frameId << " vs " << f2.AF.frameId << "\n";
			++diffs;
		}
		if (f1.AF.afjh != f2.AF.afjh) {
			std::cerr << "seq " << i << " frame " << fi << ": AFJH " << f1.AF.afjh << " vs " << f2.AF.afjh << "\n";
			++diffs;
		}
		if (memcmp(f1.AF.param, f2.AF.param, 4)) {
			std::cerr << "seq " << i << " frame " << fi << ": AFPA differs\n";
			++diffs;
		}
		if (f1.AS.ascf != f2.AS.ascf) {
			std::cerr << "seq " << i << " frame " << fi << ": ASCF " << f1.AS.ascf << " vs " << f2.AS.ascf << "\n";
			++diffs;
		}
		if (f1.AT.damageProration != f2.AT.damageProration ||
		    f1.AT.minDamage != f2.AT.minDamage ||
		    f1.AT.addHitStun != f2.AT.addHitStun ||
		    f1.AT.starterCorrection != f2.AT.starterCorrection ||
		    memcmp(f1.AT.hitStunDecay, f2.AT.hitStunDecay, sizeof(f1.AT.hitStunDecay))) {
			std::cerr << "seq " << i << " frame " << fi << ": UNI AT fields differ"
			          << " ATHH=" << f1.AT.damageProration << "/" << f2.AT.damageProration
			          << " ATAM=" << f1.AT.minDamage << "/" << f2.AT.minDamage
			          << " ATSA=" << f1.AT.addHitStun << "/" << f2.AT.addHitStun
			          << " ATSH=" << f1.AT.starterCorrection << "/" << f2.AT.starterCorrection
			          << "\n";
			++diffs;
		}
		if (memcmp(f1.AT.hitVector, f2.AT.hitVector, sizeof(f1.AT.hitVector)) ||
		    memcmp(f1.AT.guardVector, f2.AT.guardVector, sizeof(f1.AT.guardVector)) ||
		    memcmp(f1.AT.hVFlags, f2.AT.hVFlags, sizeof(f1.AT.hVFlags)) ||
		    memcmp(f1.AT.gVFlags, f2.AT.gVFlags, sizeof(f1.AT.gVFlags))) {
			std::cerr << "seq " << i << " frame " << fi << ": ATV2/ATHV/ATGV vectors differ\n";
			++diffs;
		}
		if (f1.AT.damage != f2.AT.damage || f1.AT.meter_gain != f2.AT.meter_gain ||
		    f1.AT.red_damage != f2.AT.red_damage || f1.AT.guard_damage != f2.AT.guard_damage) {
			std::cerr << "seq " << i << " frame " << fi << ": damage/meter differ\n";
			++diffs;
		}
	}
	return diffs;
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cerr << "usage: roundtrip <input.ha6> [output.ha6]\n";
		return 1;
	}
	std::string in = argv[1];
	std::string out = (argc >= 3) ? argv[2] : (in + ".rt");

	FrameData fd1;
	std::cout << "[1/3] Loading " << in << "\n";
	if (!fd1.load(in.c_str())) { std::cerr << "load failed\n"; return 2; }
	std::cout << "      " << fd1.get_sequence_count() << " sequences\n";

	std::cout << "[2/3] Saving  " << out << "\n";
	fd1.save(out.c_str());

	FrameData fd2;
	std::cout << "[3/3] Re-loading " << out << "\n";
	if (!fd2.load(out.c_str())) { std::cerr << "re-load failed\n"; return 3; }

	if (fd1.get_sequence_count() != fd2.get_sequence_count()) {
		std::cerr << "Sequence count differs: " << fd1.get_sequence_count()
		          << " vs " << fd2.get_sequence_count() << "\n";
		return 4;
	}

	int totalDiffs = 0;
	int seqWithDiffs = 0;
	for (int i = 0; i < fd1.get_sequence_count(); ++i) {
		int d = CompareSeq(i, fd1.get_sequence(i), fd2.get_sequence(i));
		if (d > 0) { totalDiffs += d; ++seqWithDiffs; }
	}

	std::cout << "\nResult: " << totalDiffs << " field-level diffs across "
	          << seqWithDiffs << " sequences\n";
	return totalDiffs > 0 ? 5 : 0;
}
