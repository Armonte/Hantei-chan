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
static void RemapShield(FrameData &fd, std::vector<std::string> *log)
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

void PrepareForHA6(FrameData &fd, const Options &opt, std::vector<std::string> *log, const FrameData *cc)
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

	if (opt.shieldRemap) RemapShield(fd, log);

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

static bool WriteBytes(const std::string &path, const void *d, size_t n)
{
	return WriteFileAtomic(path.c_str(), d, n);
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
		int n = 0;
		for (const char *suf : {"", "_r", "_0", "_0_r"}) {
			fs::path want(opt.ccDataDir);
			std::string f = Lower(cn + suf + ".ha6"), hit;
			for (auto &e : fs::directory_iterator(want, ec)) if (Lower(e.path().filename().string()) == f) hit = e.path().string();
			if (!hit.empty() && ccData.load(hit.c_str(), n > 0)) n++;
		}
		if (n) cc = &ccData;
		else rep.lines.push_back("cc: no " + cn + ".HA6 in " + opt.ccDataDir);
	}
	PrepareForHA6(fd, opt, &rep.lines, cc);
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
		std::string pal = opt.palPath.empty() ? FindSibling(dat, ".pal") : opt.palPath;
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
