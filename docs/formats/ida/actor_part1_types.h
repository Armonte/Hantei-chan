// RboActor part 1 (bytes 0x000..0x1FF) helper types, IDA-parsable (idc.parse_decls). Must be parsed BEFORE RboActor.
// Evidence: docs/formats/ida/actor_part1.md. Mover = the 76-byte "motion block" Actor_StepMotionMovers (0x44BFF0) steps every tick.
enum RboMoverKind : int { RBOMV_CONST_OFFSET=0, RBOMV_VELOCITY=1, RBOMV_LIMITED_ACCEL=2, RBOMV_GAUGE_PAIR=3, RBOMV_PATH_FOLLOW=4 };
enum RboObjectKind : int { RBOOK_NONE=0, RBOOK_CHARACTER_BOUND=1, RBOOK_SPAWNED=2 };
enum RboActorClassFlags : unsigned int { RBOAC_NIBBLE_MASK=0xF, RBOAC_PLAYER_ENTITY=0x10, RBOAC_SPAWNED=0x20, RBOAC_INHERIT_MARK=0x40, RBOAC_NO_HITSKILL_100=0x100, RBOAC_NO_HITSKILL_20000=0x20000 };
enum RboShadowStyleFlags : unsigned int { RBOSH_TINT_FROM_PART=1, RBOSH_SKIP_ALPHA_PASS=2, RBOSH_ALPHA_OVERRIDE=4 };
enum RboInheritMask : unsigned int { RBOIM_X=1, RBOIM_Y=2, RBOIM_Z=4 };
struct RboMover;
struct RboDepthTween;
typedef int (__cdecl *RboMoverStepFn)(int *x, int *y, struct RboMover *self);
typedef int (__cdecl *RboDepthTweenStepFn)(int *depthOffset, int *z, struct RboDepthTween *self);
struct RboMoverVec {                       // 24 bytes; stepped by sub_44B5B0 (vel += acc) + sub_44B5D0 (frac += vel; pos += frac/128; frac %= 128)
 int velX;                                 // +0x00 sub_44B5B0 *a3 += a3[2]; Actor_FrameMoveAddSpeed 0x44B3D0 adds frame speedX
 int velY;                                 // +0x04 sub_44B5B0 a3[1] += a3[3]
 int accX;                                 // +0x08 Actor_FrameMoveAddSpeed adds frame accelX; Actor_ApplyEnterBrake writes -vel/20
 int accY;                                 // +0x0C
 int fracX;                                // +0x10 sub-pixel remainder, 1/128 px units (sub_44B5D0)
 int fracY;                                // +0x14
};
struct RboMoverConstOffset {               // payload of kind 0: sub_44B560 adds a fixed delta to x,y every tick
 int deltaX;                               // +0x00 sub_44B560 *a1 += a3[5]
 int deltaY;                               // +0x04 sub_44B560 *a2 += a3[6]
};
struct RboMoverLimitedAccel {              // payload of kind 2: velocity + "accelerate until velocity reaches limit" (sub_44B7C0 / sub_44B720)
 unsigned int limitAxes;                   // +0x00 bit0 = X limit armed, bit1 = Y limit armed (cleared when the limit is hit)
 int limitDirX;                            // +0x04 sign of accX at arm time (sub_44B7C0 a2[6])
 int limitX;                               // +0x08 velocity target on X (a2[7])
 int limitDirY;                            // +0x0C sign of accY at arm time (a2[8])
 int limitY;                               // +0x10 velocity target on Y (a2[9])
 struct RboMoverVec vec;                   // +0x14 stepped by sub_44B5B0/sub_44B5D0 (a2+10)
};
struct RboMoverGaugeAxis {                 // 28 bytes: ramp stepped by sub_460140 (cur steps +-1 toward target then sub_460110 recomputes out)
 int w0;                                   // +0x00 template copied by sub_44B4D0 (7 dwords, masked)
 int w1;                                   // +0x04
 int cur;                                  // +0x08 sub_460140 a1[2]: moves 1 per tick toward target
 int w3;                                   // +0x0C
 int out;                                  // +0x10 sub_44B480 reads a3[+36]/a3[+64] before/after and adds the difference to x/y
 int w5;                                   // +0x14
 int target;                               // +0x18 sub_460140 a1[6]
};
struct RboMoverGaugePair {                 // payload of kind 3 (sub_44B500 / sub_44B480): two ramps -> x offset and y offset
 struct RboMoverGaugeAxis xAxis;           // +0x00
 struct RboMoverGaugeAxis yAxis;           // +0x1C
};
struct RboMoverPathFollow {                // payload of kind 4 (sub_44B910 / sub_44B860): snap x,y to a path point, optionally clamped to a radius around path point 0
 int pathIndex;                            // +0x00 sub_44B910 a2[5]; sub_44ACA0(path, idx) fetches the point
 int curX;                                 // +0x04 sub_44B860 *a1 = a3[6]
 int curY;                                 // +0x08 *a2 = a3[7]
 void *path;                               // +0x0C a2[8] path object (owner/spawner actor+1160)
 unsigned int clampAxes;                   // +0x10 bit0 clamp X, bit1 clamp Y (sub_44B860)
 int clampRadiusX;                         // +0x14 max |x - point0.x|
 int clampRadiusY;                         // +0x18
};
union RboMoverPayload {                    // 56 bytes, discriminated by RboMover.kind
 struct RboMoverConstOffset constOffset;
 struct RboMoverVec velocity;
 struct RboMoverLimitedAccel limited;
 struct RboMoverGaugePair gaugePair;
 struct RboMoverPathFollow path;
 unsigned char raw[56];
};
struct RboMover {                          // 76 bytes (0x4C)
 unsigned char unused_00[12];              // +0x00 no ctor writes and no step fn reads +0..+11 (sub_44B680/44B6D0/44B7C0/44B500/44B590/44B910 write >= +12)
 RboMoverStepFn step;                      // +0x0C ctor stores sub_44B560/44B650/44B720/44B480/44B860; called by Actor_StepMotionMovers as step(&x,&y,mover)
 RboMoverKind kind;                        // +0x10 Actor_GetVelocityBlock 0x44B370 (1 -> vec at +20, 2 -> vec at +40), sub_4205C0 tests ==4
 union RboMoverPayload payload;            // +0x14
};
struct RboDepthTween {                     // 36 bytes: eases actor localZ and drawDepthOffset (sub_44BA70 init, sub_44BA30 step, sub_44B9C0 apply)
 RboDepthTweenStepFn step;                 // +0x00 sub_44BA30 stored by sub_44BA70
 int unused_04;                            // +0x04 sub_44BA70 writes 0; no reader
 int zStart;                               // +0x08 sub_44B9C0 a3[2]: z = zStart + t*(zEnd-zStart)
 int zEnd;                                 // +0x0C a3[3]
 int duration;                             // +0x10 a3[4] ticks
 int elapsed;                              // +0x14 a3[5] incremented by sub_44BA30
 int depthStart;                           // +0x18 a3[6]: drawDepthOffset = depthStart + t*(depthEnd-depthStart)
 int depthEnd;                             // +0x1C a3[7]
 int depthFinal;                           // +0x20 a3[8] written to drawDepthOffset when the tween ends
};
