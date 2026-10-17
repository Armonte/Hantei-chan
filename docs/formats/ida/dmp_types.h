// Drill Milky Punch (DMP.EXE 2003-10-20, base 0x400000) data-file + script-VM + loader types, IDA-parsable (idc.parse_decls).
// IDB: C:\dev\frenchbread\dmp_1020\DMP.EXE.i64. Everything here is applied in the IDB.
// Evidence tags in comments: T = traced in code (function names are IDB names), D = verified on the shipped files of
// C:\dev\frenchbread\dmp_1020\DATA\GAMEDATA.PAC, U = UNPROVEN (guess), X = proved never read by the game.
// All offsets are STRUCT-RELATIVE.

// ============================================================================================
// 1. Script VM opcodes (Script_RunThread 0x415D80). Flat u16 opcode, operand forms in docs/formats/dmp.md 1.3.
// ============================================================================================
enum DmpScriptOpcode : unsigned int {
 DMPOP_DATA_DWORDS = 0x01,               // T D: u32 n; n dwords; skipped
 DMPOP_DATA_STRING = 0x02,               // T D: identical handler (ScriptOp_SkipData); the compiler uses it for string literals
 DMPOP_PUSH_IMM = 0x03,                  // T D: u32 imm
 DMPOP_PUSH_CODE_ADDR = 0x04,            // T D: u32 off; pushes codeImage + off (an absolute address)
 DMPOP_PUSH_SCRATCH_ADDR = 0x05,         // T: pushes &g_ScriptScratchBuf (256 bytes)
 DMPOP_PUSH_ARGS_ADDR = 0x06,            // T D: pushes &g_ScriptArgs[0]
 DMPOP_PUSH_CONTEXT = 0x07,              // T D: pushes the VALUE of g_ScriptContext (engine object pointer set by Script_SetContext)
 DMPOP_POP = 0x08,                       // T D
 DMPOP_STORE = 0x09,                     // T D: u16 flags; u16 kind (DmpScriptStoreKind)
 DMPOP_ADD = 0x0A,                       // T D: u16 flags (DmpScriptOperandFlags)
 DMPOP_SUB = 0x0B,
 DMPOP_MUL = 0x0C,
 DMPOP_DIV = 0x0D,
 DMPOP_MOD = 0x0E,
 DMPOP_SHL = 0x0F,
 DMPOP_SHR = 0x10,
 DMPOP_AND = 0x11,
 DMPOP_OR = 0x12,
 DMPOP_NOT = 0x13,                       // T: unary bitwise not
 DMPOP_XOR = 0x14,
 DMPOP_LNOT = 0x15,                      // T: unary logical not
 DMPOP_LAND = 0x16,
 DMPOP_LOR = 0x17,
 DMPOP_CMP = 0x18,                       // T D: u16 flags; u16 kind (DmpScriptCmpKind); pushes 0/1
 DMPOP_JCC = 0x19,                       // T D: u16 flags; u16 kind (DmpScriptCmpKind; compared with 0); u32 target; pops the condition
 DMPOP_JMP = 0x1A,                       // T D: u32 target
 DMPOP_CALL = 0x1B,                      // T D: u32 target; pushes (return pc; marker 27)
 DMPOP_CALL_NAMED = 0x1C,                // T: pops (file;label) string pairs -> Script_CallFunc (marker 28 frame; restores bank on RET_FAR)
 DMPOP_RET_FAR = 0x1E,                   // T D: pops frame; marker 28 -> Script_UnloadBanksFrom(current bank) + restore previous bank
 DMPOP_RET = 0x1F,                       // T: pops frame
 DMPOP_YIELD = 0x20,                     // T D: ends the thread slice; pc advances
 DMPOP_EXIT_THREAD = 0x21,               // T D: Script_FreeThread(current)
 DMPOP_SPAWN_THREAD = 0x22,              // T: pops (file;label); runs the label once in a new thread; thread id -> g_ScriptArgs[0]
 DMPOP_KILL_THREAD = 0x23,               // T: pops (threadId or -1; name)
 DMPOP_SWAP = 0x24,                      // T D
 DMPOP_SWITCH = 0x25,                    // T D: u16 flags; u32 tablePc -> {u32 default; u32 n; {u32 key; u32 target}[n]}
 DMPOP_DEBUG_PRINT = 0x26,               // T D: u32 n
 DMPOP_LOAD = 0x27,                      // T D: u16 flags (bit1 derefs the top)
 DMPOP_RAND_RANGE = 0x28,                // T D: (out*; max): *out = Rng_Next() % max
 DMPOP_RAND_LCG2 = 0x29,                 // T: same with the second generator Rng2_Lcg956CEA5_Next
 DMPOP_RNG_SET_STATE = 0x2A,             // T
 DMPOP_RNG_GET_STATE = 0x2B,             // T
 DMPOP_WAIT_FRAMES_REALTIME = 0x2C,      // T D: (frames): waitFlag; waitStartMs = Timer_NowMs; waitDurMs = Timer_FramesToMs(frames)
 DMPOP_SPIN_NO_ADVANCE = 0x2D,           // T: ends the slice without advancing pc (re-executes next frame)
 DMPOP_YIELD_OR_END = 0x2E,              // T: yield; rewrites the opcode to END when ScriptOp_YieldFrame returns 0
 DMPOP_LINE = 0x2F,                      // T D: u32 line (stored in the thread's line var; debug overlay when g_ScriptDebugOverlay)
 DMPOP_MEM_STORE16 = 0x30,               // T: (*dst16; val)
 DMPOP_MEM_COPY16 = 0x31,                // T: (*dst16; *src16)
 DMPOP_STR_COPY = 0x32,                  // T: strcpy
 DMPOP_STR_COPY_N = 0x33,                // T: strncpy
 DMPOP_STR_LENGTH = 0x34,                // T: (*out; str)
 DMPOP_STR_COMPARE = 0x35,               // T: (*out; s1; s2; n) strncmp
 DMPOP_STR_FIND = 0x36,                  // T: (*out; hay; needle) strstr offset or -1
 DMPOP_MEM_COPY_DWORDS = 0x37,           // T
 DMPOP_MEM_FILL_DWORDS = 0x38,           // T
 DMPOP_FILE_WRITE_BUFFER = 0x39,         // U by analogy with RBO 07.00: create file + write
 DMPOP_FILE_READ_VIRTUAL = 0x3A,         // T: read from loose file or pack (File_OpenLooseOrPack)
 DMPOP_FILE_GET_SIZE = 0x3B,             // T
 DMPOP_FILE_OPEN = 0x3C,                 // T
 DMPOP_FILE_READ = 0x3D,                 // T
 DMPOP_FILE_WRITE = 0x3E,                // T
 DMPOP_FILE_SEEK = 0x3F,                 // T
 DMPOP_FILE_CLOSE = 0x40,                // T
 DMPOP_NOP_NO_ADVANCE_41 = 0x41,         // T: handler returns without advancing pc (would spin)
 DMPOP_NOP_NO_ADVANCE_42 = 0x42,         // T: same
 DMPOP_VM_SAVE_STATE = 0x43,             // U: writes threads/banks to a file (File_Create + writes)
 DMPOP_VM_LOAD_STATE = 0x44,             // U: reads them back; Script_RebindThreadImage per thread
 DMPOP_FORMAT_NUMBER = 0x45,             // T: (flags; value; digits; dest) String_FormatNumber
 DMPOP_SLOT_INIT = 0x46,                 // T: Script_SlotsInit(count)
 DMPOP_SLOT_ALLOC = 0x47,                // T
 DMPOP_SLOT_FREE = 0x48,                 // T
 DMPOP_SLOT_GET_PTR = 0x49,              // T
 DMPOP_SLOT_GET_SIZE = 0x4A,             // T
 DMPOP_RET_VALUE = 0x4B,                 // T: pops a value and leaves the interpreter with it
 DMPOP_SINE_EASE = 0x4C,                 // T: (*out; {start;end;t;duration})
 DMPOP_FILE_EXISTS = 0x4D,               // T
 DMPOP_FILE_QUERY_VIRTUAL = 0x4E,        // T
 DMPOP_GET_ERRNO = 0x4F,                 // T
 DMPOP_SET_ERRNO = 0x50,                 // T
 DMPOP_FORMAT_PRINT = 0x51,              // T D: u32 n
 DMPOP_END = 0x54,                       // T D: stops the thread slice (Script_RunThread returns 1/0 by dword_44FD0C)
 DMPOP_SPIN_NO_ADVANCE_55 = 0x55,        // T: like 0x2D; also the opcode Script_CallFunc stores when a label is missing ("LJumpSubError")
 DMPOP_STAGE_ENEMY_LIST = 0x57,          // T D: (cmd; enemyId): -1 clears g_StageEnemyIds; 0 appends
 DMPOP_ENEMY_SPAWN = 0x58,               // T D: (desc*): looks desc[0] up in g_EnemyTypeTable and calls entry.spawn
 DMPOP_STUB_POP_REF = 0x59,              // T: pops one reference; no effect
 DMPOP_GAME_GET_FIELD_BF7FDC = 0x5B,     // T D: (*out) = value of dword 0xBF7FDC (setter sub_41CCA0)
 DMPOP_MATH_ANGLE = 0x5C,                // T D: (*out; {x1;y1;x2;y2}) angle in 1/10 degree 0..3599
 DMPOP_ENTITY_MOTION = 0x61,             // T D: (params*; ...) sub_427760 switch on params[1] writing an entity sub-block
 DMPOP_ENTITY_CALL_COLLIDE = 0x62,       // T D: (&entity; arg): entity->collide(entity; arg)  (DmpEntity +0x24)
 DMPOP_NEXT_UNIQUE_ID = 0x63,            // T: (*out) = ++g_UniqueIdCounter
 DMPOP_BG_SET_LAYER_PARAMS = 0x67,       // T
 DMPOP_BG_SCROLL_BY = 0x68,              // T
 DMPOP_BG_SET_SCROLL_LIMITS = 0x69,      // T
 DMPOP_BG_SET_VIEWPORT = 0x6A,           // T
 DMPOP_BG_GET_SCROLL = 0x6B,             // T
 DMPOP_BG_SET_ORIGIN = 0x6C,             // T
 DMPOP_BG_SET_CAMERA_MODE = 0x6E,        // T
 DMPOP_BG_COPY_STATE = 0x6F,             // T
 DMPOP_BG_SET_LAYER_IMAGE_NAME = 0x70,   // T
 DMPOP_GFX_ALLOC_IMAGE_TABLE = 0x73,     // T D: (count) malloc(276*count) DmpGfxImageSlot; frees the previous table
 DMPOP_GFX_ALLOC_SPRITE_TABLE = 0x74,    // T D: (count) 48*count DmpGfxSpriteRect + 1520*count sprite objects
 DMPOP_GFX_SET_IMAGE = 0x75,             // T D: (idx; nameRef; type 0=A1R5G5B5/1=A4R4G4B4; width; height)
 DMPOP_GFX_DEFINE_SPRITE = 0x76,         // T D: 10 args (spriteIdx; imageIdx; ofsX; ofsY; a5; a6; srcX; srcY; srcW; srcH)
 DMPOP_GFX_DRAW_SPRITE = 0x77,           // T D: 8 args (spriteIdx; layer; x; y; r; g; b; alpha)
 DMPOP_PLAYER_SLOT_COMMAND = 0x7E,       // T
 DMPOP_GAME_QUERY = 0x7F,                // T D: (mode; &inout)
 DMPOP_SOUND_LOAD_FROM_PACK = 0x80,      // T
 DMPOP_SOUND_SET_VOLUME = 0x81,          // T
 DMPOP_SOUND_RELEASE_BUFFER = 0x82,      // T
 DMPOP_SOUND_SET_FADE = 0x83,            // T
 DMPOP_AUDIO_ALLOC_CHANNELS = 0x84,      // T: (count) 32-byte channel records
 DMPOP_AUDIO_DEFINE_CHANNEL = 0x85,      // T D: (idx; a; b)
 DMPOP_AUDIO_SET_LEVELS = 0x86,          // T D: (idx; a; b; c; d)
 DMPOP_AUDIO_SET_STATE = 0x87,           // T D: (idx; state)
 DMPOP_AUDIO_GET_STATE = 0x88,           // T
 DMPOP_SOUND_LOAD_SLOT = 0x89,           // T
 DMPOP_SOUND_SET_PLAY_POINTER = 0x8A,    // T
 DMPOP_SOUND_INIT_PLAYBACK = 0x8B,       // T D
 DMPOP_SOUND_CHAIN_SLOTS = 0x8C,         // T
 DMPOP_SOUND_PREPARE_CHANNEL = 0x8D,     // T D
 DMPOP_SOUND_PLAY_LOOPING = 0x8E,        // T D
 DMPOP_SOUND_STOP_STREAM = 0x8F,         // T D
 DMPOP_UNIMPL_90 = 0x90,                 // D: occurs in STAGE01/02 after PUSH_IMM; NO case in Script_RunThread -> "ScriptUnknown Code!!"
 DMPOP_UNIMPL_91 = 0x91,                 // D: same
 DMPOP_SOUND_LOAD_FILE = 0x92,           // T
 DMPOP_SOUND_STOP_SLOT = 0x93,           // T
 DMPOP_SOUND_IS_BANK_PLAYING = 0x94,     // T
 DMPOP_AUDIO_PLAY = 0x95,                // T
 DMPOP_SOUND_PLAY_BANK_ONESHOT = 0x96,   // T
 DMPOP_SOUND_STORE_VOICE = 0x97,         // T
 DMPOP_SOUND_CMD_ONE = 0x98,             // T
 DMPOP_SOUND_RELEASE_SLOTS = 0x99,       // T
 DMPOP_SOUND_CHANNEL_SET_UNIQUE = 0x9D,  // T D: (idx; value)
 DMPOP_TIMED_EVENT_CREATE = 0x9F,        // T
 DMPOP_TIMED_EVENT_ADJUST = 0xA0,        // T
 DMPOP_END_OF_CODE = 0xA1                // D: sentinel after the last YIELD of ZAKOTEXTURELOAD/INITCONFIG; no case in Script_RunThread
};
enum DmpScriptStoreKind : unsigned int {
 DMPSTORE_ASSIGN = 0, DMPSTORE_ADD = 0x64, DMPSTORE_SUB = 0x65, DMPSTORE_MUL = 0x66, DMPSTORE_DIV = 0x67,
 DMPSTORE_MOD = 0x68, DMPSTORE_AND = 0x69, DMPSTORE_OR = 0x6A, DMPSTORE_XOR = 0x6B
};
enum DmpScriptCmpKind : unsigned int {                  // CMP compares lhs with rhs; JCC compares the (dereferenced) top with 0
 DMPCMP_EQ = 0, DMPCMP_NE = 1, DMPCMP_LE = 2, DMPCMP_GE = 3, DMPCMP_LT = 4, DMPCMP_GT = 5
};
enum DmpScriptOperandFlags : unsigned int {
 DMPOPF_DEREF_RHS = 1,                   // T: top operand is a reference; use *(int*)v
 DMPOPF_DEREF_LHS = 2                    // T: left operand is a reference (JCC/CMP: flags == 2 derefs the single operand)
};
enum DmpScriptFrameMarker : unsigned int {              // second dword pushed by CALL/CALL_NAMED
 DMPFRAME_CALL = 27,
 DMPFRAME_CALL_FAR = 28
};

// ============================================================================================
// 2. .FOB script file + runtime bank / thread (Script_LoadBank 0x412D40)
// ============================================================================================
struct DmpScriptFuncEntry {              // 36 bytes
 char name[32];                          // +0x00 T D: NUL-terminated; bytes after the NUL are stale writer junk (keep raw for byte-exact output)
 unsigned int pc;                        // +0x20 T D: byte offset into the code image
};
struct DmpScriptBank {                   // 276 bytes, g_ScriptBanks[64] at 0x94C1F8
 char name[260];                         // +0x000 T: upper-cased path string given to Script_LoadBank (full ".\data\script\..." path, not the basename)
 unsigned char *codeImage;               // +0x104 T: malloc(codeSize + 2), last 2 bytes zero; also the scripts' mutable global memory
 int codeImageBytes;                     // +0x108 T: codeSize + 2
 int funcCount;                          // +0x10C T: nFuncs
 struct DmpScriptFuncEntry *funcTable;   // +0x110 T: malloc(36 * nFuncs), copied verbatim from the file
};
struct DmpScriptThread {                 // 2148 bytes, g_ScriptThreads[50] at 0x950910
 int allocated;                          // +0x000 T
 char name[64];                          // +0x004 T: only copied up to the NUL (rest keeps the previous occupant)
 int stack[512];                         // +0x044 T: element 0 unused; stack[sp] is the top; values and tags are separate dwords
 int sp;                                 // +0x844 T
 unsigned char *codeImage;               // +0x848 T: copy of g_ScriptBanks[bank].codeImage taken at bind time (Script_RebindThreadImage refreshes it)
 int pc;                                 // +0x84C T: byte offset into codeImage
 int bank;                               // +0x850 T
 int line;                               // +0x854 T
 int waitFlag;                           // +0x858 T
 int waitStartMs;                        // +0x85C T
 int waitDurMs;                          // +0x860 T
};

// ============================================================================================
// 3. GAMEDATA.PAC / SOUNDDATA.PAC (PackArchive_Open 0x4121A0, key 0xFA261EFB)
// ============================================================================================
struct DmpPackEntry {                    // 64 bytes
 char name[56];                          // +0x00 T: stored as (name[j] ^ (3*(j*i-28)) & 0xFF) for entry i
 unsigned int sizeXorKey;                // +0x38 T D: payload size ^ 0xFA261EFB
 unsigned int dataOffset;                // +0x3C T D: absolute file offset (stored plain)
};
struct DmpPackArchive {                  // 24 bytes, g_PackArchives[3] at 0x94C180; slot 0 = SoundData.pac, 1 = GameData.pac, 2 = unused
 void *hFile;                            // +0x00 T
 int entryCount;                         // +0x04 T: second u32 of the file ^ 0xFA261EFB
 struct DmpPackEntry *entries;           // +0x08 T: malloc(64 * entryCount) decoded index
 unsigned int plainFlag;                 // +0x0C T D: first u32 of the file; nonzero = payload stored plain, 0 = first 9696 bytes XOR-enciphered with the entry name (PackArchive_ReadEntry)
 unsigned int unused_10;                 // +0x10 T: CreateFileMapping handle (rosa_fh.exe PackArchive_MapView 0x40B5E0 writes it); not used by DMP.EXE
 unsigned int dataBase;                  // +0x14 T: MapViewOfFile base (mappedView in Rosa); added by PackArchive_EntryDataOffset
};

// ============================================================================================
// 4. .IMG (Img_OpenAndReadHeader 0x407CC0) and the script-driven image/sprite tables
// ============================================================================================
struct DmpImgFileHeader {                // 20 bytes + width*height*2 raw 16-bit pixels (all 68 shipped files)
 unsigned int reserved0;                 // +0x00 T: read and discarded; 0 in every file
 unsigned int version;                   // +0x04 T D: switch in Img_OpenAndReadHeader: 0..2 -> Img_ReadV0to2, 3..4 -> Img_ReadV3to4, 6 -> Img_ReadV6 (all 68 shipped files are 6)
 unsigned int paletteFlag;               // +0x08 T D: 0 in all files; if 0 the reader sets bitdepth 16
 unsigned int width;                     // +0x0C T D
 unsigned int height;                    // +0x10 T D
 // +0x14: unsigned short pixels[width*height]  T D: copied RAW into the locked D3D texture (Tex_CopyRaw16Pixels); the texture format
 //        (A1R5G5B5 = D3DFMT 25 or A4R4G4B4 = 26) is chosen by the CALLER, not by the file.
};
struct DmpImgBuffer {                    // 32 bytes filled by Img_ReadFileToBuffer 0x4092C0
 unsigned short *pixels;                 // +0x00 T
 int hasAlphaPlane;                      // +0x04 T
 void *palette;                          // +0x08 T
 int rowBytes;                           // +0x0C T: -1 for raw 16-bit data (selects Tex_CopyRaw16Pixels)
 int bitDepth;                           // +0x10 T: 16
 int bottomUp;                           // +0x14 T
 int width;                              // +0x18 T
 int height;                             // +0x1C T
};
struct DmpGfxImageSlot {                 // 276 bytes, table pointer g_GfxImageTable (script op GFX_ALLOC_IMAGE_TABLE)
 char path[260];                         // +0x000 T: IMG path set by GFX_SET_IMAGE (e.g. ".\\Data\\BG\\bg00.img")
 int d3dFormat;                          // +0x104 T: 25 (type 0) or 26 (type 1)
 void *texture;                          // +0x108 T: IDirect3DTexture8*, released by Tex_ReleaseSlot
 int width;                              // +0x10C T: GFX_SET_IMAGE argument 4 (used as the UV divisor)
 int height;                             // +0x110 T: GFX_SET_IMAGE argument 5
};
struct DmpGfxSpriteRect {                // 48 bytes, table g_GfxSpriteRects (written by Gfx_DefineSprite and the Player/Event InitPatternData copy loops)
 float u0;                               // +0x00 T: srcX / texW
 float v0;                               // +0x04 T: srcY / texH
 float u1;                               // +0x08 T: (srcX + srcW) / texW
 float v1;                               // +0x0C T: (srcY + srcH) / texH
 float ofsX;                             // +0x10 T
 float ofsY;                             // +0x14 T
 float ofsZ;                             // +0x18 T: GFX_DEFINE_SPRITE argument 5
 float ofsW;                             // +0x1C T: argument 6
 float halfTexelU;                       // +0x20 T: 0.1 / texW
 float halfTexelV;                       // +0x24 T: 0.1 / texH
 float texWidth;                         // +0x28 T
 float texHeight;                        // +0x2C T
};

// ============================================================================================
// 5. Stage / enemy / chara script drivers
// ============================================================================================
struct DmpEnemyType {                    // 24 bytes, g_EnemyTypeTable at 0x450AC0, terminated by id -1; ids 0..39, 900, 910, 980
 int id;                                 // +0x00 T: matches the ids pushed by STAGE_ENEMY_LIST
 int (*loadScripts)(int phase);          // +0x04 T: GameMain_Init: Event_LoadResources (ENEMYnnn.FOB, InitFunction + InitPatternData x48)
 int (*loadTextures)(void);              // +0x08 T: GameMain_Load: Event_LoadTextures (ZakoTextureLoad.fob LoadTextureCallBack(i) -> g_TexEnemy[i], format 25)
 int (*releaseAssets)(int phase);        // +0x0C T: GameMain_Release
 int (*spawn)(void *desc);               // +0x10 T: script op ENEMY_SPAWN
 int (*onLevelCheck)(void);              // +0x14 U: Event_CheckLevel for ids 0..39
};
struct DmpMatchSetup {                   // 68 bytes at g_MatchSetup 0xBF7F78; first 68 bytes of every DEMOnn.DAT / Replay file
 int reserved0;                          // +0x00 T D: zeroed before Replay_WriteFileAtMatchEnd; 0 in all demos
 int seed;                               // +0x04 T D: Setup_GetSeed -> Rng_Seed in GameMain_Init (0 in all demos)
 int lives;                              // +0x08 T D: Setup field 0xBF7F80 (Player_SetLives value)
 int scoreAttackTimeSel;                 // +0x0C T D: Setup_GetScoreAttackTimeSel (1/2/3 -> 3600/10800/18000 frames)
 int configC;                            // +0x10 T D: Setup field 0xBF7F88 (Setup_SnapshotConfig copies 0x96B190..19C into 0xBF7F80..8C)
 int configD;                            // +0x14 T D: Setup field 0xBF7F8C
 int stageScriptSet;                     // +0x18 T D: Setup_SetStageScriptSet -> STAGEnn.FOB number
 int entryKind;                          // +0x1C T D: Setup_SetEntryKind
 int gameMode;                           // +0x20 T D
 int stage;                              // +0x24 T D: passed as arg0 of the stage script "Init"
 int playerCount;                        // +0x28 T D
 int slotChara[6];                       // +0x2C T D: Setup_GetSlotChara(i), -1 = empty seat; CHARAnn = value + 1
};
struct DmpReplayFrame {                  // 24 bytes
 unsigned int rawInput[6];               // +0x00 T D: g_FrameInput[seat] = Input_GetRawWord(seat) for seats 0..5
};
struct DmpReplayFile {                   // DEMOnn.DAT / Replay*.DAT = 2,592,104 bytes = 68 + 12 + 108001 * 24; read by Replay_LoadFile in two reads (68 then 2,592,036)
 struct DmpMatchSetup setup;             // +0x00 T D: g_MatchSetup 0xBF7F78
 unsigned int header;                    // +0x44 T D: g_ReplayHeader 0x96B3A8, 0 in every file
 unsigned int frameCount;                // +0x48 T D: g_ReplayFrameCount (= recorded frames; capacity 108000; demos 7615..17008)
 unsigned int frameCursor;               // +0x4C T D: g_ReplayFrameCursor at save time (== frameCount); Replay_LoadFile resets it to 0
 struct DmpReplayFrame frames[108001];   // +0x50 T D: g_ReplayInputFrames 0x96B3B4; frames beyond frameCount are zero (the extra 24 bytes are the 108001st row)
};

// ============================================================================================
// 6. Script-built pattern tables (InitPatternData(i) of CHARAnn / ENEMYnnn, filled by Player_LoadAll / Event_LoadResources)
// ============================================================================================
struct DmpPatternData {                  // 36 bytes; 74 per player (g_PlayerPatterns[6][74] at 0x1D66830), 48 per enemy (g_EnemyPatterns[40][48] at 0x93AC20)
 int scriptArg9;                         // +0x00 T: g_ScriptArgs[9] after InitPatternData(i)
 int scriptArg8;                         // +0x04 T: g_ScriptArgs[8]
 int scriptArg10;                        // +0x08 T: g_ScriptArgs[10]
 int scriptArg11;                        // +0x0C T: g_ScriptArgs[11]
 struct DmpGfxSpriteRect *spriteRects;   // +0x10 T: malloc(48 * count), count = g_ScriptArgs[0]; built from g_ScriptArgs[1] -> script rows of 10 ints {texW,texH,u0px,v0px,u1px,v1px,ofsX,ofsY,ofsZ,ofsW}
 int *intTable2;                         // +0x14 T: malloc(4 * count), copy of the script int array g_ScriptArgs[2]
 int *intTable3;                         // +0x18 T: malloc(4 * count), copy of g_ScriptArgs[3]
 int *records6;                          // +0x1C T: malloc(24 * g_ScriptArgs[4]) of 6-int records copied from g_ScriptArgs[5] (only when g_ScriptArgs[4] > 0)
 int *records5;                          // +0x20 T: malloc(24 * g_ScriptArgs[6]) holding 5-int (20-byte stride) records copied from g_ScriptArgs[7]
};
struct DmpPlayerCharaInfo {              // 548 bytes per seat at 0x1D6AA30 (Player_LoadAll: memset 0x224 before InitCharaInfo)
 int buttonInfo[3][42];                  // +0x000 T: InitCharaButtonInfo(0/1/2) copied by sub_42A2E0 (mode in dword 2)
 int zeroInit_1F8[2];                    // +0x1F8 T: written 0 by Player_LoadAll after the three InitCharaButtonInfo calls
 int charaInfo[4];                       // +0x200 T: g_ScriptArgs[0..3] after InitCharaInfo
 int unwritten_210[4];                   // +0x210 U: not written by Player_LoadAll
 int zeroInit_220;                       // +0x220 T: written 0
};

