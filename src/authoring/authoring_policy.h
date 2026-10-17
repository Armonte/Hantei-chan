#ifndef AUTHORING_POLICY_H_GUARD
#define AUTHORING_POLICY_H_GUARD
// [authoring] What the Authoring workspace may do right now, from what the link reports (docs/HANTEI_AUTHORING_MODE.md
// §3.5, §3.6, §5.1, §5.6, §9.3). Pure: tests/game_link_test.cpp drives it against the mock DLL.
//
//   not linked             files editable; no Load in game / ApplyTuning
//   rev-1 DLL              files editable; Setup read-only (no LinkCommandEx is ever sent)
//   unknown game id        nothing written (no lever table for it)
//   lever table mismatch   values shown raw, NOTHING written ("never writes a lever it cannot name")
//   session                locked: no Load, no ApplyTuning, no saves unless the user unlocked (saves only)
//   session, host between rounds   edits save and ApplyTuning is sent (queued for the next round, §8.7)
//   setup running          Load in game disabled while authState is ApplyingHot / Rebuilding
#include "../game_link.h"

#include <string>

namespace authoring {

struct LinkPolicy {
	bool linked = false;
	bool rev1 = false;             // pchost.dll predates Authoring Mode
	bool authoring = false;        // kCapSetup
	bool gameKnown = true;         // a lever table exists for caps.gameId
	bool leverTableOk = true;      // caps hash == ours (or caps unknown)
	bool sessionLocked = false;
	bool hostBetweenRounds = false;
	bool setupBusy = false;
	bool canLoadInGame = false;
	bool canApplyTuning = false;
	bool canWriteFiles = true;
	int severity = 0;              // 0 fine, 1 note, 2 warning, 3 locked / error
	std::string banner;
};
LinkPolicy DecidePolicy(const gamelink::Snapshot& s, bool unlockedInSession);

} // namespace authoring

#endif
