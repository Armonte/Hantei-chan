#ifndef GAME_LINK_MOCK_H_GUARD
#define GAME_LINK_MOCK_H_GUARD
// A stand-in for pchost.dll's dev-link endpoint, for tests and headless captures ONLY (never the game): serves
// \\.\pipe\povertycaster-link-<this pid> and answers Ping / QueryState / QueryStage / SetChar / Reload, plus the
// QueryTag (game_link_proto.h Tag, PovertyCaster mbaacc/link-tag) — so the client's QueryTag decoding, its Unknown
// fallback and the Tag / Team panel's gate probe can be exercised without launching MBAA.exe. The state it reports is a
// TAG match (by default Shiki + Sion vs V.Sion + Miyako) with Team 1 in an assist and Team 2 in its swap cooldown.
//
// [authoring] It also serves every Authoring Mode op (docs/HANTEI_AUTHORING_MODE.md §3.2, §6.3 H9):
//   * QueryCaps / QueryRoster (the roster mirror) / QueryMatchSetup / EndAuthoring;
//   * SetMatchSetup (LinkCommandEx) through a setup state machine: the same validation as the editor (so every
//     refusal status can be produced), then HOT (300 ms, battle + Ready + same mode/scene) or COLD (2 s, through
//     character select and loading), then Ready with inForce = requested, and exactly one LinkReply at the end;
//   * ApplyTuning / QueryTuning answered by HANTEI-CHAN'S OWN RESOLVER over a fixture sidecar tree
//     (Options::tagRoot, a povertycaster\tag folder), with provenance masks, the CSS assist choices on the partner slots, and knobs for
//     env overrides, "hot reload paused", a skewed lever (a game != editor cell) and a foreign lever table hash;
//   * modes: rev1 (QueryCaps Unknown, LinkCommandEx dropped without a reply, like an old DLL's linkFrame), session
//     (sessionFlags set, edits refused) and session + host between rounds (§9.3: ApplyTuning queued for the next round).
// Used by tests/game_link_test.cpp and `game_link_cli mock-dll`.
#include "game_link_proto.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace gamelink {

class MockDll {
public:
	struct Options {
		bool tagOps = true;          // answer QueryTag with a LinkTag (false = Unknown, like today's DLL)
		bool session = false;        // report a netplay session (gate refuses, sessionFlags set, reloadAllowed 0)
		bool inBattle = true;        // scene 1 (else 20 = character select)
		// [authoring]
		bool authoring = true;       // false = a revision-1 DLL: QueryCaps Unknown, LinkCommandEx dropped
		bool team4p = false;         // kCapTeam4P
		std::string tagRoot;         // fixture sidecar tree (a povertycaster\tag folder) resolved for QueryTuning
		uint8_t role = 2;            // with session: 1 host, 2 client, 3 viewer
		bool betweenRounds = false;  // with session: no round running (§9.3)
		int hotMs = 300, coldMs = 2000;
		uint32_t envMask[2] = { 0, 0 };   // levers reported as PCHOST_MBAACC_TAG_* overrides
		bool hotReloadPaused = false;
		int skewLever = -1;          // slot 0 reports this lever one higher than the files say
		uint32_t leverHash = 0;      // 0 = HC's own table hash; anything else = a foreign table (mismatch banner)
		std::string setupHex;        // the setup in force at start ("" = the default TAG match)
		// [game-view] a fake frame producer (docs §12 / §12.1): an animated test pattern (framering::DrawTestFull, a
		// moving camera, CHARS + HUD layers when layered) into Local\povertycaster-frames-<this pid>, every layer
		// checksummed; QueryFrameShare / SetEmbedded / InputInject / SetStageLighting answered. InputInject moves slot 0.
		bool frames = false;
		int frameW = 640, frameH = 480, fps = 60;
		bool layeredAtStart = false;
	};
	explicit MockDll(Options o);
	~MockDll();
	bool Start();                    // false if the pipe cannot be created
	void Stop();
	uint32_t Pid() const;            // the pid the pipe name carries (this process)
	uint32_t Commands() const { return m_commands; }
	uint32_t Probes() const { return m_probes; }
	uint32_t ExDropped() const { return m_exDropped; }
	uint32_t Applies() const { return m_applies; }
	uint32_t SetupsFinished() const { return m_setups; }
	static wire::State FakeState(const Options& o);
	static wire::Tag FakeTag(const Options& o);
	// The setup the mock starts with (the golden TAG setup unless Options::setupHex says otherwise).
	static wire::MatchSetup DefaultSetup();
	uint32_t FramesProduced() const { return m_framesProduced; }
	uint32_t Injects() const { return m_injects; }
	std::string FrameName() const;

private:
	void run();
	void serve(void* pipe);
	wire::State state() const;
	Options m_o;
	std::atomic<bool> m_quit{false};
	std::atomic<uint32_t> m_commands{0}, m_probes{0}, m_exDropped{0}, m_applies{0}, m_setups{0};
	void* m_pipe = nullptr;
	std::thread m_thread;
	// setup state machine (worker thread only)
	wire::SetupState m_ss{};
	uint64_t m_busyUntil = 0;
	uint16_t m_busySeq = 0;
	uint8_t m_busyPath = 0;
	wire::MatchSetup m_pending{};
	std::string m_busyMsg;
	uint32_t m_tuningLoads = 0;
	// [game-view]
	void produce();
	std::thread m_producer;
	void* m_ring = nullptr;                  // framering::Producer
	std::mutex m_fmx;                        // producer thread <-> serve thread
	int m_embedMode = 0;                     // SetEmbedded: 0 real window, 1 full, 2 layered
	struct Held { uint8_t dir = 5, buttons = 0; int frames = 0; } m_held[4];
	int32_t m_p1Offset = 0;                  // slot 0 x moved by injected input (1/128 px)
	std::atomic<uint32_t> m_framesProduced{0}, m_injects{0};
	uint32_t m_lightArgb = 0;
};

} // namespace gamelink

#endif
