// Melty Blood Re-ACT (mbr.exe, 2004, base 0x400000) data-file + runtime types, IDA-parsable (idc.parse_decls).
// IDB: C:\dev\ida\server\mbr.exe.i64. Everything here is applied in the IDB.
// Evidence tags in comments: T = traced in code (function names are IDB names), D = verified on the shipped files
// of C:\games\MB\R\01.p, U = UNPROVEN (guess), X = proved never read by the game.
// All offsets in this file are STRUCT-RELATIVE. The runtime Actor starts at slot+4 (see MbrCharacterSlot).

struct MbrActor;
struct MbrCharacterSlot;

// ============================================================================================
// 1. .p archive (00.p..06.p, 10.p; data00..03.p of the original Melty Blood use the same format)
// ============================================================================================
struct MbrPkEntry {                      // 68 bytes, file index entry i (cipher below)
 char name[60];                          // +0x00 T D: bytes 0..58 are stored as (name[j] ^ ((3*j*i + 61) & 0xFF)); byte 59 is never decoded
 unsigned int dataOffset;                // +0x3C T D: absolute file offset of the payload (stored plain)
 unsigned int sizeXorKey;                // +0x40 T D: payload size ^ 0xE3DF59AC
};
struct MbrPkArchive {                    // 28 bytes, runtime handle (PKArchive_Open 0x42C0D0); 15 slots at g_PkArchiveTable
 void *hFile;                            // +0x00 T: CreateFileA handle of the .p
 void *hMapping;                         // +0x04 T: file mapping handle (only when opened mapped), 0 otherwise
 void *mappedView;                       // +0x08 T: MapViewOfFile view (only when mapped)
 unsigned int cipherMode;                // +0x0C T D: first u32 of the file: 0 = payload head enciphered with the entry name, 1 = plain, 2 = XOR 0xCA
 unsigned int entryCount;                // +0x10 T D: second u32 of the file ^ 0xE3DF59AC
 unsigned int unused_14;                 // +0x14 X: no function reads or writes it (PKArchive_Open/Close/ReadEntry/FindEntryIndex/GetEntrySize/GetEntryOffset are the only users of the struct)
 struct MbrPkEntry *entries;             // +0x18 T: GlobalAlloc(68 * entryCount) decoded index
};

// ============================================================================================
// 2. <CHAR>.WMT win-quote table (WinQuote_LoadWmt 0x4120E0)
// ============================================================================================
struct MbrWmtRecord {                    // 262 bytes
 unsigned char opponentCharId;           // +0x00 T D: quote is chosen when the LOSING character's id (g_CharTable index) equals this; 0xFF = any opponent (WinQuote_PickForOpponent)
 unsigned char unused_01;                // +0x01 X D: never read (only the whole-table qmemcpy touches it); 0 in all 344 shipped records
 unsigned short chanceDivisor;           // +0x02 T D: 0 = always accepted, n = accepted with probability 1/n (rand() % n == 0); shipped values 0,1,3,10
 unsigned short portraitVariant;         // +0x04 T D: picks grp\win\win_<name><NN>_<colour>.bmp (NN = this, falls back to 00)
 char text[256];                         // +0x06 T D: Shift-JIS, NUL terminated, CRLF line breaks, NUL padded; drawn by the win screen with the 262*index+6 pointer
};
struct MbrWmtFile {                      // file = 4 + 262 * count bytes (verified for all 24 shipped files); the game loads at most 50 records
 unsigned int count;                     // +0x00 T D
 struct MbrWmtRecord records[50];        // +0x04 (declared with the loader maximum; the real array has `count` entries)
};

// ============================================================================================
// 3. <CHAR>.CPF CPU script file (193388 bytes; CPU_LoadCpfFile 0x406DF0 reads the whole entry into g_CpfData[slot])
// ============================================================================================
struct MbrCpfStep {                      // 44 bytes; 40 steps per script (CPU_ApplyCpfStepInput: base + 44*(43*script + step) + 188)
 unsigned char inputCode;                // +0x00 T: 0/0xF neutral, 1=6 2=4 3=2 4=8 5=9 6=7 (numpad dir held), 7..0xA = button 0x1000/0x2000/0x4000/0x8000 held, 0xB..0xE = 2 + that button, 0x10 = perform command move number step.moveId
 unsigned char unused_01;                // +0x01 U: no reader found among the CPU_* functions
 short duration;                         // +0x02 T: frames the step is held (copied to the per-slot counter by CPU_StartCpfActionStep)
 short durationRandom;                   // +0x04 T: if > 0 rand() % value is added to duration
 short moveId;                           // +0x06 T: CommandMove index passed to CommandMove_TryExecute when inputCode == 0x10
 unsigned char reserved_08[16];          // +0x08 U: not read by any CPU_* function (all zero in the shipped files)
 unsigned int flags;                     // +0x18 T D: bit0 | bit1 = the script ends after this step (index reset to 0); bit2 (0x4) = advance early as soon as actor.attackConnectedFlag is set and hitResultFlags is non-zero; bit3 (0x8) = with bit2: advance only when the attack HIT (hitResultFlags bit 0x2), not when guarded; bit4 (0x10) = OR the button mask of inputCode into the CPU button state while stepping; bit31 = set actor.cpuCommandRequestLevel from holdLevel when the step starts (CPU_StartCpfActionStep)
 unsigned char reserved_1C[4];           // +0x1C U: not read
 unsigned char dirCode;                  // +0x20 T: direction (same coding as inputCode 1..8) read by CPU_CpfDirCodeToNumpad
 unsigned char holdLevel;                // +0x21 T: BYTE1 of the dword at +0x20; (value + 1) clamped to 1..3 is written to actor.cpuHoldLevel when flags bit31 is set
 unsigned char reserved_22[10];          // +0x22 U: not read
};
struct MbrCpfScriptHead {                // 132 bytes, stored AFTER the 40 steps of the same script record (file offset 188 + 1892*i + 1760)
 int rangeMin;                           // +0x00 T: minimum horizontal distance (pixels) to the nearest opponent (CPU_FindNearestOpponentInRange arg a2)
 int rangeMax;                           // +0x04 T: maximum distance; 0 and rangeMin == 0 means 10000
 int useYRange;                          // +0x08 T: non-zero = test the vertical window yMin..yMax
 int yMin;                               // +0x0C T
 int yMax;                               // +0x10 T
 int chancePercent;                      // +0x14 T: probability weight; also the percent compared with rand()%100 after selection
 int reserved_18[2];                     // +0x18 U: not read by CPU_ScoreAndSelectAction
 int selfStanceCond;                     // +0x20 T: 0 any, 1..4 = own stance (AS.stanceState) condition (see CPU_ScoreAndSelectAction)
 int targetStanceCond;                   // +0x24 T: same for the opponent
 int targetActionCond;                   // +0x28 T: 0 none, 1 opponent must be in state 1, 2 not in state 1 (actor+29), ...
 int needEnemyAttackNearby;              // +0x2C T: non-zero = Character_DetectNearbyEnemyAttack must be true
 int requirePatternCond;                 // +0x30 T: non-zero = own pattern must equal requiredPattern
 int requiredPattern;                    // +0x34 T
 int requiredFrame;                      // +0x38 T: 255 = any frame
 int selfHealthCond;                     // +0x3C T: 0 any, 1..4 = own health <= 5000/2500/1250/625, 5 = full (10000)
 int reserved_40;                        // +0x40 U
 int targetHitCond;                      // +0x44 T: non-zero = opponent not in hit state (actor+292 / +254 checks)
 int interruptClass;                    // +0x48 T D: 1 = this script may be interrupted by a new selection while one of its steps runs (and is never chosen as an interrupt); 2 = not interruptible (CPU_ScoreAndSelectAction)
 int cooldownFrames;                     // +0x4C T: frames this script stays unavailable after use (written to g_CpuSlotState[slot].scriptCooldown)
 int minDifficulty;                      // +0x50 T: script is skipped while g_Opt_CpuDifficulty < this
 int heatModeCond;                       // +0x54 T: 0 any, 1..4 = actor.heatMode == value-1, 5 = != 3
 int reserved_58[3];                     // +0x58 U
 int priority;                           // +0x64 T: selection tier (highest priority among passing scripts wins ties)
 int reserved_68[7];                     // +0x68 U
};
struct MbrCpfScript {                    // 1892 bytes = 40 steps + head
 struct MbrCpfStep steps[40];            // +0x000
 struct MbrCpfScriptHead head;           // +0x6E0
};
struct MbrCpfFile {                      // 193388 bytes
 int baseSkill;                          // +0x00 T D: CPU skill base (0..100), read as g_CpfData[0] by CPU_DrawDebugOverlay / CPU_SkillPercentFromBase (25 AKIHA, 30 most, 0 for DEKU/JUMP/GUARD/CROUCH helpers)
 unsigned char reserved_04[184];         // +0x04 D: zero in every shipped file; never read
 struct MbrCpfScript scripts[100];       // +0xBC (188): script 0 = idle/neutral script, 1..99 selectable
 unsigned char tail[4000];               // +0x2E3CC: never read by the game (stale editor data in the shipped files)
};

// ============================================================================================
// 4. <CHAR>.DAT / EFFECT.DAT character container ('Hantei4', same container as Act Cadenza)
//    Loader: CharDat_LoadAndSplitBlobs 0x436940 splits the file into three GlobalAlloc blobs:
//      blob A (g_CharGfxDataPtrsA[slot]) = file[0 .. hdr.dataBlobSize)            = header + pattern table + pattern data
//      blob B (g_CharGfxDataPtrsB[slot]) = file[hdr.dataBlobSize .. +hdr.partsBlobSize) (+0x4000 slack), "parts" blob, absent when 0
//      blob C (g_CharGfxDataPtrsC[slot]) = file[hdr.cgBlobOffset .. +hdr.cgBlobSize)    (+0x4000 slack), the 'BMP Cutter3' CG
//    (if dataBlobSize is 0 the loader uses cgBlobOffset as the size of blob A). The 256 x 64-byte pattern name table that
//    follows the CG blob in the file is never read by the game.
// ============================================================================================
struct MbrDatHeader {                    // 0x444 bytes
 char magic[8];                          // +0x00 X D: "Hantei4\0", never compared by the game
 unsigned int unused_08[2];              // +0x08 X D: zero in all shipped files, never read
 unsigned int unused_10;                 // +0x10 X D: 1 in all shipped files, never read
 unsigned int dataBlobSize;              // +0x14 T D: size of blob A (patterns); the Hantei 'partsOff'
 unsigned int partsBlobSize;             // +0x18 T D: size of blob B (0 for AKIHA/LEN, 0x232638 for GAKIHA)
 unsigned int cgBlobOffset;              // +0x1C T D: file offset of blob C
 unsigned int cgBlobSize;                // +0x20 T D: size of blob C
 unsigned char unused_24[0x20];          // +0x24 X D: zero in all shipped files, never read
 int patternOffset[256];                 // +0x44 T D: offset (from blob A start) of the pattern header; -1 = pattern absent
};
struct MbrPatternHeader {                // 0x44 bytes, followed by frameCount * 216-byte MbrFrame records
 int frameCount;                         // +0x00 T D
 unsigned int flags;                     // +0x04 T: low nibble = pattern state (editor 'psts'); bit 0x40 / 0x80 tested by Pattern_HasHeaderFlag40/80
 int level;                              // +0x08 T: Pattern_GetLevelField
 int boxTableOffset;                     // +0x0C T: from the pattern header; 8-byte MbrBox table (-1 none); copied to actor.boxTable
 int atTableOffset;                      // +0x10 T: 88-byte MbrAt table (-1 none); read by the hit code
 int ifTableOffset;                      // +0x14 T: 52-byte MbrIf table (-1 none); copied to actor.ifTable
 int efTableOffset;                      // +0x18 T: 52-byte MbrEf table (-1 none); copied to actor.efTable
 unsigned char unused_1C[0x28];          // +0x1C X D: writer-stack garbage in the shipped files (e.g. 0x12F7FC), never read
};
struct MbrBox { short x0; short y0; short x1; short y1; };           // 8 bytes (frame idx 24..56 select entries)

// ---- frame record: 216 bytes = AF 44 + AS 56 + int16 idx[58]. Field names follow Hantei_Docs/update_2026_09/HA4_SECTION.md;
//      the game-side consumers are listed per field (T). "reserved" bytes were 0 in every frame of AKIHA/LEN/GAKIHA.DAT (D) and no
//      reader was found (X only where stated).
struct MbrAF {                           // 44 bytes: animation / draw part of a frame
 short spriteId;                         // +0x00 T: < 10000 = parts blob (B) entry, >= 10000 = CG image (id - 10000) in blob C, -1 = none
 short offsetX;                          // +0x02 T: draw offset
 short offsetY;                          // +0x04 T
 short duration;                         // +0x06 T: ticks the frame lasts (Character_UpdateMainLoop)
 unsigned char flipMode;                 // +0x08 T: 0 normal 1 flipH 2 flipV 3 rot90 4 rot180 5 rot270 6 H+90 7 H+270 8 arbitrary 9 arbitrary mirrored (SpriteFlipMode_GetRenderCode)
 unsigned char blendMode;                // +0x09 T: 0 opaque, 1 alpha, 2 additive, 3 subtractive
 unsigned char alpha;                    // +0x0A T: 0..255, used only when blendMode != 0
 unsigned char aniType;                  // +0x0B T: 0 end (jump = pattern), 1 next, 2 jump to frame, 3 next + end on landing, 4 jump + end on landing, 5 loop check
 unsigned char jump;                     // +0x0C T: target frame (or pattern for aniType 0)
 unsigned char landJump;                 // +0x0D T: frame (aniType 1/2/5) or pattern (aniType 3/4) used on landing
 unsigned char drawPriority;             // +0x0E T: 0 unchanged, 1 front, 2 back, 3..12 front 0..9, 13..22 back 0..9, 23/24 owner +-1, 25 before background (SetCharacterDrawPriority)
 unsigned char reserved_0F;              // +0x0F D: pad
 short zoomX;                            // +0x10 T: 256 = 1.0, 0 = 1.0
 short zoomY;                            // +0x12 T
 unsigned char interpolationType;        // +0x14 T: non-zero tweens toward the next frame
 unsigned char loopEnd;                  // +0x15 T: frame to continue with when the loop counter runs out (aniType 5)
 unsigned char loopCount;                // +0x16 T: copied to actor.loopCounter on frame entry
 unsigned char reserved_17[13];          // +0x17 D: pad
 short rotation;                         // +0x24 T: 1/10000 turn, flip modes 8/9
 unsigned char reserved_26[6];           // +0x26 D: pad
};
struct MbrAS {                           // 56 bytes: motion / state part of a frame
 unsigned char clearX;                   // +0x00 T: zero velX / accelX / maxSpeedX (Actor_ApplyFrameMovementAndHitVector)
 unsigned char clearY;                   // +0x01 T: zero velY / accelY
 unsigned char addX;                     // +0x02 T: velX += facing * speedX, accelX = facing * accelX, maxSpeedX = facing * maxSpeedX
 unsigned char addY;                     // +0x03 T: velY += speedY, accelY = accelY
 unsigned char reserved_04[4];           // +0x04 D: pad
 short speedX;                           // +0x08 T
 short speedY;                           // +0x0A T
 unsigned char reserved_0C[4];           // +0x0C D: pad
 short accelX;                           // +0x10 T
 short accelY;                           // +0x12 T
 unsigned char reserved_14[4];           // +0x14 D: pad
 unsigned char stance;                   // +0x18 T: 0 ground stand, 1 air, 2 crouch
 unsigned char cancelNormal;             // +0x19 T: 0 never, 1 on contact, 2 always, 3 on hit only (CommandMove_CheckRequirements)
 unsigned char cancelSpecial;            // +0x1A T
 unsigned char reserved_1B;              // +0x1B D: pad
 unsigned char hitsNumber;               // +0x1C T: hits the attack may land; copied to actor.hitsRemaining
 unsigned char canMove;                  // +0x1D T: 1 = actor is free to act (resets chain state, ends hit-stun)
 unsigned char reserved_1E[6];           // +0x1E D: pad
 unsigned int statusFlags0;              // +0x24 T: b0 carry velocity into the next pattern, b1 clear carry, b2 no walk, b4 ground tech ok, b5 air tech ok, b31 skip movement on this frame
 unsigned int statusFlags1;              // +0x28 T: b0 EX cancel, b1 throw escapable (CommandMove_CheckRequirements / Throw_TryThrowEscape), b2 jump cancel only, b16..19 invulnerability enum (inert in this build, Hitbox_InvincibilityCheckStub), b20..23 counter type -> actor.counterHitState, b31 no guard
 unsigned int reserved_2C;               // +0x2C D: always 0
 short maxSpeedX;                        // +0x30 T
 unsigned char reserved_32[6];           // +0x32 D: pad
};
struct MbrFrame {                        // 216 bytes
 struct MbrAF af;                        // +0x00
 struct MbrAS as;                        // +0x2C
 short idx[58];                          // +0x64 T: [0] AT record index, [8..15] IF record indices, [16..23] EF record indices (frame+0x74 / +0x84 read by Character_RunFrameIFs/EFs), [24..56] box slots (idx[24] = pushbox, 25..32 hurtboxes, 35 clash box, 49..56 attack boxes), -1 = none; [1..7] and [57] are writer garbage
};
struct MbrAt {                           // 88 bytes attack record (pattern.atTableOffset + 88 * idx[0])
 unsigned int guardFlags;                // +0x00 T: b0 stand guardable, b1 air guardable, b2 crouch guardable, b8 cannot hit standing, b9 cannot hit air, b10 cannot hit crouching, b11 cannot hit a victim in hit-stun, b12 cannot hit a blocking victim, b13 cannot hit deep juggle (Hitbox_AttackCanHitStance)
 unsigned char reserved_04[4];           // +0x04 D: pad
 short hitVector[3];                     // +0x08 T: stand, air, crouch: low byte = g_HitVectorTable id, 0x100 = reverse X, 0x200 = no X push
 unsigned char reserved_0E[10];          // +0x0E D: pad
 short guardVector[3];                   // +0x18 T: stand, air, crouch guard vectors
 unsigned char reserved_1E[10];          // +0x1E D: pad
 short hitEffect;                        // +0x28 T: g_HitEffectPresetTable row (>= 14 = no spark)
 short soundEffect;                      // +0x2A T
 unsigned char reserved_2C[4];           // +0x2C D: pad
 short hitStopOverride;                  // +0x30 X: read by no function
 short untechTime;                       // +0x32 T: -> actor.untechTimeBudget
 unsigned char reserved_34[4];           // +0x34 D: pad
 unsigned char addedEffect;              // +0x38 T: 0 none, 1 burn, 2 freeze, 3 shock, 4 confuse (Character_ApplyAddedEffect)
 unsigned char hitgrab;                  // +0x39 T: non-zero = throw (Actor_AttachThrowVictim)
 unsigned char hitStopLevel;             // +0x3A T: index into g_HitStopFramesByLevel
 unsigned char correction;               // +0x3B T: proration value
 unsigned char correctionType;           // +0x3C T: 0 keep lower, 1 multiply, 2 subtract (Combo_ApplyProration)
 unsigned char reserved_3D[3];           // +0x3D D: pad
 unsigned int otherFlags;                // +0x40 T: 0x01 chip damage, 0x02 non lethal, 0x04 no follow-up (sets juggleLockState 10), 0x10 air combo starter, 0x20 no combo count / no clash, 0x40 screen shake, 0x80 no air tech, 0x100 no ground tech, 0x200 hits own side, 0x400 attacker gets no hit-stop
 unsigned char reserved_44[4];           // +0x44 D: pad
 short damage;                           // +0x48 T
 short meterGain;                        // +0x4A T
 short guardReduction;                   // +0x4C T: red-health damage (Damage_CalcChipDamage for guarded hits)
 short stunValue;                        // +0x4E T: only passed to Actor_ApplyStunValueStub (no effect)
 short breakTime;                        // +0x50 X: read by no function
 unsigned char reserved_52[6];           // +0x52 D: pad
};
struct MbrIf {                           // 52 bytes condition / interrupt record, interpreted by IF_ProcessCondition (see docs/formats/mbr.md section 8)
 short type;                             // +0x00 T
 unsigned char reserved_02[2];           // +0x02 D: pad
 int params[9];                          // +0x04 T: p0..p8
 unsigned char reserved_28[12];          // +0x28 D: pad
};
struct MbrEf {                           // 52 bytes effect record, interpreted by Character_RunFrameEFs (see docs/formats/mbr.md section 9)
 short type;                             // +0x00 T
 short number;                           // +0x02 T
 int params[8];                          // +0x04 T: p0..p7
 short paramsShort[4];                   // +0x24 T: p8..p11
 unsigned char reserved_2C[8];           // +0x2C D: pad
};
struct MbrIntRect { int left; int top; int right; int bottom; };     // 16 bytes

// ============================================================================================
// 5. Runtime: CharacterSlot / Actor / EffectObject
//    Note on "unused_XX" members of MbrActor: proved by scanning every [reg+disp] operand of the whole binary (ebp-based included for
//    disp >= 0x40) and checking the actor functions in the code ranges 0x406780-0x408000, 0x41A260-0x41CE90, 0x426A50-0x428400,
//    0x42C880-0x433D70, 0x43A9C0-0x440400, 0x446800-0x44B9A0 (all per-frame character/effect/hit/HUD/draw code): operands at the same
//    displacement there address other structs (AF/AS/AT records, combo records, draw records), never the actor.
// ============================================================================================
struct MbrActor {                         // 0x2D0 bytes; the Actor of a player slot sits at slot+4, of an effect object at object+4
 unsigned char slotIndex; // +0x000 T: 0..3 player slot, equals the CharacterSlot index (g_CharacterSlot0_Char); effect objects copy it from their spawner; indexes g_SideActiveSlotIndex comparisons and 150*slot+400 voice banks
 unsigned char characterId; // +0x001 T: g_CharTable index of the character (CharEntry.charId); IF 0x15 / Battle_IsOtherCharacterIdPresent compare it
 unsigned char controlType; // +0x002 T: 0 = human (input ring + Slot_ProcessPlayerInput), 1 = CPU (CPU_UpdateAIForCharacter + Character_ProcessPlayerInput)
 unsigned char unused_03; // +0x003 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char objectKind; // +0x004 T: 0 = playable character, 31 = ghost afterimage copy, 0x8F / 0xFF = effect or projectile; >= 0xF0 is non-character; bit 0x80 = object owned by a character (owner at +0x264)
 unsigned char paletteIndex; // +0x005 T: costume colour chosen at character select (Battle_LoadAllCharacterSlots a4); preserved by Slot_Init*
 unsigned char patternId; // +0x006 T: current pattern (action) id; indexes MbrDatHeader.patternOffset
 unsigned char frameIndex; // +0x007 T: current frame inside the pattern (validated against MbrPatternHeader.frameCount)
 unsigned char tweenTargetFrame; // +0x008 T: UpdateCharacterAnimationFrame writes the frame index that follows the current one (used for tweening between AF records)
 unsigned char unused_09; // +0x009 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 short tweenTargetSpriteId; // +0x00A T: AF.spriteId of the next frame (Pattern_GetFrameFirstDword); drawn blended toward it when AF.interpolationType != 0, -1 for grabbed parts
 unsigned char frameStartState; // +0x00C T: 1 = frame entered this tick, 2 = pattern freshly reset (forces AS motion); cleared each tick
 unsigned char efRunPending; // +0x00D T: Character_RunFrameEFs runs the frame EF records only while non-zero and clears it afterwards; set to 1 on frame entry and by EffectObject_InitFromParent
 unsigned char hitMotionJustStarted; // +0x00E T: 1 = hit-vector knockback just started (Actor_ApplyKnockbackFromHitVector); the next frame setup clears the carried motion; blocks the hit-stun end reset
 unsigned char unused_0F; // +0x00F X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short frameTickCounter; // +0x010 T: ticks spent in the current frame, compared with AF.duration; for ghost copies (objectKind 31) the remaining life
 unsigned char unused_12[2]; // +0x012 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char animationEnded; // +0x014 T: 1 = pattern ended (aniType 0 expired or landed on aniType 3/4); read by IF 2
 unsigned char unused_15; // +0x015 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char loopCounter; // +0x016 T: AF.loopCount copied on frame entry, decremented by aniType 2/4/5 loops; IF types 9/0xA also use it as a scratch byte
 unsigned char unused_17; // +0x017 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short objectFlagsA; // +0x018 T: effect object flags from the EF record (EffectObject_InitFromParent): 0x01 die with owner hit/KO, 0x02 world fixed, 0x04 follow owner displacement, 0x08 freeze with owner hit-stop, 0x20 tied to owner pattern, 0x40 no ground clamp
 unsigned short objectFlagsB; // +0x01A T: effect object flags: 0x01 draw ground shadow, 0x02 slaved to the owner frame stepping, 0x04 die when owner airborne/suspended, 0x08 hit-stop goes to the owner, 0x40 keep running during super freeze, 0x80 (readers not found)
 unsigned char homingMode; // +0x01C T: 0 none, 1 chase, 2 x-only (EF record p4); nonzero stops Actor_IntegrateVelocityAndPosition from snapshotting the previous position
 unsigned char homingBasePattern; // +0x01D T: first pattern of the homing family (EF number); base+1 finish, base+2 hover, base+3 chase
 unsigned char unused_1E[2]; // +0x01E X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short homingElapsed; // +0x020 T: EffectObject_UpdateOwnerFollowHoming counter
 unsigned short homingTimeout; // +0x022 T: EF30 number 0 param; ends the homing when homingElapsed reaches it
 short homingOffsetX; // +0x024 T: target offset X in frame pixels (EF p0, EF30)
 short homingOffsetY; // +0x026 T: target offset Y in frame pixels (EF p1, EF30)
 unsigned short homingWaitTimer; // +0x028 T: wait before chasing, counted down
 short homingParams[10]; // +0x02A T: word table written by EF30 number 1 (a[0x2A + 2*p0] = p1); [0] is the speed of mode 2
 short cpuStepFramesLeft; // +0x03E T: CPU AI frames left in the running step (CPU_StartCpfActionStep loads MbrCpfStep.duration, CPU_AdvanceCpfStep decrements)
 unsigned char unused_40[2]; // +0x040 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 short cpuScriptIndex; // +0x042 T: running CPF script number (CPU_StartCpfActionStep a3)
 short cpuStepIndex; // +0x044 T: step inside the running CPF script
 unsigned char cpuHeatRequest; // +0x046 T: set to 1 by CPU_UpdateAIForCharacter (1/128 chance per frame when heatMode == 2 and health < 3333); consumed by the heat activation code
 unsigned char cpuCommandRequestLevel; // +0x047 T: 1..3 written by CPU_StartCpfActionStep when MbrCpfStep.flags bit31 is set; IF 0xB / 0x1F / 0x23 treat == 1 as the CPU pressing the command and clear it
 int meter; // +0x048 T: meter 0..30000 (HUD percent = meter/10); preserved across rounds; Actor_AddMeter adds, Actor_CanAffordMeter / pending cost spend it; EF06 number 4 clamps
 unsigned char meterGainFlashTimer; // +0x04C U: copied on tag swap; written by Actor_AddMeter when the 30000 bucket changes (SE 0x15)
 unsigned char unused_4D[3]; // +0x04D X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 int health; // +0x050 T: 10000 = full; Actor_ResolveIncomingHits subtracts, KO when < 0
 int redHealth; // +0x054 T: recoverable (red) health, follows health from above
 int displayHealth; // +0x058 T: HUD bar value chasing health (+200 up / -250 down per frame)
 int displayRedHealth; // +0x05C T: HUD bar value chasing redHealth
 short damageFlashTimer; // +0x060 T: set to 30 after damage, counted down by the HUD; (value << 7) / 30 drives the bar flash
 short heatRegenDelay; // +0x062 T: set to 10 / 20 by Actor_AddMeter (kind 1 / 2); Character_UpdateHeatModeRegen waits for it before regenerating
 short heatDurationInit; // +0x064 T: Actor_SetHeatMode stores the initial heat time here (and in heatGauge); HUD draws the heat bar from it
 short heatStartGrace; // +0x066 T: Actor_SetHeatMode (max with old value); frames before regeneration starts
 int heatGauge; // +0x068 T: heat time/gauge 0..100, counted down in heat mode (Character_UpdateHeatModeRegen), EF05 number 4 / EF06 number 4 subtract (skipped when heatMode != 0); preserved across rounds
 int displayHeatGauge; // +0x06C T: HUD value chasing heatGauge by 100 per frame
 int heatReserved70; // +0x070 U: zeroed at init, copied by MaidsPartner_UpdateReserveAI, no reader found
 unsigned char heatMode; // +0x074 T: 0 none, 1 Heat (regen +12/tick), 2 ready / Blood Heat (+4/tick), 3 active big regen (+29/tick, zeroes meter at the end); nonzero makes costs come from heatGauge and blocks meter gain
 unsigned char commandScratch75; // +0x075 U: CommandMove_TryExecute zeroes it
 unsigned char unused_76[4]; // +0x076 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char clearInputHistoryOnAction; // +0x07A T: CommandMove_TryExecute sets it from CommandMove.flags1 bit 0x10; Actor_ResetFrameStateForPatternChange then clears the input history (InputHistory_ClearCommandTimers)
 unsigned char unused_7B; // +0x07B X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 int guardGaugeTarget; // +0x07C U: 5000 at round init (HUD)
 int displayGuardGauge; // +0x080 U: HUD value chasing guardGaugeTarget by 100
 unsigned char unused_84[4]; // +0x084 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 int posX; // +0x088 T: world X, 1/256 pixel units (screen x = (posX - cameraX + 0x8000) >> 8)
 int posY; // +0x08C T: world Y, 0 = ground, negative = up; landing clamps to 0 and sets landedThisTick
 int cameraAuxY; // +0x090 U: averaged by Camera_ComputeTargetY into g_CameraTargetAvgAux
 int posXEndOfIntegrate; // +0x094 U: Actor_IntegrateVelocityAndPosition stores posX here at the end
 int prevPosX; // +0x098 T: posX snapshot before integration (delta propagated to a grabbed target)
 int prevPosY; // +0x09C T: posY snapshot before integration
 int velX; // +0x0A0 T: horizontal velocity from AS (mirrored by facing)
 int velY; // +0x0A4 T: vertical velocity from AS
 short accelX; // +0x0A8 T: AS.accelX * facing sign
 short accelY; // +0x0AA T: AS.accelY
 short maxSpeedX; // +0x0AC T: AS.maxSpeedX * facing sign, clamp for velX (0 = none)
 unsigned char unused_AE[2]; // +0x0AE X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 int knockVelX; // +0x0B0 T: hit-vector knockback velocity X (Actor_ApplyKnockbackFromHitVector), folded into velX once
 int knockVelY; // +0x0B4 T: hit-vector knockback velocity Y
 short knockAccelX; // +0x0B8 T: knockback acceleration X; when it overshoots zero both knock fields are zeroed
 short knockAccelY; // +0x0BA T: knockback acceleration Y
 int carriedSpeedX; // +0x0BC T: momentum carried across a pattern change, added to posX each tick, decays by carryDecay
 int carryCaptureVelX; // +0x0C0 T: velX saved when AS.statusFlags[0] bit 0x01 is set
 int carryPendingVelX; // +0x0C4 U: applied to carriedSpeedX when the pattern equals carryPendingPattern
 short carryDecay; // +0x0C8 T: carriedSpeedX / 10, subtracted per tick
 unsigned short carryCapturePattern; // +0x0CA T: pattern id belonging to carryCaptureVelX
 unsigned short carryPendingPattern; // +0x0CC U: pattern id belonging to carryPendingVelX
 short grabAttachOffsetX; // +0x0CE T: current offset of a grabbed actor, added to AF offset when grabbedState != 0 (EF04 / EF05 write)
 short grabAttachOffsetY; // +0x0D0 T: same, Y
 short grabTweenStartX; // +0x0D2 T: grab draw offset start X (EF05 number 101)
 short grabTweenStartY; // +0x0D4 T: grab draw offset start Y
 short grabOffsetTargetX; // +0x0D6 T: lerp target for grabAttachOffsetX (EF05 number 100)
 short grabOffsetTargetY; // +0x0D8 T: lerp target for grabAttachOffsetY
 short grabTweenEndX; // +0x0DA T: grab draw offset end X
 short grabTweenEndY; // +0x0DC T: grab draw offset end Y
 short grabRotationCur; // +0x0DE T: grab rotation, 1/10000 turn
 short grabRotationTarget; // +0x0E0 T: grab rotation target
 unsigned char clearCarryRequest; // +0x0E2 T: non-zero zeroes carriedSpeedX / carryDecay this frame (also AS.statusFlags[0] bit 0x02)
 unsigned char unused_E3; // +0x0E3 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short hitResultFlags; // +0x0E4 T: result of this actor attack: 0x01 guarded, 0x02 hit, 0x04 armor/clash, 0x08 whiff branch, 0x10 victim was stand/crouch... see Actor_SetHitResultFlags (0x10 victim stance 0 or 2, 0x20 victim stance 1), 0x100 special; also the hit-group counter reset on new AS.hitsNumber; read by IF types 3/6/7/0xB/0x11/0x23/0x26 and the cancel rules
 unsigned short ownerHitResultFlags; // +0x0E6 T: copy of a helper result written into its owner (Actor_SetHitResultFlags); cleared by the throw attach
 unsigned char drawPriority; // +0x0E8 T: render list index (SetCharacterDrawPriority: 0x80 point character, 0x78 partner, 125 back, ghosts 122, effects 110..144)
 unsigned char grabFlipComposeIndex; // +0x0E9 T: column of g_FlipModeComposeTable used while grabbed (EF04: (p2 % 10000) / 1000)
 unsigned short pendingMeterCost; // +0x0EA T: Actor_SetPendingMeterCost; paid by Actor_PayPendingMeterCost (99999 = whole gauge; /100 when heatMode != 0)
 unsigned short meterSpentPenaltyTimer; // +0x0EC T: set to 360 when a pending cost is paid; non-zero reduces meter gain in Actor_ResolveIncomingHits; ticked by Actor_TickWordTimersAndClearFrameFlags
 unsigned char commandFromContactFlag; // +0x0EE T: CommandMove_TryExecute stores the contact bit; EF02 number 50 applies proration when non-zero
 unsigned char jumpCancelState; // +0x0EF T: 0 off, 1 jump cancel granted (AT.otherFlags 0x10 on hit), 2 while pattern 41 runs; Character_ProcessPlayerInput with slot flags 0x20 requests pattern 41
 unsigned char comboHudHoldFlag; // +0x0F0 U: read by HUD_UpdateComboCounters (keeps the combo display alive)
 unsigned char hitStopTimer; // +0x0F1 T: hit-stop frames left; the actor does not advance while non-zero (Actor_SetHitStop)
 unsigned char hitStopGraceTimer; // +0x0F2 T: set to 1 on hit; while non-zero the actor still ticks; when it reaches 0 landedThisTick is cleared
 unsigned char attackConnectedFlag; // +0x0F3 T: set to 1 by the hit detection when this actor attack connected; cleared by CPU_ApplyCpfStepInput; chains CPF steps
 unsigned char grabbedState; // +0x0F4 T: 0 free, 1 held by a character, 2 held by an effect object (Actor_AttachThrowVictim); non-zero = suspended / grab draw path
 unsigned char inputLock; // +0x0F5 T: non-zero forces direction/buttons to 0 for IF types 1/6/7/0xB/0x23 and blocks Character_ProcessPlayerInput walking; set on KO and round end
 unsigned char strikeInvulnTimer; // +0x0F6 T: frames of strike invulnerability (0xFF = infinite), ticked by Actor_TickByteTimers; hit detection requires 0
 unsigned char throwInvulnTimer; // +0x0F7 T: frames of throw invulnerability, set to 16 after hit-stun recovery
 unsigned char reserveState; // +0x0F8 T: 1 = benched reserve partner (ignored by hit detection, intro and camera), 0 on field, 2 written by MaidsPartner_UpdateReserveAI
 unsigned char cameraExcludeFlag; // +0x0F9 U: Camera_UpdateFollowActiveCharacters skips actors with it set
 unsigned char hitsRemaining; // +0x0FA T: AS.hitsNumber loaded on frame entry; decremented per connected hit; 0 = attack exhausted
 unsigned char lastHitWasGuarded; // +0x0FB T: Actor_ResolveIncomingHits sets 1 (guarded) / 0 (hit); cleared when the actor starts a new move (ProcessPlayerInput, TryExecute); non-zero with frameCounter < 12 restricts commands to CommandMove.flags1 bit 0x40
 unsigned char cpuGuardRoll; // +0x0FC T: CPU_UpdateAIForCharacter: 0 not rolled, 1 pass, 2 fail; read by the guard decision for CPU actors
 unsigned char enemyAttackWarnTimer; // +0x0FD T: Character_ProcessPlayerInput sets 8 when Character_DetectNearbyEnemyAttack is true, counts down
 unsigned char juggleLockState; // +0x0FE T: attacks only land when <= 2; AT.otherFlags 0x04 sets 10; air action counter use in Character_ProcessPlayerInput
 unsigned char techAttemptedFlag; // +0x0FF T: set when a tech was attempted (Character_ProcessTechInput, Throw_TryThrowEscape) and cleared when the actor can move
 unsigned char wallContactSide; // +0x100 T: Character_ClampToStageBounds: 1 = right wall, 2 = left wall
 unsigned char groundJumpLatch; // +0x101 T: set to 1 after a ground jump out of a cancel (Character_ProcessPlayerInput); cleared via the team-combo flag
 unsigned char counterHitFlag; // +0x102 T: set to 1 on the attacker for the counter-hit HUD (Actor_ResolveIncomingHits)
 unsigned char unused_103; // +0x103 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short untechTimeBudget; // +0x104 T: AT.untechTime or the hit-vector default; air recovery needs hitstunElapsed >= it
 unsigned short hitstunElapsed; // +0x106 T: frames since the hit; compared with g_AirStunTechThresholdTable; 3840 on exhaustion
 unsigned char airTechArmed; // +0x108 T: Character_ProcessTechInput / CPU_TryTechRecover
 unsigned char hitSeenFlag; // +0x109 U: written by the resolver and the main loop
 unsigned char airTechCountdown; // +0x10A T: counted down to 1, then the air tech may fire
 unsigned char techEnabled; // +0x10B T: (AT.otherFlags & 0x100) == 0 and no counter hit; ProcessTechInput and Throw_TryThrowEscape require it
 unsigned char techInvulnFlag; // +0x10C T: set by a tech; hit detection requires != 1; Character_MayRunCommandMoves tests it
 unsigned char unused_10D; // +0x10D X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short armorTimers[5]; // +0x10E T: five countdown words (Actor_TickWordTimersAndClearFrameFlags); [0] is the armor/hyper-armor window read by detection and the resolver; EF06 number 10 writes [p0]
 unsigned char throwConnectedFlag; // +0x118 T: set by the throw detection; the resolver clears the hit queue when set
 unsigned char hitShakeTimer; // +0x119 T: min(hitstop, 60); bit0 gives the horizontal draw shake in Character_Draw
 unsigned char hitQueueCount; // +0x11A T: valid entries of hitQueueAt / hitQueueRect / hitQueueAttacker, max 8
 unsigned char addedEffectType; // +0x11B T: AT.addedEffect 1..4 (Character_ApplyAddedEffect): tint cycle and particle presets
 unsigned char addedEffectTimer; // +0x11C T: set to 120 for types 1..3; tint applies while non-zero
 unsigned char unused_11D; // +0x11D X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short addedEffectType4Timer; // +0x11E T: set to 600 for type 4
 int hitStunProgress; // +0x120 U: cleared by the knockback code
 int hitStunTimer; // +0x124 T: > 0 grounded stun frames left, < 0 airborne stun until landing (< -2 = deep juggle), 0 = not in a hit state
 unsigned char hitVectorStep; // +0x128 T: current step of the hit vector
 unsigned char hitVectorId; // +0x129 T: g_HitVectorTable index, 0xFF = none
 unsigned char comboStunWindowLimit; // +0x12A U: read only by the attack detection
 unsigned char unused_12B; // +0x12B X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char healthLocked; // +0x12C T: non-zero blocks damage (KO state); gates the stun tick with pattern 27; IF type 5 reads it; bit 0x10 set by EF06 number 255
 unsigned char introDone; // +0x12D T: round intro waits while an active slot has reserveState == 0 and introDone == 0; EF06 number 254 sets it
 unsigned char koFinishKind; // +0x12E T: (attacker objectKind & 0x7F) + 1 written with health = 0; 0 = alive
 unsigned char airJumpState; // +0x12F T: bits 0..6 = air actions used (compared with the CT air-jump limit), bit 0x80 = re-armed
 unsigned char techBookkeeping130; // +0x130 U: cleared by Character_UpdateMainLoop when the actor can move
 unsigned char recoveryLock; // +0x131 T: with hitstunElapsed < 16 blocks command moves
 unsigned short stunMashCount; // +0x132 T: Character_UpdateStunMashAndHeatActivation increments; lowers the damage factor by 0.003 each (cap 0.5)
 signed char moveUseCounters[10]; // +0x134 T: per-chain use counters indexed by the tens digit of CommandMove.useLimit (CommandMove_CheckRequirements, EF06 numbers 100/101)
 signed char commandLockLevel; // +0x13E T: CommandMove.lockLevel must be above it; EF06 102/103 add/subtract; set to 100 by a ground tech
 unsigned char unused_13F; // +0x13F X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 short workVars[8]; // +0x140 T: IF 0x19/0x1D/0x1F/0x26 and EF06 number 105 word registers; [0] is the default X target of IF 0x1D
 unsigned char unused_150[4]; // +0x150 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned char ghostColorMode; // +0x154 T: EF06 number 0 trail colour + 1 (0 = off, 255 clears); on a ghost copy the colour 1..12
 unsigned char unused_155; // +0x155 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short ghostLifetime; // +0x156 T: initial life of a ghost copy; alpha = 255 * tick / life
 unsigned char ghostFlags; // +0x158 T: bit 0 = priority + 10
 unsigned char unused_159; // +0x159 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 short ghostZoomDeltaX; // +0x15A T: zoom change per remaining tick
 short ghostZoomDeltaY; // +0x15C T: zoom change per remaining tick
 unsigned char trailColorMode; // +0x15E T: after-image trail colour 1..11, 0 = off (EF06 number 3)
 unsigned char trailCount; // +0x15F T: number of after-images drawn (119 / spacing)
 unsigned char trailSpacing; // +0x160 T: history step between after-images
 unsigned char trailResetRequest; // +0x161 T: AfterImage_ResetHistory requested
 unsigned short usedNormalsMask; // +0x162 T: bit n = pattern n (1..9) used in this chain (CT flag 0x01); cleared when canMove
 unsigned char usedCommandBitset[26]; // +0x164 T: bit i = command move i used in this chain; zeroed with usedNormalsMask
 unsigned char pendingCommandId; // +0x17E T: command move requested by CommandMove_TryExecute
 unsigned char lastCommandId; // +0x17F T: 0xFF none
 short comboHitsTaken; // +0x180 T: hits taken in the current combo; indexes g_AirStunTechThresholdTable
 unsigned short hitsLandedThisPattern; // +0x182 T: incremented by every connect, reset on a pattern change; IF 0x11 reads it
 unsigned char doubleTapDirIndex; // +0x184 T: Slot_PollGuardReversalCommand: 0 none, 1..4 direction index
 signed char doubleTapTimer; // +0x185 T: window counter, negative = lockout
 unsigned short recoveryWatchTimer; // +0x186 T: counts 1..60 while recovering (4095 = disabled); sets inputLock logic
 unsigned char flashMode; // +0x188 T: Actor_SetColorFlash mode
 unsigned char flashTimer; // +0x189 T: Actor_SetColorFlash timer, ticked by Actor_TickByteTimers
 unsigned char flashPeriod; // +0x18A T: Actor_SetColorFlash period for the ramp modes
 unsigned char patternRequestExtra; // +0x18B T: Character_RequestPattern a5 stored with the accepted request
 unsigned char counterHitState; // +0x18C T: AS.statusFlags[1] bits 20..23: 1 -> 1, 2 -> 2, 3 -> 0
 unsigned char unused_18D; // +0x18D X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned short motionScratch18E; // +0x18E U: zeroed by Character_StartTechRecovery
 unsigned short motionScratch190; // +0x190 U: zeroed by Character_StartTechRecovery
 unsigned short motionScratch192; // +0x192 U: zeroed by Character_StartTechRecovery
 unsigned short motionScratch194; // +0x194 U: zeroed by Character_StartTechRecovery
 unsigned char unused_196[2]; // +0x196 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 struct MbrAt *hitQueueAt[8]; // +0x198 T: pointers to the 88-byte AT records of the incoming hits
 struct MbrIntRect hitQueueRect[8]; // +0x1B8 T: intersection rectangle of each queued hit (screen coordinates)
 struct MbrActor *hitQueueAttacker[8]; // +0x238 T: attacker actor of each queued hit
 struct MbrActor *lastAttacker; // +0x258 T: used for corner push-back
 struct MbrActor *grabLink; // +0x25C T: victim of this actor grab / grabber link; non-zero blocks being hit
 unsigned char grabLinkActive; // +0x260 T: 1 = link active (victim position follows)
 unsigned char unused_261[3]; // +0x261 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 struct MbrActor *rootCharacter; // +0x264 T: effect objects: the owning character (Actor_GetOwnerCharacter)
 struct MbrActor *parentActor; // +0x268 T: effect objects: the immediate spawner; grabbed actors: the grabber
 unsigned char inputDirection; // +0x26C T: numpad direction 0..9 (0 neutral) after facing mirroring
 unsigned char unused_26D[3]; // +0x26D X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 unsigned int inputButtons; // +0x270 T: bits 0..7 held mask (A=1 B=2 C=4 D=8 E=0x10 F=0x20), bits 12..19 pressed-this-frame byte; CPU adds 0x1000..0x8000
 unsigned int inputButtonsExtra; // +0x274 T: read by CommandBuffer_PushInput and recorded by Actor_RecordOrReplayInputs; no writer found
 unsigned char sideIndex; // +0x278 T: team side 0/1 = slot & 1; indexes the 0x48-byte team records (g_SideActiveSlotIndex ...)
 unsigned char removeNextTick; // +0x279 T: effect objects: set by EffectObject_MarkForRemoval, the object is freed on the next EffectObjects_UpdateAll pass
 unsigned char unused_27A[2]; // +0x27A X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 int rotationBase10000; // +0x27C U: added to AF.rotation for flip modes 8/9; no writer found
 short pendingPatternId; // +0x280 T: pattern requested by Character_RequestPattern (-1 = none, 0xFFFF for effects)
 short pendingPatternPriority; // +0x282 T: priority of the pending pattern request
 short pendingFrameIndex; // +0x284 T: frame requested by Character_RequestFrameInPattern
 short pendingFramePriority; // +0x286 T: priority of the pending frame request
 unsigned char facing; // +0x288 T: 0 = facing right, 1 = facing left
 unsigned char desiredFacing; // +0x289 T: facing toward the target; a difference triggers the turn-around
 signed char actionByte28A; // +0x28A U: -1 at slot init; cleared by Character_StartTechRecovery
 unsigned char facingChangedFlag; // +0x28B U: set when desired facing changed
 unsigned char commandFiredFlag; // +0x28C U: IF 6/7 set 1 when a button-input action fired with p3 bit 4; pulse flag in Character_UpdateMainLoop
 unsigned char skipAdvanceFlag; // +0x28D T: 1 = pattern just reset, frame counter not advanced this tick; IF 0x12 reads it on the owner
 unsigned char landedThisTick; // +0x28E T: 1 = posY crossed the ground this tick
 unsigned char landProcessedFlag; // +0x28F T: set when the landing frame was processed; also the "mirror input history pending" flag cleared by CommandBuffer_PushInput
 unsigned char prevStance; // +0x290 T: AS.stanceState of the previous tick
 unsigned char unused_291[3]; // +0x291 X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
 struct MbrBox *boxTable; // +0x294 T: 8-byte MbrBox table of the current pattern
 struct MbrDatHeader *dataBlobA; // +0x298 T: character file header + patterns
 struct MbrPatternHeader *patternHeader; // +0x29C T: current MbrPatternHeader
 struct MbrFrame *frameRecord; // +0x2A0 T: current 216-byte MbrFrame (first copy)
 struct MbrFrame *frameRecordAlias; // +0x2A4 T: the same pointer as frameRecord, used by drawing code
 struct MbrAS *asRecord; // +0x2A8 T: current AS (frame + 44)
 struct MbrIf *ifTable; // +0x2AC T: pattern IF table (52-byte records)
 struct MbrEf *efTable; // +0x2B0 T: pattern EF table (52-byte records)
 short *frameBoxIndexArray; // +0x2B4 T: frame + 148 = idx[24]; box slot k (0..32) is the int16 at [2*k]
 void *dataBlobB; // +0x2B8 T: parts blob (sprite id < 10000)
 void *dataBlobC; // +0x2BC T: CG blob (sprite id >= 10000)
 struct MbrCharacterSlot *ownSlot; // +0x2C0 T: CharacterSlot* of this actor
 struct MbrActor *teammateActor; // +0x2C4 T: tag partner actor of the same side (Slot_Init sets it; TagTeam_ProcessSwap rewrites it)
 unsigned int frameAge; // +0x2C8 T: Character_UpdateMainLoop increments it each tick; CommandMove_CheckRequirements tests < 12
 unsigned char ranThisFrame; // +0x2CC T: Character_UpdateMainLoop writes 1 when the actor ran this tick
 unsigned char unused_2CD[3]; // +0x2CD X: no instruction of any actor function in the traced code ranges addresses it (see section 5 note); zero-filled by Slot_Init* memset
};

struct MbrCommandMove {                  // 44 bytes, slot+0x334 + 44 * i (file *_C.CT bytes 4 + 44*i); 200 per character
 unsigned char id;                       // +0x00 T D: 0xFF = unused entry, otherwise equals the entry index (Slot_FindAndExecuteCommandMove, CommandMove_TryExecute)
 unsigned char unused_01;                // +0x01 X D: never read, 0 in all shipped files
 unsigned char runtimeScratch[16];       // +0x02 X D: Slot_Init* zeroes bytes +0x02..+0x11 of the first `commandCount` entries each round; no reader; 0 in the files
 unsigned char inputSequence[16];        // +0x12 T D: 0xFF terminated, read from the LAST symbol backwards (CommandMove_MatchesInputHistory / CommandSeq_Parse*): '0'..'9' as raw bytes 0..9 = numpad direction, 0x41..0x46 = buttons A..F, 0x2B '+' joins a direction and buttons of one step, 0x5E '^' / 0x56 'V' / 0x3C '<' / 0x3E '>' = any up / down / left / right group, 0x54 'T' = hold marker
 unsigned char patternId;                // +0x22 T D: pattern requested (priority 4200 - entry index)
 unsigned char requirementMode;          // +0x23 T D: 0 = AS.cancelNormal rule, 1 = AS.cancelSpecial rule (AS.statusFlags1 bit 1 adds contact from hitResultFlags), 2 = same as 1 with the throw-escape contact test (CommandMove_CheckRequirements)
 unsigned short meterCost;               // +0x24 T D: meter needed (Actor_CanAffordMeter, free in heat mode); stored as pendingMeterCost; 99999 truncated to 0x869F = whole gauge
 unsigned char partnerRequirement;       // +0x26 T D: 1 = partner must be loaded and idle, 2 = partner must be loaded
 unsigned char useLimit;                 // +0x27 T D: < 100: moveUseCounters[(v % 100) / 10] must be < v % 10; >= 100: g_GlobalIfCounters[v / 100] must be < v % 100
 unsigned char lockLevel;                // +0x28 T D: blocked when 0 < lockLevel <= actor.commandLockLevel
 unsigned char flags1;                   // +0x29 T D: 0x01 / 0x02 / 0x04 = usable in stance stand / air / crouch, 0x08 = only from a free state, 0x10 = clear the input history after use, 0x20 = never auto matched, 0x40 = allowed in the cancel window after a hit
 unsigned char flags2;                   // +0x2A T D: 0x01 = match against input ring B (steps/durations at slot+0x2794), 0x02 = strict (newest step must be exactly 2 frames old), 0x04 = allowed while the round intro is active, 0x08 = request pattern with re-request allowed, 0x40 = only usable in the cancel context
 unsigned char unused_2B;                // +0x2B X D: never read
};
struct MbrDoubleTapEntry {               // 16 bytes, 4 per character at CT+0x1C (slot+0x2F0): directions 6, 2, 4, 8 (Slot_PollGuardReversalCommand)
 unsigned short enabled;                 // +0x00 T D: non-zero enables the double-tap probe; 0 in every shipped CT, so the feature is inert in the shipped data
 unsigned short patternId;               // +0x02 T: pattern requested by the resolver (priority 5000) when the double tap fired during a hit (Actor_ResolveIncomingHits)
 unsigned short hitStop;                 // +0x04 T: hit-stop used with it
 unsigned short param06;                 // +0x06 U: 0 in shipped data except ARC/AOKO/NECO/NEKO (10)
 unsigned short param08;                 // +0x08 U
 unsigned short param0A;                 // +0x0A U: 1 for ARC/AOKO/NECO/NEKO
 unsigned char windowFrames;             // +0x0C T: frames the double tap stays armed (written to actor.doubleTapTimer)
 unsigned char unused_0D;                // +0x0D X
 unsigned char lockoutFrames;            // +0x0E T: negated into actor.doubleTapTimer after use
 unsigned char unused_0F;                // +0x0F X
};
struct MbrCtParams {                     // 0x5C bytes at slot+0x2D4 = file *_C.CT bytes 8804..8895
 unsigned char maxAirActions;            // +0x00 T D: air jumps/dashes allowed, compared with actor.airJumpState & 0x7F (Character_ProcessPlayerInput); 0 AOKO/NECO/NEKO, 1 others
 unsigned char unused_01;                // +0x01 X D: never read (9 in all shipped files)
 unsigned char airTechMode;              // +0x02 T D: read by Character_UpdateMainLoop (1 in all shipped files)
 unsigned char unused_03[2];             // +0x03 X D: never read (0)
 unsigned char flags;                    // +0x05 T D: 0x01 = one use per chain for normals/commands (Character_TryNormalAttackFromInput, CommandMove_TryExecute, Actor_BeginRequestedPattern), 0x10 = air guard allowed (Actor_ResolveIncomingHits), 0x20 = jump cancel / ground jump enabled (Character_ProcessPlayerInput), 0x40 = KO launch override / no random heat request (Actor_ResolveIncomingHits, CPU_UpdateAIForCharacter), 0x80 = read by HUD_DrawHealthMeterBarsAndInfo; shipped 0x39 (0x79 F_CIEL, 0xB9 SION)
 unsigned char unused_06[2];             // +0x06 X D: never read (0)
 float unusedFloat08;                    // +0x08 X D: never read (1.0 in most files)
 float damageMultiplier;                 // +0x0C T D: multiplies damage dealt/taken by this character in Damage_CalcScaledDamage / Damage_CalcModeScaledDamage (0 = ignored); 1.0 default, 0.9 AKIHA, 1.1 AKAAKIHA, 0.6 GAKIHA/F_CIEL
 float unusedFloat10[3];                 // +0x10 X D: never read (second..fourth of the five shipped floats)
 struct MbrDoubleTapEntry doubleTap[4];  // +0x1C T
};
struct MbrCharacterSlot {                // 0x2994 bytes; g_CharacterSlots 0x7C5AB8 (4 slots: 0/1 = main members of side 0/1, 2/3 = their partners)
 unsigned char active;                   // +0x00 T: 1 when the slot holds a character (cleared for sides without a partner)
 unsigned char unused_01[3];             // +0x01 X: no access found
 struct MbrActor actor;                  // +0x04
 struct MbrCtParams ct;                  // +0x2D4 T: copied from the *_C.CT file by Slot_LoadCommandCT; preserved by Slot_Init*
 int commandCount;                       // +0x330 T D: first dword of the *_C.CT file = number of defined entries (Slot_Init* clears the runtime bytes of that many leading entries)
 struct MbrCommandMove commands[200];    // +0x334 T D: copied from the *_C.CT file
 unsigned int inputStepsA[64];           // +0x2594 T: input history ring A, newest first: (direction << 24) | button bits (CommandBuffer_PushInput); mirrored on a facing change
 unsigned int inputDurationsA[64];       // +0x2694 T: frames each step of ring A was held; [0] grows every frame, saturates at 1000000, InputHistory_ClearCommandTimers sets it to 1000000
 unsigned int inputStepsB[64];           // +0x2794 T: ring B (steps OR-ed with actor.inputButtonsExtra), used by commands with flags2 bit 0x01
 unsigned int inputDurationsB[64];       // +0x2894 T
};
struct MbrEffectObject {                 // 0x2D4 bytes; g_EffectObjectSlots 0x7D0938, 1000 entries
 unsigned char active;                   // +0x00 T: in-use flag
 unsigned char unused_01[3];             // +0x01 X: no access found
 struct MbrActor actor;                  // +0x04: same layout as a character actor, objectKind 0xFF
};

// CPU AI per-slot state (g_CpuSlotState 0x5564C8, 4 x 224 bytes, index = cpuSlot: 0/1 main members, 2/3 partners)
struct MbrCpuSlotState {                 // 224 bytes
 int scriptIndex;                        // +0x00 T: running CPF script
 int stepIndex;                          // +0x04 T
 int framesLeft;                         // +0x08 T: frames left in the running step
 int repeatCooldown;                     // +0x0C T: frames before the next script selection (CPU_RepeatCooldownFramesForDifficulty); counted down
 int targetDx;                           // +0x10 T: dx to the nearest opponent (Actor_FindNearestEnemyByXY)
 int targetDy;                           // +0x14 T
 short scriptCooldown[100];              // +0x18 T: per-script cooldown, ticked by CPU_TickScriptCooldowns
};
struct MbrCpuCandidate { int weight; int priority; };                 // g_CpuCandidateScores[100] 0x556848

// ============================================================================================
// 6. CHARASELECT.CT and the character table (CharaSelect_LoadCharaSelectCT 0x437E70; file '.\charaselect.ct', found in 04.p)
//    file = u32 count, 36-byte header (9 dwords, dword[0..1] = default cursor character ids of P1/P2), count * 196 bytes entries.
//    The entries are enciphered: byte[k] ^= (k + key[k % 24] + 3) & 0xFF over all count*196 bytes, key = Shift-JIS
//    "ファイルが見つかりません" (24 bytes at 0x4774F8) (Crypto_XorWithKeyString).
// ============================================================================================
struct MbrCharEntry {                    // 196 bytes; g_CharEntryStorage 0x94E648 (100 entries), g_CharTable[charId] points at them
 unsigned int valid;                     // +0x00 T: set to 1 by CharaSelect_BuildCharTableWithUnlocks for available entries (0 in the file)
 char shortName[48];                     // +0x04 T D: names the .wmt, win/face/ED bitmaps, story files ('AKIHA', 'HISUI&KOHAKU', 'M_HISUI', ...)
 char memberName0[32];                   // +0x34 T D: file stem of data\<stem>.dat / .cpf / _c.ct and cut-in / voice names (equals shortName except HISUI&KOHAKU -> HISUI)
 char memberName1[32];                   // +0x54 T D: partner stem: first char '0' = no partner, '1' = flag only (team.scriptedCharacterFlag = 1, no partner loaded), anything else = partner character files (KOHAKU)
 int compactIndex;                       // +0x74 U: written by the build routine, no reader found
 int charId;                             // +0x78 T D: index into g_CharTable; copied to actor.characterId
 int stageId;                            // +0x7C T D: g_LastCharStage on pick
 unsigned int unlockMask;                // +0x80 T D: hidden unless (mask & g_UnlockMask) != 0 (0 = always available)
 unsigned int lockedFlag;                // +0x84 T D: non-zero = locked copy
 int reserved_88;                        // +0x88 U: zero in the file, no reader found
 int reserved_8C;                        // +0x8C U: no reader found (equals charId except SATSUKI 16 and ids >= 18)
 int altCharId;                          // +0x90 T D: -1 none; pressing pause on the CSS jumps the cursor to this character
 int portraitAnchorX;                    // +0x94 T D: CSS_DrawPanelPortraitAndInfo (halved)
 int portraitAnchorY;                    // +0x98 T D
 int notSelectableTile;                  // +0x9C T D: 1 = non-character tile (skipped by the random roulette)
 int cursorX;                            // +0xA0 T D: CSS cursor icon x = value + 4
 int cursorY;                            // +0xA4 T D: y = value - 4
 int effectAnchorX;                      // +0xA8 T: copied into effect 0x33; 0 in the file
 int effectAnchorY;                      // +0xAC T
 unsigned char selectable;               // +0xB0 T: set to 1 by the build routine, 0 for locked copies
 unsigned char neighbourUp;              // +0xB1 T D: CSS cursor neighbour by numpad direction 8 (char id)
 unsigned char neighbourDown;            // +0xB2 T D: direction 2
 unsigned char neighbourLeft;            // +0xB3 T D: direction 4
 unsigned char neighbourRight;           // +0xB4 T D: direction 6
 unsigned char reserved_B5[3];           // +0xB5 D: zero, no reader
 int vsArtOffsetX;                       // +0xB8 T D: x shift of the hero art on the VS screen / CSS
 int winArtOffsetX;                      // +0xBC T D: x shift on the win screen
 unsigned int hiddenFlag;                // +0xC0 T D: non-zero hides the entry (1 for GAKIHA, NECO, AOKO, WLEN, F_CIEL)
};
struct MbrCharaSelectCtFile {            // 4548 bytes in the shipped file (23 entries)
 unsigned int count;                     // +0x00 T D
 unsigned int header[9];                 // +0x04 T D: copied to g_CharaSelectCtHeader; [0],[1] = default cursor characters of P1/P2, [2..8] never read
 struct MbrCharEntry entries[23];        // +0x28 T D: enciphered (see above)
};

// ============================================================================================
// 7. Team records, combo records, hit vectors
// ============================================================================================
struct MbrTeamRecord {                   // 0x48 bytes, 4 records at g_TeamRecords 0x69F8D8 (index = side 0/1 = actor.sideIndex)
 int activeSlotIndex;                    // +0x00 T: slot of the on-field member (0/1 main, 2/3 partner); TagTeam_ProcessSwap flips it by 2
 int swapRequest;                        // +0x04 T: 4 = swap pending (cleared by TagTeam_ProcessSwap), also counted by IF 0x64
 int hasPartner;                         // +0x08 T: 1 when slot 2+side was loaded
 int scriptedCharacterFlag;              // +0x0C T: 1 when CharEntry.memberName1 starts with '1' (GAKIHA): forces guard class 3 and CPU command directions 4/0/6, no win pose in round-end phases 3/4
 int damageRankScore;                    // +0x10 T: drawn as the score (HUD_DrawScoreAndCredits)
 int maxComboHits;                       // +0x14 T: HUD_UpdateComboCounters
 int hitsLandedCount;                    // +0x18 T
 int maxComboDamage;                     // +0x1C T: folded into g_CharStatMaxDamage at round end
 int reserved_20[2];                     // +0x20 U: no reader found
 int sideIndexCopy;                      // +0x28 T: reset to the side index by Battle_ResetTeamAndMatchState
 int counterHitCount;                    // +0x2C T
 int elapsedFrames;                      // +0x30 T: accumulates g_RoundElapsedFrames
 int counterA;                           // +0x34 U: with counterB drawn clamped to 99 after the score
 int counterB;                           // +0x38 U
 int autoGuardOption;                    // +0x3C T: Actor_ResolveIncomingHits guard decision
 int handicapLevel;                      // +0x40 T: index of g_HandicapDamageDealtScale / TakenScale
 int infiniteHeatOption;                 // +0x44 T: arcade: heat gauge never depletes and heat mode 2 starts immediately (Character_UpdateStunMashAndHeatActivation); also read by the HUD banner
};
struct MbrComboRecord {                  // 16 bytes; g_ComboCounterBlock 0x7C47D0 = 4 sides x 8 records
 short hits;                             // +0x00 T
 short airHits;                          // +0x02 T
 short groundHits;                       // +0x04 T
 short totalDamage;                      // +0x06 T
 short prorationPercent;                 // +0x08 T: 0 = 100
 unsigned char reserved_0A[2];           // +0x0A U
 unsigned char displayTimer;             // +0x0C T: 60 on hit
 unsigned char hudSlideCounter;          // +0x0D T
 unsigned char secondLineShown;          // +0x0E T
 unsigned char rankOrEscapeFlag;         // +0x0F T
};
struct MbrHitVectorStep {                // 20 bytes
 int knockbackRectIdx;                   // +0x00 T: index into g_HitVectorKnockbackRects
 int hitPattern;                         // +0x04 T
 int guardPattern;                       // +0x08 T
 int stunValue;                          // +0x0C T: written to actor.hitStunTimer
 unsigned int flagMask;                  // +0x10 T: bit k set if decimal digit k of the file value is non-zero; bit 1 wall bounce, 2 bump spark (preset 22), 4 floor spark (preset 30)
};
struct MbrHitVector {                    // 152 bytes; g_HitVectorTable 0x93E4A0, loaded from VECTOR.TXT by 0x437F20
 char name[32];                          // +0x00 T
 int stepCount;                          // +0x20 T
 int defaultUntechTime;                  // +0x24 T
 int priority;                           // +0x28 T: arbitration (HitVector_ChooseByPriority)
 int fallbackVectorId;                   // +0x2C T
 int keepOnKoLaunch;                     // +0x30 T: kept when the incoming id is 99
 struct MbrHitVectorStep steps[5];       // +0x34 T
};
struct MbrKnockbackRect { int velX; int velY; int accelX; int accelY; };   // 16 bytes, g_HitVectorKnockbackRects[200] 0x69EAF0, rows 'id velX velY accelX accelY' of VECTOR.TXT; the actor stores accel as s16

// ============================================================================================
// 8. Enumerations
// ============================================================================================
enum MbrControlType : unsigned char { MBR_CTRL_HUMAN = 0, MBR_CTRL_CPU = 1 };
enum MbrStance : unsigned char { MBR_STANCE_GROUND = 0, MBR_STANCE_AIR = 1, MBR_STANCE_CROUCH = 2 };
enum MbrHeatMode : unsigned char { MBR_HEAT_NONE = 0, MBR_HEAT_ACTIVE_SLOW = 1, MBR_HEAT_READY = 2, MBR_HEAT_BIG = 3 };
enum MbrPatternId : unsigned char {      // pattern ids hard-coded by the engine (the rest are character specific)
 MBR_PAT_STAND_IDLE = 0, MBR_PAT_STAND_A = 1, MBR_PAT_STAND_B = 2, MBR_PAT_STAND_C = 3, MBR_PAT_CROUCH_A = 4, MBR_PAT_CROUCH_B = 5, MBR_PAT_CROUCH_C = 6,
 MBR_PAT_JUMP_A = 7, MBR_PAT_JUMP_B = 8, MBR_PAT_JUMP_C = 9, MBR_PAT_WALK_FORWARD = 10, MBR_PAT_WALK_BACK = 11, MBR_PAT_CROUCH_DOWN = 12,
 MBR_PAT_CROUCH_IDLE = 13, MBR_PAT_CROUCH_UP = 14, MBR_PAT_STAND_TURN = 15, MBR_PAT_CROUCH_TURN = 16, MBR_PAT_STAND_GUARD = 17, MBR_PAT_CROUCH_GUARD = 18,
 MBR_PAT_JUMP_UP = 19, MBR_PAT_WALL_BOUNCE = 26, MBR_PAT_KO_DOWN = 27, MBR_PAT_GROUND_JUMP_FORWARD = 35, MBR_PAT_GROUND_JUMP_UP = 36, MBR_PAT_GROUND_JUMP_BACK = 37,
 MBR_PAT_AIR_JUMP_FORWARD = 38, MBR_PAT_AIR_JUMP_UP = 39, MBR_PAT_AIR_JUMP_BACK = 40, MBR_PAT_JUMP_CANCEL_JUMP = 41, MBR_PAT_ROUND_INTRO = 50,
 MBR_PAT_WIN_POSE = 52, MBR_PAT_PARTNER_FOLLOW = 53, MBR_PAT_PARTNER_RETURN = 245, MBR_PAT_HEAT_ACTIVATE = 250, MBR_PAT_THROW_ESCAPE_VICTIM_GROUND = 251,
 MBR_PAT_THROW_ESCAPE_VICTIM_AIR = 252, MBR_PAT_THROW_ESCAPE_GRABBER_GROUND = 253, MBR_PAT_THROW_ESCAPE_GRABBER_AIR = 254
};

// ============================================================================================
// 9. Further runtime globals touched by the character / effect / HUD / draw code (applied in the IDB)
// ============================================================================================
struct MbrSpriteDrawRecord {             // 0x70 bytes; g_SpriteDrawRecord 0x791700, filled by Sprite_DrawActorFrame, consumed by the quad submit routine sub_439F80
 int cutoutTexIndex;                     // +0x00 T: read as 16 bit
 int srcX;                               // +0x04 T
 int srcY;                               // +0x08 T
 int zeroedField0C;                      // +0x0C T: always written 0, never read
 int quadW;                              // +0x10 T
 int quadH;                              // +0x14 T
 int originX;                            // +0x18 T
 int originY;                            // +0x1C T
 int pivotOffX;                          // +0x20 T
 int pivotOffY;                          // +0x24 T
 int screenX;                            // +0x28 T
 int screenY;                            // +0x2C T
 short srcRectL;                         // +0x30 T
 short srcRectT;                         // +0x32 T
 short srcRectR;                         // +0x34 T
 short srcRectB;                         // +0x36 T
 float rotX;                             // +0x38 T: degrees, stored 0
 float rotY;                             // +0x3C T: degrees, stored 0
 float rotZ;                             // +0x40 T: degrees (layer rotation * 0.036)
 float scaleX;                           // +0x44 T
 float scaleY;                           // +0x48 T
 unsigned int modulateArgb;              // +0x4C T: frame tint bytes x caller multiplier
 unsigned int additiveRgb;               // +0x50 T: frame add bytes + caller additive, saturated
 int flipMode;                           // +0x54 T: 0..9
 int priorityListIndex;                  // +0x58 T: 5 / 11 / caller priority
 int tweenOffsetX;                       // +0x5C U: written only, reader not found
 int tweenOffsetY;                       // +0x60 U
 int extraRotationDeg;                   // +0x64 T
 int actorFlagsA;                        // +0x68 U: actor objectFlagsA copy, reader not found
 int actorFlagsB;                        // +0x6C U: actor objectFlagsB copy
};
struct MbrAfterImageEntry {              // 52 bytes; g_AfterImageHistory 0x9D6628 = [4 players][120 entries], entry 0 newest
 int posX;                               // +0x00 T
 int posY;                               // +0x04 T
 unsigned char patternId;                // +0x08 T
 unsigned char frameIndex;               // +0x09 T
 unsigned char pad_0A[2];                // +0x0A U
 int tweenTargetSpriteId;                // +0x0C T
 int frameTickCounter;                   // +0x10 T
 unsigned char facing;                   // +0x14 T
 unsigned char pad_15[3];                // +0x15 U
 struct MbrPatternHeader *patternHeader; // +0x18 T
 struct MbrFrame *frameRecord;           // +0x1C T
 struct MbrFrame *frameRecordAlias;      // +0x20 T
 struct MbrAS *asRecord;                 // +0x24 T
 struct MbrIf *ifTable;                  // +0x28 T
 struct MbrEf *efTable;                  // +0x2C T
 short *frameBoxIndexArray;              // +0x30 T
};
struct MbrSlotScreenFx {                 // 776 bytes; g_SlotScreenFx 0xA41500 = 5 records (0..3 player slot, 4 = extra record read by Background_DrawScreenFillEffects)
 int remaining[64];                      // +0x000 T: per-effect-id countdown (ScreenFx_StartBgEffect sets, Battle_TickFrameTimers decrements)
 int elapsed[64];                        // +0x100 T
 int duration[64];                       // +0x200 T
 int superFreezeTimer;                   // +0x300 T: Actor_MayActDuringSuperFreeze / EF06 number 11
 int superFreezeTimer2;                  // +0x304 T: decremented together with superFreezeTimer, no other reader
};
struct MbrArcadeLadderEntry {            // 28 bytes; g_ArcadeLadderTable 0x9DD338 = 32 entries (Story_LoadArcadeLadderTable, "%d,%d,%d,%d,%d" lines of <char>_table.txt)
 int opponentChar;                       // +0x00 T: g_CharTable index (255 = unused)
 int opponentColor;                      // +0x04 T: 255 default
 int init_08;                            // +0x08 U: set to 255 by the loader, never read
 int init_0C;                            // +0x0C U: set to 255 by the loader, never read
 int stageBg;                            // +0x10 T
 int opponentInfiniteHeat;               // +0x14 T: copied to the opponent team record infiniteHeatOption
 int interludeFlag;                      // +0x18 T: non-zero makes WinScreen_Update take the story interlude branch
};
struct MbrSysEffect {                    // 96 bytes; g_SysEffectPool 0x9532E0 = 5000 entries (SysEffect_Alloc takes the first entry with active == 0)
 unsigned char active;                   // +0x00 T
 unsigned char ownerOrSide;              // +0x01 T
 unsigned char flag02;                   // +0x02 T
 unsigned char pad_03[5];                // +0x03 U
 short kind;                             // +0x08 T
 short life;                             // +0x0A T
 short lifeMax;                          // +0x0C T
 unsigned char pad_0E[2];                // +0x0E U
 int posX;                               // +0x10 T
 int posY;                               // +0x14 T
 int param18;                            // +0x18 T
 unsigned char rest_1C[68];              // +0x1C U: preset specific (SysEffect_SpawnPreset)
};
struct MbrHudSideLayout {                // 80 bytes x 2 sides at g_HudSideLayout 0x477060
 int healthBarX;                         // +0x00 T
 int meterGaugeX;                        // +0x04 T
 int reserved_08[2];                     // +0x08 U: no direct reference
 int infoX;                              // +0x10 T
 int reserved_14[6];                     // +0x14 U: no direct reference (0x14..0x2B)
 int reserved_2C[9];                     // +0x2C U: zero padding
};
struct MbrHitEffectPreset {              // 12 bytes; g_HitEffectPresets[15] 0x477EC0, indexed by MbrAt.hitEffect
 short presetId;                         // +0x00 T: SysEffect_SpawnPreset id
 short paramA;                           // +0x02 T
 short paramB;                           // +0x04 T
 short paramC;                           // +0x06 T
 short soundId;                          // +0x08 T: Se_RequestPlay id, 10000 = none
 short unused_0A;                        // +0x0A X: 0 in all entries
};
struct MbrDamageRankStep { int score; int damageBelow; };           // g_DamageRankSteps[10] 0x4780A8 (Combo_AddDamageRankScore)
