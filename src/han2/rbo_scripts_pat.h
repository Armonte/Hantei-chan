// RBO (rbo.exe, French-Bread engine) script-list / PAT / CG structures.
// Same layouts as the IDA types of the same names in C:\games\rbo\rbo.exe.i64. See rbo_scripts_pat.md for evidence.
// Little-endian, packed; every struct is checked with static_asserts.
#pragma once
#include <cstddef>
#include <cstdint>

#pragma pack(push, 1)

// ---------------------------------------------------------------- enums
// Part flip flags (dword at part+0x10). The engine (PosePart_GetFlippedSrcRect) reads ONLY the low byte, bits 0 and 1.
// Bits 0x100 / 0x10000 occur in shipped data (28 + a few parts) but nothing in rbo.exe reads them.
enum RboPatPartFlip : uint32_t {
    RBO_PART_FLIP_NONE = 0,
    RBO_PART_FLIP_X = 0x1,           // mirror source rect horizontally
    RBO_PART_FLIP_Y = 0x2,           // mirror source rect vertically
    RBO_PART_FLIP_EDITOR_BIT8 = 0x100,    // data only, engine ignores
    RBO_PART_FLIP_EDITOR_BIT16 = 0x10000, // data only, engine ignores
};

// "kind" argument of Script_ResolveIndexedFunction(bank, kind, id) = index into the .FOB index-type tables.
enum RboScriptKind : uint32_t {
    RBO_SCRIPT_KIND_FRAME_ENTER_ACTIONS = 0, // section 6 lists (Actor_RunFrameScriptList6); table of 10000 ids
    RBO_SCRIPT_KIND_TRANSITION_RULES = 1,    // section 7 lists (Actor_RunFrameScriptList7); table of 2001 ids
    RBO_SCRIPT_KIND_VARIABLE = 2,            // chosen by data (AT-record condition triples, caller-supplied); 19 ids in ACOLYTE_F
    RBO_SCRIPT_KIND_SYSTEM_HOOK = 3,         // fixed ids 0/1 (sub_43D980 / sub_43DA10); 2 ids in ACOLYTE_F
};

// .FOB storage type of a CG effect image (dword at effect+0x20), decoded by CgEffect_InitPixelDecoder / CgPixel_Decode.
enum RboCgStorageType : uint32_t {
    RBO_CG_STORE_SHARED_PALETTE_8BIT = 0,        // 1 byte/pixel index into palette set chosen at load; index 0 = transparent
    RBO_CG_STORE_BGRA32 = 1,                     // 4 bytes/pixel B,G,R,A
    RBO_CG_STORE_OWN_PALETTE_8BIT = 2,           // 1024-byte BGRA palette at +0x48, indices at +0x448; index 0 = transparent
    RBO_CG_STORE_SOLID_COLOR_ALPHA8 = 3,         // 3-byte colour at +0x48 (pad +0x4B), 1 byte/pixel alpha at +0x4C
    RBO_CG_STORE_OWN_PALETTE_PLUS_ALPHA = 4,     // palette at +0x48, index plane at +0x448, alpha plane follows the index plane
    RBO_CG_STORE_SHARED_PALETTE_PLUS_ALPHA = 5,  // pixel data at +0x148 (shared palette); never present in shipped data
};

// Category assigned to a frame-enter (section 6) event record by g_FrameEnterEventCategory[recordType].
enum RboFrameEnterCategory : int32_t {
    RBO_FE_CAT_NONE = -1,
    RBO_FE_CAT_ACTIONS = 0,          // actor+0x6B8 list, run by Actor_RunFrameEnterActions (types 126..133, 233..241, 243..248, 250, 251)
    RBO_FE_CAT_ATTACKER_MODIFIERS = 1, // actor+0x6C4 list, read by hit resolution (types 252, 253)
    RBO_FE_CAT_UNLINKED_2 = 2,       // type 249: node only stores record pointer
    RBO_FE_CAT_UNLINKED_3 = 3,       // type 1: node only stores record pointer
    RBO_FE_CAT_CHANCE_ACTIONS = 4,   // actor+0x6D0 list, Actor_RunChanceFrameEnterActions (type 242)
};

// Category assigned to a transition-rule (section 7) event record by g_TransitionEventCategory[recordType].
enum RboTransitionCategory : int32_t {
    RBO_TR_CAT_NONE = -1,
    RBO_TR_CAT_INPUT_RULES = 0,       // actor+0x688 (types 11,13,21,127,128,197,201)  Actor_RunInputTransitionRules
    RBO_TR_CAT_CONDITIONAL_JUMPS = 1, // actor+0x698 (types 15,17,123,125,200)          Actor_RunConditionalJumpList
    RBO_TR_CAT_SPAWN_EVENTS = 2,      // actor+0x690 (type 14)                           Actor_SpawnHitEvent path (sub_41F560)
    RBO_TR_CAT_TICK_RULES = 3,        // actor+0x6A0 (types 20,50,52,196,198,199,202)
    RBO_TR_CAT_FORCED_JUMPS = 4,      // actor+0x6A8 (types 16,53)                       Actor_RunForcedJumpRules / Actor_RunTimedJumpRules
    RBO_TR_CAT_UNLINKED_5 = 5,        // type 249
    RBO_TR_CAT_SINGLE_SLOT = 6,       // type 122 -> actor+0x56C, returns 2
    RBO_TR_CAT_END_RULES = 7,         // actor+0x6B0 (types 51,124,126,203)              Actor_RunEndTransitionRules
};

// ---------------------------------------------------------------- pattern-area sections
// Section 6 / 7 element: five script ids. Frame record +0xBC (188) = record index into section 6, +0xC0 (192) into section 7;
// record 0 is the reserved all-zero record (index 0 = "none"). Ids run in order slot[4], slot[3] ... slot[0]; id 0 = unused.
struct RboScriptListEntry {
    int32_t script_id[5];
};
static_assert(sizeof(RboScriptListEntry) == 20, "RboScriptListEntry");

// Section 4: 28-byte records, 1-based index from frame record +0xD0 (208). NO reader in rbo.exe (see md); layout from data.
struct RboSection4Record {
    int32_t event_type;   // +0x00  same id space as script event records (2,3,1,9,10,14,15,11,249 ...)
    int32_t param_a;      // +0x04
    int32_t param_b;      // +0x08
    int32_t param_c;      // +0x0C
    int32_t param_d;      // +0x10  0 in 2948/3115 records, else 3/10/1
    int32_t reserved_14;  // +0x14  0 in all 3115 shipped records
    int32_t reserved_18;  // +0x18  0 in all 3115 shipped records
};
static_assert(sizeof(RboSection4Record) == 28, "RboSection4Record");
static_assert(offsetof(RboSection4Record, event_type) == 0x00 && offsetof(RboSection4Record, param_a) == 0x04 &&
              offsetof(RboSection4Record, param_b) == 0x08 && offsetof(RboSection4Record, param_c) == 0x0C &&
              offsetof(RboSection4Record, param_d) == 0x10 && offsetof(RboSection4Record, reserved_14) == 0x14 &&
              offsetof(RboSection4Record, reserved_18) == 0x18, "RboSection4Record offsets");

// Section 5: 20-byte records, 1-based index from frame record +0xE0 (224). NO reader in rbo.exe; layout from data.
struct RboSection5Record {
    uint32_t packed_type_bytes; // +0x00  four u8: [0]=event type (1,3,4,6,9,249..), [1]=param, [2]=param, [3]=flags
    int32_t param_a;            // +0x04
    int32_t param_b;            // +0x08
    int32_t reserved_0C;        // +0x0C  0 in all 2522 shipped records
    int32_t reserved_10;        // +0x10  0 in all 2522 shipped records
};
static_assert(sizeof(RboSection5Record) == 20, "RboSection5Record");
static_assert(offsetof(RboSection5Record, packed_type_bytes) == 0x00 && offsetof(RboSection5Record, param_a) == 0x04 &&
              offsetof(RboSection5Record, param_b) == 0x08 && offsetof(RboSection5Record, reserved_0C) == 0x0C &&
              offsetof(RboSection5Record, reserved_10) == 0x10, "RboSection5Record offsets");

// ---------------------------------------------------------------- PAT (parts) block, area 2 of the HAN2RBO container
struct RboPatHeader {          // first 24 bytes; Han2Dat_LoadPartsPatBlock never reads them (only decrypts); constant in all 208 PATs
    int32_t version;           // +0x00  always 3
    uint32_t byte_order_marker;// +0x04  always 0x01234567
    int32_t reserved_08;       // +0x08  0
    int32_t reserved_0C;       // +0x0C  0
    int32_t reserved_10;       // +0x10  0
    int32_t reserved_14;       // +0x14  0
};
static_assert(sizeof(RboPatHeader) == 24, "RboPatHeader");
static_assert(offsetof(RboPatHeader, version) == 0 && offsetof(RboPatHeader, byte_order_marker) == 4 &&
              offsetof(RboPatHeader, reserved_08) == 8 && offsetof(RboPatHeader, reserved_0C) == 12 &&
              offsetof(RboPatHeader, reserved_10) == 16 && offsetof(RboPatHeader, reserved_14) == 20, "RboPatHeader offsets");

// One part of a pose (92 bytes). A pose is exactly 40 consecutive parts (Actor_DrawPartsPose iterates 40).
// Unused slot: src_w == 0 or src_h == 0 (dwords) -> skipped.
struct RboPatPart {
    int32_t x;                 // +0x00  translate X, sprite pixels, relative to actor anchor; lerped between frames
    int32_t y;                 // +0x04  translate Y, pixels, +down; lerped
    int32_t dest_width;        // +0x08  quad width in pixels before scale (taken from the CURRENT pose part, not lerped)
    int32_t dest_height;       // +0x0C  quad height in pixels before scale
    RboPatPartFlip flip_flags; // +0x10  only bits 0/1 used
    int32_t scale_x_permille;  // +0x14  1000 = 1.0 (x 0.001); lerped
    int32_t scale_y_permille;  // +0x18  1000 = 1.0; lerped
    int32_t rotation_10000;    // +0x1C  10000 = 360 degrees (x0.036 -> degrees), shortest-arc lerp, about the origin pivot
    uint32_t modulate_argb_bytes; // +0x20  bytes in memory order A,R,G,B (A=+0x20,R=+0x21,G=+0x22,B=+0x23); 0xFFFFFFFF = no change
    uint32_t add_color_rgb;    // +0x24  bytes R=+0x24,G=+0x25,B=+0x26 added (clamped 255) as specular; +0x27 unused (0 in data)
    int32_t texture_index;     // +0x28  0..49 PAT texture slot; 0xFFFF = clip-rectangle part (unsupported: pose is skipped)
    int16_t src_x;             // +0x2C  source rect X in 1/256ths of the texture width (UV = v/256)
    int16_t src_x_hi;          // +0x2E  high word, not read by the engine, 0 in data
    int16_t src_y;             // +0x30  source rect Y, 1/256ths of texture height
    int16_t src_y_hi;          // +0x32
    int16_t src_w;             // +0x34  source rect width, 1/256ths; (dword) 0 = slot unused
    int16_t src_w_hi;          // +0x36
    int16_t src_h;             // +0x38  source rect height, 1/256ths; (dword) 0 = slot unused
    int16_t src_h_hi;          // +0x3A
    uint8_t layer;             // +0x3C  sort key (draw order ascending layer, then part index)
    uint8_t layer_hi[3];       // +0x3D  not read, 0 in data
    int32_t origin_x;          // +0x40  pivot X offset from the part top-left in pixels (scale and rotation centre); lerped
    int32_t origin_y;          // +0x44  pivot Y offset; lerped
    int32_t reserved_48;       // +0x48  never read; 0 in 998720/998720 shipped parts
    int32_t reserved_4C;       // +0x4C  same
    int32_t reserved_50;       // +0x50  same
    int32_t reserved_54;       // +0x54  same
    int32_t reserved_58;       // +0x58  same
};
static_assert(sizeof(RboPatPart) == 92, "RboPatPart");
static_assert(offsetof(RboPatPart, x) == 0x00 && offsetof(RboPatPart, y) == 0x04 && offsetof(RboPatPart, dest_width) == 0x08 &&
              offsetof(RboPatPart, dest_height) == 0x0C && offsetof(RboPatPart, flip_flags) == 0x10 &&
              offsetof(RboPatPart, scale_x_permille) == 0x14 && offsetof(RboPatPart, scale_y_permille) == 0x18 &&
              offsetof(RboPatPart, rotation_10000) == 0x1C && offsetof(RboPatPart, modulate_argb_bytes) == 0x20 &&
              offsetof(RboPatPart, add_color_rgb) == 0x24 && offsetof(RboPatPart, texture_index) == 0x28 &&
              offsetof(RboPatPart, src_x) == 0x2C && offsetof(RboPatPart, src_x_hi) == 0x2E &&
              offsetof(RboPatPart, src_y) == 0x30 && offsetof(RboPatPart, src_y_hi) == 0x32 &&
              offsetof(RboPatPart, src_w) == 0x34 && offsetof(RboPatPart, src_w_hi) == 0x36 &&
              offsetof(RboPatPart, src_h) == 0x38 && offsetof(RboPatPart, src_h_hi) == 0x3A &&
              offsetof(RboPatPart, layer) == 0x3C && offsetof(RboPatPart, layer_hi) == 0x3D &&
              offsetof(RboPatPart, origin_x) == 0x40 && offsetof(RboPatPart, origin_y) == 0x44 &&
              offsetof(RboPatPart, reserved_48) == 0x48 && offsetof(RboPatPart, reserved_4C) == 0x4C &&
              offsetof(RboPatPart, reserved_50) == 0x50 && offsetof(RboPatPart, reserved_54) == 0x54 &&
              offsetof(RboPatPart, reserved_58) == 0x58, "RboPatPart offsets");

struct RboPatPose {
    RboPatPart parts[40];
};
static_assert(sizeof(RboPatPose) == 3680, "RboPatPose");

// Start of the PAT area (36028 bytes) up to the first pose.
struct RboPatFileHead {
    RboPatHeader header;                  // +0x0000
    uint32_t pose_offset[1000];           // +0x0018  PAT-relative byte offset of the pose's first part; 0 = pose absent; (off-36028)/92 is the part index
    char pose_name[1000][32];             // +0x0FB8  Shift-JIS NUL-padded labels; never read by the engine (editor/file only)
    uint32_t image_area_offset;           // +0x8CB8  PAT-relative offset of the texture area; poses occupy [36028, image_area_offset) contiguously
};
static_assert(sizeof(RboPatFileHead) == 36028, "RboPatFileHead");
static_assert(offsetof(RboPatFileHead, header) == 0 && offsetof(RboPatFileHead, pose_offset) == 0x18 &&
              offsetof(RboPatFileHead, pose_name) == 0xFB8 && offsetof(RboPatFileHead, image_area_offset) == 0x8CB8, "RboPatFileHead offsets");

// Start of the texture area (11824 bytes) followed by the texture pixels.
struct RboPatImageHead {
    int32_t reserved_00[5];               // +0x0000  0 in all 208 PATs (never read)
    int32_t one_05;                       // +0x0014  1 in all 208 PATs (never read)
    int32_t texture_count;                // +0x0018  number of present textures (1..4 in data; never read, derived)
    uint32_t texture_offset[50];          // +0x001C  offset of the texture pixels from the image-area start; 0 = absent
    char texture_name[50][64];            // +0x00E4  Shift-JIS file names; never read by the engine
    int32_t texture_size_px[50];          // +0x0D64  square side in pixels (256 or 512; 0 if absent)
    uint8_t reserved_block[8196];         // +0x0E2C  all zero in all 208 PATs, never read
};
static_assert(sizeof(RboPatImageHead) == 11824, "RboPatImageHead");
static_assert(offsetof(RboPatImageHead, reserved_00) == 0 && offsetof(RboPatImageHead, one_05) == 0x14 &&
              offsetof(RboPatImageHead, texture_count) == 0x18 && offsetof(RboPatImageHead, texture_offset) == 0x1C &&
              offsetof(RboPatImageHead, texture_name) == 0xE4 && offsetof(RboPatImageHead, texture_size_px) == 0xD64 &&
              offsetof(RboPatImageHead, reserved_block) == 0xE2C, "RboPatImageHead offsets");
// Texture pixel data (one per present texture): texture_size_px^2 * 4 bytes, rows top-down, bytes B,G,R,A
// (copied verbatim into an A8R8G8B8 surface by Texture_UploadBgra). Pixels start at image-area + 11824 and are packed back to back.

// ---------------------------------------------------------------- CG (BMP Cutter3) block, area 3
struct RboCgFileHead {
    char signature[20];                   // +0x0000 "BMP Cutter3\0\0\0\0\0\1\0\0\0"
    uint8_t palette[8][1024];             // +0x0014 eight 256-entry B,G,R,A palettes; Han2Dat_Load's `a3` selects one; entry 0 forced transparent
    int32_t reserved_2014;                // +0x2014 count-like (1..7), never read
    int32_t reserved_2018;                // +0x2018 0, never read
    int32_t block_count;                  // +0x201C number of RboCgBlock records
    uint8_t reserved_2020[36];            // +0x2020 (e.g. 0x21,0x10,0...) never read
    int32_t effect_offset[3000];          // +0x2044 offset of the effect record from the CG area start; -1 = none
    int32_t blocks_offset;                // +0x4F24 offset of the RboCgBlock array
    int32_t reserved_4F28;                // +0x4F28 0 in all 80 CG areas, never read
    int32_t after_blocks_offset;          // +0x4F2C == blocks_offset + 24*block_count, never read
};
static_assert(sizeof(RboCgFileHead) == 20272, "RboCgFileHead");
static_assert(offsetof(RboCgFileHead, signature) == 0 && offsetof(RboCgFileHead, palette) == 0x14 &&
              offsetof(RboCgFileHead, reserved_2014) == 0x2014 && offsetof(RboCgFileHead, reserved_2018) == 0x2018 &&
              offsetof(RboCgFileHead, block_count) == 0x201C && offsetof(RboCgFileHead, reserved_2020) == 0x2020 &&
              offsetof(RboCgFileHead, effect_offset) == 0x2044 && offsetof(RboCgFileHead, blocks_offset) == 0x4F24 &&
              offsetof(RboCgFileHead, reserved_4F28) == 0x4F28 && offsetof(RboCgFileHead, after_blocks_offset) == 0x4F2C, "RboCgFileHead offsets");

struct RboCgEffectRecord {
    char name[32];                        // +0x00 Shift-JIS, unread
    RboCgStorageType storage_type;        // +0x20 0xFFFFFFFF occurs on 18 placeholder records
    int32_t src_width;                    // +0x24 source bitmap width, unread (inferred)
    int32_t src_height;                   // +0x28 unread (inferred)
    int32_t bits_per_pixel;               // +0x2C 32 in all 5513 records, unread
    int32_t bbox_left;                    // +0x30 unread (inferred bounding box of opaque area)
    int32_t bbox_top;                     // +0x34
    int32_t bbox_right;                   // +0x38
    int32_t bbox_bottom;                  // +0x3C
    uint32_t block_start;                 // +0x40 first RboCgBlock of this effect
    uint16_t block_count;                 // +0x44 number of blocks
    uint16_t reserved_46;                 // +0x46 0 in all records
    // pixel data follows at +0x48 (layout by storage_type)
};
static_assert(sizeof(RboCgEffectRecord) == 72, "RboCgEffectRecord");
static_assert(offsetof(RboCgEffectRecord, name) == 0 && offsetof(RboCgEffectRecord, storage_type) == 0x20 &&
              offsetof(RboCgEffectRecord, src_width) == 0x24 && offsetof(RboCgEffectRecord, src_height) == 0x28 &&
              offsetof(RboCgEffectRecord, bits_per_pixel) == 0x2C && offsetof(RboCgEffectRecord, bbox_left) == 0x30 &&
              offsetof(RboCgEffectRecord, bbox_top) == 0x34 && offsetof(RboCgEffectRecord, bbox_right) == 0x38 &&
              offsetof(RboCgEffectRecord, bbox_bottom) == 0x3C && offsetof(RboCgEffectRecord, block_start) == 0x40 &&
              offsetof(RboCgEffectRecord, block_count) == 0x44 && offsetof(RboCgEffectRecord, reserved_46) == 0x46, "RboCgEffectRecord offsets");

struct RboCgBlock {   // tile of an effect image placed on screen (Actor_DrawCgSprite) and blitted into a 256x256 texture
    int32_t dest_x;          // +0x00 pixel offset of the tile in the sprite
    int32_t dest_y;          // +0x04
    int32_t width;           // +0x08 tile width px (also texel width in the 256 texture)
    int32_t height;          // +0x0C
    int16_t src_x;           // +0x10 position inside the 256x256 texture (px)
    int16_t src_y;           // +0x12
    uint16_t texture_slot;   // +0x14 index into the CG texture array
    uint16_t reserved_16;    // +0x16 unread
};
static_assert(sizeof(RboCgBlock) == 24, "RboCgBlock");
static_assert(offsetof(RboCgBlock, dest_x) == 0 && offsetof(RboCgBlock, dest_y) == 4 && offsetof(RboCgBlock, width) == 8 &&
              offsetof(RboCgBlock, height) == 12 && offsetof(RboCgBlock, src_x) == 0x10 && offsetof(RboCgBlock, src_y) == 0x12 &&
              offsetof(RboCgBlock, texture_slot) == 0x14 && offsetof(RboCgBlock, reserved_16) == 0x16, "RboCgBlock offsets");

// ---------------------------------------------------------------- runtime containers (32-bit pointers)
struct RboPatRuntime {                    // Han2Dat+0x30, filled by Han2Dat_LoadPartsPatBlock (size 4220)
    int32_t file_offset;                  // +0x0000 area offset in the .DAT
    int32_t file_size;                    // +0x0004
    int32_t image_area_offset;            // +0x0008 from RboPatFileHead
    int32_t image_area_size;              // +0x000C file_size - image_area_offset
    int32_t pose_first_part[1000];        // +0x0010 (pose_offset-36028)/92, or -1 when absent
    uint32_t pose_data;                   // +0x0FB0 pointer to the RboPatPart array (parts stored contiguously)
    uint32_t texture[50];                 // +0x0FB4 IDirect3DTexture pointers created by Han2Dat_LoadPatTextures
};
static_assert(sizeof(RboPatRuntime) == 4220, "RboPatRuntime");
static_assert(offsetof(RboPatRuntime, pose_first_part) == 0x10 && offsetof(RboPatRuntime, pose_data) == 0xFB0 &&
              offsetof(RboPatRuntime, texture) == 0xFB4, "RboPatRuntime offsets");

struct RboCgEffectRef { int32_t first_block; int32_t block_count; };
struct RboCgRuntime {                     // Han2Dat+0x10AC, filled by Han2Dat_LoadCgBank (size 24020)
    int32_t file_offset;                  // +0x0000
    int32_t file_size;                    // +0x0004
    RboCgEffectRef effect[3000];          // +0x0008 per effect slot: first block and block count (0,0 for absent)
    uint32_t blocks;                      // +0x5DC8 pointer to RboCgBlock[block_count]
    int32_t texture_count;                // +0x5DCC
    uint32_t textures;                    // +0x5DD0 pointer to texture pointer array
};
static_assert(sizeof(RboCgRuntime) == 24020, "RboCgRuntime");
static_assert(offsetof(RboCgRuntime, effect) == 8 && offsetof(RboCgRuntime, blocks) == 24008 &&
              offsetof(RboCgRuntime, texture_count) == 24012 && offsetof(RboCgRuntime, textures) == 24016, "RboCgRuntime offsets");

struct RboHan2Dat {                       // CharData+0x14 (g_CharData stride 0x734C)
    uint8_t loaded;                       // +0x00 set by Han2Dat_UpdateLoadedFlag
    uint8_t pad_01[3];
    int32_t xor_flag;                     // +0x04
    uint32_t pattern_base;                // +0x08 allocation holding the pattern area
    uint32_t section[8];                  // +0x0C section 0..7 pointers (set by Han2Dat_ComputeSectionPointers); actor+0x7F0 points at pattern_base
    int32_t reserved_2C;                  // +0x2C
    RboPatRuntime pat;                    // +0x30
    RboCgRuntime cg;                      // +0x10AC
};
static_assert(offsetof(RboHan2Dat, section) == 0x0C && offsetof(RboHan2Dat, pat) == 0x30 && offsetof(RboHan2Dat, cg) == 0x10AC &&
              sizeof(RboHan2Dat) == 0x30 + 4220 + 24020, "RboHan2Dat");

// ---------------------------------------------------------------- script bank (.FOB)
// File: u32 func_count; func_count * {char name[32]; u32 entry_pc}; u32 index_type_count; per type {u32 count; i32 entry_pc[count]};
//       u32 code_size; u8 code[code_size].
struct RboScriptFuncEntry {
    char name[32];       // +0x00
    uint32_t entry_pc;   // +0x20
};
static_assert(sizeof(RboScriptFuncEntry) == 36, "RboScriptFuncEntry");

struct RboScriptIndexType {
    uint32_t count;      // +0x00 number of ids in this kind
    uint32_t entry_pc;   // +0x04 (runtime: pointer to i32[count], -1 = id undefined)
};
static_assert(sizeof(RboScriptIndexType) == 8, "RboScriptIndexType");

struct RboScriptBank {                    // g_ScriptBanks[64], stride 288
    char name[260];                       // +0x000 upper-cased path
    uint32_t code;                        // +0x104 pointer (code_size + 2 bytes, zero padded)
    uint32_t code_size;                   // +0x108 includes the 2 pad bytes
    uint32_t func_count;                  // +0x10C
    uint32_t funcs;                       // +0x110 pointer to RboScriptFuncEntry[func_count]
    uint32_t index_type_count;            // +0x114
    uint32_t index_types;                 // +0x118 pointer to RboScriptIndexType[index_type_count]
    int32_t reserved_11C;                 // +0x11C 0 after load
};
static_assert(sizeof(RboScriptBank) == 288, "RboScriptBank");
static_assert(offsetof(RboScriptBank, code) == 0x104 && offsetof(RboScriptBank, code_size) == 0x108 &&
              offsetof(RboScriptBank, func_count) == 0x10C && offsetof(RboScriptBank, funcs) == 0x110 &&
              offsetof(RboScriptBank, index_type_count) == 0x114 && offsetof(RboScriptBank, index_types) == 0x118 &&
              offsetof(RboScriptBank, reserved_11C) == 0x11C, "RboScriptBank offsets");

#pragma pack(pop)
