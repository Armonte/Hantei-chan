#pragma once

#include <cstdint>

// HA4 Format Definitions for Melty Blood Act Cadenza
// Based on reverse engineering of hantei4.exe
// Memory layout differs from file layout (compression on save)

// Structure sizes
#define HA4_CHAR_DATA_SIZE 140404      // Total character animation data
#define HA4_FRAME_SIZE_MEMORY 832      // Frame size in editor memory
#define HA4_FRAME_SIZE_FILE 216        // Frame size in .ha4 file (compressed)
#define HA4_EFFECT_SIZE 52             // Effect structure size
#define HA4_CONDITION_SIZE 52          // Condition structure size (same as effect)
#define HA4_HITBOX_SIZE 88             // Attack hitbox structure size

// Type limits
#define HA4_MAX_IF_TYPES 256           // Condition types (001-037 used)
#define HA4_MAX_EF_TYPES 257           // Effect types

// File format constants
#define HA4_MAX_FRAMES_PER_SEQUENCE 256
#define HA4_MAX_SEQUENCES 256
#define HA4_MAX_EFFECTS_PER_FRAME 8
#define HA4_MAX_CONDITIONS_PER_FRAME 8

// HA4 Effect Structure (52 bytes)
// Maps to HA6 Frame_EF structure
// Memory offset relative to frame base: varies per effect slot
struct HA4_Effect {
    int16_t type;           // +0: EF type (0-256), maps 1:1 to HA6
    int16_t number;         // +2: EF number/ID
    int32_t param[12];      // +4: 12 parameters (48 bytes)
                            // param[0-7]: Standard parameters
                            // param[8-11]: Extended parameters
};

// HA4 Condition Structure (52 bytes)
// Maps to HA6 Frame_IF structure
// Memory offset relative to frame base: +416 to +831
struct HA4_Condition {
    int16_t type;           // +0: IF type (001-037), maps 1:1 to HA6
    int16_t reserved;       // +2: Padding
    int32_t param[12];      // +4: 12 parameters (48 bytes)
                            // Only first 9 used in HA6 mapping
};

// HA4 Frame Animation Parameters (AFST section)
// These offsets are relative to frame base in 832-byte memory layout
struct HA4_Frame_Animation {
    int32_t duration;       // AFD: Frame duration in ticks
    int32_t flow_type;      // AFF1/AFF2: Flow control type
    int32_t jump_frame;     // AFJP: Jump target frame
    int32_t land_jump;      // AFJC: Landing jump frame

    // Layer/sprite data
    struct {
        int32_t sprite_id;  // AFIM: Which sprite to display
        int32_t offset_x;   // AFOF: X offset
        int32_t offset_y;   // AFOF: Y offset
        int32_t priority;   // AFPL: Layer priority (uses AFPR presets)
        int32_t flip_flags; // AFTN: Flip/transform flags
        int32_t scale_x;    // AFSC: X scale (256 = 100%)
        int32_t scale_y;    // AFSC: Y scale (256 = 100%)
        int32_t rotation;   // AFRO: Rotation angle
        int32_t alpha;      // AFAL: Alpha/transparency
        int32_t blend_mode; // AFBL: Blend mode
    } layers[8];            // Up to 8 layers per frame

    int32_t glow_params;    // AFGP: Glow effect (HA4-specific, not AFGX)
    int32_t sound_id;       // AFSE: Sound effect to play
    int32_t voice_id;       // AFVO: Voice line to play
};

// HA4 Frame State Parameters (ASST section)
struct HA4_Frame_State {
    int32_t stance;         // ASS1/ASS2: Character stance flags
    int32_t cancel_normal;  // ASCN: Normal cancel flags
    int32_t cancel_special; // ASCS: Special cancel flags
    int32_t cancel_super;   // ASCA: Super cancel flags
    int32_t counter_type;   // ASCT: Counter hit type
    int32_t invincibility;  // ASYS: Invincibility flags
    int32_t armor;          // ASAR: Armor/super armor
    int32_t guard_point;    // ASGP: Guard point flags
    int32_t state_flags;    // Various state flags
};

// HA4 Frame Attack Parameters (ATST section)
struct HA4_Frame_Attack {
    int32_t guard_flags;    // ATGD: Guard type flags
    int32_t damage;         // ATAT: Base damage value
    int32_t chip_damage;    // ATCP: Chip damage on block
    int32_t hitstun;        // ATHV: Hitstun on hit (uses ATHV presets)
    int32_t blockstun;      // ATGV: Blockstun on guard (uses ATGV presets)
    int32_t hitstop;        // ATSP: Hitstop frames (uses ATSP presets)
    int32_t hit_effect;     // ATSE: Hit effect type
    int32_t guard_effect;   // ATGS: Guard effect type
    int32_t pushback_air;   // ATPA: Air pushback
    int32_t pushback_ground;// ATPG: Ground pushback
    int32_t launch_angle;   // ATLA: Launch angle
    int32_t launch_speed;   // ATLV: Launch velocity
    int32_t properties;     // ATPR: Attack properties (overhead, low, etc)
    int32_t counter_level;  // ATCL: Counter hit level
    int32_t proration;      // ATPM: Damage proration
    int32_t limit;          // ATLM: Combo limit modifier

    // Hitbox data (88 bytes)
    struct {
        int32_t active;     // Is hitbox active
        int32_t x;          // X position
        int32_t y;          // Y position
        int32_t width;      // Width
        int32_t height;     // Height
        int32_t type;       // Hitbox type (attack/hurt/push/throw)
        // Additional hitbox parameters...
    } hitboxes[4];          // Up to 4 hitboxes per frame
};

// Complete HA4 Frame (832 bytes in memory, 216 in file)
// This is the in-memory representation
// File format uses tag-based structure (AFST, ASST, ATST, EF, IF)
struct HA4_Frame {
    HA4_Frame_Animation anim;       // Animation parameters
    HA4_Frame_State state;          // State parameters
    HA4_Frame_Attack attack;        // Attack parameters
    HA4_Condition conditions[8];    // IF conditions (52 * 8 = 416 bytes)
    HA4_Effect effects[8];          // EF effects (52 * 8 = 416 bytes)

    // Note: Actual memory layout may differ
    // This is a logical grouping for conversion purposes
};

// HA4 Sequence Structure
struct HA4_Sequence {
    uint32_t frame_count;           // Number of frames in sequence
    char name[32];                  // Sequence name (Shift-JIS)
    HA4_Frame* frames;              // Array of frames
};

// HA4 Character Data
struct HA4_Character {
    uint32_t sequence_count;        // Number of sequences
    char character_name[32];        // Character name (Shift-JIS)
    HA4_Sequence* sequences;        // Array of sequences

    // Sprite/CG data references
    char sprite_names[256][32];     // Sprite filename table
    uint32_t sprite_count;
};

// HA4 File Header (structure TBD - not fully documented yet)
struct HA4_FileHeader {
    char signature[16];             // File format signature (unknown)
    uint32_t version;               // Format version
    uint32_t data_offset;           // Offset to main data
    uint32_t data_size;             // Size of character data
    char padding[32];               // Reserved
};

// Parameters that exist in HA6 but NOT in HA4
// These need default values during conversion:
// - AFID: Frame ID (default: auto-increment)
// - AFPA: Parent frame (default: -1)
// - AFGX: Glow extended (HA4 uses AFGP instead)
// - ATV2: Launch vector v2 (HA4 uses ATHV/ATGV)
// - ATHH: Hitbox height (HA4 stores differently)
// - ATSH: Shield properties (UNI-only)
// - ATC0-ATC9: Custom parameters (UNI-only)
// - ATAM: Additional attack modifiers (UNI-only)

// Tag identifiers for file parsing
#define HA4_TAG_PTCN 0x4E435450  // "PTCN" - Pattern count
#define HA4_TAG_PTT2 0x32545450  // "PTT2" - Pattern table 2
#define HA4_TAG_AFST 0x54534641  // "AFST" - Animation Frame Start
#define HA4_TAG_ASST 0x54535341  // "ASST" - Animation State Start
#define HA4_TAG_ATST 0x54535441  // "ATST" - Attack State Start
#define HA4_TAG_EF   0x00004645  // "EF\0\0" - Effect
#define HA4_TAG_IF   0x00004649  // "IF\0\0" - Condition

// Conversion flags
#define HA4_CONVERT_ADD_DEFAULTS    0x01  // Add default values for missing HA6 params
#define HA4_CONVERT_PRESERVE_LAYOUT 0x02  // Try to preserve original frame layout
#define HA4_CONVERT_VERBOSE         0x04  // Log conversion details
