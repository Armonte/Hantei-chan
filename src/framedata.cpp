#include "framedata.h"
#include "framedata_load.h"
#include "framedata_ha4.h"
#include <fstream>
#include <algorithm>
#include "misc.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iostream>

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
	d = fd_main_load(d, d_end, m_sequences, sequence_count, utf8, &defined, patch && fillOnly, &m_stubs[m_loadIndex]);
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
	m_ha4.reset();
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
	case Ha6Game::UNI: return "UNI/UNI2";
	case Ha6Game::MBTL: return "MBTL";
	default: return "Auto";
	}
}

Ha6Game FrameData::detectedGame() const
{
	if (m_gameCacheVersion == dataVersion && m_gameCacheVersion != 0)
		return m_gameCache;
	// AFGX frames carry every layer the game has (UNI2 always writes ids
	// 0..4, MBTL 0..2: Han6_LoadFrameAF ignores ids >= 5 / >= 3).
	bool modern = false;
	size_t maxLayers = 0;
	for (const auto& seq : m_sequences) {
		if (seq.usedATV2 || seq.usedAFGX) modern = true;
		if (!seq.usedAFGX) continue;
		for (const auto& f : seq.frames)
			maxLayers = std::max(maxLayers, f.AF.layers.size());
	}
	Ha6Game g = Ha6Game::MBAACC;
	if (modern)
		g = (maxLayers == 3) ? Ha6Game::MBTL : Ha6Game::UNI;
	m_gameCache = g;
	m_gameCacheVersion = dataVersion;
	return g;
}

Ha6Game FrameData::game() const
{
	if (g_ha6GameOverride != Ha6Game::Auto) return g_ha6GameOverride;
	return detectedGame();
}

bool FrameData::usesUniFormat() const {
	const Ha6Game g = game();
	return g == Ha6Game::UNI || g == Ha6Game::MBTL;
}

int FrameData::uniLayerCountForSave() const
{
	// HA4 data exported to HA6 stays MBAACC-style.
	if (m_ha4 || !usesUniFormat()) return 0;
	return gameLayerCount();
}

int FrameData::gameLayerCount() const
{
	switch (game()) {
	case Ha6Game::UNI: return 5;
	case Ha6Game::MBTL: return 3;
	default: return 1;
	}
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
