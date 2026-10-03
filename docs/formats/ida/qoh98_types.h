// Queen of Heart '98 (Qoh98.exe, 1998) character .dat / runtime types, IDA-parsable (idc.parse_decls). Source of truth for the container, the record types and the runtime fighter/object struct.
// Evidence for each field: docs/formats/qoh98.md. Tags in comments: T = access traced in Qoh98.exe.i64, D = verified on all shipped files (tools/fb/qoh98_verify.py), U = unproven.
// A field is named unused_<hexoffset> ONLY when it is constant (0 or 0xCD fill) in all 14 shipped .dat files (13 characters + System/obj/obj1.dat) and no reader exists. Runtime gaps that are read but not understood are named unresolved_<hexoffset>.
enum Qoh98TileXform : unsigned char { Q98XF_IDENTITY=0, Q98XF_ROT_90_CW=1, Q98XF_ROT_180=2, Q98XF_ROT_90_CCW=3, Q98XF_FLIP_H=4, Q98XF_TRANSPOSE=5, Q98XF_FLIP_V=6, Q98XF_ANTI_TRANSPOSE=7 };
enum Qoh98SpriteMoveFlags : unsigned char { Q98SMF_SET_X_ACCEL=1, Q98SMF_SET_Y_ACCEL=2, Q98SMF_ON_LAND_JUMP_TO_ANIM=4 };
enum Qoh98SpriteFxFlags : unsigned int { Q98FX_SOUND_SLOT_MASK=0xF, Q98FX_SOUND_BANK_MASK=0xF0, Q98FX_SPAWN_FX_A=0x10000, Q98FX_SPAWN_THROW_FX=0x20000, Q98FX_SET_SCREEN_MODE_4=0x40000, Q98FX_CLEAR_COMBO_STATE=0x80000, Q98FX_SPAWN_FX_B=0x200000 };
struct Qoh98DatHeader {                // 28 bytes at file start, read by LoadAndParseCharacterDATFile 0x44C290
 unsigned int version;                 // +0x00 T: read into a throwaway buffer, never compared; 5 in all 14 files
 unsigned int imageCount;              // +0x04 T: c0 = TIM path entries = image table entries
 unsigned int tileCount;               // +0x08 T: c1 = 260-byte tile blocks
 unsigned int animCount;               // +0x0C T: c2 = 24-byte animation records (256 in every file)
 unsigned int spriteCount;             // +0x10 T: c3 = 52-byte sprite (frame) records
 unsigned int bodyRectCount;           // +0x14 T: c4 = 8-byte body/hurt rectangles
 unsigned int hitBoxCount;             // +0x18 T: c5 = 24-byte attack boxes
};
struct Qoh98TimPathEntry {             // 260 bytes, imageCount of them; the loader reads and DISCARDS them (editor/provenance only)
 char shortName[8];                    // +0x00 D: NUL terminated "Aka0000", rest of the 8 bytes is 0xCD fill
 char sjisAndPath[252];                // +0x08 D: CP932 display text then the original build path ("...\AKARI\Aka0000.tim"), NUL then 0xCD fill; split not read by the game
};
struct Qoh98ImageTileRange {           // 8 bytes, imageCount of them; image i = TIM i of the path table
 int firstTile;                        // +0x00 T: first tile index of this image (ranges are contiguous and sum to tileCount) (sub_416EB0: *(a2[2] + 8*img))
 int tileCount;                        // +0x04 T: number of tiles drawn for this image (a2[2] + 8*img + 4)
};
struct Qoh98Tile {                     // 260 bytes, tileCount of them; blitted 16x16 by sub_415350 / sub_4160D0 (pixels = tile + 4)
 short x;                              // +0x00 T: destination column of the tile inside the 256x256 sprite canvas (multiple of 16, 0..240)
 short y;                              // +0x02 T: destination row (multiple of 16, 0..240)
 unsigned char pixels[256];            // +0x04 T: 16x16 palette indices, row-major, values 0..15 (index 0 = transparent) D
};
struct Qoh98AnimRecord {               // 24 bytes, animCount (256) of them; index = animation id (runtime entity +0x30)
 int firstSprite;                      // +0x00 T: index of the first sprite record, -1 = animation unused
 int frameCount;                       // +0x04 T: number of sprite records; the step code resets to frame 0 when frameCount < nextFrame (sub_419470) D: next anim firstSprite == firstSprite + frameCount in order
 int bodyRectBase;                     // +0x08 T: base index into the body rectangle table; sprite fields +0x17/+0x19/+0x1D are offsets from it; -1 when none
 int bodyRectCount;                    // +0x0C D: rectangle count of this animation (contiguous ranges); not read by the game
 int hitBoxBase;                       // +0x10 T: base index into the hit box table; sprite field +0x1F is an offset from it; -1 when none
 int hitBoxCount;                      // +0x14 D: box count of this animation (contiguous ranges); not read by the game
};
struct Qoh98Sprite {                   // 52 bytes, spriteCount of them: one animation frame (drawing + timing + motion + boxes)
 int imageIndex;                       // +0x00 T: index into the image table (a2[2]); out of range = draw nothing (sub_416EB0: v13 < 0 || a2[3] <= v13)
 short drawX;                          // +0x04 T: x offset of the 256x256 canvas relative to the entity position (negated when facing left)
 short drawY;                          // +0x06 T: y offset (sub_416EB0 divides it by 3 for the shadow pass)
 int duration;                         // +0x08 T: copied to entity +0x38 frameTimer (sub_419470)
 unsigned short nextFrame;             // +0x0C T: next frame index inside the animation; 0xFFFF = frame+1, 0xFFFE = animation ends (entity +0x34 = 0xFFFE)
 unsigned short attackLevelAndFlags;   // +0x0E T: low nibble -> entity +0x9E; bit 0x10 arms the hit boxes (entity flags |= 1)
 unsigned char moveFlags;              // +0x10 T: Qoh98SpriteMoveFlags (1 = apply x accel +0x22/+0x26, 2 = apply y accel +0x24/+0x28, 4 = on landing jump to anim +0x16)
 unsigned char clearVelX;              // +0x11 T: non-zero zeroes entity velX
 unsigned char clearVelY;              // +0x12 T: non-zero zeroes entity velY
 unsigned char xAccelAbsolute;         // +0x13 T: copied to entity +0xAC
 unsigned char yAccelAbsolute;         // +0x14 T: copied to entity +0xAD
 unsigned char tileXform;              // +0x15 T: Qoh98TileXform applied to the tile positions (xor 4 when facing left)
 unsigned char landAnim;               // +0x16 T: animation entered on landing when moveFlags & 4 (entity nextFrame = this, else 0xFFFE)
 unsigned char anchorRect;             // +0x17 T: body rectangle offset (from anim bodyRectBase) used to align the entity when it turns or leaves the screen (sub_409850, sub_40A780)
 unsigned char anchorMode;             // +0x18 T: 0xFF = no anchor (sub_409850 returns 0); 0/1/2 observed
 unsigned char hurtRectFirst;          // +0x19 T: first hurt body rectangle, offset from anim bodyRectBase
 unsigned char hurtRectCount;          // +0x1A T: number of hurt rectangles; 0 = invulnerable to hits (sub_40BB30)
 unsigned char unused_1B;              // +0x1B D: 0 in all 14 files, never read
 unsigned char unused_1C;              // +0x1C D: 0 in all 14 files, never read
 unsigned char grabRectFirst;          // +0x1D T: first grab-vulnerable body rectangle, offset from anim bodyRectBase (sub_40B9B0)
 unsigned char grabRectCount;          // +0x1E T: number of grab rectangles; non-zero sets entity flags |= 2
 unsigned char hitBoxFirst;            // +0x1F T: first attack box, offset from anim hitBoxBase
 unsigned char hitBoxCountHere;        // +0x20 T: number of attack boxes (0 = none)
 unsigned char stanceBits;             // +0x21 T: copied whole to entity +0x9C (low 3 bits = stance class, 0x40 = turn toward opponent via sub_40A780, 0x20/0x80 read by the hit code)
 short xAccel;                         // +0x22 T: entity +0x64 when moveFlags & 1 (negated when facing left)
 short yAccel;                         // +0x24 T: entity +0x66 when moveFlags & 2
 short xJerk;                          // +0x26 T: entity +0x68 when moveFlags & 1
 short yJerk;                          // +0x28 T: entity +0x6A when moveFlags & 2
 unsigned char unused_2A;              // +0x2A D: 0xCD in 7106 of 8256 sprites, else 0 (uninitialised build fill), never read
 unsigned char unused_2B;              // +0x2B D: same as +0x2A
 int aiMoveClass;                      // +0x2C T: read by the CPU player code (sub_404300 and 21 more) through *(a2[6] + 52*frame + 44); values 0..11 observed
 unsigned int fxFlags;                 // +0x30 T: Qoh98SpriteFxFlags: low nibble = sound slot (+16*side), bits 4..7 = second sound, high bits = effects (sub_419470)
};
struct Qoh98BodyRect {                 // 8 bytes, bodyRectCount of them (hurt / grab / anchor rectangles), coordinates relative to entity position (+0x88/+0x8C), inclusive
 short left;                           // +0x00 T: sub_409550 (mirrored as 256 - left when facing left)
 short top;                            // +0x02 T
 short right;                          // +0x04 T
 short bottom;                         // +0x06 T
};
struct Qoh98HitBox {                   // 24 bytes, hitBoxCount of them (attack boxes + projectile clash boxes), rectangle relative to entity position
 short left;                           // +0x00 T: sub_409630
 short top;                            // +0x02 T
 short right;                          // +0x04 T
 short bottom;                         // +0x06 T
 short damage;                         // +0x08 T: life removed (0 = no damage hit); stance 1 divides by 4, stance 2 multiplies by 125/100 (sub_40BF60)
 short unresolved_0A;                  // +0x0A D: 0..10 (999 of 1031 are 0); no reader found
 short guardDamage;                    // +0x0C T: subtracted from the defender's guard gauge (entity +0x380) on guard
 short meterGain;                      // +0x0E T: passed to sub_41C260 (super gauge gain of the attacker side)
 unsigned char flags;                  // +0x10 T: bit0 = damaging hit (affects knock back / hit stop)
 unsigned char defenderStateMask;      // +0x11 T: bit0/bit1/bit2 = may hit a defender in state 0x12 / 0x13 / 0x14 (sub_40BF60 case 0x12..0x14)
 unsigned char hitClass;               // +0x12 T: 0..15; 0..9 select the reaction anim (21..25/33/36), 10 and 11 special (anim 36 / 35), 13..15 projectile clash rules (sub_40A8B0)
 unsigned char unused_13;              // +0x13 D: 0 in all 14 files, never read
 unsigned short knockMode;             // +0x14 T: bits 0..1 -> defender entity +0x38C (1 = turn defender away); bit0 also tested in sub_40BF60
 unsigned char unresolved_16;         // +0x16 D: 0 or 1 (102 of 1031 boxes), no reader found
 unsigned char unused_17;              // +0x17 D: 0 in all 14 files
};
struct Qoh98CharData {                 // 48 bytes = the 12 dwords filled by the loader = first 0x30 bytes of every runtime entity
 struct Qoh98Tile *tiles;              // +0x00 T: 260 * tileCount
 int tileCount;                        // +0x04 T: header c1
 struct Qoh98ImageTileRange *images;   // +0x08 T: 8 * imageCount
 int imageCount;                       // +0x0C T: header c0
 struct Qoh98AnimRecord *anims;        // +0x10 T: 24 * animCount (a1[4])
 int animCount;                        // +0x14 T: header c2
 struct Qoh98Sprite *sprites;          // +0x18 T: 52 * spriteCount (a1[6])
 int spriteCount;                      // +0x1C T: header c3
 struct Qoh98BodyRect *bodyRects;      // +0x20 T: 8 * bodyRectCount (a1[8])
 int bodyRectCount;                    // +0x24 T: header c4
 struct Qoh98HitBox *hitBoxes;         // +0x28 T: 24 * hitBoxCount (a1[10])
 int hitBoxCount;                      // +0x2C T: header c5
};
struct Qoh98Entity {                   // 1012 (0x3F4) bytes: fighter prefix, projectile (60 at 0x4C5F70), effect (30 at 0x4D4ED0 and 30 at 0x498690) and the Obj1 template all share it
 struct Qoh98CharData data;            // +0x00 T: sprite/animation data of the owner (copied by value into spawned objects)
 int animId;                           // +0x30 T: current animation (index into data.anims)
 int nextFrame;                        // +0x34 T: frame that follows the current one; 0xFFFE = animation ends (sub_419470)
 int frameTimer;                       // +0x38 T: counts down from sprite.duration
 int unresolved_3C;                    // +0x3C not traced
 int frameIndex;                       // +0x40 T: current frame inside the animation (sprite = anim.firstSprite + frameIndex)
 int unresolved_44;                    // +0x44 not traced
 int unresolved_48;                    // +0x48 T: switch value in sub_40BF60 (0, 0x12, 0x13, 0x14); meaning not resolved
 int unresolved_4C;                    // +0x4C T: read/written by 98 sites; meaning not resolved
 int unresolved_50;                    // +0x50 not traced
 int unresolved_54;                    // +0x54 T: 50 sites, value 34 compared in the hit code
 int animEnded;                        // +0x58 T: set to 1 when the animation ran out (sub_419A10)
 int reactionAnim;                     // +0x5C T: reaction animation requested by the hit code (21..25, 33, 35, 36)
 short velX;                           // +0x60 T: 8.8 fixed point, added to posX/256 each tick (sub_419A10)
 short velY;                           // +0x62 T: velY
 short accelX;                         // +0x64 T: added to velX; loaded from sprite.xAccel
 short accelY;                         // +0x66 T: loaded from sprite.yAccel
 short jerkX;                          // +0x68 T: added to accelX; loaded from sprite.xJerk
 short jerkY;                          // +0x6A T: loaded from sprite.yJerk
 short pushVelX;                       // +0x6C T: knock back x velocity group (cleared on hit)
 short pushVelY;                       // +0x6E T
 short pushAccelX;                     // +0x70 T
 short pushAccelY;                     // +0x72 T
 short pushJerkX;                      // +0x74 T
 short pushJerkY;                      // +0x76 T
 short slideVelX;                      // +0x78 T: second x group with a countdown
 short unresolved_7A;                  // +0x7A not touched by sub_419A10
 short slideAccelX;                    // +0x7C T
 short unresolved_7E;                  // +0x7E not touched by sub_419A10
 short slideJerkX;                     // +0x80 T
 short unresolved_82;                  // +0x82 not touched by sub_419A10
 short slideTimer;                     // +0x84 T: decremented each tick while non-zero
 short unresolved_86;                  // +0x86 not touched
 int posX;                             // +0x88 T: world x, rectangles are relative to this
 int posY;                             // +0x8C T: world y
 int unresolved_90;                    // +0x90 not traced
 int unresolved_94;                    // +0x94 not traced
 int unresolved_98;                    // +0x98 T: 1 site
 unsigned char stanceBits;             // +0x9C T: sprite.stanceBits copy
 unsigned char reactionHeight;         // +0x9D T: 0/1/2 written by the hit code (high / mid / low)
 unsigned short attackLevel;           // +0x9E T: sprite.attackLevelAndFlags & 0xF
 unsigned short boxFlags;              // +0xA0 T: bit0 = hit boxes armed, bit1 = grab rectangles present
 unsigned char unresolved_A2[2];       // +0xA2 not traced
 void *controlBlock;                   // +0xA4 T: pointer used by 275 sites (input/controller block of the owner); *(ptr+408) and *(ptr+412) cleared when an animation ends
 int hitTaken;                         // +0xA8 T: set to 1 once the entity has been hit (sub_40AF70)
 unsigned char xAccelAbsolute;         // +0xAC T: sprite.xAccelAbsolute
 unsigned char yAccelAbsolute;         // +0xAD T: sprite.yAccelAbsolute
 unsigned char facing;                 // +0xAE T: 0 = faces right, 1 = faces left
 unsigned char side;                   // +0xAF T: owner player 0 or 1
 unsigned char kind;                   // +0xB0 T: 0xFF = free slot in the object pools; otherwise object/move kind
 unsigned char unresolved_B1;          // +0xB1 T: 6 sites
 unsigned char hitAttrib;              // +0xB2 T: set to 9 by the hit code (27 sites)
 unsigned char hitDirection;           // +0xB3 T: 0 / 3 / 1.. relative direction of the last hit
 unsigned char unresolved_B4[0x2C0];   // +0xB4 not resolved (fields around +0xD0..+0xE8 are read by the AI and hit code; +0x344..+0x370 hit-effect state)
 int damageTaken;                     // +0x374 T: accumulated by the hit code (+= damage); alive while unresolved life terms at +0x370/+0x378 minus this >= 0
 int unresolved_378;                   // +0x378 T: 3 sites, enters the life test
 int unresolved_37C;                   // +0x37C T: 7 sites (reduced by 6 on a guarded hit in some states)
 int guardGauge;                       // +0x380 T: decreased by hit.guardDamage
 unsigned char unresolved_384[0x4];    // +0x384
 int unresolved_388;                   // +0x388 T: set to 20 on a hit
 int knockMode;                        // +0x38C T: hit.knockMode & 3
 unsigned char unresolved_390[0x10];   // +0x390
 unsigned char edgeFlags;              // +0x3A0 T: bit0 set by Entity_ScreenEdgeCheck when the entity is pushed against the screen edge
 unsigned char unresolved_3A1[3];      // +0x3A1
 int lastHitResult;                    // +0x3A4 T: value returned by sub_40ACF0 (damage scaling result)
 unsigned char unresolved_3A8[8];      // +0x3A8 (+0x3A8 is read by 48 sites, meaning not resolved)
 int previousFacing;                   // +0x3B0 T: facing before the last turn (Entity_FlipFacingKeepAnchor)
 unsigned char unresolved_3B4[0x40];   // +0x3B4
};
