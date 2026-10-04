#include "mv_script.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <filesystem>
#include <cstdio>

#ifdef MV_SCRIPT_STANDALONE
// Standalone parser test build (host compiler): no framedata/windows deps.
static std::string sj2utf8(const std::string &input) { return input; }
#else
#include "framedata.h"
#include "misc.h"
#endif

namespace {

constexpr size_t kMaxFileSize = 2 * 1024 * 1024; // 2MB cap
constexpr size_t kMaxSpawns = 2048;              // sanity cap

// A spawn as found in a block, before owner association / resolution.
struct RawSpawn {
	MvScriptSpawn spawn;
	std::string caseLabel; // 'case "B":' label active at extraction site
	std::string fieldName; // mvParam field it was assigned to/read from (grouping)
};

// Deferred 'start_pat=mvParam.field' use, expanded when the block closes.
struct FieldUse {
	std::string field;
	std::string caseLabel;
	int offsetX = 0, offsetY = 0;
	int frameIdRef = -1;
	std::string source;
};

// A top-level block: either a move table ('t.Mv_X <- {...}') or a factory
// function ('local f = function(...) {...}').
struct Block {
	std::string name;
	bool isFactory = false;
	std::vector<RawSpawn> spawns;
	std::vector<FieldUse> fieldUses;
	std::vector<int> ownerExtra;            // 'mvs.DataPattern == N'
	// field -> list of (caseLabel, patternCode) options from pat_num_ vars
	std::map<std::string, std::vector<std::pair<std::string, std::string>>> patFields;
	// mvParam field -> indexes into 'spawns' of spawn tables declared in that
	// field; used to bind the frame gate at the CreateFireBall/CreateObject
	// use site to the declaration ("mvParam.ball = {..pat=..}" then
	// "case 100: CreateFireBall( mvParam.ball )" fires at frame ID 100).
	std::map<std::string, std::vector<size_t>> spawnFields;
};

// Removes // and /* */ comments while preserving string literals, counts curly
// braces outside strings, and is Shift-JIS aware (a kanji trail byte can be
// 0x22/0x5C/0x7B/0x7D — never treat those as syntax).
class LineCleaner {
public:
	// Returns cleaned code text; braces out param gets net {,} delta.
	std::string Clean(const std::string &line, int &net)
	{
		std::string out;
		out.reserve(line.size());
		net = 0;
		size_t i = 0;
		const size_t n = line.size();
		while (i < n) {
			unsigned char c = (unsigned char)line[i];
			if (inBlockComment) {
				if (c == '*' && i + 1 < n && line[i+1] == '/') {
					inBlockComment = false;
					i += 2;
					continue;
				}
				i += SjisAdvance(c);
				continue;
			}
			if (inString) {
				if (c == '\\' && i + 1 < n) { // escape (SJIS ¥ maps here too)
					out += line[i];
					out += line[i+1];
					i += 2;
					continue;
				}
				if (SjisLead(c)) { // double-byte char, trail may be 0x22/0x5C
					out += line[i];
					if (i + 1 < n) out += line[i+1];
					i += 2;
					continue;
				}
				out += line[i];
				if (c == '"')
					inString = false;
				i++;
				continue;
			}
			if (c == '/' && i + 1 < n && line[i+1] == '/')
				break; // line comment: rest ignored
			if (c == '/' && i + 1 < n && line[i+1] == '*') {
				inBlockComment = true;
				i += 2;
				continue;
			}
			if (c == '"') {
				inString = true;
				out += '"';
				i++;
				continue;
			}
			if (c == '{') net++;
			else if (c == '}') net--;
			if (SjisLead(c)) {
				out += line[i];
				if (i + 1 < n) out += line[i+1];
				i += 2;
				continue;
			}
			out += line[i];
			i++;
		}
		// Strings do not span lines in these scripts; reset defensively.
		inString = false;
		return out;
	}

private:
	bool inBlockComment = false;
	bool inString = false;

	static bool SjisLead(unsigned char c) {
		return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
	}
	static size_t SjisAdvance(unsigned char c) {
		return SjisLead(c) ? 2 : 1;
	}
};

// Parse "-123" or "20*128" style constants. "N*128" products are game
// subpixels (1 px = 128 units) and convert down to pixels; large plain
// values are assumed to be subpixels too.
int ParseCoord(const std::string &a, const std::string &b)
{
	long v = 0;
	bool subpixel = false;
	try {
		v = std::stol(a);
		if (!b.empty()) {
			long f = std::stol(b);
			v *= f;
			subpixel = (f == 128 || v == 128 * f); // either factor is 128
		}
	} catch (...) {
		return 0;
	}
	if (subpixel || v >= 2048 || v <= -2048)
		v /= 128;
	return (int)v;
}

std::string Trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Innermost {...} group containing pos — used to scope x=/y= extraction to the
// same table literal as the pat=/mvname= key (a sibling table on the same line
// often holds velocity x/y that must not be read as a spawn offset).
std::string BraceRegion(const std::string &code, size_t pos)
{
	if (pos >= code.size())
		return code;
	int depth = 0;
	size_t start = std::string::npos;
	for (size_t i = pos + 1; i-- > 0; ) {
		char c = code[i];
		if (c == '}') depth++;
		else if (c == '{') {
			if (depth == 0) { start = i; break; }
			depth--;
		}
	}
	if (start == std::string::npos)
		return code;
	depth = 0;
	size_t end = code.size();
	for (size_t i = start; i < code.size(); i++) {
		if (code[i] == '{') depth++;
		else if (code[i] == '}') {
			depth--;
			if (depth == 0) { end = i + 1; break; }
		}
	}
	return code.substr(start, end - start);
}

} // namespace

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

static std::map<const FrameData*, const MvScriptIndex*> &Registry()
{
	static std::map<const FrameData*, const MvScriptIndex*> reg;
	return reg;
}

void MvScriptIndex::Register(const FrameData *fd, const MvScriptIndex *index)
{
	if (fd)
		Registry()[fd] = index;
}

void MvScriptIndex::Unregister(const FrameData *fd)
{
	Registry().erase(fd);
}

const MvScriptIndex *MvScriptIndex::Lookup(const FrameData *fd)
{
	auto it = Registry().find(fd);
	if (it == Registry().end())
		return nullptr;
	return it->second;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

void MvScriptIndex::clear()
{
	m_loaded = false;
	m_files.clear();
	m_spawns.clear();
	m_ownerExtra.clear();
	m_byPattern.clear();
	m_byTemplate.clear();
}

bool MvScriptIndex::loadForCharacter(const std::string &txtPath)
{
	clear();

	std::string path = txtPath;
	for (auto &c : path)
		if (c == '\\') c = '/';

	size_t slash = path.find_last_of('/');
	std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
	std::string file = (slash == std::string::npos) ? path : path.substr(slash + 1);

	size_t dot = file.find_last_of('.');
	std::string stem = (dot == std::string::npos) ? file : file.substr(0, dot);

	// "chr019_0" -> base "chr019", suffix "0" -> "chr019_mv_0.txt"
	std::string base = stem, suffix;
	size_t us = stem.find_last_of('_');
	if (us != std::string::npos && us + 1 < stem.size() &&
	    stem.find_first_not_of("0123456789", us + 1) == std::string::npos) {
		base = stem.substr(0, us);
		suffix = stem.substr(us + 1);
	}

	std::error_code ec;
	bool any = false;

	if (!suffix.empty()) {
		std::string candidate = dir + "/" + base + "_mv_" + suffix + ".txt";
		if (std::filesystem::exists(candidate, ec) && loadScriptFile(candidate))
			any = true;
	}

	if (!any) {
		// Fallback: any sibling "<base>_mv_*.txt"
		std::string prefix = base + "_mv";
		for (auto &entry : std::filesystem::directory_iterator(dir, ec)) {
			if (ec) break;
			if (!entry.is_regular_file(ec)) continue;
			std::string name = entry.path().filename().string();
			if (name.size() <= prefix.size()) continue;
			if (name.compare(0, prefix.size(), prefix) != 0) continue;
			if (name.size() < 4 || name.compare(name.size() - 4, 4, ".txt") != 0) continue;
			if (loadScriptFile(entry.path().string()))
				any = true;
		}
	}

	m_loaded = any;
	return any;
}

bool MvScriptIndex::loadScriptFile(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;

	in.seekg(0, std::ios::end);
	std::streamoff size = in.tellg();
	in.seekg(0, std::ios::beg);
	if (size <= 0 || (size_t)size > kMaxFileSize)
		return false;

	std::string data((size_t)size, '\0');
	in.read(&data[0], size);

	std::string shortName = path;
	{
		size_t s = shortName.find_last_of("/\\");
		if (s != std::string::npos)
			shortName = shortName.substr(s + 1);
	}

	// --- Regexes (kept simple; run on comment-stripped single lines) ---
	static const std::regex reBlockHeader(R"rx(^\s*(?:local\s+([A-Za-z_]\w*)\s*=\s*function\b|[A-Za-z_]\w*\.([A-Za-z_]\w*)\s*<-\s*(.*)))rx");
	static const std::regex reCallRhs(R"rx(^\s*([A-Za-z_]\w*)\s*\()rx");
	static const std::regex reAliasRhs(R"rx(^\s*[A-Za-z_]\w*\.([A-Za-z_]\w*)\s*;?\s*$)rx");
	static const std::regex rePatNumVar(R"rx((?:local\s+)?([A-Za-z_]\w*)\s*=\s*[A-Za-z_][\w.]*GetPatternNum\s*\(.*\bpat\s*=\s*"([^"]+)")rx");
	static const std::regex reCaseStr(R"rx(\bcase\s+"([^"]*)"\s*:)rx");
	static const std::regex reCaseNum(R"rx(\bcase\s+(\d+)\s*:)rx");
	static const std::regex reSwitchParam(R"rx(\bswitch\s*\(\s*param\s*[.\[])rx");
	static const std::regex reSwitch(R"rx(\bswitch\s*\()rx");
	static const std::regex reCaseOther(R"rx(\b(?:case\s+[^":]+:|default\s*:))rx");
	static const std::regex reBreak(R"rx(\bbreak\b)rx");
	static const std::regex reFunction(R"rx(\bfunction\b)rx");
	static const std::regex reGetUpdateFrameId(R"rx(GetUpdateFrameID\s*\(\s*\)\s*==\s*(\d+))rx");
	static const std::regex reFrameIdParam(R"rx(\b(?:frameid|FrameID|CheckFrameID)\s*=\s*(\d+))rx");
	static const std::regex reBracketKey(R"rx(^\s*\[(\d+)\]\s*=)rx");
	static const std::regex reSpawnFieldUse(R"rx((?:CreateFireBall|CreateObject)\s*\(\s*[A-Za-z_]\w*\.([A-Za-z_]\w*))rx");
	static const std::regex reDataPattern(R"rx(\bDataPattern\s*==\s*(\d+))rx");
	static const std::regex rePatStr(R"rx(\bpat\s*=\s*"([^"]+)")rx");
	static const std::regex reStartPatStr(R"rx(\bstart_pat\s*=\s*"([^"]+)")rx");
	static const std::regex reStartPatVar(R"rx(\bstart_pat\s*=\s*([A-Za-z_][\w.]*))rx");
	static const std::regex reMvName(R"rx(\bmvname\s*=\s*"([^"]+)")rx");
	static const std::regex reMv(R"rx(\bmv\s*=\s*"([^"]+)")rx");
	static const std::regex reX(R"rx(\bx\s*=\s*(-?\d+)(?:\s*\*\s*(-?\d+))?)rx");
	static const std::regex reY(R"rx(\by\s*=\s*(-?\d+)(?:\s*\*\s*(-?\d+))?)rx");
	static const std::regex reThrowPattern(R"rx(\bpattern\s*=\s*(\d+))rx");
	static const std::regex reOffX(R"rx(\boffx\s*=\s*(-?\d+))rx");
	static const std::regex reOffY(R"rx(\boffy\s*=\s*(-?\d+))rx");
	static const std::regex reFieldLhs(R"rx(^\s*(?:local\s+)?[A-Za-z_]\w*\.([A-Za-z_]\w*)\s*=\s*(\{|$))rx");
	static const std::regex reFieldAssignVar(R"rx(([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*[,;]?\s*$)rx");
	static const std::regex reQuoted(R"rx("([^"]*)")rx");

	std::map<std::string, std::string> patNumVars; // pat_num_X -> code
	std::map<std::string, Block> factories;

	LineCleaner cleaner;
	int depth = 0;
	bool inBlock = false;
	int blockBaseDepth = 0;
	Block cur;
	std::string curCase;
	int curFrameId = -1;       // active GetUpdateFrameID gate ('case N:' etc.)
	bool caseChain = false;    // inside a contiguous 'case A: case B:' chain
	// True while inside 'switch( param.type )' — only those string case
	// labels select factory variants; labels of runtime switches (gacha item
	// picks etc.) must not filter spawns out at instantiation.
	bool switchIsParam = false;
	std::string pendingName;   // header seen, waiting for its '{' to open
	bool pendingIsFactory = false;
	int pendingBase = 0;
	std::string pendingField;  // multi-line 'mvParam.field = {' assignment scope
	int pendingFieldBase = 0;
	int pendingFieldLine = 0;
	bool pendingFieldOpened = false;

	// Expand deferred field uses, then filter+emit spawns of a closing block.
	auto expandBlock = [&](Block &b) {
		for (const auto &use : b.fieldUses) {
			auto it = b.patFields.find(use.field);
			if (it == b.patFields.end())
				continue;
			for (const auto &opt : it->second) {
				RawSpawn rs;
				rs.spawn.patternCode = opt.second;
				rs.spawn.offsetX = use.offsetX;
				rs.spawn.offsetY = use.offsetY;
				rs.spawn.frameIdRef = use.frameIdRef;
				rs.spawn.source = use.source;
				rs.caseLabel = opt.first.empty() ? use.caseLabel : opt.first;
				rs.fieldName = use.field;
				b.spawns.push_back(rs);
			}
		}
		b.fieldUses.clear();
	};

	auto emitMove = [&](const std::string &moveName, const Block &b,
	                    const std::set<std::string> *labels) {
		// labels == null: direct move block, take everything.
		// labels != null: factory instantiation; per field group prefer
		// label-matching options over unlabeled defaults.
		std::set<std::string> takenFields;
		if (labels) {
			for (const auto &rs : b.spawns) {
				if (!rs.fieldName.empty() && !rs.caseLabel.empty() &&
				    labels->count(rs.caseLabel))
					takenFields.insert(rs.fieldName);
			}
		}
		for (const auto &rs : b.spawns) {
			if (labels) {
				if (!rs.caseLabel.empty() && !labels->count(rs.caseLabel))
					continue;
				if (rs.caseLabel.empty() && !rs.fieldName.empty() &&
				    takenFields.count(rs.fieldName))
					continue; // overridden default
			}
			if (m_spawns.size() >= kMaxSpawns)
				return;
			MvScriptSpawn s = rs.spawn;
			s.ownerMove = moveName;
			m_spawns.push_back(std::move(s));
		}
		for (int p : b.ownerExtra)
			m_ownerExtra[moveName].push_back(p);
	};

	auto closeBlock = [&]() {
		expandBlock(cur);
		if (cur.isFactory)
			factories[cur.name] = cur;
		else
			emitMove(cur.name, cur, nullptr);
		cur = Block();
		inBlock = false;
		curCase.clear();
		curFrameId = -1;
		caseChain = false;
		switchIsParam = false;
		pendingField.clear();
		pendingFieldOpened = false;
	};

	// Apply a line's brace delta and handle deferred block open/close. A
	// header's own braces (e.g. 'function( param={} )') net to zero, so a
	// block only opens once depth actually rises above the header's depth.
	auto advanceDepth = [&](int net) {
		depth += net;
		if (!inBlock && !pendingName.empty() && depth > pendingBase) {
			cur = Block();
			cur.name = pendingName;
			cur.isFactory = pendingIsFactory;
			blockBaseDepth = pendingBase;
			inBlock = true;
			pendingName.clear();
		} else if (inBlock && depth <= blockBaseDepth) {
			closeBlock();
		}
	};

	std::istringstream stream(data);
	std::string rawLine;
	int lineNo = 0;
	std::smatch m;

	while (std::getline(stream, rawLine)) {
		lineNo++;
		if (lineNo > 200000)
			break;

		int net = 0;
		std::string code = cleaner.Clean(rawLine, net);

		// pat_num style pre-lookups can appear anywhere.
		if (code.find("GetPatternNum") != std::string::npos) {
			if (std::regex_search(code, m, rePatNumVar))
				patNumVars[m[1].str()] = m[2].str();
			advanceDepth(net);
			continue;
		}

		if (!inBlock) {
			if (pendingName.empty() && depth == 0 && std::regex_search(code, m, reBlockHeader)) {
				if (m[1].matched) {
					// local NAME = function ...
					pendingName = m[1].str();
					pendingIsFactory = true;
					pendingBase = depth;
				} else {
					std::string name = m[2].str();
					std::string rhs = m[3].str();
					std::string rhsTrim = Trim(rhs);
					std::smatch mr;
					if (rhsTrim.empty() || rhsTrim[0] == '{') {
						pendingName = name;
						pendingIsFactory = false;
						pendingBase = depth;
					} else if (std::regex_search(rhsTrim, mr, reCallRhs)) {
						// t.X <- factory( { type="B" } );
						auto fit = factories.find(mr[1].str());
						if (fit != factories.end()) {
							std::set<std::string> labels;
							auto qb = std::sregex_iterator(rhsTrim.begin(), rhsTrim.end(), reQuoted);
							for (auto qi = qb; qi != std::sregex_iterator(); ++qi)
								labels.insert((*qi)[1].str());
							emitMove(name, fit->second, &labels);
						}
					} else if (std::regex_search(rhsTrim, mr, reAliasRhs)) {
						// t.X <- t.Y;
						std::string from = mr[1].str();
						std::vector<MvScriptSpawn> copies;
						for (const auto &s : m_spawns)
							if (s.ownerMove == from)
								copies.push_back(s);
						for (auto &s : copies) {
							if (m_spawns.size() >= kMaxSpawns)
								break;
							s.ownerMove = name;
							m_spawns.push_back(std::move(s));
						}
					}
				}
			}
			advanceDepth(net);
			continue;
		}

		// ------- inside a block -------
		// Frame-ID given on this very line overrides the surrounding gate:
		// frameid=/FrameID=/CheckFrameID= params, or a '[N] = {...}' table key
		// (spawn tables are indexed by update frame ID; keys < 10 are treated
		// as plain array indexes, real frame IDs start at 50 in practice).
		int sameLineFrameId = -1;
		if (std::regex_search(code, m, reFrameIdParam)) {
			try { sameLineFrameId = std::stoi(m[1].str()); } catch (...) {}
		} else if (std::regex_search(code, m, reBracketKey)) {
			int key = -1;
			try { key = std::stoi(m[1].str()); } catch (...) {}
			if (key >= 10)
				sameLineFrameId = key;
		}

		// Track case labels / frame-ID gates. Blank lines don't disturb state.
		if (!Trim(code).empty()) {
			if (std::regex_search(code, m, reSwitch)) {
				switchIsParam = std::regex_search(code, m, reSwitchParam);
			}
			if (std::regex_search(code, m, reCaseStr)) {
				// 'case "B":' under switch(param.type) selects the factory
				// variant; string cases of runtime switches don't filter.
				curCase = switchIsParam ? m[1].str() : std::string();
				caseChain = false;
			} else if (std::regex_search(code, m, reCaseNum)) {
				// switch(GetUpdateFrameID()) gate; keep the first label of a
				// fall-through chain ('case 100: case 150:' fires at 100).
				curCase.clear();
				if (!(caseChain && curFrameId >= 0)) {
					try { curFrameId = std::stoi(m[1].str()); }
					catch (...) { curFrameId = -1; }
				}
				caseChain = true;
			} else if (std::regex_search(code, m, reCaseOther)) {
				curCase.clear();
				curFrameId = -1;
				caseChain = false;
			} else if (std::regex_search(code, m, reBreak)) {
				curCase.clear();
				curFrameId = -1;
				caseChain = false;
			} else {
				if (std::regex_search(code, m, reFunction)) {
					// Gates don't leak across function boundaries.
					curCase.clear();
					curFrameId = -1;
				}
				if (std::regex_search(code, m, reGetUpdateFrameId)) {
					try { curFrameId = std::stoi(m[1].str()); } catch (...) {}
				}
				caseChain = false;
			}
		}
		const int gateFrameId = sameLineFrameId >= 0 ? sameLineFrameId : curFrameId;

		if (std::regex_search(code, m, reDataPattern)) {
			try { cur.ownerExtra.push_back(std::stoi(m[1].str())); } catch (...) {}
		}

		// Firing site of a spawn table declared earlier in an mvParam field:
		// 'CreateFireBall( mvParam.ball )' under 'case 100:' stamps frame gate
		// 100 onto the spawns extracted from the 'mvParam.ball = {...}' line.
		if (gateFrameId >= 0 && std::regex_search(code, m, reSpawnFieldUse)) {
			auto sit = cur.spawnFields.find(m[1].str());
			if (sit != cur.spawnFields.end()) {
				for (size_t si : sit->second) {
					if (si < cur.spawns.size() && cur.spawns[si].spawn.frameIdRef < 0)
						cur.spawns[si].spawn.frameIdRef = gateFrameId;
				}
			}
		}

		// Extract spawn-ish constructs from this line.
		std::string patCode, mvName, fieldName;
		int px = 0, py = 0;
		bool haveSpawn = false;
		bool isImpact = false;
		int directPatternId = -1;

		if (std::regex_search(code, m, reFieldLhs)) {
			fieldName = m[1].str();
			// Start of a (possibly multi-line) 'mvParam.field = {' table:
			// spawns on the following lines belong to this field.
			pendingField = fieldName;
			pendingFieldBase = depth;
			pendingFieldLine = lineNo;
			pendingFieldOpened = false;
		} else if (!pendingField.empty() && pendingFieldOpened && depth > pendingFieldBase) {
			fieldName = pendingField;
		}

		// x/y are only trusted inside the same {...} literal as the key that
		// identified the spawn (sibling tables often hold velocities).
		auto coordsFrom = [&](const std::string &region) {
			std::smatch cm;
			if (std::regex_search(region, cm, reX))
				px = ParseCoord(cm[1].str(), cm[2].matched ? cm[2].str() : "");
			if (std::regex_search(region, cm, reY))
				py = ParseCoord(cm[1].str(), cm[2].matched ? cm[2].str() : "");
		};

		if (code.find("ThrowParam") != std::string::npos) {
			if (std::regex_search(code, m, reThrowPattern)) {
				coordsFrom(BraceRegion(code, (size_t)m.position(0)));
				try { directPatternId = std::stoi(m[1].str()); } catch (...) {}
				haveSpawn = (directPatternId >= 0);
			}
		} else if (code.find("SetImpactHitEffect") != std::string::npos) {
			patCode = "ImpactHitEffect";
			isImpact = true;
			if (std::regex_search(code, m, reOffX)) {
				try { px = std::stoi(m[1].str()); } catch (...) {}
			}
			if (std::regex_search(code, m, reOffY)) {
				try { py = std::stoi(m[1].str()); } catch (...) {}
			}
			haveSpawn = true;
		} else {
			size_t keyPos = std::string::npos;
			if (std::regex_search(code, m, rePatStr)) {
				patCode = m[1].str();
				keyPos = (size_t)m.position(0);
				haveSpawn = true;
			} else if (std::regex_search(code, m, reStartPatStr)) {
				patCode = m[1].str();
				keyPos = (size_t)m.position(0);
				haveSpawn = true;
			} else if (std::regex_search(code, m, reStartPatVar)) {
				std::string tok = m[1].str();
				size_t tokPos = (size_t)m.position(0);
				size_t d = tok.find('.');
				if (d == std::string::npos) {
					auto vit = patNumVars.find(tok);
					if (vit != patNumVars.end()) {
						patCode = vit->second;
						keyPos = tokPos;
						haveSpawn = true;
					}
				} else {
					// start_pat=mvParam.field — expand when the block closes.
					coordsFrom(BraceRegion(code, tokPos));
					FieldUse use;
					use.field = tok.substr(d + 1);
					use.caseLabel = curCase;
					use.offsetX = px;
					use.offsetY = py;
					use.frameIdRef = gateFrameId;
					use.source = shortName + ":" + std::to_string(lineNo) + "  " +
					             sj2utf8(Trim(rawLine)).substr(0, 160);
					cur.fieldUses.push_back(use);
					px = py = 0;
				}
			}
			std::smatch mm;
			if (std::regex_search(code, mm, reMvName)) {
				mvName = mm[1].str();
				if (keyPos == std::string::npos)
					keyPos = (size_t)mm.position(0);
				haveSpawn = true;
			} else if (std::regex_search(code, mm, reMv)) {
				mvName = mm[1].str();
				if (keyPos == std::string::npos)
					keyPos = (size_t)mm.position(0);
				haveSpawn = true;
			}
			if (haveSpawn && keyPos != std::string::npos)
				coordsFrom(BraceRegion(code, keyPos));
		}

		if (haveSpawn) {
			RawSpawn rs;
			rs.spawn.patternCode = patCode;
			rs.spawn.patternId = directPatternId;
			rs.spawn.mvName = mvName;
			rs.spawn.offsetX = px;
			rs.spawn.offsetY = py;
			rs.spawn.frameIdRef = gateFrameId;
			rs.spawn.isImpactEffect = isImpact;
			rs.spawn.source = shortName + ":" + std::to_string(lineNo) + "  " +
			                  sj2utf8(Trim(rawLine)).substr(0, 160);
			rs.caseLabel = curCase;
			rs.fieldName = fieldName;
			if (!fieldName.empty())
				cur.spawnFields[fieldName].push_back(cur.spawns.size());
			cur.spawns.push_back(rs);
		} else {
			// Record 'field = pat_num_XXX' style assignments for later
			// 'start_pat=mvParam.field' uses.
			auto fb = std::sregex_iterator(code.begin(), code.end(), reFieldAssignVar);
			for (auto fi = fb; fi != std::sregex_iterator(); ++fi) {
				auto vit = patNumVars.find((*fi)[2].str());
				if (vit != patNumVars.end())
					cur.patFields[(*fi)[1].str()].push_back({curCase, vit->second});
			}
		}

		advanceDepth(net);

		// Multi-line field-assignment scope bookkeeping (after depth update).
		if (!pendingField.empty()) {
			if (depth > pendingFieldBase)
				pendingFieldOpened = true;
			else if (pendingFieldOpened || lineNo > pendingFieldLine)
				pendingField.clear();
		}
	}

	if (inBlock)
		closeBlock();

	m_files.push_back(shortName);
	return true;
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

void MvScriptIndex::resolveCodeNames(const std::function<int(const std::string&)> &lookup)
{
	m_byPattern.clear();

	// "Mv_Skill_236B" -> try "Mv_Skill_236B", "Skill_236B", "236B" (strip up
	// to two leading segments); longest matching suffix wins.
	auto resolveMoveName = [&](const std::string &name) -> int {
		std::string s = name;
		for (int strip = 0; strip <= 2; strip++) {
			if (s.size() >= 2) {
				int id = lookup(s);
				if (id >= 0)
					return id;
			}
			size_t us = s.find('_');
			if (us == std::string::npos || us + 1 >= s.size())
				break;
			s = s.substr(us + 1);
		}
		return -1;
	};

	m_byTemplate.clear();

	// Block names referenced as a spawned object's move (mv=/mvname=): their
	// own spawns are grandchildren of the object, tracked separately so the
	// spawn tree can chain them under the object instead of flattening.
	std::set<std::string> templateNames;
	for (const auto &raw : m_spawns)
		if (!raw.mvName.empty())
			templateNames.insert(raw.mvName);

	auto addUnique = [](std::vector<MvScriptSpawn> &list, const MvScriptSpawn &s) {
		for (const auto &e : list) {
			if (e.patternId == s.patternId && e.patternCode == s.patternCode &&
			    e.mvName == s.mvName && e.offsetX == s.offsetX &&
			    e.offsetY == s.offsetY && e.frameIdRef == s.frameIdRef &&
			    e.isImpactEffect == s.isImpactEffect)
				return;
		}
		list.push_back(s);
	};

	for (const auto &raw : m_spawns) {
		MvScriptSpawn s = raw;

		// Resolve the spawn target.
		if (s.patternId < 0 && !s.isImpactEffect) {
			if (!s.patternCode.empty())
				s.patternId = lookup(s.patternCode);
			if (s.patternId < 0 && !s.mvName.empty()) {
				s.patternId = resolveMoveName(s.mvName);
				if (s.patternId >= 0 && s.patternCode.empty())
					s.patternCode = s.mvName;
			}
		}

		// Resolve the owning pattern(s) of the move block.
		std::set<int> owners;
		int own = resolveMoveName(s.ownerMove);
		if (own >= 0)
			owners.insert(own);
		auto eit = m_ownerExtra.find(s.ownerMove);
		if (eit != m_ownerExtra.end())
			for (int p : eit->second)
				if (p >= 0)
					owners.insert(p);

		for (int owner : owners) {
			// Skip self-spawns (a block resolving to its own pattern would
			// just re-render the main pattern on top of itself).
			if (s.patternId == owner)
				continue;
			addUnique(m_byPattern[owner], s);
		}

		// Also index by template block name for grandchild chaining.
		if (templateNames.count(s.ownerMove))
			addUnique(m_byTemplate[s.ownerMove], s);
	}
}

#ifndef MV_SCRIPT_STANDALONE
void MvScriptIndex::resolveCodeNames(FrameData *fd)
{
	if (!fd)
		return;
	// Build codeName -> pattern id map once (first occurrence wins).
	std::map<std::string, int> codes;
	int count = fd->get_sequence_count();
	for (int i = 0; i < count; i++) {
		Sequence *seq = fd->get_sequence(i);
		if (!seq || seq->codeName.empty())
			continue;
		codes.emplace(std::string(seq->codeName.c_str()), i);
	}
	resolveCodeNames([&codes](const std::string &code) -> int {
		auto it = codes.find(code);
		return it == codes.end() ? -1 : it->second;
	});
}
#endif

const std::vector<MvScriptSpawn> *MvScriptIndex::spawnsForPattern(int patternId) const
{
	auto it = m_byPattern.find(patternId);
	if (it == m_byPattern.end() || it->second.empty())
		return nullptr;
	return &it->second;
}

const std::vector<MvScriptSpawn> *MvScriptIndex::spawnsForTemplate(const std::string &mvName) const
{
	auto it = m_byTemplate.find(mvName);
	if (it == m_byTemplate.end() || it->second.empty())
		return nullptr;
	return &it->second;
}
