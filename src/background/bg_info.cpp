#include "bg_info.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace bg {

namespace {

std::string Lower(std::string s) {
	for (char& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

bool ReadText(const std::string& path, std::string& out) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

std::string StripComment(const std::string& line) {
	size_t c = line.find("//");
	std::string s = c == std::string::npos ? line : line.substr(0, c);
	while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
	size_t b = 0;
	while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;
	return s.substr(b);
}

// "Key = value" -> (key, first value token). The game's ResourceParser hands
// the value to atol/atof/strncpy, so only the first token matters.
bool SplitKeyValue(const std::string& line, std::string& key, std::string& val) {
	size_t eq = line.find('=');
	if (eq == std::string::npos) return false;
	key = StripComment(line.substr(0, eq));
	std::string v = StripComment(line.substr(eq + 1));
	size_t e = 0;
	while (e < v.size() && !std::isspace((unsigned char)v[e])) ++e;
	val = v.substr(0, e);
	return !key.empty();
}

// All key/value pairs, in file order, tagged with their section.
struct KV { std::string section, key, val; };

std::vector<KV> ParseIni(const std::string& text) {
	std::vector<KV> out;
	std::istringstream in(text);
	std::string line, section;
	while (std::getline(in, line)) {
		std::string s = StripComment(line);
		if (s.empty()) continue;
		if (s.front() == '[') {
			size_t e = s.find(']');
			section = e == std::string::npos ? s.substr(1) : s.substr(1, e - 1);
			continue;
		}
		KV kv;
		if (SplitKeyValue(s, kv.key, kv.val)) { kv.section = section; out.push_back(kv); }
	}
	return out;
}

const KV* FindKey(const std::vector<KV>& kvs, const std::string& key, const std::string* section = nullptr) {
	std::string k = Lower(key);
	for (const KV& kv : kvs) {
		if (section && Lower(kv.section) != Lower(*section)) continue;
		if (Lower(kv.key) == k) return &kv;   // first occurrence (INF)
	}
	return nullptr;
}

int IntOr(const std::vector<KV>& kvs, const std::string& key, int def, const std::string* sec = nullptr) {
	const KV* kv = FindKey(kvs, key, sec);
	return kv ? (int)std::atol(kv->val.c_str()) : def;
}

} // namespace

std::string FindFileNoCase(const std::string& dir, const std::string& name) {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::path d = dir.empty() ? fs::path(".") : fs::path(dir);
	fs::path direct = d / name;
	if (fs::exists(direct, ec)) return direct.string();
	std::string want = Lower(name);
	for (auto it = fs::directory_iterator(d, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
		if (Lower(it->path().filename().string()) == want) return it->path().string();
	}
	return std::string();
}

// ---- BgList.ini -------------------------------------------------------------

bool StageList::Load(const std::string& iniPath) {
	entries.clear();
	path.clear();
	std::string text;
	if (!ReadText(iniPath, text)) return false;
	path = iniPath;
	std::vector<KV> kvs = ParseIni(text);
	// StageSelect_LoadStageData walks Bg_001..Bg_099 (slot 0 is a built-in
	// "random" entry). A section whose DataFile .dat is missing is skipped.
	for (int i = 1; i < 100; ++i) {
		char sec[16];
		snprintf(sec, sizeof(sec), "Bg_%03d", i);
		std::string s = sec;
		const KV* df = FindKey(kvs, "DataFile", &s);
		if (!df) continue;
		StageListEntry e;
		e.index = i;
		e.dataFile = df->val.substr(0, 32);
		e.infoFile = IntOr(kvs, "InfoFile", 0, &s);
		e.isSelectable = IntOr(kvs, "IsSelectAble", 0, &s);
		e.isGiantStage = IntOr(kvs, "IsGiantStage", 0, &s);
		const KV* cv = FindKey(kvs, "StageColorVal", &s);
		e.stageColorVal = cv ? (float)std::atof(cv->val.c_str()) : 0.0f;
		entries.push_back(e);
	}
	return true;
}

const StageListEntry* StageList::FindByDataFile(const std::string& name) const {
	std::string n = Lower(name);
	if (n.size() > 4 && n.compare(n.size() - 4, 4, ".dat") == 0) n.resize(n.size() - 4);
	for (const auto& e : entries)
		if (Lower(e.dataFile) == n) return &e;
	return nullptr;
}

// ---- bgNNInfo.txt -------------------------------------------------------------

bool StageInfo::Load(const std::string& txtPath) {
	*this = StageInfo();
	std::string text;
	if (!ReadText(txtPath, text)) return false;
	return LoadFromText(text, txtPath);
}

bool StageInfo::LoadFromText(const std::string& text, const std::string& txtPath) {
	*this = StageInfo();
	path = txtPath;
	loaded = true;
	std::vector<KV> kvs = ParseIni(text);   // the game ignores [Data]; keys are global

	int n = IntOr(kvs, "LightNum", 0);
	n = std::max(0, std::min(n, 10));       // g_BgLightPos/Power hold 10 entries
	for (int i = 0; i < n; ++i) {
		char k[32];
		StageLight l;
		snprintf(k, sizeof(k), "Light%02dPos", i);   l.pos = IntOr(kvs, k, 0);
		snprintf(k, sizeof(k), "Light%02dPower", i); l.power = IntOr(kvs, k, 0);
		lights.push_back(l);
	}

	count = 100;
	dropObj = IntOr(kvs, "DropObj", 0);
	if (dropObj) {
		dropType = IntOr(kvs, "DropObj_Type", -1);
		if (dropType == 1) {
			count = IntOr(kvs, "DropObj_Max", 100);
			alpha = IntOr(kvs, "DropObj_Alpha", 100);
			h     = IntOr(kvs, "DropObj_H", 150);
			wait  = IntOr(kvs, "DropObj_Wait", 50);
		} else if (dropType == 0) {
			const KV* f = FindKey(kvs, "DropObjFile");
			dropFile = f ? f->val.substr(0, 32) : std::string();
			patNum   = IntOr(kvs, "DropObj_PatNum", 0);
			frameNum = IntOr(kvs, "DropObj_FrameNum", 0);
			wait     = IntOr(kvs, "DropObj_Wait", 0);
			w        = IntOr(kvs, "DropObj_W", 0);
			h        = IntOr(kvs, "DropObj_H", 0);
		}
	}
	// The particle array (0x766008) has room for exactly 100 entries.
	count = std::max(0, std::min(count, 100));
	return true;
}

// ---- bgNNlight.txt (MBAC) -------------------------------------------------------

bool LightFile::Load(const std::string& txtPath) {
	*this = LightFile();
	std::string text;
	if (!ReadText(txtPath, text)) return false;
	path = txtPath;
	loaded = true;
	std::istringstream in(text);
	std::string line;
	int n = 0;
	if (std::getline(in, line)) n = (int)std::atol(line.c_str());
	n = std::max(0, std::min(n, 10));
	for (int i = 0; i < n && std::getline(in, line); ++i) {
		StageLight l;
		if (sscanf(line.c_str(), "%d,%d", &l.pos, &l.power) < 1) { --i; continue; }
		lights.push_back(l);
	}
	return true;
}

// ---- weather particles ------------------------------------------------------------

void DropSystem::Init(const StageInfo& info, Rng& rng) {
	cfg = info;
	type = info.dropType;
	active = info.loaded && info.dropObj != 0;
	particles.assign(active ? (size_t)info.count : 0, DropParticle());
	if (!active) return;
	if (type == 1) {
		// Rain: streaks start anywhere in x [-512, 612), y [-1024, 0], heading
		// straight down (angle ~pi) at Wait .. Wait*1.2 px/tick.
		const float wait = (float)info.wait;
		for (DropParticle& p : particles) {
			p = DropParticle();
			float rx = (float)(rng.Float01() * 1124.0);
			p.x = rx - 512.0f;
			float ry = (float)(rng.Float01() * 1024.0);
			p.y = -ry;
			p.pat = 0;
			float a = (float)(rng.Float01() * 0.004999999888241291);
			float ang = a + 0.5f;
			float spread = wait / 5.0f;
			float sp = (float)(rng.Float01() * spread) + wait;
			long double th = (long double)ang * 3.14159265L + (long double)ang * 3.14159265L;
			p.vx = (float)(std::sin((double)th) * sp);
			p.alpha = 255;
			p.vy = (float)(-(std::cos((double)th) * sp));
			p.phase = (int32_t)(rng.Float01() * 256.0);
		}
	} else if (type == 0) {
		// Bitmap particles (sakura petals): drift down at 1..3 px/tick.
		for (DropParticle& p : particles) {
			p = DropParticle();
			float rx = (float)(rng.Float01() * 1024.0);
			p.x = rx - 512.0f;
			float ry = (float)(rng.Float01() * 1024.0);
			p.y = -ry;
			int32_t r = rng.Next();
			p.pat = info.patNum > 0 ? r % info.patNum : 0;   // game divides by 0 if PatNum = 0
			p.vy = (float)(rng.Float01() * 2.0) + 1.0f;
			p.vx = (float)rng.Float01() - 0.5f;
			p.alpha = 255;
			p.phase = (int32_t)(rng.Float01() * 256.0);
		}
	}
}

void DropSystem::Update(Rng& rng) {
	if (!active) return;
	const int wait = cfg.wait;
	for (DropParticle& p : particles) {
		p.x += p.vx; p.vx += p.ax;
		p.y += p.vy; p.vy += p.ay;
		if (type == 1) {
			if (p.y > 64.0f) {
				p.y = -1024.0f;
				float rx = (float)(rng.Float01() * 1124.0);
				p.x = rx - 512.0f;
				p.phase = (int32_t)(rng.Float01() * 256.0);
			}
		} else if (type == 0) {
			if (p.y > 0.0f) p.alpha -= 16;      // fade out below the floor line
			if (p.alpha < 0) {
				p.y = -1024.0f;
				float rx = (float)(rng.Float01() * 1024.0);
				p.alpha = 255;
				p.x = rx - 512.0f;
				int32_t r = rng.Next();
				p.pat = cfg.patNum > 0 ? r % cfg.patNum : 0;
				p.phase = (int32_t)(rng.Float01() * 256.0);
			}
		}
		p.waitCtr += 1.0f;
		if (p.waitCtr >= (float)wait) {
			p.frame += 1.0f;
			if (p.frame >= (float)cfg.frameNum) p.frame = 0.0f;
			p.waitCtr = 0.0f;
		}
	}
}

void DropSystem::ImportRaw(const StageInfo& info, const uint8_t* raw, size_t n) {
	cfg = info;
	type = info.dropType;
	active = info.loaded && info.dropObj != 0;
	particles.assign(active ? (size_t)info.count : 0, DropParticle());
	for (size_t i = 0; i < particles.size() && (i + 1) * 44 <= n; ++i) {
		const uint8_t* r = raw + i * 44;
		DropParticle& p = particles[i];
		std::memcpy(&p.x, r + 0, 4);  std::memcpy(&p.y, r + 4, 4);
		std::memcpy(&p.pat, r + 8, 4); std::memcpy(&p.frame, r + 12, 4);
		std::memcpy(&p.waitCtr, r + 16, 4); std::memcpy(&p.alpha, r + 20, 4);
		std::memcpy(&p.phase, r + 24, 4);
		std::memcpy(&p.vx, r + 28, 4); std::memcpy(&p.vy, r + 32, 4);
		std::memcpy(&p.ax, r + 36, 4); std::memcpy(&p.ay, r + 40, 4);
	}
}

} // namespace bg
