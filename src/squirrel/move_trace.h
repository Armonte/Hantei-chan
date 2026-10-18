#ifndef SQUIRREL_MOVE_TRACE_H_GUARD
#define SQUIRREL_MOVE_TRACE_H_GUARD

// Pattern tracing: after a character's scripts loaded (chara_load.h), calls each <CHR>_MoveTable move's Init and FrameUpdate
// closure in the sandbox and records every pattern the move sets (BMvTbl.SetPattern / SetPattern_Reserve / any
// *VorpalPattern native) and every BMvTbl.JumpFrameID (a move that stays in the current pattern). Resolves the moves whose pattern the source only names through a variable (`usepat`, a template's
// mvname.slice(...), a computed string). Game independent: the native names come from the API table, the move table is
// <CHR>_MoveTable for UNI2/MBTL and the DFCI/UNIB loaders alike.
//
// The natives are stubs, so state the engine owns (frame counters, hit results, gauge, the opponent) reads as 0 / a fresh
// instance: a branch on such state takes its "0" side, and a closure that needs a real value fails. Both are reported per
// move (`error`), never guessed.

#include "chara_load_types.h"

#include <squirrel.h>

#include <string>
#include <vector>

namespace sqedit {

struct PatternSet {
	std::string slot;     // "Init" / "FrameUpdate"
	std::string native;   // "BMvTbl.SetPattern"
	int num = -1;         // pattern number set (-1: the engine keeps the current pattern); for JumpFrameID the frame ID
	std::string code;     // the code name when the native got a string / {pat="..."} (resolved through ctx.patternByCode)
};

struct MoveTrace {
	std::string move;
	std::vector<PatternSet> sets;
	std::vector<std::string> errors;   // "<slot>: <runtime error>"
	std::vector<std::string> slots;    // closure slots the move has (what the engine can call)
};

// Called from the stub native for every native call: records it when a trace is running and the native sets a pattern.
void MoveTrace_OnNative(HSQUIRRELVM v, const std::string& qualifiedName);

// Inside a stub, while a trace runs: pushes the stand-in object for a `value` return the API has no class for (fields read
// 0, push() returns 0). False (nothing pushed) when no trace is running.
bool MoveTrace_PushAny(HSQUIRRELVM v);

// chrUpper: "CHR001". ctx: the character's load context (patternByCode set); it is installed with objectPushed=true while
// the closures run. lastError: returns (and clears) the VM's last runtime error text.
std::vector<MoveTrace> TraceMovePatterns(HSQUIRRELVM v, const std::string& chrUpper, LoadContext& ctx,
	std::string (*lastError)());

} // namespace sqedit

#endif
