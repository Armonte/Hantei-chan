// RboActor part 4 (+0x780..+0x947): new nested struct/enum types, IDA-parsable (idc.parse_decls).
// Evidence per field: docs/formats/ida/actor_part4.md. Offsets in struct comments are relative to the struct start.
// Parse order matters (enums first). Flag enums are bitmask enums (set with idc.set_enum_bf / BTF_ENUM bitfield flag).
enum RboP4DownState : unsigned int { DOWN_NONE=0, DOWN_ALIVE_CAN_TECH=1, DOWN_KO_OR_LAUNCHED=2 };
enum RboP4FacingFollowMode : unsigned int { FFOLLOW_NONE=0, FFOLLOW_COPY_FLIP=1, FFOLLOW_TRACK_ONLY=2, FFOLLOW_COPY_AND_TRACK=3 };
enum RboP4CameraFocusMode : unsigned int { CAMFOCUS_EXCLUDED=0, CAMFOCUS_USE_FOCUS_XY=1 };
enum RboP4WallContact : unsigned int { WALL_STAGE_LEFT=0x1, WALL_STAGE_RIGHT=0x2, WALL_BEYOND_LEFT_EDGE=0x10, WALL_BEYOND_RIGHT_EDGE=0x20, WALL_AT_LEFT_EDGE=0x40, WALL_AT_RIGHT_EDGE=0x80, WALL_ABOVE_CEILING=0x100, WALL_AT_CEILING=0x400, WALL_AT_FLOOR=0x800 };
enum RboP4ScreenBoundFlags : unsigned int { SBF_CHECK_LEFT_A=0x1, SBF_CHECK_RIGHT_A=0x2, SBF_DESPAWN_ON_EXIT=0x10 };
enum RboP4VertBoundFlags : unsigned int { VBF_CEILING=0x1, VBF_FLOOR=0x2, VBF_DESPAWN_ABOVE_CEILING=0x10, VBF_DESPAWN_BELOW_FLOOR=0x20 };
enum RboP4FadeFlags : unsigned int { FADEF_DEFAULT_10F=0x1, FADEF_CUSTOM_DURATION=0x10, FADEF_KILL_ON_LIFETIME_END=0x100 };
enum RboP4SetupFlags : unsigned int { SETUPF_SKIP_IDLE_TRANSITIONS=0x1, SETUPF_NO_ENTER_BRAKE_OR_MOVE=0x2, SETUPF_CLEAR_ATTACK_BOXES_ON_ENTER=0x4, SETUPF_RESET_KASANARI_ON_ENTER=0x8, SETUPF_SCRIPT_LIST_ARG=0x10, SETUPF_COPY_TINT_TO_CHILD=0x20, SETUPF_KEEP_FREEZE_TIMER=0x40 };
enum RboP4StatusKindMask : unsigned int { STK_KIND3=0x8, STK_KIND4=0x10, STK_CC_KIND5=0x20, STK_CC_KIND6=0x40, STK_CC_KIND7=0x80, STK_CC_KIND8=0x100, STK_NO_INPUT_KIND9=0x200, STK_KIND10=0x400, STK_KIND11=0x800, STK_KIND12=0x1000, STK_KIND13=0x2000, STK_KIND14=0x4000 };
enum RboP4StatusHookMask : unsigned int { STH_DAMAGE_HOOK_A=0x4, STH_DAMAGE_HOOK_B=0x8, STH_DAMAGE_HOOK_C=0x1000 };
enum RboP4FrameStepFlags : unsigned int { FSTEP_REFRESH_ON_TICK=0x1, FSTEP_UPDATE_EFFECT_AFTER_TICK=0x2, FSTEP_BRAKE_PENDING=0x4 };
enum RboP4ColorFxKind : unsigned int { CFX_NONE=0, CFX_SPAWN_FX_1=1, CFX_SPAWN_FX_2=2, CFX_COLOR_CYCLE_3=3, CFX_SPAWN_FX_4=4, CFX_SPAWN_CYCLED_11=11, CFX_SPAWN_FX_12=12, CFX_HIT_FLASH=15 };
struct RboP4ReactionState {  // actor +0x78C, 36 bytes (hit reaction / facing / knockdown latch)
 int facingSign;                    // +0x00 dword_48A3B0[facingLeft]: +1 right / -1 left (sub_441940 0x441940 writes, sub_444CA0 scales velocity)
 int desiredFacingSign;             // +0x04 requested facing sign; sub_444C10 turns it into pendingFlip (also read by sub_44D900)
 int pendingReactionClass;          // +0x08 -1 = none; sub_444F60/sub_444DF0 run the SetYarareAnime/SetGuardAnimEd script with it then reset to -1 (7 = knockdown)
 int moveDirMultiplier;             // +0x0C 1 default / -1 when facing changed (sub_444C60); multiplies frame-move speed in Actor_FrameMoveAddSpeed 0x44B3D0
 int lastHitClass;                  // +0x10 Hit_SetVictimReactionAnim 0x444340 stores the RboHitClass; -1 reset (sub_4414E0), 7 on knockdown
 int hitsTakenCounter;              // +0x14 ++ per reaction (cap 999999) in Hit_SetVictimReactionAnim; >=2 forces heavy reaction on player 0; 0 reset
 unsigned int hurtSlotHitMask;      // +0x18 1<<hurtBoxSlot for hurt boxes touched (sub_445550 0x445550), tested by sub_420050/sub_4200D0 transition rules
 RboP4DownState downState;          // +0x1C 0 none / 1 knocked down alive / 2 KO-launched (sub_4418F0, sub_441930); sub_43DC30 wakeup test
 int downTimer;                     // +0x20 frames until tech/wakeup input accepted (20 on knockdown, -1 off); decremented in sub_440550
};
struct RboP4KasanariTouchState {   // actor +0x7B0, 40 bytes; kasanari (overlap) box state, continues into the existing kasanariBoxCount at +0x7D8
 int active;                        // +0x00 sub_441280 (set 1/0 from Actor_UpdateBoxes272State 0x4412A0 when frame attackEventFlags KASANARI_BEGIN/END)
 unsigned int hitEventFlags;        // +0x04 sub_419840 ORs 8 (self and ancestors); Actor_InputMaskMatches 0x41DBB0 tests 0x808
 unsigned int hitEventClassMask;    // +0x08 sub_419840 ORs the hitter stance class (<<8 on ancestors); Actor_InputMaskMatches ORs it into the input result
 unsigned char unused_0C[8];        // +0x0C..0x13 no reader/writer found (displacement scan + all pointer-callee functions)
 int clearedA;                      // +0x14 only ever zeroed (sub_441260 -> sub_440EF0)
 int clearedB;                      // +0x18 only ever zeroed (sub_441260, sub_4411D0)
 int clearedC;                      // +0x1C only ever zeroed (sub_440EF0)
 unsigned char unused_20[4];        // +0x20 no reader/writer found
 unsigned int overlapEventMask;     // +0x24 sub_4198B0 ORs the touched record's event bits (propagates to ancestors); sub_41FA50 rule kind 0 tests it; cleared by sub_441260
};
struct RboP4FollowState {          // actor +0x820, 12 bytes (set by frame-enter setup ops 0x0..3, Actor_RunFrameEnterSetupOps 0x41A650)
 int ticksWithParent;               // +0x00 1 = child is ticked by its parent (skipped in the child-pass of sub_440550); op0 mask 0x10000 clears, 0x20000 sets+resets tick rate
 int isFollower;                    // +0x04 op2; sub_4197B0/sub_419950/sub_4199A0/sub_43F230 walk parents while this is non-zero to find the controlling ancestor
 int forwardsEvents;                // +0x08 op3; sub_4198E0/sub_419840/sub_4198B0 keep propagating hit/input events to the parent while this is non-zero
};
struct RboP4CameraFocus {          // actor +0x82C, 16 bytes (Camera_FrameActors 0x42FFD0, sub_44A1F0 cases 1/2/10)
 RboP4CameraFocusMode mode;         // +0x00 0 = ignored by camera, 1 = use x/y below, anything else = use actor position
 int x;                             // +0x04 captured by sub_44A1F0 case 10
 int y;                             // +0x08 
 int leash;                         // +0x0C 1 = actor's wall clamp also respects the other actors' camera focus (sub_43FD50 tail); set to 1 by sub_441D40
};
struct RboP4ScreenBound {          // actor +0x83C, 36 bytes (sub_43FD50 0x43FD50 screen/stage edge contact; ops 4,5,6,17,19 of sub_41A650)
 int mode;                          // +0x00 edge mode: -2 stage bounds only, -1 none, 0/1 screen clamp (bits 0x4/0x10/0x20 are runtime state); sub_44A1F0 case 1
 unsigned int flags;                // +0x04 RboP4ScreenBoundFlags: which sides are checked / despawn when beyond
 int extentA;                       // +0x08 horizontal extent (x - A when facing left / x + A..), op 17 first value
 int extentDown;                    // +0x0C vertical extent below: y + extentDown > floor despawns (op 17 third value)
 int extentB;                       // +0x10 horizontal extent, op 17 second value
 int extentUp;                      // +0x14 vertical extent above: y + extentUp < ceiling despawns (op 17 fourth value)
 unsigned int vertFlags;            // +0x18 RboP4VertBoundFlags (op 19)
 int ceilingY;                      // +0x1C op 19
 int floorY;                        // +0x20 op 19
};
struct RboP4ParentLink {           // actor +0x860, 64 bytes: rules run by Actor_RunParentLinkRules 0x43F460 (children of the actor tree)
 int onParentFlagAction;            // +0x00 action mask for sub_43F360 when parent byte +0x53D == 1 (op 8)
 int onParentEventAction;           // +0x04 action mask run in the event pass (op 9)
 int onParentReactionAction;        // +0x08 action mask when parent.reaction.pendingReactionClass != -1 (op 10)
 int expiryInhibit;                 // +0x0C op 11; non-zero stops sub_442890 (auto-expiry) from firing
 RboP4FacingFollowMode facingFollow;// +0x10 op 14: how the child tracks its parent's facing
 int flipRelativeXOnFlip;           // +0x14 op 14: sub_43F410 negates actor+0x94 when the facing flips
 int flipVelocityOnFlip;            // +0x18 op 14: sub_43F410 negates the velocity block when the facing flips
 int lastParentFacing;              // +0x1C last facing byte seen on the parent (modes 2/3)
 RboP4FadeFlags fadeFlags;          // +0x20 op 13: sub_43F2A0 detach fade selection
 int fadeFrames;                    // +0x24 op 13 (custom fade duration)
 int lifetimeFrames;                // +0x28 op 13: written to actor+0x2BC lifetime on detach
 int detachPattern;                 // +0x2C op 12: sub_43F360 mask 0x20 sets pendingPattern from it
 int detachFrame;                   // +0x30 op 12: sets pendingFrame
 int attachedToParent;              // +0x34 op 15: non-zero = position relative to parent (cleared with sub_4422A0 on detach); sub_442CC0
 int hitCreditToParent;             // +0x38 op 16: 1 = contacts/hits by this child are credited to the parent (Combat_ResolveGuardContacts, Combat_ResolveDamageHit)
 unsigned char unused_3C[4];        // +0x3C never written by any setup op and never read
};
struct RboP4ColorFxSlot {          // 40 bytes; two per actor at +0x8A4 / +0x8CC (status/hit tint + particle effect)
 int timer;                         // +0x00 active while non-zero; >0 counts down in sub_440550, set by sub_442110 (flash=8)
 int colorStep;                     // +0x04 colour cycle phase (sub_448810 0x448810)
 unsigned char tintR;               // +0x08 multiplies the actor tint (sub_4464D0 0x4464D0), 255 = unchanged
 unsigned char tintG;               // +0x09 
 unsigned char tintB;               // +0x0A 
 unsigned char addR;                // +0x0B additive colour
 unsigned char addG;                // +0x0C 
 unsigned char addB;                // +0x0D 
 unsigned char unused_0E[2];        // +0x0E never written or read (only bytes +8..+0xD are touched)
 RboP4ColorFxKind kind;             // +0x10 effect kind dispatched by sub_440330 0x440330
 int framesLeft;                    // +0x14 frames until the effect clears itself (sub_442130)
 int period;                        // +0x18 reload value of the step counter
 int periodCounter;                 // +0x1C counts down; at 0 spawns the kind's particle effect
 int cycleLength;                   // +0x20 kind 11: number of cycle entries
 int cycleIndex;                    // +0x24 kind 11: current entry
};
struct RboP4BufferedMove {         // actor +0x908, 20 bytes: buffered special-move window (sub_43E1B0 0x43E1B0, sub_4209B0 0x4209B0)
 int armed;                         // +0x00 1 = window open (cleared by sub_43E1B0 when frames run out; never seen set by a displacement write)
 int * candidateMoves;              // +0x04 -1 terminated list of move indices tried each frame
 int framesLeft;                    // +0x08 window length; <=0 allows the buffered move to fire
 int matchedMove;                   // +0x0C index into charContext move table (+28316), -1 = none
 int matchedFacing;                 // +0x10 facing/dir captured when it matched (sub_4196B0 out value)
};
