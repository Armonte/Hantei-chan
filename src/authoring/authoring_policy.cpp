// [authoring] link policy — see authoring_policy.h.
#include "authoring_policy.h"
#include "authoring_model.h"

#include <cstdio>

namespace authoring {

LinkPolicy DecidePolicy(const gamelink::Snapshot& s, bool unlocked)
{
	LinkPolicy p;
	p.linked = s.connected;
	if (!s.connected) {
		p.severity = 1;
		p.banner = "not linked: edits are saved to the files; the game re-reads them within a second when it runs offline";
		return p;
	}
	if (s.capsUnknown) {
		p.rev1 = true;
		p.severity = 2;
		p.banner = "pchost.dll predates Authoring Mode - Setup / Load disabled; tuning edits go to the files, the game reads them if it supports them";
	} else if (s.haveCaps) {
		p.authoring = (s.caps.caps & gamelink::wire::kCapSetup) != 0;
		const GameTable* t = FindGameTable(s.gameId);
		if (!t) {
			p.gameKnown = false;
			p.canWriteFiles = false;
			p.severity = 3;
			p.banner = "the linked game is '" + s.gameId + "': this Hantei-chan has no lever table for it - nothing is written";
			return p;
		}
		if (s.caps.leverTableHash != t->leverTableHash) {
			p.leverTableOk = false;
			p.canWriteFiles = false;
			p.severity = 3;
			char b[200];
			std::snprintf(b, sizeof b, "pchost.dll lever table differs from this Hantei-chan build (pc %08X vs hc %08X) - update one of them",
			              (unsigned)s.caps.leverTableHash, (unsigned)t->leverTableHash);
			p.banner = b;
		}
	}
	if (s.SessionLive()) {
		if (s.HostBetweenRounds()) {
			p.hostBetweenRounds = true;
			p.canApplyTuning = p.leverTableOk && (s.haveCaps && (s.caps.caps & gamelink::wire::kCapTuning));
			if (p.severity < 2) { p.severity = 2; p.banner = "host between rounds: edits apply to both players at the next round start"; }
			return p;
		}
		p.sessionLocked = true;
		p.canWriteFiles = p.leverTableOk && p.gameKnown && unlocked;
		p.severity = 3;
		const char* sha = s.haveTuning ? s.tuning.sha : s.haveTag ? s.tag.sha : "";
		p.banner = std::string("SESSION - host tuning adopted") + (sha[0] ? std::string(" (sha ") + sha + ")" : std::string()) +
		           (unlocked ? "; files unlocked: saves apply after the session" : "; editing locked");
		return p;
	}
	p.setupBusy = s.haveSetup && (s.setup.authState == (uint8_t)gamelink::wire::AuthState::ApplyingHot ||
	                              s.setup.authState == (uint8_t)gamelink::wire::AuthState::Rebuilding);
	p.canLoadInGame = p.authoring && !p.setupBusy && !p.rev1;
	p.canApplyTuning = p.leverTableOk && s.haveCaps && (s.caps.caps & gamelink::wire::kCapTuning) != 0;
	if (p.severity == 0 && s.haveCaps && !(s.caps.caps & gamelink::wire::kCapSidecars)) {
		p.severity = 2;
		p.banner = "this pchost.dll does not read povertycaster\\tag\\ (no sidecar support): it still reads tag_tuning.ini";
	}
	return p;
}

} // namespace authoring
