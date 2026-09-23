#include "cmd_validate.h"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>

namespace cmdfile {

namespace {

const ExComTypeInfo kTypes[] = {
	{ 0, "Own box vs owned-effect box", false,
		"Passes when one of this character's boxes overlaps a box of a live effect it owns.",
		{ "Own boxes", "Effect box", "Unused", "Unused", "Unused" },
		{ "0 = the collision box, 1 = the hurtboxes (up to 8). Any other value fails the check.",
		  "Box #(P1 + 8) on the owned effect, i.e. special box P1. Must be 1 or more; 0 fails.",
		  "Not read for Type 0.", "Not read.", "Not read." } },
	{ 1, "Range point vs owned-effect box", false,
		"Passes when a horizontal range from this character overlaps a box of a live effect it owns (Ryougi 60-67).",
		{ "Range", "Effect box", "Range shape", "Unused", "Unused" },
		{ "Distance: P0 >> 6 box units. By default the range spans -(P0>>6) .. 2*(P0>>6) in facing space.",
		  "Box #(P1 + 8) on the owned effect (special box P1).",
		  "0 = default (-r .. 2r). 1 = 0 .. r. 2 = -r .. r. 1 and 2 swap when facing left.",
		  "Not read.", "Not read." } },
	{ 2, "Custom variable compare (BOF ABI v1)", true,
		"Extended Melty / BOF executables only. Vanilla MBAACC fails this type.",
		{ "Variable index", "Comparison", "Compare value", "Owner", "Reserved" },
		{ "Custom variable index 0..1023 (signed 16-bit bank).",
		  "0 ==, 1 !=, 2 <, 3 <=, 4 >, 5 >= (variable on the left).",
		  "Signed constant -32768..32767.",
		  "0 self, 1 opponent, 2 P1, 3 P2.",
		  "Must be 0." } },
	{ 3, "Input hold / charge (BOF)", true,
		"Extended Melty / BOF executables only. Vanilla MBAACC fails this type.",
		{ "Policy/partitions/stream", "Accepted values", "Required frames", "Partition gap", "Completion window" },
		{ "policy*1000 + partitions*10 + stream. Policy 0 transform, 1 preserve, 2 break; partitions 0..63 or 99; stream 0 direction, 1..6 A..F.",
		  "Unsigned bit mask: bit N accepts input value N.",
		  "Accumulated hold frames, 1..60000.",
		  "Largest gap inside the hold, 0..60000.",
		  "Final completion window, 0..60000." } },
};

void Add(std::vector<Diagnostic>& out, Severity s, int line, std::uint64_t uid, std::string msg)
{
	out.push_back({ s, line, uid, std::move(msg) });
}

bool AllIn(const std::string& s, std::size_t from, const char* allowed)
{
	for (std::size_t i = from; i < s.size(); ++i)
		if (!std::char_traits<char>::find(allowed, std::char_traits<char>::length(allowed), s[i])) return false;
	return true;
}

} // namespace

std::vector<const ExComTypeInfo*> ExComTypes(ExtensionProfile profile)
{
	std::vector<const ExComTypeInfo*> out;
	for (const auto& t : kTypes)
		if (!t.extendedOnly || profile == ExtensionProfile::Extended) out.push_back(&t);
	return out;
}

const ExComTypeInfo* FindExComType(int type)
{
	for (const auto& t : kTypes) if (t.type == type) return &t;
	return nullptr;
}

std::string SummarizeCheck(const ExComRecord& c)
{
	auto v = [&](int f) { return c.values[f].value_or("-"); };
	const auto type = c.values[XF_Type] ? ParseInteger(*c.values[XF_Type]) : std::nullopt;
	if (!type) return "Missing or invalid Type";
	switch (*type) {
	case 0: return std::string(v(XF_P0) == "1" ? "Hurtboxes" : "Collision box") + " vs owned-effect special box " + v(XF_P1);
	case 1: {
		std::string shape = c.values[XF_P2] ? " (shape " + v(XF_P2) + ")" : std::string();
		return "Range " + v(XF_P0) + " vs owned-effect special box " + v(XF_P1) + shape;
	}
	case 2: {
		static const char* ops[] = { "==", "!=", "<", "<=", ">", ">=" };
		static const char* owners[] = { "Self", "Opponent", "P1", "P2" };
		const auto op = c.values[XF_P1] ? ParseInteger(*c.values[XF_P1]) : std::nullopt;
		const auto ow = c.values[XF_P3] ? ParseInteger(*c.values[XF_P3]) : std::nullopt;
		return std::string(ow && *ow >= 0 && *ow < 4 ? owners[*ow] : "?") + " CV" + v(XF_P0) + " " +
			(op && *op >= 0 && *op < 6 ? ops[*op] : "?") + " " + v(XF_P2);
	}
	case 3: return "Hold " + v(XF_P2) + "f (selector " + v(XF_P0) + ", mask " + v(XF_P1) + ")";
	default: return "Unknown type " + std::to_string(*type);
	}
}

std::vector<Diagnostic> Validate(const Document& doc, ExtensionProfile profile)
{
	std::vector<Diagnostic> out;
	const bool ext = profile == ExtensionProfile::Extended;
	const Severity bofRule = ext ? Severity::Error : Severity::Warning;

	// ---- document level ----
	if (!doc.endLine)
		Add(out, Severity::Error, 0, 0, "No END line: the engine reads commands until a line starting with END.");
	for (std::size_t i = 0; i < doc.lines().size(); ++i) {
		if (doc.lines()[i].eol == "\n") {
			const bool inCommands = !doc.endLine || i < *doc.endLine;
			Add(out, inCommands ? Severity::Error : Severity::Warning, static_cast<int>(i + 1), doc.lines()[i].uid,
				"Line ends with LF only. MBAACC splits command lines at CR, so this line merges with the next.");
			break;
		}
	}

	// ---- commands ----
	std::map<long long, std::vector<std::size_t>> byId;
	for (std::size_t i = 0; i < doc.lines().size(); ++i) {
		if (doc.kind(i) == LineKind::MalformedCommand)
			Add(out, Severity::Warning, static_cast<int>(i + 1), doc.lines()[i].uid,
				"Not a full command row, but MBAACC still reads it as a command (missing columns become 0). Comment it out with // if it is not a command.");
	}
	for (std::size_t row = 0; row < doc.commands.size(); ++row) {
		const auto& c = doc.commands[row];
		const int line = static_cast<int>(c.line + 1);
		auto err = [&](Severity s, const std::string& m) { Add(out, s, line, c.uid, "Command " + c.fields[CF_Id] + ": " + m); };
		const auto id = ParseInteger(c.fields[CF_Id]);
		if (!id || *id < 0) err(Severity::Error, "ID must be a non-negative decimal number.");
		else byId[*id].push_back(row);
		if (c.extraTokens)
			err(Severity::Info, std::to_string(c.extraTokens) + " extra column(s) after the air-dash limit are ignored by the engine.");
		{
			const std::string& text = doc.lines()[c.line].text;
			const auto spans = TokenSpans(text, std::min(CommentMarker(text), text.size()));
			if (spans.size() > 2) {
				const std::size_t gapBegin = spans[1].first + spans[1].second;
				if (text.find('\t', gapBegin) < spans[2].first)
					err(Severity::Warning, "A tab follows the input. The engine ends the input at the first space, so the tab becomes part of it.");
			}
		}
		for (int f : { CF_Flags1, CF_Flags2 }) {
			const std::string& flags = c.fields[f];
			if (flags.size() != 8 || !IsDecimal(flags, false)) {
				err(bofRule, std::string(CommandFieldName(f)) + " should be exactly 8 digits; the engine reads it as a decimal number, so other lengths shift every bit.");
				continue;
			}
			const bool classOk = f == CF_Flags2 ? (flags[0] == '0' || flags[0] == '1') : (flags[0] >= '0' && flags[0] <= '2');
			if (!classOk)
				err(bofRule, f == CF_Flags1 ? "Flagset 1 cancel class (first digit) should be 0, 1 or 2." : "Flagset 2 digits should be 0 or 1.");
			if (!AllIn(flags, 1, "01"))
				err(bofRule, std::string(CommandFieldName(f)) + " bit digits should be 0 or 1.");
		}
		for (int f : { CF_Pattern, CF_Meter })
			if (!ParseInteger(c.fields[f])) err(Severity::Error, std::string(CommandFieldName(f)) + " must be a decimal number.");
		for (int f : { CF_TeamSolo, CF_Projectile, CF_AirDash }) {
			const auto v = ParseInteger(c.fields[f]);
			if (!v) { err(Severity::Error, std::string(CommandFieldName(f)) + " must be a decimal number."); continue; }
			if (*v < 0 || *v > 255) err(Severity::Warning, std::string(CommandFieldName(f)) + " is stored as a byte (0..255).");
			else if (f == CF_TeamSolo && *v > 2) err(Severity::Warning, "Team/solo should be 0 (both), 1 (team only) or 2 (solo only).");
		}
	}
	for (const auto& [id, rows] : byId) {
		if (rows.size() < 2) continue;
		std::ostringstream m;
		m << "Command ID " << id << " is defined " << rows.size() << " times (lines";
		for (auto r : rows) m << ' ' << doc.commands[r].line + 1;
		m << "). MBAACC keeps the first definition and ignores the rest.";
		Add(out, ext ? Severity::Error : Severity::Warning, static_cast<int>(doc.commands[rows[1]].line + 1), doc.commands[rows[1]].uid, m.str());
	}

	// ---- ExComCheck ----
	if (!doc.checks.empty() || doc.exComNumLine) {
		const long long count = static_cast<long long>(doc.checks.size());
		if (!doc.exComNumLine)
			Add(out, Severity::Error, doc.exComHeaderLine ? static_cast<int>(*doc.exComHeaderLine + 1) : 0, 0,
				"[ExComCheck] has rows but no Num line; the engine reads 0 rows.");
		else if (!doc.declaredExComCount)
			Add(out, Severity::Error, static_cast<int>(*doc.exComNumLine + 1), 0, "ExComCheck Num is not a number.");
		else if (*doc.declaredExComCount != count)
			Add(out, Severity::Error, static_cast<int>(*doc.exComNumLine + 1), 0,
				"ExComCheck Num = " + std::to_string(*doc.declaredExComCount) + " but " + std::to_string(count) +
				" row(s) exist. Rows past Num are ignored; missing rows read as CheckNum 0 / Type 0.");
	}
	for (std::size_t row = 0; row < doc.checks.size(); ++row) {
		const auto& c = doc.checks[row];
		const int line = c.firstLine() + 1;
		const std::string label = "ExComCheck " + std::to_string(c.index) + ": ";
		auto err = [&](Severity s, const std::string& m) { Add(out, s, line, c.uid, label + m); };
		if (c.index != static_cast<int>(row))
			err(Severity::Error, "Rows must be numbered 000.." + std::to_string(doc.checks.size() - 1) + " without gaps; this row would not be read.");
		if (c.keyDigits != 3)
			err(Severity::Error, "Key prefix must have exactly 3 digits (the engine looks up \"%03d_CheckNum\").");
		if (!c.values[XF_CheckNum]) err(Severity::Error, "CheckNum is missing.");
		if (!c.values[XF_Type]) err(Severity::Error, "Type is missing.");
		for (int f = 0; f < XF_Count; ++f)
			if (c.values[f] && !ParseInteger(*c.values[f]))
				err(Severity::Error, std::string(ExComFieldName(f)) + " must be a decimal number.");
		if (c.values[XF_CheckNum]) {
			if (const auto id = ParseInteger(*c.values[XF_CheckNum]); id && doc.commandsWithId(*c.values[XF_CheckNum]).empty())
				err(Severity::Warning, "No active command has ID " + *c.values[XF_CheckNum] + ".");
		}
		const auto type = c.values[XF_Type] ? ParseInteger(*c.values[XF_Type]) : std::nullopt;
		if (!type) continue;
		auto p = [&](int f) -> std::optional<long long> { return c.values[f] ? ParseInteger(*c.values[f]) : std::nullopt; };
		if (*type == 0) {
			const auto p0 = p(XF_P0).value_or(0), p1 = p(XF_P1).value_or(0);
			if (p0 != 0 && p0 != 1) err(Severity::Warning, "Type 0 P0 must be 0 (collision box) or 1 (hurtboxes); other values always fail.");
			if (p1 < 1) err(Severity::Warning, "Type 0 P1 (effect special box) must be 1 or more; 0 always fails.");
		} else if (*type == 1) {
			// All values are accepted by the engine.
		} else if ((*type == 2 || *type == 3) && ext) {
			bool all = true;
			for (int f = XF_P0; f <= XF_P4; ++f) all = all && c.values[f].has_value();
			if (!all) { err(Severity::Error, "Type " + std::to_string(*type) + " requires P0..P4."); continue; }
			if (*type == 2) {
				const auto idx = p(XF_P0), op = p(XF_P1), cmp = p(XF_P2), owner = p(XF_P3), res = p(XF_P4);
				if (!idx || *idx < 0 || *idx > 1023) err(Severity::Error, "Type 2 variable index must be 0..1023.");
				if (!op || *op < 0 || *op > 5) err(Severity::Error, "Type 2 comparison must be 0..5.");
				if (!cmp || *cmp < -32768 || *cmp > 32767) err(Severity::Error, "Type 2 compare value must be -32768..32767.");
				if (!owner || *owner < 0 || *owner > 3) err(Severity::Error, "Type 2 owner must be 0..3.");
				if (!res || *res != 0) err(Severity::Error, "Type 2 P4 is reserved and must be 0.");
			} else {
				const auto sel = p(XF_P0), mask = p(XF_P1), req = p(XF_P2), gap = p(XF_P3), win = p(XF_P4);
				if (!sel || *sel < 0) err(Severity::Error, "Type 3 selector must be non-negative.");
				else {
					const long long policy = *sel / 1000, parts = (*sel % 1000) / 10, stream = *sel % 10;
					if (policy > 2) err(Severity::Error, "Type 3 facing policy must be 0..2.");
					if (stream > 6) err(Severity::Error, "Type 3 input stream must be 0..6.");
					if (!(parts <= 63 || parts == 99)) err(Severity::Error, "Type 3 partition count must be 0..63 or 99.");
				}
				if (!mask || *mask <= 0 || *mask > static_cast<long long>(std::numeric_limits<std::uint32_t>::max()))
					err(Severity::Error, "Type 3 accepted-value mask must be a non-zero unsigned 32-bit value.");
				if (!req || *req < 1 || *req > 60000) err(Severity::Error, "Type 3 required frames must be 1..60000.");
				if (!gap || *gap < 0 || *gap > 60000) err(Severity::Error, "Type 3 partition gap must be 0..60000.");
				if (!win || *win < 0 || *win > 60000) err(Severity::Error, "Type 3 completion window must be 0..60000.");
			}
		} else if (*type == 2 || *type == 3) {
			err(Severity::Error, "Type " + std::to_string(*type) + " is an Extended Melty / BOF type. Vanilla MBAACC fails it, so the command can never be used. Switch to the Extended profile if you target a BOF executable.");
		} else {
			err(Severity::Error, "Unknown Type " + std::to_string(*type) + ": the engine fails the check, so the command can never be used.");
		}
	}
	for (auto l : doc.strayExComKeyLines)
		Add(out, Severity::Warning, static_cast<int>(l + 1), doc.lines()[l].uid,
			"ExComCheck key outside [ExComCheck]. The engine looks keys up globally, so this line can shadow the real row.");

	// ---- TeamChangeData ----
	if (doc.teamChange.present) {
		if (!doc.teamChange.tagIn || !doc.teamChange.tagOut)
			Add(out, Severity::Warning, static_cast<int>(doc.teamChange.headerLine + 1), 0, "[TeamChangeData] has no tag-in/tag-out pattern pair.");
		if (doc.dialect == Dialect::MBAACC)
			Add(out, Severity::Info, static_cast<int>(doc.teamChange.headerLine + 1), 0,
				"[TeamChangeData] is MBAC data. MBAACC has no reader for it (tag-in/out comes from nowhere in MBAACC).");
	}

	std::stable_sort(out.begin(), out.end(), [](const Diagnostic& a, const Diagnostic& b) {
		return static_cast<int>(a.severity) > static_cast<int>(b.severity);
	});
	return out;
}

bool HasErrors(const std::vector<Diagnostic>& d)
{
	return std::any_of(d.begin(), d.end(), [](const Diagnostic& x) { return x.severity == Severity::Error; });
}

std::size_t CountSeverity(const std::vector<Diagnostic>& d, Severity s)
{
	return static_cast<std::size_t>(std::count_if(d.begin(), d.end(), [&](const Diagnostic& x) { return x.severity == s; }));
}

} // namespace cmdfile
