// Queen of Heart '99 (qoh99.exe Nov/Dec 2000) character .chr / .Fob / .Img and runtime types, IDA-parsable (idc.parse_decls). Generated offsets are machine-checked by tools/ida/gen_cpp_types.py.
// Evidence for each field: docs/formats/qoh99.md. Tags in comments: T = access traced in qoh99_dec.exe.i64, D = verified on the shipped files (tools/fb/qoh99_verify.py), U = unproven (guess or never traced), X = never read by the game (data-bearing only).
// A field is named unused_<hexoffset> ONLY when it is constant (0 or 0xCD fill) in all 37 shipped .chr files and no reader exists in the decompiled code (ctree scan of every function). Runtime gaps that are read but whose meaning is not resolved are named unresolved_<hexoffset>.
// On-disk (.chr) values are in plaintext form: the file is enciphered per region, see qoh99_cipher.py and the doc, section 2.
enum QohTileXform : unsigned char { QXF_IDENTITY=0, QXF_ROT_90_CW=1, QXF_ROT_180=2, QXF_ROT_90_CCW=3, QXF_FLIP_H=4, QXF_TRANSPOSE=5, QXF_FLIP_V=6, QXF_ANTI_TRANSPOSE=7 };  // frame.tileXform, applied by RenderCharacterSprite (xor 4 when the actor faces left)
enum QohFrameMoveFlags : unsigned char { QFM_SET_X_ACCEL=1, QFM_SET_Y_ACCEL=2, QFM_ON_LAND_JUMP_TO_FRAME=4 };  // frame.moveFlags, HandleCharacterMovement / UpdateCharacterLogic
enum QohFrameFxFlags : unsigned int { QFX_SOUND_SLOT_MASK=31, QFX_SOUND_BANK_MASK=192, QFX_SPAWN_FX_1=65536, QFX_HIT_DAMAGE_PHASE0=131072, QFX_SET_SCREEN_MODE_4=262144, QFX_CLEAR_COMBO_STATE=524288, QFX_SPAWN_FX_2=2097152, QFX_KO_EFFECT=4194304, QFX_POSITIONED_FX=8388608, QFX_HIT_DAMAGE_PHASE1=16777216 };  // frame.fxFlags (HandleCharacterMovement); low byte = sound (id = (b&0x1F)-1, bank = b>>6)
enum QohAttackFlags : unsigned char { QAF_DAMAGING_HIT=1 };  // attack.flags
enum QohFileKind : unsigned int { QFK_CHARACTER_SLOT_FIRST=0, QFK_CHARACTER_SLOT_LAST=36, QFK_OBJ1=37, QFK_RINA_CRA=38 };  // index into g_file_type_checksums / ValidateAndDecryptFileHeader first argument
struct QohChrHeader {                 // 44 bytes at file start (name block, tag, six counts); enciphered, see doc section 2
 char nameBlock[16];                   // +0x00 T: stem of the file name (upper-case) XOR runtime key PRIM_RT; the loader decodes it and compares it with the stem of the requested path (ValidateAndDecryptFileHeader 0x42C1F0)
 unsigned int revisionStamp;           // +0x10 T/D: high 16 bits = per-file revision compared with g_file_type_checksums[fileKind] by ValidateAndDecryptFileHeader (D: 0 for 24 files, 1 for 9, 2 for Corin, Kaede and Tiria, 5 for Yosie); low 16 bits = 0x000C in all 37 files (X)
 unsigned int imageCount;              // +0x14 T: c0 -> assets.imageCount; 12-byte image records
 unsigned int tileCount;               // +0x18 T: c1 -> assets.tileCount; 260-byte tiles
 unsigned int actionCount;             // +0x1C T: c2 -> assets.actionCount; 24-byte action records (256 in all 37 files)
 unsigned int frameCount;              // +0x20 T: c3 -> assets.frameCount; 64-byte frames
 unsigned int boxCount;                // +0x24 T: c4 -> assets.boxCount; 8-byte boxes
 unsigned int attackCount;             // +0x28 T: c5 -> assets.attackCount; 24-byte attack records
};
struct QohImageRecord {                 // 12 bytes, imageCount of them; image i owns tiles [firstTile, firstTile+tileCount); runtime GetAnimationFrameData 0x4629A0
 unsigned int firstTile;               // +0x00 T: first tile index (D: ranges are contiguous from 0 and sum to tileCount in every file)
 unsigned int tileCount;               // +0x04 T: number of tiles drawn for the image (D: 0 for 31 images)
 unsigned short width;                 // +0x08 T: canvas width, passed to the sprite queue (D: 256 in 6923 records, 320 in the 15 O_Chizuru full-screen records)
 unsigned short height;                // +0x0A T: canvas height (D: 256, or 240 with width 320)
};
struct QohTile {                 // 260 bytes, tileCount of them; a 16x16 block of the image canvas, drawn by QueueSpriteRenderWithAlignment
 unsigned short x;                     // +0x00 T: column of the block inside the image canvas (D: multiple of 16, <= 240)
 unsigned short y;                     // +0x02 T: row of the block (D: multiple of 16, <= 240)
 unsigned char pixels[256];            // +0x04 T: 16x16 palette indices, row-major (D: all values 0..15; 0 = transparent)
};
struct QohPaletteEntry {                 // 4 bytes; the loader transposes bytes 0..2 into a PALETTEENTRY (SetupCharacterPalettes 0x45B3F0)
 unsigned char blue;                   // +0x00 T
 unsigned char green;                  // +0x01 T
 unsigned char red;                    // +0x02 T
 unsigned char reserved;               // +0x03 D: 0 in every entry of every file
};
struct QohChrPalette {                 // 1024 bytes, copied into the caller buffer (lpBuffer). After decoding the loader zeroes B,G,R of entries 0, 16, 32 and 48 (transparent black)
 struct QohPaletteEntry entries[256];  // +0x00 T: 16 banks of 16 colours (tile pixels 0..15 select a colour inside a bank; the bank is chosen at draw time)
};
struct QohAction {                 // 24 bytes, actionCount of them; index = action id (actor.actionIndex)
 int firstFrame;                       // +0x00 T: index of the first frame, -1 = action unused (HandleCharacterMovement resets the actor to action 0) (D: ranges tile [0,frameCount) exactly in every file)
 int frameCount;                       // +0x04 T: frames in the action; the step code resets the frame cursor to 0 when frameCount < nextFrameIndex
 int firstBox;                         // +0x08 T: base index into the box table; frame.bodyBoxIndex/hurtBoxFirst/guardBoxFirst are offsets from it; -1 when none
 int boxCount;                         // +0x0C X/D: upper bound of the box offsets+counts of the action frames (violated only by 0xFF sentinels); never read
 int firstAttack;                      // +0x10 T: base index into the attack table; frame.attackFirst is an offset from it; -1 when none
 int attackCount;                      // +0x14 X/D: upper bound of the attack offsets+counts of the action frames; never read
};
struct QohFrame {                 // 64 bytes, frameCount of them: one animation frame (draw + timing + motion + boxes + effects). Same layout as the 52-byte Qoh98Sprite plus 12 bytes at +0x34
 int imageIndex;                       // +0x00 T: index into the image table; out of range draws nothing (GetAnimationFrameData)
 short drawX;                          // +0x04 T: x offset of the 256x256 canvas relative to the actor position (negated when facing left)
 short drawY;                          // +0x06 T: y offset (the shadow pass divides it by 3)
 int duration;                         // +0x08 T: copied to actor.frameTimer (D: 231 occurs 3290 times, values up to 400+)
 unsigned short nextFrame;             // +0x0C T: frame index inside the action that follows; 0xFFFF = frame+1, 0xFFFE = action ends and holds (actor.nextFrameIndex = 0xFFFE) (D: otherwise < action.frameCount except 4 frames)
 unsigned short attackLevelAndFlags;   // +0x0E T: low nibble -> actor.attackLevel; bit 0x10 arms the attack boxes (contactFlags |= 1, attackHitMask = 0xFF); bit 0x20 tested by the hit code (marks the opponent contactFlags |= 0x10)
 unsigned char moveFlags;              // +0x10 T: QohFrameMoveFlags (1 = load x accel/jerk, 2 = load y accel/jerk, 4 = on landing jump to frame landFrame)
 unsigned char clearVelX;              // +0x11 T: non-zero zeroes actor.velX
 unsigned char clearVelY;              // +0x12 T: non-zero zeroes actor.velY
 unsigned char xAccelAbsolute;         // +0x13 T: copied to actor.xAccelAbsolute (non-zero: velX is clamped to a sign change instead of free integration)
 unsigned char yAccelAbsolute;         // +0x14 T/D: copied to actor.yAccelAbsolute; 0 in all files
 unsigned char tileXform;              // +0x15 T: QohTileXform (D: values 0..7)
 unsigned char landFrame;              // +0x16 T: frame index entered on landing when moveFlags & 4 (actor.nextFrameIndex = this, else 0xFFFE)
 unsigned char bodyBoxIndex;           // +0x17 T: body (push) box, offset from action.firstBox; used for screen-edge / push alignment (CalculateCollisionBounds)
 unsigned char bodyBoxMode;            // +0x18 T: 0 or 0xFF = no body box (the actor is skipped by the camera / push code); otherwise 1 (D: 31383 of 40452 frames are 1)
 unsigned char hurtBoxFirst;           // +0x19 T: first vulnerable box, offset from action.firstBox
 unsigned char hurtBoxCount;           // +0x1A T: number of vulnerable boxes; 0 = cannot be hit
 unsigned char unused_1B;              // +0x1B D: 0 in all frames of all files, no reader
 unsigned char unused_1C;              // +0x1C D: 0 in all frames of all files, no reader
 unsigned char guardBoxFirst;          // +0x1D T: first projectile-guard box, offset from action.firstBox (CheckProjectileVsGuardBoxCollision)
 unsigned char guardBoxCount;          // +0x1E T: number of guard boxes; non-zero sets contactFlags |= 2 and guardHitMask = 0xFF
 unsigned char attackFirst;            // +0x1F T: first attack record, offset from action.firstAttack
 unsigned char attackCount;            // +0x20 T: number of attack records; 0 = none; 0xFF = special hit 1 (returns -1), 0xFE = special hit 2 (ProcessCharacterHitDetectionAndDamage)
 unsigned char stanceBits;             // +0x21 T: copied whole to actor.stanceBits (low 3 bits = stance class, 0x40 = turn toward the nearest opponent, 0x20/0x80 read by the hit code)
 short xAccel;                         // +0x22 T: actor.accelX when moveFlags & 1 (negated when facing left)
 short yAccel;                         // +0x24 T: actor.accelY when moveFlags & 2
 short xJerk;                          // +0x26 T: actor.jerkX when moveFlags & 1
 short yJerk;                          // +0x28 T: actor.jerkY when moveFlags & 2
 unsigned char unused_2A;              // +0x2A D: 0xCD (build fill) in 26694 frames, else 0; no reader
 unsigned char unused_2B;              // +0x2B D: same as unused_2A
 int aiMoveClass;                      // +0x2C T: copied to actor.aiMoveClass, read by the CPU player (D: 0..11 observed)
 unsigned int fxFlags;                 // +0x30 T: QohFrameFxFlags; byte +0x31 is additionally read as a second sound id (QueueNetworkEventAndSound)
 unsigned int drawClass;               // +0x34 T: & 3 stored in actor.drawClass (D: 0, 1 or 3)
 unsigned char flickerFlags;           // +0x38 T: bit 0 = draw only every second tick (RenderCharacterSprite) (D: 0 or 1)
 unsigned char unused_39[7];           // +0x39 D: 0 in all frames of all files, no reader
};
struct QohBox {                 // 8 bytes, boxCount of them (body / hurt / guard boxes), inclusive rectangle relative to the actor position; facing left mirrors it inside a 256-wide canvas (CalculateCollisionBounds 0x462AC0)
 short left;                           // +0x00 T
 short top;                            // +0x02 T
 short right;                          // +0x04 T
 short bottom;                         // +0x06 T: D: right/bottom ranges are pixel offsets, bottom up to 215+
};
struct QohAttack {                 // 24 bytes, attackCount of them: one attack box plus its hit data (CalculateProjectileHitbox 0x462B70, ProcessCharacterHitDetectionAndDamage 0x466770)
 short left;                           // +0x00 T
 short top;                            // +0x02 T
 short right;                          // +0x04 T
 short bottom;                         // +0x06 T
 short damage;                         // +0x08 T: life removed (<= 0: no damage hit)
 short unresolved_0A;                  // +0x0A U/D: 34 non-zero values (1, 3, 4, 5, 10); no reader found
 short guardDamage;                    // +0x0C T: subtracted from the defender guard gauge on a guarded hit (ProcessCharacterBlockAndCounterAttack)
 short meterGain;                      // +0x0E T: passed to ManagePlayerSuperMeterGauge (attacker super gauge gain)
 unsigned char flags;                  // +0x10 T: QohAttackFlags (bit 0 = damaging hit; tested with hitClass 10/11 when the defender is in state 1)
 unsigned char guardMask;              // +0x11 T: bit0/1/2 = which guard postures (defender state 7.. variants) may block it (ProcessCharacterBlockAndCounterAttack)
 unsigned char hitClass;               // +0x12 T: 0..15; 0..9 and 12 select the defender reaction pose, 10 and 11 special (action 36 / 35), 13..15 projectile clash rules
 unsigned char unresolved_13;          // +0x13 U/D: 0 in all but one record (value 68); no reader found
 unsigned short knockMode;             // +0x14 T: bits 0..1 -> defender.hitFacingMode (1 = turn the defender away); bit 2 / bit 3 tested in the hit code; the whole word is returned to the caller
 unsigned short hitEffectFlags;        // +0x16 T: bits 12..15 = screen flash type (SetScreenFlashEffectByHitType); bit 0 set in 3000 of 4733 records (U)
};
struct QohCharAssets {                 // 48 bytes = the 12 dwords filled by LoadAndDecryptCharacterFile = the first 0x30 bytes of every actor (copied by value into spawned actors)
 struct QohTile * tiles;               // +0x00 T: 260 * tileCount
 int tileCount;                        // +0x04 T: header c1
 struct QohImageRecord * images;       // +0x08 T: 12 * imageCount
 int imageCount;                       // +0x0C T: header c0
 struct QohAction * actions;           // +0x10 T: 24 * actionCount
 int actionCount;                      // +0x14 T: header c2
 struct QohFrame * frames;             // +0x18 T: 64 * frameCount
 int frameCount;                       // +0x1C T: header c3
 struct QohBox * boxes;                // +0x20 T: 8 * boxCount
 int boxCount;                         // +0x24 T: header c4
 struct QohAttack * attacks;           // +0x28 T: 24 * attackCount
 int attackCount;                      // +0x2C T: header c5
};
struct QohTrailEntry {                 // 36 bytes: one entry of the 30-entry pose history (shifted by 1 each tick while stateFlags & 0x10000 is set, ProcessCharacterAnimationAndState 0x41A540); entries 14 and 28 are read back by the tag partner
 int posX;                             // +0x00 T
 int posY;                             // +0x04 T
 int actionIndex;                      // +0x08 T
 int frameIndex;                       // +0x0C T
 int frameTimer;                       // +0x10 T
 unsigned char facing;                 // +0x14 T
 unsigned char stanceBits;             // +0x15 T
 short impulseX;                       // +0x16 T: restored to actor+108 by the partner copy
 short impulseY;                       // +0x18 T: actor+110
 short impulseAccelX;                  // +0x1A T: actor+112
 short impulseAccelY;                  // +0x1C T: actor+114
 short impulseJerkX;                   // +0x1E T: actor+116
 short impulseJerkY;                   // +0x20 T: actor+118
 unsigned char unresolved_22[2];       // +0x22 U: not accessed
};
struct QohCommandChild {                 // 0x200 bytes: the input-history object at actor+164 (UpdatePlayerInputHistoryRing 0x47BF90). Rings are edge triggered: the head advances only when the masked value changes
 unsigned char unresolved_00[4];        // +0x00 U: not accessed by any traced code path (see doc)
 int resetMarker;                      // +0x04 T: set to -1 when the actor buffer is re-initialised (InitializeCharacterBuffer)
 unsigned int dirRingValues[32];       // +0x08 T: direction history, masked & 0xF0000
 unsigned int dirRingHold[32];         // +0x88 T: hold counters
 unsigned char dirRingHead;            // +0x108 T: index, & 0x1F
 unsigned char unresolved_109[3];        // +0x109 U: not accessed by any traced code path (see doc)
 unsigned int btnRingValues[16];       // +0x10C T: button history, masked & 0xF
 unsigned int btnRingHold[16];         // +0x14C T: hold counters
 unsigned char btnRingHead;            // +0x18C T: index, & 0xF
 unsigned char unresolved_18D[15];        // +0x18D U: not accessed by any traced code path (see doc)
 short resetMarker2;                   // +0x19C T: set to -1 when the actor buffer is re-initialised
 unsigned char unresolved_19E[18];        // +0x19E U: not accessed by any traced code path (see doc)
 int directionKill;                    // +0x1B0 T: suppresses recorded directions across a native round transition; cleared on re-initialisation
 unsigned char unresolved_1B4[76];        // +0x1B4 U: not accessed
};
struct QohActor {                 // 1532 (0x5FC) bytes: fighter, partner, projectile (120 at 0x526FA0), helper (30 at 0x618160), effect (30 at 0x8719E0) and the Obj1 / rina template all share it. P1..P4 battle data at 0x623E00 / 0x626880 / 0x4ECB40 / 0x617A20, character-state data at 0x614060 + n*0x5FC
 struct QohCharAssets assets;          // +0x00 T: loaded .chr tables (pointers masked by the savestate)
 int actionIndex;                      // +0x30 T: current action id (index into assets.actions)
 int nextFrameIndex;                   // +0x34 T: frame to enter when frameTimer reaches 0; 65534 = hold the last frame (UpdateCharacterLogic marks animEnded)
 int frameTimer;                       // +0x38 T: ticks left in the frame, set from frame.duration; HandleCharacterMovement steps the animation when 0
 int unresolved_3C;                    // +0x3C U: not accessed
 int frameIndex;                       // +0x40 T: current frame inside the action (frame = assets.frames[action.firstFrame + frameIndex])
 int unresolved_44;                    // +0x44 U: not accessed
 int requestedAction;                  // +0x48 T: action the input state machine wants (written with requestFlag)
 int requestFlag;                      // +0x4C T: 1 = a transition request is pending
 int unresolved_50;                    // +0x50 U
 int dispatchAction;                   // +0x54 T: copy of the forced action taken by UpdateCharacterAnimation
 int animEnded;                        // +0x58 T: -1 reset marker, 1 = the action ran out (UpdateCharacterLogic)
 int forcedAction;                     // +0x5C T: action forced by the hit / engine code, -1 = none
 short velX;                           // +0x60 T: 8.8 fixed point, integrated by UpdateCharacterLogic
 short velY;                           // +0x62 T
 short accelX;                         // +0x64 T: added to velX each tick; loaded from frame.xAccel
 short accelY;                         // +0x66 T: loaded from frame.yAccel
 short jerkX;                          // +0x68 T: added to accelX; loaded from frame.xJerk
 short jerkY;                          // +0x6A T: loaded from frame.yJerk
 short impulseX;                       // +0x6C T: second x group (knock back); cleared by the hit code
 short impulseY;                       // +0x6E T
 short impulseAccelX;                  // +0x70 T
 short impulseAccelY;                  // +0x72 T
 short impulseJerkX;                   // +0x74 T
 short impulseJerkY;                   // +0x76 T
 short slideVelX;                      // +0x78 T: third x group with a countdown
 short unresolved_7A;                  // +0x7A U: not touched by UpdateCharacterLogic
 short slideAccelX;                    // +0x7C T
 short unresolved_7E;                  // +0x7E U
 short slideJerkX;                     // +0x80 T
 short unresolved_82;                  // +0x82 U
 short slideTimer;                     // +0x84 T: decremented each tick while non-zero
 short unresolved_86;                  // +0x86 U
 int posX;                             // +0x88 T: world x of the anchor; boxes are relative to it
 int posY;                             // +0x8C T: world y
 int unresolved_90;                    // +0x90 U
 int unresolved_94;                    // +0x94 U
 int unresolved_98;                    // +0x98 T: written by AutoFlipCharacterFacing, meaning unresolved
 unsigned char stanceBits;             // +0x9C T: frame.stanceBits copy (low nibble posture, bits 4..7 behaviour)
 unsigned char reactionHeight;         // +0x9D T: 0/1/2 written by the hit code (high / mid / low reaction)
 unsigned short attackLevel;           // +0x9E T: frame.attackLevelAndFlags & 0xF
 unsigned short contactFlags;          // +0xA0 T: bit0 = attack boxes armed (THE active latch, set from frame bit 0x10, survives later frames), bit1 = guard boxes present, bit4 set by the hit code
 unsigned char attackHitMask;          // +0xA2 T: bit n = player slot n may still be hit by the current attack; reset to 0xFF when the attack arms
 unsigned char guardHitMask;           // +0xA3 T: same for guard boxes
 struct QohCommandChild * commandChild; // +0xA4 T: input history object
 unsigned char xAccelAbsolute;         // +0xA8 T: frame.xAccelAbsolute
 unsigned char yAccelAbsolute;         // +0xA9 T: frame.yAccelAbsolute
 unsigned char facing;                 // +0xAA T: 0 = faces right, 1 = faces left
 unsigned char team;                   // +0xAB T: owning team / EX owner index (NOT the slot)
 unsigned char playerSlot;             // +0xAC T: player slot 0..3
 unsigned char playerSlotCopy;         // +0xAD T: equals playerSlot after InitializeCharacterBuffer
 unsigned char entityKind;             // +0xAE T: 0xFF = free pool slot; otherwise the state machine selector of InitializeCharacterAnimationFrame
 unsigned char paletteEntryBase;       // +0xAF T: palette entry index of the actor 16-colour bank (added to paletteTable as 4 * value by the renderers; saved/restored across the actor reset)
 unsigned char hitAttrib;              // +0xB0 T: set to 9 by the hit code
 unsigned char hitDirection;           // +0xB1 T: relative direction of the last hit
 unsigned char unresolved_B2[2];        // +0xB2 U: not accessed by any traced code path (see doc)
 int unresolved_B4;                    // +0xB4 T: ProcessCharacterAttackSequence
 int unresolved_B8;                    // +0xB8 T
 int unresolved_BC;                    // +0xBC T
 int unresolved_C0;                    // +0xC0 T
 int unresolved_C4;                    // +0xC4 T: 17 written by ProcessCharacterCollisionAndStates
 int unresolved_C8;                    // +0xC8 T
 unsigned char stateNibbles;           // +0xCC T: low/high nibble state bits (&0xF0 / &0xF), cleared with stateFlags
 unsigned char unresolved_CD[3];        // +0xCD U: not accessed by any traced code path (see doc)
 unsigned int stateFlags;              // +0xD0 T: 0x10000 = capture a pose snapshot into trail[0]; low byte cleared by the hit code
 int tagTimer;                         // +0xD4 T: counts down while the partner is tagged in (clears stateFlags bit 8 at 0)
 struct QohTrailEntry trail[30];       // +0xD8 T: pose history
 int pendingHitDamage;                 // +0x510 T: damage event consumed (and cleared) at the end of ProcessCharacterAnimationAndState
 int lastHitPacked;                    // +0x514 T: (hitTag << 4) | stance bits & 7 of the last hit
 int lastHitStance;                    // +0x518 T: defender stance bits & 7 at the last hit
 int lastHitSlot;                      // +0x51C T: player slot of the attacker of the last hit
 int prevPosX;                         // +0x520 T: previous x (ResolveCharacterPushCollision)
 int unresolved_524;                   // +0x524 T: InitializeSpecialSuperState165
 int unresolved_528;                   // +0x528 T: 0/1 flag written by spawn code
 unsigned int updateCallback;          // +0x52C T: code pointer, set to UpdateProjectileFromParentCharacter for parented projectiles
 int unresolved_530;                   // +0x530 T
 unsigned char unresolved_534[4];        // +0x534 U: not accessed by any traced code path (see doc)
 int hitPendingFlags;                  // +0x538 T: bit0 = normal hit pending, bit1 = guarded hit pending (HandleCharacterMovement)
 int unresolved_53C;                   // +0x53C T
 int unresolved_540;                   // +0x540 T: 4800 written by the Akari special state
 unsigned char unresolved_544[4];        // +0x544 U: not accessed by any traced code path (see doc)
 int unresolved_548;                   // +0x548 T: 10 written by the transition code
 unsigned char unresolved_54C[4];        // +0x54C U: not accessed by any traced code path (see doc)
 int hitFacingMode;                    // +0x550 T: attack.knockMode & 3 of the last hit (1 = turn away, 2 = zero the knock back)
 int roundEndFlag;                     // +0x554 T
 int roundLostFlag;                    // +0x558 T
 unsigned char unresolved_55C[4];        // +0x55C U: not accessed by any traced code path (see doc)
 int fatalCause;                       // +0x560 T: -1 at round start
 unsigned char unresolved_564[4];        // +0x564 U: not accessed by any traced code path (see doc)
 unsigned char unresolved_568;         // +0x568 T
 unsigned char unresolved_569;         // +0x569 T
 unsigned char landingMode;            // +0x56A T: 0 = normal landing at y == 0, 2 = KO bounce test (UpdateCharacterLogic)
 unsigned char unresolved_56B[1];        // +0x56B U: not accessed by any traced code path (see doc)
 unsigned short unresolved_56C;        // +0x56C T: non-zero stops projectile reflection
 unsigned short wallFlags;             // +0x56E T: screen-edge contact flags (UpdateCameraPositionWithClampingAndSmoothing)
 int lastDamageResult;                 // +0x570 T: result of CalculateDamageWithModifiers stashed on the defender
 int unresolved_574;                   // +0x574 T
 int engineOwned;                      // +0x578 T: != 0 means the engine owns this actor
 int facingSnapshot;                   // +0x57C T: facing copy compared on the next frame
 int actionSnapshot;                   // +0x580 T
 int frameSnapshot;                    // +0x584 T
 struct QohPaletteEntry * paletteTable; // +0x588 T: the player palette (256 entries, g_actor_palette_p1..p4) the renderers index with paletteEntryBase
 unsigned char unresolved_58C[4];        // +0x58C U: not accessed by any traced code path (see doc)
 unsigned short pendingDamageBonus;    // +0x590 T: added to pendingHitDamage
 unsigned short pendingDamagePenalty;  // +0x592 T: subtracted from pendingHitDamage
 int subState;                         // +0x594 T: state machine sub step, written by the Initialize*State functions
 struct QohFrame * currentFrame;       // +0x598 T: frame pointer recomputed by HandleCharacterMovement every tick
 struct QohActor * lastContactActor;   // +0x59C T: the other party of the last hit (attacker for a defender, victim for an attacker)
 int unresolved_5A0;                   // +0x5A0 T
 int unresolved_5A4;                   // +0x5A4 T
 int unresolved_5A8;                   // +0x5A8 T
 int unresolved_5AC;                   // +0x5AC T
 int formMode;                         // +0x5B0 T: 1/2 = alternate form participating (hit code treats the partner as part of the actor), 2/3/4 per adapter
 unsigned char unresolved_5B4[4];        // +0x5B4 U: not accessed by any traced code path (see doc)
 int unresolved_5B8;                   // +0x5B8 T: set to 1 by the hit code
 int unresolved_5BC;                   // +0x5BC T: set to 1 by the hit code
 unsigned char unresolved_5C0[4];        // +0x5C0 U: not accessed by any traced code path (see doc)
 int unresolved_5C4;                   // +0x5C4 T: bit 1 halves incoming damage in the hit code
 int unresolved_5C8;                   // +0x5C8 T: set to 1 by ProcessCharacterAnimationAndState
 int drawClass;                        // +0x5CC T: frame.drawClass & 3
 int unresolved_5D0;                   // +0x5D0 T: 3 at spawn
 int unresolved_5D4;                   // +0x5D4 T
 int unresolved_5D8;                   // +0x5D8 T
 int hitstopFrames;                    // +0x5DC T: while non-zero the pose timers are frozen
 int unresolved_5E0;                   // +0x5E0 T
 unsigned char unresolved_5E4[8];        // +0x5E4 U: not accessed by any traced code path (see doc)
 int ownerSlotCache;                   // +0x5EC T: slot index of the owning player (ProcessCharacterCollisionAndStates)
 int unresolved_5F0;                   // +0x5F0 T: 1 at spawn
 int aiMoveClass;                      // +0x5F4 T: frame.aiMoveClass copy
 int terminalReady;                    // +0x5F8 T: written 0/1 by the transition code
};
struct QohFobFunc {                 // 36 bytes, nFuncs of them: a named script entry point
 char name[32];                        // +0x00 T/D: NUL terminated; the bytes after the NUL are uninitialised build garbage (D: "1", "Mizuho_Idou", "Mizuho_Idou2")
 unsigned int entryPc;                 // +0x20 T: byte offset into the code block (D: always an instruction start)
};
struct QohScriptBank {                 // 272 bytes, 20 slots at 0x62D640 (g_script_banks); LoadAndCacheScriptFile 0x460900 fills one slot per com_<name>.Fob
 char path[260];                       // +0x00 T: upper-cased path used as the cache key
 unsigned char * code;                 // +0x104 T: code block (codeSize + 2 zero bytes); 0 = slot free
 int funcCount;                        // +0x108 T: nFuncs
 struct QohFobFunc * funcs;            // +0x10C T: 36 * nFuncs
};
struct QohImgHeader {                 // 20 bytes at the start of every .Img; followed by 4*paletteEntries palette bytes and the pixel rows (LoadRawImageFromFile 0x45CB20)
 unsigned int reserved;                // +0x00 T/D: read and (optionally) returned, 0 in all 553 files
 unsigned int paletteEntries;          // +0x04 T: 4 bytes each (blue, green, red, 0); 0 = 24-bit image
 unsigned int bitsPerPixel;            // +0x08 T/D: 4, 8 or 24 (24 only with paletteEntries == 0)
 unsigned int width;                   // +0x0C T
 unsigned int height;                  // +0x10 T
};
struct QohFobVmHead {                 // 16 bytes at 0x62D630 (g_fob_vm_head), directly before the script banks
 unsigned char unresolved_00[4];        // +0x00 U: not accessed by any traced code path (see doc)
 int currentLine;                      // +0x04 T: written by opcode 0x3C (source line marker), printed in the "ScriptUnknown Code" error
 unsigned char unresolved_08[8];        // +0x08 U: not accessed
};
struct QohFobVmState {                 // 32 bytes at 0x62EB80 (g_fob_vm_state), directly after the 20 script banks; the value stack follows at 0x62EBA0 and the program counter at 0x66EBA0
 int stackDepth;                       // +0x00 T: index of the top of the value stack (0 = empty; pushes pre-increment; limit 0x10000)
 int activeBank;                       // +0x04 T: index of the running QohScriptBank
 unsigned char * codeBase;             // +0x08 T: code of the running bank; the pc is a byte offset into it
 unsigned char unresolved_0C[20];        // +0x0C U: not accessed
};
