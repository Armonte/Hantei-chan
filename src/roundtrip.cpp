// Standalone round-trip validator: load → save → reload → compare.
// Built as roundtrip.exe.
// Compares every field the editor round-trips, including the ones from
// issue #71 (pattern names, sprite layers, hitboxes, effects, conditions).
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include "framedata.h"

static int diffs;
static int curSeq;
static int curFrame; // -1 when comparing sequence-level fields

#define CTX() (curFrame < 0 ? (std::cerr << "seq " << curSeq << ": ") \
                            : (std::cerr << "seq " << curSeq << " frame " << curFrame << ": "))

template<typename T>
static void Cmp(const char* what, const T& x, const T& y)
{
	if (x != y) {
		CTX() << what << " " << x << " vs " << y << "\n";
		++diffs;
	}
}

static void CmpMem(const char* what, const void* x, const void* y, size_t sz)
{
	if (memcmp(x, y, sz)) {
		CTX() << what << " differs\n";
		++diffs;
	}
}

static void CompareLayer(size_t li, const Layer_Type& l1, const Layer_Type& l2)
{
	std::string p = "layer " + std::to_string(li) + " ";
	Cmp((p + "spriteId").c_str(), l1.spriteId, l2.spriteId);
	Cmp((p + "usePat").c_str(), l1.usePat, l2.usePat);
	Cmp((p + "AFRT").c_str(), l1.afrt, l2.afrt);
	Cmp((p + "offset_x").c_str(), l1.offset_x, l2.offset_x);
	Cmp((p + "offset_y").c_str(), l1.offset_y, l2.offset_y);
	Cmp((p + "blend_mode").c_str(), l1.blend_mode, l2.blend_mode);
	Cmp((p + "priority").c_str(), l1.priority, l2.priority);
	CmpMem((p + "rgba").c_str(), l1.rgba, l2.rgba, sizeof(l1.rgba));
	CmpMem((p + "rotation").c_str(), l1.rotation, l2.rotation, sizeof(l1.rotation));
	CmpMem((p + "scale").c_str(), l1.scale, l2.scale, sizeof(l1.scale));
}

static void CompareFrame(const Frame& f1, const Frame& f2)
{
	// AF — animation frame (issue #71: "adding random extra sprite layers")
	const Frame_AF& a1 = f1.AF; const Frame_AF& a2 = f2.AF;
	Cmp("layer count", a1.layers.size(), a2.layers.size());
	size_t nl = std::min(a1.layers.size(), a2.layers.size());
	for (size_t li = 0; li < nl; ++li)
		CompareLayer(li, a1.layers[li], a2.layers[li]);
	Cmp("AF jump", a1.jump, a2.jump);
	Cmp("AF duration", a1.duration, a2.duration);
	Cmp("AF aniType", a1.aniType, a2.aniType);
	Cmp("AF aniFlag", a1.aniFlag, a2.aniFlag);
	Cmp("AF landJump", a1.landJump, a2.landJump);
	Cmp("AF interpolationType", a1.interpolationType, a2.interpolationType);
	Cmp("AF priority", a1.priority, a2.priority);
	Cmp("AF loopCount", a1.loopCount, a2.loopCount);
	Cmp("AF loopEnd", a1.loopEnd, a2.loopEnd);
	Cmp("AFRT", a1.AFRT, a2.AFRT);
	Cmp("AFID", a1.frameId, a2.frameId);
	Cmp("AFJH", a1.afjh, a2.afjh);
	CmpMem("AFPA", a1.param, a2.param, sizeof(a1.param));

	// AS — state data (POD struct, but compare fields for readable output)
	const Frame_AS& s1 = f1.AS; const Frame_AS& s2 = f2.AS;
	Cmp("AS movementFlags", s1.movementFlags, s2.movementFlags);
	CmpMem("AS speed", s1.speed, s2.speed, sizeof(s1.speed));
	CmpMem("AS accel", s1.accel, s2.accel, sizeof(s1.accel));
	Cmp("AS maxSpeedX", s1.maxSpeedX, s2.maxSpeedX);
	Cmp("AS canMove", s1.canMove, s2.canMove);
	Cmp("AS stanceState", s1.stanceState, s2.stanceState);
	Cmp("AS cancelNormal", s1.cancelNormal, s2.cancelNormal);
	Cmp("AS cancelSpecial", s1.cancelSpecial, s2.cancelSpecial);
	Cmp("AS counterType", s1.counterType, s2.counterType);
	Cmp("AS hitsNumber", s1.hitsNumber, s2.hitsNumber);
	Cmp("AS invincibility", s1.invincibility, s2.invincibility);
	CmpMem("AS statusFlags", s1.statusFlags, s2.statusFlags, sizeof(s1.statusFlags));
	Cmp("AS sineFlags", s1.sineFlags, s2.sineFlags);
	CmpMem("AS sineParameters", s1.sineParameters, s2.sineParameters, sizeof(s1.sineParameters));
	CmpMem("AS sinePhases", s1.sinePhases, s2.sinePhases, sizeof(s1.sinePhases));
	Cmp("ASCF", s1.ascf, s2.ascf);

	// AT — attack data
	const Frame_AT& t1 = f1.AT; const Frame_AT& t2 = f2.AT;
	Cmp("AT guard_flags", t1.guard_flags, t2.guard_flags);
	Cmp("AT otherFlags", t1.otherFlags, t2.otherFlags);
	Cmp("AT correction", t1.correction, t2.correction);
	Cmp("AT correction_type", t1.correction_type, t2.correction_type);
	Cmp("AT damage", t1.damage, t2.damage);
	Cmp("AT red_damage", t1.red_damage, t2.red_damage);
	Cmp("AT guard_damage", t1.guard_damage, t2.guard_damage);
	Cmp("AT meter_gain", t1.meter_gain, t2.meter_gain);
	CmpMem("AT guardVector", t1.guardVector, t2.guardVector, sizeof(t1.guardVector));
	CmpMem("AT hitVector", t1.hitVector, t2.hitVector, sizeof(t1.hitVector));
	CmpMem("AT gVFlags", t1.gVFlags, t2.gVFlags, sizeof(t1.gVFlags));
	CmpMem("AT hVFlags", t1.hVFlags, t2.hVFlags, sizeof(t1.hVFlags));
	Cmp("AT hitEffect", t1.hitEffect, t2.hitEffect);
	Cmp("AT soundEffect", t1.soundEffect, t2.soundEffect);
	Cmp("AT addedEffect", t1.addedEffect, t2.addedEffect);
	Cmp("AT hitgrab", t1.hitgrab, t2.hitgrab);
	Cmp("AT extraGravity", t1.extraGravity, t2.extraGravity);
	Cmp("AT breakTime", t1.breakTime, t2.breakTime);
	Cmp("AT untechTime", t1.untechTime, t2.untechTime);
	Cmp("AT hitStopTime", t1.hitStopTime, t2.hitStopTime);
	Cmp("AT hitStop", t1.hitStop, t2.hitStop);
	Cmp("AT blockStopTime", t1.blockStopTime, t2.blockStopTime);
	Cmp("ATHH damageProration", t1.damageProration, t2.damageProration);
	Cmp("ATAM minDamage", t1.minDamage, t2.minDamage);
	Cmp("ATSA addHitStun", t1.addHitStun, t2.addHitStun);
	Cmp("ATSH starterCorrection", t1.starterCorrection, t2.starterCorrection);
	CmpMem("ATC0 hitStunDecay", t1.hitStunDecay, t2.hitStunDecay, sizeof(t1.hitStunDecay));
	Cmp("ATS1..6 hitStopLegacy", t1.hitStopLegacy, t2.hitStopLegacy);
	Cmp("ATRF", t1.atrf, t2.atrf);
	Cmp("ATBC", t1.atbc, t2.atbc);
	Cmp("ATVD", t1.atvd, t2.atvd);

	// EF / IF — effects and conditions
	Cmp("EF count", f1.EF.size(), f2.EF.size());
	size_t ne = std::min(f1.EF.size(), f2.EF.size());
	for (size_t i = 0; i < ne; ++i) {
		std::string p = "EF " + std::to_string(i) + " ";
		Cmp((p + "type").c_str(), f1.EF[i].type, f2.EF[i].type);
		Cmp((p + "number").c_str(), f1.EF[i].number, f2.EF[i].number);
		CmpMem((p + "parameters").c_str(), f1.EF[i].parameters, f2.EF[i].parameters, sizeof(f1.EF[i].parameters));
	}
	Cmp("IF count", f1.IF.size(), f2.IF.size());
	size_t ni = std::min(f1.IF.size(), f2.IF.size());
	for (size_t i = 0; i < ni; ++i) {
		std::string p = "IF " + std::to_string(i) + " ";
		Cmp((p + "type").c_str(), f1.IF[i].type, f2.IF[i].type);
		CmpMem((p + "parameters").c_str(), f1.IF[i].parameters, f2.IF[i].parameters, sizeof(f1.IF[i].parameters));
	}

	// Hitboxes (issue #68 / #71: collision, attack boxes)
	Cmp("hitbox count", f1.hitboxes.size(), f2.hitboxes.size());
	for (const auto& [idx, box] : f1.hitboxes) {
		auto it = f2.hitboxes.find(idx);
		if (it == f2.hitboxes.end()) {
			CTX() << "hitbox " << idx << " missing after round-trip\n";
			++diffs;
			continue;
		}
		std::string p = "hitbox " + std::to_string(idx) + " xy";
		CmpMem(p.c_str(), box.xy, it->second.xy, sizeof(box.xy));
	}
}

static void CompareSeq(int i, const Sequence* a, const Sequence* b)
{
	curSeq = i;
	curFrame = -1;
	if (!a && !b) return;
	if (!a || !b) { CTX() << "presence differs\n"; ++diffs; return; }

	// Issue #71: "removing pattern names"
	Cmp("name", a->name, b->name);
	Cmp("codeName", a->codeName, b->codeName);
	Cmp("psts", a->psts, b->psts);
	Cmp("level", a->level, b->level);
	Cmp("flag", a->flag, b->flag);
	Cmp("PUPS", a->pups, b->pups);
	Cmp("empty", a->empty, b->empty);
	Cmp("initialized", a->initialized, b->initialized);
	Cmp("usedATV2", a->usedATV2, b->usedATV2);
	Cmp("usedAFGX", a->usedAFGX, b->usedAFGX);
	Cmp("frame count", a->frames.size(), b->frames.size());

	size_t n = std::min(a->frames.size(), b->frames.size());
	for (size_t fi = 0; fi < n; ++fi) {
		curFrame = (int)fi;
		CompareFrame(a->frames[fi], b->frames[fi]);
	}
}

// --stack OWN OUT FILE0 FILE1 ...: load FILE0.. as a .txt stack does (later
// files overlay earlier ones), save the character the way the editor does
// with file OWN as the save target, and write OUT. Used to check that saving a
// stacked character does not bake the other files' patterns into its own.
static int StackMode(int argc, char** argv)
{
	const int own = std::atoi(argv[2]);
	const std::string out = argv[3];
	FrameData fd;
	// Same rule as LoadFromIni: files after the own one (shared BaseData)
	// only fill empty slots.
	for (int i = 4; i < argc; ++i) {
		const bool fallback = argc - 4 > 1 && i - 4 > own;
		if (!fd.load(argv[i], i > 4, fallback)) { std::cerr << "load failed: " << argv[i] << "\n"; return 2; }
	}
	if (argc - 4 > 1) fd.setOwnFile(own);
	std::cout << "stack of " << (argc - 4) << " file(s), own file " << own
	          << ", inherited patterns left out: " << fd.inheritedPatternCount() << "\n";
	if (!fd.save(out.c_str())) { std::cerr << "save failed\n"; return 6; }
	return 0;
}

static bool ReadAll(const std::string& path, std::string& out)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	char buf[65536];
	size_t n;
	out.clear();
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
	fclose(f);
	return true;
}

int main(int argc, char** argv)
{
	if (argc >= 5 && std::string(argv[1]) == "--stack")
		return StackMode(argc, argv);
	// Options (before the file names):
	//   --bytes  also require the saved file to be byte-identical to the input (exit 7)
	//   --fresh  forget the loaded encoding (ha6_enc.h) before saving, so the
	//            writer re-encodes everything from the model alone
	bool bytes = false, fresh = false;
	int ai = 1;
	for (; ai < argc && argv[ai][0] == '-' && argv[ai][1] == '-'; ++ai) {
		std::string o = argv[ai];
		if (o == "--bytes") bytes = true;
		else if (o == "--fresh") fresh = true;
	}
	if (argc - ai < 1) {
		std::cerr << "usage: roundtrip [--bytes] [--fresh] <input.ha6> [output.ha6]\n";
		return 1;
	}
	std::string in = argv[ai];
	std::string out = (argc - ai >= 2) ? argv[ai + 1] : (in + ".rt");

	FrameData fd1;
	std::cout << "[1/3] Loading " << in << "\n";
	if (!fd1.load(in.c_str())) { std::cerr << "load failed\n"; return 2; }
	std::cout << "      " << fd1.get_sequence_count() << " sequences\n";

	if (fresh) {
		for (int i = 0; i < fd1.get_sequence_count(); ++i) {
			Sequence* q = fd1.get_sequence(i);
			q->ha6 = Ha6SeqEnc{};
			for (auto& f : q->frames) f.ha6 = Ha6FrameEnc{};
		}
	}

	std::cout << "[2/3] Saving  " << out << "\n";
	if (!fd1.save(out.c_str())) { std::cerr << "save failed\n"; return 6; }

	FrameData fd2;
	std::cout << "[3/3] Re-loading " << out << "\n";
	if (!fd2.load(out.c_str())) { std::cerr << "re-load failed\n"; return 3; }

	if (fd1.get_sequence_count() != fd2.get_sequence_count()) {
		std::cerr << "Sequence count differs: " << fd1.get_sequence_count()
		          << " vs " << fd2.get_sequence_count() << "\n";
		return 4;
	}

	int seqWithDiffs = 0;
	for (int i = 0; i < fd1.get_sequence_count(); ++i) {
		int before = diffs;
		CompareSeq(i, fd1.get_sequence(i), fd2.get_sequence(i));
		if (diffs > before) ++seqWithDiffs;
	}

	std::cout << "\nResult: " << diffs << " field-level diffs across "
	          << seqWithDiffs << " sequences\n";
	if (diffs > 0) return 5;
	if (bytes) {
		std::string a, b;
		if (!ReadAll(in, a) || !ReadAll(out, b)) { std::cerr << "read back failed\n"; return 8; }
		if (a != b) {
			size_t i = 0;
			while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
			std::cout << "Bytes differ: size " << a.size() << " vs " << b.size()
			          << ", first difference at 0x" << std::hex << i << std::dec << "\n";
			return 7;
		}
		std::cout << "Bytes identical (" << a.size() << " bytes)\n";
	}
	return 0;
}
