// Assist slots — see tag_assist.h.
#include "tag_assist.h"
#include "../cmdfile/cmd_document.h"

#include <cstdlib>

namespace tagtune {

std::vector<CommandInfo> ParseCommands(const std::string& bytes, std::string (*toUtf8)(const std::string&))
{
	const cmdfile::Document doc = cmdfile::Document::parse(bytes);
	std::vector<CommandInfo> out;
	for (const cmdfile::CommandRecord& r : doc.commands) {
		const auto id = cmdfile::ParseInteger(r.fields[cmdfile::CF_Id]);
		if (!id) continue;
		bool dup = false;
		for (const CommandInfo& c : out) dup = dup || c.id == (int)*id;
		if (dup) continue;   // the engine keeps the first definition
		CommandInfo c;
		c.id = (int)*id;
		c.input = r.fields[cmdfile::CF_Input];
		c.pattern = (int)cmdfile::ParseInteger(r.fields[cmdfile::CF_Pattern]).value_or(0);
		c.meter = (int)cmdfile::ParseInteger(r.fields[cmdfile::CF_Meter]).value_or(0);
		const std::string& f1 = r.fields[cmdfile::CF_Flags1];
		c.standing = !f1.empty() && f1.back() == '1';
		c.air = f1.size() >= 2 && f1[f1.size() - 2] == '1';
		c.exComChecks = (int)doc.checksForCommand(r.fields[cmdfile::CF_Id]).size();
		c.name = toUtf8 ? toUtf8(r.comment) : r.comment;
		c.line = (int)r.line + 1;
		out.push_back(std::move(c));
	}
	return out;
}

const CommandInfo* FindCommand(const std::vector<CommandInfo>& cmds, int id)
{
	for (const CommandInfo& c : cmds) if (c.id == id) return &c;
	return nullptr;
}

bool IsDefaultCandidate(const CommandInfo& c)
{
	if (c.meter != 0 || c.pattern <= 0 || !c.standing) return false;
	size_t k = 0;
	while (k < c.input.size() && c.input[k] >= '0' && c.input[k] <= '9') ++k;
	return k >= 2 && k + 1 == c.input.size() && c.input[k] == 'A';
}

int PickDefaultCommand(const std::vector<CommandInfo>& cmds)
{
	int best = -1;
	for (int i = 0; i < (int)cmds.size(); ++i)
		if (IsDefaultCandidate(cmds[i]) && (best < 0 || cmds[i].id < cmds[best].id)) best = i;
	return best;
}

const CommandInfo* CommandForMotion(const std::vector<CommandInfo>& cmds, const std::string& motion)
{
	int32_t packed = 0;
	if (!PackMotion(motion, packed)) return nullptr;
	const std::string canon = UnpackMotion(packed);   // upper-case, as the engine compares
	const CommandInfo* best = nullptr;
	for (const CommandInfo& c : cmds) {
		std::string in = c.input;
		for (char& ch : in) if (ch >= 'a' && ch <= 'f') ch = (char)(ch - 'a' + 'A');
		if (in == canon && (!best || c.id < best->id)) best = &c;
	}
	return best;
}

std::string ValidateMotion(const std::string& m)
{
	if (m.empty()) return "empty";
	if (m.size() > 6) return "at most 6 characters (the game packs a motion into 6 x 5 bits)";
	for (char c : m)
		if (!MotionCode(c)) return std::string("'") + c + "' is not an input character (0-9, A-F, +, V)";
	int32_t p = 0;
	if (!PackMotion(m, p)) return "not a motion";
	return {};
}

const char* AssistModeName(AssistMode m)
{
	return m == AssistMode::Pattern ? "pattern" : m == AssistMode::Command ? "command" : m == AssistMode::Motion ? "motion" : "default";
}

SlotLevers SlotLeverIndices(int slot)
{
	const std::string d = std::to_string(kAssistDirs[slot < 0 || slot > 4 ? 0 : slot]);
	SlotLevers s;
	s.entry = FindLever("assist." + d + ".entry");
	s.pattern = FindLever("assist." + d + ".pattern");
	s.command = FindLever("assist." + d + ".command");
	s.motion = FindLever("assist." + d + ".motion");
	return s;
}

AssistAction ResolveAssistSlot(const Values& v, int slot)
{
	struct L { int32_t entry, pattern, command, motion; };
	auto get = [&](int s) { const SlotLevers i = SlotLeverIndices(s); return L{ v[i.entry], v[i.pattern], v[i.command], v[i.motion] }; };
	auto has = [](const L& l) { return l.pattern > 0 || l.command >= 0 || l.motion != 0; };
	const L own = get(slot), five = get(0);
	const L& a = has(own) ? own : five;
	const int from = has(own) ? slot : has(five) ? 0 : -1;
	AssistAction r;
	r.entry = v[FindLever("assistEntry")];
	r.fromSlot = from;
	if (from >= 0) {
		if (a.pattern > 0) { r.mode = AssistMode::Pattern; r.value = a.pattern; }
		else if (a.command >= 0) { r.mode = AssistMode::Command; r.value = a.command; }
		else { r.mode = AssistMode::Motion; r.value = a.motion; }
	}
	if (own.entry > 0) r.entry = own.entry - 1;
	else if (five.entry > 0) r.entry = five.entry - 1;
	return r;
}

ActionView DescribeAction(const AssistAction& a, const std::vector<CommandInfo>& cmds)
{
	ActionView v;
	switch (a.mode) {
	case AssistMode::Pattern:
		v.text = "pattern " + std::to_string(a.value);
		v.pattern = a.value;
		break;
	case AssistMode::Command: {
		const CommandInfo* c = FindCommand(cmds, a.value);
		if (!c) { v.text = "command " + std::to_string(a.value) + " (not in the _c.txt)"; v.problem = !cmds.empty(); break; }
		v.text = "command " + std::to_string(c->id) + " (" + c->input + ") -> pattern " + std::to_string(c->pattern);
		if (c->meter) v.text += " $" + std::to_string(c->meter);
		v.pattern = c->pattern;
		break;
	}
	case AssistMode::Motion: {
		const std::string m = UnpackMotion(a.value);
		const CommandInfo* c = CommandForMotion(cmds, m);
		if (!c) { v.text = "motion " + m + " (no command has this input)"; v.problem = !cmds.empty(); break; }
		v.text = "motion " + m + " -> command " + std::to_string(c->id) + " -> pattern " + std::to_string(c->pattern);
		if (c->meter) v.text += " $" + std::to_string(c->meter);
		v.pattern = c->pattern;
		break;
	}
	case AssistMode::Default: {
		const int d = PickDefaultCommand(cmds);
		if (cmds.empty()) { v.text = "character default (load the _c.txt to see it)"; break; }
		if (d < 0) { v.text = "character default: none (no meterless motion+A special)"; v.problem = true; break; }
		const CommandInfo& c = cmds[d];
		v.text = "default: command " + std::to_string(c.id) + " (" + c.input + ") -> pattern " + std::to_string(c.pattern);
		if (c.exComChecks) v.text += " (conditional: " + std::to_string(c.exComChecks) + " ExComCheck)";
		v.pattern = c.pattern;
		break;
	}
	}
	return v;
}

} // namespace tagtune
