#ifndef TAG_ASSIST_H_GUARD
#define TAG_ASSIST_H_GUARD
// Assist slots for the Tag / Team panel: a character's command list read from its _c.txt (through the cmdfile
// parser), the default assist the game picks when a slot is unset, and the slot resolution (which action / entry a
// FN1+direction call will actually use).
//
// MIRRORS PovertyCaster pc-adapters/mbaacc/include/mbaacc/MbaaccTagAssist.hpp (main 5f043727):
//   resolveAssistSlot   inside a slot pattern > command > motion; a slot with no action uses slot 5's; with neither,
//                       the character default. Entry: the slot's, else slot 5's, else assistEntry.
//   isDefaultCandidate  the LOWEST command id whose input is 2+ direction digits then the single button A, costing
//                       no meter, usable standing (flagset 1 bit 0 = its last digit), with a pattern > 0. The game
//                       also needs the entry to be enabled right now (moon / ExComCheck / limits); the panel reads
//                       the moon's own _c.txt and flags a pick that has ExComChecks ("conditional").
//   commandMatchesMotion motion mode runs the lowest-id command whose input is exactly that text.
// Command lines follow LoadCharacterCommandFile: the first definition of an id wins.
#include "tag_levers.h"

#include <string>
#include <vector>

namespace tagtune {

struct CommandInfo {
	int id = 0;
	std::string input;       // "236A", "2+A+B", "0202C"
	int pattern = 0;
	int meter = 0;
	bool standing = true;    // flagset 1 bit 0 (usable standing)
	bool air = false;        // flagset 1 bit 1
	int exComChecks = 0;     // ExComCheck rows for this id (the engine checks them at use time)
	std::string name;        // the line's // comment, UTF-8
	int line = 0;            // 1-based line in the _c.txt
};

// Parse a _c.txt (raw CP932 bytes). toUtf8 converts the comments for display (nullptr = keep the bytes).
std::vector<CommandInfo> ParseCommands(const std::string& bytes, std::string (*toUtf8)(const std::string&) = nullptr);
const CommandInfo* FindCommand(const std::vector<CommandInfo>& cmds, int id);

bool IsDefaultCandidate(const CommandInfo& c);
// Index into cmds of the default assist, or -1 (no candidate: the assist does nothing for that character).
int PickDefaultCommand(const std::vector<CommandInfo>& cmds);

// Motion mode: the command the interpreter would run for this input (lowest id, exact input), or nullptr.
const CommandInfo* CommandForMotion(const std::vector<CommandInfo>& cmds, const std::string& motion);
// Validate a motion text for assist.<d>.motion. Empty string = ok, else the problem.
std::string ValidateMotion(const std::string& motion);

enum class AssistMode : uint8_t { Default = 0, Pattern = 1, Command = 2, Motion = 3 };
const char* AssistModeName(AssistMode m);
struct SlotLevers { int entry = -1, pattern = -1, command = -1, motion = -1; };   // lever indices
SlotLevers SlotLeverIndices(int slot);   // slot 0..4 = directions 5, 2, 6, 4, 8
struct AssistAction {
	AssistMode mode = AssistMode::Default;
	int32_t value = 0;       // pattern / command id / packed motion
	int entry = 0;           // an assistEntry value: 0 behind, 1 edge, 2 drop, 3 arc
	int fromSlot = -1;       // which slot's action is used (-1 = the character default)
};
AssistAction ResolveAssistSlot(const Values& v, int slot);

// What the action ends up playing, for display: "pattern 455", "command 40 (623A) -> pattern 455", ...
struct ActionView {
	std::string text;
	int pattern = -1;        // the pattern it plays when known (-1 = unknown / the interpreter decides)
	bool problem = false;    // the action points at nothing (unknown command id, no command for the motion, ...)
};
ActionView DescribeAction(const AssistAction& a, const std::vector<CommandInfo>& cmds);

} // namespace tagtune

#endif
