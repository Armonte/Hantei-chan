#include <cassert>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include "framedata_load.h"
#include "framedata.h"
#include "ha4_format.h"
#include "misc.h"

// HA4 Binary Format Loader - Melty Blood Act Cadenza
// Based on complete reverse engineering of hantei4.exe
// HA4 uses PURE BINARY format with offset tables, NOT tag-based!

// File format structures (216-byte compressed frame)
struct HA4_CompressedFrame {
    uint8_t frame_header[44];      // +0: Animation parameters
    uint8_t layer_data[56];        // +44: State parameters
    int16_t hitbox_index;          // +100: Main hitbox (-1 = none)
    int16_t effect_indices[8];     // +102: Effect indices
    int16_t condition_indices[8];  // +118: Condition indices
    int16_t main_vector_index;     // +134: Main vector
    int16_t condition_vectors[8];  // +136: Condition vectors
    int16_t effect_vectors[16];    // +152: Effect vectors
    int16_t extra_vectors[8];      // +184: Extra vectors
    // Total: 200 bytes (need to verify 216 exact layout)
};

// Sequence header - CORRECTED based on Python ha4_parser.py
// Frame data offset is in header (not hardcoded!)
struct HA4_SequenceHeader {
    uint32_t frame_count;          // +0x00: Number of frames (uint32, NOT uint8!)
    uint32_t flag1;                // +0x04: Unknown flag
    uint32_t flag2;                // +0x08: Unknown flag
    int32_t  frame_data_offset;    // +0x0C: Frame data offset (relative to sequence start, -1 if none)
    int32_t  hitbox_ref_offset;    // +0x10: Hitbox array offset (relative to sequence start, -1 if none)
    int32_t  effect_data_offset;   // +0x14: Effect array offset (relative to sequence start, -1 if none)
    int32_t  condition_data_offset;// +0x18: Condition array offset (relative to sequence start, -1 if none)
    uint8_t  reserved[16];         // +0x1C: Reserved bytes
    int32_t  vector_data_offset;   // +0x28: Vector array offset (8 bytes per vector) ← NEW!
    uint8_t  reserved2[24];        // +0x2C: Reserved bytes
    // Total: 68 bytes (0x44)
};

// HA4 Binary File Header (64 bytes) - Distribution .DAT files
struct HA4_BinaryFileHeader {
    char     magic[8];             // +0x00: "Hantei4\0"
    uint8_t  null_padding[8];      // +0x08: Null bytes
    uint32_t version;              // +0x10: Always 1
    uint32_t anim_data_size;       // +0x14: Size of animation data
    uint32_t unknown_zero;         // +0x18: Always 0
    uint32_t anim_data_size2;      // +0x1C: Duplicate of anim_data_size
    uint32_t image_data_size;      // +0x20: Embedded CG size
    uint8_t  padding[28];          // +0x24: Padding to reach 64 bytes (0x40)
    // Total: 64 bytes (0x40)
};

struct HA4_Vector {
    int16_t start_x;    // +0x00: Left edge
    int16_t start_y;    // +0x02: Top edge
    int16_t end_x;      // +0x04: Right edge
    int16_t end_y;      // +0x06: Bottom edge
    // Total: 8 bytes
    // Width = end_x - start_x
    // Height = end_y - start_y
};

// Helper function to read HA4 file header
bool ReadHA4FileHeader(const uint8_t *data, size_t data_size, HA4_BinaryFileHeader *header) {
    if (data_size < 64) {
        std::cerr << "HA4 Error: File too small for header\n";
        return false;
    }

    memcpy(header, data, sizeof(HA4_BinaryFileHeader));

    // Validate magic
    if (memcmp(header->magic, "Hantei4", 7) != 0) {
        std::cerr << "HA4 Error: Invalid magic signature\n";
        return false;
    }

    std::cout << "HA4: File header valid\n";
    std::cout << "  Version: " << header->version << "\n";
    std::cout << "  Animation data size: " << header->anim_data_size << " bytes\n";
    std::cout << "  Image data size: " << header->image_data_size << " bytes\n";

    return true;
}

// Helper function to read offset table
bool ReadHA4OffsetTable(const uint8_t *data, size_t data_size, int32_t *offsets) {
    if (data_size < 64 + 1024) {
        std::cerr << "HA4 Error: File too small for offset table\n";
        return false;
    }

    // Offset table is at 0x40 (64 bytes), 1024 bytes total
    memcpy(offsets, data + 0x40, 1024);

    // Count non-empty sequences
    int count = 0;
    for (int i = 0; i < 256; i++) {
        if (offsets[i] != -1) {
            count++;
        }
    }

    std::cout << "HA4: Found " << count << " non-empty sequences\n";
    return true;
}

// Convert 44-byte frame header to HA6 animation parameters
void ConvertFrameHeader(const uint8_t *frame_header, Frame *frame, bool is_csel_file) {
    // COMPLETE 44-BYTE STRUCTURE MAPPING
    // Based on CharacterDataParser FrameHeader structure

    // Core sprite and positioning (+0 to +7)
    int16_t sprite_id = *(const int16_t*)(frame_header + 0x00);      // +0x00: Sprite/Pattern ID
    int16_t position_x = *(const int16_t*)(frame_header + 0x02);     // +0x02: off_x (X position offset)
    int16_t position_y = *(const int16_t*)(frame_header + 0x04);     // +0x04: off_y (Y position offset)
    int16_t duration = *(const int16_t*)(frame_header + 0x06);       // +0x06: WAIT (frame duration)

    // Map to HA6 AF fields
    // SPECIAL CASE: CSEL files store sprite_id as 10000+N where N is the CG index
    // For CSEL files (frame_data_offset=-1), subtract 10000 to get direct CG index
    // For full character files, keep sprite_id as-is (it's either <10000 for CG, or >=10000 for patterns)
    if (is_csel_file && sprite_id >= 10000) {
        sprite_id -= 10000;  // Convert pattern reference to direct CG index
    }

    frame->AF.spriteId = sprite_id;
    frame->AF.offset_x = position_x;
    frame->AF.offset_y = position_y;
    frame->AF.duration = duration;

    // Determine sprite type (CG part vs Pattern)
    // < 10000 = CG part (direct sprite), >= 10000 = Pattern (composite)
    frame->AF.usePat = (sprite_id >= 10000) ? 1 : 0;

    // Rendering transformations (+0x10, +0x12)
    uint8_t rotation_flip = frame_header[0x10];      // +0x10: 0-9 sprite transform mode (control 1080)
    uint8_t arithmetic_op = frame_header[0x12];      // +0x12: 0-3 parameter operation (control 1052)

    // Map rotation_flip to HA6 rotation
    // 0-9 modes: normal, flip X, flip Y, flip XY, rotate 90/180/270, etc.
    if (rotation_flip > 0) {
        // Simplified mapping - may need refinement
        frame->AF.rotation[0] = (rotation_flip & 0x01) ? 0.5f : 0.0f;  // X flip
        frame->AF.rotation[1] = (rotation_flip & 0x02) ? 0.5f : 0.0f;  // Y flip
        frame->AF.rotation[2] = 0.0f;  // Z rotation (TODO: map rotation modes)
    }

    // Jump targets (+0x0C, +0x0D)
    int16_t jump_target = *(const int16_t*)(frame_header + 0x0C);        // +0x0C: JUMP target (control 1013)
    uint8_t landing_jump_target = frame_header[0x0D];                    // +0x0D: Landing JP target (control 1036)

    // Animation flow control (+0x0B)
    uint8_t animation_flow = frame_header[0x0B];     // +0x0B: 0-5 flow control (control 1047)
    // HA4 animation_flow values:
    //   0 = Ordinance (End/Go to pattern)
    //   1 = Next
    //   2 = Jump (to frame)
    //   3 = NextLanding
    //   4 = JumpLanding
    //   5 = LoopCheck
    //
    // HA6 aniType values:
    //   0 = End (go to pattern using jump as pattern number)
    //   1 = Next
    //   2 = Jump (to frame using jump as frame number)

    // Map HA4 animation_flow to HA6 aniType
    if (animation_flow == 0) {
        // Ordinance/End - go to pattern
        frame->AF.aniType = 0;
        frame->AF.jump = (jump_target != -1) ? jump_target : 0;  // Pattern number
    } else if (animation_flow == 1) {
        // Next frame
        frame->AF.aniType = 1;
        frame->AF.jump = -1;
    } else if (animation_flow == 2) {
        // Jump to frame
        frame->AF.aniType = 2;
        frame->AF.jump = (jump_target != -1) ? jump_target : 0;  // Frame number
    } else if (animation_flow == 3) {
        // NextLanding - Next with landing jump set
        frame->AF.aniType = 1;
        frame->AF.jump = -1;
        frame->AF.landJump = (landing_jump_target != 0xFF) ? landing_jump_target : -1;
    } else if (animation_flow == 4) {
        // JumpLanding - Jump with landing jump set
        frame->AF.aniType = 2;
        frame->AF.jump = (jump_target != -1) ? jump_target : 0;
        frame->AF.landJump = (landing_jump_target != 0xFF) ? landing_jump_target : -1;
    } else {
        // LoopCheck or unknown - default to Next
        frame->AF.aniType = 1;
        frame->AF.jump = -1;
    }

    frame->AF.aniFlag = animation_flow;  // Keep original value for reference

    // Layer priority (+0x0E)
    uint8_t layer_priority = frame_header[0x0E];     // +0x0E: 0-26 render layer (control 1071)
    frame->AF.priority = layer_priority;

    // Additional mapped fields (with unknown semantics)
    // uint16_t unknown_1014 = *(const uint16_t*)(frame_header + 0x14);  // control 1014
    // uint16_t unknown_1013 = *(const uint16_t*)(frame_header + 0x18);  // control 1013
    // uint16_t unknown_1036 = *(const uint16_t*)(frame_header + 0x1A);  // control 1036
    // int16_t unknown_1252 = *(const int16_t*)(frame_header + 0x24);    // control 1252
    // uint8_t unknown_1177 = frame_header[0x28];                        // control 1177 (boolean)
    // uint16_t unknown_1172 = *(const uint16_t*)(frame_header + 0x2A);  // control 1172

    // Set defaults for fields not present in HA4 or unknown mapping
    // NOTE: jump and landJump already set above based on animation_flow!
    frame->AF.interpolationType = 0;
    frame->AF.blend_mode = 0;
    frame->AF.loopCount = 0;
    frame->AF.loopEnd = 0;
    frame->AF.AFRT = 0;

    // Initialize RGBA (default: opaque white)
    frame->AF.rgba[0] = 1.0f;  // R
    frame->AF.rgba[1] = 1.0f;  // G
    frame->AF.rgba[2] = 1.0f;  // B
    frame->AF.rgba[3] = 1.0f;  // A

    // Initialize scale (default: 1.0 = 100%)
    frame->AF.scale[0] = 1.0f;
    frame->AF.scale[1] = 1.0f;

    // Rotation already set above
    if (rotation_flip == 0) {
        frame->AF.rotation[0] = 0.0f;
        frame->AF.rotation[1] = 0.0f;
        frame->AF.rotation[2] = 0.0f;
    }

    // Debug output (only for first 3 patterns to avoid spam)
    // Removed - too much spam
}

// Convert 56-byte layer data to HA6 state parameters
void ConvertLayerData(const uint8_t *layer_data, Frame *frame) {
    // COMPLETE 56-BYTE STRUCTURE MAPPING
    // Based on Python structures.py LayerData class and IDA analysis

    // Boolean flags (+0 to +3)
    bool flag_set_x = layer_data[0x00];      // +0x00: Set X position flag (control 1019)
    bool flag_set_y = layer_data[0x01];      // +0x01: Set Y position flag (control 1020)
    bool flag_1034 = layer_data[0x02];       // +0x02: Boolean flag (control 1034)
    bool flag_1033 = layer_data[0x03];       // +0x03: Boolean flag (control 1033)

    // Velocity (+8, +10) - CONFIRMED
    int16_t velocity_x = *(const int16_t*)(layer_data + 0x08);  // +0x08: X velocity (control 1015)
    int16_t velocity_y = *(const int16_t*)(layer_data + 0x0A);  // +0x0A: Y velocity (control 1017)
    frame->AS.speed[0] = velocity_x;
    frame->AS.speed[1] = velocity_y;

    // Acceleration (+16, +18) - CONFIRMED
    int16_t accel_x = *(const int16_t*)(layer_data + 0x10);     // +0x10: X acceleration (control 1016)
    int16_t accel_y = *(const int16_t*)(layer_data + 0x12);     // +0x12: Y acceleration (control 1018)
    frame->AS.accel[0] = accel_x;
    frame->AS.accel[1] = accel_y;

    // State parameters (+24 to +26) - CONFIRMED
    uint8_t stance_state = layer_data[0x18];         // +0x18: Ground/air/crouch (control 1044)
                                                      // 0=地上 (ground), 1=空中 (air), 2=しゃがみ (crouch)
    uint8_t cancel_condition_1 = layer_data[0x19];   // +0x19: Cancel timing (control 1045)
                                                      // 0=標準, 1=ＨＩＴ時, 2=いつでも, 3=DAMAGE時
    uint8_t cancel_condition_2 = layer_data[0x1A];   // +0x1A: Cancel timing (control 1046)

    frame->AS.stanceState = stance_state;
    frame->AS.cancelNormal = cancel_condition_1;
    frame->AS.cancelSpecial = cancel_condition_2;

    // Additional state fields
    uint8_t unknown_1043 = layer_data[0x1C];         // +0x1C: Unknown (control 1043)
    bool flag_1041 = layer_data[0x1D];               // +0x1D: Boolean flag (control 1041)
    frame->AS.canMove = flag_1041 ? 1 : 0;

    // State flags (32-bit flag fields) - CONFIRMED
    uint32_t state_flags_1 = *(const uint32_t*)(layer_data + 0x24);  // +0x24: Velocity/position flags
    // Bit 0: SetX (control 1040)
    // Bit 1: SetY (control 2074)
    // Bit 2: ClearX (control 1196)
    // Bit 4: VEC (control 1212)
    // Bit 5: ADD (control 1213)
    // Bit 31: MAX (control 1072)

    uint32_t state_flags_2 = *(const uint32_t*)(layer_data + 0x28);  // +0x28: Invincibility/counter flags
    // Bits 0-2: Individual flags (controls 1169, 1175, 1176)
    // Bits 16-19: Invincibility type (control 1180)
    //   0=通常, 1=上段避け, 2=下段避け, 3=打撃無敵, 4=投げ無敵
    // Bits 20-23: Counter hit type (control 1070)
    //   0=該当なし, 1=HI発生, 2=LO発生, 3=消去
    // Bit 31: Flag (control 1164)

    // Map state flags to HA6
    frame->AS.statusFlags[0] = state_flags_1;
    frame->AS.statusFlags[1] = state_flags_2;

    // Extract movement flags from state_flags_1
    frame->AS.movementFlags = 0;
    if (state_flags_1 & 0x01) frame->AS.movementFlags |= 0x01;  // SetX
    if (state_flags_1 & 0x02) frame->AS.movementFlags |= 0x02;  // SetY
    if (state_flags_1 & 0x10) frame->AS.movementFlags |= 0x10;  // VEC
    if (state_flags_1 & 0x20) frame->AS.movementFlags |= 0x20;  // ADD

    // Extract invincibility from state_flags_2 bits 16-19
    frame->AS.invincibility = (state_flags_2 >> 16) & 0x0F;

    // Extract counter type from state_flags_2 bits 20-23
    frame->AS.counterType = (state_flags_2 >> 20) & 0x0F;

    // Additional fields
    int16_t unknown_1073 = *(const int16_t*)(layer_data + 0x30);  // +0x30: Unknown (control 1073)

    // Set defaults for unmapped fields
    frame->AS.sineFlags = 0;         // Not present in HA4 56-byte structure
    frame->AS.maxSpeedX = 0;         // Not present in HA4 56-byte structure
    frame->AS.hitsNumber = 0;        // Not present in HA4 56-byte structure

    memset(frame->AS.sineParameters, 0, sizeof(frame->AS.sineParameters));
    frame->AS.sinePhases[0] = frame->AS.sinePhases[1] = 0.0f;

    // Debug output - removed to reduce spam
}

// Convert HA4 effect to HA6 format (52 bytes, types map 1:1)
void ConvertEffect(const HA4_Effect *ha4_effect, Frame_EF *ha6_effect) {
    ha6_effect->type = ha4_effect->type;
    ha6_effect->number = ha4_effect->number;

    // Copy all 12 parameters
    for (int i = 0; i < 12; i++) {
        ha6_effect->parameters[i] = ha4_effect->param[i];
    }
}

// Convert HA4 condition to HA6 format (52 bytes, types map 1:1)
void ConvertCondition(const HA4_Condition *ha4_condition, Frame_IF *ha6_condition) {
    ha6_condition->type = ha4_condition->type;

    // Copy first 9 parameters (HA6 only uses 9)
    for (int i = 0; i < 9; i++) {
        ha6_condition->parameters[i] = ha4_condition->param[i];
    }
}

// Default pattern names from hantei4.exe (offset 0x27110)
// 40 default names at patterns 0-52 (with some gaps)
// Extracted from the Hantei4 editor binary
std::string GetDefaultHA4PatternName(int pattern_id) {
    switch (pattern_id) {
        case 0: return "立ち";                          // Standing
        case 1: return "立ち弱攻撃";                     // Standing light attack
        case 2: return "立ち中攻撃";                     // Standing medium attack
        case 3: return "立ち強攻撃";                     // Standing heavy attack
        case 4: return "しゃがみ弱攻撃";                 // Crouching light attack
        case 5: return "しゃがみ中攻撃";                 // Crouching medium attack
        case 6: return "しゃがみ強攻撃";                 // Crouching heavy attack
        case 7: return "ジャンプ弱攻撃";                 // Jump light attack
        case 8: return "ジャンプ中攻撃";                 // Jump medium attack
        case 9: return "ジャンプ強攻撃";                 // Jump heavy attack
        case 10: return "前進";                         // Forward walk
        case 11: return "後退";                         // Backward walk
        case 12: return "しゃがみ移行";                 // Crouch transition
        case 13: return "しゃがみ";                     // Crouching
        case 14: return "立ち上がり";                   // Stand up
        case 15: return "立ち振り向き";                 // Turn around (standing)
        case 16: return "しゃがみ振り向き";             // Turn around (crouching)
        case 17: return "立ちガード";                   // Standing guard
        case 18: return "しゃがみガード";               // Crouching guard
        case 19: return "空中ガード";                   // Air guard
        case 23: return "頭やられ";                     // Head hit
        case 24: return "腹やられ";                     // Body hit
        case 25: return "しゃがみやられ";               // Crouch hit
        case 26: return "ダウン";                       // Down
        case 27: return "ダウン中やられ";               // Hit while down
        case 28: return "受け身";                       // Tech/recovery
        case 29: return "前ダウン";                     // Forward down
        case 30: return "エアリアル用ダウン";           // Aerial down
        case 32: return "うつ伏せ→起き上がり";         // Face-down wakeup
        case 33: return "あお向け→起き上がり";         // Face-up wakeup
        case 35: return "前ジャンプ";                   // Forward jump
        case 36: return "垂直ジャンプ";                 // Neutral jump
        case 37: return "後ろジャンプ";                 // Back jump
        case 38: return "前ジャンプ二段目以降";         // Forward double jump+
        case 39: return "垂直ジャンプ二段目以降";       // Neutral double jump+
        case 40: return "後ろジャンプ二段目以降";       // Back double jump+
        case 41: return "エアリアル用ジャンプ";         // Aerial combo jump
        case 50: return "登場";                         // Entrance
        case 51: return "交代";                         // Tag out
        case 52: return "勝ちモーション";               // Victory pose
        default: return "";  // No default for this pattern
    }
}

// Decompress and load one HA4 sequence
bool LoadHA4Sequence(const uint8_t *seq_data, size_t seq_size, Sequence *seq, int seq_index) {
    // Only verbose output for first 3 patterns
    bool verbose = (seq_index < 3);

    if (verbose) {
        std::cout << "HA4: Loading sequence " << seq_index << "\n";
    }

    // Read sequence header (68 bytes)
    if (seq_size < sizeof(HA4_SequenceHeader)) {
        std::cerr << "HA4 Error: Sequence data too small for header\n";
        return false;
    }

    HA4_SequenceHeader seq_hdr;
    memcpy(&seq_hdr, seq_data, sizeof(seq_hdr));

    if (verbose) {
        std::cout << "  Frame count: " << seq_hdr.frame_count << "\n";
        std::cout << "  Frame data offset: " << seq_hdr.frame_data_offset << "\n";
        std::cout << "  Hitbox offset: " << seq_hdr.hitbox_ref_offset << "\n";
        std::cout << "  Effect offset: " << seq_hdr.effect_data_offset << "\n";
        std::cout << "  Condition offset: " << seq_hdr.condition_data_offset << "\n";
        std::cout << "  Vector offset: " << seq_hdr.vector_data_offset << "\n";
    }

    if (seq_hdr.frame_count == 0) {
        seq->empty = true;
        return true;
    }

    // Allocate frames
    seq->frames.clear();
    seq->frames.resize(seq_hdr.frame_count);
    seq->initialized = 1;
    seq->empty = false;
    seq->name = "Sequence " + std::to_string(seq_index);
    seq->codeName = std::to_string(seq_index);

    // Validate offsets are within bounds
    // Note: -1 (0xFFFFFFFF) is VALID in HA4 format - it means "no data present"
    if (seq_hdr.frame_data_offset != -1) {
        if ((size_t)seq_hdr.frame_data_offset >= seq_size) {
            std::cerr << "HA4 Error: Invalid frame_data_offset " << seq_hdr.frame_data_offset << "\n";
            return false;
        }
    }
    if (seq_hdr.hitbox_ref_offset != -1) {
        if ((size_t)seq_hdr.hitbox_ref_offset >= seq_size) {
            std::cerr << "HA4 Error: Invalid hitbox_ref_offset " << seq_hdr.hitbox_ref_offset << "\n";
            return false;
        }
    }
    if (seq_hdr.effect_data_offset != -1) {
        if ((size_t)seq_hdr.effect_data_offset >= seq_size) {
            std::cerr << "HA4 Error: Invalid effect_data_offset " << seq_hdr.effect_data_offset << "\n";
            return false;
        }
    }
    if (seq_hdr.condition_data_offset != -1) {
        if ((size_t)seq_hdr.condition_data_offset >= seq_size) {
            std::cerr << "HA4 Error: Invalid condition_data_offset " << seq_hdr.condition_data_offset << "\n";
            return false;
        }
    }
    if (seq_hdr.vector_data_offset != -1) {
        if ((size_t)seq_hdr.vector_data_offset >= seq_size) {
            std::cerr << "HA4 Error: Invalid vector_data_offset " << seq_hdr.vector_data_offset << "\n";
            return false;
        }
    }

    // Calculate data section pointers (all relative to sequence start!)
    // ALL HA4 FILES: Frames always start at +68 (immediately after 68-byte sequence header)
    // frame_data_offset points to EXTENDED data (layer/hitbox arrays), not compressed frames!
    const uint8_t *frame_array = seq_data + 68;

    const uint8_t *hitbox_data = (seq_hdr.hitbox_ref_offset != -1) ? (seq_data + seq_hdr.hitbox_ref_offset) : nullptr;
    const HA4_Effect *effect_data = (seq_hdr.effect_data_offset != -1) ? (const HA4_Effect*)(seq_data + seq_hdr.effect_data_offset) : nullptr;
    const HA4_Condition *condition_data = (seq_hdr.condition_data_offset != -1) ? (const HA4_Condition*)(seq_data + seq_hdr.condition_data_offset) : nullptr;
    const HA4_Vector *vector_array = (seq_hdr.vector_data_offset != -1) ? (const HA4_Vector*)(seq_data + seq_hdr.vector_data_offset) : nullptr;

    // Calculate max indices for bounds checking
    // Effects and conditions are stored as arrays - need to find size by looking at next section
    size_t effect_array_size = 0;
    size_t condition_array_size = 0;

    // Calculate effect array size - find distance to next section
    if (effect_data != nullptr) {
        // Find the next offset after effect_data_offset
        int32_t next_offset = seq_size;  // Default: end of sequence
        if (seq_hdr.condition_data_offset != -1 && seq_hdr.condition_data_offset > seq_hdr.effect_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.condition_data_offset);
        }
        if (seq_hdr.hitbox_ref_offset != -1 && seq_hdr.hitbox_ref_offset > seq_hdr.effect_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.hitbox_ref_offset);
        }
        if (seq_hdr.vector_data_offset != -1 && seq_hdr.vector_data_offset > seq_hdr.effect_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.vector_data_offset);
        }
        effect_array_size = (next_offset - seq_hdr.effect_data_offset) / sizeof(HA4_Effect);
    }

    // Calculate condition array size - find distance to next section
    if (condition_data != nullptr) {
        // Find the next offset after condition_data_offset
        int32_t next_offset = seq_size;  // Default: end of sequence
        if (seq_hdr.effect_data_offset != -1 && seq_hdr.effect_data_offset > seq_hdr.condition_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.effect_data_offset);
        }
        if (seq_hdr.hitbox_ref_offset != -1 && seq_hdr.hitbox_ref_offset > seq_hdr.condition_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.hitbox_ref_offset);
        }
        if (seq_hdr.vector_data_offset != -1 && seq_hdr.vector_data_offset > seq_hdr.condition_data_offset) {
            next_offset = std::min(next_offset, seq_hdr.vector_data_offset);
        }
        condition_array_size = (next_offset - seq_hdr.condition_data_offset) / sizeof(HA4_Condition);
    }

    if (verbose) {
        std::cout << "  Effect array size: " << effect_array_size << " entries\n";
        std::cout << "  Condition array size: " << condition_array_size << " entries\n";
    }

    // Determine if this is a CSEL file (frame_data_offset=-1)
    bool is_csel_file = (seq_hdr.frame_data_offset == -1);

    // Process each frame
    for (uint32_t frame_idx = 0; frame_idx < seq_hdr.frame_count; frame_idx++) {
        // Each compressed frame is 216 bytes
        const uint8_t *compressed_frame = frame_array + (frame_idx * 216);
        Frame *frame = &seq->frames[frame_idx];

        // Parse compressed frame structure
        const uint8_t *frame_header = compressed_frame;           // 44 bytes at +0
        const uint8_t *layer_data = compressed_frame + 44;        // 56 bytes at +44
        const int16_t *indices = (const int16_t*)(compressed_frame + 100);  // Indices at +100

        // Convert frame header and layer data
        ConvertFrameHeader(frame_header, frame, is_csel_file);
        ConvertLayerData(layer_data, frame);

        // Parse index arrays (starting at +100)
        int16_t hitbox_idx = indices[0];                // +100
        const int16_t *effect_indices = &indices[1];    // +102 (8 indices)
        const int16_t *condition_indices = &indices[9]; // +118 (8 indices)
        // Note: Vector indices at +134 onwards (need to map these)

        // Expand hitbox (88 bytes)
        if (hitbox_data != nullptr && hitbox_idx != -1 && hitbox_idx >= 0 && (size_t)(hitbox_idx * 88) < (seq_size - seq_hdr.hitbox_ref_offset)) {
            const uint8_t *hitbox_ptr = hitbox_data + (hitbox_idx * 88);

            // Parse 88-byte hitbox structure
            // Based on Python structures.py HitboxData class - 100% structure mapped

            // Core collision rectangle (+0x24 to +0x2A) - 2-byte aligned
            int16_t x = *(const int16_t*)(hitbox_ptr + 0x24);      // +0x24: X position (control 1073)
            int16_t y = *(const int16_t*)(hitbox_ptr + 0x26);      // +0x26: Y position (control 1074)
            int16_t width = *(const int16_t*)(hitbox_ptr + 0x28);  // +0x28: Width (control 1077)
            int16_t height = *(const int16_t*)(hitbox_ptr + 0x2A); // +0x2A: Height (control 1075)

            // Combat parameters
            uint8_t damage = hitbox_ptr[0x3B];                     // +0x3B: Damage value 0-255 (control 1095)
            uint8_t counter_hit_type = hitbox_ptr[0x3C];          // +0x3C: Counter hit timing 0-3 (control 1070)

            // Hitbox classification and rendering
            int16_t hitbox_type = *(const int16_t*)(hitbox_ptr + 0x14);  // +0x14: Hitbox type (control 1069)
            uint8_t layer_priority = hitbox_ptr[0x15];            // +0x15: Layer z-order (control 1071)
            uint8_t rotation_flip = hitbox_ptr[0x38];             // +0x38: Rotation/flip 0-9 (control 1072)

            // Bit flags
            int16_t flags_1 = *(const int16_t*)(hitbox_ptr + 0x00);      // +0x00: 14 bit flags (controls 1199-1208)
            uint32_t flags_2 = *(const uint32_t*)(hitbox_ptr + 0x10);    // +0x10: 32-bit flags (controls 1079-1249)

            // Determine if this is an attack box (HRAT) or hurt box (HRNM)
            // HA4 stores hitbox_type to classify: attack boxes have higher type values
            bool is_attack = (hitbox_type >= 10);  // Simplified heuristic

            // Find a free hitbox slot in HA6 format
            // HA6 has 33 slots: 0-24 for hurt boxes (HRNM), 25-32 for attack boxes (HRAT)
            // hitboxes is a std::map - check if key exists
            int slot = -1;
            if (is_attack) {
                // Attack box: slots 25-32
                for (int s = 25; s < 33; s++) {
                    if (frame->hitboxes.find(s) == frame->hitboxes.end()) {
                        slot = s;
                        break;
                    }
                }
            } else {
                // Hurt box: slots 0-24
                for (int s = 0; s < 25; s++) {
                    if (frame->hitboxes.find(s) == frame->hitboxes.end()) {
                        slot = s;
                        break;
                    }
                }
            }

            if (slot != -1) {
                Hitbox hitbox;
                hitbox.xy[0] = x;
                hitbox.xy[1] = y;
                hitbox.xy[2] = width;
                hitbox.xy[3] = height;
                frame->hitboxes[slot] = hitbox;

                // Debug output
                if (verbose) {
                    std::cout << "      Hitbox[" << hitbox_idx << "]→slot[" << slot << "]: "
                              << "(" << x << "," << y << "," << width << "," << height << ") "
                              << "dmg=" << (int)damage << " type=" << hitbox_type << "\n";
                }
            }
        }

        // Expand effects (52 bytes each, max 8) with bounds checking
        if (effect_data != nullptr) {
            for (int i = 0; i < 8; i++) {
                if (effect_indices[i] != -1) {
                    if (effect_indices[i] >= 0 && (size_t)effect_indices[i] < effect_array_size) {
                        frame->EF.push_back({});
                        ConvertEffect(&effect_data[effect_indices[i]], &frame->EF.back());
                    } else {
                        std::cerr << "HA4 Warning: Invalid effect index " << effect_indices[i]
                                  << " (max: " << effect_array_size << ") in frame " << frame_idx << "\n";
                    }
                }
            }
        }

        // Expand conditions (52 bytes each, max 8) with bounds checking
        if (condition_data != nullptr) {
            for (int i = 0; i < 8; i++) {
                if (condition_indices[i] != -1) {
                    if (condition_indices[i] >= 0 && (size_t)condition_indices[i] < condition_array_size) {
                        frame->IF.push_back({});
                        ConvertCondition(&condition_data[condition_indices[i]], &frame->IF.back());
                    } else {
                        std::cerr << "HA4 Warning: Invalid condition index " << condition_indices[i]
                                  << " (max: " << condition_array_size << ") in frame " << frame_idx << "\n";
                    }
                }
            }
        }

        // Load collision boxes from vector array
        // Vector indices are at +134 onwards in the 216-byte compressed frame
        const int16_t *vector_indices = (const int16_t*)(compressed_frame + 134);
        int16_t main_vec_idx = vector_indices[0];  // +134: Main collision box

        if (vector_array != nullptr && main_vec_idx != -1 && main_vec_idx >= 0) {
            // Load vector from array (8 bytes each)
            const HA4_Vector *vec = &vector_array[main_vec_idx];

            // Convert from (start_x, start_y, end_x, end_x) to (x, y, width, height)
            int16_t x = vec->start_x;
            int16_t y = vec->start_y;
            int16_t width = vec->end_x - vec->start_x;
            int16_t height = vec->end_y - vec->start_y;

            // Skip empty vectors (width/height of 0 or negative)
            if (width > 0 && height > 0) {
                // Find first available hitbox slot (hurt box slots 0-24)
                // hitboxes is a std::map, not an array!
                int slot = -1;
                for (int s = 0; s < 25; s++) {
                    if (frame->hitboxes.find(s) == frame->hitboxes.end()) {
                        slot = s;
                        break;
                    }
                }

                if (slot != -1) {
                    Hitbox box;
                    box.xy[0] = x;
                    box.xy[1] = y;
                    box.xy[2] = width;
                    box.xy[3] = height;
                    frame->hitboxes[slot] = box;

                    if (verbose) {
                        std::cout << "      Vector[" << main_vec_idx << "]→slot[" << slot << "]: "
                                  << "(" << x << "," << y << ","  << width << "," << height << ")\n";
                    }
                }
            }
        }
    }

    if (verbose) {
        std::cout << "  Loaded " << seq_hdr.frame_count << " frames\n";
    }
    return true;
}

// Main HA4 binary loader entry point
unsigned int *fd_main_load_ha4_binary(unsigned int *data_ptr, const unsigned int *data_end,
                                      std::vector<Sequence> &sequences, unsigned int nsequences) {
    const uint8_t *data = (const uint8_t*)data_ptr;
    size_t data_size = (const uint8_t*)data_end - data;

    std::cout << "HA4 Binary Loader: Loading file (" << data_size << " bytes)\n";

    // Read file header
    HA4_BinaryFileHeader header;
    if (!ReadHA4FileHeader(data, data_size, &header)) {
        return data_ptr;
    }

    // Read offset table
    int32_t offsets[256];
    if (!ReadHA4OffsetTable(data, data_size, offsets)) {
        return data_ptr;
    }

    // Load each non-empty sequence
    // IMPORTANT: Pattern indices should start from 0, even if offset[0] is skipped!
    // We need to map offset table indices to sequential pattern indices
    int loaded_count = 0;
    int pattern_index = 0;  // Separate counter for pattern numbering (0-based)

    for (int i = 0; i < 256 && pattern_index < (int)nsequences; i++) {
        if (offsets[i] != -1 && offsets[i] != 0) {
            // Offsets are ABSOLUTE file offsets, not relative to 0x440!
            const uint8_t *seq_data = data + offsets[i];

            // Calculate sequence size (distance to next sequence or end)
            size_t seq_size = header.anim_data_size - offsets[i];
            for (int j = i + 1; j < 256; j++) {
                if (offsets[j] != -1 && offsets[j] != 0) {
                    seq_size = offsets[j] - offsets[i];
                    break;
                }
            }

            // Store into sequential pattern index (0, 1, 2, ...), not offset table index (i)
            if (LoadHA4Sequence(seq_data, seq_size, &sequences[pattern_index], pattern_index)) {
                loaded_count++;
            }
            pattern_index++;  // Increment pattern counter for next valid sequence
        }
    }

    // Mark remaining patterns as empty
    for (int i = pattern_index; i < (int)nsequences; i++) {
        sequences[i].empty = true;
    }

    std::cout << "HA4: Loaded " << loaded_count << " sequences successfully\n";

    // Try to load pattern names from string section
    // Strings are stored AFTER image data
    // Each string is 64 bytes, Shift-JIS encoded, null-terminated
    const size_t STRING_SIZE = 64;

    // Calculate string section offset - CORRECTED FORMULA
    // Pattern names are located at: header(0x00) + animation_data + image_data
    // This is simpler than before: just 0x00 + anim_size + image_size
    const uint8_t *string_section = data + 0x00 + header.anim_data_size + header.image_data_size;
    size_t remaining_size = data_size - (header.anim_data_size + header.image_data_size);

    // Pattern names can be partial - file may not have full 256 × 64 bytes
    // Calculate how many pattern name slots are available
    size_t slots_available = std::min(256, (int)(remaining_size / STRING_SIZE));

    if (slots_available > 0) {
        std::cout << "HA4: Found string section (" << slots_available << " slots), loading pattern names...\n";

        int names_loaded = 0;
        // Loop through ALL 256 patterns (not just loaded ones)
        for (int i = 0; i < 256 && i < (int)nsequences; i++) {
            std::string pattern_name;

            if (i < (int)slots_available) {
                // Try to read pattern name from file
                const uint8_t *string_data = string_section + (i * STRING_SIZE);

                // Find null terminator
                size_t str_len = 0;
                for (size_t j = 0; j < STRING_SIZE; j++) {
                    if (string_data[j] == 0) {
                        str_len = j;
                        break;
                    }
                }
                if (str_len == 0) str_len = STRING_SIZE;

                // Extract raw Shift-JIS bytes
                std::string shiftjis_str((const char*)string_data, str_len);

                // Convert Shift-JIS to UTF-8 using sj2utf8() function
                std::string utf8_name = sj2utf8(shiftjis_str);

                // Remove trailing whitespace
                while (!utf8_name.empty() && (utf8_name.back() == ' ' || utf8_name.back() == '\t'))
                    utf8_name.pop_back();

                pattern_name = utf8_name;
            }

            // Use file name if present, otherwise fall back to default
            if (pattern_name.empty()) {
                pattern_name = GetDefaultHA4PatternName(i);
            }

            // Apply if valid
            if (!pattern_name.empty()) {
                sequences[i].name = pattern_name;
                sequences[i].codeName = std::to_string(i);
                names_loaded++;
            }
        }

        std::cout << "HA4: Loaded " << names_loaded << " pattern names (including defaults)\n";
    } else {
        // No string section - use all defaults
        std::cout << "HA4: No string section found, using default pattern names\n";
        int names_loaded = 0;
        for (int i = 0; i < 256 && i < (int)nsequences; i++) {
            std::string default_name = GetDefaultHA4PatternName(i);
            if (!default_name.empty()) {
                sequences[i].name = default_name;
                sequences[i].codeName = std::to_string(i);
                names_loaded++;
            }
        }
        std::cout << "HA4: Applied " << names_loaded << " default pattern names\n";
    }

    // Return pointer to end of animation data (offset 0x440 + animation size)
    return (unsigned int*)(data + 0x440 + header.anim_data_size);
}
