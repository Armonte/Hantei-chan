// UNI2 / MBTL effect (EF), EF6 sub-number and condition (IF) labels.
// Rows: {id, name, status, {p1..p12}} with p1 = parameters[0]. "No" is the EF number field.
// Status: "wiki+IDA" = Mizuumi FPANModding page agrees with the handler; "IDA" = from the
// handler only (no source covers it); "IDA-corrected" = the wiki or the MBAACC label is wrong.
// Handlers (same case sets in both games, MBTL differs only in struct offsets):
//   EF   uni2.exe 0x5B8620 Han6_EF_Dispatch   / MBTL.exe 0x582420
//   EF6  uni2.exe 0x5B5610 Han6_EF6_Dispatch  / MBTL.exe 0x57F480
//   IF   uni2.exe 0x5C6220 Han6_IF_Dispatch   / MBTL.exe 0x590260
// Full tables and sources: docs/uni_re/labels/LABELS.md. Types not listed have no handler
// in these games and show "EF <n> (unknown in <game>)".

#define P12(...) {__VA_ARGS__}

static const GameLabelRow kFbEf[] = {
	{1, "Spawn pattern (character HA6)", "wiki+IDA", P12("Relative X", "Relative Y", "Flags 1 (1 erase on parent hit, 4 follow parent, 8 parent hitstop, 16 camera centre, 32 erase on parent pattern change, 64 below floor, 128 on floor)", "Flags 2 (2 follow parent, 4 erase when parent thrown, 64 ignore super flash, 128 ignore camera, 256 relative to opponent, 512 stage centre, 1024 stage centre facing right)", nullptr, nullptr, nullptr, "Angle (as SetAngle)")},
	{5, "Held victim command", "IDA", P12("No 0: victim state / No 1: damage / No 3: signed value", nullptr, "No 1: hit effect", "No 1: sound id (normal_se)", "No 1: apply combo damage calc", nullptr)},
	{6, "Various effects 2 (see number)", "IDA", P12()},
	{8, "Spawn system effect (effect.HA6)", "wiki+IDA", P12("Relative X", "Relative Y", "Flags 1 (as EF 1)", "Flags 2 (as EF 1)", nullptr, nullptr, nullptr, "Angle (as SetAngle)")},
	{9, "Play sound", "wiki+IDA", P12("Sound id", "Probability (0 = always)", "Number of adjacent ids to pick from")},
	{11, "Spawn random pattern", "IDA", P12()},
	{14, "Set opponent visual state (on grab)", "wiki+IDA", P12("Relative X (once per pattern)", "Relative Y (once per pattern)", "Rotation", "Vector id", "Anchor (0 origin, 1-3 special box 10-12)", "Untech time", "Hold position (1 player to opponent, 2 opponent to player)")},
	{101, "Spawn pattern (relative, as EF 1)", "IDA", P12("Relative X", "Relative Y", "Flags 1 (as EF 1)", "Flags 2 (as EF 1)", nullptr, nullptr, nullptr, "Angle (as SetAngle)")},
	{108, "Spawn system effect (as EF 8)", "IDA", P12("Relative X", "Relative Y", "Flags 1 (as EF 1)", "Flags 2 (as EF 1)", nullptr, nullptr, nullptr, "Angle (as SetAngle)")},
	{111, "Spawn random pattern (as EF 11)", "IDA", P12()},
};

static const GameLabelRow kFbEf6[] = {
	{1, "Screen shake", "wiki+IDA", P12("Duration", nullptr, nullptr, "Rumble id", "Shake param", "Shake param", nullptr, nullptr, nullptr, nullptr, "Bit 1: reset shake state")},
	{2, "Invincibility timers (INVINCIBILITY ticker)", "wiki+IDA", P12("Timer A (100+ shows the ticker)", "Timer B")},
	{4, "Add to gauge", "IDA", P12("Amount (clamped to max)", "Extra flag")},
	{5, "Turn around (see p1)", "IDA-corrected", P12("Mode: 0 flip, 1 face opponent (p3 dead zone), 2-3 flip by state, 4-5 set facing, 6 random, 7 copy", "Do not flip the held victim", "Dead zone (px)")},
	{6, "Set movement vector", "IDA-corrected", P12("Value min", "Value max (random)", "Accel min", "Accel max (random)", "0 = X, 1 = Y", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Mode: 0 set, 1 angle launch, 2 arc to opponent")},
	{8, "Push toward target along angle", "IDA-corrected", P12(nullptr, "Speed", "Re-run frame change")},
	{9, "Set no-input flag", "IDA", P12("0 = set, other = clear")},
	{11, "Super flash", "IDA", P12("Flash id", "Enable")},
	{12, "Teleport / set position", "wiki+IDA", P12("X", "Y", "Relative to camera", "1 = self, other = owner", "Flags (1 opponent, 2 to parent, 4 keep X, 8 keep Y, 16 force facing)", nullptr, nullptr, nullptr, nullptr, nullptr, "Mirror for 2P", "Mode: 0 relative, 1 camera, 2 parent, 3 stage edge")},
	{14, "Add to gauge of box target", "IDA", P12("Box", "Amount", "Extra flag")},
	{15, "Start / stop music", "IDA", P12("0 = stop")},
	{16, "Directional movement", "IDA", P12("Angle / X", "Random range / Y", "Frames / speed", nullptr, "Random speed range", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Mode: 0 angle, 1 move to camera point")},
	{19, "Proration (opponent side)", "IDA", P12("Value", "Mode: 0 min, 1 multiply %, 2 subtract")},
	{21, "Restore saved camera", "IDA-corrected", P12()},
	{22, "Scale / rotation", "IDA", P12("Value (1/10000)", "Random range", nullptr, nullptr, nullptr, "Mode: 0 scale X, 1 scale Y, 2 both, 10 rotation")},
	{102, "Increase dash variable", "IDA", P12("Amount")},
	{103, "Decrease dash variable", "IDA", P12("Amount")},
	{105, "Change variable", "IDA", P12("Variable", "Value", "Tens: 0 owner, 1 self; ones: 0 set, 1 add")},
	{106, "Set / clear object flags", "IDA-corrected", P12("Flag mask", "0 clear, 1 set, 2 clear if set")},
	{111, "Store / restore movement (freeze)", "wiki+IDA", P12("0 store, 1 restore", "Zero the movement after storing", "Y speed after the freeze")},
	{112, "Proration (own side)", "IDA", P12("Value", "Mode: 0 min, 1 multiply %, 2 subtract")},
	{115, "Store movement and brake", "IDA", P12("Divisor")},
	{116, "Launch along rotation", "IDA", P12("Speed (1/128 px)")},
	{117, "Home in on own object's box", "IDA", P12("Box (+9)", "Frames", nullptr, nullptr, "Flags (1 keep X, 2 keep Y, 4 face it)")},
	{153, "Draw in front (Z priority)", "IDA", P12()},
	{252, "Set state byte", "IDA", P12("Value")},
	{254, "Round flag (intro done)", "IDA", P12()},
	{255, "Round end: win animation done", "wiki+IDA", P12()},
	{300, "Set / add draw offset", "IDA", P12("X", "Y", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Bits: 1 X, 2 Y, 4 no mirror", "0 set, 1 add")},
	{301, "Screen effect at position", "IDA", P12("X", "Y", "Effect id", "Param", "Param", "Scale (1/10000)", "Use default position")},
	{302, "Set global flag", "IDA", P12("Value")},
	{310, "Set object flags", "IDA", P12(nullptr, nullptr, "Flags (low 16)", "Flags (high 16)")},
	{311, "Clear object flags", "IDA", P12(nullptr, nullptr, "Flags (low 16)", "Flags (high 16)")},
	{1100, "Reverse velocity", "IDA", P12("0 = X, 1 = Y")},
};

static const GameLabelRow kFbIf[] = {
	{2, "Despawn conditions", "IDA", P12("Bits: 1 off-screen X, 2 off-screen Y", "On landing", nullptr, "Decrement variable (tens slot, ones amount)")},
	{3, "Branch on hit / guard", "IDA", P12("Target frame", "Contact type (%100)", "0 any, 1 hit, 2 guard", "Decrement variable", nullptr, "Combo count", "Hit count")},
	{5, "On KO: release", "IDA-corrected", P12()},
	{6, "Lever and button check (frame)", "IDA", P12()},
	{7, "Lever and button check (pattern)", "IDA", P12(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Restart pattern")},
	{8, "Random branch", "IDA", P12("Target", "Chance (of 512)", "Random target", "Count")},
	{11, "Additional command check", "IDA", P12(nullptr, nullptr, "Value", "Command")},
	{14, "Box collision check", "IDA", P12()},
	{15, "Box collision check (capture)", "IDA", P12(nullptr, "Box", "Object (%1000, /10000 side)")},
	{17, "Branch on number of hits", "IDA", P12(nullptr, "Hits at least", "Decrement variable")},
	{19, "Projectile box contact", "IDA", P12()},
	{20, "Box collision check 2", "IDA", P12(nullptr, nullptr, "Object", "Hit stop")},
	{26, "Lever direction check", "IDA", P12()},
	{29, "Clamp X to screen position", "IDA-corrected", P12(nullptr, "Screen X (-30000 = variable 0)", "0 left limit, 1 right limit")},
	{31, "Change variable on command", "IDA", P12(nullptr, "Variable", "Value")},
	{34, "Homing", "IDA", P12()},
	{35, "Multi-command check", "IDA", P12()},
	{38, "Change variable on hit", "IDA", P12("Value", "Contact type (%100)", "0 any, 1 hit, 2 guard", "Variable", "Ones: 0 set, 1 add; tens: target")},
	{56, "Vacuum (pull opponents)", "wiki+IDA", P12("Speed", "Range (px)", "Weaken with distance")},
	{70, "Stage border collision", "IDA-corrected", P12("Border check 2 (<0 off)", "Border check 3 (<0 off)", "Border check 0 (<0 off)", "Border check 1 (<0 off)")},
};

#undef P12
