// ha4tool: command-line tool for MBAC Hantei4 (.DAT) character files.
//
//   ha4tool roundtrip <file.DAT>...        load -> save must be byte-identical
//   ha4tool reencode  <file.DAT>...        drop the original bytes, encode from the
//                                          model only, reload, compare the models
//   ha4tool convert   <file.DAT>... -o DIR [--effect EFFECT.DAT] [--name N]
//                                          MBAC -> MBAACC-style HA6 (+ .cg, .pat, .pal, .txt)
//   ha4tool dump      <file.DAT> [pattern] print a pattern summary
//
// Exit code 0 when every file passed.
#include "framedata.h"
#include "framedata_ha4.h"
#include "ha4_convert.h"
#include "misc.h"
#include <glad/glad.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

static bool ReadAll(const char *path, std::vector<uint8_t> &out)
{
	char *d; unsigned int n;
	if (!ReadInMem(path, d, n)) return false;
	out.assign((uint8_t *)d, (uint8_t *)d + n);
	delete[] d;
	return true;
}

static std::string BaseName(const std::string &p)
{
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

static int CmdRoundtrip(int argc, char **argv)
{
	int fails = 0, n = 0;
	for (int i = 0; i < argc; i++) {
		std::vector<uint8_t> orig;
		if (!ReadAll(argv[i], orig)) { printf("FAIL %s: cannot read\n", argv[i]); fails++; continue; }
		FrameData fd;
		std::string err;
		if (!ha4::Load(fd, orig.data(), orig.size(), &err)) { printf("FAIL %s: load: %s\n", argv[i], err.c_str()); fails++; continue; }
		std::vector<uint8_t> out;
		std::vector<std::string> warn;
		if (!ha4::Serialize(fd, out, &err, &warn)) { printf("FAIL %s: save: %s\n", argv[i], err.c_str()); fails++; continue; }
		n++;
		int pats = 0, frames = 0;
		for (int p = 0; p < 256; p++) if (!fd.m_sequences[p].frames.empty()) { pats++; frames += (int)fd.m_sequences[p].frames.size(); }
		if (out == orig) {
			printf("OK   %-22s %9zu bytes  %3d patterns %5d frames\n", BaseName(argv[i]).c_str(), orig.size(), pats, frames);
		} else {
			size_t k = 0, m = std::min(out.size(), orig.size());
			while (k < m && out[k] == orig[k]) k++;
			printf("FAIL %-22s size %zu vs %zu, first diff at 0x%zx (%02x vs %02x)\n", BaseName(argv[i]).c_str(),
			       out.size(), orig.size(), k, k < out.size() ? out[k] : 0, k < orig.size() ? orig[k] : 0);
			fails++;
		}
		for (size_t w = 0; w < warn.size() && w < 5; w++) printf("     warn: %s\n", warn[w].c_str());
	}
	printf("roundtrip: %d/%d byte-identical\n", n - fails + (argc - n), argc);
	return fails ? 1 : 0;
}

// Model-level comparison used by reencode.
static int g_diffs;
static void D(int p, int f, const char *what, long long a, long long b)
{
	if (a == b) return;
	if (g_diffs < 40) printf("     p%d f%d %s: %lld vs %lld\n", p, f, what, a, b);
	g_diffs++;
}
static void DF(int p, int f, const char *what, float a, float b) { if (a != b) D(p, f, what, (long long)lroundf(a * 10000), (long long)lroundf(b * 10000)); }

static void CompareModels(const FrameData &A, const FrameData &B)
{
	for (int p = 0; p < 256; p++) {
		const Sequence &s = A.m_sequences[p], &t = B.m_sequences[p];
		D(p, -1, "frames", s.frames.size(), t.frames.size());
		D(p, -1, "psts", s.psts, t.psts); D(p, -1, "level", s.level, t.level);
		if (s.name != t.name) D(p, -1, "name", 0, 1);
		for (size_t f = 0; f < s.frames.size() && f < t.frames.size(); f++) {
			const Frame &x = s.frames[f], &y = t.frames[f];
			int fi = (int)f;
			const Layer_Type &l = x.AF.layers[0], &m = y.AF.layers[0];
			D(p, fi, "sprite", l.spriteId, m.spriteId); D(p, fi, "usePat", l.usePat, m.usePat);
			D(p, fi, "ofs", l.offset_x * 65536LL + l.offset_y, m.offset_x * 65536LL + m.offset_y);
			D(p, fi, "blend", l.blend_mode, m.blend_mode); DF(p, fi, "alpha", l.rgba[3], m.rgba[3]);
			for (int k = 0; k < 3; k++) DF(p, fi, "rot", l.rotation[k], m.rotation[k]);
			for (int k = 0; k < 2; k++) DF(p, fi, "scale", l.scale[k], m.scale[k]);
			D(p, fi, "dur", x.AF.duration, y.AF.duration); D(p, fi, "aniType", x.AF.aniType, y.AF.aniType);
			D(p, fi, "aniFlag", x.AF.aniFlag, y.AF.aniFlag); D(p, fi, "jump", x.AF.jump, y.AF.jump);
			D(p, fi, "landJump", x.AF.landJump, y.AF.landJump); D(p, fi, "prio", x.AF.priority, y.AF.priority);
			D(p, fi, "interp", x.AF.interpolationType, y.AF.interpolationType);
			D(p, fi, "loopEnd", x.AF.loopEnd, y.AF.loopEnd); D(p, fi, "loopCount", x.AF.loopCount, y.AF.loopCount);
			const Frame_AS &a = x.AS, &b = y.AS;
			D(p, fi, "ASV0", a.movementFlags, b.movementFlags);
			for (int k = 0; k < 2; k++) { D(p, fi, "speed", a.speed[k], b.speed[k]); D(p, fi, "accel", a.accel[k], b.accel[k]); }
			D(p, fi, "maxX", a.maxSpeedX, b.maxSpeedX); D(p, fi, "canMove", a.canMove, b.canMove);
			D(p, fi, "stance", a.stanceState, b.stanceState); D(p, fi, "cN", a.cancelNormal, b.cancelNormal);
			D(p, fi, "cS", a.cancelSpecial, b.cancelSpecial); D(p, fi, "counter", a.counterType, b.counterType);
			D(p, fi, "hits", a.hitsNumber, b.hitsNumber); D(p, fi, "inv", a.invincibility, b.invincibility);
			D(p, fi, "ASF0", a.statusFlags[0], b.statusFlags[0]); D(p, fi, "ASF1", a.statusFlags[1], b.statusFlags[1]);
			bool atx = x.hitboxes.lower_bound(25) != x.hitboxes.end();
			if (atx) {
				const Frame_AT &c = x.AT, &d = y.AT;
				D(p, fi, "ATGD", c.guard_flags, d.guard_flags); D(p, fi, "ATF1", c.otherFlags, d.otherFlags);
				D(p, fi, "ATHS", c.correction, d.correction); D(p, fi, "ATHT", c.correction_type, d.correction_type);
				D(p, fi, "dmg", c.damage, d.damage); D(p, fi, "red", c.red_damage, d.red_damage);
				D(p, fi, "gdmg", c.guard_damage, d.guard_damage); D(p, fi, "meter", c.meter_gain, d.meter_gain);
				for (int k = 0; k < 3; k++) {
					D(p, fi, "hv", c.hitVector[k], d.hitVector[k]); D(p, fi, "gv", c.guardVector[k], d.guardVector[k]);
					D(p, fi, "hvf", c.hVFlags[k], d.hVFlags[k]); D(p, fi, "gvf", c.gVFlags[k], d.gVFlags[k]);
				}
				D(p, fi, "he", c.hitEffect, d.hitEffect); D(p, fi, "se", c.soundEffect, d.soundEffect);
				D(p, fi, "kk", c.addedEffect, d.addedEffect); D(p, fi, "ng", c.hitgrab, d.hitgrab);
				D(p, fi, "bt", c.breakTime, d.breakTime); D(p, fi, "sn", c.hitStopTime, d.hitStopTime);
				D(p, fi, "su", c.untechTime, d.untechTime); D(p, fi, "sp", c.hitStop, d.hitStop);
			}
			D(p, fi, "#IF", x.IF.size(), y.IF.size());
			for (size_t k = 0; k < x.IF.size() && k < y.IF.size(); k++) {
				D(p, fi, "IF type", x.IF[k].type, y.IF[k].type);
				for (int j = 0; j < 9; j++) D(p, fi, "IF p", x.IF[k].parameters[j], y.IF[k].parameters[j]);
			}
			D(p, fi, "#EF", x.EF.size(), y.EF.size());
			for (size_t k = 0; k < x.EF.size() && k < y.EF.size(); k++) {
				D(p, fi, "EF type", x.EF[k].type, y.EF[k].type); D(p, fi, "EF no", x.EF[k].number, y.EF[k].number);
				for (int j = 0; j < 12; j++) D(p, fi, "EF p", x.EF[k].parameters[j], y.EF[k].parameters[j]);
			}
			D(p, fi, "#box", x.hitboxes.size(), y.hitboxes.size());
			for (auto &hb : x.hitboxes) {
				auto it = y.hitboxes.find(hb.first);
				if (it == y.hitboxes.end()) { D(p, fi, "box missing", hb.first, -1); continue; }
				for (int j = 0; j < 4; j++) D(p, fi, "box", hb.second.xy[j], it->second.xy[j]);
			}
		}
	}
}

static int CmdReencode(int argc, char **argv)
{
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		FrameData A;
		std::string err;
		if (!ha4::LoadFile(A, argv[i], &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		FrameData S;
		ha4::LoadFile(S, argv[i], &err);
		for (auto &seq : S.m_sequences) { seq.ha4 = Ha4SeqRaw{}; for (auto &f : seq.frames) f.ha4 = Ha4FrameRaw{}; }
		std::vector<uint8_t> out;
		if (!ha4::Serialize(S, out, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		FrameData B;
		if (!ha4::Load(B, out.data(), out.size(), &err)) { printf("FAIL %s: reload %s\n", argv[i], err.c_str()); fails++; continue; }
		g_diffs = 0;
		CompareModels(A, B);
		printf("%s %-22s model diffs after canonical re-encode: %d\n", g_diffs ? "DIFF" : "OK  ", BaseName(argv[i]).c_str(), g_diffs);
		if (g_diffs) fails++;
	}
	return fails ? 1 : 0;
}

static int CmdDump(int argc, char **argv)
{
	if (argc < 1) return 2;
	FrameData fd;
	std::string err;
	if (!ha4::LoadFile(fd, argv[0], &err)) { printf("load failed: %s\n", err.c_str()); return 1; }
	int only = argc > 1 ? atoi(argv[1]) : -1;
	const Ha4Container *c = fd.m_ha4.get();
	printf("parts blob %zu bytes, CG blob %zu bytes\n", c->parts.size(), c->cg.size());
	for (int p = 0; p < 256; p++) {
		const Sequence &s = fd.m_sequences[p];
		if (s.frames.empty() || (only >= 0 && p != only)) continue;
		printf("%3d %-32s frames=%zu psts=%d level=%d\n", p, utf82sj(s.name).c_str(), s.frames.size(), s.psts, s.level);
		if (only < 0) continue;
		for (size_t f = 0; f < s.frames.size(); f++) {
			const Frame &F = s.frames[f];
			const Layer_Type &L = F.AF.layers[0];
			printf("  f%-3zu spr=%d%s ofs=(%d,%d) dur=%d ani=%d/%u jmp=%d boxes=%zu EF=%zu IF=%zu\n", f, L.spriteId,
			       L.usePat ? "(pat)" : "", L.offset_x, L.offset_y, F.AF.duration, F.AF.aniType, F.AF.aniFlag, F.AF.jump,
			       F.hitboxes.size(), F.EF.size(), F.IF.size());
		}
	}
	return 0;
}

static int CmdConvert(int argc, char **argv)
{
	std::vector<std::string> inputs;
	std::string outDir = ".", effect, name;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) outDir = argv[++i];
		else if (!strcmp(argv[i], "--effect") && i + 1 < argc) effect = argv[++i];
		else if (!strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
		else inputs.push_back(argv[i]);
	}
	int fails = 0;
	for (auto &in : inputs) {
		ha4conv::Options opt;
		opt.outDir = outDir;
		opt.baseName = inputs.size() == 1 ? name : std::string();
		ha4conv::Report rep;
		bool ok = ha4conv::ConvertFile(in, opt, rep);
		printf("%s %s\n", ok ? "OK  " : "FAIL", BaseName(in).c_str());
		for (auto &l : rep.lines) printf("     %s\n", l.c_str());
		if (!ok) fails++;
	}
	return fails ? 1 : 0;
}

// Headless: Parts owns a Vao whose destructor calls glDeleteBuffers; there is
// no GL context in this tool, so route it to a no-op.
static void APIENTRY NoGlDeleteBuffers(GLsizei, const GLuint *) {}

int main(int argc, char **argv)
{
	if (!glad_glDeleteBuffers) glad_glDeleteBuffers = NoGlDeleteBuffers;
	if (argc < 3) {
		printf("usage: ha4tool roundtrip|reencode|dump|convert <files...>\n");
		return 2;
	}
	std::string cmd = argv[1];
	if (cmd == "roundtrip") return CmdRoundtrip(argc - 2, argv + 2);
	if (cmd == "reencode") return CmdReencode(argc - 2, argv + 2);
	if (cmd == "dump") return CmdDump(argc - 2, argv + 2);
	if (cmd == "convert") return CmdConvert(argc - 2, argv + 2);
	printf("unknown command %s\n", cmd.c_str());
	return 2;
}
