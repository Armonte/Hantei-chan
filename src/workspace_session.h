#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Authoritative, presentation-independent ownership for workspace content.
// Host 0 is the main window; every other host represents one detached window.
// Ported from Gonptechan EX (drop 1abc27f9, bigorados); content ids here are
// CharacterView ids. Pure standard C++ (tests/workspace_session_test.cpp).
class WorkspaceSession {
public:
	using Id = std::uint64_t;
	static constexpr Id MainHost = 0;
	struct Host { std::vector<Id> tabs; Id active = 0; };

	bool add(Id content, Id host = MainHost, bool select = true);
	bool select(Id host, Id content);
	bool move(Id content, Id targetHost, std::optional<std::size_t> targetIndex = std::nullopt,
		bool selectMoved = true);
	bool reorder(Id host, Id content, std::size_t targetIndex);
	bool close(Id content);
	bool detach(Id content, Id detachedHost);
	bool attach(Id content, std::optional<std::size_t> targetIndex = std::nullopt);
	void clear() { m_hosts.clear(); m_hosts.emplace(MainHost, Host{}); m_owner.clear(); }
	const Host* host(Id host) const;
	std::optional<Id> owner(Id content) const;
	bool contains(Id content) const { return m_owner.count(content) != 0; }
	bool valid() const;
	// Every host id in ascending order (the main host first).
	std::vector<Id> hostIds() const;

private:
	void eraseFromHost(Id host, Id content);
	std::map<Id, Host> m_hosts{{MainHost, {}}};
	std::map<Id, Id> m_owner;
};
