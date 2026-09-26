// tag_tuning.ini editor + resolution — see tag_ini.h.
#include "tag_ini.h"

#include <algorithm>

namespace tagtune {

namespace {

bool IsBlank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string Lower(std::string s)
{
	for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	return s;
}

} // namespace

// ================== TagIni ==================

void TagIni::LoadText(const std::string& raw)
{
	m_text = raw;
	m_saved = raw;
	reindex();
}

void TagIni::SetText(const std::string& raw)
{
	m_text = raw;
	reindex();
}

std::string TagIni::eol() const
{
	return m_text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

void TagIni::reindex()
{
	m_lines.clear();
	m_inst.clear();
	size_t p = 0;
	int cur = -1;
	while (p < m_text.size()) {
		Line l;
		l.begin = p;
		size_t nl = m_text.find('\n', p);
		l.next = nl == std::string::npos ? m_text.size() : nl + 1;
		l.end = nl == std::string::npos ? m_text.size() : nl;
		if (l.end > l.begin && m_text[l.end - 1] == '\r') --l.end;
		p = l.next;
		// comment cut: ; or # at the start, or after a blank (parseTuningIni)
		size_t e = l.end;
		for (size_t k = l.begin; k < l.end; ++k) {
			const char c = m_text[k];
			if ((c == ';' || c == '#') && (k == l.begin || m_text[k - 1] == ' ' || m_text[k - 1] == '\t')) { e = k; break; }
		}
		size_t b = l.begin;
		while (b < e && IsBlank(m_text[b])) ++b;
		while (e > b && IsBlank(m_text[e - 1])) --e;
		if (b < e && m_text[b] == '[') {
			const size_t close = m_text.find(']', b);
			size_t nb = b + 1, ne = (close == std::string::npos || close >= e) ? e : close;
			while (nb < ne && IsBlank(m_text[nb])) ++nb;
			while (ne > nb && IsBlank(m_text[ne - 1])) --ne;
			const std::string name = m_text.substr(nb, ne - nb);
			Inst in;
			in.headerLine = m_lines.size();
			if (ieq(name, "tuning")) in.kind = SecKind::Tuning;
			else if (name.size() > 6 && ieq(name.substr(0, 6), "style.")) in.kind = SecKind::Style;
			else if (name.size() > 5 && ieq(name.substr(0, 5), "char.")) in.kind = SecKind::Char;
			else if (m_charFile && ieq(name, "char")) in.kind = SecKind::Char;
			else in.kind = SecKind::Other;
			if (in.kind == SecKind::Char && ieq(name, "char")) {
				in.name.clear();
			} else if (in.kind == SecKind::Style || in.kind == SecKind::Char) {
				std::string n = name.substr(in.kind == SecKind::Style ? 6 : 5);
				size_t a = 0, z = n.size();
				while (a < z && IsBlank(n[a])) ++a;
				while (z > a && IsBlank(n[z - 1])) --z;
				in.name = n.substr(a, z - a);
			} else {
				in.name = name;
			}
			m_inst.push_back(in);
			cur = (int)m_inst.size() - 1;
			l.header = true;
			l.inst = cur;
		} else if (b < e) {
			l.inst = cur;
			const size_t eq = m_text.find('=', b);
			if (eq == std::string::npos || eq >= e) {
				l.junk = true;
			} else {
				l.kv = true;
				size_t kb = b, ke = eq;
				while (ke > kb && IsBlank(m_text[ke - 1])) --ke;
				size_t vb = eq + 1, ve = e;
				while (vb < ve && IsBlank(m_text[vb])) ++vb;
				l.keyB = kb; l.keyE = ke; l.valB = vb; l.valE = ve;
			}
		} else {
			l.inst = cur;
		}
		m_lines.push_back(l);
	}
}

std::vector<int> TagIni::scope(SecKind k, const std::string& name) const
{
	std::vector<int> out;
	for (size_t i = 0; i < m_inst.size(); ++i) {
		if (m_inst[i].kind != k) continue;
		if (k != SecKind::Tuning && !(k == SecKind::Char && m_charFile) && !ieq(m_inst[i].name, name)) continue;
		out.push_back((int)i);
		if (k == SecKind::Style) break;   // only the first [style.<name>] counts
	}
	return out;
}

std::vector<std::string> TagIni::SectionNames(SecKind k) const
{
	std::vector<std::string> out;
	for (const Inst& in : m_inst) {
		if (in.kind != k) continue;
		bool dup = false;
		for (const std::string& n : out) dup = dup || ieq(n, in.name);
		if (!dup) out.push_back(in.name);
	}
	return out;
}

bool TagIni::HasSection(SecKind k, const std::string& name) const { return !scope(k, name).empty(); }

std::vector<KeyValue> TagIni::Keys(SecKind k, const std::string& name) const
{
	std::vector<KeyValue> out;
	const std::vector<int> sc = scope(k, name);
	for (size_t i = 0; i < m_lines.size(); ++i) {
		const Line& l = m_lines[i];
		if (!l.kv || std::find(sc.begin(), sc.end(), l.inst) == sc.end()) continue;
		out.push_back({ key(l), value(l), (int)i + 1 });
	}
	return out;
}

bool TagIni::Get(SecKind k, const std::string& name, const std::string& keyName, std::string& v) const
{
	bool found = false;
	for (const KeyValue& kv : Keys(k, name))
		if (ieq(kv.key, keyName)) { v = kv.value; found = true; }
	return found;
}

void TagIni::insertAt(size_t pos, const std::string& bytes)
{
	m_text.insert(pos, bytes);
	reindex();
}

void TagIni::Set(SecKind k, const std::string& name, const std::string& keyName, const std::string& v)
{
	const std::vector<int> sc = scope(k, name);
	int last = -1;
	for (size_t i = 0; i < m_lines.size(); ++i)
		if (m_lines[i].kv && std::find(sc.begin(), sc.end(), m_lines[i].inst) != sc.end() && ieq(key(m_lines[i]), keyName))
			last = (int)i;
	if (last >= 0) {
		const Line& l = m_lines[last];
		if (value(l) == v) return;
		m_text.replace(l.valB, l.valE - l.valB, v);
		reindex();
		return;
	}
	const std::string nl = eol();
	if (sc.empty()) {
		// a new section at the end of the file
		std::string add;
		if (!m_text.empty() && m_text.back() != '\n') add += nl;
		if (!m_text.empty()) add += nl;
		add += k == SecKind::Tuning ? "[tuning]" : k == SecKind::Style ? "[style." + name + "]"
		     : k == SecKind::Other ? "[" + name + "]"
		     : m_charFile ? std::string("[char]") : "[char." + name + "]";
		add += nl + keyName + "=" + v + nl;
		insertAt(m_text.size(), add);
		return;
	}
	// after the last key line of the target instance (tuning / char: the last one; style: the first)
	const int target = k == SecKind::Style ? sc.front() : sc.back();
	size_t after = m_inst[target].headerLine;
	for (size_t i = after + 1; i < m_lines.size() && m_lines[i].inst == target && !m_lines[i].header; ++i)
		if (m_lines[i].kv) after = i;
	const Line& l = m_lines[after];
	std::string add = keyName + "=" + v + nl;
	if (l.next == l.end) add = nl + keyName + "=" + v;   // the anchor is the last line and has no terminator
	insertAt(l.next, add);
}

bool TagIni::Remove(SecKind k, const std::string& name, const std::string& keyName)
{
	const std::vector<int> sc = scope(k, name);
	bool any = false;
	for (size_t i = m_lines.size(); i-- > 0;) {
		const Line& l = m_lines[i];
		if (!l.kv || std::find(sc.begin(), sc.end(), l.inst) == sc.end() || !ieq(key(l), keyName)) continue;
		m_text.erase(l.begin, l.next - l.begin);
		any = true;
	}
	if (any) reindex();
	return any;
}

void TagIni::RemoveSection(SecKind k, const std::string& name)
{
	std::vector<int> sc;
	for (size_t i = 0; i < m_inst.size(); ++i)   // every instance (styles too: remove means gone)
		if (m_inst[i].kind == k && (k == SecKind::Tuning || (k == SecKind::Char && m_charFile) || ieq(m_inst[i].name, name)))
			sc.push_back((int)i);
	if (sc.empty()) return;
	for (size_t i = m_lines.size(); i-- > 0;) {
		const Line& l = m_lines[i];
		if (std::find(sc.begin(), sc.end(), l.inst) == sc.end() || !(l.header || l.kv)) continue;
		size_t b = l.begin;
		// a header that was preceded by one blank line takes that blank line with it
		if (l.header && i > 0) {
			const Line& p = m_lines[i - 1];
			bool blank = !p.header && !p.kv && !p.junk;
			for (size_t q = p.begin; blank && q < p.end; ++q) blank = IsBlank(m_text[q]);
			if (blank) b = p.begin;
		}
		m_text.erase(b, l.next - b);
		if (b != l.begin) --i;   // the blank line went too
	}
	reindex();
}

std::vector<TagIni::SectionRef> TagIni::Sections() const
{
	std::vector<SectionRef> out;
	for (const Inst& in : m_inst) out.push_back({ in.kind, in.name, (int)in.headerLine + 1 });
	return out;
}

std::string TagIni::ActiveStyle() const
{
	std::string v;
	Get(SecKind::Tuning, "", "active_style", v);
	return v;
}

std::vector<Warning> TagIni::ParseWarnings() const
{
	std::vector<Warning> w;
	for (size_t i = 0; i < m_lines.size(); ++i) {
		const Line& l = m_lines[i];
		const int ln = (int)i + 1;
		if (l.header && m_inst[l.inst].kind == SecKind::Other)
			w.push_back({ ln, "unknown section [" + m_inst[l.inst].name + "] ignored" });
		else if (l.junk) {
			size_t b = l.begin, e = l.end;
			while (b < e && IsBlank(m_text[b])) ++b;
			w.push_back({ ln, "not key=value: '" + m_text.substr(b, e - b) + "'" });
		} else if (l.kv && l.inst < 0)
			w.push_back({ ln, "'" + key(l) + "' outside any section ignored" });
	}
	return w;
}

// ================== resolution ==================

void ApplyKvs(Values& v, const std::vector<KeyValue>& kvs, const std::string& where, std::vector<Warning>& warn,
              bool perChar)
{
	for (const KeyValue& kv : kvs) {
		const int li = FindLever(kv.key);
		if (li < 0) { warn.push_back({ kv.line, where + ": unknown lever '" + kv.key + "'" }); continue; }
		const Lever& l = kLevers[li];
		if (perChar && !l.perChar) {
			warn.push_back({ kv.line, where + ": '" + kv.key + "' is team-wide, not per character - ignored here" });
			continue;
		}
		if (!perChar && l.scope == LeverScope::CharOnly) {
			warn.push_back({ kv.line, where + ": '" + kv.key + "' is only valid in a [char.<file>] section" });
			continue;
		}
		int32_t x = 0;
		if (!ParseLeverValue(l, kv.value, x)) {
			warn.push_back({ kv.line, where + ": bad value '" + kv.value + "' for " + l.key + " (range " +
			                 std::to_string(l.lo) + ".." + std::to_string(l.hi) + ")" });
			continue;
		}
		v[li] = x;
	}
}

bool StyleBase(const TagIni& ini, const std::string& name, Values& out, std::vector<Warning>& warn)
{
	out = DefaultValues();
	if (name.empty()) return true;
	for (const std::string& s : ini.SectionNames(SecKind::Style))
		if (ieq(s, name)) {
			ApplyKvs(out, ini.Keys(SecKind::Style, s), "[style." + s + "]", warn, false);
			return true;
		}
	if (const BuiltinStyle* b = FindBuiltinStyle(name)) {
		TagIni tmp;
		tmp.LoadText(std::string("[style.x]\n") + b->body);
		std::vector<Warning> ignored;
		ApplyKvs(out, tmp.Keys(SecKind::Style, "x"), std::string("built-in style ") + b->name, ignored, false);
		return true;
	}
	warn.push_back({ 0, "active_style '" + name + "' is neither an ini [style.*] nor a built-in style - using the defaults" });
	return false;
}

Resolved Resolve(const TagIni& ini)
{
	Resolved r;
	r.warnings = ini.ParseWarnings();
	const std::string active = ini.ActiveStyle();
	r.styleFound = StyleBase(ini, active, r.global, r.warnings);
	r.style = r.styleFound ? active : std::string();
	std::vector<KeyValue> tun;
	for (const KeyValue& kv : ini.Keys(SecKind::Tuning, ""))
		if (!ieq(kv.key, "active_style")) tun.push_back(kv);
	ApplyKvs(r.global, tun, "[tuning]", r.warnings, false);
	return r;
}

Values ForChar(const TagIni& ini, const Values& global, const std::string& file, std::vector<Warning>* warn)
{
	Values v = global;
	std::vector<Warning> w;
	for (const std::string& n : ini.SectionNames(SecKind::Char))
		if (ieq(n, file)) ApplyKvs(v, ini.Keys(SecKind::Char, n), "[char." + n + "]", w, true);
	if (warn) warn->insert(warn->end(), w.begin(), w.end());
	return v;
}

std::vector<std::string> StyleNames(const TagIni& ini)
{
	std::vector<std::string> v;
	for (const BuiltinStyle& b : kBuiltinStyles) v.emplace_back(b.name);
	for (const std::string& s : ini.SectionNames(SecKind::Style)) {
		bool dup = false;
		for (const std::string& n : v) dup = dup || ieq(n, s);
		if (!dup) v.push_back(s);
	}
	return v;
}

std::vector<Warning> AllWarnings(const TagIni& ini)
{
	Resolved r = Resolve(ini);
	std::vector<Warning> w = r.warnings;
	for (const std::string& n : ini.SectionNames(SecKind::Char)) ForChar(ini, r.global, n, &w);
	return w;
}

std::vector<Warning> NewWarnings(const std::vector<Warning>& before, const std::vector<Warning>& now)
{
	std::vector<std::string> pool;
	for (const Warning& w : before) pool.push_back(w.msg);
	std::vector<Warning> out;
	for (const Warning& w : now) {
		auto it = std::find(pool.begin(), pool.end(), w.msg);
		if (it != pool.end()) pool.erase(it);
		else out.push_back(w);
	}
	return out;
}

// ================== panel operations ==================

namespace {
void DropLeverKeys(TagIni& ini, SecKind k, const std::string& name)
{
	for (const KeyValue& kv : ini.Keys(k, name))
		if (FindLever(kv.key) >= 0) ini.Remove(k, name, kv.key);
}
} // namespace

void SelectStyle(TagIni& ini, const std::string& name, bool dropOverrides)
{
	ini.Set(SecKind::Tuning, "", "active_style", name);
	if (dropOverrides) DropLeverKeys(ini, SecKind::Tuning, "");
}

void SaveAsStyle(TagIni& ini, const std::string& name, const Values& live)
{
	const Values d = DefaultValues();
	std::string secName = name;
	for (const std::string& s : ini.SectionNames(SecKind::Style)) if (ieq(s, name)) secName = s;
	// lever keys that are no longer different from the defaults go; the rest are set in place / appended
	for (const KeyValue& kv : ini.Keys(SecKind::Style, secName)) {
		const int li = FindLever(kv.key);
		if (li >= 0 && live[li] == d[li]) ini.Remove(SecKind::Style, secName, kv.key);
	}
	bool any = false;
	for (size_t i = 0; i < kLeverCount; ++i) {
		if (live[i] == d[i]) continue;
		ini.Set(SecKind::Style, secName, kLevers[i].key, FormatLeverValue(kLevers[i], live[i]));
		any = true;
	}
	if (!any && !ini.HasSection(SecKind::Style, secName)) {
		// a style equal to the defaults: still create the (empty) section so the name exists
		std::string t = ini.Text();
		const std::string nl = t.find("\r\n") != std::string::npos ? "\r\n" : "\n";
		if (!t.empty() && t.back() != '\n') t += nl;
		if (!t.empty()) t += nl;
		t += "[style." + secName + "]" + nl;
		ini.SetText(t);
	}
	ini.Set(SecKind::Tuning, "", "active_style", secName);
	DropLeverKeys(ini, SecKind::Tuning, "");
}

void SetTuningLever(TagIni& ini, int lever, int32_t v)
{
	ini.Set(SecKind::Tuning, "", kLevers[lever].key, FormatLeverValue(kLevers[lever], v));
}

void ClearTuningLever(TagIni& ini, int lever) { ini.Remove(SecKind::Tuning, "", kLevers[lever].key); }

void SetCharLever(TagIni& ini, const std::string& file, int lever, int32_t v)
{
	std::string name = Lower(file);
	for (const std::string& n : ini.SectionNames(SecKind::Char)) if (ieq(n, file)) name = n;
	ini.Set(SecKind::Char, name, kLevers[lever].key, FormatLeverValue(kLevers[lever], v));
}

void ClearCharLever(TagIni& ini, const std::string& file, int lever)
{
	ini.Remove(SecKind::Char, file, kLevers[lever].key);
}

} // namespace tagtune
