// Melty Blood (Dec 2002, mb.exe) character-loader and runtime-state types, IDA-parsable (idc.parse_decls, one parse of this whole file).
// Applied in the IDB C:\dev\ida\server\mb.exe.i64 exactly as written here (all structs are packed: #pragma pack(1) is part of the text).
// Evidence tags in the field comments: T = access traced in mb.exe (function names are IDB names after this work), I = traced but meaning inferred from the code shape,
// E = data-bearing in the shipped files but no reader in mb.exe (editor-only), U = no access found anywhere (typed ctree scan of all 763 gameplay-range functions plus a raw
// pointer scan of the slot/pool iterator functions); fields named unused_<hexoff> are U fields, fields named writeOnly*_<hexoff> are written but never read.
// Documentation: docs/formats/mb.md. The container records (frame, pattern header, tables) are the GOF1 records of gof1_types.h under the Mb prefix.
#pragma pack(push,1)
struct MbActor;
struct MbCgPaletteEntry;
struct MbFighterSlot;
struct MbObjectSlot;
struct MbArchiveEntry;
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Enums (all unsigned char / unsigned short / unsigned int, bitmask enums are NOT flagged here; mb.md lists the bit meanings)
// ---------------------------------------------------------------------------------------------------------------------------------------------------
enum MbControlType : unsigned char { MBCTRL_HUMAN=0, MBCTRL_CPU=1 };
enum MbObjKind : unsigned char { MBOBJ_FIGHTER=0, MBOBJ_GHOST_TRAIL=31, MBOBJ_CHILD_8F=143, MBOBJ_CHILD=255 };
enum MbStance : unsigned char { MBSTANCE_GROUND=0, MBSTANCE_AIR=1, MBSTANCE_CROUCH=2 };
enum MbCancelPermission : unsigned char { MBCANCEL_NONE=0, MBCANCEL_ON_HIT_OR_GUARD=1, MBCANCEL_ALWAYS=2 };
enum MbAniFlag : unsigned char { MBANI_END_TO_PATTERN=0, MBANI_NEXT=1, MBANI_JUMP_TO_FRAME=2, MBANI_NEXT_LAND_TO_PATTERN=3, MBANI_JUMP_TO_FRAME_LAND_TO_PATTERN=4, MBANI_LOOP_COUNTED=5 };
enum MbHeatState : unsigned char { MBHEAT_IDLE=0, MBHEAT_ACTIVE=1, MBHEAT_COOLDOWN=2 };
enum MbLauncherState : unsigned char { MBLAUNCH_NONE=0, MBLAUNCH_LAUNCHER_CONNECTED=1, MBLAUNCH_FOLLOWUP_PATTERN_41=2 };
enum MbCommandMoveClass : unsigned char { MBMOVE_NORMAL=0, MBMOVE_SPECIAL=1, MBMOVE_SUPER=2 };
enum MbCommandFlags : unsigned char { MBCMD_STANCE_GROUND=1, MBCMD_STANCE_AIR=2, MBCMD_STANCE_CROUCH=4, MBCMD_NO_CANCEL_ENTRY=8, MBCMD_CLEAR_HISTORY_ON_ACTION=0x10, MBCMD_SCRIPT_ONLY=0x20, MBCMD_GUARD_CANCEL=0x40 };
enum MbCommandFlags2 : unsigned char { MBCMD2_USE_RING_B=1, MBCMD2_STRICT_SEQUENCE=2, MBCMD2_ALLOWED_IN_RESTRICTED_MODE=4, MBCMD2_HEAT_CANCEL_ONLY=0x40, MBCMD2_HEAT_CANCEL=0x80 };
enum MbCtFlags : unsigned char { MBCT_RESTRICT_REPEAT_MOVES=1, MBCT_BIT3_UNREAD=8, MBCT_GUARD_BY_INPUT_DIRECTION=0x10, MBCT_JUMP_CANCEL=0x20 };
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Archive (.p, "PAC v0"): header u32 plainFlag, u32 count^0xE3DF59AC, then 68-byte index entries, then the data
// ---------------------------------------------------------------------------------------------------------------------------------------------------
struct MbArchiveHandle { // 28 bytes, g_DataArchiveSlots[10]; LoadArchiveAndDecryptIndex fills it
 void *hFile; // +0x00 T: File_OpenRead
 void *hMapping; // +0x04 T: CreateFileMappingA (never read afterwards)
 void *view; // +0x08 T: MapViewOfFile
 unsigned int plainFlag; // +0x0C T: first file dword; PKArchive_ReadEntry returns before the payload cipher when non-zero
 int entryCount; // +0x10 T: second file dword ^ 0xE3DF59AC
 unsigned int unused_14; // +0x14 U: never written or read
 struct MbArchiveEntry *entries; // +0x18 T: GlobalAlloc(68 * count) holding the decoded index
};
struct MbArchiveEntry { // 68 bytes
 char name[60]; // +0x00 T: byte j (0..58) ^= (3*j*i + 61) & 0xFF (i = entry index); byte 59 and the NUL padding are not enciphered
 unsigned int offset; // +0x3C T: absolute file offset of the data (stored plain)
 unsigned int sizeXorKey; // +0x40 T: size ^ 0xE3DF59AC (decoded in place; the running sum is checked against the file size)
};
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Character .DAT container (identical to GOF1 apart from what mb.md section 3 lists)
// ---------------------------------------------------------------------------------------------------------------------------------------------------
struct MbFileHeader { // 0x44 bytes
 char signature[8]; // +0x00 E: CP932 94 F5 91 4F 92 B7 91 44 (Bizen Osafune); never compared
 unsigned char unused_08[8]; // +0x08 U: zero in all 18 files, never read
 unsigned int version; // +0x10 E: 18 in all files; MB does NOT check it (GOF1 does)
 unsigned int patternAreaEnd; // +0x14 T: byte size of header + pattern area; 0 means "use cgOffset"
 unsigned int partsSize; // +0x18 T: size of the parts blob (old PAT v2), 0 in 9 of 18 files
 unsigned int cgOffset; // +0x1C T: start of the CG blob (= patternAreaEnd + partsSize)
 unsigned int cgSize; // +0x20 T: size of the CG blob (non-zero in all 18 files)
 unsigned char unused_24[0x20]; // +0x24 U: zero in all files, never read
};
struct MbCharDatHead { // what g_CharPatternData[slot] points to
 struct MbFileHeader header; // +0x00
 int patternOffset[256]; // +0x44 T: absolute offset of pattern p in this block, -1 = absent
};
struct MbPatternHeader { // 20 bytes, followed by frameCount * 116-byte frames, then box / AT / IF / EF tables
 unsigned char frameCount; // +0x00 T
 unsigned char moveInfo; // +0x01 T: low nibble move type, 0x40 linear filter (Pattern_GetLinearFilterBit), 0x80 double-resolution CG (Pattern_IsCgDoubleRes)
 unsigned char moveLevel; // +0x02 T: Pattern_GetMoveLevel
 unsigned char frameSizeTag; // +0x03 E: 0 in all MB files (116 in some GOF1 backups)
 int boxTableOffset; // +0x04 T: -1 = none
 int atTableOffset; // +0x08 T: -1 = none
 int ifTableOffset; // +0x0C T
 int efTableOffset; // +0x10 T
};
struct MbAnimFrame { // frame +0x00..0x27
 __int16 spriteId; // +0x00 T: parts-pattern index < 10000, CG image (id-10000) otherwise
 __int16 offsetX; // +0x02 T
 __int16 offsetY; // +0x04 T
 unsigned __int16 duration; // +0x06 T
 unsigned char drawMode; // +0x08 T
 unsigned char blendMode; // +0x09 T
 unsigned char alpha; // +0x0A T
 enum MbAniFlag aniFlag; // +0x0B T
 unsigned char jumpTarget; // +0x0C T
 unsigned char landJumpTarget; // +0x0D T
 unsigned char drawPriority; // +0x0E T
 unsigned char unused_0F; // +0x0F U: zero in all frames
 unsigned __int16 zoom; // +0x10 T: 256 = 1.0, 0 = none
 unsigned char loopCount; // +0x12 T
 unsigned char loopEndFrame; // +0x13 T
 unsigned char unused_14[4]; // +0x14 U
 unsigned char interpolateToNext; // +0x18 T
 unsigned char unused_19[15]; // +0x19 U
};
struct MbStateFrame { // frame +0x28..0x4F
 unsigned char clearVelX; // +0x00 T
 unsigned char clearVelY; // +0x01 T
 unsigned char addVelX; // +0x02 T
 unsigned char addVelY; // +0x03 T
 unsigned char unreadData_04[4]; // +0x04 E: zero except in 16 CIEL frames; no reader in MB
 __int16 velX; // +0x08 T
 __int16 velY; // +0x0A T
 __int16 accelX; // +0x0C T
 __int16 accelY; // +0x0E T
 enum MbStance stance; // +0x10 T
 enum MbCancelPermission normalCancel; // +0x11 T
 enum MbCancelPermission specialCancel; // +0x12 T
 unsigned char attackHitCount; // +0x13 T
 unsigned char canAct; // +0x14 T
 unsigned char unused_15[3]; // +0x15 U
 unsigned int flags1; // +0x18 T
 unsigned int flags2; // +0x1C T
 __int16 maxVelX; // +0x20 T
 unsigned char unused_22[6]; // +0x22 U
};
struct MbFrameRecord { // 116 bytes
 struct MbAnimFrame anim; // +0x00
 struct MbStateFrame state; // +0x28
 unsigned char atIndex; // +0x50 T: signed char AT index, 0xFF = none
 unsigned char atIndexHighByte; // +0x51 E
 __int16 ifIndex[3]; // +0x52 T: -1 = empty
 __int16 efIndex[4]; // +0x58 T: -1 = empty
 __int16 boxIndex[10]; // +0x60 T: -1 = empty
};
struct MbBox { // 8 bytes
 __int16 x1; // +0
 __int16 y1; // +2
 __int16 x2; // +4
 __int16 y2; // +6
};
struct MbRectI32 { // runtime rectangle
 int x1;
 int y1;
 int x2;
 int y2;
};
struct MbAtRecord { // 26 bytes
 unsigned char hitReaction; // +0x00 T
 unsigned char hitstopLevel; // +0x01 T
 unsigned char hitSparkClass; // +0x02 T
 unsigned char attackKind; // +0x03 T
 unsigned char hitSoundOverride; // +0x04 T
 unsigned char flagsA; // +0x05 T
 __int16 damage; // +0x06 T
 __int16 meterGain; // +0x08 T
 __int16 guardBreakValue; // +0x0A T
 __int16 heatGaugeGain; // +0x0C T: MB-specific: gauge gain granted to the VICTIM on an unguarded hit (Battle_ResolveHit -> Actor_AddHeatGauge)
 unsigned char unused_0E; // +0x0E U: zero in all files, never read
 unsigned char statusEffect; // +0x0F T: Character_ApplyAddedEffect(mode 1..4)
 unsigned char flagsB; // +0x10 T
 unsigned char unused_11; // +0x11 U
 unsigned __int16 hitClass; // +0x12 T
 unsigned __int16 statusEffectParam; // +0x14 T: MB-specific: third argument of Character_ApplyAddedEffect
 unsigned char unused_16[4]; // +0x16 U
};
struct MbIfRecord { // 28 bytes
 unsigned char type; // +0x00 T
 unsigned char unused_01[3]; // +0x01 U
 int param[4]; // +0x04 T
 unsigned char unused_14[8]; // +0x14 U
};
struct MbEfRecord { // 20 bytes
 unsigned char type; // +0x00 T
 unsigned char subType; // +0x01 T
 __int16 arg[5]; // +0x02 T
 unsigned char unused_0C[8]; // +0x0C U
};
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Parts blob (old PAT v2) - byte-identical to GOF1
// ---------------------------------------------------------------------------------------------------------------------------------------------------
struct MbPartsHeader { // 0x8CBC bytes
 unsigned int version; // +0x00 E: 2
 unsigned int magic; // +0x04 E: 0x01234567
 unsigned char unused_08[16]; // +0x08 U
 unsigned int patternOffset[1000]; // +0x18 T: blob-relative, 0 = absent
 char patternName[1000][32]; // +0xFB8 E
 unsigned int textureRegionOffset; // +0x8CB8 T
};
struct MbPart { // 92 bytes, 40 per parts pattern
 int x; // +0x00 T
 int y; // +0x04 T
 int width; // +0x08 T
 int height; // +0x0C T
 unsigned char flipMode; // +0x10 T
 unsigned char linearMagFilter; // +0x11 T
 unsigned char editorFlag_12; // +0x12 E
 unsigned char unused_13; // +0x13 U
 int scaleX; // +0x14 T: 1/1000
 int scaleY; // +0x18 T
 int rotation; // +0x1C T: 1/10000 turn
 unsigned char colorA; // +0x20 T
 unsigned char colorR; // +0x21 T
 unsigned char colorG; // +0x22 T
 unsigned char colorB; // +0x23 T
 unsigned char addR; // +0x24 T
 unsigned char addG; // +0x25 T
 unsigned char addB; // +0x26 T
 unsigned char unused_27; // +0x27 U
 int textureSlot; // +0x28 T: SpriteDataSlot_UploadPartsAndCG adds 200*slot+2
 int srcX; // +0x2C T
 int srcY; // +0x30 T
 int srcWidth; // +0x34 T
 int srcHeight; // +0x38 T
 unsigned char drawOrder; // +0x3C T
 unsigned char unused_3D[3]; // +0x3D U
 int scaleOriginX; // +0x40 T
 int scaleOriginY; // +0x44 T
 unsigned char unused_48[20]; // +0x48 U
};
struct MbTextureRegion { // 0x2E30 header bytes, then raw A8R8G8B8 squares
 unsigned char unused_00[20]; // +0x00 U
 unsigned int version; // +0x14 E: 1
 unsigned int textureCount; // +0x18 E
 unsigned int textureOffset[50]; // +0x1C T: region-relative, 0 = unused slot
 char textureName[50][64]; // +0xE4 E
 unsigned int textureSize[50]; // +0xD64 T: edge length (512)
 unsigned char unused_E2C[8196]; // +0xE2C U
};
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// CG blob (MB-specific; GOF1 never stores one)
// ---------------------------------------------------------------------------------------------------------------------------------------------------
struct MbCgPaletteEntry { // 4 bytes
 unsigned char r; // +0 T: byte 0 indexes the red conversion table (0x758982) in Surface_BlitIndexedBitmapToSlot
 unsigned char g; // +1 T
 unsigned char b; // +2 T
 unsigned char flag; // +3 U: 0 or 1, never read
};
struct MbCgPiece { // 14 bytes, one 16-row strip of an image
 __int16 x; // +0x00 T: destination x relative to the image origin (128, 224; halved for double-resolution patterns)
 __int16 y; // +0x02 T
 __int16 width; // +0x04 T: 1..256
 __int16 height; // +0x06 T: always 16 in the shipped data
 __int16 u; // +0x08 T: source x inside the 256x256 texture
 __int16 v; // +0x0A T: source y (multiple of 16)
 __int16 textureSlot; // +0x0C T: texture index; SpriteDataSlot_UploadPartsAndCG overwrites it with the surface slot id
};
struct MbCgImageHeader { // 52 bytes, then the strips' pixels
 char sourceName[36]; // +0x00 E: "AKI00_000.BMP", NUL terminated, remainder is uninitialised editor memory
 __int16 bound[4]; // +0x24 E: x1, y1, x2, y2 of the image (never read by the game)
 unsigned int firstPiece; // +0x2C T: index of the image's first strip in the piece table
 unsigned __int16 pieceCount; // +0x30 T: strip count (Draw_ActorSpriteScratch reads a word, SpriteDataSlot_UploadPartsAndCG a signed byte)
 unsigned __int16 unusedGarbage_32; // +0x32 E: uninitialised editor memory in a few images, never read
};
struct MbCgBlobHead { // first 20220 bytes of the CG blob (g_CharCgData[slot])
 unsigned char titleStub[6]; // +0x00 E: CP932 bytes 82 C9 82 E5 81 48 in all files
 unsigned char unused_06[10]; // +0x06 U: zero
 unsigned char trueColorFlag; // +0x10 T: 0xFF = images are 3-byte BGR rasters, anything else = 8-bit indexed
 unsigned char unused_11[2]; // +0x11 U: zero
 unsigned char unused_13; // +0x13 U: 0x80 in all files
 int imageOffset[3000]; // +0x14 T: blob-relative offset of image i, -1 = absent
 struct MbCgPaletteEntry palette[8][256]; // +0x2EF4 T: palette set selected by the character colour (paletteIndex 0..7)
 unsigned int pieceCount; // +0x4EF4 E: number of 14-byte strip records
 unsigned int imageDataSize; // +0x4EF8 E: bytes from the first image to the end of the blob
};
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// CT (command table), WMT (win message table), CHARSEL, CPF (CPU AI)
// ---------------------------------------------------------------------------------------------------------------------------------------------------
struct MbCtCommand { // 44 bytes
 unsigned char commandId; // +0x00 T: own index, 0xFF = unused slot
 unsigned char unused_01; // +0x01 U: zero in all files
 unsigned char sequence[32]; // +0x02 T: token string, 0xFF terminated: 0..9 direction, 0x41..0x46 buttons A..F, 0x2B '+' joiner, 0x54 'T' tap
 unsigned char targetPattern; // +0x22 T: pattern requested when the command fires
 enum MbCommandMoveClass moveClass; // +0x23 T
 unsigned __int16 meterCost; // +0x24 T: super meter needed (10000 per level), copied to actor.activeMoveMeterCost
 unsigned char requirePartner; // +0x26 T: 1 = only usable when the team has a partner (g_TeamRecords.partnerPresent)
 unsigned char counterRequirement; // +0x27 T: >= 100: global counter bank; < 100: tens digit = tobiCount[] index, units digit = amount
 unsigned char usageLimit; // +0x28 T: refused once actor.dashCount reaches it (0 = unlimited)
 enum MbCommandFlags flags; // +0x29 T
 enum MbCommandFlags2 flags2; // +0x2A T
 unsigned char unused_2B; // +0x2B U: zero in all files, never read
};
struct MbCtEvadeEntry { // 16 bytes, one per double-tap direction (6, 2, 4, 8)
 unsigned __int16 enabled; // +0x00 T
 unsigned __int16 pattern; // +0x02 T
 unsigned char unused_04[8]; // +0x04 U: zero in all files
 unsigned __int16 window; // +0x0C T
 unsigned __int16 cooldown; // +0x0E T
};
struct MbCtHeader { // 92 bytes
 unsigned char maxAirJumps; // +0x00 T: Character_ProcessPlayerInput (inputHeldFlags & 0x7F counts the jumps used)
 unsigned char unused_01; // +0x01 U: 9 in all files, never read
 unsigned char recoveryStyle; // +0x02 T: ObjRunActionScript (0, 1 or 2)
 unsigned char unused_03; // +0x03 U
 unsigned char unused_04; // +0x04 U
 enum MbCtFlags flags; // +0x05 T
 unsigned char unused_06[2]; // +0x06 U
 float knockbackScale; // +0x08 E: 1.0 (GAKIHA 1.0); no reader found in MB
 float damageScale[4]; // +0x0C T: only [0] is read (Damage_ScaleByDifficultyAndTeam, Damage_ScaleByGuardGaugeAndStance); data 0.9 (1.0 in WARC, 0.4/0.3/0.3/0.1 in GAKIHA)
 struct MbCtEvadeEntry evade[4]; // +0x1C T: Fighter_DetectEvadeDoubleTap
};
struct MbCtFile { // 4496 bytes
 unsigned int commandCount; // +0x00 T: slot.commandCount
 struct MbCtCommand command[100]; // +0x04 T
 struct MbCtHeader ct; // +0x1134 T
};
struct MbWmtRecord { // 156 bytes
 unsigned char loserCharId; // +0x00 T: loser's character file id this message applies to, 0xFF = any
 unsigned char unused_01; // +0x01 U
 unsigned __int16 oneInN; // +0x02 T: acceptance odds (0 = always); the picker retries while rand() % oneInN != 0
 unsigned __int16 imageVariant; // +0x04 T: win_<char><nn>.bmp variant (0 = win_<char>.bmp)
 unsigned char unused_06[150]; // +0x06 U: zero (one stray 0x30 at +6 in V_SION)
};
struct MbCharSelEntry { // 136 bytes
 char name[32]; // +0x00 T: face bitmap / display base name (".\grp\system\<name>_face.bmp")
 char datName[32]; // +0x20 T: data file base name (<datName>.dat, _c.ct, .cpf, .wmt)
 char partnerDatName[32]; // +0x40 T: second character file of a two-character team, "0" = none, "1" = special single-fighter marker
 unsigned int runtimeTableIndex; // +0x60 T: written by CSS_LoadCharselTxtGridWithUnlocks (file value 0)
 unsigned int charFileId; // +0x64 T: id used by g_CharFileIdToTableIndex, voice ids, statistics
 unsigned int homeStageId; // +0x68 T: stage chosen for the character (bg%02dinfo.txt)
 unsigned int gridIconIndex; // +0x6C T: CSS_DrawCharGridIcons: sheet (>>4) + 1670, column (&3), row ((>>2)&3)
 unsigned int unlockMask; // +0x70 T: tested against g_UnlockMask, 0 = always available
 unsigned int page; // +0x74 T: 1 = grid rows 10+
 unsigned int gridPos; // +0x78 T: row * 10 + column
 unsigned int portraitWidth; // +0x7C T: CSS_DrawPanelPortraitAndInfo draws (width >> 1)
 unsigned int portraitHeight; // +0x80 T
 unsigned int unused_84; // +0x84 U: zero in all files, never read
};
struct MbCharSelFile { // 2180 bytes (16 entries): CHARSEL.CT, the entry block is enciphered with the CP932 string key 83 74 83 40 83 43 83 8B 82 AA 8C A9 82 C2 82 A9 82 E8 82 DC 82 B9 82 F1
 unsigned int count; // +0x00
 struct MbCharSelEntry entry[16]; // +0x04
};
struct MbCpfStep { // 44 bytes
 unsigned short inputCode; // +0x00 T: CPU_ApplyCpfStepInput switch (0/15 neutral, 1 fwd, 2 back, 3 down, 4 up, 5 up-fwd, 6 up-back, 7..10 A..D, 11..14 down+A..D, 16 = command move)
 unsigned short durationBase; // +0x02 T: frames
 unsigned short durationRandom; // +0x04 T: rand() % durationRandom added to durationBase
 unsigned short commandIndex; // +0x06 T: code 16: index handed to Fighter_TryStartCommandMove
 unsigned char unused_08[16]; // +0x08 U
 unsigned int flags; // +0x18 T: 1 = end script, 2 = end script (alt), 4 = wait for hit, 8 = ..., 0x10 = press button mask, bit31 = set cpuCommandReady
 unsigned int unused_1C; // +0x1C U
 unsigned char dirCode; // +0x20 T: CPU_CpfDirCodeToNumpad
 unsigned char commandReadyLevel; // +0x21 T: when flags bit31 is set, cpuCommandReady = clamp(value + 1, 1, 3)
 unsigned char unused_22[10]; // +0x22 U
};
struct MbCpfCondition { // 132 bytes, last part of each script block
 int minDistX; // +0x00 T: CPU_FindTargetInRange (distance window to the target, world/256)
 int maxDistX; // +0x04 T
 int useYRange; // +0x08 T
 int minDistY; // +0x0C T
 int maxDistY; // +0x10 T
 int weight; // +0x14 T: 0 = script is never a candidate; also used as the percent chance after selection
 int unused_18; // +0x18 U
 int unused_1C; // +0x1C U
 int ownStanceCond; // +0x20 T: 0 any, 1..4 ground/air/crouch rules
 int targetStanceCond; // +0x24 T
 int targetCanActCond; // +0x28 T
 int enemyAttackNearbyCond; // +0x2C T
 int patternCondEnable; // +0x30 T
 int patternId; // +0x34 T
 int patternFrame; // +0x38 T: 255 = any frame
 int ownLifeCond; // +0x3C T
 int unused_40; // +0x40 U
 int targetNotDownedCond; // +0x44 T
 int interruptible; // +0x48 T
 int cooldown; // +0x4C T: frames before the script can be chosen again
 int minDifficulty; // +0x50 T: skipped when g_Opt_CpuDifficulty < value
 unsigned char unused_54[48]; // +0x54 U
};
struct MbCpfScript { // 1892 bytes
 struct MbCpfStep step[40]; // +0x000 T
 struct MbCpfCondition condition; // +0x6E0 T
};
struct MbCpfFile { // 193388 bytes
 int guardPercent; // +0x00 T: CPU_RollGuardReaction (reaction chance, scaled by difficulty in CPU_GuardPercentByDifficulty)
 unsigned char unused_04[184]; // +0x04 U: zero in all 20 files
 struct MbCpfScript script[100]; // +0xBC T
 char scriptName[100][40]; // +0x2E3CC E: CP932 editor labels, never read
};
struct MbCpuCandidate { // 8 bytes, g_CpuCandidates[100]
 int weight; // +0x00 T
 int score; // +0x04 T
};
struct MbHitVectorRow { // 18 bytes, row r of g_HitVectorTable[32]: VECTOR.TXT section 1, 9 integers stored as int16; row = hit reaction id (Actor_StartKnockback)
 __int16 velX; // +0x00 T: knockback velocity X, multiplied by the attack direction sign (-> actor.knockVelX); column 0
 __int16 velY; // +0x02 T: knockback velocity Y (-> actor.knockVelY); column 1
 __int16 accelX; // +0x04 T: knockback friction X (-> actor.knockAccelX); column 2
 __int16 accelY; // +0x06 T: knockback gravity Y (-> actor.knockAccelY); column 3
 __int16 launchVelX; // +0x08 T: stage-2 launch velocity X (-> actor.launchVelX); column 4
 __int16 launchVelY; // +0x0A T: column 5 (-> actor.launchVelY)
 __int16 launchAccelX; // +0x0C T: column 6 (-> actor.launchAccelX)
 __int16 launchAccelY; // +0x0E T: column 7 (-> actor.launchAccelY)
 __int16 hitStage; // +0x10 T: stun frames (< 240) or launch stage code (255 = blow-away, 241 = air hit-stun, 220 = guard-stun); column 8 (-> actor.hitStage)
};
struct MbInputRing { // 512 bytes, 64 entries newest first
 unsigned int state[64]; // +0x000 T: (direction << 24) | buttons for each distinct input state (ring A: held, ring B: held | released)
 unsigned int age[64]; // +0x100 T: frames spent in state[i], capped at 1000000
};
struct MbCpuState { // 224 bytes, g_CpuStates[4] (one per player slot), CPU AI runtime state
 int scriptIndex; // +0x00 T: running script (CPU_StartCpfActionStep)
 int stepIndex; // +0x04 T: running step
 int stepTimer; // +0x08 T: frames left in the step (decremented by CPU_TickCpfStep)
 int decisionCooldown; // +0x0C T: frames before the next script decision (CPU_RunAiForSlot, set from the difficulty in CPU_ScoreAndSelectAction)
 int targetDistX; // +0x10 T: |dx| to the nearest enemy (CPU_FindNearestEnemyDistance)
 int targetDistY; // +0x14 T: dy to the nearest enemy
 unsigned short scriptCooldown[100]; // +0x18 T: per-script cooldown counters (decremented by CPU_TickScriptCooldowns)
};
struct MbTeamRecord { // 68 bytes, g_PlayerSlotRecords[4]: per-team round/match record (Battle_LoadAllCharacterSlots writes partnerPresent / bossFlag)
 int pointSlotIndex; // +0x00 T: slot index of the team's on-field character (Players_ProcessSlotSwap toggles it)
 int partnerAiState; // +0x04 T: MaidsPartner_UpdateReserveAI state machine
 int partnerPresent; // +0x08 T: 1 when a second character was loaded for the team
 int bossFlag; // +0x0C T: 1 when the third CHARSEL string is "1" (single boss-type fighter)
 int score; // +0x10 T: WinScreen_Update adds the round score
 int teamStat_14; // +0x14 I: best value copied into a per-character record by Battle_RoundStateMachine
 int hitsTaken; // +0x18 T: ComboRecord_RegisterHit increments it for the victim team on every hit not flagged AT.flagsB & 4
 int teamStat_1C; // +0x1C I: best value copied by Battle_RoundStateMachine
 int teamStat_20; // +0x20 I
 int teamStat_24; // +0x24 I
 int evadesPerformed; // +0x28 T: Hitbox_DetectAttackHitsForAttacker increments it when an attack is evaded (attackResult bit 3)
 int counterHitsLanded; // +0x2C T: Battle_ResolveHit increments it next to counterHitSide = 1
 int roundFrameTotal; // +0x30 T: Battle_RoundStateMachine adds g_RoundFrameCounter
 int teamStat_34; // +0x34 I: story / continue counter
 int continuesUsed; // +0x38 I: ContinueScene_Update increments it; with teamStat_34 it is tested for 'first stage'
 int guardEnabled; // +0x3C I: Battle_ResolveHit skips the guard logic when 0 (training-mode guard setting)
 int damageScalePercent; // +0x40 I: read by Damage_ScaleByDifficultyAndTeam for victim and attacker team
};
struct MbTechVectorRow { // 18 bytes, row r of g_TechVectorTable[7]: VECTOR.TXT section 2 (tech / recovery vectors), 5 integers stored as int16
 __int16 velX; // +0x00 T: Actor_StartTechRecovery (mirrored by the facing)
 __int16 velY; // +0x02 T
 __int16 accelX; // +0x04 T
 __int16 accelY; // +0x06 T
 __int16 invulnFrames; // +0x08 T: becomes freezeTimerA/B and flashTimer (invulnerability)
 unsigned char unused_0A[8]; // +0x0A U: columns 5..8 are never filled
};

struct MbActor {
 unsigned char slotIndex; // +0x000 T: player slot / owner player index 0..3 (Slot_InitPointCharacterForRound writes the slot id); children copy it (InitChildObject); EfType9 sound bank
 unsigned char characterId; // +0x001 T: roster character file id (Battle_LoadAllCharacterSlots); ObjRunActionScript compares it with 16 (debug reset)
 enum MbControlType controlType; // +0x002 T: MbControlType, 1 = CPU (Battle_LoadAllCharacterSlots a5 == 1), CPU_RunAiForSlot / IF 11 / IF 32
 unsigned char unused_03; // +0x003 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 enum MbObjKind objKind; // +0x004 T: MbObjKind; 0 fighter, 31 ghost trail (EffectObject_SpawnGhostCopy), 0xFF / 0x8F spawned child (InitChildObject); values >= 0xF0 are tested as "is a child"
 unsigned char paletteIndex; // +0x005 T: colour palette chosen at character select (written by Battle_LoadAllCharacterSlots, preserved by Slot_InitPointCharacterForRound); the palette is applied at upload time, the byte itself is never read back
 unsigned char pattern; // +0x006 T: current pattern (action) id; Actor_ResolveFrameDataPointers indexes charData.patternOffset with it
 unsigned char frame; // +0x007 T: current frame inside the pattern
 unsigned __int16 spriteId; // +0x008 T: sprite id of the current frame (ObjEnterCurrentAction), read by the draw code
 unsigned char ifRunState; // +0x00A T: IF-table run gate (ObjEnterCurrentAction sets 1/2, Actor_TickWordTimers clears)
 unsigned char efRunState; // +0x00B T: EF-table run gate (Character_RunFrameEFs clears it after running)
 unsigned char airReactionActive; // +0x00C T: 1 while a launch/air hit-reaction stage (hitStage >= 240) is running (Actor_ApplyFrameMotionFlags)
 unsigned char unused_0D; // +0x00D U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 frameTicks; // +0x00E T: ticks spent on the current frame (compared with AnimFrame.duration)
 unsigned __int16 unused_10; // +0x010 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned char animationEnded; // +0x012 T: set by ObjRunActionScript when the animation reached its last frame; IF 2
 unsigned char unused_13; // +0x013 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned char loopCounter; // +0x014 T: AnimFrame.loopCount is loaded here; IF 9/10 and aniFlag 5 use it
 unsigned char unused_15; // +0x015 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 spawnFlagsA; // +0x016 T: child flags from EF arg2 bits (InitChildObject); 0x40 no ground landing, 0x20/0x8/0x4/0x2/0x1 see gof1.md Gof1ObjFlagsA (same meaning)
 unsigned __int16 spawnFlagsB; // +0x018 T: child flags from EF arg3 bits (0x40 ownership test in Actor_IsTickAllowed, 8 hitstop to owner, 1 draw shadow copy)
 unsigned char homingMode; // +0x01A T: EF arg4 low byte (InitChildObject); Actor_UpdateHomingObject
 unsigned char homingBaseAction; // +0x01B T: first pattern of the homing action family
 unsigned char spawnFlagsC; // +0x01C T (write-only): InitChildObject writes 0 / 1 and ORs 2 / 4 from EF arg bits; no reader found
 unsigned char unused_1D; // +0x01D U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 homingTick; // +0x01E T: counts up while homingTimeout != 0
 unsigned __int16 homingTimeout; // +0x020 T: EfType30 sub 0 arg2
 __int16 homingOffsetX; // +0x022 T: target offset X, 128 = centred
 __int16 homingOffsetY; // +0x024 T: target offset Y, 224 = baseline
 unsigned __int16 homingCooldown; // +0x026 T: set to 40 after a hit and counted down
 __int16 homingParam[10]; // +0x028 T: word table written by EfType30_SetActorParams sub 1 (word[40 + 2*arg0] = arg1)
 unsigned __int16 aiActionTimer; // +0x03C T: CPU AI: frames left in the running step (CPU_StartCpfActionStep loads durationBase + rand % durationRandom, CPU_TickCpfStep decrements)
 unsigned char unused_3E[2]; // +0x03E U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 __int16 aiScriptIndex; // +0x040 T: CPU AI: running script, -1/0 = none
 __int16 aiScriptStep; // +0x042 T: CPU AI: step inside the script
 unsigned char cpuCommandReady; // +0x044 T: CPU command request latch (CPU_StartCpfActionStep sets 1..3 from step.commandReadyLevel; IF 11/31 consume it)
 unsigned char unused_45[3]; // +0x045 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 int superMeter; // +0x048 T: super meter 0..30000 (10000 per level); AddSuperMeter, EfType6 op 4, Actor_SpawnEffectById spends activeMoveMeterCost
 unsigned char superLevelCrossMark; // +0x04C T: AddSuperMeter sets 10 when the level changes; partner mirror copies it
 unsigned char unused_4D[3]; // +0x04D U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 int heatGauge; // +0x050 T: 0..100 gauge filled by AT.heatGaugeGain (Actor_AddHeatGauge); at 100 Actor_TickHeatGauge enters heatState ACTIVE (MB analog of GOF reserveGauge)
 unsigned char teamCopiedOnly_54[4]; // +0x054 T: copied between a lead and its partner (MaidsPartner_UpdateReserveAI, Players_ProcessSlotSwap) and cleared at round start; never read otherwise
 int pendingHeatGain; // +0x058 T: gauge gain waiting to be banked into heatGauge when the combo ends (Fighter_BankPendingHeatGain clears it)
 int life; // +0x05C T: hit points, 10000 at round start (30000 in attract demo); Damage_*, Battle_ResolveHit, EfType5/6
 int maxLife; // +0x060 T: 10000, written next to life by Slot_InitPointCharacterForRound; partner mirror copies it
 __int16 heatTimer; // +0x064 T: delay / duration counter of the heat gauge (60 on gain, 60 when heat ends, counts down in Actor_TickHeatGauge)
 enum MbHeatState heatState; // +0x066 T: MbHeatState; 1 = heat active (gauge drains, aura drawn by Actor_DrawHeatAuraAndTintParticles), 2 = cooldown
 unsigned char heatCancelPending; // +0x067 T: armed by Fighter_TryStartCommandMove for a heat-cancel move; Actor_OnActionChanged then forces heatState = 2
 unsigned char unused_68[2]; // +0x068 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 __int16 heatLastGain; // +0x06A T: last gain passed to Actor_AddHeatGauge; doubled into heatTimer when the gauge fills
 unsigned char clearHistoryOnAction; // +0x06C T: set from command flag 0x10 (Fighter_TryStartCommandMove); Actor_OnActionChanged invalidates both input rings
 unsigned char unused_6D[3]; // +0x06D U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 int guardGauge; // +0x070 T: guard balance 0..10000, pulled toward 5000 by Actor_DecayGuardGauge, changed by Actor_AdjustGuardGauge; Character_Draw flashes above 8000; damage scale input
 int writeOnly5000_74; // +0x074 T (write-only): Battle_InitRoundCharactersAndTeams sets it to 5000 together with guardGauge; nothing reads it
 unsigned char guardRegenDelay; // +0x078 T: frames before the guard gauge starts to decay (30 / 60 set by Actor_AdjustGuardGauge); byte access only
 unsigned char unused_79[3]; // +0x079 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 int posX; // +0x07C T: world X in 1/128 pixel units
 int posY; // +0x080 T: world Y in 1/128 pixel units, 0 = ground, negative = up
 int posZ; // +0x084 T: read by the camera (Camera_UpdateFocusYZFromFighters, averaged into g_CameraZoomFocus) and never written: always 0
 int posXAfterIntegrate; // +0x088 T (write-only): posX when ObjIntegrateMotion returns
 int posXAtTickStart; // +0x08C T: posX before the tick
 int posYAtTickStart; // +0x090 T: posY before the tick
 int velX; // +0x094 T: velocity X
 int velY; // +0x098 T: velocity Y
 __int16 accelX; // +0x09C T: acceleration X
 __int16 accelY; // +0x09E T: acceleration Y (gravity)
 __int16 maxVelX; // +0x0A0 T: X speed clamp
 unsigned char unused_A2[2]; // +0x0A2 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 int knockVelX; // +0x0A4 T: knockback velocity X (Actor_StartKnockback)
 int knockVelY; // +0x0A8 T: knockback velocity Y
 __int16 knockAccelX; // +0x0AC T: knockback friction X
 __int16 knockAccelY; // +0x0AE T: knockback gravity Y
 int inertiaX; // +0x0B0 T: carried inertia added to posX each tick, decays by inertiaDecay
 int carriedVelX; // +0x0B4 T: velX saved when frame flags1 bit 0 is set, becomes inertiaX on the next pattern
 int knockCarryVelX; // +0x0B8 T: MB-specific: knockVelX saved by the wall-bounce stage (0xFE) and converted to inertia on the pattern change checked against knockCarryPattern
 __int16 inertiaDecay; // +0x0BC T: inertiaX / 10 (GOF: / 16), Actor_ApplyFrameMotionFlags
 unsigned __int16 carryPattern; // +0x0BE T: pattern in which carriedVelX was saved
 unsigned __int16 knockCarryPattern; // +0x0C0 T: MB-specific: pattern in which knockCarryVelX was saved
 __int16 grabDrawOffsetX0; // +0x0C2 T: start draw offset X of a grabbed actor (EF04/EF05, Character_Draw lerps to grabDrawOffsetX1)
 __int16 grabDrawOffsetY0; // +0x0C4 T: start draw offset Y
 __int16 zoomOriginX0; // +0x0C6 T: zoom/rotation origin X at the start, 320 by default
 __int16 zoomOriginY0; // +0x0C8 T: origin Y, 448 by default
 __int16 grabDrawOffsetX1; // +0x0CA T: end draw offset X
 __int16 grabDrawOffsetY1; // +0x0CC T: end draw offset Y
 __int16 zoomOriginX1; // +0x0CE T: origin X at the end
 __int16 zoomOriginY1; // +0x0D0 T: origin Y at the end
 __int16 rotAngleA; // +0x0D2 T: rotation angle, 1/10000 turn (+950 per tick while thrown)
 __int16 rotAngleB; // +0x0D4 T: second rotation angle
 unsigned char clearInertiaRequest; // +0x0D6 T: set by Actor_StartKnockback; Actor_ApplyFrameMotionFlags then clears inertiaX/inertiaDecay
 unsigned char attackResult; // +0x0D7 T: result of this actor attack on its victim: bit0 hit, bit1 guarded, ... (Battle_ResolveHit, Hitbox_Detect*); 0 = none
 unsigned char ownerAttackResult; // +0x0D8 T: copy of a child attack result written into its owner
 unsigned char drawDepth; // +0x0D9 T: draw order key (SetCharacterDrawPriority, SpawnGhostCopy)
 unsigned char drawModeVariant; // +0x0DA T: row offset into the grabbed draw-mode remap table (Character_Draw, Box_ApplyDrawModeTransform)
 unsigned char unused_DB; // +0x0DB U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 activeMoveMeterCost; // +0x0DC T: super meter cost of the running command move (IF 11 copies command.meterCost; Actor_SpawnEffectById spends it)
 unsigned char unused_DE; // +0x0DE U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 enum MbLauncherState launcherState; // +0x0DF T: MbLauncherState: 1 set by Battle_ResolveHit when an AT with flagsB bit 0 connects first (victim goes to pattern 30), cleared by the next hit; 2 while executing pattern 41 (follow-up); Character_ProcessPlayerInput uses it for the jump-cancel into pattern 41
 unsigned char unused_E0; // +0x0E0 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned char hitstopFrames; // +0x0E1 T: remaining hit-stop frames (Actor_SetHitstopFrames)
 unsigned char hitstopTrailing; // +0x0E2 T: set to 1 by Battle_ResolveHit; counted down with hitstopFrames by ObjRunActionScript
 unsigned char contactMark; // +0x0E3 T: set by the hit detection when this actor touched a target; CPU_TickCpfStep reads it, CPU_ApplyCpfStepInput clears it
 unsigned char grabState; // +0x0E4 T: 0 free, 1 held by a fighter, 2 held by a child object (Actor_TryGrabFighter)
 unsigned char inputLock; // +0x0E5 T: EfType6 op 9 sets it; blocks input dispatch and IF input handling; ObjRunActionScript sets it when swapOutTimer expires; Players_ProcessSlotSwap clears it
 unsigned char freezeTimerA; // +0x0E6 T: EfType6 op 2 arg0; Actor_TickFreezeTimers counts it down
 unsigned char freezeTimerB; // +0x0E7 T: EfType6 op 2 arg1; checked by the hit detection
 unsigned char koFlag; // +0x0E8 T: set at KO by Battle_ResolveHit (EfType6 op 5 writes it); camera and IF 100 test it
 unsigned char cameraExcludeFlag; // +0x0E9 T (read-only): Camera_UpdateFollowActiveCharacters / Camera_UpdateFocusXFromFighters / Camera_UpdateFocusYZFromFighters skip fighters with 1; never written
 unsigned char attackHitsLeft; // +0x0EA T: hits the current attack may still land (AS.attackHitCount loaded by ObjEnterCurrentAction)
 unsigned char guardState; // +0x0EB T: non-zero while guarding (Battle_ResolveHit sets 1)
 unsigned char cpuGuardRequest; // +0x0EC T: CPU_RunAiForSlot (random guard decision by CPU_RollGuardReaction); Battle_ResolveHit
 unsigned char guardInputWindow; // +0x0ED T: MB-specific: 8-frame window after Character_DetectNearbyEnemyAttack during which pattern 17/18 (guard) keeps being entered by direction input (Character_ProcessPlayerInput)
 unsigned char recoverMode; // +0x0EE T: wake-up/recovery selector set by Battle_ResolveHit and Actor_WallBounceReaction
 unsigned char airTechLatch; // +0x0EF T: 0/1 latch in CPU_TryEvadeOrTechReaction / Character_ProcessTechInput: set to 1 when Actor_CanAirTech allows an air tech, tested before starting the tech; ObjRunActionScript clears it
 unsigned char bounceCount; // +0x0F0 T: wall/ground bounces taken
 unsigned char recoveryTimer; // +0x0F1 T: frames since the last hit
 unsigned char throwTechSeen; // +0x0F2 T: set on the grabber when a throw tech succeeds; ComboRecord_RegisterHit flags the combo
 unsigned char stageEdgeSide; // +0x0F3 T: 1/2 = pushed past the right/left stage edge (Actor_ClampToStageEdge)
 unsigned char airComboStarterLatch; // +0x0F4 T: set when an AT.flagsB 0x10 hit lands, cleared by ObjRunActionScript
 unsigned char jumpCancelUsed; // +0x0F5 T: MB-specific: set by Character_ProcessPlayerInput after the jump-cancel patterns 35/36/37 started, cleared by Fighter_BankPendingHeatGain
 unsigned char throwTechAllowed; // +0x0F6 T: Battle_ResolveHit sets !(AT.flagsB & 0x20); Actor_TryStartThrowTech requires it
 unsigned char inputBlockFlag; // +0x0F7 T: Fighter_CanReceiveInput refuses input while it is 1; set by Character_ProcessTechInput, cleared by ObjRunActionScript
 unsigned char counterHitSide; // +0x0F8 T (write-only here): 1 = this actor landed a counter-hit, 2 = was counter-hit
 unsigned char unused_F9; // +0x0F9 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 scriptTimer[5]; // +0x0FA T: five countdown words (Actor_TickWordTimers); EfType6 op 10 sets [arg0]; [0] doubles as the Battle_ResolveHit "launcher hit seen" lock
 unsigned char hitQueueClear; // +0x104 T: hit detection sets 1 to discard last tick hit queue; Battle_ResolveHit zeroes hitQueueCount
 unsigned char shakeTimer; // +0x105 T: draw shake after a hit (Character_Draw offsets X by shakeTimer >> 2 on odd ticks); counted down by Actor_TickFreezeTimers
 unsigned char hitQueueCount; // +0x106 T: number of valid hitAt/hitRect/hitAttacker entries, max 8
 unsigned char tintMode; // +0x107 T: AT.statusEffect 1..4 (Character_ApplyAddedEffect); Character_Draw / Actor_DrawHeatAuraAndTintParticles
 unsigned char tintTimer; // +0x108 T: MB-specific: 120 when a tint effect starts, counted down by Actor_TickWordTimers; Character_Draw tints while non-zero
 unsigned char unused_109; // +0x109 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 statusEffectParam; // +0x10A T: MB-specific: AT.statusEffectParam stored by effect mode 4, counted down by Actor_TickWordTimers
 unsigned char hitStage; // +0x10C T: 0 = not in hit-stun; 1..239 = remaining stun frames; 240..255 = launch/air stages dispatched by Actor_ApplyFrameMotionFlags (byte in MB, word in GOF)
 unsigned char juggleState; // +0x10D T: non-zero while the victim is in an air juggle (set by Actor_StartKnockback; cleared on landing)
 unsigned char hitReactionId; // +0x10E T: reaction index passed to Actor_StartKnockback (g_HitVectorTable row); 27 = wall bounce
 unsigned char hitStageAtHit; // +0x10F T (write-only here): copy of hitStage taken by Actor_StartKnockback / Battle_ResolveHit
 unsigned char unused_110; // +0x110 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned char invulnFlags; // +0x111 T: non-zero = takes no damage (Battle_ResolveHit); EfType6 op 8 sets 0x10
 unsigned char introFinished; // +0x112 T: 1 once the intro is over; set by EF op 254 and by Battle_InitRoundCharactersAndTeams when there is no intro, waited on by Round_IntroStateMachine
 unsigned char koMoveType; // +0x113 T: (attacker move type & 0x7F) + 1 when the actor is KO by it; non-zero blocks pushbox resolution
 unsigned char inputHeldFlags; // +0x114 T: low 7 bits = air jumps used (compared with ct.maxAirJumps), bit 7 = direction neutral seen since the last jump (Character_ProcessPlayerInput)
 unsigned char writeOnlyZero_115; // +0x115 T (write-only): ObjRunActionScript clears it
 unsigned char guardRecoveryFlag; // +0x116 T: 1 after a guard-cancel / early guard; read by Actor_CountStunMashAndCheckGuardRecovery and Fighter_ScanCommandMoves
 unsigned char unused_117; // +0x117 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 mashCounter; // +0x118 T: counts button presses while in hit-stun (Actor_CountStunMashAndCheckGuardRecovery)
 signed char tobiCount[10]; // +0x11A T: ten projectile / counter bytes ("TOBI" counters): EfType6 op 3 adds / subtracts, IF 24/25 and command counterRequirement test them
 unsigned char dashCount; // +0x124 T: EfType6 op 1/2 add/subtract; command usageLimit compares it; set to 100 by the tech recovery
 unsigned char unused_125; // +0x125 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 __int16 paramVar[10]; // +0x126 T: ten word variables: EfType6 op 105 sets/adds [arg0]; IF 25/29/31 compare them
 unsigned char trailMode; // +0x13A T: ghost trail colour mode + 1 (EfType6 op 0); EffectObject_Draw switches on it
 unsigned char unused_13B; // +0x13B U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 decorLifeTotal; // +0x13C T: ghost object total life (EffectObject_SpawnGhostCopy a5; EffectObject_Draw fades alpha by frameTicks / this)
 unsigned __int16 decorFlags; // +0x13E T: ghost object flags (bit 0: depth + 10)
 __int16 decorZoomGrowX; // +0x140 T: ghost zoom change per remaining tick X
 __int16 decorZoomGrowY; // +0x142 T: ghost zoom change per remaining tick Y
 unsigned char afterimageMode; // +0x144 T: afterimage config id + 1 (EfType6 op 3); AfterImage_* switch on it, 0 = off
 unsigned char afterimageSteps; // +0x145 T: number of afterimage steps <= 119
 unsigned char afterimageSpacing; // +0x146 T: spacing of the steps
 unsigned char afterimageRestart; // +0x147 T: EfType6 op 3 sets 1 when the mode changes; raw read/write in Characters_DrawAll
 unsigned char justGuardActive; // +0x148 T: set when a just-guard absorbs a hit; Battle_ResolveHit clears it, Actor_StartKnockback reads it
 unsigned char unused_149; // +0x149 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned __int16 locomotionUsedMask; // +0x14A T: bit n = pattern n (1..9 walk/run/dash/jump) used since the last landing (ObjRunActionScript)
 unsigned char commandUsedMask[14]; // +0x14C T: bit i = command move i already used in this chain (ObjRunActionScript sets it, Fighter_TryStartCommandMove refuses repeats)
 unsigned char pendingCommandId; // +0x15A T: command move accepted by Fighter_TryStartCommandMove this tick (0xFF = none)
 unsigned char startedByCommandId; // +0x15B T: command id that requested the action just entered; read by Fighter_TryStartCommandMove (moveClass compared with MBMOVE_SUPER)
 __int16 hitsTakenInCombo; // +0x15C T: consecutive hits taken (Battle_ResolveHit increments, ObjRunActionScript indexes the recovery table)
 unsigned __int16 attackHitsConnected; // +0x15E T: hits landed by the current action (hit detection increments; IF 17 compares with param[1])
 unsigned char evadeDir; // +0x160 T: 1..4 double-tap evade detected by Fighter_DetectEvadeDoubleTap; Battle_ResolveHit picks the evade entry
 signed char evadeTimer; // +0x161 T: evade window counter (signed; negative = cooldown)
 unsigned __int16 swapOutTimer; // +0x162 T: MB-specific: Players_ProcessSlotSwap sets 1 on the character leaving the field; ObjRunActionScript counts it 1..61 and then releases (sets inputLock, clears launcherState); pushbox resolution and EffectObjects_UpdateAll skip actors with a non-zero value; 4095 = held
 unsigned char flashMode; // +0x164 T: colour flash mode (Actor_SetColorFlash); Character_Draw / EffectObject_Draw switch
 unsigned char flashTimer; // +0x165 T: remaining flash frames (Actor_TickFreezeTimers)
 unsigned char flashDuration; // +0x166 T: total flash frames, denominator of the fade modes
 unsigned char requestParam; // +0x167 T (write-only): argument of ObjRequestAction, cleared by ObjRunActionScript
 unsigned char counterWindowState; // +0x168 T: counter-hit window set by frame flags2 bits 20..23 (ObjEnterCurrentAction); Battle_ResolveHit reads it
 unsigned char unused_169; // +0x169 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 __int16 launchVelX; // +0x16A T: stage-2 launch velocity X loaded when hitStage advances (Actor_StartKnockback fills it from g_HitVectorTable.launchVelX)
 __int16 launchVelY; // +0x16C T: stage-2 launch velocity Y
 __int16 launchAccelX; // +0x16E T: stage-2 acceleration X
 __int16 launchAccelY; // +0x170 T: stage-2 acceleration Y
 unsigned char unused_172[2]; // +0x172 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 struct MbAtRecord * hitAt[8]; // +0x174 T: AT record of each hit received this tick (hit detection writes, Battle_ResolveHit reads)
 struct MbRectI32 hitRect[8]; // +0x194 T: world rectangle of each overlap
 struct MbActor * hitAttacker[8]; // +0x214 T: attacking actor of each hit
 struct MbActor * lastAttacker; // +0x234 T: attacker of the last accepted hit (Battle_ResolveHit); Actor_StartKnockback applies the push to it
 struct MbActor * grabbedBy; // +0x238 T: grabber while grabState != 0 (Actor_TryGrabFighter)
 struct MbActor * ownerFighter; // +0x23C T: owning fighter of a child object (InitChildObject); homing target; ghost owner
 struct MbActor * linkedActor; // +0x240 T: parent actor of a child / grab partner (Character_Draw reads its frame while grabbed)
 unsigned char inputDir; // +0x244 T: numpad direction facing-corrected (ProcessPlayerInput, CPU_ApplyCpfStepInput; Actor_MirrorInputDirection)
 unsigned char unused_245[3]; // +0x245 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 unsigned int inputButtons; // +0x248 T: buttons held (bits 0..5) and pressed this tick (bits 12..17)
 unsigned int inputButtonsReleased; // +0x24C T: buttons released this tick (bits 0..5); input ring B ORs it in (CommandBuffer_PushInput)
 unsigned char teamIndex; // +0x250 T: side/team 0 or 1; children copy it
 unsigned char endConditionMet; // +0x251 T: set by IF 2 END_CONDITION; the object is removed on the next EffectObjects_UpdateAll
 unsigned __int16 pendingPattern; // +0x252 T: pattern requested by ObjRequestAction, 0xFFFF = none
 __int16 pendingPatternPriority; // +0x254 T: priority of the pending request, -1 = none; a request wins when higher
 unsigned __int16 pendingFrame; // +0x256 T: frame requested by IF jumps (Actor_JumpToFrame / ObjEnterCurrentAction), 0xFFFF = none
 __int16 pendingFramePriority; // +0x258 T: priority of the pending frame jump
 unsigned char facingLeft; // +0x25A T: 0 faces right, 1 faces left
 unsigned char nextFacingLeft; // +0x25B T: facing to turn to when the action changes
 unsigned char writeOnlyFF_25C; // +0x25C T (write-only): Slot_InitPointCharacterForRound writes 0xFF
 unsigned char mirrorHistoryPending; // +0x25D T: Fighter_MatchCommandSequence / CommandBuffer_PushInput: when the facing changed and it is 1, the recorded directions are mirrored once
 unsigned char inputActionLatch; // +0x25E T: set 1 by IF 6/7 input jumps with flag 4; ObjRunActionScript steps it 1 -> 2 -> 0
 unsigned char actionChangedFlag; // +0x25F T: Actor_OnActionChanged sets 1, Actor_TickWordTimers clears
 unsigned char landed; // +0x260 T: 1 when posY reached the ground (ObjIntegrateMotion): drives aniFlag 3/4 landing jumps
 unsigned char actionEnded; // +0x261 T (write-only): ObjRunActionScript sets it when a landing/animation end finished the action
 unsigned char stanceCopy; // +0x262 T: AS.stance of the previous tick (ObjRunActionScript)
 unsigned char unused_263; // +0x263 U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
 struct MbBox * boxTable; // +0x264 T: pattern + boxTableOffset or 0 (Actor_ResolveFrameDataPointers)
 struct MbCharDatHead * charData; // +0x268 T: character pattern block (Slot_BindCharacterData from g_CharPatternData)
 struct MbPatternHeader * patternHeader; // +0x26C T: current pattern header
 struct MbFrameRecord * frameRecord; // +0x270 T: current 116-byte frame record
 struct MbAnimFrame * animFrame; // +0x274 T: same address as frameRecord, the AF half
 struct MbStateFrame * stateFrame; // +0x278 T: frameRecord + 40, the AS half
 struct MbIfRecord * ifTable; // +0x27C T: pattern + ifTableOffset or 0
 struct MbEfRecord * efTable; // +0x280 T: pattern + efTableOffset or 0
 __int16 * boxIndex; // +0x284 T: frameRecord + 96, the 10 box indices (slots 8 and 9 are two attack boxes in MB)
 void * partsData; // +0x288 T: character parts blob (Slot_BindCharacterData from g_CharPartsData)
 void * cgData; // +0x28C T: character CG blob (Slot_BindCharacterData from g_CharCgData)
 struct MbFighterSlot * fighterSlot; // +0x290 T: pointer to the owning fighter slot (Slot_InitPointCharacterForRound stores the slot itself); 0 for child objects
 struct MbActor * teamPartnerActor; // +0x294 T: the other member of the team (Players_ProcessSlotSwap sets it on the entering character to the leaving one); MaidsPartner_UpdateReserveAI mirrors life/meter/gauges from it
 int actionTicks; // +0x298 T: ticks since the action started (ObjRunActionScript increments, Actor_OnActionChanged clears)
 unsigned char ranThisTick; // +0x29C T: ObjRunActionScript sets 1 once it passed all gates, 0 at its start
 unsigned char unused_29D[3]; // +0x29D U: no member access in any of the typed functions and no raw access in the slot/pool iterator functions
};
struct MbFighterSlot {
 unsigned char active; // +0x000 T: 1 = slot in use (Slot_BindCharacterData / Slot_InitPointCharacterForRound set 1, FighterSlot_ResetKeepingDataPointers / Battle_LoadAllCharacterSlots clear)
 unsigned char unused_01[3]; // +0x001 U: no access
 struct MbActor actor; // +0x004 T: the actor (actor = slot + 4; memset 0x2A0 by Slot_InitPointCharacterForRound)
 struct MbCtHeader ct; // +0x2A4 T: CT header copied by Slot_LoadCommandCT (slot + 676)
 unsigned int commandCount; // +0x300 T: CT count dword (slot + 768)
 struct MbCtCommand command[100]; // +0x304 T: command table (slot + 772)
 struct MbInputRing ringA; // +0x1434 T: input history A (CommandBuffer_PushInput)
 struct MbInputRing ringB; // +0x1634 T: input history B with releases
};
struct MbObjectSlot {
 unsigned char active; // +0x000 T: 1 = slot in use (InitChildObject)
 unsigned char unused_01[3]; // +0x001 U: no access
 struct MbActor actor; // +0x004 T: the actor (InitChildObject memset(slot + 4, 0, 0x2A0))
};
#pragma pack(pop)
