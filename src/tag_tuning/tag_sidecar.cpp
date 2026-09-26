// [authoring] TAG tuning sidecar tree — see tag_sidecar.h and docs/HANTEI_AUTHORING_MODE.md §9.1.
#include "tag_sidecar.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <set>

namespace tagtune {

namespace fs = std::filesystem;

// ================== ids and names ==================

bool DocId::operator<(const DocId& o) const
{
	if (layer != o.layer) return layer < o.layer;
	if (kind != o.kind) return kind < o.kind;
	if (file != o.file) return file < o.file;
	return moon < o.moon;
}

std::string DocId::RelPath() const
{
	switch (kind) {
	case DocKind::Global: return "global.ini";
	case DocKind::Hud: return "hud.ini";
	case DocKind::CharShared: return "chars\\" + file + ".ini";
	case DocKind::CharMoon: return "chars\\" + file + "_" + MoonName(moon) + ".ini";
	}
	return {};
}

std::string DocId::Label() const { return std::string(layer == Layer::Local ? "local " : "shipped ") + RelPath(); }

DocId GlobalDoc(Layer l) { DocId d; d.layer = l; d.kind = DocKind::Global; return d; }
DocId HudDoc(Layer l) { DocId d; d.layer = l; d.kind = DocKind::Hud; return d; }
DocId CharDoc(Layer l, const std::string& file, int moon)
{
	DocId d;
	d.layer = l;
	d.kind = moon < 0 ? DocKind::CharShared : DocKind::CharMoon;
	d.file = file;
	for (char& c : d.file) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	d.moon = moon < 0 ? -1 : moon;
	return d;
}

bool ValidCharFileName(const std::string& f)
{
	if (f.empty() || f.size() > 27) return false;
	for (char c : f)
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
	return true;
}

bool ParseCharFileName(const std::string& leafIn, std::string& file, int& moon)
{
	std::string leaf = leafIn;
	for (char& c : leaf) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	if (leaf.size() < 5 || leaf.compare(leaf.size() - 4, 4, ".ini") != 0) return false;
	std::string stem = leaf.substr(0, leaf.size() - 4);
	moon = -1;
	for (int m = 0; m < kMoonCount; ++m) {
		const std::string suf = std::string("_") + MoonName(m);
		if (stem.size() > suf.size() && stem.compare(stem.size() - suf.size(), suf.size(), suf) == 0) {
			stem.resize(stem.size() - suf.size());
			moon = m;
			break;
		}
	}
	if (!ValidCharFileName(stem)) return false;
	file = stem;
	return true;
}

const char* SrcName(Src s)
{
	switch (s) {
	case Src::Default: return "default";
	case Src::Style: return "style";
	case Src::Tuning: return "global";
	case Src::Env: return "env";
	case Src::Char: return "char";
	case Src::Moon: return "moon";
	case Src::Css: return "CSS";
	}
	return "?";
}

// ================== resolution ==================

namespace {

const char* LayerWord(int l) { return l == (int)Layer::Local ? "local " : "shipped "; }

std::string CharLabel(int layer, const std::string& file, int moon)
{
	return std::string(LayerWord(layer)) + "chars\\" + file + (moon >= 0 ? std::string("_") + MoonName(moon) : std::string()) + ".ini";
}

// ApplyKvs one pair at a time, recording which levers each pair really set.
void ApplyTracked(Values& v, std::array<Provenance, kLeverCount>* from, Provenance p, const std::vector<KeyValue>& kvs,
                  const std::string& where, std::vector<Warning>& warn, bool perChar)
{
	for (const KeyValue& kv : kvs) {
		const size_t before = warn.size();
		ApplyKvs(v, { kv }, where, warn, perChar);
		const int li = FindLever(kv.key);
		if (warn.size() == before && li >= 0 && from) (*from)[li] = p;
	}
}

std::vector<Warning> Prefixed(const std::vector<Warning>& w, const std::string& label)
{
	std::vector<Warning> out;
	for (const Warning& x : w) out.push_back({ x.line, label + ": " + x.msg });
	return out;
}

// The per-character keys of a character document (every [char] / [char.<name>] instance, file order).
std::vector<KeyValue> CharKeys(const TagIni& ini) { return ini.Keys(SecKind::Char, ""); }

// Warnings of a character document that do not come from applying its keys.
void CharDocWarnings(const TagIni& ini, const std::string& label, const std::string& file, std::vector<Warning>& w)
{
	for (const Warning& x : ini.ParseWarnings()) w.push_back({ x.line, label + ": " + x.msg });
	for (const TagIni::SectionRef& s : ini.Sections()) {
		if (s.kind == SecKind::Tuning || s.kind == SecKind::Style)
			w.push_back({ s.line, label + ": [" + std::string(s.kind == SecKind::Tuning ? "tuning" : "style." + s.name) +
			                          "] ignored in a character file" });
		else if (s.kind == SecKind::Char && !s.name.empty() && !ieq(s.name, file))
			w.push_back({ s.line, label + ": [char." + s.name + "] in the file of '" + file + "': the file name wins" });
	}
}

} // namespace

bool SidecarSet::Empty() const
{
	for (int l = 0; l < 2; ++l) {
		if (global[l] || !shared[l].empty()) return false;
		for (int m = 0; m < kMoonCount; ++m) if (!moon[l][m].empty()) return false;
	}
	return true;
}

std::vector<std::string> SidecarSet::CharFiles() const
{
	std::set<std::string> names;
	for (int l = 0; l < 2; ++l) {
		for (const auto& kv : shared[l]) names.insert(kv.first);
		for (int m = 0; m < kMoonCount; ++m) for (const auto& kv : moon[l][m]) names.insert(kv.first);
		if (global[l])
			for (const std::string& n : global[l]->SectionNames(SecKind::Char)) {
				std::string x = n;
				for (char& c : x) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
				names.insert(x);
			}
	}
	return { names.begin(), names.end() };
}

GlobalResolution ResolveGlobal(const SidecarSet& s)
{
	GlobalResolution r;
	for (int l = 0; l < 2; ++l)
		if (s.global[l]) {
			const std::string label = std::string(LayerWord(l)) + "global.ini";
			for (const Warning& w : s.global[l]->ParseWarnings()) r.warnings.push_back({ w.line, label + ": " + w.msg });
		}
	// active_style: the last one over shipped [tuning] then local [tuning]
	for (int l = 0; l < 2; ++l)
		if (s.global[l]) { std::string v; if (s.global[l]->Get(SecKind::Tuning, "", "active_style", v)) r.styleRequested = v; }
	r.values = DefaultValues();
	for (auto& p : r.from) p = Provenance{ Src::Default, Layer::Shipped };
	if (!r.styleRequested.empty()) {
		bool found = false;
		for (int l = 1; l >= 0 && !found; --l) {
			if (!s.global[l]) continue;
			for (const std::string& n : s.global[l]->SectionNames(SecKind::Style))
				if (ieq(n, r.styleRequested)) {
					const std::string label = std::string(LayerWord(l)) + "global.ini";
					ApplyTracked(r.values, &r.from, { Src::Style, (Layer)l }, s.global[l]->Keys(SecKind::Style, n),
					             label + ": [style." + n + "]", r.warnings, false);
					found = true;
					break;
				}
		}
		if (!found) {
			if (const BuiltinStyle* b = FindBuiltinStyle(r.styleRequested)) {
				TagIni tmp;
				tmp.LoadText(std::string("[style.x]\n") + b->body);
				std::vector<Warning> ignored;
				ApplyTracked(r.values, &r.from, { Src::Style, Layer::Shipped }, tmp.Keys(SecKind::Style, "x"), "built-in", ignored, false);
				found = true;
			}
		}
		r.styleFound = found;
		if (found) r.style = r.styleRequested;
		else r.warnings.push_back({ 0, "active_style '" + r.styleRequested + "' is neither an ini [style.*] nor a built-in style - using the defaults" });
		// §3.3 styleMask: a lever counts as the style's only when it DIFFERS from Tuning{} (a style key that restates the
		// default is not provenance). The same rule as PovertyCaster's TagSidecar.hpp styleMaskOf.
		const Values def = DefaultValues();
		for (size_t i = 0; i < kLeverCount; ++i)
			if (r.from[i].src == Src::Style && r.values[i] == def[i]) r.from[i] = Provenance{ Src::Default, Layer::Shipped };
	}
	for (int l = 0; l < 2; ++l) {
		if (!s.global[l]) continue;
		std::vector<KeyValue> tun;
		for (const KeyValue& kv : s.global[l]->Keys(SecKind::Tuning, "")) if (!ieq(kv.key, "active_style")) tun.push_back(kv);
		ApplyTracked(r.values, &r.from, { Src::Tuning, (Layer)l }, tun, std::string(LayerWord(l)) + "global.ini: [tuning]",
		             r.warnings, false);
	}
	return r;
}

SlotResolution ResolveSlot(const SidecarSet& s, const GlobalResolution& g, const std::string& fileIn, int moon)
{
	SlotResolution r;
	r.values = g.values;
	r.from = g.from;
	std::string file = fileIn;
	for (char& c : file) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	for (int l = 0; l < 2; ++l) {   // legacy-tolerated [char.f] in global.ini
		if (!s.global[l]) continue;
		for (const std::string& n : s.global[l]->SectionNames(SecKind::Char))
			if (ieq(n, file)) {
				ApplyTracked(r.values, &r.from, { Src::Char, (Layer)l }, s.global[l]->Keys(SecKind::Char, n),
				             std::string(LayerWord(l)) + "global.ini: [char." + n + "]", r.warnings, true);
				r.hasCharFile = true;
			}
	}
	for (int l = 0; l < 2; ++l) {
		auto it = s.shared[l].find(file);
		if (it == s.shared[l].end() || !it->second) continue;
		ApplyTracked(r.values, &r.from, { Src::Char, (Layer)l }, CharKeys(*it->second), CharLabel(l, file, -1), r.warnings, true);
		r.hasCharFile = true;
	}
	if (moon >= 0 && moon < kMoonCount)
		for (int l = 0; l < 2; ++l) {
			auto it = s.moon[l][moon].find(file);
			if (it == s.moon[l][moon].end() || !it->second) continue;
			ApplyTracked(r.values, &r.from, { Src::Moon, (Layer)l }, CharKeys(*it->second), CharLabel(l, file, moon), r.warnings, true);
			r.hasMoonFile = true;
		}
	return r;
}

std::vector<Warning> AllSidecarWarnings(const SidecarSet& s)
{
	GlobalResolution g = ResolveGlobal(s);
	std::vector<Warning> w = g.warnings;
	for (int l = 0; l < 2; ++l) {
		if (s.global[l]) {
			const std::string label = std::string(LayerWord(l)) + "global.ini";
			for (const std::string& n : s.global[l]->SectionNames(SecKind::Char)) {
				w.push_back({ 0, label + ": per-character section in global.ini: move it to chars\\" + n + ".ini" });
				Values v = g.values;
				ApplyKvs(v, s.global[l]->Keys(SecKind::Char, n), label + ": [char." + n + "]", w, true);
			}
		}
		for (const auto& kv : s.shared[l]) {
			if (!kv.second) continue;
			const std::string label = CharLabel(l, kv.first, -1);
			CharDocWarnings(*kv.second, label, kv.first, w);
			Values v = g.values;
			ApplyKvs(v, CharKeys(*kv.second), label, w, true);
		}
		for (int m = 0; m < kMoonCount; ++m)
			for (const auto& kv : s.moon[l][m]) {
				if (!kv.second) continue;
				const std::string label = CharLabel(l, kv.first, m);
				CharDocWarnings(*kv.second, label, kv.first, w);
				Values v = g.values;
				ApplyKvs(v, CharKeys(*kv.second), label, w, true);
			}
	}
	return w;
}

std::vector<std::string> SidecarStyleNames(const SidecarSet& s)
{
	std::vector<std::string> v;
	for (const BuiltinStyle& b : kBuiltinStyles) v.emplace_back(b.name);
	for (int l = 0; l < 2; ++l) {
		if (!s.global[l]) continue;
		for (const std::string& n : s.global[l]->SectionNames(SecKind::Style)) {
			bool dup = false;
			for (const std::string& x : v) dup = dup || ieq(x, n);
			if (!dup) v.push_back(n);
		}
	}
	return v;
}

// ================== files ==================

namespace {
std::set<std::string>& BakDone() { static std::set<std::string> s; return s; }

std::wstring Wide(const std::string& utf8) { return fs::u8path(utf8).wstring(); }

bool Stat(const std::string& path, uint64_t& t, uint64_t& size)
{
	WIN32_FILE_ATTRIBUTE_DATA fa{};
	if (!GetFileAttributesExW(Wide(path).c_str(), GetFileExInfoStandard, &fa)) return false;
	t = ((uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
	size = ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
	return true;
}
} // namespace

void ResetBakMemory() { BakDone().clear(); }

bool ReadWholeFile(const std::string& path, std::string& out, bool* exists)
{
	out.clear();
	HANDLE h = CreateFileW(Wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		const DWORD e = GetLastError();
		const bool missing = e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND;
		if (exists) *exists = !missing;
		return missing;   // "absent" is a successful read of nothing; a sharing violation is a failure
	}
	if (exists) *exists = true;
	char buf[65536];
	DWORD rd = 0;
	bool ok = true;
	for (;;) {
		if (!ReadFile(h, buf, sizeof buf, &rd, nullptr)) { ok = false; break; }
		if (!rd) break;
		out.append(buf, rd);
	}
	CloseHandle(h);
	return ok;
}

bool WriteFileAtomic(const std::string& path, const std::string& bytes, bool bakOnce, std::string* err)
{
	std::error_code ec;
	const fs::path p = fs::u8path(path);
	fs::create_directories(p.parent_path(), ec);
	if (bakOnce && !BakDone().count(path)) {
		std::string prev;
		bool exists = false;
		if (ReadWholeFile(path, prev, &exists) && exists) {
			const std::wstring bak = Wide(path + ".bak");
			HANDLE b = CreateFileW(bak.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (b != INVALID_HANDLE_VALUE) {
				DWORD wr = 0;
				WriteFile(b, prev.data(), (DWORD)prev.size(), &wr, nullptr);
				CloseHandle(b);
			}
		}
		BakDone().insert(path);
	}
	const std::wstring tmp = Wide(path + ".tmp");
	HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) { if (err) *err = "cannot create " + path + ".tmp"; return false; }
	DWORD wr = 0;
	const bool wrote = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &wr, nullptr) && wr == bytes.size();
	FlushFileBuffers(h);
	CloseHandle(h);
	if (!wrote) { DeleteFileW(tmp.c_str()); if (err) *err = "write failed: " + path; return false; }
	if (!MoveFileExW(tmp.c_str(), Wide(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		DeleteFileW(tmp.c_str());
		if (err) *err = "replace failed (file busy?): " + path;
		return false;
	}
	return true;
}

// ================== workspace ==================

std::string SidecarWorkspace::LayerRoot(Layer l) const { return l == Layer::Local ? m_root + "\\local" : m_root; }
std::string SidecarWorkspace::pathOf(const DocId& id) const { return LayerRoot(id.layer) + "\\" + id.RelPath(); }

void SidecarWorkspace::loadDoc(const DocId& id)
{
	auto d = std::make_unique<SidecarDoc>();
	d->id = id;
	d->path = pathOf(id);
	d->ini.SetCharFileMode(id.kind == DocKind::CharShared || id.kind == DocKind::CharMoon);
	std::string text;
	bool exists = false;
	ReadWholeFile(d->path, text, &exists);
	d->onDisk = exists;
	if (exists) Stat(d->path, d->diskTime, d->diskSize);
	d->ini.LoadText(text);
	m_docs[id] = std::move(d);
}

bool SidecarWorkspace::Open(const std::string& root)
{
	Close();
	if (root.empty()) return false;
	m_root = root;
	while (!m_root.empty() && (m_root.back() == '\\' || m_root.back() == '/')) m_root.pop_back();
	for (int l = 0; l < 2; ++l) {
		const Layer layer = (Layer)l;
		loadDoc(GlobalDoc(layer));
		loadDoc(HudDoc(layer));
		std::error_code ec;
		for (const auto& e : fs::directory_iterator(fs::u8path(LayerRoot(layer) + "\\chars"), ec)) {
			if (!e.is_regular_file(ec)) continue;
			std::string file;
			int moon = -1;
			if (!ParseCharFileName(e.path().filename().u8string(), file, moon)) continue;
			loadDoc(CharDoc(layer, file, moon));
		}
	}
	return true;
}

void SidecarWorkspace::Close() { m_root.clear(); m_docs.clear(); }

bool SidecarWorkspace::Exists() const
{
	for (const auto& kv : m_docs)
		if (kv.second->onDisk && kv.first.kind != DocKind::Hud) return true;
	return false;
}

SidecarDoc& SidecarWorkspace::Doc(const DocId& id)
{
	auto it = m_docs.find(id);
	if (it != m_docs.end()) return *it->second;
	auto d = std::make_unique<SidecarDoc>();
	d->id = id;
	d->path = pathOf(id);
	d->ini.SetCharFileMode(id.kind == DocKind::CharShared || id.kind == DocKind::CharMoon);
	d->ini.LoadText("");
	SidecarDoc& ref = *d;
	m_docs[id] = std::move(d);
	return ref;
}

const SidecarDoc* SidecarWorkspace::Find(const DocId& id) const
{
	auto it = m_docs.find(id);
	return it == m_docs.end() ? nullptr : it->second.get();
}

std::vector<const SidecarDoc*> SidecarWorkspace::Docs() const
{
	std::vector<const SidecarDoc*> v;
	for (const auto& kv : m_docs) v.push_back(kv.second.get());
	return v;
}

std::vector<std::string> SidecarWorkspace::CharFiles() const
{
	std::set<std::string> n;
	for (const auto& kv : m_docs)
		if ((kv.first.kind == DocKind::CharShared || kv.first.kind == DocKind::CharMoon) &&
		    (kv.second->onDisk || !kv.second->ini.Text().empty()))
			n.insert(kv.first.file);
	return { n.begin(), n.end() };
}

SidecarSet SidecarWorkspace::Set(bool saved) const
{
	SidecarSet s;
	static thread_local std::vector<std::unique_ptr<TagIni>> keep;   // saved copies live until the next Set(true)
	if (saved) keep.clear();
	for (const auto& kv : m_docs) {
		const SidecarDoc& d = *kv.second;
		const TagIni* ini = &d.ini;
		const std::string& text = saved ? d.ini.SavedText() : d.ini.Text();
		if (text.empty()) continue;   // an empty document contributes nothing (and does not exist)
		if (saved) {
			auto c = std::make_unique<TagIni>();
			c->SetCharFileMode(d.ini.CharFileMode());
			c->LoadText(text);
			ini = c.get();
			keep.push_back(std::move(c));
		}
		const int l = (int)kv.first.layer;
		switch (kv.first.kind) {
		case DocKind::Global: s.global[l] = ini; break;
		case DocKind::CharShared: s.shared[l][kv.first.file] = ini; break;
		case DocKind::CharMoon: if (kv.first.moon >= 0 && kv.first.moon < kMoonCount) s.moon[l][kv.first.moon][kv.first.file] = ini; break;
		case DocKind::Hud: break;
		}
	}
	return s;
}

GlobalResolution SidecarWorkspace::Global(bool saved) const { return ResolveGlobal(Set(saved)); }

SlotResolution SidecarWorkspace::Slot(const std::string& file, int moon, bool saved) const
{
	const SidecarSet s = Set(saved);
	return ResolveSlot(s, ResolveGlobal(s), file, moon);
}

bool SidecarWorkspace::AnyDirty() const
{
	for (const auto& kv : m_docs) if (kv.second->ini.IsDirty()) return true;
	return false;
}

std::vector<DocId> SidecarWorkspace::DirtyDocs() const
{
	std::vector<DocId> v;
	for (const auto& kv : m_docs) if (kv.second->ini.IsDirty()) v.push_back(kv.first);
	return v;
}

SidecarWorkspace::SaveResult SidecarWorkspace::SaveDirty()
{
	SaveResult r;
	std::vector<DocId> dirty = DirtyDocs();
	if (dirty.empty()) { r.nothing = true; r.message = "nothing to save"; return r; }
	for (const DocId& id : dirty)
		if (m_docs[id]->externalChange) {
			r.ok = false;
			r.message = id.Label() + " changed on disk: choose Load theirs / Keep mine first";
			return r;
		}
	const std::vector<Warning> before = AllSidecarWarnings(Set(true));
	const std::vector<Warning> now = AllSidecarWarnings(Set(false));
	r.blocked = NewWarnings(before, now);
	if (!r.blocked.empty()) {
		r.ok = false;
		r.message = "NOT saved: the edit adds " + std::to_string(r.blocked.size()) + " warning(s): " + r.blocked.front().msg;
		return r;
	}
	// character files first, hud next, global.ini last (§2.8 4)
	std::stable_sort(dirty.begin(), dirty.end(), [](const DocId& a, const DocId& b) {
		auto rank = [](DocKind k) { return k == DocKind::CharShared || k == DocKind::CharMoon ? 0 : k == DocKind::Hud ? 1 : 2; };
		return rank(a.kind) < rank(b.kind);
	});
	for (const DocId& id : dirty) {
		SidecarDoc& d = *m_docs[id];
		std::string err;
		if (!WriteFileAtomic(d.path, d.ini.Text(), true, &err)) {
			r.ok = false;
			r.message = err;
			return r;
		}
		d.ini.MarkSaved();
		d.onDisk = true;
		Stat(d.path, d.diskTime, d.diskSize);
		r.written.push_back(id.Label());
	}
	r.message = "saved " + std::to_string(r.written.size()) + " file(s)";
	return r;
}

void SidecarWorkspace::Revert(const DocId& id)
{
	auto it = m_docs.find(id);
	if (it == m_docs.end()) return;
	std::string text;
	bool exists = false;
	ReadWholeFile(it->second->path, text, &exists);
	it->second->ini.LoadText(text);
	it->second->onDisk = exists;
	it->second->externalChange = false;
	if (exists) Stat(it->second->path, it->second->diskTime, it->second->diskSize);
}

std::vector<DocId> SidecarWorkspace::PollExternal()
{
	std::vector<DocId> changed;
	if (m_root.empty()) return changed;
	// new character files that appeared
	for (int l = 0; l < 2; ++l) {
		std::error_code ec;
		for (const auto& e : fs::directory_iterator(fs::u8path(LayerRoot((Layer)l) + "\\chars"), ec)) {
			std::string file;
			int moon = -1;
			if (!e.is_regular_file(ec) || !ParseCharFileName(e.path().filename().u8string(), file, moon)) continue;
			const DocId id = CharDoc((Layer)l, file, moon);
			if (!m_docs.count(id)) { loadDoc(id); changed.push_back(id); }
		}
	}
	for (auto& kv : m_docs) {
		SidecarDoc& d = *kv.second;
		uint64_t t = 0, sz = 0;
		const bool exists = Stat(d.path, t, sz);
		if (exists == d.onDisk && (!exists || (t == d.diskTime && sz == d.diskSize))) continue;
		std::string text;
		if (!ReadWholeFile(d.path, text)) continue;   // busy: try again next poll
		if (exists && text == d.ini.SavedText()) { d.diskTime = t; d.diskSize = sz; d.onDisk = true; continue; }
		d.onDisk = exists;
		d.diskTime = t;
		d.diskSize = sz;
		if (!d.ini.IsDirty()) {
			d.ini.LoadText(text);
		} else {
			d.externalChange = true;
			d.theirs = text;
		}
		changed.push_back(kv.first);
	}
	return changed;
}

void SidecarWorkspace::ResolveExternal(const DocId& id, bool loadTheirs)
{
	auto it = m_docs.find(id);
	if (it == m_docs.end() || !it->second->externalChange) return;
	SidecarDoc& d = *it->second;
	if (loadTheirs) {
		d.ini.LoadText(d.theirs);
	} else {
		// keep mine: my bytes stay; the disk's bytes become the baseline, so a save overwrites them
		const std::string mine = d.ini.Text();
		d.ini.LoadText(d.theirs);
		d.ini.SetText(mine);
	}
	d.externalChange = false;
	d.theirs.clear();
}

SidecarWorkspace::Snapshot SidecarWorkspace::Take() const
{
	Snapshot s;
	for (const auto& kv : m_docs) s[kv.first] = kv.second->ini.Text();
	return s;
}

std::vector<DocId> SidecarWorkspace::Restore(const Snapshot& s)
{
	std::vector<DocId> changed;
	for (auto& kv : m_docs) {
		auto it = s.find(kv.first);
		const std::string want = it == s.end() ? std::string() : it->second;
		if (kv.second->ini.Text() != want) { kv.second->ini.SetText(want); changed.push_back(kv.first); }
	}
	for (const auto& kv : s)
		if (!m_docs.count(kv.first)) {
			SidecarDoc& d = Doc(kv.first);
			if (d.ini.Text() != kv.second) { d.ini.SetText(kv.second); changed.push_back(kv.first); }
		}
	return changed;
}

namespace {
SecKind LeverSection(DocKind k) { return k == DocKind::Global ? SecKind::Tuning : SecKind::Char; }
} // namespace

void SetDocLever(TagIni& ini, DocKind kind, int lever, int32_t v)
{
	ini.Set(LeverSection(kind), "", kLevers[lever].key, FormatLeverValue(kLevers[lever], v));
}

void ClearDocLever(TagIni& ini, DocKind kind, int lever) { ini.Remove(LeverSection(kind), "", kLevers[lever].key); }

bool DocHasLever(const TagIni& ini, DocKind kind, int lever, int32_t* v)
{
	std::string s;
	if (!ini.Get(LeverSection(kind), "", kLevers[lever].key, s)) return false;
	int32_t x = 0;
	if (!ParseLeverValue(kLevers[lever], s, x)) return false;
	if (v) *v = x;
	return true;
}

bool SidecarWorkspace::PromoteLever(const DocId& localDoc, int lever, std::string* err)
{
	if (localDoc.layer != Layer::Local || lever < 0 || lever >= (int)kLeverCount) { if (err) *err = "not a local lever"; return false; }
	SidecarDoc& from = Doc(localDoc);
	std::string v;
	if (!from.ini.Get(LeverSection(localDoc.kind), "", kLevers[lever].key, v)) { if (err) *err = "the local file does not set it"; return false; }
	DocId to = localDoc;
	to.layer = Layer::Shipped;
	Doc(to).ini.Set(LeverSection(to.kind), "", kLevers[lever].key, v);
	from.ini.Remove(LeverSection(localDoc.kind), "", kLevers[lever].key);
	return true;
}

int SidecarWorkspace::PromoteAll(const DocId& localDoc)
{
	int n = 0;
	for (size_t i = 0; i < kLeverCount; ++i)
		if (DocHasLever(Doc(localDoc).ini, localDoc.kind, (int)i) && PromoteLever(localDoc, (int)i, nullptr)) ++n;
	return n;
}

} // namespace tagtune
