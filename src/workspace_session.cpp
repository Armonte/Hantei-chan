#include "workspace_session.h"

#include <algorithm>
#include <set>

bool WorkspaceSession::add(Id content, Id hostId, bool selectContent)
{
	if (content == 0 || contains(content)) return false;
	auto& host = m_hosts[hostId];
	host.tabs.push_back(content);
	m_owner[content] = hostId;
	if (selectContent || host.active == 0) host.active = content;
	return true;
}

bool WorkspaceSession::select(Id hostId, Id content)
{
	const auto ownerIt = m_owner.find(content);
	if (ownerIt == m_owner.end() || ownerIt->second != hostId) return false;
	m_hosts[hostId].active = content;
	return true;
}

void WorkspaceSession::eraseFromHost(Id hostId, Id content)
{
	auto hostIt = m_hosts.find(hostId);
	if (hostIt == m_hosts.end()) return;
	auto& host = hostIt->second;
	const auto tabIt = std::find(host.tabs.begin(), host.tabs.end(), content);
	if (tabIt == host.tabs.end()) return;
	const auto index = static_cast<std::size_t>(tabIt - host.tabs.begin());
	host.tabs.erase(tabIt);
	if (host.active == content) {
		if (host.tabs.empty()) host.active = 0;
		else host.active = host.tabs[std::min(index, host.tabs.size() - 1)];
	}
	if (hostId != MainHost && host.tabs.empty()) m_hosts.erase(hostIt);
}

bool WorkspaceSession::move(Id content, Id targetHost, std::optional<std::size_t> targetIndex,
	bool selectMoved)
{
	const auto ownerIt = m_owner.find(content);
	if (ownerIt == m_owner.end()) return false;
	const Id sourceHost = ownerIt->second;
	eraseFromHost(sourceHost, content);
	auto& destination = m_hosts[targetHost];
	const auto index = std::min(targetIndex.value_or(destination.tabs.size()), destination.tabs.size());
	destination.tabs.insert(destination.tabs.begin() + index, content);
	m_owner[content] = targetHost;
	if (selectMoved || destination.active == 0) destination.active = content;
	return true;
}

bool WorkspaceSession::reorder(Id hostId, Id content, std::size_t targetIndex)
{
	const auto ownerIt = m_owner.find(content);
	auto hostIt = m_hosts.find(hostId);
	if (ownerIt == m_owner.end() || ownerIt->second != hostId || hostIt == m_hosts.end()) return false;
	auto& tabs = hostIt->second.tabs;
	const auto source = std::find(tabs.begin(), tabs.end(), content);
	if (source == tabs.end()) return false;
	const Id active = hostIt->second.active;
	const std::size_t sourceIndex = static_cast<std::size_t>(source - tabs.begin());
	tabs.erase(source);
	// targetIndex describes the insertion boundary in the original list.
	if (sourceIndex < targetIndex && targetIndex > 0) --targetIndex;
	targetIndex = std::min(targetIndex, tabs.size());
	tabs.insert(tabs.begin() + targetIndex, content);
	hostIt->second.active = active;
	return true;
}

bool WorkspaceSession::close(Id content)
{
	const auto ownerIt = m_owner.find(content);
	if (ownerIt == m_owner.end()) return false;
	const Id hostId = ownerIt->second;
	eraseFromHost(hostId, content);
	m_owner.erase(content);
	return true;
}

bool WorkspaceSession::detach(Id content, Id detachedHost)
{
	if (detachedHost == MainHost || m_hosts.count(detachedHost) != 0) return false;
	return move(content, detachedHost, 0, true);
}

bool WorkspaceSession::attach(Id content, std::optional<std::size_t> targetIndex)
{
	return move(content, MainHost, targetIndex, true);
}

const WorkspaceSession::Host* WorkspaceSession::host(Id hostId) const
{
	const auto found = m_hosts.find(hostId);
	return found == m_hosts.end() ? nullptr : &found->second;
}

std::optional<WorkspaceSession::Id> WorkspaceSession::owner(Id content) const
{
	const auto found = m_owner.find(content);
	return found == m_owner.end() ? std::nullopt : std::optional<Id>(found->second);
}

bool WorkspaceSession::valid() const
{
	std::set<Id> seen;
	for (const auto& [hostId, host] : m_hosts) {
		if (host.tabs.empty() && hostId != MainHost) return false;
		if (host.active != 0 && std::find(host.tabs.begin(), host.tabs.end(), host.active) == host.tabs.end()) return false;
		for (const auto content : host.tabs) {
			if (content == 0 || !seen.insert(content).second) return false;
			const auto ownerIt = m_owner.find(content);
			if (ownerIt == m_owner.end() || ownerIt->second != hostId) return false;
		}
	}
	return seen.size() == m_owner.size();
}

std::vector<WorkspaceSession::Id> WorkspaceSession::hostIds() const
{
	std::vector<Id> ids;
	ids.reserve(m_hosts.size());
	for (const auto& entry : m_hosts) ids.push_back(entry.first);
	return ids;
}
