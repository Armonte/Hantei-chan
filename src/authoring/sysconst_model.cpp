#include "sysconst_model.h"

#include "../misc.h" // WriteFileAtomic

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace sysconst {

namespace {

bool ReadAll(const std::string& path, std::string& out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

// Byte offset of "//" outside nothing (these files have no string literals), or npos.
size_t CommentAt(const std::string& s, size_t b, size_t e)
{
	for (size_t i = b; i + 1 < e; i++)
		if (s[i] == '/' && s[i + 1] == '/') return i;
	return std::string::npos;
}

bool IEq(const std::string& a, const std::string& b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++)
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
	return true;
}

bool IsSpace(char c) { return c == ' ' || c == '\t'; }

template <class F>
void ForEachLine(const std::string& s, F&& f)
{
	size_t b = 0, n = 0;
	while (b < s.size()) {
		size_t e = s.find('\n', b);
		const size_t next = e == std::string::npos ? s.size() : e + 1;
		if (e == std::string::npos) e = s.size();
		if (e > b && s[e - 1] == '\r') e--;
		f(n++, b, e);
		b = next;
	}
}

} // namespace

// ---- _type.txt ----

bool TypeFile::load(const std::string& path)
{
	std::string raw;
	if (!ReadAll(path, raw)) return false;
	return loadText(raw);
}

bool TypeFile::loadText(const std::string& raw)
{
	m_text = m_saved = raw;
	parse();
	return true;
}

bool TypeFile::save(const std::string& path)
{
	if (!WriteFileAtomic(path.c_str(), m_text.data(), m_text.size())) return false;
	m_saved = m_text;
	return true;
}

void TypeFile::parse()
{
	m_sections.clear();
	m_keys.clear();
	m_valueSpan.clear();
	std::string section;
	ForEachLine(m_text, [&](size_t n, size_t b, size_t e) {
		const size_t c = CommentAt(m_text, b, e);
		const size_t ce = c == std::string::npos ? e : c;
		size_t i = b;
		while (i < ce && IsSpace(m_text[i])) i++;
		if (i < ce && m_text[i] == '[') {
			const size_t close = m_text.find(']', i);
			if (close != std::string::npos && close < ce) {
				section = m_text.substr(i + 1, close - i - 1);
				m_sections.push_back(section);
			}
			return;
		}
		const size_t eq = m_text.find('=', i);
		if (eq == std::string::npos || eq >= ce) return;
		size_t kb = i, ke = eq;
		while (ke > kb && IsSpace(m_text[ke - 1])) ke--;
		if (ke == kb) return;
		size_t vb = eq + 1, ve = ce;
		while (vb < ve && IsSpace(m_text[vb])) vb++;
		while (ve > vb && IsSpace(m_text[ve - 1])) ve--;
		TypeKey k;
		k.section = section;
		k.key = m_text.substr(kb, ke - kb);
		k.value = m_text.substr(vb, ve - vb);
		if (c != std::string::npos) {
			size_t cb = c + 2;
			while (cb < e && IsSpace(m_text[cb])) cb++;
			k.comment = m_text.substr(cb, e - cb);
		}
		k.line = n;
		m_keys.push_back(std::move(k));
		m_valueSpan.emplace_back(vb, ve);
	});
}

const TypeKey* TypeFile::find(const std::string& section, const std::string& key) const
{
	for (const auto& k : m_keys)
		if (k.section == section && IEq(k.key, key)) return &k;
	return nullptr;
}

bool TypeFile::setValue(const std::string& section, const std::string& key, const std::string& value)
{
	for (size_t i = 0; i < m_keys.size(); i++) {
		if (m_keys[i].section != section || !IEq(m_keys[i].key, key)) continue;
		const auto [vb, ve] = m_valueSpan[i];
		m_text.replace(vb, ve - vb, value);
		parse();
		return true;
	}
	return false;
}

// ---- vector.txt ----

const char* const VectorFile::kListFields[5] = {"Id", "X", "Y", "Add X", "Add Y"};
const char* const VectorFile::kHeaderFields[7] = {"No", "Name", "VecCnt", "UkemiTime", "Prio", "PrioAni", "KoCheck"};
const char* const VectorFile::kStepFields[6] = {"VecNum", "HitAni", "GuardAni", "Time", "VecTime", "flag"};

bool VectorFile::load(const std::string& path)
{
	std::string raw;
	if (!ReadAll(path, raw)) return false;
	return loadText(raw);
}

bool VectorFile::loadText(const std::string& raw)
{
	m_text = m_saved = raw;
	parse();
	return true;
}

bool VectorFile::save(const std::string& path)
{
	if (!WriteFileAtomic(path.c_str(), m_text.data(), m_text.size())) return false;
	m_saved = m_text;
	return true;
}

void VectorFile::parse()
{
	m_rows.clear();
	bool part2 = false;
	int header = -1;
	ForEachLine(m_text, [&](size_t n, size_t b, size_t e) {
		const size_t c = CommentAt(m_text, b, e);
		const size_t ce = c == std::string::npos ? e : c;
		Row r;
		r.line = n;
		for (size_t i = b; i < ce;) {
			while (i < ce && IsSpace(m_text[i])) i++;
			if (i >= ce) break;
			size_t j = i;
			while (j < ce && !IsSpace(m_text[j])) j++;
			r.tok.emplace_back(i, j);
			i = j;
		}
		if (r.tok.empty()) return;
		const std::string first = m_text.substr(r.tok[0].first, r.tok[0].second - r.tok[0].first);
		if (!part2 && first == "END") { part2 = true; return; }
		const auto isNum = [&](size_t t) {
			const auto [tb, te] = r.tok[t];
			size_t k = tb;
			if (k < te && (m_text[k] == '-' || m_text[k] == '+')) k++;
			if (k >= te) return false;
			for (; k < te; k++) if (!std::isdigit(static_cast<unsigned char>(m_text[k]))) return false;
			return true;
		};
		if (!isNum(0)) return;
		if (c != std::string::npos) {
			size_t cb = c + 2;
			while (cb < e && IsSpace(m_text[cb])) cb++;
			r.comment = m_text.substr(cb, e - cb);
		}
		if (!part2) {
			if (r.tok.size() < 5) return;
			r.tok.resize(5);
			r.kind = List;
		} else if (r.tok.size() >= 2 && !isNum(1)) {
			r.kind = Header;
			if (r.tok.size() > 7) r.tok.resize(7);
			header = static_cast<int>(m_rows.size());
		} else {
			if (header < 0 || r.tok.size() < 6) return;
			r.tok.resize(6);
			r.kind = Step;
			r.owner = header;
		}
		m_rows.push_back(std::move(r));
	});
}

std::string VectorFile::field(size_t row, size_t i) const
{
	if (row >= m_rows.size() || i >= m_rows[row].tok.size()) return "";
	const auto [b, e] = m_rows[row].tok[i];
	return m_text.substr(b, e - b);
}

int VectorFile::idOf(size_t row) const
{
	const std::string s = field(row, 0);
	return s.empty() ? -1 : std::atoi(s.c_str());
}

bool VectorFile::setField(size_t row, size_t i, const std::string& value)
{
	if (row >= m_rows.size() || i >= m_rows[row].tok.size() || value.empty()) return false;
	for (char ch : value)
		if (IsSpace(ch) || ch == '\r' || ch == '\n' || ch == '/') return false;
	const auto [b, e] = m_rows[row].tok[i];
	m_text.replace(b, e - b, value);
	parse();
	return true;
}

} // namespace sysconst
