#include "ha4_convert.h"
#include "framedata.h"
#include "framedata_ha4.h"
#include "ha4_parts.h"
#include "parts/parts.h"
#include "misc.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace fs = std::filesystem;

namespace ha4conv {

const std::vector<SystemPatternRule> &SystemPatternRules()
{
	// CC data\vector.txt HitAni column (MBAC VECTOR.TXT: 23/24/25 for all strengths;
	// the old MBAC rows survive as comments in CC's file, e.g. "0 23 17 13 13").
	static const std::vector<SystemPatternRule> rules = {
		{900, 23, "head hit, light (CC vector 0/76)"},
		{903, 23, "head hit, medium (CC vector 1/87)"},
		{906, 23, "head hit, heavy (CC vector 2/55/65)"},
		{901, 24, "body hit, light (CC vector 3)"},
		{904, 24, "body hit, medium (CC vector 4/79)"},
		{907, 24, "body hit, heavy (CC vector 5/54/81/153)"},
		{902, 25, "crouch hit, light (CC vector 50)"},
		{905, 25, "crouch hit, medium (CC vector 51/56/80/88)"},
		{908, 25, "crouch hit, heavy (CC vector 52/66/82/152)"},
		{351, 26, "slide down (CC-only vector 72/75)"},
		{354, 26, "aerial down bounce (CC-only vector 161-174)"},
	};
	return rules;
}

static bool HasFrames(FrameData &fd, int p)
{
	Sequence *s = fd.get_sequence(p);
	return s && !s->frames.empty();
}

static bool HasAttackBox(const Frame &f)
{
	for (auto &b : f.hitboxes) if (b.first >= 25) return true;
	return false;
}

// M8: mbacPC IF_ProcessCondition case 0x33 (0x439A80) vs MBAA Cond51_FindShieldCondInFrame 0x4684A0
// + Character_TickShieldDuration / Character_ApplyShieldSuccess:
//  p0: MBAC -1 marks the EX-window frame, >=0 the release frame; MBAA always jumps to p0 when
//      the [Type_N] ExShieldTime+ShieldKeepTime window ends -> -1 becomes the release frame of
//      the pattern's next (else any) IF 51 record.
//  p1: success frame in both (priority 3000).
//  p2: MBAC JumpToFrame(p2) for the EX follow-up; MBAA queues *pattern* p2. Every MBAC p2 frame
//      is a 1-frame end frame (AniFlag 0) whose AF jump is the follow-up pattern -> use that.
static void RemapShield(FrameData &fd, std::vector<std::string> *log, int *exShieldOut)
{
	int recs = 0, fixedP0 = 0, fixedP2 = 0, droppedP2 = 0, pats = 0;
	int window[3] = {-1, -1, -1};   // EX window length per stance (0 stand, 1 air, 2 crouch)
	for (int p = 0; p < fd.get_sequence_count(); p++) {
		Sequence *seq = fd.get_sequence(p);
		if (!seq || seq->frames.empty()) continue;
		auto &fr = seq->frames;
		bool any = false;
		for (size_t i = 0; i < fr.size(); i++) {
			for (auto &c : fr[i].IF) {
				if (c.type != 51) continue;
				any = true; recs++;
				if (c.parameters[0] < 0) {
					int rel = -1;
					for (size_t j = i; j < fr.size() && rel < 0; j++)
						for (auto &d : fr[j].IF) if (d.type == 51 && d.parameters[0] >= 0) { rel = d.parameters[0]; break; }
					for (size_t j = 0; j < i && rel < 0; j++)
						for (auto &d : fr[j].IF) if (d.type == 51 && d.parameters[0] >= 0) { rel = d.parameters[0]; break; }
					int st = fr[i].AS.stanceState;
					if (st >= 0 && st < 3) window[st] = std::max(window[st], fr[i].AF.duration);
					if (rel >= 0) { c.parameters[0] = rel; fixedP0++; }
				}
				int p2 = c.parameters[2];
				if (p2 > 0) {
					if (p2 < (int)fr.size() && fr[p2].AF.aniType == 0) { c.parameters[2] = fr[p2].AF.jump; fixedP2++; }
					else { c.parameters[2] = 0; droppedP2++; }
				}
			}
		}
		pats += any;
	}
	if (exShieldOut) { exShieldOut[0] = window[0]; exShieldOut[1] = window[2]; exShieldOut[2] = window[1]; }
	if (log && recs) {
		log->push_back("IF51 shield (M8): " + std::to_string(recs) + " records in " + std::to_string(pats) +
		               " patterns; p0 -1 -> release frame: " + std::to_string(fixedP0) +
		               "; p2 frame -> follow-up pattern: " + std::to_string(fixedP2) +
		               (droppedP2 ? "; p2 dropped (not an end frame): " + std::to_string(droppedP2) : std::string()));
		std::string w = "IF51 shield: MBAC EX window (frame with p0 -1) stand/crouch/air = ";
		for (int st : {0, 2, 1}) w += (window[st] < 0 ? std::string("-") : std::to_string(window[st])) + (st == 1 ? "" : "/");
		log->push_back(w + " f -> [Type_N] ExShieldTime_Stand/Crouch/Air");
	}
}

void PrepareForHA6(FrameData &fd, const Options &opt, std::vector<std::string> *log, const FrameData *cc, int *exShieldOut)
{
	int ggZeroed = 0, ggFromCC = 0, efBits = 0;
	for (int p = 0; p < fd.get_sequence_count(); p++) {
		Sequence &seq = fd.m_sequences[p];
		// Empty HA4 slots still carry a (default) name; HA6 only has entries
		// for patterns with frames.
		if (seq.frames.empty()) { seq.name.clear(); continue; }
		// HA6 has no per-frame HA4 bytes.
		seq.ha4 = Ha4SeqRaw{};
		const Sequence *ccSeq = nullptr;
		if (cc && opt.guardDamage == GuardDamage::FromCC && p < (int)cc->m_sequences.size() &&
		    cc->m_sequences[p].frames.size() == seq.frames.size())
			ccSeq = &cc->m_sequences[p];
		for (size_t i = 0; i < seq.frames.size(); i++) {
			Frame &f = seq.frames[i];
			f.ha4 = Ha4FrameRaw{};
			// M3: ATVV[2]. Frames without an attack box have no ATST in HA6 (nothing to do).
			if (opt.guardDamage != GuardDamage::Raw && HasAttackBox(f)) {
				int gd = 0;
				if (ccSeq && HasAttackBox(ccSeq->frames[i])) { gd = ccSeq->frames[i].AT.guard_damage; ggFromCC++; }
				if (f.AT.guard_damage != gd && !ccSeq) ggZeroed++;
				f.AT.guard_damage = gd;
			}
			// M12: EF 1/8/11 p2 bits above the low byte are unread by mbacPC
			// EffectObject_InitFromParent 0x423140 (byte reads only) but live in MBAA
			// Effect_InitializeComplex 0x454900 (0x100 add, 0x200/0x400 screen edge, 0x800 facing).
			if (opt.efClearCcOnlyBits)
				for (auto &e : f.EF)
					if ((e.type == 1 || e.type == 8 || e.type == 11) && (e.parameters[2] & ~0xFF)) {
						e.parameters[2] &= 0xFF; efBits++;
					}
		}
	}
	if (log) {
		if (opt.guardDamage == GuardDamage::Zero)
			log->push_back("ATVV[2] guard damage (M3): MBAC stun value replaced by 0 in " + std::to_string(ggZeroed) + " attack frames");
		else if (opt.guardDamage == GuardDamage::FromCC)
			log->push_back("ATVV[2] guard damage (M3): " + std::to_string(ggFromCC) + " attack frames from CC" +
			               (cc ? "" : " (no CC data loaded)") + ", the rest 0");
		if (efBits) log->push_back("EF p2 bits >= 0x100 cleared (M12): " + std::to_string(efBits) + " records");
	}

	if (opt.shieldRemap) RemapShield(fd, log, exShieldOut);

	// M7: victim reaction patterns. Only for character data (has the MBAC reaction set).
	if (opt.systemPatterns && HasFrames(fd, 17) && HasFrames(fd, 23) && HasFrames(fd, 24) && HasFrames(fd, 25) && HasFrames(fd, 26)) {
		std::string added;
		for (auto &r : SystemPatternRules()) {
			fd.ensureSequenceCount(r.target + 1);
			if (HasFrames(fd, r.target) || !HasFrames(fd, r.source)) continue;
			Sequence copy = fd.m_sequences[r.source];
			copy.name += " (MBAC " + std::to_string(r.source) + ")";
			fd.m_sequences[r.target] = copy;
			added += (added.empty() ? "" : " ") + std::to_string(r.target) + "<-" + std::to_string(r.source);
		}
		if (log && !added.empty()) log->push_back("system patterns (M7): " + added);
	}
}

static std::string Lower(std::string s)
{
	for (auto &c : s) c = (char)tolower((unsigned char)c);
	return s;
}

// Find <dir>/<stem>.<ext> with any letter case.
static std::string FindSibling(const fs::path &dat, const std::string &ext)
{
	std::string want = Lower(dat.stem().string() + ext);
	std::error_code ec;
	for (auto &e : fs::directory_iterator(dat.parent_path(), ec))
		if (Lower(e.path().filename().string()) == want) return e.path().string();
	return {};
}

// FindSibling, then <altDir>/<stem>.<ext> (any case).
static std::string FindSiblingOr(const fs::path &dat, const std::string &ext, const std::string &altDir)
{
	std::string hit = FindSibling(dat, ext);
	if (!hit.empty() || altDir.empty()) return hit;
	return FindSibling(fs::path(altDir) / dat.filename(), ext);
}

static bool WriteBytes(const std::string &path, const void *d, size_t n)
{
	return WriteFileAtomic(path.c_str(), d, n);
}

// ---------------------------------------------------------------------------------------------
// ActCadenza parity data (S8b): CT status, command file, spawn leaks, giant reactions.

bool ParseAcVersion(const std::string &s, AcVersion &out)
{
	std::string l = Lower(s);
	if (l == "1.00" || l == "1.0" || l == "100") { out = AcVersion::V100; return true; }
	if (l == "1.03a" || l == "1.03" || l == "103a") { out = AcVersion::V103A; return true; }
	return false;
}

const char *AcVersionName(AcVersion v) { return v == AcVersion::V100 ? "1.00" : "1.03A"; }

bool ParseCtStatus(const uint8_t *d, size_t n, CtStatus &out, std::string *err)
{
	out = CtStatus{};
	// 8916 B for every playable character; GAKIHA / F_CIEL ship an older 8908 B CT (the game still copies 0x70
	// bytes from +0x2264, so it reads 8 bytes past their end; the status and team fields are inside).
	if (n < CtStatus::kTeamOff + 8) {
		if (err) *err = "CT size " + std::to_string(n) + " (expected " + std::to_string(CtStatus::kSize) + ")";
		return false;
	}
	memcpy(out.head, d + CtStatus::kStatusOff, sizeof out.head);
	memcpy(out.floats, d + CtStatus::kFloatOff, sizeof out.floats);
	int32_t t[2];
	memcpy(t, d + CtStatus::kTeamOff, sizeof t);
	out.team[0] = t[0]; out.team[1] = t[1];
	out.ok = true;
	return true;
}

bool ReadCtStatus(const std::string &path, CtStatus &out, std::string *err)
{
	char *d = nullptr; unsigned int n = 0;
	if (!ReadInMem(path.c_str(), d, n)) { if (err) *err = "cannot read " + path; return false; }
	bool ok = ParseCtStatus((const uint8_t *)d, n, out, err);
	delete[] d;
	return ok;
}

// "0.92" for 0.92f, "1.0" for 1.0f (the _C.TXT / MBAACC _c.txt style).
static std::string FmtF(float v)
{
	char b[32];
	snprintf(b, sizeof b, "%g", (double)v);
	std::string s = b;
	if (s.find_first_of(".e") == std::string::npos) s += ".0";
	return s;
}

static std::string StripComment(const std::string &l)
{
	size_t c = l.find("//");
	std::string s = c == std::string::npos ? l : l.substr(0, c);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
	return s;
}

static std::vector<std::string> SplitWs(const std::string &s)
{
	std::vector<std::string> v; std::istringstream is(s); std::string w;
	while (is >> w) v.push_back(w);
	return v;
}

static bool IsInt(const std::string &w)
{
	size_t i = (!w.empty() && w[0] == '-') ? 1 : 0;
	if (i >= w.size()) return false;
	for (; i < w.size(); i++) if (!isdigit((unsigned char)w[i])) return false;
	return true;
}

static bool IsBits8(const std::string &w)
{
	return w.size() == 8 && w.find_first_not_of("01") == std::string::npos;
}

bool EmitCommandFile(const std::string &txtPath, const CtStatus *ct, const std::string &ctLabel,
                     const std::string &outPath, std::vector<std::string> *log, int highJumpPat)
{
	char *raw = nullptr; unsigned int rn = 0;
	if (!ReadInMem(txtPath.c_str(), raw, rn)) { if (log) log->push_back("cmd: cannot read " + txtPath); return false; }
	std::string text;
	text.reserve(rn);
	for (unsigned i = 0; i < rn; i++) if (raw[i] != '\r') text += raw[i];
	delete[] raw;

	size_t endPos = text.find("\nEND");
	std::string head = text.substr(0, endPos);
	while (!head.empty() && head.back() == '\n') head.pop_back();
	// Trailer: the lines after the first line that is exactly END (comments stripped).
	std::vector<std::string> lines, tr;
	{ std::istringstream is(text); std::string l; while (std::getline(is, l)) lines.push_back(l); }
	size_t i = 0;
	for (; i < lines.size(); i++) { auto w = SplitWs(StripComment(lines[i])); if (w.size() == 1 && w[0] == "END") break; }
	for (i = i + 1; i < lines.size(); i++) { std::string s = StripComment(lines[i]); if (!SplitWs(s).empty()) tr.push_back(s); }

	std::vector<std::string> team, status;
	std::vector<std::vector<std::string>> throws, block;
	for (size_t k = 0; k < tr.size(); k++) {
		auto w = SplitWs(tr[k]);
		if (w.size() == 1 && w[0] == "[TeamChangeData]") { if (k + 1 < tr.size()) team = SplitWs(tr[k + 1]); k++; continue; }
		if (w.size() == 10 && IsBits8(w[4])) status = w;
		else if (w.size() == 8) block.push_back(w);
		else if (w.size() == 2 && IsInt(w[0]) && IsInt(w[1])) throws.push_back(w);
	}

	std::vector<std::string> out = {head, "", "END", ""};
	std::vector<std::string> mism;
	if (ct && ct->ok && !team.empty() && (team.size() != 2 || atoi(team[0].c_str()) != ct->team[0] || atoi(team[1].c_str()) != ct->team[1]))
		mism.push_back("TeamChangeData");
	if (ct && ct->ok && (team.empty() || mism.size())) team = {std::to_string(ct->team[0]), std::to_string(ct->team[1])};
	if (!team.empty()) {
		std::string t = "test = ";
		for (size_t k = 0; k < team.size(); k++) t += (k ? ", " : "") + team[k];
		out.insert(out.end(), {"[TeamChangeData]", t, ""});
	}
	if (!throws.empty()) {
		out.push_back("[ThrowParam]");
		out.push_back("Ground = " + throws[0][0] + "," + throws[0][1]);
		if (throws.size() > 1) out.push_back("Air = " + throws[1][0] + "," + throws[1][1]);
		out.push_back("");
	}
	if (!status.empty() || (ct && ct->ok)) {
		int air = status.empty() ? 0 : atoi(status[0].c_str());
		bool bit7 = !status.empty() && status[4][0] == '1';
		float g[4];
		for (int q = 0; q < 4; q++) g[q] = status.empty() ? 0.f : (float)atof(status[6 + q].c_str());
		std::string src = "_C.TXT";
		if (ct && ct->ok) {
			if (!status.empty()) {
				if (ct->airJumps() != air) mism.push_back("AirJumpNum");
				if (((ct->bits() >> 7) & 1) != (unsigned)bit7) mism.push_back("status bit7");
			}
			air = ct->airJumps(); bit7 = (ct->bits() >> 7) & 1;
			for (int q = 0; q < 4; q++) g[q] = ct->guts(q);
			src = ctLabel;
		}
		out.push_back("[Status]");
		out.push_back("AirJumpNum = " + std::to_string(air));
		if (!status.empty()) {
			std::string r;
			for (auto &w : status) r += (r.empty() ? "" : " ") + w;
			out.push_back("// ActCadenza _C.TXT status row: " + r);
		}
		out.push_back("// Guard = guts for HP >= 3/4, >= 1/2, >= 1/4, < 1/4 = status floats f1..f4 from " + src +
		              " (mbacPC Damage_ComputeProrated 0x451660 reads CT+0x2270+4q; f0 is never read)");
		std::string gl = "Guard = ";
		for (int q = 0; q < 4; q++) gl += (q ? ", " : "") + FmtF(g[q] == 0.f ? 1.f : g[q]);
		out.push_back(gl);
		if (bit7) out.push_back("Flags = 1 // ActCadenza status bit7 (Sion: bullet display, = MBAACC sion Flags=1)");
		if (highJumpPat >= 0)
			out.push_back("CancelHighJumpPat = " + std::to_string(highJumpPat) +
			              " // ActCadenza launcher (AT flag 0x10) jump cancel -> high jump 41 (mbacPC 0x439200); MBAACC reads it for 8 only");
		out.push_back("");
		if (log) {
			std::string l = "guts (" + src + "): ";
			for (int q = 0; q < 4; q++) l += (q ? " " : "") + FmtF(g[q]);
			if (!status.empty()) {
				std::string t;
				for (int q = 0; q < 4; q++) if (FmtF((float)atof(status[6 + q].c_str())) != FmtF(g[q])) t = " (differs from _C.TXT f1..f4)";
				l += t;
			}
			log->push_back(l + "; AirJumpNum " + std::to_string(air));
		}
	}
	if (!block.empty()) {
		std::string b;
		for (auto &r : block) { std::string x; for (auto &w : r) x += (x.empty() ? "" : " ") + w; b += (b.empty() ? "" : " | ") + x; }
		out.push_back("// ActCadenza blocking timing table dropped (no MBAACC field): " + b);
	}
	if (log && !mism.empty()) {
		std::string m;
		for (auto &x : mism) m += (m.empty() ? "" : ", ") + x;
		log->push_back("cmd: CT and _C.TXT trailer differ in " + m + " (the CT wins: the game reads only the CT)");
	}
	std::string all;
	for (auto &l : out) all += l + "\r\n";
	if (!WriteFileAtomic(outPath.c_str(), all.data(), all.size())) { if (log) log->push_back("cmd: could not write " + outPath); return false; }
	return true;
}

static Sequence MakeNoopPattern()
{
	Sequence s;
	s.name = "ActCadenza no-op (spawn leak)";
	s.initialized = true;
	s.empty = false;
	s.frames.resize(1);
	Frame &f = s.frames[0];
	f.AF.layers.resize(1);
	f.AF.layers[0].spriteId = -1;
	f.AF.duration = 1;
	f.AF.aniType = 0;   // end: the effect object is removed (MBAACC's own empty effects 216/217 do the same)
	f.AF.jump = 0;
	return s;
}

int RewriteSpawnLeaks(FrameData &fd, const FrameData &mbaacc, int preferredNoop,
                      std::vector<std::pair<int, int>> *leaks, std::vector<std::string> *log)
{
	auto has = [](const FrameData &d, int p) {
		return p >= 0 && p < (int)d.m_sequences.size() && !d.m_sequences[p].frames.empty();
	};
	std::vector<std::pair<int, int>> found;
	std::vector<Frame_EF *> sites;
	for (int p = 0; p < (int)fd.m_sequences.size(); p++)
		for (auto &f : fd.m_sequences[p].frames)
			for (auto &e : f.EF)
				if (e.type == 1 && !has(fd, e.number) && has(mbaacc, e.number)) {
					sites.push_back(&e);
					if (std::find(found.begin(), found.end(), std::make_pair(p, e.number)) == found.end()) found.push_back({p, e.number});
				}
	if (sites.empty()) return -1;
	int noop = preferredNoop;
	while (noop > 256 && (has(fd, noop) || has(mbaacc, noop))) noop--;
	std::vector<int> targets;
	for (auto *e : sites) { if (std::find(targets.begin(), targets.end(), e->number) == targets.end()) targets.push_back(e->number); }
	// sites point into fd; growing the sequence vector would move them, so retarget first.
	for (auto *e : sites) e->number = noop;
	fd.ensureSequenceCount(noop + 1);
	fd.m_sequences[noop] = MakeNoopPattern();
	std::sort(targets.begin(), targets.end());
	if (leaks) *leaks = found;
	if (log) {
		std::string t, src;
		for (int x : targets) t += (t.empty() ? "" : " ") + std::to_string(x);
		for (auto &pr : found) src += (src.empty() ? "" : " ") + std::to_string(pr.first) + "->" + std::to_string(pr.second);
		log->push_back("spawn leaks into MBAACC-only slots: EF1 targets " + t + " (" + std::to_string(sites.size()) +
		               " records: " + src + ") -> no-op pattern " + std::to_string(noop) +
		               " (empty in ActCadenza, so they spawn nothing there)");
	}
	return noop;
}

std::vector<int> AddGiantReactions(FrameData &fd, const FrameData &donor, int giantFlag, std::vector<std::string> *log)
{
	std::vector<int> added;
	if (giantFlag != 1 && giantFlag != 2) return added;
	auto has = [](const FrameData &d, int p) {
		return p >= 0 && p < (int)d.m_sequences.size() && !d.m_sequences[p].frames.empty();
	};
	// GiantFlag 1 victims only ever enter 44/45; GiantFlag 2 also takes the vector table's HitAni (900-908 / 351 /
	// 354, matching MBAACC vector.txt) and MBAACC's G_AKIHA ships the old 23-30 set and 350 next to them.
	std::vector<int> slots = giantFlag == 1 ? std::vector<int>{44, 45}
	                                        : std::vector<int>{23, 24, 25, 26, 27, 28, 29, 30, 44, 45, 350};
	int poseNo = has(fd, 24) ? 24 : has(fd, 23) ? 23 : 0;
	if (!has(fd, poseNo)) { if (log) log->push_back("giant reactions: no pose pattern (24/23/0)"); return added; }
	const Sequence pose = fd.m_sequences[poseNo];
	int dropped = 0;
	for (int slot : slots) {
		if (has(fd, slot) || !has(donor, slot)) continue;
		Sequence s = donor.m_sequences[slot];
		for (size_t i = 0; i < s.frames.size(); i++) {
			Frame &f = s.frames[i];
			const Frame &pf = pose.frames[std::min(i, pose.frames.size() - 1)];
			f.AF.layers = pf.AF.layers;
			f.AF.AFRT = pf.AF.AFRT;
			f.hitboxes.clear();
			for (auto &b : pf.hitboxes) if (b.first < 25) f.hitboxes[b.first] = b.second;
			std::vector<Frame_EF> keep;
			for (auto &e : f.EF) {
				bool spawn = e.type == 1 || e.type == 11 || e.type == 101 || e.type == 111 || e.type == 1000;
				if (spawn && !has(fd, e.number)) { dropped++; continue; }
				keep.push_back(e);
			}
			f.EF.assign(keep.begin(), keep.end());
		}
		s.name += " (MBAACC giant " + std::to_string(slot) + ")";
		fd.ensureSequenceCount(slot + 1);
		fd.m_sequences[slot] = s;
		added.push_back(slot);
	}
	std::string m7;
	if (giantFlag == 2) {
		for (auto &r : SystemPatternRules()) {
			fd.ensureSequenceCount(r.target + 1);
			if (has(fd, r.target) || !has(fd, r.source)) continue;
			Sequence copy = fd.m_sequences[r.source];
			copy.name += " (" + std::to_string(r.source) + ")";
			fd.m_sequences[r.target] = copy;
			added.push_back(r.target);
			m7 += (m7.empty() ? "" : " ") + std::to_string(r.target) + "<-" + std::to_string(r.source);
		}
	}
	if (log) {
		std::string a;
		for (int x : added) if (x < 900 && x != 351 && x != 354) a += (a.empty() ? "" : " ") + std::to_string(x);
		log->push_back("giant reactions (GiantFlag " + std::to_string(giantFlag) + "): donor slots " + (a.empty() ? "-" : a) +
		               " with own pose p" + std::to_string(poseNo) + (dropped ? "; " + std::to_string(dropped) + " unresolved donor EF spawns dropped" : "") +
		               (m7.empty() ? "" : "; HitAni slots " + m7));
		bool ko = has(fd, 44) && has(fd, 45);
		log->push_back(std::string("giant structure check: 44/45 (forced on a KO hit) ") + (ko ? "present" : "MISSING") +
		               (giantFlag == 1 ? "; vector-table HitAni is never used for GiantFlag 1" : ""));
	}
	return added;
}

// <dir>/<name><suf>.ha6 stack (MBAACC crescent: "", _r, _0, _0_r; later files win). Returns files loaded.
static int LoadMbaaccStack(const std::string &dir, const std::string &name, FrameData &out)
{
	int n = 0;
	std::error_code ec;
	for (const char *suf : {"", "_r", "_0", "_0_r"}) {
		std::string f = Lower(name + suf + ".ha6"), hit;
		for (auto &e : fs::directory_iterator(fs::path(dir), ec)) if (Lower(e.path().filename().string()) == f) hit = e.path().string();
		if (!hit.empty() && out.load(hit.c_str(), n > 0)) n++;
	}
	return n;
}

bool Convert(const FrameData &src, const std::string &datPath, const Options &opt, Report &rep)
{
	if (!src.m_ha4) { rep.lines.push_back("not HA4 data"); return false; }
	std::error_code aec;
	fs::path dat = fs::absolute(fs::path(datPath), aec);
	std::string base = opt.baseName.empty() ? Lower(dat.stem().string()) : opt.baseName;
	fs::path out = opt.outDir.empty() ? dat.parent_path() : fs::path(opt.outDir);
	std::error_code ec;
	fs::create_directories(out, ec);

	// --- HA6 ---
	FrameData fd = src;
	FrameData ccData;
	const FrameData *cc = nullptr;
	if (opt.guardDamage == GuardDamage::FromCC && !opt.ccDataDir.empty()) {
		// CC crescent stack: <c>.HA6, <c>_r.HA6, <c>_0.HA6, <c>_0_r.HA6 (later files win).
		std::string cn = opt.ccName.empty() ? base : opt.ccName;
		if (LoadMbaaccStack(opt.ccDataDir, cn, ccData)) cc = &ccData;
		else rep.lines.push_back("cc: no " + cn + ".HA6 in " + opt.ccDataDir);
	}
	PrepareForHA6(fd, opt, &rep.lines, cc, rep.exShield);

	// Spawn leaks into MBAACC-only slots of the moon-3 stack.
	if (opt.leakNoop && !opt.mbaaccDataDir.empty()) {
		std::string cn = opt.ccName.empty() ? base : opt.ccName;
		FrameData stack;
		if (LoadMbaaccStack(opt.mbaaccDataDir, cn, stack)) {
			rep.noopUsed = RewriteSpawnLeaks(fd, stack, opt.noopPattern, &rep.leaks, &rep.lines);
			if (rep.noopUsed < 0) rep.lines.push_back("spawn leaks into MBAACC-only slots: none (vs " + cn + " stack)");
		} else {
			rep.lines.push_back("spawn leaks: no " + cn + ".HA6 in " + opt.mbaaccDataDir + " (not checked)");
		}
	}

	// Giants: MBAACC's giant reaction layout.
	if (opt.giantFlag) {
		FrameData donor;
		if (!opt.giantDonorHa6.empty() && donor.load(opt.giantDonorHa6.c_str())) {
			rep.giantAdded = AddGiantReactions(fd, donor, opt.giantFlag, &rep.lines);
		} else {
			rep.lines.push_back("giant reactions: cannot load donor " + opt.giantDonorHa6);
			return false;
		}
	}
	fd.m_ha4.reset();             // force the HA6 writer
	rep.ha6Path = (out / (base + ".ha6")).string();
	if (!fd.save(rep.ha6Path.c_str())) { rep.lines.push_back("could not write " + rep.ha6Path); return false; }
	int pats = 0, frames = 0;
	for (auto &s : fd.m_sequences) if (!s.frames.empty()) { pats++; frames += (int)s.frames.size(); }
	rep.lines.push_back("ha6: " + std::to_string(pats) + " patterns, " + std::to_string(frames) + " frames");

	// --- CG ---
	const Ha4Container &c = *src.m_ha4;
	if (opt.writeCG && !c.cg.empty()) {
		rep.cgPath = (out / (base + ".cg")).string();
		if (!WriteBytes(rep.cgPath, c.cg.data(), c.cg.size())) { rep.lines.push_back("could not write " + rep.cgPath); return false; }
		if (!memcmp(c.cg.data(), "BMP Cutter2", 11))
			rep.lines.push_back("cg: source is \"BMP Cutter2\" (GAKIHA); written as-is");
	}

	// --- parts ---
	if (opt.writePat && !c.parts.empty()) {
		CG dummy;
		Parts parts(&dummy);
		std::string err;
		if (ha4::OldPatToParts(c.parts.data(), c.parts.size(), parts, &err)) {
			rep.patPath = (out / (base + ".pat")).string();
			if (!parts.Save(rep.patPath.c_str())) { rep.lines.push_back("could not write " + rep.patPath); return false; }
			rep.lines.push_back("pat: " + std::to_string(parts.partSets.size()) + " part sets, " +
			                    std::to_string(parts.cutOuts.size()) + " cutouts, " +
			                    std::to_string(parts.gfxMeta.size()) + " textures");
		} else {
			rep.lines.push_back("pat: " + err);
		}
	}

	// --- palette ---
	if (opt.writePal) {
		std::string pal = opt.palPath.empty() ? FindSiblingOr(dat, ".pal", opt.acBaseDir) : opt.palPath;
		if (!pal.empty()) {
			rep.palPath = (out / (base + ".pal")).string();
			char *pd = nullptr; unsigned int pn = 0;
			bool same = fs::equivalent(pal, rep.palPath, ec);
			// (read + atomic write: MinGW's copy_file(overwrite_existing) fails on existing targets)
			if (!same && !(ReadInMem(pal.c_str(), pd, pn) && WriteBytes(rep.palPath, pd, pn))) {
				rep.lines.push_back("could not copy palette " + pal);
				rep.palPath.clear();
			}
			delete[] pd;
		} else {
			rep.lines.push_back("pal: no .PAL next to the .DAT (the CG's built-in palette applies)");
		}
	}

	// --- txt ---
	if (opt.writeTxt) {
		rep.txtPath = (out / (base + ".txt")).string();
		std::ofstream t(rep.txtPath, std::ios::binary | std::ios::trunc);
		t << "[DataFile]\r\nFileNum=1\r\nFile00=" << base << ".ha6\r\n";
		if (!rep.cgPath.empty()) t << "\r\n[BmpcutFile]\r\nFileNum=1\r\nFile00=" << base << ".cg\r\n";
		if (!rep.patPath.empty()) t << "\r\n[PAniFile]\r\nFileNum=1\r\nFile00=" << base << ".pat\r\n";
		if (!t) { rep.lines.push_back("could not write " + rep.txtPath); return false; }
	}

	// --- command file: _C.TXT commands + the CT status of the reference version ---
	if (opt.writeCmd) {
		std::string stem = dat.stem().string();
		std::string txt = FindSiblingOr(dat, "_c.txt", opt.acBaseDir), ctPath, label;
		if (opt.acVersion == AcVersion::V103A && !opt.acPatchDir.empty()) {
			std::string want = Lower(stem + "_c.ct");
			for (auto &e : fs::directory_iterator(fs::path(opt.acPatchDir), ec))
				if (Lower(e.path().filename().string()) == want) ctPath = e.path().string();
			if (!ctPath.empty()) label = "1.03A CT (06.p) " + fs::path(ctPath).filename().string();
		}
		if (ctPath.empty()) {
			ctPath = FindSiblingOr(dat, "_c.ct", opt.acBaseDir);
			if (!ctPath.empty()) {
				label = std::string(opt.acVersion == AcVersion::V103A ? (opt.acPatchDir.empty() ? "1.00 CT (no 1.03A patch dir given!) " : "1.00 CT, unchanged in 1.03A ")
				                                                     : "1.00 CT ") + fs::path(ctPath).filename().string();
			}
		}
		CtStatus ct;
		std::string err;
		bool haveCt = !ctPath.empty() && ReadCtStatus(ctPath, ct, &err);
		if (!ctPath.empty() && !haveCt) rep.lines.push_back("ct: " + err);
		if (haveCt) {
			rep.gutsSource = label;
			for (int q = 0; q < 4; q++) rep.guts[q] = ct.guts(q);
		}
		if (!txt.empty()) {
			rep.cmdPath = (out / (base + "_cc_c.txt")).string();
			int highJump = HasFrames(fd, 41) ? 41 : -1;
			if (!EmitCommandFile(txt, haveCt ? &ct : nullptr, label, rep.cmdPath, &rep.lines, highJump)) return false;
		} else if (haveCt) {
			std::string l = "cmd: no _C.TXT (CT command records are not decoded), not written; CT guts (" + label + "):";
			for (int q = 0; q < 4; q++) l += " " + FmtF(ct.guts(q));
			rep.lines.push_back(l);
		}
	}

	// --- per-character EX-shield sidecar (moon 3; format docs/actcadenza_parity/ACTCADENZA_MOON_SIDECAR.md) ---
	if (opt.writeShieldSidecar && rep.exShield[0] >= 0) {
		bool differs = false;
		for (int k = 0; k < 3; k++) if (rep.exShield[k] >= 0 && rep.exShield[k] != kExShieldCommon[k]) differs = true;
		std::string sec = opt.sidecarSection;
		if (sec.empty()) { sec = opt.ccName.empty() ? base : opt.ccName; for (auto &ch : sec) ch = (char)toupper((unsigned char)ch); }
		if (differs) {
			rep.sidecarPath = (out / (base + "_actcadenza_moon.ini")).string();
			static const char *keys[3] = {"ExShieldTime_Stand", "ExShieldTime_Crouch", "ExShieldTime_Air"};
			std::string t = "; ActCadenza moon-3 override for " + sec + " (ha4tool convert, ActCadenza " + AcVersionName(opt.acVersion) +
			                "). [Type_3] has the common 6/6/8.\r\n[" + sec + "]\r\n";
			for (int k = 0; k < 3; k++)
				if (rep.exShield[k] >= 0 && rep.exShield[k] != kExShieldCommon[k]) t += std::string(keys[k]) + " = " + std::to_string(rep.exShield[k]) + "\r\n";
			if (!WriteFileAtomic(rep.sidecarPath.c_str(), t.data(), t.size())) { rep.lines.push_back("could not write " + rep.sidecarPath); return false; }
			rep.lines.push_back("ExShieldTime sidecar: " + fs::path(rep.sidecarPath).filename().string() + " [" + sec + "] " +
			                    std::to_string(rep.exShield[0]) + "/" + std::to_string(rep.exShield[1]) + "/" + std::to_string(rep.exShield[2]) +
			                    " (common [Type_3] 6/6/8)");
		} else {
			rep.lines.push_back("ExShieldTime: common [Type_3] 6/6/8 applies (no sidecar)");
		}
	}
	return true;
}

bool ConvertFile(const std::string &datPath, const Options &opt, Report &rep)
{
	FrameData fd;
	std::string err;
	if (!ha4::LoadFile(fd, datPath.c_str(), &err)) { rep.lines.push_back("load: " + err); return false; }
	return Convert(fd, datPath, opt, rep);
}

} // namespace ha4conv
