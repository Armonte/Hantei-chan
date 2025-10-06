#ifndef FRAMEDATA_LABELS_H_GUARD
#define FRAMEDATA_LABELS_H_GUARD

// ============================================================================
// MBAACC Frame Data Display Labels
// ============================================================================
// This file contains all the lookup tables and label arrays used for
// displaying frame data in the UI. Extracted from frame_disp.h for
// better organization and maintainability.
//
// These arrays map numeric IDs to human-readable descriptions for:
// - Hit vectors
// - Condition types
// - Character IDs
// - Effect types
// - And various other game data
// ============================================================================

// Hit vector list - from MBAACC Hit Vectors CSV
// TODO: Translate Japanese names for vectors 80-99 (currently showing JP text)
static const char* const hitVectorList[] = {
	"0: Head damage (weak)",
	"1: Head damage (medium)",
	"2: Head damage (strong)",
	"3: Gut damage (weak)",
	"4: Gut damage (medium)",
	"5: Gut damage (strong)",
	"6: Sweep",
	"7: Down",
	"8: Airborne damage",
	"9: Sent flying horizontal",
	"10: Sent flying upwards",
	"11: Sent flying up and away",
	"12: Spiked straight down",
	"13: Spiked down and away",
	"14: Stagger (unused?)",
	"15: Sent flying very far upwards",
	"16: Guard (unused?)",
	"17: No hitstun",
	"30: Standing crush (unused?)",
	"31: Crouching crush (unused?)",
	"32: Airborne crush (unused?)",
	"40: Floating guard? (unused?)",
	"50: Crouching damage (Weak)",
	"51: Crouching damage (Medium)",
	"52: Crouching damage (Strong)",
	"54: Jumping attack gut damage",
	"55: Jumping attack standing damage",
	"56: Jumping attack crouching damage",
	"57: Airborne down",
	"58: Soft launch",
	"59: Soft launch 2",
	"60: Air guard",
	"61: Spiked straight down (explode)",
	"62: Spiked down and away (bounce)",
	"63: Spike (bounce)",
	"64: Sent flying up 2",
	"65: Head damage (strong) 2",
	"66: Crouching damage (strong) 2",
	"67: Down 2",
	"68: Floating low down",
	"69: 7th HS sent flying horizontal",
	"70: Sent flying up (low grav)",
	"71: Home run",
	"72: Sliding down",
	"73: In-place head damage",
	"74: In-place gut damage",
	"75: Ryougi AT down",
	"76: Ries j.A head damage (weak)",
	"77: Sent flying up",
	"78: Sent flying up and away",
	"79: Low knockback gut damage",
	"80: 離れにくい屈中",
	"81: 離れにくい腹強",
	"82: 離れにくい屈強",
	"83: さつき用斜め叩きつけバウンド",
	"85: 特大のけぞり立",
	"86: 特大のけぞり屈",
	"87: 頭やられちょい中",
	"88: 屈やられちょい中",
	"90: 地上受け身＿短",
	"91: 地上受け身＿長",
	"92: 地上受け身＿前",
	"93: 空中受け身＿前",
	"94: 空中受け身＿中",
	"95: 空中受け身＿後",
	"98: ダウン追い討ち",
	"99: KOダウン",
	"152: 屈やられ中＋１",
	"153: 腹やられ中＋１",
	"161: 叩きつけ小バウンド",
	"162: 斜め叩きつけ小バウンド",
	"163: 叩きつけ中バウンド",
	"164: 斜め叩きつけ中バウンド",
	"171: 叩きつけ小バウンド",
	"172: 斜め叩きつけ小バウンド",
	"173: 叩きつけ中バウンド",
	"174: 斜め叩きつけ中バウンド",
	"200: 空中ガード2",
};

// Condition types (IF system)
static const char* const conditionTypes[] = {
	"0: (None)",
	"1: Jump on directional input",
	"2: Effect despawn conditions",
	"3: Branch on hit (all hits used)",
	"4: Vector check",
	"5: KO flag check",
	"6: Lever & Trigger check (Frame)",
	"7: Lever & Trigger check (Pattern)",
	"8: Random check",
	"9: Loop counter settings",
	"10: Loop counter check",
	"11: Additional command input check",
	"12: Opponent distance check",
	"13: Screen corner check",
	"14: Box collision check",
	"15: Box collision check (Capture)",
	"16: Be affected by scrolling",
	"17: Branch on number of hits",
	"18: Check main/trunk animation",
	"19: Projectile box col check",
	"20: Box collision check 2",
	"21: Opponent's character check",
	"22: BG number check",
	"23: BG type check",
	"24: Projectile variable check",
	"25: Variable comparison",
	"26: Check lever and change vector",
	"27: Branch when parent gets hurt",
	"28: Jump if knocked out",
	"29: Check X pos on screen",
	"30: Facing direction check",
	"31: Change variable on command input",
	"32: Jump if CPU side of CPU battle",
	"33: If sound effect is playing",
	"34: Homing",
	"35: Custom cancel command check",
	"36: Meter bar mode check",
	"37: Jump according to color",
	"38: Change variable on hit",
	"39: (Unknown)",
	"40: Jump after N frames",
	"41: Jump if controlled char mismatch",
	"42: (Unknown)",
	"43: (Unknown)",
	"44: (Unknown)",
	"45: (Unknown)",
	"46: (Unknown)",
	"47: (Unknown)",
	"48: (Unknown)",
	"49: (Unknown)",
	"50: Effect reflection box",
	"51: Check Shield Conditions",
	"52: Throw check",
	"53: (Unknown)",
	"54: Jump on hit or block",
	"55: (Unknown)",
	"56: (Unknown)",
	"57: (Unknown)",
	"58: (Unknown)",
	"59: (Unknown)",
	"60: (Unknown)",
	"61: (Unknown)",
	"62: (Unknown)",
	"63: (Unknown)",
	"64: (Unknown)",
	"65: (Unknown)",
	"66: (Unknown)",
	"67: (Unknown)",
	"68: (Unknown)",
	"69: (Unknown)",
	"70: Jump on reaching screen border",
	"71: (Unknown)",
	"72: (Unknown)",
	"73: (Unknown)",
	"74: (Unknown)",
	"75: (Unknown)",
	"76: (Unknown)",
	"77: (Unknown)",
	"78: (Unknown)",
	"79: (Unknown)",
	"80: (Unknown)",
	"81: (Unknown)",
	"82: (Unknown)",
	"83: (Unknown)",
	"84: (Unknown)",
	"85: (Unknown)",
	"86: (Unknown)",
	"87: (Unknown)",
	"88: (Unknown)",
	"89: (Unknown)",
	"90: (Unknown)",
	"91: (Unknown)",
	"92: (Unknown)",
	"93: (Unknown)",
	"94: (Unknown)",
	"95: (Unknown)",
	"96: (Unknown)",
	"97: (Unknown)",
	"98: (Unknown)",
	"99: (Unknown)",
	"100: Box collision with partner",
	"101: (Unknown)",
	"102: (Unknown)",
	"103: (Unknown)",
	"104: (Unknown)",
	"105: (Unknown)",
	"106: (Unknown)",
	"107: (Unknown)",
	"108: (Unknown)",
	"109: (Unknown)",
	"110: (Unknown)",
	"111: (Unknown)",
	"112: (Unknown)",
	"113: (Unknown)",
	"114: (Unknown)",
	"115: (Unknown)",
	"116: (Unknown)",
	"117: (Unknown)",
	"118: (Unknown)",
	"119: (Unknown)",
	"120: (Unknown)",
	"121: (Unknown)",
	"122: (Unknown)",
	"123: (Unknown)",
	"124: (Unknown)",
	"125: (Unknown)",
	"126: (Unknown)",
	"127: (Unknown)",
	"128: (Unknown)",
	"129: (Unknown)",
	"130: (Unknown)",
	"131: (Unknown)",
	"132: (Unknown)",
	"133: (Unknown)",
	"134: (Unknown)",
	"135: (Unknown)",
	"136: (Unknown)",
	"137: (Unknown)",
	"138: (Unknown)",
	"139: (Unknown)",
	"140: (Unknown)",
	"141: (Unknown)",
	"142: (Unknown)",
	"143: (Unknown)",
	"144: (Unknown)",
	"145: (Unknown)",
	"146: (Unknown)",
	"147: (Unknown)",
	"148: (Unknown)",
	"149: (Unknown)",
	"150: (Unknown)",
	"151: (Unknown)",
};

// Character list
static const char* const characterList[] = {
	"0: Sion", "1: Arc", "2: Ciel", "3: Akiha", "4: Maids",
	"5: Hisui", "6: Kohaku", "7: Tohno", "8: Miyako", "9: Wara",
	"10: Nero", "11: V.Sion", "12: Warc", "13: V.Akiha", "14: Mech",
	"15: Nanaya", "16: G.Akiha", "17: Satsuki", "18: Len", "19: P.Ciel",
	"20: Neco", "21: (None)", "22: Aoko", "23: W.Len", "24: (None)",
	"25: NAC", "26: (None)", "27: G.Chaos", "28: Kouma", "29: Sei",
	"30: Ries", "31: Roa", "32: Hermes", "33: Ryougi", "34: NecoMech",
	"35: KohaMech", "36: (None)", "37: (None)", "38: (None)", "39: (None)",
	"40: (None)", "41: (None)", "42: (None)", "43: (None)", "44: (None)",
	"45: (None)", "46: (None)", "47: (None)", "48: (None)", "49: (None)",
	"50: (None)", "51: B.Hime", "52: (None)", "53: (None)", "54: (None)",
	"55: (None)", "56: (None)", "57: (None)", "58: B.Miyako", "59: B.Wara",
	"60: (None)", "61: (None)", "62: (None)", "63: (None)", "64: (None)",
	"65: (None)", "66: (None)", "67: (None)", "68: (None)", "69: (None)",
	"70: (None)", "71: (None)", "72: B.Aoko", "73: B.W.Len", "74: (None)",
	"75: (None)", "76: (None)", "77: (None)", "78: (None)", "79: (None)",
	"80: (None)", "81: (None)", "82: (None)", "83: (None)", "84: (None)",
	"85: B.KohaMech",
};

// Hit conditions
static const char* const hitConditions[] = {
	"0: On hit",
	"1: On hit or block",
	"2: On hit, clash, or shield",
	"3: On hit, block, clash, or shield",
	"5: On block",
	"6: On clash or shield",
	"7: On block, clash, or shield",
	"100: On shield only",
};

// Opponent state
static const char* const opponentStateList[] = {
	"0: Grounded or airborne",
	"1: Only grounded",
	"2: Only airborne",
};

// Comparison types
static const char* const comparisonTypes[] = {
	"0: Greater than",
	"1: Less than",
	"2: Equal to",
};

// Effect types
static const char* const effectTypes[] = {
	"0: (Unknown)",
	"1: Spawn Pattern",
	"2: Various Effects",
	"3: Spawn Preset Effect",
	"4: Set Opponent State (no bounce reset)",
	"5: Damage",
	"6: Various Effects 2",
	"8: Spawn Actor (effect.ha6)",
	"9: Play Audio",
	"11: Spawn Random Pattern",
	"14: Set Opponent State (reset bounces)",
	"101: Spawn Relative Pattern",
	"111: Spawn Random Relative Pattern",
	"257: (Arc typo?)",
	"1000: Spawn and Follow",
	"10002: Unknown",
};

// Action state (AS) labels
static const char* const stateList[] = {
	"Standing",
	"Airborne",
	"Crouching"
};

static const char* const cancelList[] = {
	"Never",
	"On hit",
	"Always",
	"On successful hit"
};

static const char* const counterList[] = {
	"No change",
	"High counter",
	"Low counter",
	"Clear"
};

static const char* const invulList[] = {
	"None",
	"High and mid",
	"Low and mid",
	"All but throw",
	"Throw only"
};

// Attack (AT) labels
static const char* const hitEffectList[] = {
	"Weak punch",
	"Medium punch",
	"Strong punch",
	"Weak kick",
	"Medium kick",
	"Strong kick",
	"Super punch",
	"Super kick",
	"Slash (sparks)",
	"Burn",
	"Freeze",
	"Shock",
	"Big flash (SE)",
	"Small flash (SE)",
	"None",
	"Strong hit",
	"Double slash",
	"Super slash",
	"Weak cut",
	"Medium cut",
	"Strong cut",
	"Faint wave",
};

static const char* const addedEffectList[] = {
	"None",
	"Burn",
	"Freeze",
	"Shock",
	"Confuse"
};

static const char* const vectorFlags[] = {
	"Default",
	"Reverse",
	"Nullify",
	"Both?"
};

static const char* const hitStopList[] = {
	"Weak (6f)",
	"Medium (8f)",
	"Strong (10f)",
	"None (0f)",
	"Stronger (15f)",
	"Strongest (29f)",
	"Weakest (3f)"
};

// Animation frame (AF) labels
static const char* const interpolationList[] = {
	"None",
	"Linear",
	"Slow->Fast",
	"Fast->Slow",
	"Fast middle",
	"Slow middle", //Never used, but it works.
};

static const char* const animationList[] = {
	"Go to pattern",
	"Next frame",
	"Go to frame"
};

#endif /* FRAMEDATA_LABELS_H_GUARD */

