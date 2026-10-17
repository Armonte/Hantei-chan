#include "cmd_document.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace cmdfile {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t'; }

// CP932 lead byte: the following byte is a trail byte and may be any of 0x40..0xFC,
// including '[' ']' '\\'. Scanning byte-wise would misread those.
bool IsLeadByte(unsigned char c) { return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

std::size_t FirstNonSpace(const std::string& s, std::size_t from = 0)
{
	while (from < s.size() && IsSpace(s[from])) ++from;
	return from;
}

std::string Trim(const std::string& s, std::size_t begin, std::size_t end)
{
	while (begin < end && (IsSpace(s[begin]) || s[begin] == '\r')) ++begin;
	while (end > begin && (IsSpace(s[end - 1]) || s[end - 1] == '\r')) --end;
	return s.substr(begin, end - begin);
}

bool StartsWith(const std::string& s, std::size_t at, const char* prefix)
{
	for (std::size_t i = 0; prefix[i]; ++i)
		if (at + i >= s.size() || s[at + i] != prefix[i]) return false;
	return true;
}

// Header name for "[Name]" lines (CP932-aware closing bracket search); nullopt otherwise.
std::optional<std::string> HeaderName(const std::string& text)
{
	const std::size_t start = FirstNonSpace(text);
	if (start >= text.size() || text[start] != '[') return std::nullopt;
	for (std::size_t i = start + 1; i < text.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (IsLeadByte(c)) { ++i; continue; }
		if (c == ']') return text.substr(start + 1, i - start - 1);
	}
	return std::nullopt;
}

struct KeyValueSpans {
	std::size_t keyBegin = 0, keyEnd = 0, valueBegin = 0, valueEnd = 0;
};

std::optional<KeyValueSpans> AssignmentSpans(const std::string& text)
{
	const std::size_t limit = std::min(CommentMarker(text), text.size());
	const std::size_t eq = text.find('=');
	if (eq == std::string::npos || eq >= limit) return std::nullopt;
	KeyValueSpans s;
	s.keyBegin = FirstNonSpace(text);
	s.keyEnd = eq;
	while (s.keyEnd > s.keyBegin && IsSpace(text[s.keyEnd - 1])) --s.keyEnd;
	s.valueBegin = FirstNonSpace(text, eq + 1);
	if (s.valueBegin > limit) s.valueBegin = limit;
	s.valueEnd = limit;
	while (s.valueEnd > s.valueBegin && (IsSpace(text[s.valueEnd - 1]) || text[s.valueEnd - 1] == '\r')) --s.valueEnd;
	return s;
}

int ExComFieldFromName(const std::string& name)
{
	if (name == "CheckNum") return XF_CheckNum;
	if (name == "Type") return XF_Type;
	if (name.size() == 2 && name[0] == 'P' && name[1] >= '0' && name[1] <= '4') return XF_P0 + (name[1] - '0');
	return -1;
}

// "NNN_Field" -> (index, digits, field)
struct ExComKey { int index; std::size_t digits; int field; };
std::optional<ExComKey> ParseExComKey(const std::string& key)
{
	const auto underscore = key.find('_');
	if (underscore == std::string::npos || underscore == 0 || underscore > 6) return std::nullopt;
	for (std::size_t i = 0; i < underscore; ++i)
		if (key[i] < '0' || key[i] > '9') return std::nullopt;
	const int field = ExComFieldFromName(key.substr(underscore + 1));
	if (field < 0) return std::nullopt;
	return ExComKey{ std::stoi(key.substr(0, underscore)), underscore, field };
}

std::string Pad3(int value)
{
	std::string s = std::to_string(value);
	while (s.size() < 3) s.insert(s.begin(), '0');
	return s;
}

// Replace [begin, begin+len) with value while keeping the column where the next token
// starts, when the surrounding padding allows it.
void ReplaceAligned(std::string& text, std::size_t begin, std::size_t len, const std::string& value, std::size_t limit)
{
	const std::size_t end = begin + len;
	std::size_t gapEnd = end;
	while (gapEnd < limit && IsSpace(text[gapEnd])) ++gapEnd;
	const bool hasFollowing = gapEnd < limit || (limit < text.size() && gapEnd == limit && gapEnd > end);
	text.replace(begin, len, value);
	if (!hasFollowing) return;
	const std::size_t newEnd = begin + value.size();
	std::size_t gap = gapEnd - end;
	if (value.size() > len) {
		std::size_t grow = value.size() - len;
		std::size_t removable = gap > 1 ? gap - 1 : 0;
		std::size_t remove = std::min(grow, removable);
		// Only shrink runs of plain spaces; tabs keep their own alignment.
		std::size_t i = 0;
		while (i < remove && text[newEnd + i] == ' ') ++i;
		text.erase(newEnd, i);
	} else if (value.size() < len) {
		if (gap > 0 && text[newEnd] == ' ') text.insert(newEnd, len - value.size(), ' ');
	}
}

bool ValidToken(const std::string& value, std::string* error)
{
	if (value.empty()) { if (error) *error = "Value cannot be empty"; return false; }
	for (char c : value)
		if (IsSpace(c) || c == '\r' || c == '\n') { if (error) *error = "Value cannot contain whitespace"; return false; }
	if (value.find("//") != std::string::npos) { if (error) *error = "Value cannot contain //"; return false; }
	return true;
}

bool ValidCommentText(const std::string& value, std::string* error)
{
	if (value.find('\n') != std::string::npos || value.find('\r') != std::string::npos) {
		if (error) *error = "Comment must be a single line";
		return false;
	}
	return true;
}

} // namespace

// ---------------------------------------------------------------------------------------

std::vector<std::pair<std::size_t, std::size_t>> TokenSpans(const std::string& text, std::size_t limit)
{
	std::vector<std::pair<std::size_t, std::size_t>> spans;
	limit = std::min(limit, text.size());
	for (std::size_t pos = 0; pos < limit;) {
		while (pos < limit && (IsSpace(text[pos]) || text[pos] == '\r')) ++pos;
		if (pos >= limit) break;
		const std::size_t start = pos;
		while (pos < limit && !IsSpace(text[pos]) && text[pos] != '\r') ++pos;
		spans.emplace_back(start, pos - start);
	}
	return spans;
}

std::size_t CommentMarker(const std::string& text)
{
	for (std::size_t i = 0; i + 1 < text.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (IsLeadByte(c)) { ++i; continue; }
		if (c == '/' && text[i + 1] == '/') return i;
	}
	return std::string::npos;
}

bool IsDecimal(const std::string& value, bool allowSign)
{
	std::size_t i = 0;
	if (allowSign && !value.empty() && (value[0] == '-' || value[0] == '+')) i = 1;
	if (i >= value.size()) return false;
	for (; i < value.size(); ++i)
		if (value[i] < '0' || value[i] > '9') return false;
	return true;
}

std::optional<long long> ParseInteger(const std::string& value)
{
	if (!IsDecimal(value)) return std::nullopt;
	long long result = 0;
	const char* begin = value.data() + (value[0] == '+' ? 1 : 0);
	const auto parsed = std::from_chars(begin, value.data() + value.size(), result);
	if (parsed.ec != std::errc{}) return std::nullopt;
	return result;
}

const char* CommandFieldName(int field)
{
	static const char* names[CF_Count] = {
		"ID", "Input", "Flagset 1", "Flagset 2", "Pattern", "Meter",
		"Team/solo", "Projectile limit", "Air-dash limit" };
	return field >= 0 && field < CF_Count ? names[field] : "?";
}

const char* ExComFieldName(int field)
{
	static const char* names[XF_Count] = { "CheckNum", "Type", "P0", "P1", "P2", "P3", "P4" };
	return field >= 0 && field < XF_Count ? names[field] : "?";
}

int ExComRecord::firstLine() const
{
	int first = -1;
	for (int l : lineOf) if (l >= 0 && (first < 0 || l < first)) first = l;
	return first;
}

int ExComRecord::lastLine() const
{
	int last = -1;
	for (int l : lineOf) if (l > last) last = l;
	return last;
}

// ---------------------------------------------------------------------------------------

Document Document::parse(const std::string& bytes)
{
	Document doc;
	std::size_t offset = 0;
	while (offset < bytes.size()) {
		const std::size_t nl = bytes.find('\n', offset);
		Line line;
		line.uid = doc.newUid();
		if (nl == std::string::npos) {
			line.text = bytes.substr(offset);
			offset = bytes.size();
		} else {
			std::size_t end = nl;
			if (end > offset && bytes[end - 1] == '\r') { line.eol = "\r\n"; --end; }
			else line.eol = "\n";
			line.text = bytes.substr(offset, end - offset);
			offset = nl + 1;
		}
		doc.m_lines.push_back(std::move(line));
	}
	bool anyLf = false, anyCrLf = false;
	for (const auto& l : doc.m_lines) { if (l.eol == "\n") anyLf = true; if (l.eol == "\r\n") anyCrLf = true; }
	doc.eolStyle = (anyLf && !anyCrLf) ? "\n" : "\r\n";
	doc.reindex();
	return doc;
}

std::string Document::serialize() const
{
	std::string out;
	std::size_t total = 0;
	for (const auto& l : m_lines) total += l.text.size() + l.eol.size();
	out.reserve(total);
	for (const auto& l : m_lines) { out += l.text; out += l.eol; }
	return out;
}

std::optional<std::size_t> Document::lineIndexOf(std::uint64_t uid) const
{
	for (std::size_t i = 0; i < m_lines.size(); ++i)
		if (m_lines[i].uid == uid) return i;
	return std::nullopt;
}

int Document::findCommandRow(std::uint64_t uid) const
{
	for (std::size_t i = 0; i < commands.size(); ++i)
		if (commands[i].uid == uid) return static_cast<int>(i);
	return -1;
}

int Document::findCheckRow(std::uint64_t uid) const
{
	for (std::size_t i = 0; i < checks.size(); ++i)
		if (checks[i].uid == uid) return static_cast<int>(i);
	return -1;
}

std::vector<std::size_t> Document::checksForCommand(const std::string& commandId) const
{
	std::vector<std::size_t> rows;
	const auto id = ParseInteger(commandId);
	if (!id) return rows;
	for (std::size_t i = 0; i < checks.size(); ++i) {
		const auto& v = checks[i].values[XF_CheckNum];
		if (v && ParseInteger(*v) == id) rows.push_back(i);
	}
	return rows;
}

std::vector<std::size_t> Document::commandsWithId(const std::string& commandId) const
{
	std::vector<std::size_t> rows;
	const auto id = ParseInteger(commandId);
	if (!id) return rows;
	for (std::size_t i = 0; i < commands.size(); ++i)
		if (ParseInteger(commands[i].fields[CF_Id]) == id) rows.push_back(i);
	return rows;
}

std::string Document::commandRegionText(std::size_t line) const
{
	const std::string& text = m_lines[line].text;
	const std::size_t marker = CommentMarker(text);
	if (marker == std::string::npos) return Trim(text, 0, text.size());
	return Trim(text, marker + 2, text.size());
}

std::optional<CommandFields> Document::commentedCommand(std::size_t line) const
{
	if (line >= m_lines.size() || m_kinds[line] != LineKind::CommentedCommand) return std::nullopt;
	const std::string& text = m_lines[line].text;
	const std::size_t marker = CommentMarker(text);
	const std::string body = text.substr(marker + 2);
	const auto spans = TokenSpans(body, CommentMarker(body));
	CommandFields f;
	for (int i = 0; i < CF_Count; ++i) f.fields[i] = body.substr(spans[i].first, spans[i].second);
	const std::size_t inner = CommentMarker(body);
	if (inner != std::string::npos) f.comment = Trim(body, inner + 2, body.size());
	return f;
}

void Document::reindex()
{
	commands.clear();
	checks.clear();
	assignments.clear();
	teamChange = TeamChangeData{};
	endLine.reset();
	exComHeaderLine.reset();
	exComNumLine.reset();
	declaredExComCount.reset();
	strayExComKeyLines.clear();
	m_kinds.assign(m_lines.size(), LineKind::Other);

	std::map<int, ExComRecord> exCom;
	std::string section;
	bool sawMbaacSection = false;
	bool awaitingTeamValue = false;

	for (std::size_t i = 0; i < m_lines.size(); ++i) {
		const std::string& text = m_lines[i].text;
		const std::size_t first = FirstNonSpace(text);
		const bool blank = Trim(text, 0, text.size()).empty();
		const bool comment = !blank && StartsWith(text, first, "//");

		if (!endLine) {
			if (blank) { m_kinds[i] = LineKind::Blank; continue; }
			if (comment) {
				const std::string body = text.substr(first + 2);
				const auto spans = TokenSpans(body, CommentMarker(body));
				const bool restorable = spans.size() >= CF_Count &&
					IsDecimal(body.substr(spans[0].first, spans[0].second), false);
				m_kinds[i] = restorable ? LineKind::CommentedCommand : LineKind::Comment;
				continue;
			}
			if (StartsWith(text, first, "END")) { m_kinds[i] = LineKind::End; endLine = i; continue; }
			const auto spans = TokenSpans(text, CommentMarker(text));
			const bool idOk = !spans.empty() && IsDecimal(text.substr(spans[0].first, spans[0].second), false);
			if (spans.size() < CF_Count || !idOk) { m_kinds[i] = LineKind::MalformedCommand; continue; }
			m_kinds[i] = LineKind::Command;
			CommandRecord rec;
			rec.uid = m_lines[i].uid;
			rec.line = i;
			rec.fieldCount = CF_Count;
			rec.extraTokens = spans.size() - CF_Count;
			for (int f = 0; f < CF_Count; ++f) rec.fields[f] = text.substr(spans[f].first, spans[f].second);
			const std::size_t marker = CommentMarker(text);
			if (marker != std::string::npos) { rec.hasComment = true; rec.comment = Trim(text, marker + 2, text.size()); }
			commands.push_back(std::move(rec));
			continue;
		}

		// ---- after END ----
		if (blank) { m_kinds[i] = LineKind::Blank; continue; }
		if (comment) { m_kinds[i] = LineKind::Comment; continue; }
		if (const auto header = HeaderName(text)) {
			m_kinds[i] = LineKind::SectionHeader;
			section = *header;
			awaitingTeamValue = false;
			if (section == "ExComCheck") { if (!exComHeaderLine) exComHeaderLine = i; }
			if (section == "TeamChangeData" && !teamChange.present) {
				teamChange.present = true;
				teamChange.headerLine = i;
				awaitingTeamValue = true;
			}
			if (section == "ExComCheck" || section == "Status" || section == "ThrowParam" || section == "ShieldCounter")
				sawMbaacSection = true;
			continue;
		}
		if (awaitingTeamValue) {
			awaitingTeamValue = false;
			teamChange.valueLine = static_cast<int>(i);
			const auto kv = AssignmentSpans(text);
			teamChange.assignmentForm = kv.has_value();
			std::string values = kv ? text.substr(kv->valueBegin, kv->valueEnd - kv->valueBegin)
				: text.substr(0, std::min(CommentMarker(text), text.size()));
			for (char& c : values) if (c == ',') c = ' ';
			const auto spans = TokenSpans(values, values.size());
			if (spans.size() >= 1) { auto v = ParseInteger(values.substr(spans[0].first, spans[0].second)); if (v) teamChange.tagIn = static_cast<int>(*v); }
			if (spans.size() >= 2) { auto v = ParseInteger(values.substr(spans[1].first, spans[1].second)); if (v) teamChange.tagOut = static_cast<int>(*v); }
		}
		const auto kv = AssignmentSpans(text);
		if (!kv) continue;
		m_kinds[i] = LineKind::Assignment;
		Assignment a;
		a.line = i;
		a.section = section;
		a.key = text.substr(kv->keyBegin, kv->keyEnd - kv->keyBegin);
		a.value = text.substr(kv->valueBegin, kv->valueEnd - kv->valueBegin);
		if (section != "TeamChangeData") sawMbaacSection = true;
		const auto exKey = ParseExComKey(a.key);
		if (section == "ExComCheck") {
			if (a.key == "Num") {
				if (!exComNumLine) { exComNumLine = i; declaredExComCount = ParseInteger(a.value); }
			} else if (exKey) {
				auto& rec = exCom[exKey->index];
				if (rec.uid == 0) { rec.uid = m_lines[i].uid; rec.index = exKey->index; rec.keyDigits = exKey->digits; }
				if (rec.lineOf[exKey->field] < 0) {
					rec.lineOf[exKey->field] = static_cast<int>(i);
					rec.values[exKey->field] = a.value;
					if (exKey->field == XF_CheckNum) {
						const std::size_t marker = CommentMarker(text);
						if (marker != std::string::npos) rec.comment = Trim(text, marker + 2, text.size());
					}
				}
			}
		} else if (exKey) {
			strayExComKeyLines.push_back(i);
		}
		assignments.push_back(std::move(a));
	}

	for (auto& [index, rec] : exCom) checks.push_back(std::move(rec));
	dialect = sawMbaacSection ? Dialect::MBAACC : Dialect::MBAC;
}

Line Document::makeLine(std::string text)
{
	Line l;
	l.uid = newUid();
	l.text = std::move(text);
	l.eol = eolStyle;
	return l;
}

void Document::ensureTerminated(std::size_t line)
{
	if (line < m_lines.size() && m_lines[line].eol.empty()) m_lines[line].eol = eolStyle;
}

// ---------------------------------------------------------------------------------------
// Command region edits

bool Document::setCommandField(std::size_t row, int field, const std::string& value, std::string* error)
{
	if (row >= commands.size() || field < 0 || field >= CF_Count) { if (error) *error = "Command index out of range"; return false; }
	if (!ValidToken(value, error)) return false;
	std::string& text = m_lines[commands[row].line].text;
	const std::size_t limit = std::min(CommentMarker(text), text.size());
	const auto spans = TokenSpans(text, limit);
	if (static_cast<std::size_t>(field) >= spans.size()) { if (error) *error = "Command row has no such column"; return false; }
	if (text.compare(spans[field].first, spans[field].second, value) == 0) return true;
	ReplaceAligned(text, spans[field].first, spans[field].second, value, limit);
	reindex();
	return true;
}

bool Document::setCommandComment(std::size_t row, const std::string& cp932, std::string* error)
{
	if (row >= commands.size()) { if (error) *error = "Command index out of range"; return false; }
	if (!ValidCommentText(cp932, error)) return false;
	if (commands[row].comment == cp932 && (commands[row].hasComment || cp932.empty())) return true;
	std::string& text = m_lines[commands[row].line].text;
	const std::size_t marker = CommentMarker(text);
	if (marker == std::string::npos) {
		std::size_t end = text.size();
		while (end > 0 && IsSpace(text[end - 1])) --end;
		text = text.substr(0, end) + "  // " + cp932;
	} else {
		std::size_t start = marker + 2;
		while (start < text.size() && IsSpace(text[start])) ++start;
		if (start == marker + 2 && !cp932.empty()) { text.insert(start, " "); ++start; }
		text.replace(start, text.size() - start, cp932);
	}
	reindex();
	return true;
}

std::uint64_t Document::insertCommand(std::uint64_t anchorUid, bool after, const CommandFields& f, std::string* error)
{
	for (const auto& v : f.fields) if (!ValidToken(v, error)) return 0;
	if (!ValidCommentText(f.comment, error)) return 0;
	std::size_t at;
	std::optional<std::size_t> templateLine;
	if (anchorUid) {
		const auto idx = lineIndexOf(anchorUid);
		if (!idx || !inCommandRegion(*idx) || m_kinds[*idx] == LineKind::End) { if (error) *error = "Anchor is not in the command list"; return 0; }
		at = *idx + (after ? 1 : 0);
		if (m_kinds[*idx] == LineKind::Command) templateLine = *idx;
	} else {
		at = endLine ? *endLine : m_lines.size();
	}
	if (!templateLine) {
		for (std::size_t i = at; i-- > 0;) if (m_kinds[i] == LineKind::Command) { templateLine = i; break; }
		if (!templateLine) for (std::size_t i = at; i < m_lines.size() && inCommandRegion(i); ++i)
			if (m_kinds[i] == LineKind::Command) { templateLine = i; break; }
	}
	std::string text;
	if (templateLine) {
		// Reuse the neighbour's column layout so the new row lines up with it.
		text = m_lines[*templateLine].text;
		const std::size_t marker = CommentMarker(text);
		if (marker != std::string::npos) {
			std::size_t end = marker;
			while (end > 0 && IsSpace(text[end - 1])) --end;
			text.erase(end);
		}
		auto spans = TokenSpans(text, text.size());
		if (spans.size() > CF_Count) { text.erase(spans[CF_Count].first); while (!text.empty() && IsSpace(text.back())) text.pop_back(); spans.resize(CF_Count); }
		for (int i = CF_Count; i-- > 0;) ReplaceAligned(text, spans[i].first, spans[i].second, f.fields[i], text.size());
	} else {
		text = f.fields[0] + "  " + f.fields[1] + "  " + f.fields[2] + " " + f.fields[3] + "  " +
			f.fields[4] + "  " + f.fields[5] + "  " + f.fields[6] + " " + f.fields[7] + " " + f.fields[8];
	}
	text += "  // " + f.comment;
	if (at > 0) ensureTerminated(at - 1);
	Line line = makeLine(std::move(text));
	const std::uint64_t uid = line.uid;
	m_lines.insert(m_lines.begin() + at, std::move(line));
	reindex();
	return uid;
}

std::uint64_t Document::insertCommentLines(std::uint64_t anchorUid, bool after, const std::vector<std::string>& cp932Lines, std::string* error)
{
	if (cp932Lines.empty()) { if (error) *error = "Nothing to insert"; return 0; }
	for (const auto& l : cp932Lines) if (!ValidCommentText(l, error)) return 0;
	std::size_t at;
	if (anchorUid) {
		const auto idx = lineIndexOf(anchorUid);
		if (!idx || !inCommandRegion(*idx) || m_kinds[*idx] == LineKind::End) { if (error) *error = "Anchor is not in the command list"; return 0; }
		at = *idx + (after ? 1 : 0);
	} else {
		at = endLine ? *endLine : m_lines.size();
	}
	if (at > 0) ensureTerminated(at - 1);
	std::uint64_t firstUid = 0;
	for (std::size_t i = 0; i < cp932Lines.size(); ++i) {
		Line line = makeLine(cp932Lines[i].empty() ? std::string("//") : "// " + cp932Lines[i]);
		if (!firstUid) firstUid = line.uid;
		m_lines.insert(m_lines.begin() + at + i, std::move(line));
	}
	reindex();
	return firstUid;
}

bool Document::deleteLines(const std::vector<std::uint64_t>& uids, std::string* error)
{
	std::vector<std::size_t> idx;
	for (auto uid : uids) {
		const auto i = lineIndexOf(uid);
		if (!i) { if (error) *error = "Line no longer exists"; return false; }
		if (!inCommandRegion(*i) || m_kinds[*i] == LineKind::End) { if (error) *error = "Only command-list lines can be deleted here"; return false; }
		idx.push_back(*i);
	}
	std::sort(idx.begin(), idx.end());
	idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
	for (auto it = idx.rbegin(); it != idx.rend(); ++it) m_lines.erase(m_lines.begin() + *it);
	reindex();
	return true;
}

bool Document::commentOut(const std::vector<std::uint64_t>& commandUids, std::string* error)
{
	for (auto uid : commandUids) {
		const auto i = lineIndexOf(uid);
		if (!i || m_kinds[*i] != LineKind::Command) { if (error) *error = "Only active commands can be commented out"; return false; }
	}
	for (auto uid : commandUids) {
		std::string& text = m_lines[*lineIndexOf(uid)].text;
		text.insert(FirstNonSpace(text), "// ");
	}
	reindex();
	return true;
}

bool Document::uncomment(std::uint64_t lineUid, std::string* error)
{
	const auto i = lineIndexOf(lineUid);
	if (!i || m_kinds[*i] != LineKind::CommentedCommand) { if (error) *error = "Line is not a commented-out command"; return false; }
	std::string& text = m_lines[*i].text;
	const std::size_t marker = FirstNonSpace(text);
	text.erase(marker, 2);
	if (marker < text.size() && text[marker] == ' ') text.erase(marker, 1);
	reindex();
	return true;
}

bool Document::replaceCommentLine(std::uint64_t lineUid, const std::string& cp932, std::string* error)
{
	const auto i = lineIndexOf(lineUid);
	if (!i || !inCommandRegion(*i) || (m_kinds[*i] != LineKind::Comment && m_kinds[*i] != LineKind::CommentedCommand)) {
		if (error) *error = "Line is not a comment in the command list";
		return false;
	}
	if (!ValidCommentText(cp932, error)) return false;
	std::string& text = m_lines[*i].text;
	const std::size_t marker = FirstNonSpace(text);
	std::size_t bodyStart = marker + 2;
	std::string pad = " ";
	if (bodyStart < text.size() && IsSpace(text[bodyStart])) {
		std::size_t e = bodyStart;
		while (e < text.size() && IsSpace(text[e])) ++e;
		pad = text.substr(bodyStart, e - bodyStart);
	}
	text = text.substr(0, bodyStart) + (cp932.empty() ? std::string() : pad + cp932);
	reindex();
	return true;
}

bool Document::moveLines(const std::vector<std::uint64_t>& uids, std::uint64_t targetUid, bool after, std::string* error)
{
	if (uids.empty()) return true;
	if (std::find(uids.begin(), uids.end(), targetUid) != uids.end()) return true;
	const auto target = lineIndexOf(targetUid);
	if (!target || !inCommandRegion(*target) || m_kinds[*target] == LineKind::End) { if (error) *error = "Drop target is not in the command list"; return false; }
	std::vector<std::size_t> idx;
	for (auto uid : uids) {
		const auto i = lineIndexOf(uid);
		if (!i || !inCommandRegion(*i) || m_kinds[*i] == LineKind::End) { if (error) *error = "Only command-list lines can be moved"; return false; }
		idx.push_back(*i);
	}
	std::sort(idx.begin(), idx.end());
	idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
	std::vector<Line> moving;
	for (auto i : idx) moving.push_back(m_lines[i]);
	for (auto& l : moving) if (l.eol.empty()) l.eol = eolStyle;
	for (auto it = idx.rbegin(); it != idx.rend(); ++it) m_lines.erase(m_lines.begin() + *it);
	std::size_t at = *lineIndexOf(targetUid) + (after ? 1 : 0);
	if (at > 0) ensureTerminated(at - 1);
	m_lines.insert(m_lines.begin() + at, moving.begin(), moving.end());
	reindex();
	return true;
}

// ---------------------------------------------------------------------------------------
// ExComCheck edits

bool Document::renumberExCom(std::string* error)
{
	(void)error;
	if (!exComHeaderLine) return true;
	std::size_t end = *exComHeaderLine + 1;
	while (end < m_lines.size() && m_kinds[end] != LineKind::SectionHeader) ++end;
	std::map<int, int> mapping;
	int next = 0;
	for (std::size_t i = *exComHeaderLine + 1; i < end; ++i) {
		if (m_kinds[i] != LineKind::Assignment) continue;
		const auto kv = AssignmentSpans(m_lines[i].text);
		const auto key = ParseExComKey(m_lines[i].text.substr(kv->keyBegin, kv->keyEnd - kv->keyBegin));
		if (!key) continue;
		if (!mapping.count(key->index)) mapping[key->index] = next++;
		std::string& text = m_lines[i].text;
		text.replace(kv->keyBegin, key->digits, Pad3(mapping[key->index]));
	}
	reindex();
	const std::string count = std::to_string(checks.size());
	if (exComNumLine) {
		std::string& text = m_lines[*exComNumLine].text;
		const auto kv = AssignmentSpans(text);
		text.replace(kv->valueBegin, kv->valueEnd - kv->valueBegin, count);
	} else {
		ensureTerminated(*exComHeaderLine);
		m_lines.insert(m_lines.begin() + *exComHeaderLine + 1, makeLine("Num = " + count));
	}
	reindex();
	return true;
}

bool Document::setCheckValue(std::size_t row, int field, const std::optional<std::string>& value, std::string* error)
{
	if (row >= checks.size() || field < 0 || field >= XF_Count) { if (error) *error = "ExComCheck index out of range"; return false; }
	const ExComRecord& rec = checks[row];
	if (value && !ValidToken(*value, error)) return false;
	if (!value) {
		if (field <= XF_Type) { if (error) *error = "CheckNum and Type are required"; return false; }
		if (rec.lineOf[field] < 0) return true;
		const std::size_t line = static_cast<std::size_t>(rec.lineOf[field]);
		if (line + 1 == m_lines.size() && line > 0) m_lines[line - 1].eol = m_lines[line].eol;
		m_lines.erase(m_lines.begin() + line);
		reindex();
		return true;
	}
	if (rec.lineOf[field] >= 0) {
		std::string& text = m_lines[rec.lineOf[field]].text;
		const auto kv = AssignmentSpans(text);
		if (text.compare(kv->valueBegin, kv->valueEnd - kv->valueBegin, *value) == 0) return true;
		text.replace(kv->valueBegin, kv->valueEnd - kv->valueBegin, *value);
		reindex();
		return true;
	}
	// Author a missing field in engine order: after the nearest earlier field of the
	// record, otherwise before the nearest later one.
	std::size_t at = static_cast<std::size_t>(rec.lastLine()) + 1;
	bool placed = false;
	for (int f = field - 1; f >= 0 && !placed; --f)
		if (rec.lineOf[f] >= 0) { at = static_cast<std::size_t>(rec.lineOf[f]) + 1; placed = true; }
	for (int f = field + 1; f < XF_Count && !placed; ++f)
		if (rec.lineOf[f] >= 0) { at = static_cast<std::size_t>(rec.lineOf[f]); placed = true; }
	std::string prefix = rec.keyDigits == 3 ? Pad3(rec.index) : std::to_string(rec.index);
	ensureTerminated(at - 1);
	m_lines.insert(m_lines.begin() + at, makeLine(prefix + "_" + ExComFieldName(field) + " = " + *value));
	reindex();
	return true;
}

bool Document::setCheckComment(std::size_t row, const std::string& cp932, std::string* error)
{
	if (row >= checks.size()) { if (error) *error = "ExComCheck index out of range"; return false; }
	if (!ValidCommentText(cp932, error)) return false;
	const int line = checks[row].lineOf[XF_CheckNum];
	if (line < 0) { if (error) *error = "Row has no CheckNum line"; return false; }
	if (checks[row].comment == cp932) return true;
	std::string& text = m_lines[line].text;
	const std::size_t marker = CommentMarker(text);
	if (marker == std::string::npos) {
		if (!cp932.empty()) text += " // " + cp932;
	} else if (cp932.empty()) {
		std::size_t cut = marker;
		while (cut > 0 && IsSpace(text[cut - 1])) --cut;
		text.erase(cut);
	} else {
		std::size_t start = marker + 2;
		while (start < text.size() && IsSpace(text[start])) ++start;
		if (start == marker + 2) { text.insert(start, " "); ++start; }
		text.replace(start, text.size() - start, cp932);
	}
	reindex();
	return true;
}

std::uint64_t Document::insertCheck(int anchorRow, bool after, const ExComFields& fields, std::string* error)
{
	if (!fields.values[XF_CheckNum] || !fields.values[XF_Type]) { if (error) *error = "CheckNum and Type are required"; return 0; }
	for (const auto& v : fields.values) if (v && !ValidToken(*v, error)) return 0;
	if (!ValidCommentText(fields.comment, error)) return 0;
	if (anchorRow >= static_cast<int>(checks.size())) { if (error) *error = "ExComCheck index out of range"; return 0; }

	if (!exComHeaderLine) {
		if (!endLine) { if (error) *error = "The file has no END line; cannot add an [ExComCheck] section"; return 0; }
		if (!m_lines.empty()) ensureTerminated(m_lines.size() - 1);
		if (!m_lines.empty() && !Trim(m_lines.back().text, 0, m_lines.back().text.size()).empty())
			m_lines.push_back(makeLine(""));
		m_lines.push_back(makeLine("[ExComCheck]"));
		m_lines.push_back(makeLine("Num = 0"));
		reindex();
	}
	std::size_t at;
	if (anchorRow >= 0) {
		const auto& anchor = checks[anchorRow];
		at = after ? static_cast<std::size_t>(anchor.lastLine()) + 1 : static_cast<std::size_t>(anchor.firstLine());
	} else if (!checks.empty()) {
		int last = -1;
		for (const auto& c : checks) last = std::max(last, c.lastLine());
		at = static_cast<std::size_t>(last) + 1;
	} else {
		at = (exComNumLine ? *exComNumLine : *exComHeaderLine) + 1;
	}
	int temp = 0;
	for (const auto& c : checks) temp = std::max(temp, c.index + 1);
	const std::string prefix = Pad3(temp) + "_";
	if (at > 0) ensureTerminated(at - 1);
	std::vector<Line> authored;
	for (int f = 0; f < XF_Count; ++f) {
		if (!fields.values[f]) continue;
		std::string text = prefix + ExComFieldName(f) + " = " + *fields.values[f];
		if (f == XF_CheckNum && !fields.comment.empty()) text += " // " + fields.comment;
		authored.push_back(makeLine(std::move(text)));
	}
	const std::uint64_t uid = authored.front().uid;
	m_lines.insert(m_lines.begin() + at, authored.begin(), authored.end());
	reindex();
	if (!renumberExCom(error)) return 0;
	return uid;
}

bool Document::deleteChecks(const std::vector<std::size_t>& rows, std::string* error)
{
	std::vector<std::size_t> lines;
	for (auto r : rows) {
		if (r >= checks.size()) { if (error) *error = "ExComCheck index out of range"; return false; }
		for (int l : checks[r].lineOf) if (l >= 0) lines.push_back(static_cast<std::size_t>(l));
	}
	std::sort(lines.begin(), lines.end());
	lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
	for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
		if (*it + 1 == m_lines.size() && *it > 0) m_lines[*it - 1].eol = m_lines[*it].eol;
		m_lines.erase(m_lines.begin() + *it);
	}
	reindex();
	return renumberExCom(error);
}

// Blank and comment lines directly above a row belong to it: they move with the row, so a
// row's heading comment is never separated from it.
std::size_t Document::checkBlockStart(std::size_t row) const
{
	std::size_t start = static_cast<std::size_t>(checks[row].firstLine());
	const std::size_t floor = exComNumLine ? *exComNumLine + 1 : (exComHeaderLine ? *exComHeaderLine + 1 : 0);
	while (start > floor && (m_kinds[start - 1] == LineKind::Blank || m_kinds[start - 1] == LineKind::Comment)) --start;
	return start;
}

bool Document::moveCheck(std::size_t sourceRow, std::size_t targetRow, bool after, std::string* error)
{
	if (sourceRow >= checks.size() || targetRow >= checks.size()) { if (error) *error = "ExComCheck index out of range"; return false; }
	if (sourceRow == targetRow) return true;
	const std::uint64_t targetUid = checks[targetRow].uid;
	std::vector<std::size_t> lines;
	for (std::size_t l = checkBlockStart(sourceRow); l < static_cast<std::size_t>(checks[sourceRow].firstLine()); ++l) lines.push_back(l);
	for (int l : checks[sourceRow].lineOf) if (l >= 0) lines.push_back(static_cast<std::size_t>(l));
	std::sort(lines.begin(), lines.end());
	lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
	std::vector<Line> moving;
	for (auto l : lines) moving.push_back(m_lines[l]);
	for (auto& l : moving) if (l.eol.empty()) l.eol = eolStyle;
	for (auto it = lines.rbegin(); it != lines.rend(); ++it) m_lines.erase(m_lines.begin() + *it);
	reindex();
	const int target = findCheckRow(targetUid);
	if (target < 0) { if (error) *error = "Target row vanished"; return false; }
	const std::size_t at = after ? static_cast<std::size_t>(checks[target].lastLine()) + 1 : checkBlockStart(static_cast<std::size_t>(target));
	if (at > 0) ensureTerminated(at - 1);
	m_lines.insert(m_lines.begin() + at, moving.begin(), moving.end());
	reindex();
	return renumberExCom(error);
}

// ---------------------------------------------------------------------------------------

bool Document::setTeamChange(int tagIn, int tagOut, std::string* error)
{
	if (!teamChange.present) { if (error) *error = "The file has no [TeamChangeData] section"; return false; }
	if (teamChange.valueLine < 0) {
		const std::size_t at = teamChange.headerLine + 1;
		ensureTerminated(teamChange.headerLine);
		m_lines.insert(m_lines.begin() + at, makeLine(std::to_string(tagIn) + " " + std::to_string(tagOut)));
		reindex();
		return true;
	}
	std::string& text = m_lines[teamChange.valueLine].text;
	std::size_t begin = 0, limit = std::min(CommentMarker(text), text.size());
	if (const auto kv = AssignmentSpans(text)) { begin = kv->valueBegin; limit = kv->valueEnd; }
	// Numeric runs inside the value area.
	std::vector<std::pair<std::size_t, std::size_t>> nums;
	for (std::size_t i = begin; i < limit;) {
		if ((text[i] >= '0' && text[i] <= '9') || (text[i] == '-' && i + 1 < limit && text[i + 1] >= '0' && text[i + 1] <= '9')) {
			const std::size_t s = i++;
			while (i < limit && text[i] >= '0' && text[i] <= '9') ++i;
			nums.emplace_back(s, i - s);
		} else ++i;
	}
	if (nums.size() < 2) { if (error) *error = "TeamChangeData value line does not hold two numbers"; return false; }
	text.replace(nums[1].first, nums[1].second, std::to_string(tagOut));
	text.replace(nums[0].first, nums[0].second, std::to_string(tagIn));
	reindex();
	return true;
}

} // namespace cmdfile
