// Yamayuri Rendan / Lilian Fourhand (French-Bread, Dec 2005, LilianFourhand.exe, MSVC, 32-bit) loader and runtime-state types, IDA-parsable (idc.parse_decls, one parse of this whole file).
// Applied in the IDB C:\dev\ida\server\LilianFourhand.exe.i64 exactly as written here (all structs are packed: #pragma pack(1) is part of the text).
// Evidence tags in the field comments: T = access traced in the exe (function names are IDB names after this work), I = traced but meaning inferred from code shape, E = data-bearing in the shipped files but no reader in the exe (editor-only), U = no access found.
// Fields named unknown_<hexoff> are runtime bytes never touched by a loader and not understood; fields named unused_<hexoff> are never read or written. Documentation: docs/formats/lilian.md
#pragma pack(push,1)
enum LilPakCipherMode : unsigned int { LILPAK_CIPHER_NAME_KEYED=0, LILPAK_CIPHER_PLAIN=1, LILPAK_CIPHER_XOR_CA=2 };
enum LilAniFlag : unsigned int { LILANI_NEXT=0, LILANI_STOP_FINISHED=1, LILANI_JUMP_TO_FRAME=2, LILANI_HOLD_FINISHED_MINUS1=3 };
enum LilAttrFlag : unsigned int { LILATTR_FLIP_H=1, LILATTR_FLIP_V=2 };
enum LilChipMode : unsigned int { LILCHIP_INDEXED8=0, LILCHIP_RGB24=1 };
enum LilChipAttr : unsigned char { LILCHIPATTR_AIR=0, LILCHIPATTR_FLOOR_A=1, LILCHIPATTR_SOLID=2, LILCHIPATTR_WALL=3, LILCHIPATTR_FLOOR_B=4 };
enum LilEnemyPosition : unsigned int { LILPOS_FRONT=0, LILPOS_BACK=1 };
enum LilFrameEffectId : unsigned int { LILFX_NONE=0, LILFX_CALL_VIRTUAL_5=1, LILFX_SOUND_OR_SHAKE=5, LILFX_CALL_VIRTUAL_2=10, LILFX_CALL_VIRTUAL_3=15, LILFX_CALL_VIRTUAL_4=100 };
enum LilReplayMode : unsigned int { LILREP_OFF=0, LILREP_PLAYBACK=1, LILREP_RECORD=2 };
enum LilBgCmdId : unsigned int { LILBGCMD_WAIT=0, LILBGCMD_SET_SCROLL_X=1, LILBGCMD_SET_SCROLL_Y=2, LILBGCMD_SET_SCROLL_ADD_X=3, LILBGCMD_SET_SCROLL_ADD_Y=4, LILBGCMD_FLIP=5, LILBGCMD_SET_SCROLL_RATIO=6, LILBGCMD_SET_SCROLL_LOOP=7, LILBGCMD_SET_LOOP_END_COUNT=8, LILBGCMD_SET_MOVE_MODE=9, LILBGCMD_SET_SCROLL_FREE=10, LILBGCMD_SET_BG_POS=11, LILBGCMD_SET_BG_PRIO=12, LILBGCMD_INIT_BG_COLOR_DEPTH=13, LILBGCMD_SET_BG_COLOR_DEPTH=14, LILBGCMD_STOP_BG_COUNT=15, LILBGCMD_HANTEI_VIEW=17, LILBGCMD_SPRITE_HANTEI_VIEW=18, LILBGCMD_MUTEKI_MODE=19, LILBGCMD_SET_CHARA_POS=20, LILBGCMD_SET_CHARA_POS_GROUND=21, LILBGCMD_SET_CHARA_CONTROL=22, LILBGCMD_SET_CHARA_CLIP=23, LILBGCMD_SET_CHARA_KEY=24, LILBGCMD_SET_PAUSE_ABLE=25, LILBGCMD_SET_NEXT_ITEM=26, LILBGCMD_SET_ENEMY=27, LILBGCMD_SET_ENEMY_AREA_BOSS=28, LILBGCMD_SET_ENEMY_PARAM=29, LILBGCMD_RESET_ENEMY_PARAM=30, LILBGCMD_LOAD_BGM=31, LILBGCMD_PLAY_BGM=32, LILBGCMD_STOP_BGM=33, LILBGCMD_FADE_BGM=34, LILBGCMD_PLAY_EFFECT=35, LILBGCMD_STAGE_CLEAR=36, LILBGCMD_COMMENT=0xFFFFFFFE, LILBGCMD_END=0xFFFFFFFF };
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// .p archive (00dt.p 00bg.p 00bgt.p 00dm.p 00b.p 00e.p): header u32 cipherMode, u32 entryCount ^ 0xE3DF59AC, then entryCount x LilPakEntry, then the payloads
struct LilPakEntry {                   // 68 bytes (LilPak_Open 0x437BA0 deciphers the name in place)
 char name[60];                        // +0x00 T: CP932 NUL padded; bytes 0..58 stored as name[j] ^ ((3*j*i + 61) & 0xFF), i = entry index; byte 59 stays 0
 unsigned int offset;                  // +0x3C T: absolute file offset of the payload (plain)
 unsigned int size;                    // +0x40 T: stored as size ^ 0xE3DF59AC
};
struct LilPakArchive {                 // 284 bytes: the 10-slot table g_PakArchives 0x59B378 (data slots 0..3 = 00dt 00bg 00bgt 00dm), and the 5+5 audio slots opened by LilAudio_OpenPaks
 void *hFile;                          // +0x00 T: File handle (0 = slot empty)
 void *hMapping;                       // +0x04 T: CreateFileMappingA (only when the open flag a3 != 0; never in the shipped calls)
 void *mappedView;                     // +0x08 T: MapViewOfFile
 enum LilPakCipherMode cipherMode;     // +0x0C T: first dword of the file; 1 = plain (00b.p), 0 = name-keyed (everything else), 2 = XOR 0xCA
 int entryCount;                       // +0x10 T: second dword ^ 0xE3DF59AC
 unsigned int unused_14;               // +0x14 U
 char path[256];                       // +0x18 T: the path the archive was opened with
 struct LilPakEntry *entries;          // +0x118 T: malloc(68 * entryCount) holding the deciphered index
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// INI-style text reader used for EVERY text file (AniEdit .TXT, [DATA] files, enemy/effect/bullet files); the proc scripts and ENDSTAFF use the raw line cursor instead
struct LilIniSection {                 // 36 bytes, heap, one per "[name]" line found by LilIni_IndexSections 0x4389E0
 char name[32];                        // +0x00 T: text between '[' and the first ']' (1..31 chars, else the section is not indexed)
 char *textPos;                        // +0x20 T: pointer to the '[' of that line inside the text buffer
};
struct LilPtrVector {                  // 20 bytes (sub_401C50 init, LilVector_Push 0x401CA0): growable array, initial capacity 16 then doubling
 unsigned int storesPointers;          // +0x00 T: 1 = elements are pointers (elemSize forced to 4); 0 = elements are copied by value
 void *data;                           // +0x04 T: element array
 unsigned int elemSize;                // +0x08 T
 unsigned int count;                   // +0x0C T
 unsigned int capacity;                // +0x10 T
};
struct LilIniFile {                    // 0x134 bytes
 unsigned int failed;                  // +0x00 T: 0 normally; every Get* returns the default when non-zero
 char *text;                           // +0x04 T: the whole file (NUL terminated by the caller), not owned when ownsText == 1
 unsigned int ownsText;                // +0x08 T: LilIni_Attach sets 1 (text is borrowed from the caller)
 unsigned char unused_0C[0x100];       // +0x0C U
 struct LilPtrVector *sections;        // +0x10C T: vector of LilIniSection*
 char cacheSectionName[32];            // +0x110 T: one-entry lookup cache (section name)
 char *cacheSectionPos;                // +0x130 T: one-entry lookup cache (section start)
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// AniEdit animation table (.TXT starting "[Header]/Name=AniEdit Data"); loaded by LilAniTable_LoadFromIni 0x4010E0, owned by a LilAniAsset
struct LilAniTexture {                 // 68 bytes, [TextureNN] (NN 0..199, loop stops at the first missing Size)
 char name[64];                        // +0x00 T: Name= (a .bmp name; the data file is the .EX3 of the same stem, plus <stem>_m.EX3 as alpha mask)
 int size;                             // +0x40 T: Size= (texture edge in pixels); a missing/zero Size ends the texture list
};
struct LilAniPattern {                 // 76 bytes, [PatternNNN] (NNN 0..254; every NNN gets a record, FrameNum default 0 = absent)
 int patternIndex;                     // +0x00 T: running index (= NNN)
 char name[64];                        // +0x04 T: Name= (CP932, editor label, never displayed)
 int frameNum;                         // +0x44 T: FrameNum=
 int firstFrame;                       // +0x48 T: index of the pattern's first LilAniFrame in the global frame array
};
struct LilAniFrame {                   // 92 bytes, [PatternNNN-MMM]; keys read by LilAniFrame_ReadFromIni 0x401830
 int frameIndex;                       // +0x00 T: running global index
 unsigned int unused_04;               // +0x04 U
 int u;                                // +0x08 T: U=
 int v;                                // +0x0C T: V=
 int w;                                // +0x10 T: W=
 int h;                                // +0x14 T: H=  (INI key text is "H")
 int shiftX;                           // +0x18 T: ShiftX= (sprite origin offset)
 int shiftY;                           // +0x1C T: ShiftY=
 unsigned char unused_20[16];          // +0x20 U
 int tpage;                            // +0x30 T: tpage= (index into the table's [TextureNN] list)
 float angle;                          // +0x34 T: Angle= (read with atof, printed %f)
 float zoomX;                          // +0x38 T: ZoomX= (atof; shipped files print %7.3f)
 float zoomY;                          // +0x3C T: ZoomY=
 enum LilAttrFlag attrFlag;            // +0x40 T: AttrFlag= bit0 = flip horizontally, bit1 = flip vertically (LilAniAsset_LoadFrameUV 0x43EA20)
 int alphaFlag;                        // +0x44 T: AlphaFlag= 0 opaque, 1 and 2 = blend modes (draw switch in LilEntity_Draw 0x43D9B0)
 int alphaDepth;                       // +0x48 T: AlphaDepth= 0..255
 enum LilAniFlag aniFlag;              // +0x4C T: AniFlag= end-of-frame action (LilEntity_StepAnimation 0x43D600)
 int delay;                            // +0x50 T: Delay= ticks the frame is shown
 int jump;                             // +0x54 T: Jump= frame index used by LILANI_JUMP_TO_FRAME
 int etcFlag0;                         // +0x58 T: EtcFlag0= 0/1 (read and stored; the draw code branches on it)
};
struct LilAniHanteiSet {               // 64 bytes per frame: Hantei00..Hantei15 (absent key = 0 = no box)
 int rectIndex[16];                    // +0x00 T: index into the table's LilAniRect array (0 = empty); slot roles in docs/formats/lilian.md
};
struct LilAniEffectSet {               // 40 bytes per frame: Effect00..Effect09 (absent key = 0)
 int effectParamIndex[10];             // +0x00 T: index into the table's LilAniEffectParam array (0 = none)
};
struct LilAniRect {                    // 16 bytes, [HanteiRect] Hantei_NNNN=x1,y1,x2,y2 (NNNN contiguous from 0; the loop stops at the first missing key)
 int x1;                               // +0x00 T
 int y1;                               // +0x04 T
 int x2;                               // +0x08 T
 int y2;                               // +0x0C T
};
struct LilAniEffectParam {             // 64 bytes, [EffectParam] Param_NNNN=16 integers separated by one space (NNNN contiguous from 0)
 int v[16];                            // +0x00 T: v[0] = LilFrameEffectId, v[1..15] = arguments of that effect
};
struct LilAniTable {                   // 68 bytes (LilAniTable_Init 0x401000 / LilAniTable_Free 0x401050)
 int textureCount;                     // +0x00 T
 int patternCount;                     // +0x04 T: number of LilAniPattern records (255 once loaded)
 int frameCount;                       // +0x08 T
 struct LilAniTexture *textures;       // +0x0C T
 struct LilAniPattern *patterns;       // +0x10 T
 struct LilAniFrame *frames;           // +0x14 T
 int hanteiSetCount;                   // +0x18 T: equals frameCount
 int rectCount;                        // +0x1C T
 struct LilAniHanteiSet *hanteiSets;   // +0x20 T
 struct LilAniRect *rects;             // +0x24 T
 int effectSetCount;                   // +0x28 T: equals frameCount
 int effectParamCount;                 // +0x2C T: 1 (a zero row) when the file has no [EffectParam] rows
 struct LilAniEffectSet *effectSets;   // +0x30 T
 struct LilAniEffectParam *effectParams; // +0x34 T
 int lastPattern;                      // +0x38 T: one-entry cache of LilAniTable_FrameIndex
 int lastFrame;                        // +0x3C T
 int lastIndex;                        // +0x40 T
};
struct LilAniAsset {                   // 0x34 bytes, cached per file name by LilAniAsset_Get 0x43ECF0 (LilAniAsset_LoadFiles 0x43EB10 loads the .TXT and the EX3 textures)
 void *unknown_00;                     // +0x00 I: vtable or cache link, set by the constructor 0x43E970
 int shiftX;                           // +0x04 T: current frame ShiftX (LilAniAsset_LoadFrameUV)
 int shiftY;                           // +0x08 T
 int width;                            // +0x0C T: current frame W
 int height;                           // +0x10 T: current frame H
 float u0;                             // +0x14 T: U / textureWidth (flipped when AttrFlag bit0)
 float v0;                             // +0x18 T: V / textureHeight (flipped when AttrFlag bit1)
 float du;                             // +0x1C T: W / textureWidth (negated when flipped)
 float dv;                             // +0x20 T: H / textureHeight
 void *currentTexture;                 // +0x24 T: texture object of the current frame (textures[tpage]); +0x1C/+0x20 of it hold width/height
 struct LilAniTable *table;            // +0x28 T
 unsigned int unknown_2C;              // +0x2C I
  void *textures;                       // +0x30 T: array of texture objects, one per [TextureNN]
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// BG chips (.BGC "FCHIP SYS") and maps (.MAP "FMAP SYS"), loaded by LilBgLayer_Load 0x404A40
struct LilBgcFileHeader {              // 33 bytes on disk (packed, unaligned): magic + 6 dwords
 char magic[9];                        // +0x00 T: "FCHIP SYS" (no NUL)
 unsigned int version;                 // +0x09 T: must be 0
 enum LilChipMode mode;                // +0x0D T: 0 = 8-bit indexed (collision layers), non-zero (1) = 24-bit RGB (picture layers)
 unsigned int flag;                    // +0x11 T: read into a local and copied to the layer (+0x40); 1 in every shipped file
 unsigned int zero;                    // +0x15 T: copied to layer +0x44; 0 in every shipped file
 unsigned int gridCols;                // +0x19 T: copied to layer +0x48; chip atlas columns the editor used (gridCols * gridRows == chipCount)
 unsigned int gridRows;                // +0x1D T: copied to layer +0x4C
};
struct LilChipset {                    // 0x50 bytes at the start of LilBgLayer; one union of the two modes (LilChipset_Copy 0x402600)
 enum LilChipMode mode;                // +0x00 T
 int rgbChipCount;                     // +0x04 T: mode 1: chips in the file
 unsigned char *rgbColor;              // +0x08 T: mode 1: chipCount * w * h * 3 bytes, chip-major, rows top down, 3 bytes per pixel
 unsigned char *rgbAlpha;              // +0x0C T: mode 1: second plane of the same size; the 3 bytes of a pixel are equal (gray alpha)
 int rgbChipW;                         // +0x10 T: mode 1: chip width (32 in all files)
 int rgbChipH;                         // +0x14 T: mode 1: chip height (32)
 int indexedChipCount;                 // +0x18 T: mode 0: chips in the file
 int *indexedPaletteSelect;            // +0x1C T: mode 0: chipCount dwords (palette row start per chip; all 0 in the shipped files)
 unsigned char *indexedPixels;         // +0x20 T: mode 0: chipCount * w * h bytes = the LilChipAttr value per pixel (what the game queries)
 unsigned char *indexedPixels2;        // +0x24 T: mode 0: second plane (identical to the first in the shipped files)
 int indexedChipW;                     // +0x28 T: mode 0: chip width (32)
 int indexedChipH;                     // +0x2C T: mode 0: chip height (32)
 unsigned int *paletteA;               // +0x30 T: mode 0: 256 ARGB dwords (first plane palette)
 int paletteACount;                    // +0x34 T: mode 0: 256
 unsigned int *paletteB;               // +0x38 T: mode 0: 256 ARGB dwords
 int paletteBCount;                    // +0x3C T: mode 0: 256
 unsigned int headerFlag;              // +0x40 T: LilBgcFileHeader.flag
 unsigned int headerZero;              // +0x44 T: LilBgcFileHeader.zero
 unsigned int gridCols;                // +0x48 T: LilBgcFileHeader.gridCols
 unsigned int gridRows;                // +0x4C T: LilBgcFileHeader.gridRows
};
struct LilMapFileHeader {              // 20 bytes on disk, followed by width * height little-endian u16 chip indices (row major)
 char magic[8];                        // +0x00 T: "FMAP SYS"
 unsigned int version;                 // +0x08 T: must be 0
 unsigned int width;                   // +0x0C T: chips per row
 unsigned int height;                  // +0x10 T: rows
};
struct LilMap {                        // 12 bytes inside LilBgLayer (LilMap_ReadMap 0x402AD0)
 unsigned short *cells;                // +0x00 T: width * height u16 chip indices (index into the layer's LilChipset)
 int width;                            // +0x04 T
 int height;                           // +0x08 T
};
struct LilBgLayer {                    // 152 bytes: g_BgLayers 0x47FD58 = 5 layers x 2 sets (index layer + 5 * set, stride 152), plus the collision layer g_BgHanteiLayer 0x480448
 struct LilChipset chips;              // +0x00 T: .BGC
 struct LilMap map;                    // +0x50 T: .MAP
 unsigned int *textures;               // +0x5C T: LilBgLayer_BuildTextures output: one D3D texture per chip (and per alpha chip for mode 1)
 float posX;                           // +0x60 T: SetBgPos x
 float posY;                           // +0x64 T: SetBgPos y
 float loopRefX;                       // +0x68 T: copy of posX used by the loop wrap test
 float loopRefY;                       // +0x6C T
 float ratioX;                         // +0x70 T: SetScrollRatio x
 float ratioY;                         // +0x74 T: SetScrollRatio y
 float velX;                           // +0x78 T: SetScrollX * ratioX
 float velY;                           // +0x7C T: SetScrollY * ratioY
 float accelX;                         // +0x80 T: SetScrollAddX * ratioX
 float accelY;                         // +0x84 T: SetScrollAddY * ratioY
 int loopEnabled;                      // +0x88 T: SetScrollLoop arg 2 != 0
 int loopWidth;                        // +0x8C T: SetScrollLoop arg 3
 float loopStartX;                     // +0x90 T: loopRefX at the time SetScrollLoop ran
 int drawPriority;                     // +0x94 T: SetBgPrio: 0 -> 0, 1 -> 230, 2 -> 400
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// stage script interpreter (bgNNProc[_k].txt): raw line cursor g_BgScriptCursor 0x480524, command table g_BgScriptCommands 0x471210
struct LilBgScriptCommand {            // 40 bytes
 char name[32];                        // +0x00 T: command name, compared exactly (case sensitive) with the token before '=' / ' ' / TAB
 unsigned int id;                      // +0x20 T: LilBgCmdId; 0xFFFFFFFE = "//" comment line, 0xFFFFFFFF = End (terminator row)
 void *handler;                        // +0x24 T: int __cdecl handler(char *argsAfterEquals); returns 0 to stop for this frame (Wait not yet reached), 1 to continue
};
struct LilSpawnParams {                // 32 bytes built by the SetEnemy handler and passed to LilEnemy_Spawn 0x436730
 int enemyId;                          // +0x00 T: 3rd SetEnemy argument = [Enemy_NNN] index (1000 = generic effect object)
 float x;                              // +0x04 T: 1st argument (0xFFFF = random 0..639)
 float y;                              // +0x08 T: 2nd argument (0xFFFF = random 0..479)
 int depthBias;                        // +0x0C T: 4th argument; draw priority = bias + 200 (Position 0) or + 250 (Position 1)
 int unknown_10;                       // +0x10 I
 int flag;                             // +0x14 I: nonzero marks the child as a sub-object of the parent
 struct LilEntity *parent;             // +0x18 T: 0 for script spawns
 int areaBoss;                         // +0x1C T: 1 for SetEnemy_AreaBoss (stored into entity +0x218)
};
struct LilEnemyDef {                   // 192 bytes x 1000 = g_EnemyDefs 0x56B308 (+ generic g_EnemyDefGeneric 0x56AFA0 with id 1000); LilEnemyDef_ReadKeys 0x435FF0
 int enemyId;                          // +0x00 T: Type= (default = the section number NNN); -1 = section absent in every source
 unsigned int unknown_04;              // +0x04 U
 unsigned int unknown_08;              // +0x08 U
 float maxHp;                          // +0x0C T: MaxHp=
 float attack;                         // +0x10 T: Attack=
 int score;                            // +0x14 T: Score=
 char aniTablePath[32];                // +0x18 T: AniTablePath=
 char aniTableName[32];                // +0x38 T: AniTableName=
 struct LilAniAsset *aniAsset;         // +0x58 T: loaded asset for aniTablePath + aniTableName
 enum LilEnemyPosition position;       // +0x5C T: Position= 0 front plane, 1 back plane (stomp test ignores plane 1)
 int isScroll;                         // +0x60 T: IsScroll= pulled along by the background scroll
 int groundHit;                        // +0x64 T: GroundHit= collides with the collision layer
 int startPat;                         // +0x68 T: StartPat= first pattern
 int destroyPat;                       // +0x6C T: DestroyPat= pattern on death (default -1)
 int playerBulletHit;                  // +0x70 T: PlayerBulletHit=
 int isBullet;                         // +0x74 T: IsBullet=
 int centerPos;                        // +0x78 T: CenterPos=
 int radarType;                        // +0x7C T: RadarType= (default -1)
 int itemType;                         // +0x80 T: ItemType= (default -1; -2 means "use SetNextItem")
 int noShotDownPlus;                   // +0x84 T: NoShotDownPlus=
 int shadow;                           // +0x88 T: Shadow=
 int shadowX;                          // +0x8C T: ShadowX= (parsed as float, stored as int)
 int shadowY;                          // +0x90 T: ShadowY=
 int shadowW;                          // +0x94 T: ShadowW=
 int param[10];                        // +0x98 T: Param_00..Param_09
};
struct LilBulletDef {                  // 108 bytes x 100 = g_BulletDefs 0x480650 ([Bullet_NNN] of CharaBullet.txt, LilBulletDefs_Load 0x404D70)
 int bulletId;                         // +0x00 T: NNN
 float speed;                          // +0x04 T: Speed=
 int time;                             // +0x08 T: Time= lifetime in ticks
 char aniTablePath[32];                // +0x0C T: AniTablePath=
 char aniTableName[32];                // +0x2C T: AniTableName=
 struct LilAniAsset *aniAsset;         // +0x4C T
 int startPattern;                     // +0x50 T: StartPattern=
 int hitPattern;                       // +0x54 T: HitPattern=
 int groundHit;                        // +0x58 T: GroundHit=
 int hitVectorRoll;                    // +0x5C T: HitVectorRoll=
 float power;                          // +0x60 T: Power=
 float powPower;                       // +0x64 T: PowPower=
 int detonation;                       // +0x68 T: Detonation= (atof, stored as int)
};
struct LilEffectDef {                  // 84 bytes x 1000 = g_EffectDefs 0x5A0260 ([Effect_NNN] of SystemEffect.txt, LilEffectDefs_Load 0x441710)
 int effectId;                         // +0x00 T: Type= (default NNN)
 int time;                             // +0x04 T: Time=
 char aniTablePath[32];                // +0x08 T: AniTablePath=
 char aniTableName[32];                // +0x28 T: AniTableName=
 struct LilAniAsset *aniAsset;         // +0x48 T
 int startPat;                         // +0x4C T: StartPat=
 int isScroll;                         // +0x50 T: IsScroll=
};
struct LilEntity {                     // 0x278 bytes: enemy / bullet / effect object (LilEntity_New 0x435EA0, LilEntity_InitFromEnemyDef 0x436D60); the player is a larger class
 void *vtable;                         // +0x00 T
 int isScroll;                         // +0x04 T: from LilEnemyDef.isScroll
 unsigned char unknown_08[0x158];      // +0x08 U
 float posX;                           // +0x160 T: world x
 float posY;                           // +0x164 T: world y
 unsigned char unknown_168[0xC];       // +0x168 U
 float offsetX;                        // +0x174 T
 float offsetY;                        // +0x178 T
 unsigned char unknown_17C[0x10];      // +0x17C U
 int drawPriority;                     // +0x18C T: LilSpawnParams.depthBias + 200 / 250
 int unknown_190;                      // +0x190 U
 int visible;                          // +0x194 I: set to 1 by LilEntity_InitFromEnemyDef
 unsigned char unknown_198[0x10];      // +0x198 U
 float angle;                          // +0x1A8 T: radians / 6.283 (debug box rotation)
 unsigned char unknown_1AC[0xCC];     // +0x1AC U: includes finishedFlag at +0x1AC (byte, 1 = ended, 0xFF = held) and pattern/frame/timer ints at +0x1B4/+0x1B8/+0x1BC, animAsset at +0x1C0 (see docs)
};
// LilEntity is only partially typed; the proven offsets are listed in docs/formats/lilian.md section 11 (the full 0x278 layout is not claimed here).
#pragma pack(pop)
