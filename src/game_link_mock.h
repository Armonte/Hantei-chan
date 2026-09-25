#ifndef GAME_LINK_MOCK_H_GUARD
#define GAME_LINK_MOCK_H_GUARD
// A stand-in for pchost.dll's dev-link endpoint, for tests and headless captures ONLY (never the game): serves
// \\.\pipe\povertycaster-link-<this pid> and answers Ping / QueryState / QueryStage / SetChar / Reload, plus the
// PROPOSED QueryTag (game_link_proto.h Tag) — so the client's QueryTag decoding, its Unknown fallback and the Tag /
// Team panel's gate probe can be exercised without launching MBAA.exe. The state it reports is a fixed TAG match
// (Shiki + Sion vs V.Sion + Miyako) with Team 1 in an assist and Team 2 in its swap cooldown.
// Used by tests/game_link_test.cpp and `game_link_cli mock-dll`. docs/HANTEI_TAG_PANEL.md §6.
#include "game_link_proto.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace gamelink {

class MockDll {
public:
	struct Options {
		bool tagOps = true;          // answer QueryTag with a LinkTag (false = Unknown, like today's DLL)
		bool session = false;        // report a netplay session (gate refuses, sessionFlags set, reloadAllowed 0)
		bool inBattle = true;        // scene 1 (else 20 = character select)
	};
	explicit MockDll(Options o);
	~MockDll();
	bool Start();                    // false if the pipe cannot be created
	void Stop();
	uint32_t Pid() const;            // the pid the pipe name carries (this process)
	uint32_t Commands() const { return m_commands; }
	uint32_t Probes() const { return m_probes; }
	static wire::State FakeState(const Options& o);
	static wire::Tag FakeTag(const Options& o);

private:
	void run();
	Options m_o;
	std::atomic<bool> m_quit{false};
	std::atomic<uint32_t> m_commands{0}, m_probes{0};
	void* m_pipe = nullptr;
	std::thread m_thread;
};

} // namespace gamelink

#endif
