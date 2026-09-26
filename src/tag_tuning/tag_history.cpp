// [authoring] workspace undo history — see tag_history.h.
#include "tag_history.h"

namespace tagtune {

bool TagHistory::Commit(const std::string& label, const SidecarWorkspace::Snapshot& before, const SidecarWorkspace& now)
{
	HistoryEntry e;
	e.label = label;
	const SidecarWorkspace::Snapshot after = now.Take();
	for (const auto& kv : after) {
		auto it = before.find(kv.first);
		const std::string b = it == before.end() ? std::string() : it->second;
		if (b != kv.second) e.changes.push_back({ kv.first, b, kv.second });
	}
	for (const auto& kv : before)
		if (!after.count(kv.first) && !kv.second.empty()) e.changes.push_back({ kv.first, kv.second, std::string() });
	if (e.changes.empty()) return false;
	e.seq = ++m_seq;
	m_entries.resize(m_pos);
	m_entries.push_back(std::move(e));
	if (m_entries.size() > cap) m_entries.erase(m_entries.begin());
	m_pos = m_entries.size();
	return true;
}

std::vector<DocId> TagHistory::Undo(SidecarWorkspace& ws)
{
	std::vector<DocId> out;
	if (!CanUndo()) return out;
	const HistoryEntry& e = m_entries[--m_pos];
	for (const auto& c : e.changes) { ws.Doc(c.doc).ini.SetText(c.before); out.push_back(c.doc); }
	return out;
}

std::vector<DocId> TagHistory::Redo(SidecarWorkspace& ws)
{
	std::vector<DocId> out;
	if (!CanRedo()) return out;
	const HistoryEntry& e = m_entries[m_pos++];
	for (const auto& c : e.changes) { ws.Doc(c.doc).ini.SetText(c.after); out.push_back(c.doc); }
	return out;
}

} // namespace tagtune
