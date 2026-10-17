#include "script_document.h"

#include "../cmdfile/cmd_io.h"

#include <map>

namespace sqedit {

std::vector<std::string> SplitLines(const std::string& text)
{
	std::vector<std::string> out;
	std::size_t start = 0;
	for (;;) {
		const std::size_t nl = text.find('\n', start);
		if (nl == std::string::npos) {
			out.push_back(text.substr(start));
			return out;
		}
		out.push_back(text.substr(start, nl - start));
		start = nl + 1;
	}
}

void ScriptDocument::loadBytes(const std::string& bytes)
{
	m_original = bytes;
	m_lines.clear();
	// CP932 trail bytes are 0x40..0xFC, so 0x0A/0x0D are always real line breaks. NEL (0x85) is a
	// trail byte in these files, never a terminator.
	std::size_t crlf = 0, lf = 0, cr = 0;
	std::size_t start = 0;
	const std::size_t n = bytes.size();
	for (std::size_t i = 0; i < n; ++i) {
		const char c = bytes[i];
		if (c != '\n' && c != '\r') continue;
		RawLine line;
		line.bytes = bytes.substr(start, i - start);
		if (c == '\r' && i + 1 < n && bytes[i + 1] == '\n') {
			line.eol = "\r\n";
			++i;
			++crlf;
		} else {
			line.eol = std::string(1, c);
			(c == '\n' ? lf : cr)++;
		}
		m_lines.push_back(std::move(line));
		start = i + 1;
	}
	RawLine last;
	last.bytes = bytes.substr(start);
	m_lines.push_back(std::move(last));
	for (auto& l : m_lines) l.utf8 = cmdfile::Cp932ToUtf8(l.bytes);
	m_defaultEol = (lf > crlf && lf >= cr) ? "\n" : (cr > crlf && cr > lf) ? "\r" : "\r\n";
}

std::string ScriptDocument::utf8Text() const
{
	std::string out;
	for (std::size_t i = 0; i < m_lines.size(); ++i) {
		if (i) out += '\n';
		out += m_lines[i].utf8;
	}
	return out;
}

std::vector<std::string> ScriptDocument::utf8Lines() const
{
	std::vector<std::string> out;
	out.reserve(m_lines.size());
	for (const auto& l : m_lines) out.push_back(l.utf8);
	return out;
}

bool ScriptDocument::encode(const std::vector<std::string>& in, std::string& out, std::vector<EncodeError>* errors) const
{
	const std::size_t n = in.size(), m = m_lines.size();
	// Align: common prefix and suffix map 1:1 onto original lines; inside the changed middle region,
	// lines whose text matches an original middle line (moved/duplicated lines) reuse its bytes too.
	std::size_t pre = 0;
	while (pre < n && pre < m && in[pre] == m_lines[pre].utf8) ++pre;
	std::size_t suf = 0;
	while (suf < n - pre && suf < m - pre && in[n - 1 - suf] == m_lines[m - 1 - suf].utf8) ++suf;

	std::multimap<std::string, std::size_t> pool;
	for (std::size_t j = pre; j < m - suf; ++j) pool.emplace(m_lines[j].utf8, j);

	out.clear();
	out.reserve(m_original.size() + 64);
	bool ok = true;
	for (std::size_t i = 0; i < n; ++i) {
		const RawLine* src = nullptr;
		if (i < pre) src = &m_lines[i];
		else if (i >= n - suf) src = &m_lines[m - (n - i)];
		else {
			auto it = pool.find(in[i]);
			if (it != pool.end()) { src = &m_lines[it->second]; pool.erase(it); }
		}
		if (src) out += src->bytes;
		else {
			auto enc = cmdfile::Utf8ToCp932(in[i]);
			if (!enc) {
				ok = false;
				if (errors) errors->push_back({static_cast<int>(i), "character with no CP932 form"});
				continue;
			}
			out += *enc;
		}
		if (i + 1 < n) {
			// When the line count is unchanged, a rewritten line keeps the terminator of the line it
			// replaced; inserted lines and the old unterminated last line take the dominant one.
			const RawLine* eolSrc = src;
			if (!eolSrc && n == m) eolSrc = &m_lines[i];
			out += (eolSrc && !eolSrc->eol.empty()) ? eolSrc->eol : m_defaultEol;
		}
	}
	return ok;
}

} // namespace sqedit
