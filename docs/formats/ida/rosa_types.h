// Rosa Chinensis Four hand (rosa_fh.exe 2002-12-28, base 0x400000) data-file + script-VM + loader types, IDA-parsable (idc.parse_decls).
// IDB: C:\dev\frenchbread\benibara_rendan\files\Rosa Chinensis Four hand\rosa_fh.exe.i64. Everything here is applied in the IDB.
// Evidence tags: T = traced in code (function names are IDB names), D = verified on the shipped files (PAC.PAC 2002 + omake 2005), U = UNPROVEN.
// All offsets are STRUCT-RELATIVE. The engine is the Drill Milky Punch engine (docs/formats/dmp.md, dmp_types.h) one year earlier.

// ============================================================================================
// 1. Script VM opcodes (Script_RunThread 0x40ECF0). Flat u16 opcode, 0x01..0x69. Operand forms in docs/formats/rosa.md 3.3.
// ============================================================================================
enum RosaScriptOpcode : unsigned int {
 ROSAOP_DATA_DWORDS = 0x01,  // T D: u32 n; n dwords; skipped
 ROSAOP_DATA_STRING = 0x02,  // T D: identical handler; string literals
 ROSAOP_PUSH_IMM = 0x03,  // T D: u32 imm
 ROSAOP_PUSH_CODE_ADDR = 0x04,  // T D: u32 off; pushes codeImage + off
 ROSAOP_PUSH_SCRATCH_ADDR = 0x05,  // T: pushes &g_ScriptScratchBuf
 ROSAOP_PUSH_ARGS_ADDR = 0x06,  // T D: pushes &g_ScriptArgs[0]
 ROSAOP_PUSH_CONTEXT = 0x07,  // T D: pushes g_ScriptContext (value)
 ROSAOP_POP = 0x08,  // T D
 ROSAOP_STORE = 0x09,  // T D: u16 flags; u16 kind (RosaScriptStoreKind)
 ROSAOP_ADD = 0x0A,
 ROSAOP_SUB = 0x0B,
 ROSAOP_MUL = 0x0C,
 ROSAOP_DIV = 0x0D,
 ROSAOP_MOD = 0x0E,
 ROSAOP_SHL = 0x0F,
 ROSAOP_SHR = 0x10,
 ROSAOP_AND = 0x11,
 ROSAOP_OR = 0x12,
 ROSAOP_NOT = 0x13,
 ROSAOP_XOR = 0x14,
 ROSAOP_LNOT = 0x15,
 ROSAOP_LAND = 0x16,
 ROSAOP_LOR = 0x17,
 ROSAOP_CMP = 0x18,  // T D: u16 flags; u16 kind (RosaScriptCmpKind)
 ROSAOP_JCC = 0x19,  // T D: u16 flags; u16 kind; u32 target; pops the operand
 ROSAOP_JMP = 0x1A,  // T D: u32 target
 ROSAOP_CALL = 0x1B,  // T D: u32 target; pushes (retpc; 27)
 ROSAOP_CALL_NAMED = 0x1C,  // T D: pops (file;label) -> Script_CallFunc
 ROSAOP_RET_FAR = 0x1E,  // T D
 ROSAOP_RET = 0x1F,  // T D
 ROSAOP_EXIT_THREAD = 0x20,  // T D: Script_FreeThread(current); ends the slice (dMp 0x21)
 ROSAOP_SPAWN_THREAD = 0x21,  // T: pops (file;label); runs it in a new thread (dMp 0x22)
 ROSAOP_KILL_THREAD = 0x22,  // T: pops (threadId or -1; name) (dMp 0x23)
 ROSAOP_SWAP = 0x23,  // T D: swap top two (dMp 0x24)
 ROSAOP_SWITCH = 0x24,  // T D: u16 flags; u32 tablePc -> {u32 default; u32 n; {u32 key; u32 target}[n]} (dMp 0x25)
 ROSAOP_DEBUG_PRINT = 0x25,  // T: u32 n (dMp 0x26)
 ROSAOP_LOAD = 0x26,  // T D: u16 flags (dMp 0x27)
 ROSAOP_RAND_RANGE = 0x27,  // T D: (out*; max) Rng_Next % max
 ROSAOP_RAND_LCG2 = 0x28,  // T: second generator
 ROSAOP_RNG_SET_STATE = 0x29,  // T
 ROSAOP_RNG_GET_STATE = 0x2A,  // T
 ROSAOP_WAIT_FRAMES_REALTIME = 0x2B,  // T D: (frames) realtime wait
 ROSAOP_SPIN_NO_ADVANCE = 0x2C,  // T: ends the slice without advancing pc
 ROSAOP_YIELD_OR_END = 0x2D,  // T: yield; becomes END (0x53) when the yield helper fails
 ROSAOP_LINE = 0x2E,  // T D: u32 line
 ROSAOP_MEM_STORE16 = 0x2F,  // T
 ROSAOP_MEM_COPY16 = 0x30,  // T
 ROSAOP_STR_COPY = 0x31,  // T
 ROSAOP_STR_COPY_N = 0x32,  // T
 ROSAOP_STR_LENGTH = 0x33,  // T
 ROSAOP_STR_COMPARE = 0x34,  // T
 ROSAOP_STR_FIND = 0x35,  // T
 ROSAOP_MEM_COPY_DWORDS = 0x36,  // T
 ROSAOP_MEM_FILL_DWORDS = 0x37,  // T
 ROSAOP_FILE_WRITE_BUFFER = 0x38,  // T
 ROSAOP_FILE_READ_VIRTUAL = 0x39,  // T
 ROSAOP_FILE_GET_SIZE = 0x3A,  // T
 ROSAOP_FILE_OPEN = 0x3B,  // T
 ROSAOP_FILE_READ = 0x3C,  // T
 ROSAOP_FILE_WRITE = 0x3D,  // T
 ROSAOP_FILE_SEEK = 0x3E,  // T
 ROSAOP_FILE_CLOSE = 0x3F,  // T
 ROSAOP_NOP_NO_ADVANCE_40 = 0x40,  // T
 ROSAOP_NOP_NO_ADVANCE_41 = 0x41,  // T
 ROSAOP_VM_SAVE_STATE = 0x42,  // T
 ROSAOP_VM_LOAD_STATE = 0x43,  // T
 ROSAOP_FORMAT_NUMBER = 0x44,  // T
 ROSAOP_SLOT_INIT = 0x45,  // T
 ROSAOP_SLOT_ALLOC = 0x46,  // T
 ROSAOP_SLOT_FREE = 0x47,  // T
 ROSAOP_SLOT_GET_PTR = 0x48,  // T
 ROSAOP_SLOT_GET_SIZE = 0x49,  // T
 ROSAOP_RET_VALUE = 0x4A,  // T: pops a value and leaves the interpreter with it
 ROSAOP_SINE_EASE = 0x4B,  // T
 ROSAOP_FILE_EXISTS = 0x4C,  // T
 ROSAOP_FILE_QUERY_VIRTUAL = 0x4D,  // T
 ROSAOP_GET_ERRNO = 0x4E,  // T
 ROSAOP_SET_ERRNO = 0x4F,  // T
 ROSAOP_FORMAT_PRINT = 0x50,  // T: u32 n
 ROSAOP_END = 0x53,  // T D: stops the slice
 ROSAOP_SPIN_NO_ADVANCE_54 = 0x54,  // T: spin
 ROSAOP_SYSTEM_COMMAND = 0x56,  // T D: (argblock*) cmd = argblock[0]: 1 Enemy01; 2 Enemy02 master; 3 Enemy03; 4 Enemy04 master; 0xA piano master (spawn + run StartInit/MasterInit)
 ROSAOP_SPAWN_ACTOR = 0x57,  // T D: (desc*) shot/actor spawn through sub_40A290
 ROSAOP_SPAWN_EFFECT = 0x58,  // T: (type; x; y; a; b; c) effect spawn
 ROSAOP_GAME_GET_FIELD_77A99C = 0x59,  // T D: (*out) = dword 0x77A99C
 ROSAOP_MATH_ANGLE = 0x5A,  // T D: (*out; int[4]) angle in 1/10 degree
 ROSAOP_ENTITY_CALL_COLLIDE = 0x5B,  // T D: (&actor; arg): actor->commandHandler(actor; arg) (slot +0x28)
 ROSAOP_NEXT_UNIQUE_ID = 0x5C,  // T: (*out) ++g_UniqueIdCounter
 ROSAOP_BG_SET_LAYER_PARAMS = 0x5D,  // T D: BG layer params
 ROSAOP_BG_SCROLL_BY = 0x5E,  // T D
 ROSAOP_BG_SET_SCROLL_LIMITS = 0x5F,  // T D
 ROSAOP_BG_SET_VIEWPORT = 0x60,  // T D
 ROSAOP_BG_GET_SCROLL = 0x61,  // T D
 ROSAOP_BG_SET_ORIGIN = 0x62,  // T
 ROSAOP_PLAYER_SLOT_COMMAND = 0x63,  // T D: (cmd; int[3]) cmd 0 -> Player position/slot set
 ROSAOP_SOUND_LOAD_FROM_PACK = 0x64,  // T D
 ROSAOP_SOUND_SET_VOLUME = 0x65,  // T
 ROSAOP_SOUND_RELEASE_BUFFER = 0x66,  // T
 ROSAOP_SOUND_SET_FADE = 0x67,  // T
 ROSAOP_TIMED_EVENT_CREATE = 0x68,  // T
 ROSAOP_TIMED_EVENT_ADJUST = 0x69  // T
};
enum RosaScriptStoreKind : unsigned int {
 ROSASTORE_ASSIGN = 0, ROSASTORE_ADD = 0x64, ROSASTORE_SUB = 0x65, ROSASTORE_MUL = 0x66, ROSASTORE_DIV = 0x67,
 ROSASTORE_MOD = 0x68, ROSASTORE_AND = 0x69, ROSASTORE_OR = 0x6A, ROSASTORE_XOR = 0x6B
};
enum RosaScriptCmpKind : unsigned int {                 // CMP compares lhs with rhs; JCC compares the (dereferenced) operand with 0
 ROSACMP_EQ = 0, ROSACMP_NE = 1, ROSACMP_LE = 2, ROSACMP_GE = 3, ROSACMP_LT = 4, ROSACMP_GT = 5
};
enum RosaScriptOperandFlags : unsigned int {
 ROSAOPF_DEREF_RHS = 1,                  // T: top operand is a reference; use *(int*)v
 ROSAOPF_DEREF_LHS = 2                   // T: left operand is a reference (JCC/CMP: flags == 2 derefs the single operand)
};
enum RosaScriptFrameMarker : unsigned int {             // second dword pushed by CALL / CALL_NAMED
 ROSAFRAME_CALL = 27,
 ROSAFRAME_CALL_FAR = 28
};
enum RosaD3dFormat : unsigned int {                     // Img_LoadToTexture format argument (D3DFORMAT)
 ROSAFMT_R8G8B8 = 20, ROSAFMT_A8R8G8B8 = 21, ROSAFMT_X8R8G8B8 = 22, ROSAFMT_R5G6B5 = 23, ROSAFMT_X1R5G5B5 = 24, ROSAFMT_A1R5G5B5 = 25, ROSAFMT_A4R4G4B4 = 26
};
enum RosaActorKind : unsigned int {                     // RosaActor.kind (+0x98); set by the spawn natives
 ROSAKIND_ENEMY01 = 2, ROSAKIND_ENEMY02_MASTER = 3, ROSAKIND_ENEMY04_MASTER = 5, ROSAKIND_ENEMY03 = 11, ROSAKIND_SHOT = 12, ROSAKIND_EFFECT = 14
};

// ============================================================================================
// 2. .FOB script file + runtime bank / thread (Script_LoadBank 0x40BD00): identical shapes to DMP.EXE, 20 banks instead of 64
// ============================================================================================
struct RosaScriptFuncEntry {             // 36 bytes
 char name[32];                          // +0x00 T D: NUL-terminated; bytes after the NUL are stale writer junk (keep raw)
 unsigned int pc;                        // +0x20 T D: byte offset into the code image
};
struct RosaScriptBank {                  // 276 bytes, g_ScriptBanks[20] at 0x75EA70
 char name[260];                         // +0x000 T: UPPER-CASED path string given to Script_LoadBank
 unsigned char *codeImage;               // +0x104 T: malloc(codeSize + 2), last 2 bytes zero; also the scripts' mutable global memory
 int codeImageBytes;                     // +0x108 T: codeSize + 2
 int funcCount;                          // +0x10C T
 struct RosaScriptFuncEntry *funcTable;  // +0x110 T: malloc(36 * funcCount), verbatim copy of the file's entry table
};
struct RosaScriptThread {                // 2148 bytes, g_ScriptThreads[50] at 0x760218
 int allocated;                          // +0x000 T
 char name[64];                          // +0x004 T
 int stack[512];                         // +0x044 T: element 0 unused; stack[sp] is the top; (value, tag) pairs, tag 2 = reference
 int sp;                                 // +0x844 T
 unsigned char *codeImage;               // +0x848 T: copy of the bank's codeImage taken at bind time (Script_RebindThreadImage refreshes it)
 int pc;                                 // +0x84C T
 int bank;                               // +0x850 T
 int line;                               // +0x854 T
 int waitFlag;                           // +0x858 T
 int waitStartMs;                        // +0x85C T
 int waitDurMs;                          // +0x860 T
};

// ============================================================================================
// 3. PAC.PAC / the sound PAC (PackArchive_Open 0x40B1F0, key 0xFA261EFB): v1 (2002, plain) and v0 (2005, name-keyed)
// ============================================================================================
struct RosaPackEntry {                   // 64 bytes
 char name[56];                          // +0x00 T D: stored as (name[j] ^ (3*(j*i-28)) & 0xFF) for entry i
 unsigned int sizeXorKey;                // +0x38 T D: payload size ^ 0xFA261EFB
 unsigned int dataOffset;                // +0x3C T D: absolute file offset (stored plain)
};
struct RosaPackArchive {                 // 24 bytes, g_PackArchives[2] at 0x75EA10; slot 0 = the sound PAC (mapped), slot 1 = PAC.PAC
 void *hFile;                            // +0x00 T
 int entryCount;                         // +0x04 T: second u32 of the file ^ 0xFA261EFB
 struct RosaPackEntry *entries;          // +0x08 T: malloc(64 * entryCount) decoded index
 unsigned int plainFlag;                 // +0x0C T D: first u32 of the file: 1 = payload plain (2002), 0 = first 9696 bytes XOR (k + UPPER(name)[k % len]) (2005 repack PAC.PAC)
 void *hMapping;                         // +0x10 T: CreateFileMapping handle (PackArchive_MapView, used for the sound PAC)
 unsigned char *mappedView;              // +0x14 T: MapViewOfFile base; PackArchive_EntryDataOffset(slot, off) = mappedView + off
};

// ============================================================================================
// 4. .IMG (Img_OpenAndReadHeader 0x402D90)
// ============================================================================================
struct RosaImgFileV4 {                   // 44 bytes of header; all 100 files of the 2002 PAC.PAC are version 4
 unsigned int reserved0;                 // +0x00 T D: 0, read and discarded
 unsigned int version;                   // +0x04 T D: 4 (3 and 4 -> Img_ReadV3to4; 0..2 -> Img_ReadV0to2)
 unsigned char nameSeed[16];             // +0x08 T D: (UPPER(file stem) repeated to 16 bytes) XOR g_ImgKey (cyclic, 9 bytes); Img_DeriveKeyStem checks the first strlen(stem) bytes
 unsigned int flagEnc;                   // +0x18 T D: encrypted u32 (own cipher call), plaintext 0 in all files
 unsigned int paletteCountEnc;           // +0x1C T D: encrypted u32: 0 (24 bpp) or 256 (8 bpp)
 unsigned int bitsPerPixelEnc;           // +0x20 T D: encrypted u32: 8 or 24 (4/16 are supported by Img_RowBytes)
 unsigned int widthEnc;                  // +0x24 T D: encrypted u32
 unsigned int heightEnc;                 // +0x28 T D: encrypted u32
 // +0x2C: palette[paletteCount] as 4 bytes (B,G,R,0) each, ONE cipher call (index restarts at 0)       T D
 // +0x2C + 4*paletteCount: pixels[height * rowBytes], rowBytes = (bpp*width/8 rounded up + 3) & ~3, ONE cipher call, top-down rows, BGR(24) or palette index (8)   T D
};
struct RosaImgFileV1 {                   // 28 bytes of header: versions 0..2 (Img_ReadV0to2, plain); the 2005 repack stores version 1
 unsigned int reserved0;                 // +0x00 T D: 0
 unsigned int version;                   // +0x04 T D: 1
 unsigned int flag;                      // +0x08 T D: 0
 unsigned int paletteCount;              // +0x0C T D
 unsigned int bitsPerPixel;              // +0x10 T D
 unsigned int width;                     // +0x14 T D
 unsigned int height;                    // +0x18 T D
 // +0x1C: palette[paletteCount] x 4 bytes, then pixels[height * rowBytes] (same pixel layout as v4)
};
struct RosaImgBuffer {                   // 32 bytes, the decoded surface Img_LoadToTexture builds on its stack (same shape as DmpImgBuffer)
 unsigned char *pixels;                  // +0x00 T
 unsigned char *alphaPlane;              // +0x04 T: non-NULL when the caller passes splitAlpha: the lower half of the image is an 8-bit alpha plane
 unsigned char *palette;                 // +0x08 T: up to 256 x 4 bytes (Buffer[1024] in the loader)
 int rowBytes;                           // +0x0C T
 int bitsPerPixel;                       // +0x10 T
 int bottomUp;                           // +0x14 T: 0 (rows are top-down; verified visually on TITLE.IMG)
 int width;                              // +0x18 T
 int height;                             // +0x1C T: halved when splitAlpha
};
struct RosaGfxSpriteRect {               // 48 bytes; hard-coded tables in the EXE .data (e.g. 0x4340F8 for the effect sheet), indexed by RosaActor.frameIndex
 float u0;                               // +0x00 T D: srcX / texW
 float v0;                               // +0x04 T
 float u1;                               // +0x08 T
 float v1;                               // +0x0C T
 float ofsX;                             // +0x10 T: quad corner offsets (Actor_ApplyFrameRect copies [4],[5],[6],[7] to the 4 corners)
 float ofsY;                             // +0x14 T
 float ofsZ;                             // +0x18 T
 float ofsW;                             // +0x1C T
 float halfTexelU;                       // +0x20 T: 0.1 / texW
 float halfTexelV;                       // +0x24 T: 0.1 / texH
 float texWidth;                         // +0x28 T
 float texHeight;                        // +0x2C T
};

// ============================================================================================
// 5. Runtime actor (object pool record, 660 bytes, Actor_AllocFromFreeList 0x41B560 zeroes 0x294 bytes). Only fields touched by the loaders / spawn natives are typed.
// ============================================================================================
struct RosaActor {
 int untyped_00;                         // +0x00 U
 struct RosaActor *prev;                 // +0x04 T: list links (Actor_LinkListN 0x41B440..)
 struct RosaActor *next;                 // +0x08 T
 unsigned char untyped_0C[16];           // +0x0C U
 int (*updateFn)(struct RosaActor *);    // +0x1C T: e.g. Actor_UpdateEffect 0x41A9A0
 int untyped_20;                         // +0x20 U
 int (*aux24Fn)(struct RosaActor *);     // +0x24 T: Enemy02/04/piano masters store a function here (0x408790 ...); role U
 int (*commandHandler)(struct RosaActor *, int arg); // +0x28 T: called by script opcode ENTITY_CALL_COLLIDE (0x5B) with (actor, arg)
 int untyped_2C;                         // +0x2C U
 int drawLayer;                          // +0x30 T: 24, 28, 30 ...
 struct RosaGfxSpriteRect *spriteRects;  // +0x34 T: table the frame index selects from (Actor_ApplyFrameRect 0x41A1A0)
 void *aux38;                            // +0x38 U
 void *imageObject;                      // +0x3C T: texture / image object the actor draws with (e.g. dword_75E0A8)
 int frameIndex;                         // +0x40 T: index into spriteRects
 int untyped_44;                         // +0x44 T: -1 at spawn
 unsigned char untyped_48[24];           // +0x48 U
 float quadVerts[12];                    // +0x60 T: 4 vertices x (x, y, z) written by Actor_ApplyFrameRect from the sprite rect
 int alpha;                              // +0x90 T: 255 at spawn
 int untyped_94;                         // +0x94 U
 int kind;                               // +0x98 T: RosaActorKind
 int subKind;                            // +0x9C T
 int ownerArg;                           // +0xA0 T: first element of the spawn argument block
 unsigned char untyped_A4[28];           // +0xA4 U
 int posX;                               // +0xC0 T: spawn block [1]
 int posY;                               // +0xC4 T: spawn block [2]
 int untyped_C8;                         // +0xC8 U
 int state_CC;                           // +0xCC T: -1 at spawn
 unsigned char untyped_D0[168];          // +0xD0 U
 int scriptField_178;                    // +0x178 T: spawn block [3] for the shot spawn
 unsigned char untyped_17C[4];           // +0x17C U
 int state_180;                          // +0x180 T: -1 at spawn
 unsigned char untyped_184[180];         // +0x184 U
 int scriptThreadResult;                 // +0x238 T: g_ScriptArgs[0] after the actor's init script
 int scriptField_23C;                    // +0x23C T: spawn block [4] / [2]
 unsigned char scriptContext[80];        // +0x240 T: context block published with Script_SetContext while the actor's script runs (Actor_SaveScriptContext)
 unsigned char untyped_290[4];           // +0x290 T: unique id from Script_NextUniqueId
};

