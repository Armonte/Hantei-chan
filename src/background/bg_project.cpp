// Stage project and line-preserving text files; see bg_project.h.
#include "bg_project.h"
#include "bg_info.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace bg {

uint64_t NextEditSeq() { static uint64_t s = 0; return ++s; }

namespace {

std::string Trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
	return s.substr(a, b - a);
}
std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
bool IEq(const std::string& a, const std::string& b) { return Lower(a) == Lower(b); }
std::string DirOf(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? std::string() : p.substr(0, s);
}
std::string BaseOf(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}
std::string Join(const std::string& a, const std::string& b) {
	if (a.empty()) return b;
	char sep = a.find('\\') != std::string::npos ? '\\' : '/';
	return (a.back() == '/' || a.back() == '\\') ? a + b : a + sep + b;
}
// "[Name]" -> Name, "" if the line is not a live section header
std::string HeaderName(const std::string& line) {
	std::string t = Trim(line);
	if (t.size() < 2 || t[0] != '[') return std::string();
	size_t e = t.find(']');
	return e == std::string::npos ? std::string() : t.substr(1, e - 1);
}
// numeric suffix of Prefix_NNN, -1 if none
int SecNum(const std::string& s) {
	size_t u = s.find_last_of('_');
	if (u == std::string::npos || u + 1 >= s.size()) return -1;
	for (size_t i = u + 1; i < s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return -1;
	return std::atoi(s.c_str() + u + 1);
}
std::string SecPrefix(const std::string& s) {
	size_t u = s.find_last_of('_');
	return u == std::string::npos ? s : s.substr(0, u);
}

} // namespace

// ---- TextIni ------------------------------------------------------------------

bool TextIni::Load(const std::string& p) {
	loaded = false; dirty = false; path = p;
	std::ifstream f(p, std::ios::binary);
	if (!f) { text.clear(); return false; }
	text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	saved = text;
	loaded = true;
	return true;
}

bool TextIni::LoadText(const std::string& raw) { text = raw; saved = raw; loaded = true; dirty = false; return true; }

bool TextIni::Save(const std::string& p) {
	std::string target = p.empty() ? path : p;
	std::string tmp = target + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary);
		if (!f) return false;
		f.write(text.data(), (std::streamsize)text.size());
		if (!f) return false;
	}
	std::remove(target.c_str());
	if (std::rename(tmp.c_str(), target.c_str()) != 0) return false;
	path = target;
	saved = text;
	dirty = false;
	loaded = true;
	return true;
}

std::string TextIni::eol() const { return text.find("\r\n") != std::string::npos || text.empty() ? "\r\n" : "\n"; }

std::vector<TextIni::Line> TextIni::Lines() const {
	std::vector<Line> out;
	size_t i = 0;
	while (i < text.size()) {
		size_t nl = text.find('\n', i);
		size_t e = nl == std::string::npos ? text.size() : nl;
		size_t ce = (e > i && text[e - 1] == '\r') ? e - 1 : e;
		out.push_back({i, ce});
		if (nl == std::string::npos) break;
		i = nl + 1;
	}
	return out;
}

bool TextIni::SectionRange(const std::vector<Line>& ls, const std::string& sec, size_t& first, size_t& last) const {
	for (size_t i = 0; i < ls.size(); ++i) {
		std::string h = HeaderName(LineText(ls[i]));
		if (h.empty() || !IEq(h, sec)) continue;
		first = i + 1;
		last = first;
		while (last < ls.size() && HeaderName(LineText(ls[last])).empty()) ++last;
		return true;
	}
	return false;
}

std::vector<std::string> TextIni::Sections() const {
	std::vector<std::string> out;
	for (const Line& l : Lines()) { std::string h = HeaderName(LineText(l)); if (!h.empty()) out.push_back(h); }
	return out;
}

bool TextIni::HasSection(const std::string& sec) const {
	auto ls = Lines();
	size_t a, b;
	return SectionRange(ls, sec, a, b);
}

namespace {
// Locate key/value within one line. valBegin/valEnd are offsets in the line.
bool ParseKV(const std::string& line, std::string& key, size_t& valBegin, size_t& valEnd) {
	std::string t = Trim(line);
	if (t.empty() || t[0] == '[' || (t.size() >= 2 && t[0] == '/' && t[1] == '/')) return false;
	size_t eq = line.find('=');
	if (eq == std::string::npos) return false;
	size_t cm = line.find("//");
	if (cm != std::string::npos && cm < eq) return false;
	key = Trim(line.substr(0, eq));
	size_t vb = eq + 1;
	while (vb < line.size() && (line[vb] == ' ' || line[vb] == '\t')) ++vb;
	size_t ve = cm != std::string::npos && cm >= vb ? cm : line.size();
	while (ve > vb && (line[ve - 1] == ' ' || line[ve - 1] == '\t')) --ve;
	valBegin = vb; valEnd = ve;
	return true;
}
} // namespace

bool TextIni::Get(const std::string& sec, const std::string& key, std::string& value) const {
	auto ls = Lines();
	size_t a = 0, b = ls.size();
	if (!sec.empty() && !SectionRange(ls, sec, a, b)) return false;
	for (size_t i = a; i < b; ++i) {
		std::string line = LineText(ls[i]), k;
		size_t vb, ve;
		if (ParseKV(line, k, vb, ve) && IEq(k, key)) { value = line.substr(vb, ve - vb); return true; }
	}
	return false;
}

std::string TextIni::GetOr(const std::string& sec, const std::string& key, const std::string& def) const {
	std::string v;
	return Get(sec, key, v) ? v : def;
}

void TextIni::Set(const std::string& sec, const std::string& key, const std::string& value) {
	auto ls = Lines();
	size_t a = 0, b = ls.size();
	if (!sec.empty() && !SectionRange(ls, sec, a, b)) {
		AddSection(sec, {{key, value}});
		return;
	}
	size_t lastKey = sec.empty() ? (ls.empty() ? 0 : ls.size() - 1) : a - 1;   // header line when no keys
	for (size_t i = a; i < b; ++i) {
		std::string line = LineText(ls[i]), k;
		size_t vb, ve;
		if (!ParseKV(line, k, vb, ve)) continue;
		lastKey = i;
		if (!IEq(k, key)) continue;
		if (line.substr(vb, ve - vb) == value) return;
		std::string repl = value;
		// "Key = // comment": keep a space before the comment
		if (vb == ve && vb < line.size()) repl += " ";
		text.replace(ls[i].begin + vb, ve - vb, repl);
		dirty = true;
		return;
	}
	std::string ins = key + " = " + value;
	if (ls.empty()) { text = ins + eol(); dirty = true; return; }
	size_t at = ls[lastKey].end;
	text.insert(at, eol() + ins);
	dirty = true;
}

void TextIni::RemoveKey(const std::string& sec, const std::string& key) {
	auto ls = Lines();
	size_t a = 0, b = ls.size();
	if (!sec.empty() && !SectionRange(ls, sec, a, b)) return;
	for (size_t i = a; i < b; ++i) {
		std::string line = LineText(ls[i]), k;
		size_t vb, ve;
		if (!ParseKV(line, k, vb, ve) || !IEq(k, key)) continue;
		size_t end = i + 1 < ls.size() ? ls[i + 1].begin : text.size();
		text.erase(ls[i].begin, end - ls[i].begin);
		dirty = true;
		return;
	}
}

void TextIni::AddSection(const std::string& sec, const std::vector<std::pair<std::string, std::string>>& keys) {
	if (HasSection(sec)) { for (auto& kv : keys) Set(sec, kv.first, kv.second); return; }
	const std::string e = eol();
	std::string block = "[" + sec + "]" + e;
	for (auto& kv : keys) block += kv.first + " = " + kv.second + e;
	block += e;
	auto ls = Lines();
	const int num = SecNum(sec);
	const std::string pre = Lower(SecPrefix(sec));
	for (size_t i = 0; i < ls.size(); ++i) {
		std::string h = HeaderName(LineText(ls[i]));
		if (h.empty() || num < 0) continue;
		if (Lower(SecPrefix(h)) == pre && SecNum(h) > num) {
			text.insert(ls[i].begin, block);
			dirty = true;
			return;
		}
	}
	if (!text.empty() && text.back() != '\n') text += e;
	if (!text.empty() && text.size() >= e.size() * 2 && text.compare(text.size() - e.size() * 2, e.size() * 2, e + e) != 0)
		text += e;
	text += block;
	dirty = true;
}

void TextIni::RemoveSection(const std::string& sec) {
	auto ls = Lines();
	size_t a, b;
	if (!SectionRange(ls, sec, a, b)) return;
	size_t from = ls[a - 1].begin;
	size_t to = b < ls.size() ? ls[b].begin : text.size();
	text.erase(from, to - from);
	dirty = true;
}

void TextIni::RenameSection(const std::string& from, const std::string& to) {
	auto ls = Lines();
	for (const Line& l : ls) {
		std::string line = LineText(l);
		std::string h = HeaderName(line);
		if (h.empty() || !IEq(h, from)) continue;
		size_t p = line.find('[');
		text.replace(l.begin + p + 1, h.size(), to);
		dirty = true;
		return;
	}
}

// ---- StageProject -------------------------------------------------------------

namespace {
const int kExclTraining[] = {55, 57, 58, 99};
const int kExclOther[] = {3, 4, 18, 20, 21, 23, 31, 32, 43, 44, 47, 49, 50, 51, 52, 54, 55, 57, 58, 99};
// mbacPC g_StageTable 0x491050: select order, -1 = gap, {id, enabled}
const int kMbacTable[][2] = {
	{0, 1}, {13, 1}, {1, 1}, {6, 1}, {7, 1}, {2, 1}, {3, 1}, {5, 1}, {25, 1}, {9, 1}, {24, 1},
	{27, 1}, {21, 1}, {16, 1}, {17, 1}, {10, 1}, {8, 1}, {19, 1}, {18, 1}, {20, 1}, {26, 1}, {22, 1},
	{23, 1}, {31, 1}, {28, 1}, {29, 1}, {30, 1}, {32, 0}, {33, 0},
};
}

bool StageProject::ExcludedTraining(int id) { return std::find(std::begin(kExclTraining), std::end(kExclTraining), id) != std::end(kExclTraining); }
bool StageProject::ExcludedOther(int id) { return std::find(std::begin(kExclOther), std::end(kExclOther), id) != std::end(kExclOther); }

bool StageProject::Open(const std::string& pathInGame, Game g) {
	*this = StageProject();
	game = g;
	std::string p = pathInGame;
	while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
	std::string lowerBase = Lower(BaseOf(p));
	bool isFile = lowerBase.size() > 4 && (lowerBase.rfind(".dat") == lowerBase.size() - 4 || lowerBase.rfind(".ini") == lowerBase.size() - 4);
	std::string dir = isFile ? DirOf(p) : p;
	if (game == Game::MBAACC) {
		std::string sub = FindFileNoCase(dir, "bg");
		if (!isFile && !sub.empty() && !FindFileNoCase(sub, "BgList.ini").empty()) { gameDir = dir; bgDir = sub; }
		else { bgDir = dir; gameDir = Lower(BaseOf(dir)) == "bg" ? DirOf(dir) : dir; }
		std::string ini = FindFileNoCase(bgDir, "BgList.ini");
		if (ini.empty()) return false;
		bgList.Load(ini);
		std::string bgmDir = FindFileNoCase(gameDir, "Bgm");
		std::string bgmTxt = bgmDir.empty() ? std::string() : FindFileNoCase(bgmDir, "bgm.txt");
		if (!bgmTxt.empty()) bgm.Load(bgmTxt);
	} else {
		bgDir = dir;
		gameDir = DirOf(dir);
		// Dumps keep BGM.TXT in another archive folder (dump/03); look in siblings.
		std::string bgmTxt = FindFileNoCase(dir, "bgm.txt");
		if (bgmTxt.empty() && !gameDir.empty()) {
			for (const char* s : {"03", "00", "01", "02", "04", "10"}) {
				std::string d = FindFileNoCase(gameDir, s);
				if (!d.empty()) bgmTxt = FindFileNoCase(d, "bgm.txt");
				if (!bgmTxt.empty()) break;
			}
		}
		if (!bgmTxt.empty()) bgm.Load(bgmTxt);
	}
	open = true;
	Rebuild();
	return true;
}

void StageProject::Rebuild() {
	entries.clear();
	auto fillBgm = [&](StageEntry& e) {
		char sec[16];
		snprintf(sec, sizeof(sec), "BGM_%03d", e.id);
		if (!bgm.IsLoaded() || !bgm.HasSection(sec)) return;
		e.hasBgm = true;
		e.bgmFile = bgm.GetOr(sec, "File", game == Game::MBAC ? std::to_string(e.id) : std::string());
		e.bgmLoop = std::atoi(bgm.GetOr(sec, "IsLoop", "0").c_str());
		e.bgmLoopPos = bgm.GetOr(sec, "LoopPos", "0");
		// the trailing comment of File = ... names the track and stage
		const std::string& t = bgm.Text();
		std::string needle = "[" + std::string(sec) + "]";
		size_t at = t.find(needle);
		if (at != std::string::npos) {
			size_t f = t.find("File", at);
			size_t next = t.find('[', at + 1);
			if (f != std::string::npos && (next == std::string::npos || f < next)) {
				size_t eol = t.find('\n', f);
				size_t c = t.find("//", f);
				if (c != std::string::npos && (eol == std::string::npos || c < eol))
					e.bgmComment = Trim(t.substr(c + 2, (eol == std::string::npos ? t.size() : eol) - c - 2));
				while (!e.bgmComment.empty() && e.bgmComment.back() == '\r') e.bgmComment.pop_back();
			}
		}
	};
	if (game == Game::MBAACC) {
		std::string grp = FindFileNoCase(gameDir, "GRP");
		std::string sel = grp.empty() ? std::string() : FindFileNoCase(grp, "BgSelect");
		auto img = [&](const char* sub, const char* pfx, int id) {
			if (sel.empty()) return std::string();
			std::string d = FindFileNoCase(sel, sub);
			if (d.empty()) return std::string();
			char n[64];
			snprintf(n, sizeof(n), "%s%02d.dds", pfx, id);
			return FindFileNoCase(d, n);
		};
		for (int id = 0; id <= 99; ++id) {
			char sec[16];
			snprintf(sec, sizeof(sec), "Bg_%03d", id);
			StageEntry e;
			e.id = id;
			e.order = id;
			e.listed = bgList.HasSection(sec);
			if (e.listed) {
				e.dataFile = bgList.GetOr(sec, "DataFile", "");
				e.infoFile = std::atoi(bgList.GetOr(sec, "InfoFile", "0").c_str());
				e.selectable = std::atoi(bgList.GetOr(sec, "IsSelectAble", "0").c_str());
				e.giant = std::atoi(bgList.GetOr(sec, "IsGiantStage", "0").c_str());
				e.colorVal = (float)std::atof(bgList.GetOr(sec, "StageColorVal", "0").c_str());
			} else {
				char dn[16];
				snprintf(dn, sizeof(dn), "bg%02d", id);
				if (FindFileNoCase(bgDir, std::string(dn) + ".dat").empty()) continue;
				e.dataFile = dn;   // a stage file with no BgList entry (unreachable in game)
			}
			if (!e.dataFile.empty()) e.datPath = FindFileNoCase(bgDir, e.dataFile + ".dat");
			e.excludedTraining = ExcludedTraining(id);
			e.excludedOther = ExcludedOther(id);
			fillBgm(e);
			e.previewPath = img("stsel_view", "chr_stsel_view", id);
			e.nameEnPath = img("stsel_en", "chr_stsel_en", id);
			e.nameJpPath = img("stsel_jp", "chr_stsel_jp", id);
			entries.push_back(e);
		}
	} else {
		int order = 0;
		for (const auto& t : kMbacTable) {
			StageEntry e;
			e.id = t[0];
			e.order = order++;
			e.listed = t[1] != 0;
			e.selectable = t[1];
			char dn[16];
			snprintf(dn, sizeof(dn), "BG%02d", e.id);
			e.dataFile = dn;
			e.datPath = FindFileNoCase(bgDir, std::string(dn) + ".DAT");
			fillBgm(e);
			entries.push_back(e);
		}
	}
}

const StageEntry* StageProject::Find(int id) const {
	for (const auto& e : entries) if (e.id == id) return &e;
	return nullptr;
}

const StageEntry* StageProject::FindByDat(const std::string& datPath) const {
	std::string b = Lower(BaseOf(datPath));
	for (const auto& e : entries) if (!e.datPath.empty() && Lower(BaseOf(e.datPath)) == b) return &e;
	return nullptr;
}

const StageEntry* StageProject::Step(int fromId, int dir) const {
	if (entries.empty()) return nullptr;
	int n = (int)entries.size(), cur = -1;
	for (int i = 0; i < n; ++i) if (entries[i].id == fromId) { cur = i; break; }
	for (int k = 1; k <= n; ++k) {
		int i = ((cur < 0 ? (dir > 0 ? -1 : 0) : cur) + dir * k % n + n * 2) % n;
		if (!entries[i].datPath.empty()) return &entries[i];
	}
	return nullptr;
}

bool StageProject::SaveAll() {
	bool ok = true;
	if (bgList.IsDirty()) ok &= bgList.Save(bgList.Path());
	if (bgm.IsDirty()) ok &= bgm.Save(bgm.Path());
	return ok;
}

void StageProject::Commit(const std::string& label) {
	pending.label = label;
	pending.seq = NextEditSeq();
	undoStack.push_back(pending);
	if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
	redoStack.clear();
	Rebuild();
}

void StageProject::SetListValue(int id, const std::string& key, const std::string& value) {
	if (game != Game::MBAACC) return;
	pending = Snap{bgList.Text(), bgm.Text(), "", 0};
	char sec[16];
	snprintf(sec, sizeof(sec), "Bg_%03d", id);
	std::string before = bgList.Text();
	if (value.empty()) bgList.RemoveKey(sec, key); else bgList.Set(sec, key, value);
	if (bgList.Text() != before) Commit("BgList [" + std::string(sec) + "] " + key);
}

void StageProject::SetBgmValue(int id, const std::string& key, const std::string& value) {
	if (!bgm.IsLoaded()) return;
	pending = Snap{bgList.Text(), bgm.Text(), "", 0};
	char sec[16];
	snprintf(sec, sizeof(sec), "BGM_%03d", id);
	std::string before = bgm.Text();
	bgm.Set(sec, key, value);
	if (bgm.Text() != before) Commit("bgm.txt [" + std::string(sec) + "] " + key);
}

void StageProject::AddStage(int id, const std::string& dataFile) {
	if (game != Game::MBAACC) return;
	pending = Snap{bgList.Text(), bgm.Text(), "", 0};
	char sec[16];
	snprintf(sec, sizeof(sec), "Bg_%03d", id);
	if (bgList.HasSection(sec)) return;
	bgList.AddSection(sec, {{"DataFile", dataFile}, {"IsSelectAble", "1"}});
	Commit("add stage " + std::to_string(id));
}

void StageProject::RemoveStage(int id) {
	if (game != Game::MBAACC) return;
	pending = Snap{bgList.Text(), bgm.Text(), "", 0};
	char sec[16];
	snprintf(sec, sizeof(sec), "Bg_%03d", id);
	if (!bgList.HasSection(sec)) return;
	bgList.RemoveSection(sec);
	Commit("remove stage " + std::to_string(id));
}

void StageProject::MoveStage(int fromId, int toId) {
	if (game != Game::MBAACC || fromId == toId || toId < 1 || toId > 99) return;
	char a[16], b[16], ba[16], bb[16];
	snprintf(a, sizeof(a), "Bg_%03d", fromId); snprintf(b, sizeof(b), "Bg_%03d", toId);
	snprintf(ba, sizeof(ba), "BGM_%03d", fromId); snprintf(bb, sizeof(bb), "BGM_%03d", toId);
	if (!bgList.HasSection(a) || bgList.HasSection(b)) return;
	pending = Snap{bgList.Text(), bgm.Text(), "", 0};
	// Re-add under the new number so the file stays in id order.
	std::vector<std::pair<std::string, std::string>> keys;
	for (const char* k : {"DataFile", "InfoFile", "IsSelectAble", "IsGiantStage", "StageColorVal"}) {
		std::string v;
		if (bgList.Get(a, k, v)) keys.push_back({k, v});
	}
	bgList.RemoveSection(a);
	bgList.AddSection(b, keys);
	Commit("move stage " + std::to_string(fromId) + " -> " + std::to_string(toId));
}

bool StageProject::Undo() {
	if (undoStack.empty()) return false;
	Snap s = undoStack.back();
	undoStack.pop_back();
	redoStack.push_back(Snap{bgList.Text(), bgm.Text(), s.label, s.seq});
	bgList.SetText(s.bgList);
	bgm.SetText(s.bgm);
	Rebuild();
	return true;
}

bool StageProject::Redo() {
	if (redoStack.empty()) return false;
	Snap s = redoStack.back();
	redoStack.pop_back();
	undoStack.push_back(Snap{bgList.Text(), bgm.Text(), s.label, s.seq});
	bgList.SetText(s.bgList);
	bgm.SetText(s.bgm);
	Rebuild();
	return true;
}

const std::string& StageProject::UndoLabel() const { static std::string e; return undoStack.empty() ? e : undoStack.back().label; }
const std::string& StageProject::RedoLabel() const { static std::string e; return redoStack.empty() ? e : redoStack.back().label; }

} // namespace bg
