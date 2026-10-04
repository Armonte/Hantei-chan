#include "cmd_notes.h"

#include "../../third_party/json/json.hpp"

#include <algorithm>

namespace cmdfile {

namespace {

using json = nlohmann::json;

bool IsAscii(const std::string& s)
{
	return std::all_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
}

std::string CommandHint(const CommandRecord& c) { return IsAscii(c.fields[CF_Input]) ? c.fields[CF_Input] : std::string(); }
std::string CheckHint(const ExComRecord& c) { return c.values[XF_Type] && IsAscii(*c.values[XF_Type]) ? "Type " + *c.values[XF_Type] : std::string(); }

} // namespace

bool Notes::operator==(const Notes& o) const
{
	if (byUid.size() != o.byUid.size() || orphans.size() != o.orphans.size()) return false;
	for (const auto& [k, v] : byUid) {
		auto it = o.byUid.find(k);
		if (it == o.byUid.end() || it->second != v) return false;
	}
	for (std::size_t i = 0; i < orphans.size(); ++i)
		if (orphans[i].key != o.orphans[i].key || orphans[i].text != o.orphans[i].text) return false;
	return true;
}

std::string NotesPathFor(const std::string& commandFilePath) { return commandFilePath + ".notes.json"; }

std::string CommandNoteKey(const Document& doc, std::size_t row)
{
	const auto& id = doc.commands[row].fields[CF_Id];
	const auto same = doc.commandsWithId(id);
	const auto n = std::find(same.begin(), same.end(), row) - same.begin();
	const auto parsed = ParseInteger(id);
	return "command:" + (parsed ? std::to_string(*parsed) : id) + "#" + std::to_string(n);
}

std::string CheckNoteKey(const Document& doc, std::size_t row)
{
	const auto& num = doc.checks[row].values[XF_CheckNum];
	const std::string id = num ? *num : std::string("?");
	std::size_t n = 0;
	for (std::size_t i = 0; i < row; ++i)
		if (doc.checks[i].values[XF_CheckNum] == num) ++n;
	const auto parsed = ParseInteger(id);
	return "excom:" + (parsed ? std::to_string(*parsed) : id) + "#" + std::to_string(n);
}

bool ParseNotes(const std::string& text, const Document& doc, Notes& out, std::string* error)
{
	out = Notes{};
	json root;
	try {
		root = json::parse(text);
	} catch (const std::exception& e) {
		if (error) *error = std::string("Notes file is not valid JSON: ") + e.what();
		return false;
	}
	if (!root.is_object() || root.value("format", std::string()) != "hantei-cmdfile-notes") {
		if (error) *error = "Notes file has an unknown format (expected \"hantei-cmdfile-notes\").";
		return false;
	}
	if (root.value("version", 0) != 1) {
		if (error) *error = "Notes file version " + std::to_string(root.value("version", 0)) + " is not supported.";
		return false;
	}
	std::map<std::string, std::uint64_t> keyToUid;
	for (std::size_t r = 0; r < doc.commands.size(); ++r) keyToUid[CommandNoteKey(doc, r)] = doc.commands[r].uid;
	for (std::size_t r = 0; r < doc.checks.size(); ++r) keyToUid[CheckNoteKey(doc, r)] = doc.checks[r].uid;
	try {
		for (const char* list : { "notes", "orphaned" }) {
			if (!root.contains(list)) continue;
			if (!root[list].is_array()) { if (error) *error = std::string("\"") + list + "\" must be an array."; return false; }
			for (const auto& item : root[list]) {
				if (!item.is_object() || !item.contains("key") || !item.contains("text") || !item["key"].is_string() || !item["text"].is_string()) {
					if (error) *error = std::string("Malformed entry in \"") + list + "\".";
					return false;
				}
				OrphanNote note{ item["key"].get<std::string>(), item.value("hint", std::string()), item["text"].get<std::string>() };
				auto it = keyToUid.find(note.key);
				if (it != keyToUid.end() && !out.byUid.count(it->second)) out.byUid[it->second] = note.text;
				else out.orphans.push_back(std::move(note));
			}
		}
	} catch (const std::exception& e) {
		if (error) *error = std::string("Notes file could not be read: ") + e.what();
		return false;
	}
	return true;
}

std::string SerializeNotes(const Document& doc, const Notes& notes)
{
	json root = json::object();
	root["format"] = "hantei-cmdfile-notes";
	root["version"] = 1;
	json list = json::array();
	json orphaned = json::array();
	for (std::size_t r = 0; r < doc.commands.size(); ++r) {
		auto it = notes.byUid.find(doc.commands[r].uid);
		if (it == notes.byUid.end() || it->second.empty()) continue;
		list.push_back({ { "key", CommandNoteKey(doc, r) }, { "hint", CommandHint(doc.commands[r]) }, { "text", it->second } });
	}
	for (std::size_t r = 0; r < doc.checks.size(); ++r) {
		auto it = notes.byUid.find(doc.checks[r].uid);
		if (it == notes.byUid.end() || it->second.empty()) continue;
		list.push_back({ { "key", CheckNoteKey(doc, r) }, { "hint", CheckHint(doc.checks[r]) }, { "text", it->second } });
	}
	// A note on a line that is no longer an active record (for example a commented-out
	// command) is kept as orphaned rather than lost.
	for (const auto& [uid, text] : notes.byUid) {
		if (text.empty() || doc.findCommandRow(uid) >= 0 || doc.findCheckRow(uid) >= 0) continue;
		const auto line = doc.lineIndexOf(uid);
		if (!line) continue; // the record itself was deleted by the user
		std::string hint;
		if (const auto cmd = doc.commentedCommand(*line)) hint = "commented command " + cmd->fields[CF_Id] + " " + (IsAscii(cmd->fields[CF_Input]) ? cmd->fields[CF_Input] : "");
		orphaned.push_back({ { "key", "line:" + std::to_string(*line + 1) }, { "hint", hint }, { "text", text } });
	}
	for (const auto& o : notes.orphans)
		orphaned.push_back({ { "key", o.key }, { "hint", o.hint }, { "text", o.text } });
	root["notes"] = std::move(list);
	root["orphaned"] = std::move(orphaned);
	return root.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

void PruneNotes(const Document& doc, Notes& notes)
{
	for (auto it = notes.byUid.begin(); it != notes.byUid.end();) {
		if (it->second.empty() || !doc.lineIndexOf(it->first)) it = notes.byUid.erase(it);
		else ++it;
	}
}

} // namespace cmdfile
