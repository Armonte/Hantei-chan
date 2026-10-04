// RboActor part 2 (+0x200..+0x4FF) nested types, IDA-parsable (idc.parse_decls). Evidence: actor_part2.md.
// Prefix RboP2 / RBOP2_ keeps the names unique in the shared IDB. Offsets in comments are relative to the start of the containing type.
enum RboP2SlotKind : int { RBOP2_SLOT_CONST_VEL=0, RBOP2_SLOT_VELOCITY=1, RBOP2_SLOT_VELOCITY_LIMITED=2, RBOP2_SLOT_EASE_PAIR=3, RBOP2_SLOT_FOLLOW_TRAIL=4 };
enum RboP2SlotLimitFlags : unsigned int { RBOP2_LIMIT_X_ACTIVE=1, RBOP2_LIMIT_Y_ACTIVE=2 };
enum RboP2AutoMoveMode : unsigned int { RBOP2_AMX_NONE=0, RBOP2_AMX_UNTIL_POS_REACHES_FROM_BELOW=1, RBOP2_AMX_UNTIL_POS_REACHES_FROM_ABOVE=2, RBOP2_AMX_SNAP=3, RBOP2_AMX_CLAMP_RANGE=4, RBOP2_AMX_UNTIL_SCREEN_LEFT_PLUS=5, RBOP2_AMX_UNTIL_SCREEN_RIGHT_PLUS=6, RBOP2_AMY_UNTIL_POS_LE_TARGET=0x100, RBOP2_AMY_UNTIL_POS_GE_TARGET=0x200, RBOP2_AMY_SNAP=0x300 };
enum RboP2DrawOffsetMode : unsigned char { RBOP2_DRAWOFS_NONE=0, RBOP2_DRAWOFS_INTERPOLATED=1 };
enum RboP2ColorFlashMode : unsigned char { RBOP2_FLASH_BLINK=0, RBOP2_FLASH_FADE=1 };
enum RboP2RepeatFlags : unsigned int { RBOP2_REPEAT_RECORD_PENDING=2, RBOP2_REPEAT_RESET_PENDING=0x10 };
struct RboP2Ramp {
 int velocity;                          // +0x00 Ramp_Step 0x449210: added to value each tick
 int accel;                             // +0x04 added to velocity each tick when non-zero
 int velocityLimit;                     // +0x08 clamp for velocity (sign selects min/max); step returns 0x10/0x20 on clamp
 int value;                             // +0x0C 24.8 fixed point accumulator; step returns 1/2 when it hits valueLimit
 int valueLimit;                        // +0x10 clamp for value
};
struct RboP2RampFx {
 void *update;                          // +0x00 sub_4496E0 (no-op, returns 0) or RampFx_Step 0x4491E0
 int value;                             // +0x04 integer output = ramp.value / 256 (alpha 0..255 for the alpha fade)
 struct RboP2Ramp ramp;                 // +0x08 AlphaFadeFx_StartRamp 0x449280 -> Ramp_Init 0x4490C0 at +8
};
struct RboP2ZoomFx {
 void *update;                          // +0x00 no-op sub_4496E0 or ZoomFx_Step 0x449810
 void *getXY;                           // +0x04 getter(this, &x, &y): sub_449680 (static) / sub_4497E0 (ramped); Actor_DrawTree calls it through actor+780
 int valueX;                            // +0x08 ramped X value (ramp.value/256)
 int valueY;                            // +0x0C ramped Y value
 struct RboP2Ramp rampX;                // +0x10 ZoomFx_Init 0x4496A0 stores the static zoom (256 = 1.0) in rampX.velocity (+0x10)
 struct RboP2Ramp rampY;                // +0x24 ZoomFx_StartRamp 0x449840 second ramp
};
struct RboP2RgbFx {
 void *update;                          // +0x00 sub_4493A0 (returns 1) or RgbFx_Step 0x4493D0 (steps the 3 ramps)
 int unused_04;                         // +0x04 no reader found
 int rgb[3];                            // +0x08 current R,G,B 0..255 (RgbFx_Init 0x4493B0; read by sub_4464D0 / sub_446350)
 struct RboP2Ramp ramp[3];              // +0x14 per-channel ramps (sub_449450 / sub_4495A0), ramp[i].value/256 -> rgb[i]
};
struct RboP2RotFx {
 int angleDeg;                          // +0x00 output angle in degrees = (accum % 92160 + base) / 256 (RotFx_Step 0x449B60); read by Actor_DrawTree
 void *update;                          // +0x04 sub_4496E0 (static) or RotFx_Step 0x449B60
 int baseAngle;                         // +0x08 8.8 fixed (RotFx_InitStatic 0x449AB0 stores the static angle here and in +0x00)
 int accum;                             // +0x0C accumulated rotation, wrapped at 92160 (=360*256)
 int speedLimitEnabled;                 // +0x10 1 when angVelLimit applies (RotFx_StartSpin 0x449AD0 flag 0x40)
 int angVel;                            // +0x14 added to accum each tick
 int angAccel;                          // +0x18 added to angVel each tick
 int angVelLimit;                       // +0x1C clamp for angVel when speedLimitEnabled
};
struct RboP2FlickerGate {
 int phase;                             // +0x00 FlickerGate_Tick 0x440DC0: (phase+1) % period each draw
 int period;                            // +0x04 0 = always visible
 int visibleTicks;                      // +0x08 actor is drawn while phase < visibleTicks
 int spare;                             // +0x0C FlickerGate_Init arg 4 (Actor_AdvanceByAniFlag passes 3), never read by the tick
};
struct RboP2ColorFlash {
 unsigned char mode;                    // +0x00 RboP2ColorFlashMode: 0 blink every blinkPeriod ticks, 1 fade out over total (Actor_GetFlashAddColor 0x446350)
 unsigned char elapsed;                 // +0x01 ticks since start, ++ by sub_440550 while remaining != 0
 unsigned char blinkPeriod;             // +0x02 2 in blink mode
 unsigned char remaining;               // +0x03 ticks left, -- by sub_440550; 0 = no flash
 unsigned char total;                   // +0x04 initial remaining (fade denominator)
 unsigned char unused_05[3];            // +0x05 pad, never read
 int addR;                              // +0x08 additive tint, table byte_48AF04[3*idx] (Actor_StartColorFlash 0x4487A0)
 int addG;                              // +0x0C
 int addB;                              // +0x10
};
struct RboP2MotionState {
 int velX;                              // +0x00 MoverVec_StepAccel 0x44B5B0 does velX += accelX; MoverVec_ApplyToPosition adds it to subPixelX
 int velY;                              // +0x04
 int accelX;                            // +0x08 added to velX each tick (Actor_FrameMoveAddSpeed 0x44B3D0 edits velX/accelX/velY/accelY)
 int accelY;                            // +0x0C
 int subPixelX;                         // +0x10 MoverVec_ApplyToPosition 0x44B5D0: pos += (sub + vel)/128, sub %= 128
 int subPixelY;                         // +0x14
};
struct RboP2EaseChannel {
 int start;                             // +0x00 sub_4600D0: value = start + sin(step*90deg/duration) * (end-start)
 int end;                               // +0x04
 int step;                              // +0x08 current step; sub_460140 moves it by 1 toward stepTarget each tick
 int duration;                          // +0x0C
 int value;                             // +0x10 result written by sub_460110
 int aux;                               // +0x14 only copied by sub_44B4D0, no other reader
 int stepTarget;                        // +0x18
};
struct RboP2SlotBodyConst {
 int velX;                              // +0x00 sub_44B560 adds to posX each tick
 int velY;                              // +0x04
 int unused_tail[12];                   // +0x08 not touched by kind 0
};
struct RboP2SlotBodyVelocity {
 struct RboP2MotionState state;         // +0x00 Actor_GetVelocityBlock 0x44B370 returns slot+20 for kind 1
 int unused_tail[8];                    // +0x18 not touched by kind 1
};
struct RboP2SlotBodyLimited {
 unsigned int limitFlags;               // +0x00 RboP2SlotLimitFlags, Mover_InitLimitedAccel 0x44B7C0 arg a7
 int xAccelSign;                        // +0x04 sign of state.accelX (1/-1/0) selects which side of xSpeedLimit stops
 int xSpeedLimit;                       // +0x08 MoverKind2_StepLimitedAccel 0x44B720: velX clamped to it, then accelX=0 and flag cleared
 int yAccelSign;                        // +0x0C
 int ySpeedLimit;                       // +0x10
 struct RboP2MotionState state;         // +0x14 Actor_GetVelocityBlock returns slot+40 for kind 2
 int unused_tail[3];                    // +0x2C not touched by kind 2
};
struct RboP2SlotBodyEasePair {
 struct RboP2EaseChannel chanX;         // +0x00 MoverKind3_StepGaugePair 0x44B480 steps slot+20 and writes posX += chanX.value delta
 struct RboP2EaseChannel chanY;         // +0x1C slot+48
};
struct RboP2SlotBodyFollowTrail {
 int trailEntryIndex;                   // +0x00 MoverKind4_StepPathFollow 0x44B860 passes it to sub_44ACA0 with trail
 int outX;                              // +0x04 pos written from here (a3+24)
 int outY;                              // +0x08
 void *trail;                           // +0x0C afterimage trail (actor+1160 of the followed actor)
 unsigned int clampFlags;               // +0x10 bit0 clamp X distance to maxDX, bit1 clamp Y to maxDY
 int maxDX;                             // +0x14
 int maxDY;                             // +0x18
 int unused_tail[7];                    // +0x1C not touched by kind 4
};
union RboP2SlotBody {
 struct RboP2SlotBodyConst constVel;            // +0x00 kind 0
 struct RboP2SlotBodyVelocity velocity;         // +0x00 kind 1
 struct RboP2SlotBodyLimited limited;           // +0x00 kind 2
 struct RboP2SlotBodyEasePair easePair;         // +0x00 kind 3
 struct RboP2SlotBodyFollowTrail followTrail;   // +0x00 kind 4
};
struct RboP2MotionSlot {
 int unused_hdr[3];                     // +0x00 no actor-pointer access at the absolute displacement and no slot function touches [0..2]
 void *update;                          // +0x0C update(&posX,&posY,slot): Actor_StepMotionMovers 0x44BFF0 / Actor_StepMotionWithTimedSlot 0x4404F0 / sub_440550 call it
 enum RboP2SlotKind kind;               // +0x10 Actor_GetVelocityBlock 0x44B370 selects the body overlay on it
 union RboP2SlotBody body;              // +0x14 56-byte kind-dependent payload
};
struct RboP2AutoMove {
 unsigned int mode;                     // +0x00 RboP2AutoMoveMode: low byte = X mode, byte 1 = Y mode (Actor_StepAutoMove 0x44BBD0)
 int targetX;                           // +0x04 Actor_SetAutoMoveTargetX 0x4418B0 (also script op 7)
 int clampExtra;                        // +0x08 script op 7 type 1 record[7]; written only, cleared by Actor_MarkKilledAndCleanup
 int clampMin;                          // +0x0C range for mode 4
 int clampMax;                          // +0x10
 int arriveXPattern;                    // +0x14 -1 none; copied to pendingPattern (+0x528) on arrival
 int arriveXFrame;                      // +0x18 copied to pendingFrame (+0x52A)
 int arriveXFlip;                       // +0x1C copied to pendingFlip (+0x52C), bit 0x10 = also negate velocity
 int arriveXClear;                      // +0x20 1 = clear mode on arrival
 int targetY;                           // +0x24 script op 7 type 2
 int arriveYPattern;                    // +0x28
 int arriveYFrame;                      // +0x2C
 int arriveYClear;                      // +0x30
};
struct RboP2DrawOffsetKeys {
 __int16 startOffsetX;                  // +0x00 Actor_GetBoxOrigin 0x440B50 adds it to the box origin when drawOffsetMode != 0; Actor_DrawCgSprite uses it too
 __int16 startOffsetY;                  // +0x02
 __int16 startAuxA;                     // +0x04 interpolated into out pair a5[0] by Actor_GetDrawOffsetLerp 0x445C20
 __int16 startAuxB;                     // +0x06 a5[1]
 __int16 endOffsetX;                    // +0x08 lerp target, t = float at arg+12
 __int16 endOffsetY;                    // +0x0A
 __int16 endAuxA;                       // +0x0C
 __int16 endAuxB;                       // +0x0E
};
struct RboP2RepeatTracker {
 int lastIndex;                         // +0x00 index of newest history entry, -1 empty (RepeatTracker_Reset 0x4415A0)
 int windowParam;                       // +0x04 set to 60 for playerId 0 by Actor_InitRepeatBonus 0x441720; no reader found
 int scaleBonus;                        // +0x08 +500 for pattern 1/4/7 (Actor_InitRepeatBonus); used by Actor_ApplyRepeatPenalty 0x441760
 int penalty;                           // +0x0C Actor_UpdateRepeatPenalty 0x4415C0 adds 850 - 3*stat for every repeated pattern in history
 int lastJumpCounter;                   // +0x10 jumpCounter value of the last recorded move
 unsigned int flags;                    // +0x14 RboP2RepeatFlags
 int history[16];                       // +0x18 ring of recent pattern ids (+0x4E0..+0x51F)
};
