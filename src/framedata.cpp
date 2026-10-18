#include "framedata.h"
#include "framedata_load.h"
#include "framedata_ha4.h"
#include "framedata_han2.h"
#include "framedata_pb2k1.h"
#include "framedata_qoh.h"
#include "framedata_gof1.h"
#include <fstream>
#include <algorithm>
#include "misc.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <cctype>

int maxCount = 0;
std::set<int> numberSet;

// Process-wide version clock: every bump gets a unique value, so a new
// FrameData at a recycled address never repeats an old (pointer, version).
static uint64_t NextFrameDataVersion()
{
	static uint64_t clock = 0;
	return ++clock;
}

void FrameData::initEmpty(unsigned int count)
{
	Free();
	m_nsequences = count;
	m_sequences.resize(m_nsequences);
	m_loaded = 1;
}

bool FrameData::load(const char *filename, bool patch, bool fillOnly) {
	// allow loading over existing data

	char *data;
	unsigned int size;

	if (!ReadInMem(filename, data, size)) {
		return 0;
	}

	// French-Bread RBO / GOF2 HAN2RBO file (detected by content)
	if (han2::IsHan2(data, size)) {
		bool ok = !patch && han2::Load(*this, (const uint8_t *)data, size);
		delete[] data;
		if (ok && m_han2) m_han2->sourcePath = filename;
		return ok;
	}

	// MBAC Hantei4 .DAT (detected by content, not extension)
	if (ha4::IsHA4(data, size)) {
		bool ok = !patch && ha4::Load(*this, (const uint8_t *)data, size);
		delete[] data;
		if (ok && m_ha4) m_ha4->sourcePath = filename;
		return ok;
	}

	// verify header
	if (memcmp(data, "Hantei6DataFile", 15)) {
		delete[] data;

		return 0;
	}
	if (g_ha6SpriteRefSink) g_ha6SpriteRefSink->base = data;

	// Check for legacy UTF-8 flag (old Hantei-chan set byte 31 to 0xFF for UTF-8 files)
	// Modern files always use Shift-JIS and don't set this flag
	bool utf8 = ((unsigned char*)data)[31] == 0xFF;

	// initialize the root
	unsigned int *d = (unsigned int *)(data + 0x20);
	unsigned int *d_end = (unsigned int *)(data + size);
	if (memcmp(d, "_STR", 4)) {
		delete[] data;
		return 0;
	}

	test.filename = filename;

	unsigned int sequence_count = d[1];

	if(!patch)
		Free();

	if(sequence_count > m_nsequences)
		m_sequences.resize(sequence_count);
	// A patch file with fewer slots (UNI BaseData has 48) must not shrink the
	// character: that hid, and dropped on save, every later pattern.
	if(!patch || sequence_count > m_nsequences)
		m_nsequences = sequence_count;

	d += 2;
	// parse and recursively store data
	++m_loadIndex;
	std::vector<unsigned int> defined;
	m_stubs.resize(m_loadIndex + 1);
	d = fd_main_load(d, d_end, m_sequences, sequence_count, utf8, &defined, patch && fillOnly, &m_stubs[m_loadIndex], &m_templates);
	if(m_origin.size() < m_sequences.size())
		m_origin.resize(m_sequences.size(), -1);
	for(unsigned int id : defined)
		if(id < m_origin.size()) m_origin[id] = m_loadIndex;

	// Clear modified flags after loading - only track NEW edits from this session
	for(auto& seq : m_sequences) {
		seq.modified = false;
	}

	// cleanup and finish
	delete[] data;

	dataVersion = NextFrameDataVersion();
	m_loaded = 1;
	// Detect the dialect now, from the loaded data only: pasted patterns are
	// converted to it later instead of re-detecting the file (D13).
	if (m_sourcePath.empty()) m_sourcePath = filename;
	m_detected = DetectHa6Game(m_sequences, m_sourcePath);
	return 1;
}


#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

// Hitbox cleanup applied to the *written* data only: degenerate boxes are
// dropped and inverted boxes are fixed -- but only boxes created or edited in
// this session. Boxes exactly as loaded are game data and are written as they
// are: MBTL chr016 (Powered Ciel) p6 f6 and p25 f2-3 have inverted attack
// boxes, and "fixing" them moved the box on every save. Returns true if
// anything would change.
static bool BoxIsAsLoaded(const Frame &frame, int loc, const Hitbox &box)
{
	const Ha6FrameEnc &enc = frame.ha6;
	if (enc.valid && loc >= 0 && loc < Ha6FrameEnc::kMaxBoxes && ((enc.boxMask >> loc) & 1ull))
		return !memcmp(enc.boxXY[loc], box.xy, sizeof(box.xy));
	return false;
}

static bool SequenceNeedsBoxFix(const Sequence &seq)
{
	for(const auto &frame : seq.frames)
	for(const auto &it : frame.hitboxes)
	{
		const Hitbox &box = it.second;
		if(BoxIsAsLoaded(frame, it.first, box))
			continue;
		if(box.xy[0] >= box.xy[2] || box.xy[1] >= box.xy[3])
			return true;
	}
	return false;
}

static void FixBoxesForSave(Sequence &seq)
{
	for(auto &frame : seq.frames)
	for(auto it = frame.hitboxes.begin(); it != frame.hitboxes.end();)
	{
		Hitbox &box = it->second;
		if(BoxIsAsLoaded(frame, it->first, box))
		{
			++it;
			continue;
		}
		//Delete degenerate boxes when exporting.
		if( (box.xy[0] == box.xy[2]) ||
			(box.xy[1] == box.xy[3]) )
		{
			frame.hitboxes.erase(it++);
		}
		else
		{
			//Fix inverted boxes drawn backwards in the editor.
			if(box.xy[0] > box.xy[2])
				std::swap(box.xy[0], box.xy[2]);
			if(box.xy[1] > box.xy[3])
				std::swap(box.xy[1], box.xy[3]);
			++it;
		}
	}
}

// Serialize to memory, then atomically replace the target file. The in-memory
// sequences are never modified: sequences that need box cleanup are written
// from a temporary copy.
static bool WriteHA6File(const char *filename, const std::vector<Sequence> &sequences,
                         uint32_t count, bool modifiedOnly,
                         const std::vector<int> *origin = nullptr, int ownFile = -1,
                         const std::map<unsigned int, Sequence> *ownStubs = nullptr,
                         int uniLayerCount = 0)
{
	if(!filename || !*filename)
		return false;

	std::ostringstream file(std::ios_base::out | std::ios_base::binary);

	char header[32] = "Hantei6DataFile";

	// Keep header in original format - no modification flag
	file.write(header, sizeof(header));

	file.write("_STR", 4); file.write(VAL(count), 4);

	for(uint32_t i = 0; i < count && i < sequences.size(); i++)
	{
		const Sequence &src = sequences[i];
		if(modifiedOnly && !src.modified)
			continue;

		file.write("PSTR", 4); file.write(VAL(i), 4);
		// Stacked character: leave patterns inherited from another file of
		// the stack (and not edited) as empty slots in this one.
		if(ownFile >= 0 && origin && !src.modified &&
		   (i >= origin->size() || (*origin)[i] != ownFile))
		{
			if(ownStubs) {
				auto it = ownStubs->find(i);
				if(it != ownStubs->end()) WriteSequence(file, &it->second, uniLayerCount);
			}
			file.write("PEND", 4);
			continue;
		}
		if(SequenceNeedsBoxFix(src))
		{
			Sequence copy = src;
			FixBoxesForSave(copy);
			WriteSequence(file, &copy, uniLayerCount);
		}
		else
		{
			WriteSequence(file, &src, uniLayerCount);
		}
		file.write("PEND", 4);
	}

	file.write("_END", 4);
	if(!file)
		return false;

	const std::string bytes = file.str();
	return WriteFileAtomic(filename, bytes.data(), bytes.size());
}

// HA4 data is written back as HA4 unless the target is explicitly *.ha6
// (then it is exported through the HA6 writer, i.e. converted).
static bool TargetIsHA6(const char *filename)
{
	std::string f = filename ? filename : "";
	if (f.size() < 4) return false;
	std::string ext = f.substr(f.size() - 4);
	for (auto &c : ext) c = (char)tolower((unsigned char)c);
	return ext == ".ha6";
}

bool FrameData::save(const char *filename)
{
	if (m_ha4 && !TargetIsHA6(filename))
		return ha4::SaveFile(*this, filename);
	if (m_han2 && m_han2->sub == 3) return gof1::SaveFile(*this, filename);
	if (m_han2 && m_han2->sub == 4) return pb2k1::SaveFile(*this, filename);
	if (m_han2 && m_han2->sub == 5) return qoh::SaveFile(*this, filename);
	if (m_han2) {
		std::string f = filename ? filename : "";
		std::string ext = f.size() >= 4 ? f.substr(f.size() - 4) : "";
		for (auto &c : ext) c = (char)tolower((unsigned char)c);
		return han2::SaveFile(*this, filename, nullptr, nullptr, ext == ".dt2");
	}
	const std::map<unsigned int, Sequence> *stubs =
		(m_ownFile >= 0 && m_ownFile < (int)m_stubs.size()) ? &m_stubs[m_ownFile] : nullptr;
	return WriteHA6File(filename, m_sequences, get_sequence_count(), false, &m_origin, m_ownFile, stubs, uniLayerCountForSave());
}

int FrameData::StackSaveTarget(const std::vector<std::string>& names)
{
	auto shared = [](std::string n) {
		for (auto& c : n) c = (char)tolower((unsigned char)c);
		std::replace(n.begin(), n.end(), '\\', '/');
		if (n.rfind("../", 0) == 0) return true;
		const size_t slash = n.find_last_of('/');
		const std::string base = slash == std::string::npos ? n : n.substr(slash + 1);
		return base.find("basedata") != std::string::npos;
	};
	int t = (int)names.size() - 1;
	while (t > 0 && shared(names[t]))
		--t;
	return t;
}

int FrameData::inheritedPatternCount() const
{
	if(m_ownFile < 0) return 0;
	int n = 0;
	for(size_t i = 0; i < m_sequences.size() && i < m_nsequences; ++i)
		if(!m_sequences[i].modified && i < m_origin.size() && m_origin[i] >= 0 && m_origin[i] != m_ownFile)
			++n;
	return n;
}

bool FrameData::save_merged(const char *filename)
{
	return WriteHA6File(filename, m_sequences, get_sequence_count(), false, nullptr, -1, nullptr, uniLayerCountForSave());
}

bool FrameData::save_modified_only(const char *filename)
{
	// Only write modified sequences
	return WriteHA6File(filename, m_sequences, get_sequence_count(), true, nullptr, -1, nullptr, uniLayerCountForSave());
}

void FrameData::Free() {
	m_origin.clear();
	m_stubs.clear();
	notes = Ha6Notes{};
	m_loadIndex = -1;
	m_ownFile = -1;
	m_detected = Ha6Game::Auto;
	m_sourcePath.clear();
	m_templates.clear();
	m_ha4.reset();
	m_han2.reset();
	dataVersion = NextFrameDataVersion();
	m_sequences.clear();
	m_nsequences = 0;
	m_loaded = 0;
}

int FrameData::get_sequence_count() {
	if (!m_loaded) {
		return 0;
	}
	return m_nsequences;
}

Ha6Game g_ha6GameOverride = Ha6Game::Auto;

const char* Ha6GameName(Ha6Game g)
{
	switch (g) {
	case Ha6Game::MBAACC: return "MBAACC";
	case Ha6Game::UNI: return "UNI2";
	case Ha6Game::MBTL: return "MBTL";
	case Ha6Game::UNIB: return "UNIB";
	case Ha6Game::UNIST: return "UNIST";
	default: return "Auto";
	}
}

bool Ha6GameIsUni(Ha6Game g)
{
	return g == Ha6Game::UNI || g == Ha6Game::MBTL || g == Ha6Game::UNIB || g == Ha6Game::UNIST;
}

EngineAniStep EngineAniStepFor(Ha6Game g, int aniType, unsigned aniFlag)
{
	if (g == Ha6Game::MBAACC || g == Ha6Game::Auto) {
		switch (aniType) {
		case 3: return { 1, aniFlag | 1u };
		case 4: return { 2, aniFlag | 1u };
		case 5: return { 2, aniFlag | 2u };
		default: break;
		}
	}
	return { aniType, aniFlag };
}

unsigned AniFlagBitsRead(Ha6Game g)
{
	switch (g) {
	case Ha6Game::UNI: case Ha6Game::MBTL: return 0x8Fu;
	case Ha6Game::MBAACC: case Ha6Game::Auto: return 0x0Fu;
	default: return 0u;
	}
}

const char* AniFlagBitLabel(Ha6Game g, int bit)
{
	if (bit < 0 || bit > 31 || !(AniFlagBitsRead(g) & (1u << bit))) return nullptr;
	switch (bit) {
	case 0: return "Landing frame: Land to pattern";
	case 1: return "Loop: Decrement loops and go to end if 0";
	case 2: return "Go to: Use relative offset";
	case 3: return "End of loop: Use relative offset";
	case 7: return "Interpolation: draw blend mode 3 instead of 1 (AFHK frames)";
	default: return nullptr;
	}
}

int Ha6GameLayerCount(Ha6Game g)
{
	switch (g) {
	case Ha6Game::UNI: case Ha6Game::UNIST: return 5;
	case Ha6Game::MBTL: case Ha6Game::UNIB: return 3;
	default: return 1;
	}
}

namespace {
std::string LowerAscii(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }
// "Hyd", "Hyd_0", "Hyd_s": UNIB's three-letter character naming.
bool IsUnibName(const std::string& stem)
{
	if (stem.size() < 3) return false;
	if (!(stem[0] >= 'A' && stem[0] <= 'Z')) return false;
	for (int i = 1; i < 3; ++i) if (!(stem[i] >= 'a' && stem[i] <= 'z')) return false;
	if (stem.size() == 3) return true;
	return stem.size() == 5 && stem[3] == '_' && ((stem[4] >= '0' && stem[4] <= '9') || stem[4] == 's');
}
// MBTL/UNI2 naming: chrNNN, edit_chrNNN, ava_/csel_chrNNN, npcNNN, effect_temp.
bool IsChrName(const std::string& lowerStem)
{
	for (const char* p : {"chr", "edit_chr", "ava_chr", "csel_chr", "npc"}) {
		const size_t n = strlen(p);
		if (lowerStem.compare(0, n, p) == 0 && lowerStem.size() >= n + 3 &&
		    isdigit((unsigned char)lowerStem[n]) && isdigit((unsigned char)lowerStem[n + 1]) && isdigit((unsigned char)lowerStem[n + 2]))
			return true;
	}
	return lowerStem == "effect_temp";
}
// 3-layer file whose tags don't decide: UNIB vs MBTL from the file naming.
Ha6Game ThreeLayerGameFromPath(const std::string& path)
{
	if (path.empty()) return Ha6Game::MBTL;
	std::error_code ec;
	const std::filesystem::path fp = std::filesystem::u8path(path);
	const std::string stem = fp.stem().u8string();
	const std::string dir = fp.parent_path().filename().u8string();
	if (IsChrName(LowerAscii(stem)) || IsChrName(LowerAscii(dir))) return Ha6Game::MBTL;
	if (IsUnibName(stem) || IsUnibName(dir)) return Ha6Game::UNIB;
	if (stem.compare(0, 5, "csel_") == 0 && IsUnibName(stem.substr(5))) return Ha6Game::UNIB; // _csel/csel_Hyd
	// Shared files at the data root (BaseData, effect, effect_s): look at the
	// character folders next to them.
	for (auto it = std::filesystem::directory_iterator(fp.parent_path(), ec);
	     !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
		if (!it->is_directory(ec)) continue;
		const std::string n = it->path().filename().u8string();
		if (IsChrName(LowerAscii(n))) return Ha6Game::MBTL;
		if (n.size() == 5 && IsUnibName(n)) return Ha6Game::UNIB;
	}
	return Ha6Game::MBTL;
}
}

Ha6Game DetectHa6Game(const std::vector<Sequence>& seqs, const std::string& path)
{
	// AFGX frames carry every layer the game has (UNI2 always writes ids
	// 0..4, MBTL/UNIB 0..2: Han6_LoadFrameAF ignores ids >= 5 / >= 3).
	bool modern = false, unibTags = false, mbtlTags = false;
	size_t maxLayers = 0;
	const int iAFJH = ha6order::Index(ha6order::kAF, "AFJH");
	const int iATHH = ha6order::Index(ha6order::kAT, "ATHH");
	const int iATC0 = ha6order::Index(ha6order::kAT, "ATC0");
	for (const auto& seq : seqs) {
		if (seq.usedATV2 || seq.usedAFGX) modern = true;
		// PSTS/PLVL: UNIB still writes them (12 of 61 files); UNI2/MBTL never do.
		if (seq.psts || seq.level) unibTags = true;
		for (const auto& f : seq.frames) {
			if (seq.usedAFGX) maxLayers = std::max(maxLayers, f.AF.layers.size());
			// AFJH, ATHH, ATC0: MBTL/UNI2 tags that no UNIB file has.
			if (((f.ha6.afTags >> iAFJH) & 1) || ((f.ha6.atTags >> iATHH) & 1) || ((f.ha6.atTags >> iATC0) & 1))
				mbtlTags = true;
		}
	}
	if (!modern) {
		// UNIB project stubs (Hyd_0/_temp.HA6, Hyd_0/Hyd_0.HA6: names only, at
		// most one frame, no AFGX/ATV2) sit in a "Xxx_N" character folder.
		size_t frames = 0;
		for (const auto& seq : seqs) frames += seq.frames.size();
		if (frames <= 1 && !path.empty()) {
			const std::string dir = std::filesystem::u8path(path).parent_path().filename().u8string();
			if (dir.size() == 5 && IsUnibName(dir)) return Ha6Game::UNIB;
		}
		return Ha6Game::MBAACC;
	}
	if (maxLayers == 3) {
		if (unibTags && !mbtlTags) return Ha6Game::UNIB;
		if (mbtlTags) return Ha6Game::MBTL;
		return ThreeLayerGameFromPath(path);
	}
	return Ha6Game::UNI;
}

Ha6Game FrameData::detectedGame() const
{
	if (m_detected == Ha6Game::Auto)
		m_detected = DetectHa6Game(m_sequences, m_sourcePath);
	return m_detected;
}

Ha6Game FrameData::game() const
{
	if (m_gameOverride != Ha6Game::Auto) return m_gameOverride;
	if (g_ha6GameOverride != Ha6Game::Auto) return g_ha6GameOverride;
	return detectedGame();
}

bool FrameData::usesUniFormat() const {
	return Ha6GameIsUni(game());
}

int FrameData::uniLayerCountForSave() const
{
	// HA4 data exported to HA6 stays MBAACC-style.
	if (m_ha4 || !usesUniFormat()) return 0;
	return gameLayerCount();
}

int FrameData::gameLayerCount() const
{
	return Ha6GameLayerCount(game());
}

bool FrameData::hasNoFrames() const
{
	for (const auto& s : m_sequences) if (!s.frames.empty()) return false;
	return true;
}

const Sequence* FrameData::templateFor(int n) const
{
	if (n < 0) return nullptr;
	auto it = m_templates.find((unsigned)n);
	if (it == m_templates.end()) return nullptr;
	if ((size_t)n >= m_sequences.size() || m_sequences[n].frames.empty()) return nullptr;
	return &it->second;
}

// ---------------------------------------------------------------------------
// D13: conversion of pasted data to the file's dialect.
namespace {
bool LayerHasSprite(const Layer_Type& l) { return l.spriteId >= 0; }

// UNI/MBTL-only fields that the MBAACC writer would emit (or the game can't use).
bool ClearUniOnly(Frame& f)
{
	bool any = false;
	auto z = [&](auto& v, auto def) { if (v != def) { v = def; any = true; } };
	z(f.AF.frameId, 0); z(f.AF.afjh, false);
	if (f.AF.param[0] || f.AF.param[1] || f.AF.param[2] || f.AF.param[3]) { memset(f.AF.param, 0, 4); any = true; }
	z(f.AS.ascf, 0);
	z(f.AT.damageProration, 100); z(f.AT.minDamage, 0); z(f.AT.addHitStun, 0); z(f.AT.starterCorrection, 0);
	z(f.AT.atrf, 0); z(f.AT.atbc, 0); z(f.AT.atvd, 0);
	if (f.AT.hitStunDecay[0] || f.AT.hitStunDecay[1] || f.AT.hitStunDecay[2]) { memset(f.AT.hitStunDecay, 0, sizeof(f.AT.hitStunDecay)); any = true; }
	for (auto& l : f.AF.layers) z(l.priority, 0);
	return any;
}
// MBAACC-only AT fields that the UNI writer never writes (D14).
bool ClearMbaaccOnlyAT(Frame_AT& at)
{
	bool any = at.red_damage || at.guard_damage || at.breakTime || at.extraGravity != 0.f;
	at.red_damage = at.guard_damage = at.breakTime = 0; at.extraGravity = 0.f;
	return any;
}

// One frame to `game`. Returns true if it changed.
bool ConformFrame(Frame& f, Ha6Game game, Ha6ConvertReport& r)
{
	bool changed = false;
	auto& L = f.AF.layers;
	if (Ha6GameIsUni(game)) {
		const size_t cap = (size_t)Ha6GameLayerCount(game);
		if (L.size() > cap) {
			for (size_t i = cap; i < L.size(); ++i) { ++r.layersDropped; if (LayerHasSprite(L[i])) ++r.spriteLayersDropped; }
			L.resize(cap);
			for (size_t i = cap; i < 5; ++i) f.ha6.layerTags[i] = 0;
			changed = true;
		}
		if (ClearMbaaccOnlyAT(f.AT)) { ++r.mbaaccFieldsCleared; changed = true; }
	} else {
		if (L.size() > 1) {
			// MBAACC (AFGP) has one layer: keep layer 0, or the first layer
			// with a sprite when layer 0 is empty (MBTL keeps the body on 1).
			size_t keep = 0;
			if (!LayerHasSprite(L[0]))
				for (size_t i = 1; i < L.size(); ++i) if (LayerHasSprite(L[i])) { keep = i; break; }
			for (size_t i = 0; i < L.size(); ++i) if (i != keep) { ++r.layersDropped; if (LayerHasSprite(L[i])) ++r.spriteLayersDropped; }
			Layer_Type k = L[keep];
			L.clear(); L.push_back(k);
			f.ha6 = Ha6FrameEnc{}; // the per-layer "present at load" bits no longer match
			changed = true;
		}
		if (!L.empty() && L[0].afrt) { f.AF.AFRT = true; L[0].afrt = false; changed = true; }
		if (ClearUniOnly(f)) { ++r.uniFieldsCleared; changed = true; }
	}
	if (changed) ++r.frames;
	return changed;
}
}

Ha6ConvertReport FrameData::conformPasted(Sequence& seq)
{
	Ha6ConvertReport r;
	if (m_gameOverride == Ha6Game::Auto && g_ha6GameOverride == Ha6Game::Auto && hasNoFrames()) {
		// A new, empty file takes the pasted pattern's dialect.
		std::vector<Sequence> one(1); one[0] = seq;
		m_detected = DetectHa6Game(one, m_sourcePath);
		r.to = m_detected;
		r.adopted = seq.usedAFGX || seq.usedATV2;
		return r;
	}
	const Ha6Game g = game();
	r.to = g;
	const bool uni = Ha6GameIsUni(g);
	bool changed = false;
	if (uni) {
		// An MBAACC pattern becomes a UNI one so it is written with AFGX/ATV2
		// like the rest (a UNI pattern keeps its own flags: byte-exact paste).
		if (!seq.usedAFGX && !seq.usedATV2) { seq.usedAFGX = seq.usedATV2 = true; changed = true; }
	} else {
		if (seq.usedAFGX || seq.usedATV2) { seq.usedAFGX = seq.usedATV2 = false; changed = true; }
		if (seq.pups) { seq.pups = 0; changed = true; }
	}
	for (auto& f : seq.frames) changed |= ConformFrame(f, g, r);
	if (changed) r.patterns = 1;
	return r;
}

Ha6ConvertReport FrameData::conformPasted(Frame& frame)
{
	Ha6ConvertReport r;
	r.to = game();
	if (m_gameOverride == Ha6Game::Auto && g_ha6GameOverride == Ha6Game::Auto && hasNoFrames())
		return r; // nothing to conform to yet
	ConformFrame(frame, r.to, r);
	return r;
}

Ha6ConvertReport FrameData::conformPastedAT(Frame_AT& at) const
{
	Ha6ConvertReport r;
	r.to = game();
	if (Ha6GameIsUni(r.to)) {
		if (ClearMbaaccOnlyAT(at)) { ++r.mbaaccFieldsCleared; ++r.frames; }
	} else {
		Frame f; f.AT = at;
		if (ClearUniOnly(f)) { at = f.AT; ++r.uniFieldsCleared; ++r.frames; }
	}
	return r;
}

// ---------------------------------------------------------------------------
// D17: BaseData template merge (Han6_MergeTemplatePattern_PFLG).
Sequence MergeTemplatePattern(const Sequence& tmpl, const Sequence& chr, int layerCount)
{
	Sequence out = tmpl;
	const size_t n = std::min(tmpl.frames.size(), chr.frames.size());
	for (size_t i = 0; i < n; ++i) {
		Frame& o = out.frames[i];
		const Frame& c = chr.frames[i];
		if ((int)o.AF.layers.size() < layerCount) o.AF.layers.resize(layerCount);
		for (int l = 0; l < layerCount; ++l) {
			Layer_Type src; src.spriteId = -1;
			if (l < (int)c.AF.layers.size()) src = c.AF.layers[l];
			Layer_Type& d = o.AF.layers[l];
			if (l >= (int)tmpl.frames[i].AF.layers.size()) d.spriteId = -1;
			d.usePat = src.usePat;
			d.spriteId = src.spriteId;
			d.offset_x = src.offset_x;
			d.offset_y = src.offset_y;
		}
		// Template frames with an AT block take the character's attack boxes;
		// all frames take its hurt boxes.
		const bool tmplAT = tmpl.frames[i].ha6.hadAT || tmpl.frames[i].hitboxes.lower_bound(25) != tmpl.frames[i].hitboxes.end();
		BoxList boxes;
		for (const auto& kv : c.hitboxes)
			if (kv.first < 25 || tmplAT) boxes[kv.first] = kv.second;
		o.hitboxes = boxes;
	}
	out.flag = chr.flag;
	return out;
}

void PasteLayerRange(Sequence& seq, const Frame& src, int first, int last, int layer, LayerPaste what)
{
	const int nsrc = (int)src.AF.layers.size();
	if (nsrc == 0) return;
	int l0 = layer, l1 = layer;
	if (layer < 0) { l0 = 0; l1 = nsrc - 1; }
	if (l0 >= nsrc) return;
	l1 = std::min(l1, nsrc - 1);
	for (int i = std::max(first, 0); i <= last && i < (int)seq.frames.size(); ++i) {
		Frame& f = seq.frames[i];
		if (what == LayerPaste::SpriteDuration) f.AF.duration = src.AF.duration;
		if (what == LayerPaste::Rotation || what == LayerPaste::AllTransforms) f.AF.AFRT = src.AF.AFRT;
		if (what == LayerPaste::AllTransforms) f.AF.priority = src.AF.priority;
		for (int l = l0; l <= l1; ++l) {
			if ((int)f.AF.layers.size() <= l) {
				Layer_Type e; e.spriteId = -1;
				f.AF.layers.resize(l + 1, e);
			}
			const Layer_Type& s = src.AF.layers[l];
			Layer_Type& d = f.AF.layers[l];
			switch (what) {
			case LayerPaste::SpriteDuration: d.spriteId = s.spriteId; d.usePat = s.usePat; break;
			case LayerPaste::Offset: d.offset_x = s.offset_x; d.offset_y = s.offset_y; break;
			case LayerPaste::Rotation: memcpy(d.rotation, s.rotation, sizeof(d.rotation)); d.afrt = s.afrt; break;
			case LayerPaste::Scale: memcpy(d.scale, s.scale, sizeof(d.scale)); break;
			case LayerPaste::ColorBlend: memcpy(d.rgba, s.rgba, sizeof(d.rgba)); d.blend_mode = s.blend_mode; break;
			case LayerPaste::AllTransforms:
				d.offset_x = s.offset_x; d.offset_y = s.offset_y;
				memcpy(d.rotation, s.rotation, sizeof(d.rotation)); d.afrt = s.afrt;
				memcpy(d.scale, s.scale, sizeof(d.scale));
				memcpy(d.rgba, s.rgba, sizeof(d.rgba)); d.blend_mode = s.blend_mode;
				break;
			}
		}
	}
}

int DeleteLayerKeepIds(Frame_AF& af, int idx, bool uni)
{
	if (idx < 0 || idx >= (int)af.layers.size() || af.layers.size() <= 1) return std::max(0, std::min(idx, (int)af.layers.size() - 1));
	if (uni && idx != (int)af.layers.size() - 1) {
		Layer_Type e; e.spriteId = -1;
		af.layers[idx] = e;
		return idx;
	}
	af.layers.erase(af.layers.begin() + idx);
	return std::min(idx, (int)af.layers.size() - 1);
}

Sequence* FrameData::get_sequence(int n) {
	if (!m_loaded) {
		return 0;
	}
	
	if (n < 0 || (unsigned int)n >= m_nsequences) {
		return 0;
	}
	
	return &m_sequences[n];
}

std::string FrameData::GetDecoratedName(int n)
{
		std::stringstream ss;
		ss.flags(std::ios_base::right);

		ss << std::setfill('0') << std::setw(3) << n << " ";

		if(!m_sequences[n].empty)
		{
			bool noFrames = m_sequences[n].frames.empty();
			if(noFrames)
				ss << u8"〇 ";

			if(m_sequences[n].name.empty() && m_sequences[n].codeName.empty() && !noFrames)
			{
					ss << u8"---";
			}
		}

		// Strings are already stored as UTF-8 in memory (converted during load)
		ss << m_sequences[n].name;
		if(!m_sequences[n].codeName.empty())
			ss << " - " << m_sequences[n].codeName;

		// Add asterisk for modified patterns
		if(m_sequences[n].modified)
			ss << " *";

		return ss.str();
}

Command* FrameData::get_command(int id)
{
	for(auto &cmd : m_commands) {
		if(cmd.id == id)
			return &cmd;
	}
	return nullptr;
}

void FrameData::mark_modified(int sequence_index)
{
	dataVersion = NextFrameDataVersion();
	if(sequence_index >= 0 && sequence_index < (int)m_sequences.size()) {
		m_sequences[sequence_index].modified = true;
		if(undoDirtyFeed.size() < kUndoDirtyFeedCap) undoDirtyFeed.push_back(sequence_index);
	}
}

// load_commands() lives in cmdfile/cmd_framedata.cpp (lossless _c.txt parser).

FrameData::FrameData() {
	m_nsequences = 0;
	m_loaded = 0;
}

FrameData::~FrameData() {
	Free();
}
