#include "winquote_model.h"

#include "../misc.h" // WriteFileAtomic

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace winquote {

namespace {

bool IsLead(unsigned char c) { return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

std::string FormatLine(const Quote& q)
{
	char head[32];
	snprintf(head, sizeof head, "%3d %2d %2d\t", q.opponent, q.mode, q.face);
	return head + q.voice + " " + q.text;
}

// "<int> <int> <int><ws><voice> <text>"; false for anything else (comments, END, blanks).
bool ParseLine(const std::string& l, Quote& q)
{
	const char* s = l.c_str();
	char* e = nullptr;
	int v[3];
	for (int k = 0; k < 3; k++) {
		while (*s == ' ' || *s == '\t') s++;
		if (!(*s >= '0' && *s <= '9')) return false;
		v[k] = static_cast<int>(std::strtol(s, &e, 10));
		s = e;
	}
	if (*s != ' ' && *s != '\t') return false;
	while (*s == ' ' || *s == '\t') s++;
	const char* sp = s;
	while (*sp && *sp != ' ' && *sp != '\t') sp++;
	q.opponent = v[0];
	q.mode = v[1];
	q.face = v[2];
	q.voice.assign(s, sp);
	if (*sp) sp++;
	q.text = sp;
	return !q.voice.empty();
}

} // namespace

bool File::load(const std::string& path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	return loadText(ss.str());
}

bool File::loadText(const std::string& raw)
{
	m_text = m_saved = raw;
	split();
	parse();
	return true;
}

bool File::save(const std::string& path)
{
	if (!WriteFileAtomic(path.c_str(), m_text.data(), m_text.size())) return false;
	m_saved = m_text;
	return true;
}

void File::split()
{
	m_lines.clear();
	m_eol = m_text.find("\r\n") != std::string::npos || m_text.find('\n') == std::string::npos ? "\r\n" : "\n";
	size_t b = 0;
	while (b < m_text.size()) {
		size_t e = m_text.find('\n', b);
		if (e == std::string::npos) { m_lines.push_back(m_text.substr(b)); m_trailingEol = false; return; }
		size_t ce = e;
		if (ce > b && m_text[ce - 1] == '\r') ce--;
		m_lines.push_back(m_text.substr(b, ce - b));
		b = e + 1;
	}
	m_trailingEol = true;
}

void File::join()
{
	std::string out;
	for (size_t i = 0; i < m_lines.size(); i++) {
		out += m_lines[i];
		if (i + 1 < m_lines.size() || m_trailingEol) out += m_eol;
	}
	m_text = out;
}

void File::parse()
{
	m_quotes.clear();
	for (size_t i = 0; i < m_lines.size(); i++) {
		if (m_lines[i].rfind("END", 0) == 0) break;
		Quote q;
		if (!ParseLine(m_lines[i], q)) continue;
		q.line = i;
		m_quotes.push_back(std::move(q));
	}
}

void File::set(size_t i, const Quote& q)
{
	if (i >= m_quotes.size()) return;
	m_lines[m_quotes[i].line] = FormatLine(q);
	join();
	parse();
}

void File::add(size_t after, const Quote& q)
{
	size_t at = m_lines.size();
	if (after < m_quotes.size()) at = m_quotes[after].line + 1;
	else
		for (size_t k = 0; k < m_lines.size(); k++)
			if (m_lines[k].rfind("END", 0) == 0) { at = k; break; }
	m_lines.insert(m_lines.begin() + static_cast<long>(at), FormatLine(q));
	join();
	parse();
}

void File::remove(size_t i)
{
	if (i >= m_quotes.size()) return;
	m_lines.erase(m_lines.begin() + static_cast<long>(m_quotes[i].line));
	join();
	parse();
}

int File::widestLine(const std::string& t)
{
	int best = 0, cur = 0;
	for (size_t i = 0; i < t.size(); i++) {
		const unsigned char c = static_cast<unsigned char>(t[i]);
		if (IsLead(c) && i + 1 < t.size()) {
			if (c == 0x81 && static_cast<unsigned char>(t[i + 1]) == 0xA1) { best = std::max(best, cur); cur = 0; i++; continue; } // ■
			cur += 2;
			i++;
		} else cur += 1;
	}
	return std::max(best, cur);
}

} // namespace winquote
